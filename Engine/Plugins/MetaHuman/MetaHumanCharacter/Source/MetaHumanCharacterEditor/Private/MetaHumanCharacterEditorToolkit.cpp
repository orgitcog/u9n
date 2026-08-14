// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorToolkit.h"

#include "AssetEditorModeManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Cloud/MetaHumanARServiceRequest.h"
#include "Components/SkeletalMeshComponent.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "DNAReader.h"
#include "DNAUtils.h"
#include "Editor/EditorEngine.h"
#include "EditorDialogLibrary.h"
#include "EditorViewportTabContent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ImageUtils.h"
#include "Logging/StructuredLog.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterActorInterface.h"
#include "MetaHumanCharacterAnalytics.h"
#include "MetaHumanCharacterAnimInstance.h"
#include "MetaHumanCharacterAssetEditor.h"
#include "MetaHumanCharacterAssetEditorContext.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterEditorMode.h"
#include "MetaHumanCharacterEditorModule.h"
#include "MetaHumanCharacterEditorPipelineSpecification.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorUILayer.h"
#include "MetaHumanCharacterEditorViewportClient.h"
#include "MetaHumanCharacterEnvironmentLightRig.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPaletteProjectSettings.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanInvisibleDrivingActor.h"
#include "MetaHumanRigEvaluatedState.h"
#include "MetaHumanWardrobeItem.h"
#include "Misc/FileHelper.h"
#include "PackageTools.h"
#include "PreviewScene.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "SMetaHumanCharacterEditorPreviewSettingsView.h"
#include "ToolMenus.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "Tools/MetaHumanCharacterEditorEyesTool.h"
#include "UI/Viewport/SMetaHumanCharacterEditorViewport.h"
#include "UI/Viewport/SMetaHumanCharacterEditorViewportAnimationBar.h"
#include "SMetaHumanCharacterEditorPreviewSettingsView.h"
#include "MetaHumanCharacterAnimInstance.h"
#include "ScopedTransaction.h"
#include "Widgets/Colors/SColorPicker.h"
#include "GroomComponent.h"
#include "Dialogs/Dialogs.h"

extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

static const TCHAR* MetaHumanCharacterEditorToolkitTransactionContext = TEXT("MetaHumanCharacterEditorToolkitTransaction");


namespace UE::MetaHuman
{
	static void DuplicateSkeletalMesh(const UMetaHumanCharacter* MetaHumanCharacter, const FString& InNameSuffix, const USkeletalMesh* InSkeletalMeshAsset)
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();

		// Create a unique package name and asset name for the actor skel mesh
		const FString MetaHumanCharacterPackageName = MetaHumanCharacter->GetOutermost()->GetName();
		const FString NewSkelMeshAssetName = MetaHumanCharacter->GetName() + InNameSuffix;
		const FString TentativePackageName = FPaths::Combine(FPackageName::GetLongPackagePath(MetaHumanCharacterPackageName), NewSkelMeshAssetName);
		const FString TentativePackagePath = UPackageTools::SanitizePackageName(TentativePackageName);
		FString DefaultSuffix;
		FString NewAssetName;
		FString NewPackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(TentativePackagePath, DefaultSuffix, /*out*/ NewPackageName, /*out*/ NewAssetName);

		UPackage* NewPackage = CreatePackage(*NewPackageName);
		USkeletalMesh* NewAsset = DuplicateObject(InSkeletalMeshAsset, NewPackage, FName(NewAssetName));

		FAssetRegistryModule::AssetCreated(NewAsset);
	}

	static void SaveBufferToFileWithDialog(const FSharedBuffer& StateData)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		TArray<FString> OutFilenames;

		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const FString DialogTitle = LOCTEXT("SaveFaceDNADialogTitle", "Save Face DNA file").ToString();
		const FString DefaultPath = TEXT("");
		const FString DefaultFile = TEXT("");
		const FString FileTypes = TEXT("*");
		if (DesktopPlatform->SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultFile, DefaultPath, FileTypes, EFileDialogFlags::None, OutFilenames))
		{
			if (OutFilenames.Num() == 1)
			{
				// The serialized state is a json string
				FFileHelper::SaveArrayToFile(TArrayView64<const uint8>{
					reinterpret_cast<const uint8*>(StateData.GetData()), static_cast<int64>(StateData.GetSize())}, * OutFilenames[0]);
			}
		}
	}
}

const FName FMetaHumanCharacterEditorToolkit::MetaHumanCharacterPreviewTabID(TEXT("MetaHumanCharacterEditor_PreviewSettingsTab"));
const FName FMetaHumanCharacterEditorToolkit::MetaHumanCharcterAnimationPanelID(TEXT("MetaHumanCharacterEditor_AnimationPanelTab"));

FMetaHumanCharacterEditorToolkit::FMetaHumanCharacterEditorToolkit(UMetaHumanCharacterAssetEditor* InOwningAssetEditor)
	: FBaseAssetToolkit{ InOwningAssetEditor }
{
	StandaloneDefaultLayout = FTabManager::NewLayout(TEXT("MetaHumanCharacterEditorLayout_5"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					// ->AddTab(UAssetEditorUISubsystem::TopLeftTabID, ETabState::OpenedTab)
					->SetExtensionId("EditorSidePanelArea")
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.5)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.95f)
						->AddTab(ViewportTabID, ETabState::OpenedTab)
						->SetExtensionId(TEXT("ViewportArea"))
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.05f)
						->AddTab(MetaHumanCharcterAnimationPanelID, ETabState::OpenedTab)
						->SetExtensionId(TEXT("AnimationArea"))
						->SetHideTabWell(true)
					)

				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(DetailsTabID, ETabState::ClosedTab)
					->AddTab(MetaHumanCharacterPreviewTabID, ETabState::OpenedTab)
					->SetExtensionId(TEXT("DetailsArea"))
					->SetHideTabWell(false)
				)
			)
		);

	LayoutExtender = MakeShared<FLayoutExtender>();
	FMetaHumanCharacterEditorModule::GetChecked().OnRegisterLayoutExtensions().Broadcast(*LayoutExtender);
	StandaloneDefaultLayout->ProcessExtensions(*LayoutExtender);

	// Constructs the preview scene without its default directional light since W
	PreviewScene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues()
											 .SetCreateDefaultLighting(false));

	/*	PreviewScene = MakeUnique<FMetaHumanCharacterEditorPreviewScene>(FMetaHumanCharacterEditorPreviewScene::ConstructionValues()
				.SetCreateDefaultLighting(false));*/
	
	UWorld* PreviewWorld = PreviewScene->GetWorld();
	
	// Creating Character Actor 
	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter);
	check(MetaHumanCharacter->IsCharacterValid());

	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	// Object should have been added before this toolkit was created
	check(MetaHumanCharacterSubsystem->IsObjectAddedForEditing(MetaHumanCharacter));

	// Creates the MetaHuman Preview Actor using information from the asset
	PreviewActor = MetaHumanCharacterSubsystem->CreateMetaHumanCharacterEditorActor(MetaHumanCharacter, PreviewWorld);
	check(PreviewActor);
	check(PreviewActor.GetObject()->IsA<AActor>());

	// Create the invisible driving actor used for animation preview. This will act as the retargeting source for the preview actor.
	MetaHumanCharacterSubsystem->CreateMetaHumanInvisibleDrivingActor(MetaHumanCharacter, PreviewActor, PreviewWorld);

	// Build the collection and assemble the character wardrobe items
	MetaHumanCharacterSubsystem->RunCharacterEditorPipelineForPreview(MetaHumanCharacter);
}

FMetaHumanCharacterEditorToolkit::~FMetaHumanCharacterEditorToolkit()
{
	// We need to force the editor mode deletion now because otherwise the preview world
	// will end up getting destroyed before the mode's Exit() function gets to run, and we'll get some
	// warnings when we destroy any mode actors.
	EditorModeManager->DestroyMode(UMetaHumanCharacterEditorMode::EM_MetaHumanCharacterEditorModeId);

	AActor* Actor = CastChecked<AActor>(PreviewActor.GetObject());
	Actor->Destroy();
}

FName FMetaHumanCharacterEditorToolkit::GetToolkitFName() const
{
	return TEXT("MetaHumanCharacterEditor");
}

FText FMetaHumanCharacterEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "MetaHuman Character Editor");
}

void FMetaHumanCharacterEditorToolkit::CreateEditorModeManager()
{
	FBaseAssetToolkit::CreateEditorModeManager();

	// The mode manager is the authority on what the world is for the mode and the tools context,
	// and setting the preview scene here makes our GetWorld() function return the preview scene
	// world instead of the normal level editor one. Important because that is where we create
	// any preview meshes, gizmo actors, etc.
	StaticCastSharedPtr<FAssetEditorModeManager>(EditorModeManager)->SetPreviewScene(PreviewScene.Get());
}

void FMetaHumanCharacterEditorToolkit::SaveAsset_Execute()
{
	if (HasActiveTool())
	{
		// Saving the asset while a tool is active will accept the tool
		GetMetaHumanCharacterEditorMode()->GetToolManager()->DeactivateTool(EToolSide::Mouse, EToolShutdownType::Completed);
	}

	// Close the color picker on save
	DestroyColorPicker();

	FBaseAssetToolkit::SaveAsset_Execute();
}

void FMetaHumanCharacterEditorToolkit::InitToolMenuContext(FToolMenuContext& InMenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(InMenuContext);

	UMetaHumanCharacterAssetEditorContext* Context = NewObject<UMetaHumanCharacterAssetEditorContext>();
	Context->MetaHumanCharacterAssetEditor = SharedThis(this);
	InMenuContext.AddObject(Context);
}

void FMetaHumanCharacterEditorToolkit::OnClose()
{
	// Close any color picker opened during an edit session
	DestroyColorPicker();

	if (UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit())
	{
		// OnClose is called twice for the same character, so only remove if currently editing it
		if (UMetaHumanCharacterEditorSubsystem::Get()->IsObjectAddedForEditing(MetaHumanCharacter))
		{
			// Deactivate the current active tool before removing object to ensure tool can commit its changes
			GetMetaHumanCharacterEditorMode()->GetToolManager()->DeactivateTool(EToolSide::Left, EToolShutdownType::Completed);

			UMetaHumanCharacterEditorSubsystem::Get()->RemoveObjectToEdit(MetaHumanCharacter);
		}
	}
}

void FMetaHumanCharacterEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::RegisterTabSpawners(InTabManager);

	MetaHumanCharacterEditorMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MetaHumanCharacterEditor", "MetaHuman"));

	InTabManager->RegisterTabSpawner(MetaHumanCharcterAnimationPanelID, FOnSpawnTab::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SpawnTab_AnimationBar))
		.SetDisplayName(LOCTEXT("AnimationPanel", "Animation Bar"))
		.SetGroup(MetaHumanCharacterEditorMenuCategory.ToSharedRef())
		.SetCanSidebarTab(false);

	InTabManager->RegisterTabSpawner(MetaHumanCharacterPreviewTabID, FOnSpawnTab::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SpawnTab_PreviewSceneDetails))
		.SetDisplayName(LOCTEXT("PreviewSceneDetails", "Preview Scene Details"))
		.SetGroup(MetaHumanCharacterEditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FMetaHumanCharacterEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(MetaHumanCharcterAnimationPanelID);
	InTabManager->UnregisterTabSpawner(MetaHumanCharacterPreviewTabID);
}

AssetEditorViewportFactoryFunction FMetaHumanCharacterEditorToolkit::GetViewportDelegate()
{
	AssetEditorViewportFactoryFunction ViewportDelegateFunction = [this](FAssetEditorViewportConstructionArgs InArgs)
	{
		return SNew(SMetaHumanCharacterEditorViewport, InArgs)
			.EditorViewportClient(ViewportClient);
	};

	return ViewportDelegateFunction;
}

TSharedPtr<FEditorViewportClient> FMetaHumanCharacterEditorToolkit::CreateEditorViewportClient() const
{
	UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();

	return MakeShared<FMetaHumanCharacterViewportClient>(EditorModeManager.Get(), PreviewScene.Get(), PreviewActor, MetaHumanCharacter);
}

void FMetaHumanCharacterEditorToolkit::PostInitAssetEditor()
{
	// Make sure the viewport is always available as the mode will try to add an overlay to it
	if (!TabManager->FindExistingLiveTab(ViewportTabID))
	{
		TabManager->TryInvokeTab(ViewportTabID);
	}

	// default hide the details tab
	TSharedPtr<SDockTab> DetailsTab = TabManager->FindExistingLiveTab(FBaseAssetToolkit::DetailsTabID);
	if (DetailsTab.IsValid())
	{
		DetailsTab->RequestCloseTab();
	}

	check(ToolkitHost.IsValid());
	TSharedPtr<IToolkitHost> PinnedToolkitHost = ToolkitHost.Pin();
	ModeUILayer = MakeShared<FMetaHumanCharacterEditorModeUILayer>(PinnedToolkitHost.Get());
	ModeUILayer->SetModeMenuCategory(MetaHumanCharacterEditorMenuCategory);

	// Currently, aside from setting up all the UI elements, the toolkit also kicks off the
	// editor mode, which is the mode that the editor always works in (things are packaged into
	// a mode so that they can be moved to another asset editor if necessary).
	check(EditorModeManager.IsValid());
	EditorModeManager->ActivateMode(UMetaHumanCharacterEditorMode::EM_MetaHumanCharacterEditorModeId);

	TNotNull<UMetaHumanCharacter*> Character = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	GetMetaHumanCharacterEditorMode()->SetCharacter(Character);

	ExtendToolbar();
	ExtendMenu();
	BindCommands();

	USelection* SelectedActors = EditorModeManager->GetSelectedActors();
	USelection* SelectedComponents = EditorModeManager->GetSelectedComponents();
	check(SelectedActors && SelectedComponents);
	// The selection set of the editor mode manager is used by the tools
	// to determine which ones can be built, this can be mechanism to enable
	// disable tools depending on which part of the character we are editing
	// SelectedActors->Select(PreviewActor);
	// 
	// TODO: Remove this. Select the Face component to enable tools that rely on USkeletalMeshComponentToolTargetFactory
	// The logic to handle which component is selected should be handled by the Mode Toolkit since it knows category of tools
	// the user enabled
	// 
	// Cast away const-ness to suit the mode manager API. The face component will not be modified.
	SelectedComponents->Select(const_cast<USkeletalMeshComponent*>(static_cast<const USkeletalMeshComponent*>(PreviewActor->GetFaceComponent())));

	// We need the viewport client to start out focused, or else it won't get ticked until
	// we click inside it. This makes sure streaming of assets will actually finish before
	// the user clicks on the viewport
	ViewportClient->ReceivedFocus(ViewportClient->Viewport);

	// Same FoV used in MHC
	ViewportClient->ViewFOV = 18.001738f;

	
	UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter);
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();

	// Bind to light environment delegate so we can update the preview scene
	MetaHumanCharacterSubsystem->OnLightEnvironmentChanged(MetaHumanCharacter).BindSP(this, &FMetaHumanCharacterEditorToolkit::OnLightingStudioEnvironmentChanged);
	MetaHumanCharacterSubsystem->OnLightRotationChanged(MetaHumanCharacter).BindSP(this, &FMetaHumanCharacterEditorToolkit::OnLightRotationChanged);
	MetaHumanCharacterSubsystem->OnLightTonemapperChanged(MetaHumanCharacter).BindSP(this, &FMetaHumanCharacterEditorToolkit::OnTonemapperEnvironmentChanged);
	MetaHumanCharacterSubsystem->OnBackgroundColorChanged(MetaHumanCharacter).BindSP(this, &FMetaHumanCharacterEditorToolkit::OnBackgroundColorChanged);

	// Set widget of viewport client
	TSharedRef<FMetaHumanCharacterViewportClient> MHCViewportClient = StaticCastSharedPtr<FMetaHumanCharacterViewportClient>(ViewportClient).ToSharedRef();
	TSharedPtr<SEditorViewport> ViewportWidget = StaticCastSharedPtr<SMetaHumanCharacterEditorViewport>(ViewportTabContent->GetFirstViewport());
	MHCViewportClient->SetViewportWidget(ViewportWidget);
	

	// Load all of the lighting environments which are represented as streaming levels
	TArray<FSoftObjectPath> LightingScenarioPaths =
	{
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/Studio.Studio")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/Split.Split")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/Fireside.Fireside")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/Moonlight.Moonlight")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/Tungsten.Tungsten")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/Portrait.Portrait")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/RedLantern.RedLantern")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/TextureBooth.TextureBooth"))
	};

	FSoftObjectPath BaseEnvironment = FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/L_BaseEnvironment.L_BaseEnvironment"));
	FSoftObjectPath TonemapperEnvironement = FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/L_PostProcessing.L_PostProcessing"));
	TArray<FSoftObjectPath> PostProcessLevelsPaths = 
	{
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/L_BaseEnvironment.L_BaseEnvironment")),
		FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/L_PostProcessing.L_PostProcessing"))
	};

	LoadPostProcessScenariosInWorld(BaseEnvironment, TonemapperEnvironement);
	LoadLightingScenariosInWorld(LightingScenarioPaths);
}

void FMetaHumanCharacterEditorToolkit::ExtendToolbar()
{
	const FName MainToolbarMenuName = GetToolMenuToolbarName();
	const FName SectionName = UToolMenus::JoinMenuPaths(MainToolbarMenuName, TEXT("DynamicToolbarSection"));

	if (UToolMenu* ToolBarMenu = UToolMenus::Get()->ExtendMenu(MainToolbarMenuName))
	{
		// Define the dynamic section only once and use the UMetaHumanCharacterAssetEditorContext
		// to get the state of the open asset
		if (!ToolBarMenu->FindSection(SectionName))
		{
			ToolBarMenu->AddDynamicSection(SectionName, FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
			{
				UMetaHumanCharacterAssetEditorContext* Context = InMenu->FindContext<UMetaHumanCharacterAssetEditorContext>();
				if (Context && Context->MetaHumanCharacterAssetEditor.IsValid())
				{
					FMetaHumanCharacterEditorToolkit* AssetEditor = Context->MetaHumanCharacterAssetEditor.Pin().Get();
					FToolMenuSection& CharacterToolsSection = InMenu->AddSection(TEXT("MetaHumanCharacterTools"));

					/*
					// Disable save thumbnail for now
					CharacterToolsSection.AddEntry
					(
						FToolMenuEntry::InitToolBarButton
						(
							FMetaHumanCharacterEditorCommands::Get().SaveThumbnail,
							FMetaHumanCharacterEditorCommands::Get().SaveThumbnail->GetLabel(),
							FMetaHumanCharacterEditorCommands::Get().SaveThumbnail->GetDescription(),
							FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.SaveThumbnail"))
						)
					);

					CharacterToolsSection.AddSeparator(NAME_None);
					*/

					CharacterToolsSection.AddEntry
					(
						FToolMenuEntry::InitToolBarButton
						(
							FMetaHumanCharacterEditorCommands::Get().AutoRigFaceBlendShapes,
							FMetaHumanCharacterEditorCommands::Get().AutoRigFaceBlendShapes->GetLabel(),
							FMetaHumanCharacterEditorCommands::Get().AutoRigFaceBlendShapes->GetDescription(),
							FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.AddRigFull"))
						)
					);

					CharacterToolsSection.AddEntry
					(
						FToolMenuEntry::InitToolBarButton
						(
							FMetaHumanCharacterEditorCommands::Get().AutoRigFaceJointsOnly,
							FMetaHumanCharacterEditorCommands::Get().AutoRigFaceJointsOnly->GetLabel(),
							FMetaHumanCharacterEditorCommands::Get().AutoRigFaceJointsOnly->GetDescription(),
							FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.AddRigSkeletal"))
						)
					);

					CharacterToolsSection.AddEntry
					(
						FToolMenuEntry::InitToolBarButton
						(
							FMetaHumanCharacterEditorCommands::Get().RemoveFaceRig,
							FMetaHumanCharacterEditorCommands::Get().RemoveFaceRig->GetLabel(),
							FMetaHumanCharacterEditorCommands::Get().RemoveFaceRig->GetDescription(),
							FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.RemoveRig"))
						)
					);
					
					CharacterToolsSection.AddSeparator(NAME_None);

					CharacterToolsSection.AddEntry(FToolMenuEntry::InitComboButton
					(
						TEXT("DownloadHighResTexturesButton"),
						FToolUIActionChoice(FUIAction(FExecuteAction(),
						FCanExecuteAction::CreateSP(AssetEditor, &FMetaHumanCharacterEditorToolkit::CanRequestHighResolutionTextures))),
						FNewToolMenuDelegate::CreateLambda([](UToolMenu* InToolMenu)
						{
							const FMetaHumanCharacterEditorCommands& Commands = FMetaHumanCharacterEditorCommands::Get();
							FToolMenuSection& DownloadHighResTexturesSection = InToolMenu->AddSection(TEXT("DownloadHighResTexturesSubmenu"));
								{
									DownloadHighResTexturesSection.AddMenuEntry(Commands.DownloadHighResTextures2k);
									DownloadHighResTexturesSection.AddMenuEntry(Commands.DownloadHighResTextures4k);
									DownloadHighResTexturesSection.AddMenuEntry(Commands.DownloadHighResTextures8k);
								}
						}),
						LOCTEXT("DownloadHighResTexturesButtonLabel", "Download Texture Source"),
						LOCTEXT("DownloadHighResTexturesButtonToolTip", "Download Texture Source"), 
						FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.DownloadHighResTextures")))
					);

					CharacterToolsSection.AddSeparator(NAME_None);

					CharacterToolsSection.AddEntry
					(
						FToolMenuEntry::InitToolBarButton
						(
							FMetaHumanCharacterEditorCommands::Get().RefreshPreview,
							FMetaHumanCharacterEditorCommands::Get().RefreshPreview->GetLabel(),
							FMetaHumanCharacterEditorCommands::Get().RefreshPreview->GetDescription(),
							FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("Icons.Refresh"))
						)
					);
				}
			}));
		}
	}
}

void FMetaHumanCharacterEditorToolkit::ExtendMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	const FMetaHumanCharacterEditorCommands& Commands = FMetaHumanCharacterEditorCommands::Get();
	const FName MHCMenuName = UToolMenus::JoinMenuPaths(GetToolMenuName(), TEXT("MetaHumanCharacter"));
	const FName MHCSectionName = UToolMenus::JoinMenuPaths(MHCMenuName, TEXT("MetaHumanCharacterSectionName"));

	if (!ToolMenus->IsMenuRegistered(MHCMenuName))
	{
		UToolMenu* MHCMainMenu = ToolMenus->RegisterMenu(MHCMenuName);
		MHCMainMenu->AddDynamicSection(MHCSectionName, FNewToolMenuDelegate::CreateLambda([Commands](UToolMenu* InMenu)
		{
			UMetaHumanCharacterAssetEditorContext* Context = InMenu->FindContext<UMetaHumanCharacterAssetEditorContext>();
			if (Context && Context->MetaHumanCharacterAssetEditor.IsValid())
			{				
				FToolMenuSection& Section = InMenu->AddSection(TEXT("MetaHumanCharacterAssetServicesActions"), LOCTEXT("MetaHumanCharacterAssetServicesActionsSection", "MetaHuman Character Online Services"));

				Section.AddMenuEntry
				(
					Commands.DownloadHighResTextures2k,
					FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures2k->GetLabel(),
					FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures2k->GetDescription(),
					FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.DownloadHighResTextures"))
				);
				
				Section.AddMenuEntry
				(
					Commands.DownloadHighResTextures4k,
					FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures4k->GetLabel(),
					FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures4k->GetDescription(),
					FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.DownloadHighResTextures"))
				);

				Section.AddMenuEntry
				(
					Commands.DownloadHighResTextures8k,
					FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures8k->GetLabel(),
					FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures8k->GetDescription(),
					FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.DownloadHighResTextures"))
				);


				Section.AddMenuEntry
				(
					Commands.AutoRigFaceBlendShapes,
					FMetaHumanCharacterEditorCommands::Get().AutoRigFaceBlendShapes->GetLabel(),
					FMetaHumanCharacterEditorCommands::Get().AutoRigFaceBlendShapes->GetDescription(),
					FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.AddRigFull"))
				);

				Section.AddMenuEntry
				(
					Commands.AutoRigFaceJointsOnly,
					FMetaHumanCharacterEditorCommands::Get().AutoRigFaceJointsOnly->GetLabel(),
					FMetaHumanCharacterEditorCommands::Get().AutoRigFaceJointsOnly->GetDescription(),
					FSlateIcon(FMetaHumanCharacterEditorStyle::Get().GetStyleSetName(), TEXT("MetaHumanCharacterEditor.Toolbar.AddRigSkeletal"))
				);

				// Add the data menu
				{
					FMetaHumanCharacterEditorDebugCommands DataCommands = FMetaHumanCharacterEditorDebugCommands::Get();
					FToolMenuSection& DataSection = InMenu->AddSection(TEXT("MetaHumanCharacterDataActions"), LOCTEXT("MetaHumanCharacterDataActionsSection", "MetaHuman Character Data"));

					// Meshes
					DataSection.AddMenuEntry(DataCommands.ExportFaceSkelMesh);
					DataSection.AddMenuEntry(DataCommands.ExportBodySkelMesh);
					DataSection.AddMenuEntry(DataCommands.ExportCombinedSkelMesh);

					// Identity state
					//DataSection.AddMenuEntry(DataCommands.DebugSaveFaceState);
					//DataSection.AddMenuEntry(DataCommands.DebugSaveFaceStateToDNA);
					//DataSection.AddMenuEntry(DataCommands.DebugDumpFaceStateDataForAR);
					//DataSection.AddMenuEntry(DataCommands.DebugSaveBodyState);

					// DNA
					DataSection.AddMenuEntry(DataCommands.SaveFaceDNA);
					DataSection.AddMenuEntry(DataCommands.SaveBodyDNA);

					// Textures
					DataSection.AddMenuEntry(DataCommands.SaveFaceTextures);

					// Presets
					//DataSection.AddMenuEntry(DataCommands.SaveEyePreset);

					// Screenshot
					DataSection.AddMenuEntry(DataCommands.TakeHighResScreenshot);
				}
			}
		}));
	}

	const FName CharacterMainMenuName = UToolMenus::JoinMenuPaths(GetToolMenuName(), TEXT("MetaHumanCharacter"));

	if (!ToolMenus->IsMenuRegistered(CharacterMainMenuName))
	{
		ToolMenus->RegisterMenu(CharacterMainMenuName, MHCMenuName);
	}

	if (UToolMenu* MainMenu = ToolMenus->ExtendMenu(GetToolMenuName()))
	{
		const FToolMenuInsert MenuInsert{ TEXT("Tools"), EToolMenuInsertType::After };

		FToolMenuSection& Section = MainMenu->FindOrAddSection(NAME_None);

		FToolMenuEntry& MetaHumanCharacterEntry = Section.AddSubMenu(TEXT("MetaHumanCharacter"),
			LOCTEXT("MetaHumanCharacterEditorMenuLabel", "MetaHuman Character"),
			LOCTEXT("MetaHumanCharacterEditorMenuTooltip", "Commands used for MetaHuman Character"),
			FNewToolMenuChoice{});

		MetaHumanCharacterEntry.InsertPosition = MenuInsert;
	}
}

void FMetaHumanCharacterEditorToolkit::BindCommands()
{
	ToolkitCommands->MapAction(FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures2k,
	                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::RequestHighResolutionTextures, ERequestTextureResolution::Res2k),
	                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanRequestHighResolutionTextures));
	
	ToolkitCommands->MapAction(FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures4k,
	                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::RequestHighResolutionTextures, ERequestTextureResolution::Res4k),
	                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanRequestHighResolutionTextures));
	
	ToolkitCommands->MapAction(FMetaHumanCharacterEditorCommands::Get().DownloadHighResTextures8k,
	                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::RequestHighResolutionTextures, ERequestTextureResolution::Res8k),
	                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanRequestHighResolutionTextures));
	
	ToolkitCommands->MapAction(FMetaHumanCharacterEditorCommands::Get().AutoRigFaceJointsOnly,
	                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::AutoRigFace, UE::MetaHuman::ERigType::JointsOnly),
	                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanAutoRigFace));
	
	ToolkitCommands->MapAction(FMetaHumanCharacterEditorCommands::Get().AutoRigFaceBlendShapes,
	                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::AutoRigFace, UE::MetaHuman::ERigType::JointsAndBlendshapes),
	                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanAutoRigFace));
	
	ToolkitCommands->MapAction(FMetaHumanCharacterEditorCommands::Get().RemoveFaceRig,
	                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::RemoveFaceRig),
	                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanRemoveFaceRig));
		
	ToolkitCommands->MapAction(FMetaHumanCharacterEditorCommands::Get().RefreshPreview,
	                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::RefreshPreview));

	// Add the debug menu if enabled
	{
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().ExportFaceSkelMesh,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::ExportFaceSkelMesh),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanExportPreviewSkelMeshes));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().ExportBodySkelMesh,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::ExportBodySkelMesh),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanExportPreviewSkelMeshes));
	
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().ExportCombinedSkelMesh,
								   FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::ExportCombinedSkelMesh),
								   FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanExportCombinedSkelMesh));

		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().SaveFaceDNA,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SaveFaceDNA),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveFaceDNA));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().SaveBodyDNA,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SaveBodyDNA),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveBodyDNA));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().SaveFaceState,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SaveFaceState),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveStates));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().SaveFaceStateToDNA,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SaveFaceStateToDNA),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveStates));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().DumpFaceStateDataForAR,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::DumpFaceStateDataForAR),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveStates));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().SaveBodyState,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SaveBodyState),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveStates));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().SaveFaceTextures,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SaveFaceTextures),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveTextures));
		
		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().SaveEyePreset,
		                           FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::SaveEyePreset),
		                           FCanExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::CanSaveEyePreset));

		ToolkitCommands->MapAction(FMetaHumanCharacterEditorDebugCommands::Get().TakeHighResScreenshot,
								   FExecuteAction::CreateSP(this, &FMetaHumanCharacterEditorToolkit::TakeHighResScreenshot),
								   FCanExecuteAction());
	}
}

void FMetaHumanCharacterEditorToolkit::OnToolkitHostingStarted(const TSharedRef<IToolkit>& InToolkit)
{
	ModeUILayer->OnToolkitHostingStarted(InToolkit);
}

void FMetaHumanCharacterEditorToolkit::OnToolkitHostingFinished(const TSharedRef<IToolkit>& InToolkit)
{
	ModeUILayer->OnToolkitHostingFinished(InToolkit);
}

bool FMetaHumanCharacterEditorToolkit::CanRequestHighResolutionTextures() const
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	check(MetaHumanCharacterSubsystem);

	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter);

	return MetaHumanCharacter->HasSynthesizedTextures() &&
		!MetaHumanCharacterSubsystem->IsRequestingHighResolutionTextures(MetaHumanCharacter) &&
		MetaHumanCharacterSubsystem->IsTextureSynthesisEnabled();
}

void FMetaHumanCharacterEditorToolkit::RequestHighResolutionTextures(ERequestTextureResolution InResolution)
{
	if (HasActiveTool())
	{
		// Saving the asset while a tool is active will accept the tool
		GetMetaHumanCharacterEditorMode()->GetToolManager()->DeactivateTool(EToolSide::Mouse, EToolShutdownType::Completed);
	}

	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	UMetaHumanCharacterEditorSubsystem* CharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	CharacterEditorSubsystem->RequestHighResolutionTextures(MetaHumanCharacter, InResolution);
}

void FMetaHumanCharacterEditorToolkit::OnLightingStudioEnvironmentChanged(const EMetaHumanCharacterEnvironment NewStudioEnvironment)
{
	check(PreviewScene->GetWorld());
	FString NewStudioEnvironmentName = StaticEnum<EMetaHumanCharacterEnvironment>()->GetAuthoredNameStringByValue(static_cast<uint8>(NewStudioEnvironment));
	
	for (ULevelStreaming* LevelStreaming : PreviewScene->GetWorld()->GetStreamingLevels())
	{
		// Skip post process levels
		if(PostProcessLevels.Contains(LevelStreaming))
		{
			continue;
		}

		const FSoftObjectPath StreamingLevelPath = LevelStreaming->GetWorldAsset().ToSoftObjectPath();
		FString LightingScenarioName = StreamingLevelPath.GetAssetName();

		if(NewStudioEnvironmentName == LightingScenarioName)
		{
			LevelStreaming->SetShouldBeVisibleInEditor(true);
		}
		else
		{
			LevelStreaming->SetShouldBeVisibleInEditor(false);
		}
	}

	PreviewScene->GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	
	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	OnLightRotationChanged(MetaHumanCharacter->ViewportSettings.LightRotation);
}

void FMetaHumanCharacterEditorToolkit::OnLightRotationChanged(float InRotation)
{
	check(PreviewScene->GetWorld());

	for (FActorIterator It(PreviewScene->GetWorld()); It; ++It)
	{
		AActor* Actor = *It;

		if (Actor->GetClass()->ImplementsInterface(UMetaHumanCharacterEnvironmentLightRig::StaticClass()))
		{
			IMetaHumanCharacterEnvironmentLightRig::Execute_SetRotation(Actor, InRotation);
		}
	}
}

void FMetaHumanCharacterEditorToolkit::OnBackgroundColorChanged(const FLinearColor& InBackgroundColor)
{
	check(PreviewScene->GetWorld());

	for (FActorIterator It(PreviewScene->GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetClass()->ImplementsInterface(UMetaHumanCharacterEnvironmentBackground::StaticClass()))
		{
			IMetaHumanCharacterEnvironmentBackground::Execute_SetBackgroundColor(Actor, InBackgroundColor);
		}
	}
}

void FMetaHumanCharacterEditorToolkit::OnTonemapperEnvironmentChanged(const bool InTonemapperEnabled)
{
	check(PreviewScene->GetWorld());

	if(InTonemapperEnabled)
	{
		for(ULevelStreaming* Level : PostProcessLevels)
		{
			Level->SetShouldBeVisibleInEditor(true);
		}
	}
	else
	{
		PostProcessLevels[InTonemapperEnabled]->SetShouldBeVisibleInEditor(true);
		PostProcessLevels[!InTonemapperEnabled]->SetShouldBeVisibleInEditor(false);
	}

	PreviewScene->GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
}

TNotNull<UMetaHumanCharacterEditorMode*> FMetaHumanCharacterEditorToolkit::GetMetaHumanCharacterEditorMode() const
{
	return CastChecked<UMetaHumanCharacterEditorMode>(EditorModeManager->GetActiveScriptableMode(UMetaHumanCharacterEditorMode::EM_MetaHumanCharacterEditorModeId));
}

bool FMetaHumanCharacterEditorToolkit::HasActiveTool() const
{
	return GetMetaHumanCharacterEditorMode()->GetInteractiveToolsContext()->HasActiveTool();
}

bool FMetaHumanCharacterEditorToolkit::CanRemoveFaceRig() const
{
	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter);

	return MetaHumanCharacter->HasFaceDNA();
}

void FMetaHumanCharacterEditorToolkit::RemoveFaceRig()
{
	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	if (MetaHumanCharacter && MetaHumanCharacter->HasFaceDNA())
	{
		UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
		check(MetaHumanCharacterSubsystem);

		const FScopedTransaction Transaction(MetaHumanCharacterEditorToolkitTransactionContext, LOCTEXT("CharacterRemoveRigTransaction", "Remove Face Rig"), MetaHumanCharacter);
		MetaHumanCharacter->Modify();

		TArray<uint8> DNABuffer = MetaHumanCharacter->GetFaceDNABuffer();
		TSharedRef<FMetaHumanCharacterIdentity::FState> OriginalFaceState = MetaHumanCharacterSubsystem->CopyFaceState(MetaHumanCharacter);

		// remove the rig
		MetaHumanCharacterSubsystem->RemoveFaceRig(MetaHumanCharacter);

		TUniquePtr<FRemoveFaceRigCommandChange> Change = MakeUnique<FRemoveFaceRigCommandChange>(
			DNABuffer,
			OriginalFaceState,
			MetaHumanCharacter);

		if (GUndo != nullptr)
		{
			GUndo->StoreUndo(MetaHumanCharacter, MoveTemp(Change));
		}
	}
	else
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Expected Character to have a Face DNA present");
	}
}


bool FMetaHumanCharacterEditorToolkit::CanAutoRigFace() const
{
	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter);

	// Prevent auto-rigging if PIE is running since interchange will fail to import the DNA
	const bool bIsRunningPIE = GEditor && GEditor->IsPlaySessionInProgress();
	const bool bIsRigPending = UMetaHumanCharacterEditorSubsystem::Get()->GetRiggingState(MetaHumanCharacter) == EMetaHumanCharacterRigState::RigPending;

	return !bIsRunningPIE && !bIsRigPending;
}


void FMetaHumanCharacterEditorToolkit::AutoRigFace(const UE::MetaHuman::ERigType InRigType)
{
	UMetaHumanCharacter* MetaHumanCharacter = Cast<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter);
	check(MetaHumanCharacter->IsCharacterValid());

	if (InRigType == UE::MetaHuman::ERigType::JointsAndBlendshapes)
	{
		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		constexpr uint64 GB = 1024 * 1024 * 1024;
		if (MemoryStats.AvailableVirtual < (10 * GB))
		{
			const FText NotEnoughMemoryTitle = LOCTEXT("ToolkitNotEnoughMemoryDialogTitle", "Not enough memory to auto-rig with blend shapes");
			const FText NotEnoughMemoryMessage = FText::Format(LOCTEXT("ToolkitNotEnoughMemoryDialogMessage", "Auto-rigging with blend shapes requires at least 10 GiB of free memory but only {0} is available.\n"
																									   "If you proceed the editor might crash. Would you like to continue?"),
															   FText::AsMemory(MemoryStats.AvailableVirtual));

			FSuppressableWarningDialog::FSetupInfo SetupInfo(NotEnoughMemoryMessage, NotEnoughMemoryTitle, TEXT("MetaHumanCharacterSupressNotEnoughMemory"));
			SetupInfo.ConfirmText = LOCTEXT("ToolkitNotEnoughMemoryDialogConfirmText", "Yes");
			SetupInfo.CancelText = LOCTEXT("ToolkitNotEnoughMemoryDialogCancelText", "Cancel");

			const FSuppressableWarningDialog NotEnoughMemoryDialog{ SetupInfo };

			const FSuppressableWarningDialog::EResult Result = NotEnoughMemoryDialog.ShowModal();
			if (Result == FSuppressableWarningDialog::EResult::Cancel)
			{
				return;
			}
		}
	}

	UMetaHumanCharacterEditorSubsystem::Get()->AutoRigFace(MetaHumanCharacter, InRigType);
}

bool FMetaHumanCharacterEditorToolkit::CanExportCombinedSkelMesh() const
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	if (MetaHumanCharacter->IsCharacterValid())
	{
		return true;
	}

	return false;
}

void FMetaHumanCharacterEditorToolkit::ExportCombinedSkelMesh()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DefaultPath = MetaHumanCharacter->GetPackage()->GetPathName();
	SaveAssetDialogConfig.DefaultAssetName = FString::Format(TEXT("{0}_CombinedSkelMesh"), { MetaHumanCharacter->GetName() });
	SaveAssetDialogConfig.AssetClassNames.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("ExportCombinedSkeletalMesh", "Export Combined Skeletal Mesh");

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString AssetPathAndName = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!AssetPathAndName.IsEmpty())
	{
		UMetaHumanCharacterEditorSubsystem::Get()->CreateCombinedFaceAndBodyMesh(MetaHumanCharacter, AssetPathAndName);
	}
}

bool FMetaHumanCharacterEditorToolkit::CanExportPreviewSkelMeshes() const
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();

	// TODO: check if actors are available?
	return MetaHumanCharacter->IsCharacterValid();
}

bool FMetaHumanCharacterEditorToolkit::CanSaveStates() const
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	return MetaHumanCharacter->IsCharacterValid();
}

bool FMetaHumanCharacterEditorToolkit::CanSaveFaceDNA() const
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	if (MetaHumanCharacter->IsCharacterValid())
	{
		return MetaHumanCharacter->HasFaceDNA();
	}

	return false;
}

bool FMetaHumanCharacterEditorToolkit::CanSaveBodyDNA() const
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	return MetaHumanCharacter->IsCharacterValid();
}

void FMetaHumanCharacterEditorToolkit::ExportFaceSkelMesh()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());

	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	check(MetaHumanCharacterSubsystem);

	const USkeletalMesh* FaceMeshAsset = MetaHumanCharacterSubsystem->Debug_GetFaceEditMesh(MetaHumanCharacter);
	const FString FaceSuffix = TEXT("_ExportedFace");
	UE::MetaHuman::DuplicateSkeletalMesh(MetaHumanCharacter, FaceSuffix, FaceMeshAsset);
}

void FMetaHumanCharacterEditorToolkit::ExportBodySkelMesh()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());

	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	check(MetaHumanCharacterSubsystem);

	const USkeletalMesh* BodyMeshAsset = MetaHumanCharacterSubsystem->Debug_GetBodyEditMesh(MetaHumanCharacter);
	const FString BodySuffix = TEXT("_ExportedBody");
	UE::MetaHuman::DuplicateSkeletalMesh(MetaHumanCharacter, BodySuffix, BodyMeshAsset);
}

void FMetaHumanCharacterEditorToolkit::SaveFaceState()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());

	// The serialized state is a json string
	const FSharedBuffer FaceStateData = MetaHumanCharacter->GetFaceStateData();
	UE::MetaHuman::SaveBufferToFileWithDialog(FaceStateData);
}

void FMetaHumanCharacterEditorToolkit::SaveBodyState()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());

	const FSharedBuffer BodyStateData = MetaHumanCharacter->GetBodyStateData();
	UE::MetaHuman::SaveBufferToFileWithDialog(BodyStateData);
}

void FMetaHumanCharacterEditorToolkit::SaveFaceDNA()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());

	if (MetaHumanCharacter->HasFaceDNA())
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		TArray<FString> DNAFilenames;

		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const FString DialogTitle = LOCTEXT("SaveFaceDNADialogTitle", "Save Face DNA file").ToString();
		const FString DefaultPath = TEXT("");
		const FString DefaultFile = TEXT("face.dna");
		const FString FileTypes = TEXT("*.dna");
		if (DesktopPlatform->SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultFile, DefaultPath, FileTypes, EFileDialogFlags::None, DNAFilenames))
		{
			if (DNAFilenames.Num() == 1)
			{
				TArray<uint8> FaceDNABuffer = MetaHumanCharacter->GetFaceDNABuffer();
				TSharedPtr<IDNAReader> FaceDNAReader = ReadDNAFromBuffer(&FaceDNABuffer);
				WriteDNAToFile(FaceDNAReader.Get(), EDNADataLayer::All, *DNAFilenames[0]);
				UE::MetaHuman::Analytics::RecordSaveFaceDNAEvent(MetaHumanCharacter);
			}
		}
	}
}

void FMetaHumanCharacterEditorToolkit::SaveFaceStateToDNA()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());
	TNotNull<UMetaHumanCharacterEditorSubsystem*> MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();

	TSharedPtr<IDNAReader> OutFaceStateDNAReader;
	if (MetaHumanCharacter->HasFaceDNA())
	{
		// Use the stored DNA definition to save the state out if available
		TArray<uint8> FaceDNABuffer = MetaHumanCharacter->GetFaceDNABuffer();
		TSharedPtr<IDNAReader> FaceDNAReader = ReadDNAFromBuffer(&FaceDNABuffer);
		OutFaceStateDNAReader = MetaHumanCharacterSubsystem->GetFaceState(MetaHumanCharacter)->StateToDna(FaceDNAReader->Unwrap());
	}
	else
	{
		// Otherwise, use the dna from the preview skeletal mesh
		const USkeletalMesh* ConstFaceSkeletalMesh = MetaHumanCharacterSubsystem->Debug_GetFaceEditMesh(MetaHumanCharacter);
		if (UDNAAsset* FaceDNA = const_cast<USkeletalMesh*>(ConstFaceSkeletalMesh)->GetAssetUserData<UDNAAsset>())
		{
			OutFaceStateDNAReader = MetaHumanCharacterSubsystem->GetFaceState(MetaHumanCharacter)->StateToDna(FaceDNA);
		}
	}

	if (OutFaceStateDNAReader.IsValid())
	{
		if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
		{
			const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
			const FString DialogTitle = LOCTEXT("SaveFaceStateToDNADialogTitle", "Save Face State to DNA file").ToString();
			const FString DefaultPath = TEXT("");
			const FString DefaultFile = TEXT("face_state.dna");
			const FString FileTypes = TEXT("*.dna");
			TArray<FString> DNAFilenames;
			if (DesktopPlatform->SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultFile, DefaultPath, FileTypes, EFileDialogFlags::None, DNAFilenames))
			{
				if (DNAFilenames.Num() == 1)
				{
					WriteDNAToFile(OutFaceStateDNAReader.Get(), EDNADataLayer::All, *DNAFilenames[0]);
				}
			}
		}
		else
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to retrieve Desktop Platform module");
		}
	}
	else
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to read the face DNA");
	}
}

void FMetaHumanCharacterEditorToolkit::DumpFaceStateDataForAR()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());

	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		// Prompt the user to select a folder where all the face textures will be saved
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const FString DialogTitle = LOCTEXT("SaveFaceTexturesDialogTitle", "Save Face Textures folder").ToString();
		const FString DefaultPath = FPaths::ProjectSavedDir();
		FString OutputFolder;
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, DialogTitle, DefaultPath, OutputFolder))
		{
			TNotNull<UMetaHumanCharacterEditorSubsystem*> MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
			MetaHumanCharacterSubsystem->GetFaceState(MetaHumanCharacter)->WriteDebugAutoriggingData(OutputFolder);
		}
	}
	else
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to retrieve Desktop Platform module");
	}
}

void FMetaHumanCharacterEditorToolkit::SaveBodyDNA()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());
	TNotNull<UMetaHumanCharacterEditorSubsystem*> MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();

	const USkeletalMesh* ConstBodySkeletalMesh = MetaHumanCharacterSubsystem->Debug_GetBodyEditMesh(MetaHumanCharacter);
	// Cast away const-ness for GetAssetUserData. The mesh will not be modified.
	USkeletalMesh* BodySkeletalMesh = const_cast<USkeletalMesh*>(ConstBodySkeletalMesh);
	if (UDNAAsset* BodyDNA = BodySkeletalMesh->GetAssetUserData<UDNAAsset>())
	{
		TSharedRef<IDNAReader> BodyDnaReader = MetaHumanCharacterSubsystem->GetBodyState(MetaHumanCharacter)->StateToDna(BodyDNA);

		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		TArray<FString> DNAFilenames;

		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const FString DialogTitle = LOCTEXT("SaveBodyDNADialogTitle", "Save Body DNA file").ToString();
		const FString DefaultPath = TEXT("");
		const FString DefaultFile = TEXT("body.dna");
		const FString FileTypes = TEXT("*.dna");
		if (DesktopPlatform->SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultFile, DefaultPath, FileTypes, EFileDialogFlags::None, DNAFilenames))
		{
			if (DNAFilenames.Num() == 1)
			{
				WriteDNAToFile(&BodyDnaReader.Get(), EDNADataLayer::All, *DNAFilenames[0]);
				UE::MetaHuman::Analytics::RecordSaveBodyDNAEvent(MetaHumanCharacter);
			}
		}
	}
}

bool FMetaHumanCharacterEditorToolkit::CanSaveTextures() const
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	if (MetaHumanCharacter->IsCharacterValid())
	{
		return MetaHumanCharacter->HasSynthesizedTextures();
	}

	return false;
}

bool FMetaHumanCharacterEditorToolkit::CanSaveEyePreset() const
{
	return !HasActiveTool();
}

void FMetaHumanCharacterEditorToolkit::SaveFaceTextures()
{
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter->IsCharacterValid());

	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		// Prompt the user to select a folder where all the face textures will be saved
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const FString DialogTitle = LOCTEXT("SaveFaceTexturesDialogTitle", "Save Face Textures folder").ToString();
		const FString DefaultPath = FPaths::ProjectSavedDir();
		FString OutputFolder;
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, DialogTitle, DefaultPath, OutputFolder))
		{
			const FString MetaHumanAssetName = MetaHumanCharacter->GetName();

			FScopedSlowTask SaveFaceTexturesTask(static_cast<int32>(EFaceTextureType::Count), LOCTEXT("SaveFaceTexturesTaskMessage", "Saving synthesized face textures"));
			SaveFaceTexturesTask.MakeDialog();

			for (const TPair<EFaceTextureType, FMetaHumanCharacterTextureInfo>& Pair: MetaHumanCharacter->SynthesizedFaceTexturesInfo)
			{
				const EFaceTextureType TextureType = Pair.Key;
				const FMetaHumanCharacterTextureInfo& TextureInfo = Pair.Value;

				SaveFaceTexturesTask.EnterProgressFrame();
				
				TFuture<FSharedBuffer> SynthesizedImageBuffer = MetaHumanCharacter->GetSynthesizedFaceTextureDataAsync(TextureType);
				if (!SynthesizedImageBuffer.Get().IsNull())
				{
					// Add the type of the texture as a suffix to the filename
					const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(TextureType));
					const FString OutFileName = OutputFolder / MetaHumanAssetName + TEXT("_") + TextureTypeName + TEXT(".png");

					FImageUtils::SaveImageByExtension(*OutFileName, FImageView(TextureInfo.ToImageInfo(), const_cast<void*>(SynthesizedImageBuffer.Get().GetData())));

					UE::MetaHuman::Analytics::RecordSaveHighResolutionTexturesEvent(MetaHumanCharacter);
				}
			}
		}
	}
	else
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to retrieve Desktop Platform module");
	}
}

void FMetaHumanCharacterEditorToolkit::SaveEyePreset()
{
	UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	UMetaHumanCharacterEyePresets* EyePresets = UMetaHumanCharacterEyePresets::Get();
	EyePresets->Modify();

	EyePresets->Presets.Emplace(FMetaHumanCharacterEyePreset
								{
									.EyesSettings = Character->EyesSettings
								});
}

void FMetaHumanCharacterEditorToolkit::TakeHighResScreenshot()
{
	if (ViewportClient)
	{
		// Need to reset the resolution for it to use the current viewport size
		GScreenshotResolutionX = 0;
		GScreenshotResolutionY = 0;

		ViewportClient->TakeHighResScreenShot();
	}
}

TSharedRef<SDockTab> FMetaHumanCharacterEditorToolkit::SpawnTab_AnimationBar(const FSpawnTabArgs& Args)
{
	TSharedRef<FMetaHumanCharacterViewportClient> MHCViewportClient = StaticCastSharedPtr<FMetaHumanCharacterViewportClient>(ViewportClient).ToSharedRef();
	return SNew(SDockTab)
		.CanEverClose(true)
		.Label(FText::GetEmpty())
		.ToolTipText(LOCTEXT("AnimationBarTabTooltip", "Animation Controls for MetaHuman Character"))
		[
			SNew(SMetaHumanCharacterEditorViewportAnimationBar)
				.Cursor(EMouseCursor::Default)
				.AnimationBarViewportClient(MHCViewportClient)
		];
}

TSharedRef<SDockTab> FMetaHumanCharacterEditorToolkit::SpawnTab_PreviewSceneDetails(const FSpawnTabArgs& Args)
{
	if(!PreviewSettingsWidget.IsValid())
	{
		InitPreviewSceneDetails();
	}

	return SNew(SDockTab)
		.CanEverClose(true)
		.Label(FText::GetEmpty())
		.ToolTipText(LOCTEXT("MHPreviewSettingsTooltip", "Preview Settings for MetaHuman Character"))
		[
			PreviewSettingsWidget ? PreviewSettingsWidget.ToSharedRef() : SNullWidget::NullWidget
		];
}

void FMetaHumanCharacterEditorToolkit::InitPreviewSceneDetails()
{
	TNotNull<UMetaHumanCharacterEditorSubsystem*> MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	TNotNull<UMetaHumanCharacter*> MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	AMetaHumanInvisibleDrivingActor* DrivingActor = MetaHumanCharacterSubsystem->GetInvisibleDrivingActor(MetaHumanCharacter);

	UMetaHumanCharacterEditorPreviewSceneDescription* SceneDescription = NewObject<UMetaHumanCharacterEditorPreviewSceneDescription>(MetaHumanCharacter);
	
	SceneDescription->OnAnimationControllerChanged.BindLambda([DrivingActor](EMetaHumanCharacterAnimationController AnimationController, UAnimSequence* FaceAnimSequence, UAnimSequence* BodyAnimSequence)
		{
			switch (AnimationController)
			{
			case EMetaHumanCharacterAnimationController::None:
			{
				DrivingActor->ResetAnimInstance();
				break;
			}
			case EMetaHumanCharacterAnimationController::AnimSequence:
			{
				DrivingActor->InitPreviewAnimInstance();
				DrivingActor->SetAnimation(FaceAnimSequence, BodyAnimSequence);
				break;
			}
			case EMetaHumanCharacterAnimationController::LiveLink:
			{
				DrivingActor->InitLiveLinkAnimInstance();
				break;
			}
			}
		});

	SceneDescription->OnAnimationChanged.BindLambda([DrivingActor](UAnimSequence* FaceAnimSequence, UAnimSequence* BodyAnimSequence)
	{
			DrivingActor->SetAnimation(FaceAnimSequence, BodyAnimSequence);
	});

	SceneDescription->OnPlayRateChanged.BindLambda([DrivingActor](float NewPlayRate)
	{
			DrivingActor->SetAnimationPlayRate(NewPlayRate);
	});

	SceneDescription->OnLiveLinkSubjectChanged.BindLambda([DrivingActor](FLiveLinkSubjectName LiveLinkSubjectName)
		{
			DrivingActor->SetLiveLinkSubjectNameChanged(LiveLinkSubjectName);
		});

	SceneDescription->OnPreviewModeChanged.BindLambda([MetaHumanCharacter, MetaHumanCharacterSubsystem](EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterial)
		{
			MetaHumanCharacterSubsystem->UpdateCharacterPreviewMaterial(MetaHumanCharacter, InPreviewMaterial);
		});

	// Initialize Animation on Editor start
	SceneDescription->BodyAnimationType = EMetaHumanAnimationType::TemplateAnimation;
	SceneDescription->FaceAnimationType = EMetaHumanAnimationType::TemplateAnimation;
	SceneDescription->PlayRate = 1.f;

	if (MetaHumanCharacterSubsystem->GetRiggingState(MetaHumanCharacter) != EMetaHumanCharacterRigState::Rigged)
	{
		SceneDescription->bAnimationControllerEnabled = false;
		SceneDescription->AnimationController = EMetaHumanCharacterAnimationController::None;
	}

	MetaHumanCharacterSubsystem->OnRiggingStateChanged.AddUObject(SceneDescription, &UMetaHumanCharacterEditorPreviewSceneDescription::OnRiggingStateChanged);

	// Iterate through all data table assets specified in the settings and add their animations to the list of available template animations.
	if (const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>())
	{
		for (const FSoftObjectPath& ObjectPath : Settings->TemplateAnimationDataTableAssets)
		{
			SceneDescription->AddTemplateAnimationsFromDataTable(ObjectPath);
		}
	}

	// Use the default template animation on editor startup.
	SceneDescription->BodyTemplateAnimation = SceneDescription->DefaultBodyTemplateAnimationName;
	SceneDescription->FaceTemplateAnimation = SceneDescription->DefaultFaceTemplateAnimationName;

	UAnimSequence* DefaultBodyTemplateAnimation = SceneDescription->GetTemplateAnimation(false, SceneDescription->BodyTemplateAnimation);
	UAnimSequence* DefaultFaceTemplateAnimation = SceneDescription->GetTemplateAnimation(true, SceneDescription->FaceTemplateAnimation);
	DrivingActor->SetAnimation(DefaultFaceTemplateAnimation, DefaultBodyTemplateAnimation);

	SceneDescription->OnGroomHiddenChanged.BindLambda([this](EMetaHumanPreviewAssemblyVisibility InValue)
	{
		PreviewActor->SetHairVisibilityState(InValue == EMetaHumanPreviewAssemblyVisibility::Hidden ? EMetaHumanHairVisibilityState::Hidden : EMetaHumanHairVisibilityState::Shown);		
	});

	SceneDescription->OnClothingHiddenChanged.BindLambda([MetaHumanCharacter, MetaHumanCharacterSubsystem](EMetaHumanPreviewAssemblyVisibility InValue)
	{
		EMetaHumanClothingVisibilityState ClothingVisibilityState = InValue == EMetaHumanPreviewAssemblyVisibility::Hidden ? EMetaHumanClothingVisibilityState::Hidden : EMetaHumanClothingVisibilityState::Shown;
		MetaHumanCharacterSubsystem->SetClothingVisibilityState(MetaHumanCharacter, ClothingVisibilityState, true);
	});

	SAssignNew(PreviewSettingsWidget, SMetaHumanCharacterEditorPreviewSettingsView)
			.SettingsObject(SceneDescription);
}

ULevelStreaming* FMetaHumanCharacterEditorToolkit::LoadLevelInWorld(const FSoftObjectPath& LevelPath)
{
	TSoftObjectPtr<UWorld> LevelAsset(LevelPath);

	bool bLoaded = false;
	const FString OptionalOverrideName;
	const bool bLoadAsTempPackage = true;
	TSubclassOf<ULevelStreamingDynamic> OptionalStreamingClass;
	ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(PreviewScene->GetWorld(),
		LevelAsset,
		FTransform::Identity,
		bLoaded,
		OptionalOverrideName,
		OptionalStreamingClass,
		bLoadAsTempPackage);
	check(bLoaded);
	check(StreamingLevel);

	StreamingLevel->SetShouldBeVisibleInEditor(false);

	PreviewScene->GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	ULevel* NewLevel = StreamingLevel->GetLoadedLevel();

	if (!NewLevel)
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to add lighting scenario {LightingScenario}.", *LevelPath.ToString());
		return nullptr;
	}

	NewLevel->SetLightingScenario(true);

	return StreamingLevel;
}


void FMetaHumanCharacterEditorToolkit::LoadLightingScenariosInWorld(const TArray<FSoftObjectPath>& LevelPaths)
{
	for(const FSoftObjectPath& LightingScenarioPath: LevelPaths)
	{
		LoadLevelInWorld(LightingScenarioPath);
	}

	OnLightingStudioEnvironmentChanged(PreviewActor->GetCharacter()->ViewportSettings.CharacterEnvironment);
}

void FMetaHumanCharacterEditorToolkit::LoadPostProcessScenariosInWorld(const FSoftObjectPath& BaseLevelPath, const FSoftObjectPath& TonemapperLevelPath)
{
	bool bIsTonemapperEnabled = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit()->ViewportSettings.bTonemapperEnabled;

	// Order of adding here is important!
	ULevelStreaming* BaseLevel = LoadLevelInWorld(BaseLevelPath);
	BaseLevel->SetShouldBeVisibleInEditor(true);
	PostProcessLevels.Add(BaseLevel);
	
	ULevelStreaming* TonemapperLevel = LoadLevelInWorld(TonemapperLevelPath);
	TonemapperLevel->SetShouldBeVisibleInEditor(bIsTonemapperEnabled);
	PostProcessLevels.Add(TonemapperLevel);
}

void FMetaHumanCharacterEditorToolkit::RefreshPreview()
{
	UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacterAssetEditor>(OwningAssetEditor)->GetObjectToEdit();
	check(MetaHumanCharacter);
	UMetaHumanCharacterEditorSubsystem::Get()->RunCharacterEditorPipelineForPreview(MetaHumanCharacter);
}

#undef LOCTEXT_NAMESPACE

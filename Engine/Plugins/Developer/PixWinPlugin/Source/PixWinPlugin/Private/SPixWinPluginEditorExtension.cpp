// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPixWinPluginEditorExtension.h"

#if WITH_EDITOR

#include "PixWinPluginCommands.h"
#include "PixWinPluginModule.h"
#include "PixWinPluginStyle.h"

#include "Editor/EditorEngine.h"
#include "SEditorViewportToolBarMenu.h"
#include "SViewportToolBarComboMenu.h"
#include "Kismet2/DebuggerCommands.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Interfaces/IMainFrameModule.h"
#include "LevelEditor.h"
#include "RHI.h"
#include "ToolMenus.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

extern UNREALED_API UEditorEngine* GEditor;

class SPixWinCaptureButton : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SPixWinCaptureButton) { }
	SLATE_END_ARGS()

	/** Widget constructor */
	void Construct(const FArguments& Args)
	{
		FSlateIcon IconBrush = FSlateIcon(FPixWinPluginStyle::Get()->GetStyleSetName(), "PixWinPlugin.CaptureFrame");

		ChildSlot
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(1.0f))
			.ToolTipText(FPixWinPluginCommands::Get().CaptureFrame->GetDescription())
			.OnClicked_Lambda([this]()
			{
				FPlayWorldCommands::GlobalPlayWorldActions->GetActionForCommand(FPixWinPluginCommands::Get().CaptureFrame)->Execute();
				return(FReply::Handled());
			})
			[
				SNew(SImage)
				.Image(IconBrush.GetIcon())
			]
		];
	}
};

FPixWinPluginEditorExtension::FPixWinPluginEditorExtension(FPixWinPluginModule* ThePlugin)
	: ToolbarExtension()
	, ExtensionManager()
	, ToolbarExtender()
{
	Initialize(ThePlugin);
}

FPixWinPluginEditorExtension::~FPixWinPluginEditorExtension()
{
	if (ExtensionManager.IsValid())
	{
		FPixWinPluginStyle::Shutdown();
		FPixWinPluginCommands::Unregister();

		ToolbarExtender->RemoveExtension(ToolbarExtension.ToSharedRef());

		ExtensionManager->RemoveExtender(ToolbarExtender);
	}
	else
	{
		ExtensionManager.Reset();
	}
}

void FPixWinPluginEditorExtension::Initialize(FPixWinPluginModule* ThePlugin)
{
	if (GUsingNullRHI)
	{
		UE_LOG(PixWinPlugin, Display, TEXT("PixWin Plugin will not be loaded because a Null RHI (Cook Server, perhaps) is being used."));
		return;
	}

	// The LoadModule request below will crash if running as an editor commandlet!
	check(!IsRunningCommandlet());

	FPixWinPluginStyle::Initialize();
	FPixWinPluginCommands::Register();

#if WITH_EDITOR
	if (!IsRunningGame())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedRef<FUICommandList> CommandBindings = LevelEditorModule.GetGlobalLevelEditorActions();
		ExtensionManager = LevelEditorModule.GetToolBarExtensibilityManager();
		ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtension = ToolbarExtender->AddToolBarExtension("CameraSpeed", EExtensionHook::After, CommandBindings,
			FToolBarExtensionDelegate::CreateLambda([this, ThePlugin](FToolBarBuilder& ToolbarBuilder)
				{
					AddToolbarExtension(ToolbarBuilder, ThePlugin);
				})
		);
		ExtensionManager->AddExtender(ToolbarExtender);
		
		ExtendToolbar();
	}
#endif // WITH_EDITOR

	// Would be nice to use the preprocessor definition WITH_EDITOR instead, but the user may launch a standalone the game through the editor...
	if (GEditor != nullptr)
	{
		check(FPlayWorldCommands::GlobalPlayWorldActions.IsValid());

		//Register the editor hotkeys
		FPlayWorldCommands::GlobalPlayWorldActions->MapAction(FPixWinPluginCommands::Get().CaptureFrame,
			FExecuteAction::CreateLambda([]()
				{
					FPixWinPluginModule& PluginModule = FModuleManager::GetModuleChecked<FPixWinPluginModule>("PixWinPlugin");
					PluginModule.CaptureFrame(nullptr, IRenderCaptureProvider::ECaptureFlags_Launch, FString());
				}),
			FCanExecuteAction());
	}
}

void FPixWinPluginEditorExtension::ExtendToolbar()
{
	FToolMenuOwnerScoped ScopedOwner(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.ViewportToolbar");
	
	FToolMenuSection& RightSection = Menu->FindOrAddSection("Right");
	FToolMenuEntry& Entry = RightSection.AddMenuEntry(FPixWinPluginCommands::Get().CaptureFrame);
	Entry.ToolBarData.LabelOverride = FText::GetEmpty();
	Entry.InsertPosition.Position = EToolMenuInsertType::First;
}

void FPixWinPluginEditorExtension::AddToolbarExtension(FToolBarBuilder& ToolbarBuilder, FPixWinPluginModule* ThePlugin)
{
#define LOCTEXT_NAMESPACE "LevelEditorToolBar"

	UE_LOG(PixWinPlugin, Verbose, TEXT("Attaching toolbar extension..."));
	
	TAttribute<EVisibility> Visibility;
	Visibility.BindLambda([]
	{
		return UE::UnrealEd::ShowOldViewportToolbars() ? EVisibility::Visible : EVisibility::Collapsed;
	});

	ToolbarBuilder.BeginSection("PixWinPlugin", false);
	ToolbarBuilder.AddWidget(
		SNew(SPixWinCaptureButton),
		NAME_None,
		true,
		HAlign_Fill,
		FNewMenuDelegate(),
		Visibility
	);
	ToolbarBuilder.EndSection();

#undef LOCTEXT_NAMESPACE
}

#endif //WITH_EDITOR

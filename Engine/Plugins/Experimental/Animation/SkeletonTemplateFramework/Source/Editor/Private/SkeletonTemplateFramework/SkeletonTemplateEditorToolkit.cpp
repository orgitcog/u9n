// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonTemplateEditorToolkit.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SkeletonTemplateFramework/SkeletonTemplate.h"
#include "SkeletonTemplateFramework/SkeletonTemplateNamedAttributesEditor.h"
#include "SkeletonTemplateFramework/SkeletonTemplateNamedAttributeSetsEditor.h"
#include "SkeletonTemplateFramework/SkeletonTemplateNamedAttributeMappingsEditor.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SkeletonTemplateEditorToolkit"

namespace UE::Anim::STF
{
	struct FEditorTabs
	{
		static const FName AttributesId;
		static const FName AttributeSetsId;
		static const FName AttributeMappingsId;
		static const FName DetailsId;
	};
}

const FName UE::Anim::STF::FEditorTabs::AttributesId(TEXT("Attributes"));
const FName UE::Anim::STF::FEditorTabs::AttributeSetsId(TEXT("AttributeSets"));
const FName UE::Anim::STF::FEditorTabs::AttributeMappingsId(TEXT("AttributeMappings"));
const FName UE::Anim::STF::FEditorTabs::DetailsId(TEXT("Details"));

void FSkeletonTemplateEditorToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	using namespace UE::Anim::STF;

	check(!InObjects.IsEmpty())
	SkeletonTemplate = CastChecked<USkeletonTemplate>(InObjects[0]);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("SkeletonTemplateEditorLayout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.25f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(FEditorTabs::AttributesId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(FEditorTabs::AttributeSetsId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.5f)
				->AddTab(FEditorTabs::AttributeMappingsId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->AddTab(FEditorTabs::DetailsId, ETabState::OpenedTab)
			)
		);

	FAssetEditorToolkit::InitAssetEditor(EToolkitMode::Standalone, {}, "SkeletonTemplateEditor", Layout, true, true, InObjects);
}

void FSkeletonTemplateEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	using namespace UE::Anim::STF;

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "Skeleton Template Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	InTabManager->RegisterTabSpawner(FEditorTabs::AttributesId, FOnSpawnTab::CreateSP(this, &FSkeletonTemplateEditorToolkit::SpawnTabAttributes))
		.SetDisplayName(LOCTEXT("AttributesTabMenu_Description", "Attributes"))
		.SetTooltipText(LOCTEXT("AttributesTabMenu_ToolTip", "Shows the attributes panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(FEditorTabs::AttributeSetsId, FOnSpawnTab::CreateSP(this, &FSkeletonTemplateEditorToolkit::SpawnTabAttributeSets))
		.SetDisplayName(LOCTEXT("AttributeSetsTabMenu_Description", "Attribute Sets"))
		.SetTooltipText(LOCTEXT("AttributeSetsTabMenu_ToolTip", "Shows the attribute sets panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	
	InTabManager->RegisterTabSpawner(FEditorTabs::AttributeMappingsId, FOnSpawnTab::CreateSP(this, &FSkeletonTemplateEditorToolkit::SpawnTabAttributeMappings))
		.SetDisplayName(LOCTEXT("AttributeMappingsTabMenu_Description", "Attribute Mappings"))
		.SetTooltipText(LOCTEXT("AttributeMappingsTabMenu_ToolTip", "Shows the attribute mappings view panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(FEditorTabs::DetailsId, FOnSpawnTab::CreateSP(this, &FSkeletonTemplateEditorToolkit::SpawnTabDetails))
		.SetDisplayName(LOCTEXT("DetailsTabMenu_Description", "Details"))
		.SetTooltipText(LOCTEXT("DetailsTabMenu_Tooltip", "Shows the details panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FSkeletonTemplateEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	using namespace UE::Anim::STF;

	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FEditorTabs::AttributesId);
	InTabManager->UnregisterTabSpawner(FEditorTabs::AttributeSetsId);
	InTabManager->UnregisterTabSpawner(FEditorTabs::AttributeMappingsId);
	InTabManager->UnregisterTabSpawner(FEditorTabs::DetailsId);
}

FName FSkeletonTemplateEditorToolkit::GetToolkitFName() const
{
	return "SkeletonTemplateEditor";
}

FText FSkeletonTemplateEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Skeleton Template Editor");
}

FString FSkeletonTemplateEditorToolkit::GetWorldCentricTabPrefix() const
{
	return "SkeletonTemplateEditor";
}

FLinearColor FSkeletonTemplateEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FSkeletonTemplateEditorToolkit::SetDetailsObject(TObjectPtr<UObject> InObject)
{
	if (InObject)
	{
		DetailsView->SetObjects(TArray{ InObject });
	}
	else
	{
		DetailsView->SetObjects(TArray<TObjectPtr<UObject>>{ SkeletonTemplate });
	}
}

TSharedRef<SDockTab> FSkeletonTemplateEditorToolkit::SpawnTabAttributes(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SAssignNew(AttributesTreeView, UE::Anim::STF::SAttributesTreeView, SkeletonTemplate)
		];
}

TSharedRef<SDockTab> FSkeletonTemplateEditorToolkit::SpawnTabAttributeSets(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SAssignNew(AttributeSetsTreeView, UE::Anim::STF::SAttributeSetsTreeView, SkeletonTemplate)
				.OnNamedAttributeSetsChanged_Lambda([this]()
				{
					if (AttributeMappingsTreeView)
					{
						AttributeMappingsTreeView->OnNamedAttributeSetsChanged();
					}
				})
		];
}

TSharedRef<SDockTab> FSkeletonTemplateEditorToolkit::SpawnTabAttributeMappings(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SAssignNew(AttributeMappingsTreeView, UE::Anim::STF::SAttributeMappingsTreeView, SkeletonTemplate, this)
		];
}

TSharedRef<SDockTab> FSkeletonTemplateEditorToolkit::SpawnTabDetails(const FSpawnTabArgs& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowObjectLabel = false;
	DetailsViewArgs.bAllowFavoriteSystem = false;
	
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ SkeletonTemplate });
	
	return SNew(SDockTab)
		[
			DetailsView.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
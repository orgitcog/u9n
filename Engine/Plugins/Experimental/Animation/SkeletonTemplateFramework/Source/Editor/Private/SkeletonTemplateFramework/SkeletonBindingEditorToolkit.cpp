// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonBindingEditorToolkit.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SkeletonTemplateFramework/SkeletonBinding.h"
#include "SkeletonTemplateFramework/SkeletonBindingNamedAttributesEditor.h"
#include "SkeletonTemplateFramework/SkeletonBindingNamedAttributeSetsEditor.h"
#include "SkeletonTemplateFramework/SkeletonBindingNamedAttributeMappingsEditor.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SkeletonBindingEditorToolkit"

namespace UE::Anim::STF
{
	struct FSkeletonBindingEditorTabs
	{
		static const FName AttributesId;
		static const FName AttributeSetsId;
		static const FName AttributeMappingsId;
		static const FName DetailsId;
	};
}

const FName UE::Anim::STF::FSkeletonBindingEditorTabs::AttributesId(TEXT("Attributes"));
const FName UE::Anim::STF::FSkeletonBindingEditorTabs::AttributeSetsId(TEXT("AttributeSets"));
const FName UE::Anim::STF::FSkeletonBindingEditorTabs::AttributeMappingsId(TEXT("AttributeMappings"));
const FName UE::Anim::STF::FSkeletonBindingEditorTabs::DetailsId(TEXT("Details"));

void FSkeletonBindingEditorToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	using namespace UE::Anim::STF;

	check(!InObjects.IsEmpty())
	SkeletonBinding = CastChecked<USkeletonBinding>(InObjects[0]);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("SkeletonBindingEditorLayout")
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
					->AddTab(FSkeletonBindingEditorTabs::AttributesId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(FSkeletonBindingEditorTabs::AttributeSetsId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.5f)
				->AddTab(FSkeletonBindingEditorTabs::AttributeMappingsId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->AddTab(FSkeletonBindingEditorTabs::DetailsId, ETabState::OpenedTab)
			)
		);

	FAssetEditorToolkit::InitAssetEditor(EToolkitMode::Standalone, {}, "SkeletonBindingEditor", Layout, true, true, InObjects);
}

void FSkeletonBindingEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	using namespace UE::Anim::STF;

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "Skeleton Binding Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	InTabManager->RegisterTabSpawner(FSkeletonBindingEditorTabs::AttributesId, FOnSpawnTab::CreateSP(this, &FSkeletonBindingEditorToolkit::SpawnTabAttributes))
		.SetDisplayName(LOCTEXT("AttributesTabMenu_Description", "Attributes"))
		.SetTooltipText(LOCTEXT("AttributesTabMenu_ToolTip", "Shows the attributes panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(FSkeletonBindingEditorTabs::AttributeSetsId, FOnSpawnTab::CreateSP(this, &FSkeletonBindingEditorToolkit::SpawnTabAttributeSets))
		.SetDisplayName(LOCTEXT("AttributeSetsTabMenu_Description", "Attribute Sets"))
		.SetTooltipText(LOCTEXT("AttributeSetsTabMenu_ToolTip", "Shows the attribute sets panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	
	InTabManager->RegisterTabSpawner(FSkeletonBindingEditorTabs::AttributeMappingsId, FOnSpawnTab::CreateSP(this, &FSkeletonBindingEditorToolkit::SpawnTabAttributeMappings))
		.SetDisplayName(LOCTEXT("AttributeMappingsTabMenu_Description", "Attribute Mappings"))
		.SetTooltipText(LOCTEXT("AttributeMappingsTabMenu_Tooltip", "Shows the attribute mappings view panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(FSkeletonBindingEditorTabs::DetailsId, FOnSpawnTab::CreateSP(this, &FSkeletonBindingEditorToolkit::SpawnTabDetails))
		.SetDisplayName(LOCTEXT("DetailsTabMenu_Description", "Details"))
		.SetTooltipText(LOCTEXT("DetailsTabMenu_Tooltip", "Shows the details panel"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FSkeletonBindingEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	using namespace UE::Anim::STF;

	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FSkeletonBindingEditorTabs::AttributesId);
	InTabManager->UnregisterTabSpawner(FSkeletonBindingEditorTabs::AttributeSetsId);
	InTabManager->UnregisterTabSpawner(FSkeletonBindingEditorTabs::AttributeMappingsId);
	InTabManager->UnregisterTabSpawner(FSkeletonBindingEditorTabs::DetailsId);
}

FName FSkeletonBindingEditorToolkit::GetToolkitFName() const
{
	return "SkeletonBindingEditor";
}

FText FSkeletonBindingEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Skeleton Binding Editor");
}

FString FSkeletonBindingEditorToolkit::GetWorldCentricTabPrefix() const
{
	return "SkeletonBindingEditor";
}

FLinearColor FSkeletonBindingEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FSkeletonBindingEditorToolkit::SetDetailsObject(TObjectPtr<UObject> InObject)
{
	if (InObject)
	{
		DetailsView->SetObjects(TArray{ InObject });
	}
	else
	{
		DetailsView->SetObjects(TArray<TObjectPtr<UObject>>{ SkeletonBinding });
	}
}

TSharedRef<SDockTab> FSkeletonBindingEditorToolkit::SpawnTabAttributes(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SAssignNew(AttributeBindingsTreeView, UE::Anim::STF::SAttributeBindingsTreeView, SkeletonBinding)
		];
}

TSharedRef<SDockTab> FSkeletonBindingEditorToolkit::SpawnTabAttributeSets(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SAssignNew(BindingSetsTreeView, UE::Anim::STF::SBindingSetsTreeView, SkeletonBinding)
		];
}

TSharedRef<SDockTab> FSkeletonBindingEditorToolkit::SpawnTabAttributeMappings(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SAssignNew(BindingMappingsTreeView, UE::Anim::STF::SBindingMappingsTreeView, SkeletonBinding, this)
		];
}

TSharedRef<SDockTab> FSkeletonBindingEditorToolkit::SpawnTabDetails(const FSpawnTabArgs& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ SkeletonBinding });
	
	return SNew(SDockTab)
		[
			DetailsView.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
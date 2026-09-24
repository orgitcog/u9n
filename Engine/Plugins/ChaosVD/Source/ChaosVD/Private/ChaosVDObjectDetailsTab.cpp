// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDObjectDetailsTab.h"

#include "ChaosVDCollisionDataDetailsTab.h"
#include "ChaosVDScene.h"
#include "ChaosVDStyle.h"
#include "ChaosVDTabsIDs.h"
#include "Editor.h"
#include "Elements/Actor/ActorElementData.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "TEDS/ChaosVDSelectionInterface.h"
#include "TEDS/ChaosVDStructTypedElementData.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SChaosVDCollisionDataInspector.h"
#include "Widgets/SChaosVDDetailsView.h"
#include "Widgets/SChaosVDMainTab.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void FChaosVDStandAloneObjectDetailsTab::AddUnsupportedStruct(const UStruct* Struct)
{
	UnsupportedStructs.Add(Struct);
}

bool FChaosVDStandAloneObjectDetailsTab::IsSupportedStruct(const TWeakObjectPtr<const UStruct>& InWeakStructPtr)
{
	return !UnsupportedStructs.Contains(InWeakStructPtr);
}

TSharedRef<SDockTab> FChaosVDStandAloneObjectDetailsTab::HandleTabSpawnRequest(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DetailsPanelTab =
	SNew(SDockTab)
	.TabRole(ETabRole::PanelTab)
	.Label(LOCTEXT("DetailsPanel", "Details"))
	.ToolTipText(LOCTEXT("DetailsPanelToolTip", "See the details of the selected object"));

	// The following types have their own data inspectors, we should not open them in the details pannel
	AddUnsupportedStruct(FChaosVDConstraintDataWrapperBase::StaticStruct());
	AddUnsupportedStruct(FChaosVDQueryDataWrapper::StaticStruct());
	AddUnsupportedStruct(FChaosVDParticlePairMidPhase::StaticStruct());

	if (const TSharedPtr<SChaosVDMainTab> MainTabPtr = OwningTabWidget.Pin())
	{
		DetailsPanelTab->SetContent
		(
			SAssignNew(DetailsPanelView, SChaosVDDetailsView, MainTabPtr.ToSharedRef())
		);
	}
	else
	{
		DetailsPanelTab->SetContent(GenerateErrorWidget());
	}

	DetailsPanelTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconDetailsPanel"));
	
	HandleTabSpawned(DetailsPanelTab);
	
	return DetailsPanelTab;
}

void FChaosVDStandAloneObjectDetailsTab::HandleTabClosed(TSharedRef<SDockTab> InTabClosed)
{
	FChaosVDTabSpawnerBase::HandleTabClosed(InTabClosed);
	
	DetailsPanelView.Reset();
}

void FChaosVDObjectDetailsTab::HandleActorsSelection(TArrayView<AActor*> SelectedActors)
{
	if (SelectedActors.Num() > 0)
	{
		// We don't support multi selection yet
		ensure(SelectedActors.Num() == 1);

		CurrentSelectedObject = SelectedActors[0];

		if (DetailsPanelView)
		{
			DetailsPanelView->SetSelectedObject(CurrentSelectedObject.Get());
		}
	}
	else
	{
		CurrentSelectedObject = nullptr;
	}
}

TSharedRef<SDockTab> FChaosVDObjectDetailsTab::HandleTabSpawnRequest(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> NewTab = FChaosVDStandAloneObjectDetailsTab::HandleTabSpawnRequest(Args);
	
	if (const TSharedPtr<FChaosVDScene> ScenePtr = GetChaosVDScene().Pin())
	{
		RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());

		if (TSharedPtr<FChaosVDSolverDataSelection> SolverDataSelectionObject = ScenePtr->GetSolverDataSelectionObject().Pin())
		{
			SolverDataSelectionObject->GetDataSelectionChangedDelegate().AddSP(this, &FChaosVDObjectDetailsTab::HandleSolverDataSelectionChange);
		}
	}

	// If we closed the tab and opened it again with an object already selected, try to restore the selected object view
	if (DetailsPanelView.IsValid() && CurrentSelectedObject.IsValid())
	{
		DetailsPanelView->SetSelectedObject(CurrentSelectedObject.Get());
	}
	
	return NewTab;
}

void FChaosVDObjectDetailsTab::HandleTabClosed(TSharedRef<SDockTab> InTabClosed)
{
	if (const TSharedPtr<FChaosVDScene> ScenePtr = GetChaosVDScene().Pin())
	{
		if (TSharedPtr<FChaosVDSolverDataSelection> SolverDataSelectionObject = ScenePtr->GetSolverDataSelectionObject().Pin())
		{
			SolverDataSelectionObject->GetDataSelectionChangedDelegate().RemoveAll(this);
		}
	}

	FChaosVDStandAloneObjectDetailsTab::HandleTabClosed(InTabClosed);
}

void FChaosVDObjectDetailsTab::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet)
{
	TArray<AActor*> SelectedActors = ChangedSelectionSet->GetSelectedObjects<AActor>();
	if (SelectedActors.Num() > 0)
	{
		HandleActorsSelection(SelectedActors);
		return;
	}

	constexpr int32 MaxElements = 1;
	TArray<FTypedElementHandle, TInlineAllocator<MaxElements>> SelectedParticlesHandles;
	ChangedSelectionSet->GetSelectedElementHandles(SelectedParticlesHandles, UChaosVDSelectionInterface::StaticClass());

	if (SelectedParticlesHandles.Num() > 0)
	{
		// We don't support multi selection yet
		ensure(SelectedParticlesHandles.Num() == MaxElements);

		using namespace Chaos::VD::TypedElementDataUtil;
	
		DetailsPanelView->SetSelectedStruct(GetStructOnScopeDataFromTypedElementHandle(SelectedParticlesHandles[0]));
		return;
	}

	DetailsPanelView->SetSelectedObject(nullptr);
	DetailsPanelView->SetSelectedStruct(nullptr);
}

void FChaosVDObjectDetailsTab::HandleSolverDataSelectionChange(const TSharedPtr<FChaosVDSolverDataSelectionHandle>& SelectionHandle)
{
	TSharedPtr<FStructOnScope> StructOnScope = SelectionHandle ? SelectionHandle->GetDataAsStructScope() : nullptr;
	if (!StructOnScope || !IsSupportedStruct(StructOnScope->GetStructPtr()))
	{
		DetailsPanelView->SetSelectedStruct(nullptr);
		return;
	}

	HandleActorsSelection(TArrayView<AActor*>());

	DetailsPanelView->SetSelectedStruct(SelectionHandle->GetCustomDataReadOnlyStructViewForDetails());
}

#undef LOCTEXT_NAMESPACE

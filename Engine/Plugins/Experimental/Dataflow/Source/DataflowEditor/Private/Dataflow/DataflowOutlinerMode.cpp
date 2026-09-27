// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowOutlinerMode.h"
#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "AssetEditorModeManager.h"
#include "EditorViewportCommands.h"
#include "TedsOutlinerItem.h"
#include "Selection.h"
#include "Components/PrimitiveComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowElement.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementVisibilityColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"

struct FTypedElementScriptStructTypeInfoColumn;

namespace UE::Dataflow::Private
{
	template<typename ObjectType>
	static ObjectType* GetOutlinerItemObject(const TWeakPtr<ISceneOutlinerTreeItem>& WeakTreeItem)
	{
		using namespace UE::Editor::Outliner;
		using namespace UE::Editor::DataStorage;

		if (const TSharedPtr<ISceneOutlinerTreeItem> TreeItem = WeakTreeItem.Pin())
		{
			if (FTedsOutlinerTreeItem* TEDSItem = TreeItem->CastTo<FTedsOutlinerTreeItem>())
			{
				ICoreProvider* Storage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
				if (TEDSItem->IsValid() && Storage)
				{
					const FTypedElementUObjectColumn* RawObjectColumn = Storage->GetColumn<FTypedElementUObjectColumn>(TEDSItem->GetRowHandle());
					return RawObjectColumn ? Cast<ObjectType>(RawObjectColumn->Object) : nullptr;
				}
			}
		}
	
		return nullptr;
	}

	template<typename ObjectType>
	static ObjectType* GetOutlinerItemStruct(const TWeakPtr<ISceneOutlinerTreeItem>& WeakTreeItem)
	{
		using namespace UE::Editor::Outliner;
		using namespace UE::Editor::DataStorage;
		
		if (const TSharedPtr<ISceneOutlinerTreeItem> TreeItem = WeakTreeItem.Pin())
		{
			if (FTedsOutlinerTreeItem* TEDSItem = TreeItem->CastTo<FTedsOutlinerTreeItem>())
			{
				ICoreProvider* Storage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
				if (TEDSItem->IsValid() && Storage)
				{
					const FTypedElementExternalObjectColumn* RawObjectColumn = Storage->GetColumn<FTypedElementExternalObjectColumn>(TEDSItem->GetRowHandle());
					const FTypedElementScriptStructTypeInfoColumn* TypeInfoColumn = Storage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(TEDSItem->GetRowHandle());
					if (RawObjectColumn && TypeInfoColumn)
					{
						if (RawObjectColumn->Object && TypeInfoColumn->TypeInfo == ObjectType::StaticStruct())
						{
							return static_cast<ObjectType*>(RawObjectColumn->Object);
						}
					}
				}
			}
		}
	
		return nullptr;
	}
	
	static void SetOutlinerItemVisibility(const TWeakPtr<ISceneOutlinerTreeItem>& WeakTreeItem, const bool bIsVisible)
	{
		using namespace UE::Editor::Outliner;
		using namespace UE::Editor::DataStorage;

		if (const TSharedPtr<ISceneOutlinerTreeItem> TreeItem = WeakTreeItem.Pin())
		{
			if (FTedsOutlinerTreeItem* TEDSItem = TreeItem->CastTo<FTedsOutlinerTreeItem>())
			{
				ICoreProvider* Storage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
				if (TEDSItem->IsValid() && Storage)
				{
					if(FVisibleInEditorColumn* VisibilityColumn = Storage->GetColumn<FVisibleInEditorColumn>(TEDSItem->GetRowHandle()))
					{
						VisibilityColumn->bIsVisibleInEditor = bIsVisible;
					}
				}
			}
		}
	}

	/** Functor which can be used to get dataflow actors from a selection */
	template<typename ObjectType>
	struct FDataflowObjectSelector
	{
		FDataflowObjectSelector()
		{}

		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& TreeItem, ObjectType*& SceneObject) const
		{
			if (ObjectType* ItemObject = GetOutlinerItemObject<ObjectType>(TreeItem))
			{
				SceneObject = ItemObject;
				return true;
			}
			return false;
		}
	};
	
	/** Functor which can be used to get dataflow actors from a selection */
	template<typename ObjectType>
	struct FDataflowStructSelector
	{
		FDataflowStructSelector()
		{}

		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& TreeItem, ObjectType*& SceneObject) const
		{
			if (ObjectType* ItemObject = GetOutlinerItemStruct<ObjectType>(TreeItem))
			{
				SceneObject = ItemObject;
				return true;
			}
			return false;
		}
	};
	
	static void UpdateSceneSelection(const FSceneOutlinerItemSelection& Selection, FDataflowPreviewSceneBase* PreviewScene, USelection* SceneSelection)
	{
		// Get the selected components in TEDS
		TArray<UPrimitiveComponent*> SelectedComponents = Selection.GetData<UPrimitiveComponent*>(UE::Dataflow::Private::FDataflowObjectSelector<UPrimitiveComponent>());

		// Unselect all previous components
		TArray<UPrimitiveComponent*> PreviousSelection;
		SceneSelection->GetSelectedObjects<UPrimitiveComponent>(PreviousSelection);

		SceneSelection->Modify();
		SceneSelection->BeginBatchSelectOperation();
		SceneSelection->DeselectAll();

		// Transfer components selection from TEDS
		for(UPrimitiveComponent* SelectedComponent : SelectedComponents)
		{
			if(SelectedComponent->GetWorld() == PreviewScene->GetWorld())
			{
				SceneSelection->Select(SelectedComponent);
				SelectedComponent->PushSelectionToProxy();
			}
		}
		SceneSelection->EndBatchSelectOperation();

		// Update the previous proxies 
		for(UPrimitiveComponent* PreviousComponent : PreviousSelection)
		{
			if(PreviousComponent->GetWorld() == PreviewScene->GetWorld())
			{
				PreviousComponent->PushSelectionToProxy();
			}
		}

		// Get the selected elements in TEDS
		TArray<FDataflowBaseElement*> SelectedElements = Selection.GetData<FDataflowBaseElement*>(UE::Dataflow::Private::FDataflowStructSelector<FDataflowBaseElement>());

		// Unselect all previous elements
		for(IDataflowDebugDrawInterface::FDataflowElementsType::ElementType& SelectedElement : PreviewScene->ModifySceneElements())
		{
			if(SelectedElement.IsValid())
			{
				SelectedElement->bIsSelected = false;
			}
		}

		// Transfer elements selection from TEDS
		for(FDataflowBaseElement* SelectedElement : SelectedElements)
		{
			if(SelectedElement)
			{
				SelectedElement->bIsSelected = true;
			}
		}
	}
}

FDataflowOutlinerMode::FDataflowOutlinerMode(const UE::Editor::Outliner::FTedsOutlinerParams& InModeParams,
		FDataflowPreviewSceneBase* InConstructionScene, FDataflowPreviewSceneBase* InSimulationScene)
	: FTedsOutlinerMode(InModeParams)
	, ConstructionScene(InConstructionScene)
	, SimulationScene(InSimulationScene)
{
	if (SceneOutliner)
	{
		TAttribute<bool> ConditionalEnabledAttribute(true);
		ConditionalEnabledAttribute.BindRaw(this, &FDataflowOutlinerMode::CanPopulate);

		SceneOutliner->SetEnabled(ConditionalEnabledAttribute);
	}
}

FDataflowOutlinerMode::~FDataflowOutlinerMode()
{
	if (SceneOutliner)
	{
		TAttribute<bool> EmptyConditionalEnabledAttribute;
		SceneOutliner->SetEnabled(EmptyConditionalEnabledAttribute);
	}
}

void FDataflowOutlinerMode::OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection)
{
	if ((SelectionType == ESelectInfo::Direct) || !ConstructionScene || !SimulationScene)
	{
		return;
	}
	if(TSharedPtr<FAssetEditorModeManager> ConstructionManager = ConstructionScene->GetDataflowModeManager())
	{
		UE::Dataflow::Private::UpdateSceneSelection(Selection, ConstructionScene, ConstructionManager->GetSelectedComponents());
	}
	if(TSharedPtr<FAssetEditorModeManager> SimulationManager = SimulationScene->GetDataflowModeManager())
	{
		UE::Dataflow::Private::UpdateSceneSelection(Selection, SimulationScene, SimulationManager->GetSelectedComponents());
	}
}

void FDataflowOutlinerMode::OnItemAdded(FSceneOutlinerTreeItemPtr Item)
{
	SceneOutliner->SetItemExpansion(Item, false);
	Item->Flags.bIsExpanded = false;
}

void FDataflowOutlinerMode::OnItemDoubleClick(FSceneOutlinerTreeItemPtr SelectedItem)
{
	if (!ConstructionScene || !SimulationScene)
	{
		return;
	}

	if(const UPrimitiveComponent* SelectedComponent = UE::Dataflow::Private::GetOutlinerItemObject<UPrimitiveComponent>(SelectedItem))
	{
		if(SelectedComponent->GetWorld() == ConstructionScene->GetWorld())
		{
			ConstructionScene->OnFocusRequest().Broadcast(SelectedComponent->Bounds.GetBox());
		}
		else if(SelectedComponent->GetWorld() == SimulationScene->GetWorld())
		{
			SimulationScene->OnFocusRequest().Broadcast(SelectedComponent->Bounds.GetBox());
		}
	}
	else if(FDataflowBaseElement* SelectedElement = UE::Dataflow::Private::GetOutlinerItemStruct<FDataflowBaseElement>(SelectedItem))
	{
		if(SelectedElement->bIsConstruction)
		{
			ConstructionScene->OnFocusRequest().Broadcast(SelectedElement->BoundingBox);
		}
		else
		{
			SimulationScene->OnFocusRequest().Broadcast(SelectedElement->BoundingBox);
		}
	}
}

FReply FDataflowOutlinerMode::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	if(SceneOutliner)
	{
		const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
		const FInputChord CheckChord( InKeyEvent.GetKey(), EModifierKey::FromBools(ModifierKeys.IsControlDown(), ModifierKeys.IsAltDown(), ModifierKeys.IsShiftDown(), ModifierKeys.IsCommandDown()) );

		const FSceneOutlinerItemSelection& Selection = SceneOutliner->GetSelection();
	
		// Use the keyboard shortcut bound to 'Focus Viewport To Selection'
		if (FEditorViewportCommands::Get().FocusViewportToSelection->HasActiveChord(CheckChord))
		{
			FBox ConstructionBox, SimulationBox;
			for(TWeakPtr<ISceneOutlinerTreeItem>& SelectedItem: Selection.SelectedItems)
			{
				if(const UPrimitiveComponent* SelectedComponent = UE::Dataflow::Private::GetOutlinerItemObject<UPrimitiveComponent>(SelectedItem))
				{
					if(SelectedComponent->GetWorld() == ConstructionScene->GetWorld())
					{
						ConstructionBox += SelectedComponent->Bounds.GetBox();
					}
					else if(SelectedComponent->GetWorld() == SimulationScene->GetWorld())
					{
						SimulationBox += SelectedComponent->Bounds.GetBox();
					}
				}
				else if(FDataflowBaseElement* SelectedElement = UE::Dataflow::Private::GetOutlinerItemStruct<FDataflowBaseElement>(SelectedItem))
				{
					if(SelectedElement->bIsConstruction)
					{
						ConstructionBox += SelectedElement->BoundingBox;
					}
					else
					{
						SimulationBox += SelectedElement->BoundingBox;
					}
				}
			}
			if(ConstructionBox.IsValid)
			{
				ConstructionScene->OnFocusRequest().Broadcast(ConstructionBox);
			}
			if(SimulationBox.IsValid)
			{
				SimulationScene->OnFocusRequest().Broadcast(SimulationBox);
			}
		}
		else if (InKeyEvent.GetKey() == EKeys::H)
		{
			const bool bIsVisible = InKeyEvent.IsControlDown() ? true : false;
			for(TWeakPtr<ISceneOutlinerTreeItem>& SelectedItem : Selection.SelectedItems)
			{
				if(UPrimitiveComponent* SelectedComponent = UE::Dataflow::Private::GetOutlinerItemObject<UPrimitiveComponent>(SelectedItem))
				{
					SelectedComponent->SetVisibility(bIsVisible);
				}
				else if(FDataflowBaseElement* SelectedElement = UE::Dataflow::Private::GetOutlinerItemStruct<FDataflowBaseElement>(SelectedItem))
				{
					SelectedElement->bIsVisible = bIsVisible;
				}
				UE::Dataflow::Private::SetOutlinerItemVisibility(SelectedItem, bIsVisible);
			}
		}
	}
	return UE::Editor::Outliner::FTedsOutlinerMode::OnKeyDown(InKeyEvent);
}

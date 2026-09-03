// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCControllerPanelList.h"

#include "Behaviour/Builtin/Bind/RCBehaviourBind.h"
#include "Behaviour/Builtin/Bind/RCBehaviourBindNode.h"
#include "Commands/RemoteControlCommands.h"
#include "Controller/RCController.h"
#include "Controller/RCControllerContainer.h"
#include "Controller/RCControllerUtilities.h"
#include "Controller/RCCustomControllerUtilities.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "IRemoteControlModule.h"
#include "Materials/MaterialInterface.h"
#include "RCControllerModel.h"
#include "RCMultiController.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlField.h"
#include "RemoteControlPreset.h"
#include "SDropTarget.h"
#include "SRCControllerItemRow.h"
#include "SRCControllerPanel.h"
#include "ScopedTransaction.h"
#include "SlateOptMacros.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/SRCActionPanelList.h"
#include "UI/BaseLogicUI/RCLogicModeBase.h"
#include "UI/RCUIHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRCPanelDragHandle.h"
#include "UI/SRCPanelExposedEntity.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "SRCControllerPanelList"

TArray<TSharedPtr<FRCControllerModel>> FRCControllerDragDrop::ResolveControllers() const
{
	TArray<TSharedPtr<FRCControllerModel>> ResolvedControllers;
	ResolvedControllers.Reserve(ControllersWeak.Num());

	for (const TWeakPtr<FRCControllerModel>& WeakController : ControllersWeak)
	{
		if (TSharedPtr<FRCControllerModel> ResolvedController = WeakController.Pin())
		{
			ResolvedControllers.Add(ResolvedController);
		}
	}
	return ResolvedControllers;
}

void SRCControllerPanelList::Construct(const FArguments& InArgs, const TSharedRef<SRCControllerPanel> InControllerPanel, const TSharedRef<SRemoteControlPanel> InRemoteControlPanel)
{
	SRCLogicPanelListBase::Construct(SRCLogicPanelListBase::FArguments(), InControllerPanel, InRemoteControlPanel);
	
	ControllerPanelWeakPtr = InControllerPanel;

	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.LogicControllersPanel");

	ListView = SNew(SListView<TSharedPtr<FRCControllerModel>>)
		.ListItemsSource(&ControllerItems)
		.OnSelectionChanged(this, &SRCControllerPanelList::OnTreeSelectionChanged)
		.OnGenerateRow(this, &SRCControllerPanelList::OnGenerateWidgetForList)
		.SelectionMode(ESelectionMode::Multi)
		.OnContextMenuOpening(this, &SRCLogicPanelListBase::GetContextMenuWidget)
		.HeaderRow(
			SAssignNew(ControllersHeaderRow, SHeaderRow)
			.Style(&RCPanelStyle->HeaderRowStyle)

			+ SHeaderRow::Column(FRCControllerColumns::TypeColor)
			.DefaultLabel(FText())
			.FixedWidth(15)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(FRCControllerColumns::ControllerId)
			.DefaultLabel(LOCTEXT("ControllerIdColumnName", "Controller Id"))
			.FillWidth(0.2f)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(FRCControllerColumns::Description)
			.DefaultLabel(LOCTEXT("ControllerNameColumnDescription", "Description"))
			.FillWidth(0.35f)

			+ SHeaderRow::Column(FRCControllerColumns::Value)
			.DefaultLabel(LOCTEXT("ControllerValueColumnName", "Input"))
			.FillWidth(0.45f)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)
		);

	ChildSlot
	[
		SNew(SDropTarget)
		.VerticalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.VerticalDash"))
		.HorizontalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.HorizontalDash"))
		.OnDropped_Lambda([this](const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) { return SRCControllerPanelList::OnControllerListViewDragDrop(InDragDropEvent.GetOperation()); })
		.OnAllowDrop(this, &SRCControllerPanelList::OnAllowDrop)
		.OnIsRecognized(this, &SRCControllerPanelList::OnAllowDrop)
		[
			ListView.ToSharedRef()
		]
	];

	// Add delegates
	if (const URemoteControlPreset* Preset = ControllerPanelWeakPtr.Pin()->GetPreset())
	{
		// Refresh list
		const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanelWeakPtr.Pin()->GetRemoteControlPanel();
		check(RemoteControlPanel)
		RemoteControlPanel->OnControllerAdded.AddSP(this, &SRCControllerPanelList::OnControllerAdded);
		RemoteControlPanel->OnEmptyControllers.AddSP(this, &SRCControllerPanelList::OnEmptyControllers);

		Preset->OnVirtualPropertyContainerModified().AddSP(this, &SRCControllerPanelList::OnControllerContainerModified);
	}

	FPropertyRowGeneratorArgs Args;
	Args.bShouldShowHiddenProperties = true;
	Args.NotifyHook = this;
	PropertyRowGenerator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);

	Reset();
}

bool SRCControllerPanelList::IsEmpty() const
{
	return ControllerItems.IsEmpty();
}

int32 SRCControllerPanelList::Num() const
{
	return NumControllerItems();
}

int32 SRCControllerPanelList::NumSelectedLogicItems() const
{
	return ListView->GetNumItemsSelected();
}

void SRCControllerPanelList::Reset()
{
	for (const TSharedPtr<FRCControllerModel>& ControllerModel : ControllerItems)
	{
		if (ControllerModel)
		{
			ControllerModel->OnValueTypeChanged.RemoveAll(this);
		}
	}

	// Cache Controller Selection
	const TArray<TSharedPtr<FRCControllerModel>> SelectedControllers = ListView->GetSelectedItems();

	ControllerItems.Empty();

	check(ControllerPanelWeakPtr.IsValid());

	TSharedPtr<SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin();
	URemoteControlPreset* Preset = ControllerPanel->GetPreset();
	TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel();
	
	check(Preset);

	PropertyRowGenerator->SetStructure(Preset->GetControllerContainerStructOnScope());
	if (!PropertyRowGenerator->OnFinishedChangingProperties().IsBoundToObject(this))
	{
		PropertyRowGenerator->OnFinishedChangingProperties().AddSP(this, &SRCControllerPanelList::OnFinishedChangingProperties);
	}

	// Generator should be moved to separate class
	TArray<TSharedRef<IDetailTreeNode>> RootTreeNodes = PropertyRowGenerator->GetRootTreeNodes();

	MultiControllers.ResetMultiControllers();

	bool bShowFieldIdsColumn = false;
	
	for (const TSharedRef<IDetailTreeNode>& CategoryNode : RootTreeNodes)
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		CategoryNode->GetChildren(Children);

		ControllerItems.SetNumZeroed(Children.Num());

		for (TSharedRef<IDetailTreeNode>& Child : Children)
		{
			FProperty* Property = Child->CreatePropertyHandle()->GetProperty();
			check(Property);

			if (URCVirtualPropertyBase* Controller = Preset->GetController(Property->GetFName()))
			{
				bool bIsVisible = true;
				bool bIsMultiController = false;

				const FName& FieldId = Controller->FieldId;

				if (FieldId != NAME_None)
				{
					// there's at least one Field Id set, let's show their column
					bShowFieldIdsColumn = true;
				}
				
				// MultiController Mode: only showing one Controller per Field Id
				if (bIsInMultiControllerMode && Preset->GetControllersByFieldId(FieldId).Num() > 1)
				{					
					bIsMultiController = MultiControllers.TryToAddAsMultiController(Controller);
					bIsVisible = bIsMultiController;
				}

				if (bIsVisible)
				{
					if (ensureAlways(ControllerItems.IsValidIndex(Controller->DisplayIndex)))
					{
						const TSharedRef<FRCControllerModel> ControllerModel = MakeShared<FRCControllerModel>(Controller, Child, RemoteControlPanel);
						ControllerModel->Initialize();
						ControllerModel->OnValueChanged.AddSP(this, &SRCControllerPanelList::OnControllerValueChanged, bIsMultiController);
						if (bIsMultiController)
						{
							ControllerModel->SetMultiController(bIsMultiController);
							ControllerModel->OnValueTypeChanged.AddSP(this, &SRCControllerPanelList::OnControllerValueTypeChanged);
						}
						ControllerItems[Controller->DisplayIndex] = ControllerModel;
					}
				}
			}
		}
	}

	// sort by Field Id
	if (bIsInMultiControllerMode)
	{
		Algo::Sort(ControllerItems, [](const TSharedPtr<FRCControllerModel>& A, const TSharedPtr<FRCControllerModel>& B)
		{
			if (A.IsValid() && B.IsValid())
			{
				const URCVirtualPropertyBase* ControllerA = A->GetVirtualProperty();
				const URCVirtualPropertyBase* ControllerB = B->GetVirtualProperty();

				if (ControllerA && ControllerB)
				{
					return ControllerA->FieldId.FastLess(ControllerB->FieldId);
				}
			}

			return false;
		});
	}

	ShowFieldIdHeaderColumn(bShowFieldIdsColumn);
	ShowValueTypeHeaderColumn(bIsInMultiControllerMode);

	// Handle custom additional columns
	CustomColumns.Empty();
	IRemoteControlUIModule::Get().OnAddControllerExtensionColumn().Broadcast(CustomColumns);
	for (const FName& ColumnName : CustomColumns)
	{
		if (ControllersHeaderRow.IsValid())
		{
			const bool bColumnIsGenerated = ControllersHeaderRow->IsColumnGenerated(ColumnName);
			if (!bColumnIsGenerated)
			{
				ControllersHeaderRow->AddColumn(
					SHeaderRow::FColumn::FArguments()
					.ColumnId(ColumnName)
					.DefaultLabel(FText::FromName(ColumnName))
					.FillWidth(0.2f)
					.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)
				);
			}			
		}
	}
	
	ListView->RebuildList();

	// Restore Controller Selection
	for (const TSharedPtr<FRCControllerModel>& ControllerModel : SelectedControllers)
	{
		if (ControllerModel.IsValid())
		{
			const FName SelectedControllerName = ControllerModel->GetPropertyName();

			const TSharedPtr<FRCControllerModel>* SelectedController = ControllerItems.FindByPredicate([&SelectedControllerName]
				(const TSharedPtr<FRCControllerModel>& InControllerModel)
				{
					// Internal PropertyName is unique, so we use that
					return InControllerModel.IsValid() && SelectedControllerName == InControllerModel->GetPropertyName();
				});

			if (SelectedController)
			{
				ListView->SetItemSelection(*SelectedController, true);
			}
		}
	}
}

TSharedRef<ITableRow> SRCControllerPanelList::OnGenerateWidgetForList(TSharedPtr<FRCControllerModel> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRCControllerItemRow, OwnerTable, InItem.ToSharedRef(), SharedThis(this))
		.Style(&RCPanelStyle->TableRowStyle);
}

void SRCControllerPanelList::OnTreeSelectionChanged(TSharedPtr<FRCControllerModel> InItem, ESelectInfo::Type InSelectInfo)
{
	if (TSharedPtr<SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin())
	{
		if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel())
		{
			// Tree selection gets updated and notifies with the first element in the selection set.
			// Controller Behavior / Actions only support viewing one behavior at a time
			if (InItem != SelectedControllerItemWeakPtr)
			{
				SelectedControllerItemWeakPtr = InItem;
				RemoteControlPanel->OnControllerSelectionChanged.Broadcast(InItem, InSelectInfo);
				RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(InItem.IsValid() ? InItem->GetSelectedBehaviourModel() : nullptr);
			}
		}
	}
}

void SRCControllerPanelList::SelectController(URCController* InController)
{
	for (TSharedPtr<FRCControllerModel> ControllerItem : ControllerItems)
	{
		if (!ensure(ControllerItem))
		{
			continue;
		}

		if (ControllerItem->GetVirtualProperty() == InController)
		{
			ListView->SetSelection(ControllerItem);
		}
	}
}

void SRCControllerPanelList::OnControllerAdded(const FName& InNewPropertyName)
{
	Reset();
}

void SRCControllerPanelList::OnNotifyPreChangeProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		Preset->OnNotifyPreChangeVirtualProperty(PropertyChangedEvent);
	}
}

void SRCControllerPanelList::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		Preset->OnModifyController(PropertyChangedEvent);
	}
}

void SRCControllerPanelList::OnControllerValueTypeChanged(URCVirtualPropertyBase* InController, EPropertyBagPropertyType InValueType)
{	
	if (InController)
	{		
		MultiControllers.UpdateFieldIdValueType(InController->FieldId, InValueType);
		Reset();

		// todo: do we also want to refresh controllers values after type change?
	}
}

void SRCControllerPanelList::OnControllerValueChanged(TSharedPtr<FRCControllerModel> InControllerModel, bool bInIsMultiController)
{
	if (bInIsMultiController)
	{
		if (const URCVirtualPropertyBase* Controller = InControllerModel->GetVirtualProperty())
		{
			FRCMultiController MultiController = MultiControllers.GetMultiController(Controller->FieldId);

			if (MultiController.IsValid())
			{
				MultiController.UpdateHandledControllersValue();
			}
		}
	}

	if (const TSharedPtr<SRemoteControlPanel>& RemoteControlPanel = GetRemoteControlPanel())
	{
		RemoteControlPanel->OnControllerValueChangedDelegate.Broadcast(InControllerModel);
	}
}


void SRCControllerPanelList::OnEmptyControllers()
{
	if (TSharedPtr< SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin())
	{
		if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel())
		{
			RemoteControlPanel->OnControllerSelectionChanged.Broadcast(nullptr, ESelectInfo::Direct);
			RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(nullptr);
		}

		Reset();
	}
}

void SRCControllerPanelList::OnControllerContainerModified()
{
	Reset();
}

void SRCControllerPanelList::BroadcastOnItemRemoved()
{
	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanelWeakPtr.Pin()->GetRemoteControlPanel())
	{
		RemoteControlPanel->OnControllerSelectionChanged.Broadcast(nullptr, ESelectInfo::Direct);
		RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(nullptr);
	}
}

URemoteControlPreset* SRCControllerPanelList::GetPreset()
{
	if (ControllerPanelWeakPtr.IsValid())
	{
		return ControllerPanelWeakPtr.Pin()->GetPreset();
	}

	return nullptr;	
}

int32 SRCControllerPanelList::RemoveModel(const TSharedPtr<FRCLogicModeBase> InModel)
{
	if(ControllerPanelWeakPtr.IsValid())
	{
		if (URemoteControlPreset* Preset = ControllerPanelWeakPtr.Pin()->GetPreset())
		{
			if(const TSharedPtr<FRCControllerModel> SelectedController = StaticCastSharedPtr<FRCControllerModel>(InModel))
			{
				if(URCVirtualPropertyBase* ControllerToRemove = SelectedController->GetVirtualProperty())
				{
					const int32 DisplayIndexToRemove = ControllerToRemove->DisplayIndex;

					FScopedTransaction Transaction(LOCTEXT("RemoveController", "Remove Controller"));
					Preset->Modify();

					// Remove Model from Data Container
					const bool bRemoved = Preset->RemoveController(SelectedController->GetPropertyName());
					if (bRemoved)
					{
						// Shift all display indices higher than the deleted index down by 1
						for (int32 ControllerIndex = 0; ControllerIndex < ControllerItems.Num(); ControllerIndex++)
						{
							if (ensure(ControllerItems[ControllerIndex]))
							{
								if (URCVirtualPropertyBase* Controller = ControllerItems[ControllerIndex]->GetVirtualProperty())
								{
									if (Controller->DisplayIndex > DisplayIndexToRemove)
									{
										Controller->Modify();

										Controller->SetDisplayIndex(Controller->DisplayIndex - 1);
									}
								}
							}
						}

						return 1; // Remove Count
					}
				}
			}
		}
	}

	return 0;
}

bool SRCControllerPanelList::IsListFocused() const
{
	return ListView->HasAnyUserFocus().IsSet() || ContextMenuWidgetCached.IsValid();
}

void SRCControllerPanelList::DeleteSelectedPanelItems()
{
	FScopedTransaction Transaction(LOCTEXT("DeleteSelectedItems", "Delete Selected Items"));
	if (!DeleteItemsFromLogicPanel<FRCControllerModel>(ControllerItems, ListView->GetSelectedItems()))
	{
		Transaction.Cancel();
	}
}

TArray<TSharedPtr<FRCControllerModel>> SRCControllerPanelList::GetSelectedControllers() const
{
	return ListView->GetSelectedItems();
}

TArray<TSharedPtr<FRCLogicModeBase>> SRCControllerPanelList::GetSelectedLogicItems()
{
	return TArray<TSharedPtr<FRCLogicModeBase>>(ListView->GetSelectedItems());
}

void SRCControllerPanelList::NotifyPreChange(FEditPropertyChain* PropertyAboutToChange)
{
	// If a Vector is modified and the Z value changes, the sub property (corresponding to Z) gets notified to us.
	// However for Controllers we are actually interested in the parent Struct property (corresponding to the Vector) as the Virtual Property is associated with Struct's FProperty (rather than its X/Y/Z components)
	// For this reason the "Active Member Node" is extracted below from the child property
	if (FEditPropertyChain::TDoubleLinkedListNode* ActiveMemberNode = PropertyAboutToChange->GetActiveMemberNode())
	{
		FPropertyChangedEvent PropertyChangedEvent(ActiveMemberNode->GetValue());

		OnNotifyPreChangeProperties(PropertyChangedEvent);
	}
}

void SRCControllerPanelList::EnterRenameMode()
{
	if (TSharedPtr<FRCControllerModel> SelectedItem = SelectedControllerItemWeakPtr.Pin())
	{
		SelectedItem->EnterDescriptionEditingMode();
	}
}

TSharedPtr<FRCControllerModel> SRCControllerPanelList::FindControllerItemById(const FGuid& InId) const
{
	for (TSharedPtr<FRCControllerModel> ControllerItem : ControllerItems)
	{
		if (ControllerItem && ControllerItem->GetId() == InId)
		{
			return ControllerItem;
		}
	}

	return nullptr;
}

TArray<TSharedPtr<FRCControllerModel>> SRCControllerPanelList::FindControllerItemsById(TConstArrayView<FGuid> InIds)
{
	TArray<TSharedPtr<FRCControllerModel>> FoundControllerItems;
	FoundControllerItems.Reserve(InIds.Num());

	for (const TSharedPtr<FRCControllerModel>& ControllerItem : ControllerItems)
	{
		if (ControllerItem.IsValid() && InIds.Contains(ControllerItem->GetId()))
		{
			FoundControllerItems.Add(ControllerItem);
		}
	}

	return FoundControllerItems;
}

TArray<TSharedPtr<FRCControllerModel>> SRCControllerPanelList::FindControllerItemsByObject(TConstArrayView<URCController*> InControllers)
{
	TArray<TSharedPtr<FRCControllerModel>> FoundControllerItems;
	FoundControllerItems.Reserve(InControllers.Num());

	for (const TSharedPtr<FRCControllerModel>& ControllerItem : ControllerItems)
	{
		if (ControllerItem.IsValid() && InControllers.Contains(ControllerItem->GetVirtualProperty()))
		{
			FoundControllerItems.Add(ControllerItem);
		}
	}

	return FoundControllerItems;
}

int32 SRCControllerPanelList::GetDropIndex(const TSharedPtr<FRCControllerModel>& InItem, EItemDropZone InDropZone) const
{
	int32 Index = ControllerItems.Find(InItem);
	if (Index == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (InDropZone == EItemDropZone::BelowItem)
	{
		++Index;
	}
	return Index;
}

bool SRCControllerPanelList::ReorderControllerItems(TConstArrayView<TSharedPtr<FRCControllerModel>> InItemsToMove, int32 InTargetIndex)
{
	// Only allow valid indices and the index 1 more of the end index (i.e. index == num)
	if (InTargetIndex == INDEX_NONE || InTargetIndex > ControllerItems.Num())
	{
		return false;
	}

	// Remove the items to move.
	// if these items come before the target index, the target index needs to shift to compensate for the removed items
	for (TArray<TSharedPtr<FRCControllerModel>>::TIterator Iter(ControllerItems); Iter; ++Iter)
	{
		if (InItemsToMove.Contains(*Iter))
		{
			if (Iter.GetIndex() < InTargetIndex)
			{
				--InTargetIndex;
			}
			Iter.RemoveCurrent();
		}
	}

	// Update UI list. Remove all the items to move to then insert them at the desired index
	ControllerItems.RemoveAll([&InItemsToMove](const TSharedPtr<FRCControllerModel>& InController)
		{
			return InItemsToMove.Contains(InController);
		});

	ControllerItems.Insert(InItemsToMove.GetData(), InItemsToMove.Num(), InTargetIndex);

	// Update display indices
	for (int32 Index = 0; Index < ControllerItems.Num(); Index++)
	{
		if (ControllerItems[Index])
		{
			if (URCVirtualPropertyBase* Controller = ControllerItems[Index]->GetVirtualProperty())
			{
				Controller->Modify();
				Controller->SetDisplayIndex(Index);
			}
		}
	}

	ListView->RequestListRefresh();
	return true;
}

static TSharedPtr<FExposedEntityDragDrop> GetExposedEntityDragDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (DragDropOperation)
	{
		if (DragDropOperation->IsOfType<FExposedEntityDragDrop>())
		{
			return StaticCastSharedPtr<FExposedEntityDragDrop>(DragDropOperation);
		}
	}

	return nullptr;
}

bool SRCControllerPanelList::IsEntitySupported(const FGuid ExposedEntityId)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		if (const TSharedPtr<const FRemoteControlProperty>& RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
		{
			return UE::RCControllers::CanCreateControllerFromEntity(RemoteControlProperty);
		}
	}

	return false;
}

bool SRCControllerPanelList::OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (IsListViewHovered())
	{
		bIsAnyControllerItemEligibleForDragDrop = false;
	}
	// Ensures that this drop target is visually disabled whenever the user is attempting to drop onto an existing Controller (rather than the ListView's empty space)
	else if (bIsAnyControllerItemEligibleForDragDrop)
	{
		return false;
	}

	if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = GetExposedEntityDragDrop(DragDropOperation))
	{
		// Fetch the Exposed Entity
		const TArray<FGuid>& ExposedEntitiesIds = DragDropOp->GetSelectedFieldsId();

		// Check if Entity is supported by controllers
		// Allow drop if at least one entity is supported.
		for (const FGuid& EntityId : ExposedEntitiesIds)
		{
			if (IsEntitySupported(EntityId))
			{
				return true;
			}
		}
	}
	return false;
}

FReply SRCControllerPanelList::OnControllerListViewDragDrop(TSharedPtr<FDragDropOperation> InDragDropOperation)
{
	URemoteControlPreset* const Preset = GetPreset();
	if (!Preset)
	{
		return FReply::Handled();
	}

	if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = GetExposedEntityDragDrop(InDragDropOperation))
	{
		FScopedTransaction Transaction(LOCTEXT("AutoBindEntities", "Auto bind entities to controllers"));

		bool bModified = false;
		for (const FGuid& ExposedEntityId : DragDropOp->GetSelectedFieldsId())
		{
			if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
			{
				bModified |= CreateAutoBindForProperty(RemoteControlProperty);
			}
		}

		if (!bModified)
		{
			Transaction.Cancel();
		}
	}

	return FReply::Handled();
}

bool SRCControllerPanelList::CreateAutoBindForProperty(TSharedPtr<const FRemoteControlProperty> InRemoteControlProperty)
{
	if (URCController* NewController = UE::RCUIHelpers::CreateControllerFromEntity(GetPreset(), InRemoteControlProperty))
	{
		// Refresh UI
		Reset();

		// Create Bind Behaviour and Bind to the property
		constexpr bool bExecuteBind = true;
		CreateBindBehaviourAndAssignTo(NewController, InRemoteControlProperty.ToSharedRef(), bExecuteBind);
		return true;
	}
	return false;
}

void SRCControllerPanelList::CreateBindBehaviourAndAssignTo(URCController* Controller, TSharedRef<const FRemoteControlProperty> InRemoteControlProperty, const bool bExecuteBind)
{
	const URCBehaviourBind* BindBehaviour = nullptr;

	bool bRequiresNumericConversion = false;
	if (!URCBehaviourBind::CanHaveActionForField(Controller, InRemoteControlProperty, false))
	{
		if (URCBehaviourBind::CanHaveActionForField(Controller, InRemoteControlProperty, true))
		{
			bRequiresNumericConversion = true;
		}
		else
		{
			ensureAlwaysMsgf(false, TEXT("Incompatible property provided for Auto Bind!"));
			return;
		}
	}

	for (const URCBehaviour* Behaviour : Controller->Behaviours)
	{
		if (Behaviour && Behaviour->IsA(URCBehaviourBind::StaticClass()))
		{
			BindBehaviour = Cast<URCBehaviourBind>(Behaviour);

			// In case numeric conversion is required we might have multiple Bind behaviours with different settings,
			// so we do not break in case there is at least one Bind behaviour with a matching clause. If not, we will create a new Bind behaviour with the requried setting (see below)
			if (!bRequiresNumericConversion || BindBehaviour->AreNumericInputsAllowedAsStrings())
			{
				break;
			}
		}
	}

	if (BindBehaviour)
	{
		if (bRequiresNumericConversion && !BindBehaviour->AreNumericInputsAllowedAsStrings())
		{
			// If the requested Bind operation requires numeric conversion but the existing Bind behaviour doesn't support this, then we prefer creating a new Bind behaviour to facilitate this operation.
			// This allows the the user to successfully perform the Auto Bind as desired without disrupting the setting on the existing Bind behaviour
			BindBehaviour = nullptr;
		}
		
	}

	if (!BindBehaviour)
	{
		Controller->Modify();

		URCBehaviourBind* NewBindBehaviour = Cast<URCBehaviourBind>(Controller->AddBehaviour(URCBehaviourBindNode::StaticClass()));

		// If this is a new Bind Behaviour and we are attempting to link unrelated (but compatible types), via numeric conversion, then we apply the bAllowNumericInputAsStrings flag
		NewBindBehaviour->SetAllowNumericInputAsStrings(bRequiresNumericConversion);

		BindBehaviour = NewBindBehaviour;

		// Broadcast new behaviour
		if (const TSharedPtr<SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin())
		{
			if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel())
			{
				RemoteControlPanel->OnBehaviourAdded.Broadcast(BindBehaviour);
			}
		}
	}

	if (ensure(BindBehaviour))
	{
		URCBehaviourBind* BindBehaviourMutable = const_cast<URCBehaviourBind*>(BindBehaviour);

		BindBehaviourMutable->Modify();
		URCAction* BindAction = BindBehaviourMutable->AddPropertyBindAction(InRemoteControlProperty);

		if (bExecuteBind)
		{
			BindAction->Execute();
		}
	}

	// Update the UI selection to provide the user visual feedback indicating that a Bind behaviour has been created/updated
	SelectController(Controller);
}

bool SRCControllerPanelList::IsListViewHovered()
{
	return ListView->IsDirectlyHovered();
}

void SRCControllerPanelList::ShowValueTypeHeaderColumn(bool bInShowColumn)
{
	if (ControllersHeaderRow.IsValid())
	{
		const bool bColumnIsGenerated = ControllersHeaderRow->IsColumnGenerated(FRCControllerColumns::ValueTypeSelection);
		if (bInShowColumn)
		{
			if (!bColumnIsGenerated)
			{
				ControllersHeaderRow->AddColumn(
					SHeaderRow::FColumn::FArguments()
					.ColumnId(FRCControllerColumns::ValueTypeSelection)
					.DefaultLabel(LOCTEXT("ControllerValueTypeColumnName", "Value Type"))
					.FillWidth(0.2f)
					.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)
				);
			}
		}
		else if (bColumnIsGenerated)
		{
			ControllersHeaderRow->RemoveColumn(FRCControllerColumns::ValueTypeSelection);
		}
	}
}

void SRCControllerPanelList::ShowFieldIdHeaderColumn(bool bInShowColumn)
{	
	if (ControllersHeaderRow.IsValid())
	{
		const bool bColumnIsGenerated = ControllersHeaderRow->IsColumnGenerated(FRCControllerColumns::FieldId);
		if (bInShowColumn)
		{
			if (!bColumnIsGenerated)
			{
				ControllersHeaderRow->InsertColumn(
					SHeaderRow::FColumn::FArguments()
					.ColumnId(FRCControllerColumns::FieldId)
					.DefaultLabel(LOCTEXT("ControllerNameColumnFieldId", "Field Id"))
					.FillWidth(0.2f)
					.HeaderContentPadding(RCPanelStyle->HeaderRowPadding),
					2);
			}
		}
		else if (bColumnIsGenerated)
		{
			ControllersHeaderRow->RemoveColumn(FRCControllerColumns::FieldId);
		}
	}
}

void SRCControllerPanelList::AddColumn(const FName& InColumnName)
{
	CustomColumns.AddUnique(InColumnName);
}

void SRCControllerPanelList::SetMultiControllerMode(bool bIsUniqueModeOn)
{
	bIsInMultiControllerMode = bIsUniqueModeOn;
	Reset();
}

#undef LOCTEXT_NAMESPACE

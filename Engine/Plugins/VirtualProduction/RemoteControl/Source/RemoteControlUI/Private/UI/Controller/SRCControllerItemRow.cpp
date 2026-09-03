// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCControllerItemRow.h"
#include "Behaviour/Builtin/Bind/RCBehaviourBind.h"
#include "Controller/RCController.h"
#include "Controller/RCControllerUtilities.h"
#include "RemoteControlPreset.h"
#include "SRCControllerPanelList.h"
#include "ScopedTransaction.h"
#include "UI/RCUIHelpers.h"
#include "UI/SRCPanelExposedEntity.h"

#define LOCTEXT_NAMESPACE "SRCControllerItemRow"

const FLazyName FRCControllerColumns::TypeColor = TEXT("TypeColor");
const FLazyName FRCControllerColumns::ControllerId = TEXT("Controller Id");
const FLazyName FRCControllerColumns::Description = TEXT("Controller Description");
const FLazyName FRCControllerColumns::Value = TEXT("Controller Value");
const FLazyName FRCControllerColumns::FieldId = TEXT("Controller Field Id");
const FLazyName FRCControllerColumns::ValueTypeSelection = TEXT("Value Type Selection");

void SRCControllerItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<FRCControllerModel> InControllerItem, TSharedRef<SRCControllerPanelList> InControllerPanelList)
{
	ControllerItem = InControllerItem;
	ControllerPanelListWeak = InControllerPanelList.ToWeakPtr();

	FSuperRowType::Construct(FSuperRowType::FArguments()
		.Style(InArgs._Style)
		.Padding(4.5f)
		.OnCanAcceptDrop(this, &SRCControllerItemRow::CanAcceptDrop)
		.OnAcceptDrop(this, &SRCControllerItemRow::OnAcceptDrop)
		, InOwnerTableView);
}

TSharedRef<SWidget> SRCControllerItemRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	if (!ensure(ControllerItem.IsValid()))
	{
		return SNullWidget::NullWidget;
	}

	if (InColumnName == FRCControllerColumns::TypeColor)
	{
		if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
		{
			if (const FProperty* Property = Controller->GetProperty())
			{
				return UE::RCUIHelpers::GetTypeColorWidget(Property);
			}
		}
	}
	else if (InColumnName == FRCControllerColumns::FieldId)
	{
		return ControllerItem->GetFieldIdWidget();
	}
	else if (InColumnName == FRCControllerColumns::ValueTypeSelection)
	{
		return ControllerItem->GetTypeSelectionWidget();
	}
	else if (InColumnName == FRCControllerColumns::ControllerId)
	{
		return ControllerItem->GetNameWidget();
	}
	else if (InColumnName == FRCControllerColumns::Description)
	{
		return ControllerItem->GetDescriptionWidget();
	}
	else if (InColumnName == FRCControllerColumns::Value)
	{
		return ControllerItem->GetWidget();
	}
	else if (const TSharedPtr<SRCControllerPanelList> ControlPanelList = ControllerPanelListWeak.Pin())
	{
		if (ControlPanelList->GetCustomColumns().Contains(InColumnName))
		{
			return ControllerItem->GetControllerExtensionWidget(InColumnName);
		}
	}
	return SNullWidget::NullWidget;
}

TOptional<EItemDropZone> SRCControllerItemRow::CanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, TSharedRef<FRCControllerModel> InItem)
{
	const TSharedPtr<SRCControllerPanelList> ControllerPanelList = ControllerPanelListWeak.Pin();
	if (!ControllerPanelList.IsValid() || !ControllerItem.IsValid())
	{
		return TOptional<EItemDropZone>();
	}

	// Fetch the Exposed Entities
	URemoteControlPreset* const Preset = ControllerPanelList->GetPreset();
	if (!Preset)
	{
		return TOptional<EItemDropZone>();
	}

	// Dragging Controllers onto Controllers (Reordering). Only allow Above/Below re-ordering (no-onto support)
	if (TSharedPtr<FRCControllerDragDrop> ControllerDragDrop = InDragDropEvent.GetOperationAs<FRCControllerDragDrop>())
	{
		if (InDropZone == EItemDropZone::OntoItem)
		{
			return TOptional<EItemDropZone>();
		}
		return InDropZone;
	}

	if (TSharedPtr<FExposedEntityDragDrop> EntityDragDrop = InDragDropEvent.GetOperationAs<FExposedEntityDragDrop>())
	{
		// Support when adding entities onto this controller, ensure that at least one entity can be added via bind action 
		if (InDropZone == EItemDropZone::OntoItem)
		{
			for (const FGuid& ExposedEntityId : EntityDragDrop->GetSelectedFieldsId())
			{
				if (TSharedPtr<const FRemoteControlField> RemoteControlField = Preset->GetExposedEntity<FRemoteControlField>(ExposedEntityId).Pin())
				{
					if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
					{
						const bool bAllowNumericInputAsStrings = true;
						if (URCBehaviourBind::CanHaveActionForField(Controller, RemoteControlField.ToSharedRef(), bAllowNumericInputAsStrings))
						{
							ControllerPanelList->bIsAnyControllerItemEligibleForDragDrop = true;
							return InDropZone;
						}
					}
				}
			}
		}
		// When adding entities above/below controllers (i.e. create a new controller above/below this one), at least one has to be supported
		else
		{
			for (const FGuid& ExposedEntityId : EntityDragDrop->GetSelectedFieldsId())
			{
				if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
				{
					if (UE::RCControllers::CanCreateControllerFromEntity(RemoteControlProperty))
					{
						ControllerPanelList->bIsAnyControllerItemEligibleForDragDrop = true;
						return InDropZone;
					}
				}
			}
		}
	}

	return TOptional<EItemDropZone>();
}

FReply SRCControllerItemRow::OnAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, TSharedRef<FRCControllerModel> InItem)
{
	if (!ControllerItem.IsValid())
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<SRCControllerPanelList> ControllerPanelList = ControllerPanelListWeak.Pin();
	if (!ControllerPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	ControllerPanelList->bIsAnyControllerItemEligibleForDragDrop = false;

	URemoteControlPreset* Preset = ControllerPanelList->GetPreset();
	if (!Preset)
	{
		return FReply::Unhandled();
	}

	if (TSharedPtr<FRCControllerDragDrop> ControllerDragDrop = InDragDropEvent.GetOperationAs<FRCControllerDragDrop>())
	{
		FScopedTransaction Transaction(LOCTEXT("ReorderControllers", "Reorder Controllers"));

		const int32 DropIndex = ControllerPanelList->GetDropIndex(ControllerItem, InDropZone);
		if (!ControllerPanelList->ReorderControllerItems(ControllerDragDrop->ResolveControllers(), DropIndex))
		{
			Transaction.Cancel();
		}
		return FReply::Handled();
	}

	if (TSharedPtr<FExposedEntityDragDrop> ExposedEntityDragDrop = InDragDropEvent.GetOperationAs<FExposedEntityDragDrop>())
	{
		const FDragDropContext DragDropContext
			{
				.Preset = Preset,
				.ControllerPanelList = ControllerPanelList,
				.Item = InItem,
				.DropZone = InDropZone,
			};

		if (InDropZone == EItemDropZone::OntoItem)
		{
			CreateBindBehaviorsFromEntities(*ExposedEntityDragDrop, DragDropContext);
		}
		else
		{
			CreateControllersFromEntities(*ExposedEntityDragDrop, DragDropContext);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SRCControllerItemRow::CreateBindBehaviorsFromEntities(const FExposedEntityDragDrop& InExposedEntityDragDrop, const FDragDropContext& InContext)
{
	const TArray<FGuid>& DroppedFieldIds = InExposedEntityDragDrop.GetSelectedFieldsId();

	if (!ControllerItem.IsValid() || DroppedFieldIds.IsEmpty())
	{
		return;
	}

	URCController* const Controller = Cast<URCController>(ControllerItem->GetVirtualProperty());
	if (!Controller)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("BindPropertiesToController", "Bind properties to Controller"));

	bool bModified = false;

	// Update controller description if empty to match the first dragged property
	if (Controller->Description.IsEmpty())
	{
		if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = InContext.Preset->GetExposedEntity<FRemoteControlProperty>(DroppedFieldIds[0]).Pin())
		{
			Controller->Modify();
			Controller->Description = UE::RCUIHelpers::GenerateControllerDescriptionFromEntity(RemoteControlProperty);
			bModified = true;
		}
	}

	for (const FGuid& ExposedEntityId : DroppedFieldIds)
	{
		if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = InContext.Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
		{
			InContext.ControllerPanelList->CreateBindBehaviourAndAssignTo(Controller, RemoteControlProperty.ToSharedRef(), true);
			bModified = true;
		}
	}

	if (!bModified)
	{
		Transaction.Cancel();
	}
}

void SRCControllerItemRow::CreateControllersFromEntities(const FExposedEntityDragDrop& InExposedEntityDragDrop, const FDragDropContext& InContext)
{
	FScopedTransaction Transaction(LOCTEXT("AutoBindEntities", "Auto bind entities to controllers"));

	const TArray<FGuid>& SelectedFieldIds = InExposedEntityDragDrop.GetSelectedFieldsId();

	TArray<URCController*, TInlineAllocator<1>> CreatedControllers;
	TArray<TSharedRef<const FRemoteControlProperty>, TInlineAllocator<1>> SourcePropertyEntities;

	CreatedControllers.Reserve(SelectedFieldIds.Num());
	SourcePropertyEntities.Reserve(SelectedFieldIds.Num());

	for (const FGuid& ExposedEntityId : SelectedFieldIds)
	{
		if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = InContext.Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
		{
			if (URCController* NewController = UE::RCUIHelpers::CreateControllerFromEntity(InContext.Preset, RemoteControlProperty))
			{
				CreatedControllers.Emplace(NewController);
				SourcePropertyEntities.Add(RemoteControlProperty.ToSharedRef());
			}
		}
	}

	if (CreatedControllers.IsEmpty())
	{
		Transaction.Cancel();
		return;
	}

	// Store the drop index prior to refresh
	const int32 DropIndex = InContext.ControllerPanelList->GetDropIndex(ControllerItem, InContext.DropZone);

	// Refresh panel list so that the controller models are created
	InContext.ControllerPanelList->Reset();

	// Reorder the controller items to match the target drop zone
	{
		const TArray<TSharedPtr<FRCControllerModel>> ControllersToMove = InContext.ControllerPanelList->FindControllerItemsByObject(CreatedControllers);
		InContext.ControllerPanelList->ReorderControllerItems(ControllersToMove, DropIndex);
	}

	check(CreatedControllers.Num() == SourcePropertyEntities.Num());

	// Create Bind Behaviour and Bind to the property
	for (int32 Index = 0; Index < CreatedControllers.Num(); ++Index)
	{
		constexpr bool bExecuteBind = true;
		InContext.ControllerPanelList->CreateBindBehaviourAndAssignTo(CreatedControllers[Index], SourcePropertyEntities[Index], bExecuteBind);
	}
}

FReply SRCControllerItemRow::OnDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (FSuperRowType::OnDragDetected(InMyGeometry, InMouseEvent).IsEventHandled())
	{
		return FReply::Handled();
	}

	const TSharedPtr<SRCControllerPanelList> ControllerPanelList = ControllerPanelListWeak.Pin();
	if (!ControllerPanelList.IsValid())
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		TSharedRef<FRCControllerDragDrop> ControllerDragDrop = MakeShared<FRCControllerDragDrop>(ControllerPanelList->GetSelectedControllers());
		return FReply::Handled().BeginDragDrop(ControllerDragDrop);
	}
	return FReply::Unhandled();
}

FReply SRCControllerItemRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (ControllerItem.IsValid())
	{
		ControllerItem->EnterDescriptionEditingMode();
	}
	return FSuperRowType::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

#undef LOCTEXT_NAMESPACE

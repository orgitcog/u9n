// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStateTreeContextMenuButton.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "StateTreeState.h"
#include "StateTreeViewModel.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

void SStateTreeContextMenuButton::Construct(const FArguments& InArgs, const TSharedRef<FStateTreeViewModel>& InStateTreeViewModel, TWeakObjectPtr<UStateTreeState> InOwnerState, const FGuid& InNodeID, bool InbIsTransition)
{
	StateTreeViewModel = InStateTreeViewModel.ToSharedPtr();
	OwnerStateWeak = InOwnerState;
	NodeID = InNodeID;

	bIsStateTransition = false;
	bIsTransition = InbIsTransition;
	if (bIsTransition)
	{
		if (UStateTreeState* OwnerState = OwnerStateWeak.Get())
		{
			for (const FStateTreeTransition& Transition : OwnerState->Transitions)
			{
				if (NodeID == Transition.ID)
				{
					bIsStateTransition = true;
					break;
				}
			}
		}
	}

	SButton::FArguments ButtonArgs;

	ButtonArgs
	.OnClicked_Lambda([this]()
	{
		StateTreeViewModel->BringNodeToFocus(OwnerStateWeak.Get(), NodeID);
		return FReply::Handled();
	})
	.ButtonStyle(InArgs._ButtonStyle)
	.ContentPadding(InArgs._ContentPadding)
	[
		SAssignNew(MenuAnchor, SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		.OnGetMenuContent(FOnGetContent::CreateSP(this, &SStateTreeContextMenuButton::MakeContextMenu))
		[
			InArgs._Content.Widget
		]
	];

	SButton::Construct(ButtonArgs);
}

FReply SStateTreeContextMenuButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	StateTreeViewModel->BringNodeToFocus(OwnerStateWeak.Get(), NodeID);

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (MenuAnchor.IsValid())
		{
			if (MenuAnchor->ShouldOpenDueToClick())
			{
				MenuAnchor->SetIsOpen(true);
			}
			else
			{
				MenuAnchor->SetIsOpen(false);
			}

			return FReply::Handled();
		}
	}

	return SButton::OnMouseButtonUp(MyGeometry, MouseEvent);
}

TSharedRef<SWidget> SStateTreeContextMenuButton::MakeContextMenu() const
{
	FMenuBuilder MenuBuilder(/*ShouldCloseWindowAfterMenuSelection*/true, /*CommandList*/nullptr);

	if (StateTreeViewModel.IsValid() && OwnerStateWeak.IsValid())
	{
		MenuBuilder.BeginSection(FName("Edit"), LOCTEXT("Edit", "Edit"));

		// Copy
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CopyItem", "Copy"),
			LOCTEXT("CopyItemTooltip", "Copy this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [this]()
			{
				StateTreeViewModel->CopyNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Paste
		MenuBuilder.AddMenuEntry(
			LOCTEXT("PasteItem", "Paste"),
			LOCTEXT("PasteItemTooltip", "Paste into this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [this]()
			{
				StateTreeViewModel->PasteNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Duplicate
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DuplicateItem", "Duplicate"),
			LOCTEXT("DuplicateItemTooltip", "Duplicate this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Duplicate"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [&]()
			{
				StateTreeViewModel->DuplicateNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Delete
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteItem", "Delete"),
			LOCTEXT("DeleteItemTooltip", "Delete this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [&]()
			{
				StateTreeViewModel->DeleteNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Delete All
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteAllItems", "Delete all"),
			LOCTEXT("DeleteAllItemsTooltip", "Delete all items"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [&]()
			{
				StateTreeViewModel->DeleteAllNodes(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Handlers/NavigationToolItemDropHandler.h"

enum class EItemDropZone;

namespace UE::SequenceNavigator
{

class INavigationToolView;
class FNavigationToolView;
class FNavigationToolItemDropHandler;

/** Drag Drop Operation for Navigation Tool Items. Customized behavior can be added in via the AddDropHandler function */
class FNavigationToolItemDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNavigationToolItemDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FNavigationToolItemDragDropOp> New(const TArray<FNavigationToolItemPtr>& InItems
		, const TSharedPtr<FNavigationToolView>& InToolView
		, ENavigationToolDragDropActionType InActionType);

	TSharedPtr<INavigationToolView> GetToolView() const
	{
		return WeakToolView.Pin();
	}

	TConstArrayView<FNavigationToolItemPtr> GetItems() const
	{
		return Items;
	}

	ENavigationToolDragDropActionType GetActionType() const
	{
		return ActionType;
	}

	template<typename InDropHandlerType
		, typename = typename TEnableIf<TIsDerivedFrom<InDropHandlerType, FNavigationToolItemDropHandler>::Value>::Type
		, typename... InArgTypes>
	void AddDropHandler(InArgTypes&&... InArgs)
	{
		TSharedRef<FNavigationToolItemDropHandler> DropHandler = MakeShared<InDropHandlerType>(Forward<InArgTypes>(InArgs)...);
		DropHandler->Initialize(*this);
		DropHandlers.Add(DropHandler);
	}

	FReply Drop(EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem);

	TOptional<EItemDropZone> CanDrop(const EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) const;

protected:
	void Init(const TArray<FNavigationToolItemPtr>& InItems
		, const TSharedPtr<FNavigationToolView>& InToolView
		, ENavigationToolDragDropActionType InActionType);

	TArray<FNavigationToolItemPtr> Items;

	TArray<TSharedRef<FNavigationToolItemDropHandler>> DropHandlers;

	TWeakPtr<INavigationToolView> WeakToolView;

	ENavigationToolDragDropActionType ActionType = ENavigationToolDragDropActionType::Move;
};

} // namespace UE::SequenceNavigator

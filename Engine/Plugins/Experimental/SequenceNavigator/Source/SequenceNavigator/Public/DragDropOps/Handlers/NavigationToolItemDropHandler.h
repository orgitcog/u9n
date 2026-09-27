// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Items/INavigationToolItem.h"
#include "NavigationToolDefines.h"
#include "NavigationToolItemType.h"
#include "Templates/EnableIf.h"
#include "Templates/SharedPointer.h"

enum class EItemDropZone;
template<typename OptionalType> struct TOptional;

namespace UE::SequenceNavigator
{

class FNavigationToolItemDragDropOp;

/**
 * Base Class to Handle Dropping Navigation Tool Items into a Target Navigation Tool Item
 * @see built-in example FNavigationToolActorDropHandler
 */
class FNavigationToolItemDropHandler : public INavigationToolItemTypeCastable, public TSharedFromThis<FNavigationToolItemDropHandler>
{
	friend FNavigationToolItemDragDropOp;

	SEQUENCENAVIGATOR_API void Initialize(const FNavigationToolItemDragDropOp& InDragDropOp);

public:
	UE_NAVIGATIONTOOL_INHERITS(FNavigationToolItemDropHandler, INavigationToolItemTypeCastable);

	TConstArrayView<TSharedPtr<INavigationToolItem>> GetItems() const
	{
		return Items;
	}	

protected:
	virtual bool IsDraggedItemSupported(const FNavigationToolItemPtr& InDraggedItem) const = 0;

	virtual TOptional<EItemDropZone> CanDrop(EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) const = 0;

	virtual bool Drop(EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) = 0;

	enum class EIterationResult
	{
		Continue,
		Break,
	};
	template<typename InItemType, typename = typename TEnableIf<TIsDerivedFrom<InItemType, INavigationToolItem>::Value>::Type>
	void ForEachItem(const TFunctionRef<EIterationResult(InItemType&)>& InFunc) const
	{
		for (const TSharedPtr<INavigationToolItem>& Item : Items)
		{
			if (!Item.IsValid())
			{
				continue;
			}

			if (InItemType* const CastedItem = Item->CastTo<InItemType>())
			{
				EIterationResult IterationResult = InFunc(*CastedItem);
				if (IterationResult == EIterationResult::Break)
				{
					break;
				}
			}
		}
	}

	TArray<FNavigationToolItemPtr> Items;

	ENavigationToolDragDropActionType ActionType = ENavigationToolDragDropActionType::Move;
};

} // namespace UE::SequenceNavigator

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "DragDropOps/Handlers/NavigationToolItemDropHandler.h"

class IAvaSequencer;
enum class EItemDropZone;
template<typename OptionalType> struct TOptional;

namespace UE::SequenceNavigator
{

class FNavigationToolAvaSequenceDropHandler : public SequenceNavigator::FNavigationToolItemDropHandler
{
public:
	UE_NAVIGATIONTOOL_INHERITS(FNavigationToolAvaSequenceDropHandler, SequenceNavigator::FNavigationToolItemDropHandler);

	FNavigationToolAvaSequenceDropHandler(const TWeakPtr<IAvaSequencer>& InWeakAvaSequencer);

protected:
	//~ Begin FNavigationToolItemDropHandler
	virtual bool IsDraggedItemSupported(const SequenceNavigator::FNavigationToolItemPtr& InDraggedItem) const override;
	virtual TOptional<EItemDropZone> CanDrop(const EItemDropZone InDropZone, const SequenceNavigator::FNavigationToolItemPtr& InTargetItem) const override;
	virtual bool Drop(const EItemDropZone InDropZone, const SequenceNavigator::FNavigationToolItemPtr& InTargetItem) override;
	//~ End FNavigationToolItemDropHandler

	void MoveItems(const EItemDropZone InDropZone, const SequenceNavigator::FNavigationToolItemPtr& InTargetItem);

	void DuplicateItems(const TArray<SequenceNavigator::FNavigationToolItemPtr>& InItems
		, const SequenceNavigator::FNavigationToolItemPtr& InRelativeItem
		, const TOptional<EItemDropZone>& InRelativeDropZone);

	TWeakPtr<IAvaSequencer> WeakAvaSequencer;
};

} // namespace UE::SequenceNavigator

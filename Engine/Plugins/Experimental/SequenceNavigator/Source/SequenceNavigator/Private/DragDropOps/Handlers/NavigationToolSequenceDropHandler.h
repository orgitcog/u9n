// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragDropOps/Handlers/NavigationToolItemDropHandler.h"

namespace UE::SequenceNavigator
{

/** Class that handles Dropping Sequence Items into a Target Item */
class FNavigationToolSequenceDropHandler : public FNavigationToolItemDropHandler
{
public:
	UE_NAVIGATIONTOOL_INHERITS(FNavigationToolSequenceDropHandler, FNavigationToolItemDropHandler);

private:
	//~ Begin FNavigationToolItemDropHandler
	virtual bool IsDraggedItemSupported(const TSharedPtr<INavigationToolItem>& InDraggedItem) const override;
	virtual TOptional<EItemDropZone> CanDrop(EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) const override;
	virtual bool Drop(EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) override;
	//~ End FNavigationToolItemDropHandler

	void MoveItems(EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem);
};

} // namespace UE::SequenceNavigator

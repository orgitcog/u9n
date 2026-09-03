// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/CurveEditorFilterBase.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/SharedPointer.h"

class FCurveEditor;
class FUICommandInfo;
class FUICommandList;

namespace UE::CurveEditor
{
class FPromotedFilterContainer;

/**
 * Binds and unbinds commands that are created by FPromotedFilterContainer.
 * When a filter is added to FPromotedFilterContainer, it creates a command to invoke that filter. This class is responsible to bind / unbind it.
 */
class CURVEEDITOR_API FPromotedFilterCommandBinder : public FNoncopyable
{
public:
	
	/**
	 * @param InContainer Holds the promoted filter commands to bind to. The caller ensures that it outlives the constructed object. 
	 * @param InCommandList The command list to bind commands to.
	 * @param InCurveEditor The curve editor on which the filters will be applied.
	 */
	explicit FPromotedFilterCommandBinder(
		const TSharedRef<FPromotedFilterContainer>& InContainer,
		const TSharedRef<FUICommandList>& InCommandList,
		const TSharedRef<FCurveEditor>& InCurveEditor
		);
	~FPromotedFilterCommandBinder();

private:

	/** The filter container this object binds to. */
	const TWeakPtr<FPromotedFilterContainer> Container;
	/** Commands are added to and removed from this list. */
	const TWeakPtr<FUICommandList> CommandList;
	/** Passed to filter as argument. */
	const TWeakPtr<FCurveEditor> CurveEditor;
	
	void OnFilterAdded(UCurveEditorFilterBase& InFilter, const TSharedRef<FUICommandInfo>& InCommand) const;
	void OnFilterRemoved(UCurveEditorFilterBase& InFilter, const TSharedRef<FUICommandInfo>& InCommand) const;

	void MapAction(UCurveEditorFilterBase& InFilter, const TSharedRef<FUICommandInfo>& InCommand, FUICommandList& CommandListPin) const;
	
	void ApplyFilter(UCurveEditorFilterBase* InFilter, TWeakPtr<FUICommandInfo> Command) const;
	bool CanApplyFilter(UCurveEditorFilterBase* InFilter, TWeakPtr<FUICommandInfo> Command) const;
};
}


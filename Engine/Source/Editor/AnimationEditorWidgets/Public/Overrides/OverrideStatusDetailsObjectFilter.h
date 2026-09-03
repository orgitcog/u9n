// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Overrides/OverrideStatusDetailsDisplayManager.h"
#include "Overrides/OverrideStatusWidgetMenuBuilder.h"
#include "DetailsViewObjectFilter.h"

DECLARE_DELEGATE_RetVal_TwoParams(bool, FOverrideStatusObjectFilter_CanMergeObjects, const UObject*, const UObject*);

/**
 * An object filter for the property editor / details view.
 * The filter can decide if it can display a certain object - in this case the object filter
 * is used to provide the override status widget instead of the reset value arrow to the details
 * panel.
 */
class ANIMATIONEDITORWIDGETS_API FOverrideStatusDetailsViewObjectFilter : public FDetailsViewObjectFilter
{	
public:
	// the standard method to create an object filter. 
	template<typename T = FOverrideStatusDetailsViewObjectFilter>
	static TSharedPtr<T> Create()
	{
		TSharedPtr<T> ObjectFilter = MakeShared<T>();
		ObjectFilter->InitializeDisplayManager();
		return ObjectFilter;
	}

	// default constructor
	FOverrideStatusDetailsViewObjectFilter();

	// sets up the  display manager for this filter
	virtual void InitializeDisplayManager();

	// Given a const TArray<UObject*>& SourceObjects, filters the objects and puts the objects which should
	// be shown in the Details panel in the return TArray<FDetailsViewObjectRoot>. These may be some part of the
	// original SourceObjects array, itself, or it may be some contained sub-objects within the SourceObjects.
	virtual TArray<FDetailsViewObjectRoot> FilterObjects(const TArray<UObject*>& SourceObjects) override;

	// Returns a preconfigured menu builder for this filter.
	TSharedPtr<FOverrideStatusWidgetMenuBuilder> GetMenuBuilder(const FOverrideStatusSubject& InSubject) const;
	
	FOverrideStatus_CanCreateWidget& OnCanCreateWidget();
	FOverrideStatus_GetStatus& OnGetStatus();
	FOverrideStatus_OnWidgetClicked& OnWidgetClicked();
	FOverrideStatus_OnGetMenuContent& OnGetMenuContent();
	FOverrideStatus_AddOverride& OnAddOverride();
	FOverrideStatus_ClearOverride& OnClearOverride();
	FOverrideStatus_ResetToDefault& OnResetToDefault();
	FOverrideStatus_ValueDiffersFromDefault& OnValueDiffersFromDefault();
	FOverrideStatusObjectFilter_CanMergeObjects& OnCanMergeObjects() { return CanMergeObjectDelegate; }

	static bool MergeObjectByClass(const UObject* InObjectA, const UObject* InObjectB);

private:
	
	/**
	 * The @code FOverrideStatusDetailsDisplayManager @endcode which provides an API to manage some of the characteristics of the
	 * details display
	 */
	TSharedPtr<FOverrideStatusDetailsDisplayManager> OverrideStatusDisplayManager;

	FOverrideStatusObjectFilter_CanMergeObjects CanMergeObjectDelegate;
};
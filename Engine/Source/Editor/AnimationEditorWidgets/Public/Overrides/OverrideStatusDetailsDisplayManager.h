//  Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetailsDisplayManager.h"
#include "DetailsViewStyleKey.h"
#include "Overrides/OverrideStatusDetailsWidgetBuilder.h"
#include "Overrides/OverrideStatusWidgetMenuBuilder.h"

class FOverrideStatusDetailsWidgetBuilder;

/**
 * The display manager is used to determine how the details view should behave when using an object
 * filter. In this case the display manager is used to set the property-updated-widget (the override status widget).
 */
class ANIMATIONEDITORWIDGETS_API FOverrideStatusDetailsDisplayManager : public FDetailsDisplayManager
{
public:

	virtual ~FOverrideStatusDetailsDisplayManager() override;

	/**
	 * Returns a @code bool @endcode indicating whether this @code DetailsViewObjectFilter @endcode instance
	 * has a category menu
	 */
	virtual bool ShouldShowCategoryMenu() override;

	/**
	 * Returns the @code const FDetailsViewStyleKey& @endcode that is the Key to the current FDetailsViewStyle style
	 */
	virtual const FDetailsViewStyleKey& GetDetailsViewStyleKey() const override;

	// Returns true if this manager can construct the property updated widget
	virtual bool CanConstructPropertyUpdatedWidgetBuilder() const override;

	// Returns the builder used to construct the property updated widgets, in this case the SOverrideStatusWidget 
	virtual TSharedPtr<FPropertyUpdatedWidgetBuilder> ConstructPropertyUpdatedWidgetBuilder(const FConstructPropertyUpdatedWidgetBuilderArgs& Args) override;

	// Returns the preconfigured menu builder for this display manager and a given subject
	TSharedPtr<FOverrideStatusWidgetMenuBuilder> GetMenuBuilder(const FOverrideStatusSubject& InSubject) const;

	FOverrideStatus_CanCreateWidget& OnCanCreateWidget() { return CanCreateWidgetDelegate; }
	FOverrideStatus_GetStatus& OnGetStatus() { return GetStatusDelegate; }
	FOverrideStatus_OnWidgetClicked& OnWidgetClicked() { return WidgetClickedDelegate; }
	FOverrideStatus_OnGetMenuContent& OnGetMenuContent() { return GetMenuContentDelegate; }
	FOverrideStatus_AddOverride& OnAddOverride() { return AddOverrideDelegate; }
	FOverrideStatus_ClearOverride& OnClearOverride() { return ClearOverrideDelegate; }
	FOverrideStatus_ResetToDefault& OnResetToDefault() { return ResetToDefaultDelegate; }
	FOverrideStatus_ValueDiffersFromDefault& OnValueDiffersFromDefault() { return ValueDiffersFromDefaultDelegate; }

private:
	
	TSharedPtr<FOverrideStatusDetailsWidgetBuilder> ConstructOverrideWidgetBuilder(const FConstructPropertyUpdatedWidgetBuilderArgs& Args);
	void OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);
	void SetIsDisplayingOverrideableObject(bool InIsDisplayingOverrideableObject);

	bool bIsDisplayingOverrideableObject = false;
	
	FExecuteAction InvalidateCachedState;

	FOverrideStatus_CanCreateWidget CanCreateWidgetDelegate;
	FOverrideStatus_GetStatus GetStatusDelegate;
	FOverrideStatus_OnWidgetClicked WidgetClickedDelegate;
	FOverrideStatus_OnGetMenuContent GetMenuContentDelegate;
	FOverrideStatus_AddOverride AddOverrideDelegate;
	FOverrideStatus_ClearOverride ClearOverrideDelegate;
	FOverrideStatus_ResetToDefault ResetToDefaultDelegate;
	FOverrideStatus_ValueDiffersFromDefault ValueDiffersFromDefaultDelegate;

	friend class FOverrideStatusDetailsViewObjectFilter;
};

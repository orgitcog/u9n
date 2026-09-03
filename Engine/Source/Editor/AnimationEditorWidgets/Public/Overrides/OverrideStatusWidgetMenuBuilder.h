// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ToolElementRegistry.h"
#include "ToolMenu.h"
#include "Widgets/SWidget.h"
#include "Overrides/OverrideStatusSubject.h"
#include "Overrides/SOverrideStatusWidget.h"
#include "Overrides/OverrideStatusDetailsWidgetBuilder.h"

class FPropertyPath;
class FOverrideStatusDetailsDisplayManager;

/**
 * Builder for the override status menu that goes on the top right side of each property
 * Component in the details panel
 */
class ANIMATIONEDITORWIDGETS_API FOverrideStatusWidgetMenuBuilder : public FToolElementRegistrationArgs
{

	DECLARE_DELEGATE(FResetToDefault);
	
public:
	/**
	 * constructor
	 *
	 */
	FOverrideStatusWidgetMenuBuilder(const FOverrideStatusSubject& InSubject, const TWeakPtr<FOverrideStatusDetailsDisplayManager>& InDisplayManager = TWeakPtr<FOverrideStatusDetailsDisplayManager>());

	virtual ~FOverrideStatusWidgetMenuBuilder() override;

	// returns the status of the override
	EOverrideWidgetStatus::Type GetStatus() const;

	// returns the attribute backing up the status of the override
	TAttribute<EOverrideWidgetStatus::Type>& GetStatusAttribute() { return StatusAttribute; }

	/**
	 * Override the active overrideable object at the given property path
	 */
	void AddOverride();
	bool CanAddOverride() const;
	FOverrideStatus_AddOverride& OnAddOverride();
	
	/**
	 * Clears any active overrides on the property / object
	 */
	void ClearOverride();
	bool CanClearOverride() const;
	FOverrideStatus_ClearOverride& OnClearOverride();

	/**
	 * Override the active overrideable object at the given property path
	 */
	void ResetToDefault();
	bool CanResetToDefault() const;
	FOverrideStatus_ResetToDefault& OnResetToDefault();
	FOverrideStatus_ValueDiffersFromDefault& OnValueDiffersFromDefault();

	/**
	 * Set up the menu
	 */
	void InitializeMenu();

	/**
	 * Fill this method in with your Slate to create it
	 */
	virtual TSharedPtr<SWidget> GenerateWidget() override;

private:
	/**
	 * The UToolMenu providing the context menu
	 */
	TWeakObjectPtr<UToolMenu> ToolMenu;

	/**
	 * The object that will be queried for its override state
	 */
	FOverrideStatusSubject Subject;

	/**
	 * The status wrapped as a property
	 */
	TAttribute<EOverrideWidgetStatus::Type> StatusAttribute;

	FOverrideStatus_AddOverride AddOverrideDelegate;
	FOverrideStatus_ClearOverride ClearOverrideDelegate;
	FOverrideStatus_ResetToDefault ResetToDefaultDelegate;
	FOverrideStatus_ValueDiffersFromDefault ValueDiffersFromDefaultDelegate;
};


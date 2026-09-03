//  Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"
#include "UserInterface/Widgets/PropertyUpdatedWidgetBuilder.h"
#include "OverrideStatusSubject.h"

class FOverrideStatusDetailsDisplayManager;

/**
 * A Display builder for the override status combo button
 */
class ANIMATIONEDITORWIDGETS_API FOverrideStatusDetailsWidgetBuilder : public FPropertyUpdatedWidgetBuilder
{
public:
	DECLARE_DELEGATE_RetVal(TSharedRef<SWidget>, FOnGetWidget);

public:

	/**
	 * The constructor, which takes a @code TSharedRef<FDetailsDisplayManager> @endcode to initialize
	 * the Details Display Manager
	 *
	 * @param InDetailsDisplayManager the FDetailsDisplayManager which manages the details display
	 * than a property row   
	 */
	FOverrideStatusDetailsWidgetBuilder(
		const TSharedRef<FOverrideStatusDetailsDisplayManager>& InDetailsDisplayManager,
		const TArray<FOverrideStatusObject>& InObjects,
		const TSharedPtr<const FPropertyPath>& InPropertyPath,
		const FName& InCategory);

	/**
	 * Implements the generation of the Category Menu button SWidget
	 */
	virtual TSharedPtr<SWidget> GenerateWidget() override;

	/**
	 * Converts this into the SWidget it builds
	 */
	virtual ~FOverrideStatusDetailsWidgetBuilder() override;

	FOverrideStatus_CanCreateWidget& OnCanCreateWidget();
	FOverrideStatus_GetStatus& OnGetStatus();
	FOverrideStatus_OnWidgetClicked& OnWidgetClicked();
	FOverrideStatus_OnGetMenuContent& OnGetMenuContent();
	FOverrideStatus_AddOverride& OnAddOverride();
	FOverrideStatus_ClearOverride& OnClearOverride();
	FOverrideStatus_ResetToDefault& OnResetToDefault();
	FOverrideStatus_ValueDiffersFromDefault& OnValueDiffersFromDefault();

private:

	/**
	 * The @code DetailsDisplayManager @endcode which provides an API to manage some of the characteristics of the
	 * details display
	 */
	TSharedRef<FOverrideStatusDetailsDisplayManager> DisplayManager;

	FOverrideStatusSubject Subject;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CycleFunctionController.h"
#include "Templates/SharedPointerFwd.h"
#include "TweenMouseSlidingController.h"
#include "TweenToolbarController.h"

class FUICommandList;

namespace UE::TweeningUtilsEditor
{
/** Holds functionality that may be reused across modules to add tweening widgets to a toolbar. */
struct TWEENINGUTILSEDITOR_API FTweenControllers
{
	/** Manages the toolbar widget */
	FTweenToolbarController ToolbarController;

	/** Cycles functions (Shift + U). */
	FCycleFunctionController CycleFunctionController;

	/** Allows indirect movement of the slider by using u + Move Mouse */
	FTweenMouseSlidingController MouseSlidingController;

	/**
	 * @param InCommandList The command list to bind commands to
	 * @param InTweenModels Contains all tween models that can be selected by the tweening widgets
	 * @param UserPreferredFunctionContext Optional. The config key under which the selected tween function is saved. The function currently saved under this key will be the first one to be selected.
	 */
	explicit FTweenControllers(
		const TSharedRef<FUICommandList>& InCommandList,
		const TSharedRef<ITweenModelContainer>& InTweenModels,
		FName UserPreferredFunctionContext = NAME_None
		);
};
}


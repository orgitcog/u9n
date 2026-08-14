// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayTypes.h"
#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"

/** 
 * Deprecated. This type will be removed in a future release
 * 
 * Optional interface returned from IXRTrackingSystem if the plugin requires being able to grab touch or keyboard input events.
 */
class UE_DEPRECATED(5.6, "IXRInput is deprecated and will be removed in a future release.") IXRInput
{
public:
	/**
	* Passing key events to HMD.
	* If returns 'false' then key will be handled by PlayerController;
	* otherwise, key won't be handled by the PlayerController.
	*/
	virtual bool HandleInputKey(class UPlayerInput*, const struct FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) { return false; }

	/**
	* Passing touch events to HMD.
	* If returns 'false' then touch will be handled by PlayerController;
	* otherwise, touch won't be handled by the PlayerController.
	*/
	virtual bool HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) { return false; }
};


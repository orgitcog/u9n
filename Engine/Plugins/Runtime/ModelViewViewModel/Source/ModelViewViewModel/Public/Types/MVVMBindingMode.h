// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBindingMode.generated.h"


/** */
UENUM()
enum class EMVVMBindingMode : uint8
{
	OneTimeToDestination = 0,
	OneWayToDestination,
	TwoWay,
	OneTimeToSource UMETA(Hidden),
	OneWayToSource,
};


namespace UE::MVVM
{
	[[nodiscard]] MODELVIEWVIEWMODEL_API bool IsForwardBinding(EMVVMBindingMode Mode);
	[[nodiscard]] MODELVIEWVIEWMODEL_API bool IsBackwardBinding(EMVVMBindingMode Mode);
	[[nodiscard]] MODELVIEWVIEWMODEL_API bool IsOneTimeBinding(EMVVMBindingMode Mode);
}

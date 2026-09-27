// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "DataLinkEnums.generated.h"

UENUM()
enum class EDataLinkExecutionResult : uint8
{
	Failed,
	Succeeded,
};

UENUM()
enum class EDataLinkExecutionReply : uint8
{
	Unhandled,
	Handled,
};

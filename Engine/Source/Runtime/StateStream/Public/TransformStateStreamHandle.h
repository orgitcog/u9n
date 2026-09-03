// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateStreamHandle.h"
#include "TransformStateStreamHandle.generated.h"

struct FTransformDynamicState;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle for transform instance

USTRUCT(StateStreamHandle)
struct FTransformHandle : public FStateStreamHandle
{
	GENERATED_USTRUCT_BODY()
};

////////////////////////////////////////////////////////////////////////////////////////////////////

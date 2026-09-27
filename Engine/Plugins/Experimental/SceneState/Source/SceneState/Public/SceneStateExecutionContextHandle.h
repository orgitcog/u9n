// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

namespace UE::SceneState
{

/** Holds a unique number mapping a context in a registry */
class FExecutionContextHandle
{
public:
	bool IsValid() const
	{
		return Id != 0;
	}

	bool operator==(FExecutionContextHandle InOtherHandle) const
	{
		return Id == InOtherHandle.Id;
	}

	friend uint32 GetTypeHash(FExecutionContextHandle InHandle)
	{
		return InHandle.Id;
	}

private:
	uint32 Id = 0;

	friend class FExecutionContextRegistry;
};

} // UE::SceneState

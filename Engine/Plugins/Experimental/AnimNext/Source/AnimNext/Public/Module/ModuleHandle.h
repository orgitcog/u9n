// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextPoolHandle.h"

#include "ModuleHandle.generated.h"

struct FAnimNextModuleInstance;

namespace UE::AnimNext
{

// Opaque handle representing a module instance
using FModuleHandle = TPoolHandle<FAnimNextModuleInstance>;

}

USTRUCT(BlueprintType)
struct FAnimNextModuleHandle
{
	GENERATED_BODY()

	FAnimNextModuleHandle() = default;

	explicit FAnimNextModuleHandle(const UE::AnimNext::FModuleHandle InModuleHandle)
		: ModuleHandle(InModuleHandle)
	{
	}

public:
	UE::AnimNext::FModuleHandle ModuleHandle;
};
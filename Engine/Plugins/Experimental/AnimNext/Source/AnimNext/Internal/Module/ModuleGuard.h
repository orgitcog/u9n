// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Module/ModuleHandle.h"

struct FAnimNextModuleInstance;

namespace UE::AnimNext
{

// RAII helper that guards against concurrent write access to the specified module and all its prerequisites
struct FModuleWriteGuard
{
	UE_NONCOPYABLE(FModuleWriteGuard);

	ANIMNEXT_API explicit FModuleWriteGuard(const FAnimNextModuleInstance* InModuleInstance);
	ANIMNEXT_API ~FModuleWriteGuard();

private:
	const FAnimNextModuleInstance* ModuleInstance = nullptr;
	TArray<FModuleHandle, TInlineAllocator<4>> PrerequisiteHandles;
};

}

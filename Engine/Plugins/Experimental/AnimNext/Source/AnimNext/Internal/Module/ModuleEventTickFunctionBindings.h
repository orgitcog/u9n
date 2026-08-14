// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

struct FTickFunction;
struct FAnimNextModuleInstance;

namespace UE::AnimNext
{

// Binding context for tick functions
struct FTickFunctionBindingContext
{
	FTickFunctionBindingContext(FAnimNextModuleInstance& InModuleInstance, UObject* InObject, UWorld* InWorld)
		: ModuleInstance(InModuleInstance)
		, Object(InObject)
		, World(InWorld)
	{}

	// The module instance
	FAnimNextModuleInstance& ModuleInstance;

	// Object that a module is bound on (e.g. a component)
	UObject* Object = nullptr;

	// World that the object is contained within
	UWorld* World = nullptr;
};

// Function called to bind a tick function to a module event, set up prerequisites etc.
using FModuleEventBindingFunction = TFunction<void(const FTickFunctionBindingContext& InContext, FTickFunction& InTickFunction)>;

}

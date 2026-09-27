// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextModuleInstanceComponent.generated.h"

struct FAnimNextModuleInstance;
struct FAnimNextTraitEvent;

/**
 * FAnimNextModuleInstanceComponent
 *
 * A module instance component is attached and owned by a module instance.
 * It persists as long as it is needed.
 */
USTRUCT()
struct FAnimNextModuleInstanceComponent
{
	GENERATED_BODY()

	FAnimNextModuleInstanceComponent() = default;
	virtual ~FAnimNextModuleInstanceComponent() {}

	ANIMNEXT_API void Initialize(FAnimNextModuleInstance& InOwnerInstance);
	ANIMNEXT_API void Uninitialize();

	// Returns the owning module instance this component lives on
	FAnimNextModuleInstance& GetModuleInstance() { return *OwnerInstance; }

	// Returns the owning module instance this component lives on
	const FAnimNextModuleInstance& GetModuleInstance() const { return *OwnerInstance; }

	// Called when the component is first created to initialize it.
	// This can occur on module initialize or lazily during execution.
	virtual void OnInitialize() {}

	// Called when the component is destroyed. Once created, components persist until the module instance is destroyed.
	virtual void OnUninitialize() {}

	// Called during module execution for any events to be handled
	virtual void OnTraitEvent(FAnimNextTraitEvent& Event) {}

	// Called at end of module execution each frame
	virtual void OnEndExecution(float InDeltaTime) {}

private:
	// The owning module instance this component lives on
	FAnimNextModuleInstance* OwnerInstance = nullptr;

	friend struct FAnimNextModuleInstance;
};

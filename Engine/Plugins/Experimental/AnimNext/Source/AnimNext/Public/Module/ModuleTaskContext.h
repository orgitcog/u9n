// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/TraitEvent.h"

struct FAnimNextModuleInstanceComponent;
struct FAnimNextModuleInstance;
struct FInstancedPropertyBag;
struct FAnimNextModuleInjectionComponent;

namespace UE::AnimNext
{
	struct FScheduleContext;
	enum class EParameterScopeOrdering : int32;
	struct FModuleEventTickFunction;
}

namespace UE::AnimNext
{

// Context passed to schedule task callbacks
struct FModuleTaskContext
{
public:
	// Queues an input trait event
	// Input events will be processed in the next graph update after they are queued
	ANIMNEXT_API void QueueInputTraitEvent(FAnimNextTraitEventPtr Event) const;

	// Access a module instance component of the specified type. If the component exists, then InFunction will be called
	ANIMNEXT_API void TryAccessComponent(UScriptStruct* InComponentType, TFunctionRef<void(FAnimNextModuleInstanceComponent&)> InFunction) const;

	template<typename ComponentType>
	void TryAccessComponent(TFunctionRef<void(ComponentType&)> InFunction) const
	{
		TryAccessComponent(ComponentType::StaticStruct(), [&InFunction](FAnimNextModuleInstanceComponent& InComponent)
		{
			InFunction(static_cast<ComponentType&>(InComponent));
		});
	}

	ANIMNEXT_API FAnimNextModuleInstance* const GetModuleInstance() const;

private:
	FModuleTaskContext(FAnimNextModuleInstance& InModuleInstance);

	// The module instance currently running
	FAnimNextModuleInstance* ModuleInstance;

	friend UE::AnimNext::FModuleEventTickFunction;
	friend ::FAnimNextModuleInjectionComponent;
};

}
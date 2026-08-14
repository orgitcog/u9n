// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextPool.h"
#include "Component/AnimNextWorldSubsystem.h"
#include "Module/ModuleHandle.h"
#include "Module/ModuleTaskContext.h"
#include "AnimNextComponentWorldSubsystem.generated.h"

class UAnimNextComponent;

// Represents AnimNext systems to the AActor/UActorComponent gameplay framework
UCLASS()
class UAnimNextComponentWorldSubsystem : public UAnimNextWorldSubsystem
{
	GENERATED_BODY()

	friend class UAnimNextComponent;

	// Register a component to the subsystem
	void Register(UAnimNextComponent* InComponent);

	// Unregister a component to the subsystem
	// The full release of the module referenced by the component's handle will be deferred after this call is made
	void Unregister(UAnimNextComponent* InComponent);

	// Returns whether the module is enabled.
	bool IsEnabled(const UAnimNextComponent* InComponent) const;

	// Enables or disables the module represented by the supplied handle
	// This operation is deferred until the next time the schedule ticks
	void SetEnabled(UAnimNextComponent* InComponent, bool bInEnabled);

#if UE_ENABLE_DEBUG_DRAWING
	// Enables or disables debug drawing for the module represented by the supplied handle
	// This operation is deferred until the next time the schedule ticks
	void ShowDebugDrawing(UAnimNextComponent* InComponent, bool bInShowDebugDrawing);
#endif

	// Queue a task to run at a particular point in a schedule
	// @param	InComponent			The component to execute the task on
	// @param	InModuleEventName	The name of the event in the module to run the supplied task relative to. If this is NAME_None, then the first valid event will be used.
	// @param	InTaskFunction		The function to run
	// @param	InLocation			Where to run the task, before or after
	void QueueTask(UAnimNextComponent* InComponent, FName InModuleEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation = UE::AnimNext::ETaskRunLocation::Before);

	// Queue an input trait event
	void QueueInputTraitEvent(UAnimNextComponent* InComponent, FAnimNextTraitEventPtr Event);

	// Find the component tick function for the specified event
	// @param	InComponent			The component being searched for the tick function
	// @param	InEventName			The event associated to the wanted tick function
	const FTickFunction* FindTickFunction(const UAnimNextComponent* InComponent, FName InEventName) const;

	// Add a dependency on a tick function to the specified event
	// @param	InComponent			The component whose dependencies are being modified
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function to depend on
	// @param	InEventName			The event to add the dependency to
	void AddDependency(UAnimNextComponent* InComponent, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency);

	// Remove a dependency on a tick function from the specified event
	// @param	InComponent			The component whose dependencies are being modified
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function that was depended on
	// @param	InEventName			The event to remove the dependency from
	void RemoveDependency(UAnimNextComponent* InComponent, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency);

	// Add a dependency on a module event to the specified module event
	// @param	InComponent			The component whose dependencies are being modified
	// @param	InEventName			The event to add the dependency to/from
	// @param	OtherComponent		The other component whose dependencies are being modified
	// @param	OtherEventName		The other module's event to add the dependency to/from
	// @param	InDependency		The kind of dependency to add
	void AddModuleEventDependency(UAnimNextComponent* InComponent, FName InEventName, UAnimNextComponent* OtherComponent, FName OtherEventName, EDependency InDependency);

	// Remove a dependency on a module event to the specified module event
	// @param	InComponent			The component whose dependencies are being modified
	// @param	InEventName			The event to remove the dependency to/from
	// @param	OtherComponent		The other component whose dependencies are being modified
	// @param	OtherEventName		The other module's event to remove the dependency to/from
	// @param	InDependency		The kind of dependency to remove
	void RemoveModuleEventDependency(UAnimNextComponent* InComponent, FName InEventName, UAnimNextComponent* OtherComponent, FName OtherEventName, EDependency InDependency);

#if WITH_EDITOR
	// UAnimNextWorldSubsystem interface
	virtual void OnModuleCompiled(UAnimNextModule* InModule) override;
#endif
};
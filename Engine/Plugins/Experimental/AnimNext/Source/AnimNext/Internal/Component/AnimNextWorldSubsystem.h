// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextPool.h"
#include "Subsystems/WorldSubsystem.h"
#include "Module/ModuleHandle.h"
#include "Module/ModuleTickFunction.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/ModuleTaskContext.h"
#include "Module/TaskRunLocation.h"
#include "AnimNextWorldSubsystem.generated.h"

#define UE_API ANIMNEXT_API

class UAnimNextComponent;
struct FRigUnit_AnimNextInitializeEvent;

namespace UE::AnimNext
{
	struct FModuleEventTickFunction;
	struct FInjectionRequest;
}

namespace UE::AnimNext
{

// A queued action to complete next frame
struct FModulePendingAction
{
	enum class EType : int32
	{
		None = 0,
		ReleaseHandle,			// Payload = FModulePayloadNone
		EnableHandle,			// Payload = bool
		EnableDebugDrawing,		// Payload = bool
		AddPrerequisite,		// Payload = UE::AnimNext::FModuleHandle (to-be-added handle)
		RemovePrerequisite,	    // Payload = UE::AnimNext::FModuleHandle (to-be-removed handle)
	};

	// Marker struct for an 'empty' payload
	struct FModulePayloadNone
	{
	};

	FModulePendingAction() = default;

	FModulePendingAction(FModuleHandle InHandle, EType InType)
		: Handle(InHandle)
		, Type(InType)
	{
		Payload.Set<FModulePayloadNone>(FModulePayloadNone());
	}

	template<typename PayloadType>
	FModulePendingAction(FModuleHandle InHandle, EType InType, PayloadType InPayload)
		: Handle(InHandle)
		, Type(InType)
	{
		Payload.Set<PayloadType>(InPayload);
	}

	TVariant<FModulePayloadNone, bool, UE::AnimNext::FModuleHandle> Payload;

	FModuleHandle Handle;

	EType Type = EType::None;
};

}

// Represents AnimNext systems to the gameplay framework
UCLASS(MinimalAPI, Abstract)
class UAnimNextWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UE_API UAnimNextWorldSubsystem();

	UE_API virtual void BeginDestroy() override;

	// Get the subsystem for the specified object's world
	static UE_API UAnimNextWorldSubsystem* Get(UObject* InObject);
	
	static UE_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// Dependency type passed to AddDependencyHandle etc.
	enum class EDependency : uint8
	{
		// Dependency runs before the specified event
		Prerequisite,
		// Dependency runs after the specified event
		Subsequent
	};

private:
	// UWorldSubsystem interface
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UE_API bool IsValidHandle(UE::AnimNext::FModuleHandle InHandle) const;

	UE_API void FlushPendingActions();

protected:
#if WITH_EDITOR
	// Refresh any entries that use the provided module as it has been recompiled.
	UE_API virtual void OnModuleCompiled(UAnimNextModule* InModule);
#endif
	
	// Register a handle to the subsystem
	UE_API void RegisterHandle(UE::AnimNext::FModuleHandle& InOutHandle, UAnimNextModule* InModule, UObject* InObject, IAnimNextVariableProxyHost* InProxyHost, EAnimNextModuleInitMethod InitMethod);

	// Unregister a handle from the subsystem
	UE_API void UnregisterHandle(UE::AnimNext::FModuleHandle& InOutHandle);

	// Returns whether the module represented by the supplied handle is enabled.
	// If there are pending actions they will take precedent over the actual current state.
	UE_API bool IsHandleEnabled(UE::AnimNext::FModuleHandle InHandle) const;

	// Enables or disables the module represented by the supplied handle
	// This operation is deferred until the next time the schedule ticks
	UE_API void EnableHandle(UE::AnimNext::FModuleHandle InHandle, bool bInEnabled);

#if UE_ENABLE_DEBUG_DRAWING
	// Enables or disables the module's debug drawing
	// This operation is deferred until the next time the schedule ticks
	UE_API void ShowDebugDrawingHandle(UE::AnimNext::FModuleHandle InHandle, bool bInShowDebugDrawing);
#endif

	// Queue a task to run at a particular point in a schedule
	// @param	InHandle			The handle to execute the task on
	// @param	InModuleEventName	The name of the event in the module to run the supplied task relative to. If this is NAME_None, then the first valid event will be used.
	// @param	InTaskFunction		The function to run
	// @param	InLocation			Where to run the task, before or after
	UE_API void QueueTaskHandle(UE::AnimNext::FModuleHandle InHandle, FName InModuleEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation = UE::AnimNext::ETaskRunLocation::Before);

	// Queue an input trait event on the supplied handle
	UE_API void QueueInputTraitEventHandle(UE::AnimNext::FModuleHandle InHandle, FAnimNextTraitEventPtr Event);

	// Add a dependency between equivalent module events for the specified handles
	// @param	InPrerequisiteHandle	The handle of the module that will be executed first
	// @param	InSubsequentHandle		The handle of the module that will be executed second
	UE_API void AddModuleDependencyHandle(UE::AnimNext::FModuleHandle InPrerequisiteHandle, UE::AnimNext::FModuleHandle InSubsequentHandle);

	// Remove a dependency between equivalent module events for the specified handles
	// @param	InPrerequisiteHandle	The handle of the module that would have been executed first
	// @param	InSubsequentHandle		The handle of the module that would have been executed second
	UE_API void RemoveModuleDependencyHandle(UE::AnimNext::FModuleHandle InPrerequisiteHandle, UE::AnimNext::FModuleHandle InSubsequentHandle);

	// Find the const tick function for the specified event. Useful to analyze/log prerequisites. Use AddDependencyHandle, RemoveDependencyHandle to actually modify dependencies
	// @param	InHandle			The handle of the module
	// @param	InEventName			The event associated to the wanted tick function
	UE_API const FTickFunction* FindTickFunctionHandle(UE::AnimNext::FModuleHandle InHandle, FName InEventName) const;

	// Add a dependency on a tick function to the specified event
	// @param	InHandle			The handle of the module whose dependencies are being modified
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function to depend on
	// @param	InEventName			The event to add the dependency to
	// @param	InDependency		The kind of dependency to add
	UE_API void AddDependencyHandle(UE::AnimNext::FModuleHandle InHandle, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency);
	
	// Remove a dependency on a tick function from the specified event
	// @param	InHandle			The handle of the module whose dependencies are being modified
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function that was depended on
	// @param	InEventName			The event to remove the dependency from
	// @param	InDependency		The kind of dependency to remove
	UE_API void RemoveDependencyHandle(UE::AnimNext::FModuleHandle InHandle, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency);

	// Add a dependency on a module event to the specified module event
	// @param	InHandle			The handle of the module whose dependencies are being modified
	// @param	InEventName			The event to add the dependency to/from
	// @param	OtherHandle			The handle of the other module whose dependencies are being modified
	// @param	OtherEventName		The other module's event to add the dependency to/from
	// @param	InDependency		The kind of dependency to add
	UE_API void AddModuleEventDependencyHandle(UE::AnimNext::FModuleHandle InHandle, FName InEventName, UE::AnimNext::FModuleHandle OtherHandle, FName OtherEventName, EDependency InDependency);

	// Remove a dependency on a module event from the specified module event
	// @param	InHandle			The handle of the module whose dependencies are being modified
	// @param	InEventName			The event to remove the dependency to/from
	// @param	OtherHandle			The handle of the other module whose dependencies are being modified
	// @param	OtherEventName		The other module's event to remove the dependency to/from
	// @param	InDependency		The kind of dependency to remove
	UE_API void RemoveModuleEventDependencyHandle(UE::AnimNext::FModuleHandle InHandle, FName InEventName, UE::AnimNext::FModuleHandle OtherHandle, FName OtherEventName, EDependency InDependency);

protected:
	friend struct FRigUnit_AnimNextInitializeEvent;
	friend struct UE::AnimNext::FModuleEventTickFunction;
	friend struct UE::AnimNext::FInjectionRequest;

	// Currently running instances, pooled
	UE::AnimNext::TPool<FAnimNextModuleInstance> Instances;

	// Queued actions
	TArray<UE::AnimNext::FModulePendingAction> PendingActions;

	// Locks for concurrent modifications
	FRWLock InstancesLock;
	FRWLock PendingLock;

#if WITH_EDITOR
	// Handle used to hook module compilation
	FDelegateHandle OnModuleCompiledHandle;
#endif

	// Handle used to hook into pre-world tick
	FDelegateHandle OnWorldPreActorTickHandle;

	// Cached delta time
	float DeltaTime;
};

#undef UE_API

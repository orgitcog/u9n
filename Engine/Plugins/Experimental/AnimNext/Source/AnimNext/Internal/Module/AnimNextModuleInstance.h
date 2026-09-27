// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextModule.h"
#include "Module/ModuleHandle.h"
#include "Component/AnimNextPublicVariablesProxy.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "Module/ModuleTickFunction.h"
#include "TraitCore/TraitEvent.h"
#include "TraitCore/TraitEventList.h"
#include "Module/ModuleHandle.h"
#include "Module/AnimNextModuleInstanceComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "Module/AnimNextModuleInitMethod.h"
#include "Module/TaskRunLocation.h"
#include "AnimNextModuleInstance.generated.h"

struct FAnimNextModuleInstance;
struct FRigUnit_AnimNextRunAnimationGraph_v1;
struct FRigUnit_AnimNextRunAnimationGraph_v2;
class UAnimNextWorldSubsystem;
struct FRigUnit_AnimNextInitializeEvent;
struct FRigUnit_CopyModuleProxyVariables;
struct FAnimNextNotifyDispatcherComponent;
struct FAnimNextGraphInstance;
class IAnimNextVariableProxyHost;

namespace UE::AnimNext
{
	struct FProxyVariablesContext;
	struct FModuleEventTickFunction;
	struct FModuleEndTickFunction;
	struct FModuleInjectionDataInterfaceAdapter;
	struct FModuleWriteGuard;
}

namespace UE::AnimNext::Debug
{
	struct FDebugDraw;
}

namespace UE::AnimNext
{
	using ModuleInstanceComponentMapType = TMap<FName, TInstancedStruct<FAnimNextModuleInstanceComponent>>;
}

// Root memory owner of a parameterized schedule 
USTRUCT()
struct FAnimNextModuleInstance : public FAnimNextDataInterfaceInstance
{
	GENERATED_BODY()

	FAnimNextModuleInstance();
	ANIMNEXT_API FAnimNextModuleInstance(
		UAnimNextModule* InModule,
		UObject* InObject,
		UE::AnimNext::TPool<FAnimNextModuleInstance>* InPool,
		IAnimNextVariableProxyHost* InProxyHost,
		EAnimNextModuleInitMethod InInitMethod);
	ANIMNEXT_API ~FAnimNextModuleInstance();

	// Checks to see if this entry is ticking
	bool IsEnabled() const;

	// Enables/disables the ticking of this entry
	void Enable(bool bInEnabled);

	// Queues an input trait event
	// Input events will be processed in the next graph update after they are queued
	ANIMNEXT_API void QueueInputTraitEvent(FAnimNextTraitEventPtr Event);

	// Queues an output trait event
	// Output events will be processed at the end of the schedule tick
	ANIMNEXT_API void QueueOutputTraitEvent(FAnimNextTraitEventPtr Event);

	// Get the object that this module is bound to
	UObject* GetObject() const { return Object; }

	// Get the module that this instance represents
	ANIMNEXT_API const UAnimNextModule* GetModule() const;

	// Returns a typed module instance component, creating it lazily the first time it is queried
	template<class ComponentType>
	ComponentType& GetComponent();

	// Returns a typed module instance component pointer if found or nullptr otherwise
	template<class ComponentType>
	ComponentType* TryGetComponent();

	// Returns a typed module instance component pointer if found or nullptr otherwise
	template<class ComponentType>
	const ComponentType* TryGetComponent() const;

	// Returns a pointer to the specified component, or nullptr if not found
	ANIMNEXT_API const FAnimNextModuleInstanceComponent* TryGetComponent(int32 ComponentNameHash, FName ComponentName) const;
	ANIMNEXT_API FAnimNextModuleInstanceComponent* TryGetComponent(int32 ComponentNameHash, FName ComponentName);

#if UE_ENABLE_DEBUG_DRAWING
	// Get the debug draw interface
	ANIMNEXT_API FRigVMDrawInterface* GetDebugDrawInterface();

	// Set whether to show the module's debug drawing instructions in the viewport
	ANIMNEXT_API void ShowDebugDrawing(bool bInShowDebugDrawing);
#endif

	// Run a simple task on the GT via FFunctionGraphTask::CreateAndDispatchWhenReady
	ANIMNEXT_API static void RunTaskOnGameThread(TUniqueFunction<void(void)>&& InFunction);

	// Find the tick function for the specified event
	UE::AnimNext::FModuleEventTickFunction* FindTickFunctionByName(FName InEventName);
	const UE::AnimNext::FModuleEventTickFunction* FindTickFunctionByName(FName InEventName) const;

	// Find the first 'user' tick function
	ANIMNEXT_API UE::AnimNext::FModuleEventTickFunction* FindFirstUserTickFunction();

	// Run the specified RigVM event
	ANIMNEXT_API void RunRigVMEvent(FName InEventName, float InDeltaTime);

	// Get the world type that this module was instantiated within
	EWorldType::Type GetWorldType() const { return WorldType; }

	// Get tick functions for this module instance
	ANIMNEXT_API TArrayView<UE::AnimNext::FModuleEventTickFunction> GetTickFunctions();

	UE::AnimNext::FModuleHandle GetHandle() const { return Handle; }

	// Queue a task to run at a particular module event
	// @param	InModuleEvent		The event in the module to run the supplied task relative to. If this is NAME_None or not found then the first 'user' event will be used.
	// @param	InTaskFunction		The function to run
	// @param	InLocation			Where to run the task, before or after
	ANIMNEXT_API void QueueTask(FName InEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation = UE::AnimNext::ETaskRunLocation::Before);

	// Queue a task to run at a particular module event on some other module
	// @param	InModuleHandle		The handle for the module to queue the task on
	// @param	InModuleEventName	The event in the module to run the supplied task relative to. If this is NAME_None or not found then the first user 'event' will be used.
	// @param	InTaskFunction		The function to run
	// @param	InLocation			Where to run the task, before or after
	ANIMNEXT_API void QueueTaskOnOtherModule(const UE::AnimNext::FModuleHandle InOtherModuleHandle, FName InEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation = UE::AnimNext::ETaskRunLocation::Before);

private:
	// Setup the entry
	void Initialize();

	// Tear-down the entry
	void Uninitialize();

	// Clear data that binds the schedule to a runtime (e.g. tick functions) and any instance data
	void ResetBindingsAndInstanceData();

	// Remove all of our tick function's dependencies
	void RemoveAllTickDependencies();

	// Flip proxy variable buffers then copy any dirty values
	void CopyProxyVariables();

#if ANIMNEXT_TRACE_ENABLED
	// trace this module if it hasn't already been traced this frame
	ANIMNEXT_API void Trace();
#endif

#if WITH_EDITOR
	// Resets internal state if the module we are bound to is recompiled in editor
	void OnModuleCompiled();
#endif

	// Give the module components a chance to handle events and call their OnEndExecution extension point
	void EndExecution(float InDeltaTime);

	// Handles trait events at the module level.
	// Called in the module end tick function before any events are dispatched.
	void RaiseTraitEvents(const UE::AnimNext::FTraitEventList& EventList);

	// Add a prerequisite instance
	void AddPrerequisite(FAnimNextModuleInstance& InPrerequisiteInstance);

	// Remove a prerequisite instance
	void RemovePrerequisite(FAnimNextModuleInstance& InPrerequisiteInstance);

	// @return true if the instance is a prerequisite instance
	ANIMNEXT_API bool IsPrerequisite(const FAnimNextModuleInstance& InPrerequisiteInstance) const;

	// Call the supplied function for each prerequisite module
	ANIMNEXT_API void ForEachPrerequisite(TFunctionRef<void(FAnimNextModuleInstance& InPrerequisiteInstance)>) const;

private:
	friend UE::AnimNext::FModuleEventTickFunction;
	friend UE::AnimNext::FModuleEndTickFunction;
	friend FRigUnit_AnimNextRunAnimationGraph_v1;
	friend FRigUnit_AnimNextRunAnimationGraph_v2;
	friend UAnimNextWorldSubsystem;
	friend FRigUnit_AnimNextInitializeEvent;
	friend UE::AnimNext::FProxyVariablesContext;
	friend FRigUnit_CopyModuleProxyVariables;
	friend UE::AnimNext::FModuleInjectionDataInterfaceAdapter;
	friend UE::AnimNext::FModuleWriteGuard;
	friend FAnimNextNotifyDispatcherComponent;
	friend FAnimNextGraphInstance;

	// Object this entry is bound to
	UPROPERTY(Transient)
	TObjectPtr<UObject> Object = nullptr;

	// The pool that this module instance exists in
	UE::AnimNext::TPool<FAnimNextModuleInstance>* Pool = nullptr;

	// Seperate proxy host ptr - TODO: this only exists because of UHT not parsing Verse native classes right now.
	// SHould be removed and replaced with a cast of the Cast<IAnimNextVariableProxyHost>(Object) when VNI gets replaced.
	IAnimNextVariableProxyHost* ProxyHost = nullptr;

	// Copy of the handle that represents this entry to client systems
	UE::AnimNext::FModuleHandle Handle;

	// Pre-allocated graph of tick functions
	TArray<UE::AnimNext::FModuleEventTickFunction> TickFunctions;

	// Reference-counted prerequisites. Prerequisites can be requested/unrequested multiple times, so are only truly removed when references are zero
	struct FPrerequisiteReference
	{
		UE::AnimNext::FModuleHandle Handle;
		int32 ReferenceCount = 0;
	};

	// All pre-requisite references, only modified from the game thread
	TArray<FPrerequisiteReference> PrerequisiteRefs;

	// All subsequent references, only modified from the game thread
	TArray<UE::AnimNext::FModuleHandle> SubsequentRefs;

	// Input event list to be processed on the next update
	UE::AnimNext::FTraitEventList InputEventList;

	// Output event list to be processed at the end of the schedule tick
	UE::AnimNext::FTraitEventList OutputEventList;

	// Access detector to ensure module prerequisite and general execution access is safe
	// This protects all calls & tick functions and prevents prerequisites from running unrelated tick functions concurrently
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(AccessDetector);

	// Lock to ensure event list actions are thread safe
	FRWLock EventListLock;

	// Proxy public variables
	UPROPERTY(Transient)
	FAnimNextPublicVariablesProxy PublicVariablesProxy;

#if UE_ENABLE_DEBUG_DRAWING
	// Debug draw support
	// TODO: Move this to a 'module component' once that code is checked in
	TUniquePtr<UE::AnimNext::Debug::FDebugDraw> DebugDraw;
#endif

	// Components of this module
	UPROPERTY()
	TMap<FName, TInstancedStruct<FAnimNextModuleInstanceComponent>> ComponentMap;

	enum class EInitState : uint8
	{
		NotInitialized,

		CreatingTasks,

		BindingTasks,

		SetupVariables,

		PendingInitializeEvent,

		FirstUpdate,

		Initialized
	};

	// Current initialization state
	EInitState InitState = EInitState::NotInitialized;

	enum class ERunState : uint8
	{
		NotInitialized,

		Running,

		Paused,
	};

	// Current running state
	ERunState RunState = ERunState::NotInitialized;

	// Transition to the specified init state, verifying that the current state is valid
	void TransitionToInitState(EInitState InNewState);

	// Transition to the specified run state, verifying that the current state is valid
	void TransitionToRunState(ERunState InNewState);

	// How this entry initializes
	EAnimNextModuleInitMethod InitMethod = EAnimNextModuleInitMethod::InitializeAndPauseInEditor;

	// Whether this represents an editor object 
	EWorldType::Type WorldType = EWorldType::None;

#if WITH_EDITOR
	// Whether we are currently recreating this instance because of compilation/reinstancing
	bool bIsRecreatingOnCompile : 1 = false;
#endif

private:
	// Adds the specified component and returns a reference to it
	ANIMNEXT_API FAnimNextModuleInstanceComponent& AddComponentInternal(int32 ComponentNameHash, FName ComponentName, TInstancedStruct<FAnimNextModuleInstanceComponent>&& Component);

#if ANIMNEXT_TRACE_ENABLED
	bool bTracedThisFrame = false;
#endif
};

template<>
struct TStructOpsTypeTraits<FAnimNextModuleInstance> : public TStructOpsTypeTraitsBase2<FAnimNextModuleInstance>
{
	enum
	{
		WithCopy = false
	};
};

template<typename ComponentType>
ComponentType& FAnimNextModuleInstance::GetComponent()
{
	const FName ComponentName = ComponentType::StaticStruct()->GetFName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	if (ComponentType* Component = static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName)))
	{
		return *Component;
	}

	TInstancedStruct<ComponentType> ComponentStruct = TInstancedStruct<ComponentType>::Make();
	ComponentStruct.template GetMutable<ComponentType>().Initialize(*this);
	return static_cast<ComponentType&>(AddComponentInternal(ComponentNameHash, ComponentName, MoveTemp(ComponentStruct)));
}

template<class ComponentType>
ComponentType* FAnimNextModuleInstance::TryGetComponent()
{
	const FName ComponentName = ComponentType::StaticStruct()->GetFName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}

template<class ComponentType>
const ComponentType* FAnimNextModuleInstance::TryGetComponent() const
{
	const FName ComponentName = ComponentType::StaticStruct()->GetFName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}

#if ANIMNEXT_TRACE_ENABLED
#define TRACE_ANIMNEXT_MODULE(ModuleInstance) ModuleInstance.Trace();
#else
#define TRACE_ANIMNEXT_MODULE(ModuleInstance)
#endif

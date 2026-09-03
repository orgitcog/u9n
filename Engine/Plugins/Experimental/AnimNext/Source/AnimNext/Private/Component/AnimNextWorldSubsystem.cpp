// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/AnimNextWorldSubsystem.h"

#include "Engine/World.h"
#include "Logging/StructuredLog.h"
#include "Module/AnimNextModule.h"
#include "Module/ModuleHandle.h"
#include "Module/ModuleTaskContext.h"
#include "AnimNextDebugDraw.h"
#include "Module/ModuleGuard.h"

UAnimNextWorldSubsystem::UAnimNextWorldSubsystem()
{
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
#if WITH_EDITOR
		OnModuleCompiledHandle = UAnimNextModule::OnModuleCompiled().AddUObject(this, &UAnimNextWorldSubsystem::OnModuleCompiled);
#endif

		// Kick off root task at the start of each world tick
		OnWorldPreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddLambda([this](UWorld* InWorld, ELevelTick InTickType, float InDeltaSeconds)
		{
			if (InTickType == LEVELTICK_All || InTickType == LEVELTICK_ViewportsOnly)
			{
				// Flush actions here as they require game thread callbacks (e.g. to reconfigure tick functions)
				FlushPendingActions();
				DeltaTime = InDeltaSeconds;
			}
		});
	}
}

void UAnimNextWorldSubsystem::BeginDestroy()
{
	Super::BeginDestroy();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
#if WITH_EDITOR
		UAnimNextModule::OnModuleCompiled().Remove(OnModuleCompiledHandle);
#endif

		FWorldDelegates::OnWorldPreActorTick.Remove(OnWorldPreActorTickHandle);

		{
			FRWScopeLock InstancesLockScope(InstancesLock, SLT_Write);
			for (FAnimNextModuleInstance& Instance : Instances)
			{
				Instance.RemoveAllTickDependencies();
				Instance.Uninitialize();
			}
			Instances = UE::AnimNext::TPool<FAnimNextModuleInstance>(); // Force instances destruction
		}
	}
}

UAnimNextWorldSubsystem* UAnimNextWorldSubsystem::Get(UObject* InObject)
{
	if(InObject == nullptr)
	{
		return nullptr;
	}

	UWorld* World = InObject->GetWorld();
	if(World == nullptr)
	{
		return nullptr;
	}

	return World->GetSubsystem<UAnimNextWorldSubsystem>();
}

void UAnimNextWorldSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UAnimNextWorldSubsystem* This = CastChecked<UAnimNextWorldSubsystem>(InThis);
	for(FAnimNextModuleInstance& Instance : This->Instances)
	{
		Collector.AddPropertyReferencesWithStructARO(FAnimNextModuleInstance::StaticStruct(), &Instance, InThis);
	}
}

bool UAnimNextWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	switch (WorldType)
	{
	case EWorldType::Game:
	case EWorldType::Editor:
	case EWorldType::PIE:
	case EWorldType::EditorPreview:
	case EWorldType::GamePreview:
		return true;
	}

	return false;
}

bool UAnimNextWorldSubsystem::IsValidHandle(UE::AnimNext::FModuleHandle InHandle) const
{
	return Instances.IsValidHandle(InHandle);
}

void UAnimNextWorldSubsystem::FlushPendingActions()
{
	using namespace UE::AnimNext;

	FRWScopeLock PendingLockScope(PendingLock, SLT_Write);

	if (PendingActions.Num() > 0)
	{
		FRWScopeLock InstancesLockScope(InstancesLock, SLT_Write);

		for (FModulePendingAction& PendingAction : PendingActions)
		{
			switch (PendingAction.Type)
			{
			case FModulePendingAction::EType::ReleaseHandle:
				if (IsValidHandle(PendingAction.Handle))
				{
					FAnimNextModuleInstance& Instance = Instances.Get(PendingAction.Handle);
					Instance.Uninitialize();
					Instances.Release(PendingAction.Handle);
				}
				break;
			case FModulePendingAction::EType::EnableHandle:
				if (IsValidHandle(PendingAction.Handle))
				{
					FAnimNextModuleInstance& Instance = Instances.Get(PendingAction.Handle);
					Instance.Enable(PendingAction.Payload.Get<bool>());
				}
				break;
			case FModulePendingAction::EType::EnableDebugDrawing:
#if UE_ENABLE_DEBUG_DRAWING
				if (IsValidHandle(PendingAction.Handle))
				{
					FAnimNextModuleInstance& Instance = Instances.Get(PendingAction.Handle);
					Instance.ShowDebugDrawing(PendingAction.Payload.Get<bool>());
				}
#endif
				break;

			case FModulePendingAction::EType::AddPrerequisite:
				{					
					FAnimNextModuleInstance* SubsequentInstance = Instances.TryGet(PendingAction.Handle);
					FAnimNextModuleInstance* PrerequisiteInstance = Instances.TryGet(PendingAction.Payload.Get<UE::AnimNext::FModuleHandle>());
					if(PrerequisiteInstance != nullptr && SubsequentInstance != nullptr)
					{
						SubsequentInstance->AddPrerequisite(*PrerequisiteInstance);
					}
					else
					{
						UE_LOGFMT(LogAnimation, Warning, "FlushPendingActions: Invalid Module handle(s) provided for AddPrerequisite action SubsequentHandle: %i PrerequisiteInstance: %i", PendingAction.Handle.IsValid(), PendingAction.Payload.Get<UE::AnimNext::FModuleHandle>().IsValid());
					}
				
					break;
				}
			case FModulePendingAction::EType::RemovePrerequisite:
				{					
					FAnimNextModuleInstance* SubsequentInstance = Instances.TryGet(PendingAction.Handle);
					FAnimNextModuleInstance* PrerequisiteInstance = Instances.TryGet(PendingAction.Payload.Get<UE::AnimNext::FModuleHandle>());
					if(PrerequisiteInstance != nullptr && SubsequentInstance != nullptr)
					{
						if (SubsequentInstance->IsPrerequisite(*PrerequisiteInstance))
						{							
							SubsequentInstance->RemovePrerequisite(*PrerequisiteInstance);
						}
						else
						{
							UE_LOGFMT(LogAnimation, Warning, "FlushPendingActions: Trying to remove prerequisite ModuleInstance which isn't actually a prerequisite");
						}
					}
					else
					{
						UE_LOGFMT(LogAnimation, Warning, "FlushPendingActions: Invalid Module handle(s) provided for RemovePrerequisite action SubsequentHandle: %i PrerequisiteInstance: %i", PendingAction.Handle.IsValid(), PendingAction.Payload.Get<UE::AnimNext::FModuleHandle>().IsValid());
					}
				
					break;
				}		
			default:
				checkNoEntry();
				break;
			}
		}

		PendingActions.Reset();
	}
}

void UAnimNextWorldSubsystem::RegisterHandle(UE::AnimNext::FModuleHandle& InOutHandle, UAnimNextModule* InModule, UObject* InObject, IAnimNextVariableProxyHost* InProxyHost, EAnimNextModuleInitMethod InitMethod)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	FWriteScopeLock InstancesLockScope(InstancesLock);

	InOutHandle = Instances.Emplace(InModule, InObject, &Instances, InProxyHost, InitMethod);
	FAnimNextModuleInstance& Instance = Instances.Get(InOutHandle);
	Instance.Handle = InOutHandle;
	Instance.Initialize();
}

void UAnimNextWorldSubsystem::UnregisterHandle(UE::AnimNext::FModuleHandle& InOutHandle)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());

	if(IsValidHandle(InOutHandle))
	{
		FWriteScopeLock InstancesLockScope(InstancesLock);
		FAnimNextModuleInstance& Instance = Instances.Get(InOutHandle);

#if UE_ENABLE_DEBUG_DRAWING
		// Remove debug drawing immediately as the renderer will need to know about this before EOF
		Instance.DebugDraw->RemovePrimitive();
#endif

		// Remove all tick dependencies immediately, as once the handle has been invalidated there is no way for external systems to remove their dependencies
		Instance.RemoveAllTickDependencies();

		// Remove this module as a dependency on all other modules that depend upon it before the handle is invalidated
		for (const UE::AnimNext::FModuleHandle& SubsequentHandle : Instance.SubsequentRefs)
		{
			RemoveModuleDependencyHandle(InOutHandle, SubsequentHandle);
		}

		PendingActions.Emplace(InOutHandle, FModulePendingAction::EType::ReleaseHandle);
		InOutHandle.Reset();
	}
}

bool UAnimNextWorldSubsystem::IsHandleEnabled(UE::AnimNext::FModuleHandle InHandle) const
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (IsValidHandle(InHandle))
	{
		// The last pending action takes precedent here if present
		int32 PendingActionIndex = PendingActions.FindLastByPredicate([&InHandle](const FModulePendingAction& PendingAction) { return PendingAction.Handle == InHandle && PendingAction.Type == FModulePendingAction::EType::EnableHandle;});

		if (PendingActionIndex != INDEX_NONE)
		{
			return PendingActions[PendingActionIndex].Payload.Get<bool>();
		}

		// Otherwise return the current value on the instance
		const FAnimNextModuleInstance& Instance = Instances.Get(InHandle);
		return Instance.IsEnabled();
	}
	return false;
}

void UAnimNextWorldSubsystem::EnableHandle(UE::AnimNext::FModuleHandle InHandle, bool bInEnabled)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (IsValidHandle(InHandle))
	{
		PendingActions.Emplace(InHandle, FModulePendingAction::EType::EnableHandle, bInEnabled);
	}
}

#if UE_ENABLE_DEBUG_DRAWING
void UAnimNextWorldSubsystem::ShowDebugDrawingHandle(UE::AnimNext::FModuleHandle InHandle, bool bInShowDebugDrawing)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (IsValidHandle(InHandle))
	{
		PendingActions.Emplace(InHandle, FModulePendingAction::EType::EnableDebugDrawing, bInShowDebugDrawing);
	}
}
#endif

void UAnimNextWorldSubsystem::QueueTaskHandle(UE::AnimNext::FModuleHandle InOutHandle, FName InModuleEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (IsValidHandle(InOutHandle))
	{
		FAnimNextModuleInstance& Instance = Instances.Get(InOutHandle);
		Instance.QueueTask(InModuleEventName, MoveTemp(InTaskFunction), InLocation);
	}
}

#if WITH_EDITOR

void UAnimNextWorldSubsystem::OnModuleCompiled(UAnimNextModule* InModule)
{
	// Cant do this while we are running in a world tick
	check(!GetWorld()->bInTick); 

	for(FAnimNextModuleInstance& Instance : Instances)
	{
		if(Instance.GetModule() == InModule)
		{
			Instance.OnModuleCompiled();
		}
	}
}

#endif

void UAnimNextWorldSubsystem::QueueInputTraitEventHandle(UE::AnimNext::FModuleHandle InHandle, FAnimNextTraitEventPtr Event)
{
	using namespace UE::AnimNext;

	QueueTaskHandle(InHandle, NAME_None, [Event = MoveTemp(Event)](const FModuleTaskContext& InContext)
	{
		InContext.QueueInputTraitEvent(Event);
	},
	ETaskRunLocation::Before);
}

void UAnimNextWorldSubsystem::AddModuleDependencyHandle(UE::AnimNext::FModuleHandle InPrerequisiteHandle, UE::AnimNext::FModuleHandle InSubsequentHandle)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (!IsValidHandle(InPrerequisiteHandle) || !IsValidHandle(InSubsequentHandle))
	{
		return;
	}

	PendingActions.Emplace(InSubsequentHandle, FModulePendingAction::EType::AddPrerequisite, InPrerequisiteHandle);
}

void UAnimNextWorldSubsystem::RemoveModuleDependencyHandle(UE::AnimNext::FModuleHandle InPrerequisiteHandle, UE::AnimNext::FModuleHandle InSubsequentHandle)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (!IsValidHandle(InPrerequisiteHandle) || !IsValidHandle(InSubsequentHandle))
	{
		return;
	}

	PendingActions.Emplace(InSubsequentHandle, FModulePendingAction::EType::RemovePrerequisite, InPrerequisiteHandle);
}

const FTickFunction* UAnimNextWorldSubsystem::FindTickFunctionHandle(UE::AnimNext::FModuleHandle InHandle, FName InEventName) const
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (IsValidHandle(InHandle))
	{
		const FAnimNextModuleInstance& Instance = Instances.Get(InHandle);
		const FModuleEventTickFunction* TickFunction = Instance.FindTickFunctionByName(InEventName);
		if(TickFunction == nullptr)
		{
			UE_LOGFMT(LogAnimation, Warning, "FindTickFunctionHandle: Could not find event '{EventName}' in module '{ModuleName}'", InEventName, Instance.GetDataInterfaceName());
			return nullptr;
		}
		
		if(TickFunction->bUserEvent)
		{
			return TickFunction;
		}

		UE_LOGFMT(LogAnimation, Warning, "FindTickFunctionHandle: Event '{EventName}' in module '{ModuleName}' is not a bUserEvent, therefore cannot be exposed", InEventName, Instance.GetDataInterfaceName());
	}
	return nullptr;
}

void UAnimNextWorldSubsystem::AddDependencyHandle(UE::AnimNext::FModuleHandle InHandle, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (IsValidHandle(InHandle))
	{
		FAnimNextModuleInstance& Instance = Instances.Get(InHandle);
		FModuleEventTickFunction* TickFunction = Instance.FindTickFunctionByName(InEventName);
		if(TickFunction == nullptr)
		{
			UE_LOGFMT(LogAnimation, Warning, "AddDependencyHandle: Could not find event '{EventName}' in module '{ModuleName}'", InEventName, Instance.GetDataInterfaceName());
			return;
		}

		if(InDependency == EDependency::Prerequisite)
		{
			TickFunction->AddPrerequisite(InObject, InTickFunction);
		}
		else
		{
			TickFunction->AddSubsequent(InObject, InTickFunction);
		}
	}
}

void UAnimNextWorldSubsystem::RemoveDependencyHandle(UE::AnimNext::FModuleHandle InHandle, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	if (IsValidHandle(InHandle))
	{
		FAnimNextModuleInstance& Instance = Instances.Get(InHandle);
		FModuleEventTickFunction* TickFunction = Instance.FindTickFunctionByName(InEventName);
		if(TickFunction == nullptr)
		{
			UE_LOGFMT(LogAnimation, Warning, "RemoveDependencyHandle: Could not find event '{EventName}' in module '{ModuleName}'", InEventName, Instance.GetDataInterfaceName());
			return;
		}

		if(InDependency == EDependency::Prerequisite)
		{
			TickFunction->RemovePrerequisite(InObject, InTickFunction);
		}
		else
		{
			TickFunction->RemoveSubsequent(InObject, InTickFunction);
		}
	}
}

void UAnimNextWorldSubsystem::AddModuleEventDependencyHandle(UE::AnimNext::FModuleHandle InHandle, FName InEventName, UE::AnimNext::FModuleHandle OtherHandle, FName OtherEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InHandle != OtherHandle);
	if (IsValidHandle(OtherHandle))
	{
		FAnimNextModuleInstance& Instance = Instances.Get(OtherHandle);
		FModuleEventTickFunction* TickFunction = Instance.FindTickFunctionByName(OtherEventName);
		if(TickFunction == nullptr)
		{
			UE_LOGFMT(LogAnimation, Warning, "AddDependencyHandle: Could not find event '{EventName}' in module '{ModuleName}'", OtherEventName, Instance.GetDataInterfaceName());
			return;
		}

		AddDependencyHandle(InHandle, Instance.GetObject(), *TickFunction, InEventName, InDependency);
	}
}

void UAnimNextWorldSubsystem::RemoveModuleEventDependencyHandle(UE::AnimNext::FModuleHandle InHandle, FName InEventName, UE::AnimNext::FModuleHandle OtherHandle, FName OtherEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InHandle != OtherHandle);
	if (IsValidHandle(InHandle))
	{
		FAnimNextModuleInstance& Instance = Instances.Get(InHandle);
		FModuleEventTickFunction* TickFunction = Instance.FindTickFunctionByName(InEventName);
		if(TickFunction == nullptr)
		{
			UE_LOGFMT(LogAnimation, Warning, "RemoveDependencyHandle: Could not find event '{EventName}' in module '{ModuleName}'", OtherEventName, Instance.GetDataInterfaceName());
			return;
		}

		RemoveDependencyHandle(InHandle, Instance.GetObject(), *TickFunction, InEventName, InDependency);
	}
}

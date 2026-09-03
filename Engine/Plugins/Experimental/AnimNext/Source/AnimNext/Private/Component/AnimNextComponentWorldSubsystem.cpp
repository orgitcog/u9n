// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextComponentWorldSubsystem.h"

#include "Component/AnimNextComponent.h"
#include "Engine/World.h"
#include "Logging/StructuredLog.h"
#include "Module/AnimNextModule.h"
#include "Module/ModuleHandle.h"
#include "Module/ModuleTaskContext.h"

void UAnimNextComponentWorldSubsystem::Register(UAnimNextComponent* InComponent)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	RegisterHandle(InComponent->ModuleHandle, InComponent->Module, InComponent, InComponent, InComponent->InitMethod);
}

void UAnimNextComponentWorldSubsystem::Unregister(UAnimNextComponent* InComponent)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	UnregisterHandle(InComponent->ModuleHandle);
}

void UAnimNextComponentWorldSubsystem::SetEnabled(UAnimNextComponent* InComponent, bool bInEnabled)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	EnableHandle(InComponent->ModuleHandle, bInEnabled);
}

bool UAnimNextComponentWorldSubsystem::IsEnabled(const UAnimNextComponent* InComponent) const
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	return IsHandleEnabled(InComponent->ModuleHandle);
}

#if UE_ENABLE_DEBUG_DRAWING
void UAnimNextComponentWorldSubsystem::ShowDebugDrawing(UAnimNextComponent* InComponent, bool bInShowDebugDrawing)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	ShowDebugDrawingHandle(InComponent->ModuleHandle, bInShowDebugDrawing);
}
#endif

void UAnimNextComponentWorldSubsystem::QueueTask(UAnimNextComponent* InComponent, FName InModuleEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	QueueTaskHandle(InComponent->ModuleHandle, InModuleEventName, MoveTemp(InTaskFunction), InLocation);
}

void UAnimNextComponentWorldSubsystem::QueueInputTraitEvent(UAnimNextComponent* InComponent, FAnimNextTraitEventPtr Event)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	QueueInputTraitEventHandle(InComponent->ModuleHandle, Event);
}

const FTickFunction* UAnimNextComponentWorldSubsystem::FindTickFunction(const UAnimNextComponent* InComponent, FName InEventName) const
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	return FindTickFunctionHandle(InComponent->ModuleHandle, InEventName);
}

void UAnimNextComponentWorldSubsystem::AddDependency(UAnimNextComponent* InComponent, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	AddDependencyHandle(InComponent->ModuleHandle, InObject, InTickFunction, InEventName, InDependency);
}

void UAnimNextComponentWorldSubsystem::RemoveDependency(UAnimNextComponent* InComponent, UObject* InObject, FTickFunction& InTickFunction, FName InEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent);
	RemoveDependencyHandle(InComponent->ModuleHandle, InObject, InTickFunction, InEventName, InDependency);
}

void UAnimNextComponentWorldSubsystem::AddModuleEventDependency(UAnimNextComponent* InComponent, FName InEventName, UAnimNextComponent* OtherComponent, FName OtherEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent && OtherComponent && InComponent != OtherComponent);
	AddModuleEventDependencyHandle(InComponent->ModuleHandle, InEventName, OtherComponent->ModuleHandle, OtherEventName, InDependency);
}

void UAnimNextComponentWorldSubsystem::RemoveModuleEventDependency(UAnimNextComponent* InComponent, FName InEventName, UAnimNextComponent* OtherComponent, FName OtherEventName, EDependency InDependency)
{
	using namespace UE::AnimNext;
	check(IsInGameThread());
	check(InComponent && OtherComponent && InComponent != OtherComponent);
	RemoveModuleEventDependencyHandle(InComponent->ModuleHandle, InEventName, OtherComponent->ModuleHandle, OtherEventName, InDependency);
}

#if WITH_EDITOR

void UAnimNextComponentWorldSubsystem::OnModuleCompiled(UAnimNextModule* InModule)
{
	Super::OnModuleCompiled(InModule);

	for(FAnimNextModuleInstance& Instance : Instances)
	{
		if(Instance.GetModule() == InModule)
		{
			UAnimNextComponent* AnimNextComponent = CastChecked<UAnimNextComponent>(Instance.GetObject());
			AnimNextComponent->OnModuleCompiled();
		}
	}
}

#endif
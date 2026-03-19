// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_AnimNextSetNotifyContext.h"

#include "Components/SkeletalMeshComponent.h"
#include "Module/AnimNextModuleInstance.h"
#include "Traits/NotifyDispatcher.h"

FRigUnit_AnimNextSetNotifyContext_Execute()
{
	FAnimNextModuleInstance& ModuleInstance = ExecuteContext.GetContextData<FAnimNextModuleContextData>().GetModuleInstance();
	FAnimNextNotifyDispatcherComponent& NotifyDispatcher = ModuleInstance.GetComponent<FAnimNextNotifyDispatcherComponent>();
	NotifyDispatcher.SkeletalMeshComponent = SkeletalMeshComponent;
	NotifyDispatcher.NotifyQueue.PredictedLODLevel = SkeletalMeshComponent ? SkeletalMeshComponent->GetPredictedLODLevel() : 0;
}
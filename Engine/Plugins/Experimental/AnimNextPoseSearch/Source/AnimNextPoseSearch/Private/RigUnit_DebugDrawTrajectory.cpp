// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_DebugDrawTrajectory.h"

#include "Animation/TrajectoryTypes.h"
#include "Component/AnimNextComponent.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "Module/AnimNextModuleInstance.h"

FRigUnit_DebugDrawTrajectory_Execute()
{
#if ENABLE_ANIM_DEBUG
	if (!Enabled)
		return;

	using namespace UE::AnimNext;

	const FAnimNextModuleContextData& ModuleContextData = ExecuteContext.GetContextData<FAnimNextModuleContextData>();
	FAnimNextModuleInstance& ModuleInstance = ModuleContextData.GetModuleInstance();

	// TODO: this is not thread safe at the moment. Once we get the 'weak semantics' CL checked in, we can move commonly-used debug info
	// into a module component that can be used to access GT data where needed. 
	UAnimNextComponent* Component = Cast<UAnimNextComponent>(ModuleInstance.GetObject());

	UTransformTrajectoryBlueprintLibrary::DebugDrawTrajectory(Trajectory, Component, LogPoseSearch, ELogVerbosity::Display, DebugThickness, DebugOffset);
#endif
}

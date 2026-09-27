// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"
#include "Graph/AnimNext_LODPose.h"
#include "Graph/RigUnit_AnimNextBase.h"
#include "RemapPoseData.h"
#include "AnimNextStats.h"

#include "RigUnit_AnimNextRemapPose.generated.h"

DECLARE_CYCLE_STAT_EXTERN(TEXT("UAF RigUnit: Remap Pose"), STAT_AnimNext_RigUnit_RemapPose, STATGROUP_AnimNext, ANIMNEXT_API);

/** Remaps an anim graph pose from to another skeletal mesh component. */
USTRUCT(meta=(DisplayName="Remap Pose", Category="Animation Graph", NodeColor="0, 1, 1", Keywords="Output,Pose,Port"))
struct FRigUnit_AnimNextRemapPose : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	// Pose to read
	UPROPERTY(EditAnywhere, DisplayName = "Input Pose", Category = "Graph", meta = (Input))
	FAnimNextGraphLODPose Pose;

	UPROPERTY(EditAnywhere, DisplayName = "Converted Pose", Category = "Graph", meta = (Output))
	FAnimNextGraphLODPose Result;

	UPROPERTY(EditAnywhere, DisplayName = "Target Ref Pose", Category = "Graph", meta = (Input))
	FAnimNextGraphReferencePose TargetAnimGraphRefPose;

	// The execution result
	UPROPERTY(EditAnywhere, DisplayName = "Execute", Category = "BeginExecution", meta = (Input, Output))
	FAnimNextExecuteContext ExecuteContext;

	UPROPERTY(meta = (Hidden))
	FRemapPoseData RemapPoseData; // TODO: Store this per remap node or inside the data registry? We don't have ref counting ref poses and don't know when to remove these though.
};

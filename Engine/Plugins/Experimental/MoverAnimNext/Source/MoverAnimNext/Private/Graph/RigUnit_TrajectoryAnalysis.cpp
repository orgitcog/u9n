// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_TrajectoryAnalysis.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"

FRigUnit_GetTrajectorySampleAtTime_Execute()
{
	UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(InTrajectory, Time, OutTrajectorySample, bExtrapolate);
}

FRigUnit_GetTrajectoryVelocity_Execute()
{
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(InTrajectory, Time1, Time2, OutVelocity, bExtrapolate);
}

FRigUnit_GetTrajectoryAngularVelocity_Execute()
{
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(InTrajectory, Time1, Time2, OutAngularVelocity, bExtrapolate);
}
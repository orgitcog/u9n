// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_GenerateMoverTrajectory.h"

#include "Component/AnimNextComponent.h"
#include "MoverComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"

FRigUnit_GenerateMoverTrajectory_Execute()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRigUnit_GenerateMoverTrajectory_Execute);
	using namespace UE::AnimNext;

	// Note this logic is based on UPoseSearchTrajectoryLibrary::PoseSearchGeneratePredictorTrajectory

	FPoseSearchTrajectoryData::FSampling TrajectoryDataSampling;
	TrajectoryDataSampling.NumHistorySamples = NumHistorySamples;
	TrajectoryDataSampling.SecondsPerHistorySample = HistorySamplingInterval;
	TrajectoryDataSampling.NumPredictionSamples = NumPredictionSamples;
	TrajectoryDataSampling.SecondsPerPredictionSample = PredictionSamplingInterval;

	// TODO: handle controller yaw
	//TrajectoryDataState.DesiredControllerYawLastUpdate = InOutDesiredControllerYawLastUpdate;

	FVector CurrentPosition = FVector::ZeroVector;
	FVector CurrentVelocity = FVector::ZeroVector;
	FQuat CurrentFacing = FQuat::Identity;

	if (MoverComponent == nullptr)
	{
		// Cannot execute, abort
		// Toreview: error / warning logging necessary?
		return;
	}

	UMoverTrajectoryPredictor::GetCurrentState(*MoverComponent, CurrentPosition, CurrentFacing, CurrentVelocity);
	UPoseSearchTrajectoryLibrary::InitTrajectorySamples(InOutTrajectory, CurrentPosition, CurrentFacing, TrajectoryDataSampling, DeltaTime);
	UPoseSearchTrajectoryLibrary::UpdateHistory_TransformHistory(InOutTrajectory, CurrentPosition, CurrentVelocity, TrajectoryDataSampling, DeltaTime);
	UMoverTrajectoryPredictor::Predict(*MoverComponent, InOutTrajectory,
		NumPredictionSamples + 1, PredictionSamplingInterval, NumHistorySamples);
}

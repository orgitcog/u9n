// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"
#include "Graph/RigUnit_AnimNextBase.h"
#include "Animation/TrajectoryTypes.h"

#include "RigUnit_GenerateMoverTrajectory.generated.h"

class UMoverComponent;

USTRUCT(meta = (DisplayName = "Generate Trajectory from Mover", Category = "Mover", NodeColor = "0, 1, 1", Keywords = "Mover, Trajectory"))
struct FRigUnit_GenerateMoverTrajectory : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (Input))
	TObjectPtr<UMoverComponent> MoverComponent;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (Input))
	// This should be the most recent simulation time that was used to get us to our current state
	float DeltaTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (Input))
	float HistorySamplingInterval = -1.f;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (Input))
	int32 NumHistorySamples = 30;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (Input))
	float PredictionSamplingInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (Input))
	int32 NumPredictionSamples = 15;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (Input, Output))
	FTransformTrajectory InOutTrajectory;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"
#include "Graph/RigUnit_AnimNextBase.h"
#include "Animation/TrajectoryTypes.h"

#include "RigUnit_TrajectoryAnalysis.generated.h"

USTRUCT(meta = (DisplayName = "Get Trajectory Sample At Time", Category = "Trajectory", NodeColor = "0, 1, 1", Keywords = "Trajectory"))
struct FRigUnit_GetTrajectorySampleAtTime : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Output))
	FTransformTrajectorySample OutTrajectorySample;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	FTransformTrajectory InTrajectory;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	float Time = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	bool bExtrapolate = false;
};

USTRUCT(meta = (DisplayName = "Get Trajectory Velocity", Category = "Trajectory", NodeColor = "0, 1, 1", Keywords = "Trajectory"))
struct FRigUnit_GetTrajectoryVelocity : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	FTransformTrajectory InTrajectory;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Output))
	FVector OutVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	float Time1 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	float Time2 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	bool bExtrapolate = false;
};

USTRUCT(meta = (DisplayName = "Get Trajectory Angular Velocity", Category = "Trajectory", NodeColor = "0, 1, 1", Keywords = "Trajectory"))
struct FRigUnit_GetTrajectoryAngularVelocity : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	FTransformTrajectory InTrajectory;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Output))
	FVector OutAngularVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	float Time1 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	float Time2 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory", meta = (Input))
	bool bExtrapolate = false;
};
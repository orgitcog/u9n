// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDSolverJointConstraintDataComponent.h"

#include "ChaosVDRecording.h"

void UChaosVDSolverJointConstraintDataComponent::UpdateFromNewSolverStageData(const FChaosVDSolverFrameData& InSolverFrameData, const FChaosVDFrameStageData& InSolverFrameStageData)
{
	if (EnumHasAnyFlags(InSolverFrameStageData.StageFlags, EChaosVDSolverStageFlags::ExplicitStage))
	{
		UpdateConstraintData(InSolverFrameStageData.RecordedJointConstraints);
	}
}

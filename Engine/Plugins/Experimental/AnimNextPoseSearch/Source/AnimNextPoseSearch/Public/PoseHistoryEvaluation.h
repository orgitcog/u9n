// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchHistory.h"
#include "EvaluationVM/EvaluationVM.h"

namespace UE::PoseSearch
{
	extern ANIMNEXTPOSESEARCH_API const UE::AnimNext::FEvaluationVMStackName POSEHISTORY_STACK_NAME;

	struct FPoseHistoryEvaluationHelper
	{
		TSharedPtr<UE::PoseSearch::FGenerateTrajectoryPoseHistory, ESPMode::ThreadSafe> PoseHistoryPtr = nullptr;
	};
}

namespace UE::AnimNext
{
	ANIM_NEXT_ENABLE_EVALUATION_STACK_USAGE(TUniquePtr<UE::PoseSearch::FPoseHistoryEvaluationHelper>);
}
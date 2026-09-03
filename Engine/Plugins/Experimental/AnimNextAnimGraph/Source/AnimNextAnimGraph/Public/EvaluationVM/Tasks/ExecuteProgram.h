// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include "ExecuteProgram.generated.h"

#define UE_API ANIMNEXTANIMGRAPH_API

namespace UE::AnimNext
{
	struct FEvaluationProgram;
}

/*
 * Execute Program Task
 *
 * This allows external caching of evaluation programs by deferring evaluation
 * or repeated evaluations.
 */
USTRUCT()
struct FAnimNextExecuteProgramTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextExecuteProgramTask)

	static UE_API FAnimNextExecuteProgramTask Make(TSharedPtr<const UE::AnimNext::FEvaluationProgram> Program);

	// Task entry point
	UE_API virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// The program to execute
	TSharedPtr<const UE::AnimNext::FEvaluationProgram> Program = nullptr;
};

#undef UE_API

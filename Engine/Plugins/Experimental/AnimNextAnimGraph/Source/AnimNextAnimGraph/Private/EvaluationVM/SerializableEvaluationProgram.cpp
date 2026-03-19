// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/SerializableEvaluationProgram.h"
#include "EvaluationVM/EvaluationProgram.h"

FSerializableEvaluationProgram::FSerializableEvaluationProgram(const UE::AnimNext::FEvaluationProgram& Other)
{
	for (const TSharedPtr<FAnimNextEvaluationTask>& Task : Other.Tasks)
	{
		Tasks.Add(FInstancedStruct(Task->GetStruct()));
		Task->GetStruct()->CopyScriptStruct(Tasks.Last().GetMutableMemory(), &*Task);
	}
}
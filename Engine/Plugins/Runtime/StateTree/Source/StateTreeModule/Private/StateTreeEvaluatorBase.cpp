// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEvaluatorBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeEvaluatorBase)

#if WITH_GAMEPLAY_DEBUGGER
FString FStateTreeEvaluatorBase::GetDebugInfo(const FStateTreeReadOnlyExecutionContext& Context) const
{
	TStringBuilder<256> Builder;
	Builder << TEXT('[');
	Builder << Name;
	Builder << TEXT("]\n");
	return Builder.ToString();
}

// Deprecated
void FStateTreeEvaluatorBase::AppendDebugInfoString(FString& DebugString, const FStateTreeExecutionContext& Context) const
{
	DebugString += FString::Printf(TEXT("[%s]\n"), *Name.ToString());
}
#endif // WITH_GAMEPLAY_DEBUGGER


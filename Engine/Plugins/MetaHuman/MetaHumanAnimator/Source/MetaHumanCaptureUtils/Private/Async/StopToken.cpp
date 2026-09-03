// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/StopToken.h"

FStopToken::FStopToken() :
	SharedState(MakeShared<FSharedState>())
{
}

void FStopToken::RequestStop()
{
	check(SharedState.IsValid());
	SharedState->State.store(true);
}

bool FStopToken::IsStopRequested() const
{
	check(SharedState.IsValid());
	return SharedState->State.load();
}
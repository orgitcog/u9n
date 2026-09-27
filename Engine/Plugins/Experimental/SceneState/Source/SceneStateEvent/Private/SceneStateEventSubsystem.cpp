// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateEventSubsystem.h"
#include "Engine/Engine.h"
#include "SceneStateEventStream.h"

USceneStateEventSubsystem* USceneStateEventSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<USceneStateEventSubsystem>() : nullptr;
}

void USceneStateEventSubsystem::RegisterEventStream(USceneStateEventStream* InEventStream)
{
	EventStreams.AddUnique(InEventStream);
}

void USceneStateEventSubsystem::UnregisterEventStream(USceneStateEventStream* InEventStream)
{
	EventStreams.Remove(InEventStream);
}

void USceneStateEventSubsystem::ForEachEventStream(TFunctionRef<void(USceneStateEventStream*)> InCallable)
{
	for (USceneStateEventStream* EventStream : EventStreams)
	{
		if (EventStream)
		{
			InCallable(EventStream);
		}
	}
}

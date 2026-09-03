// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Event.h"
#include "MetaHumanTakeData.h"

METAHUMAN_CAPTURE_DEFINE_EMPTY_EVENT(FTakeListResetEvent, "Take List Reset")

struct METAHUMANCAPTURESOURCE_API FNewTakesAddedEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("New Takes Added");

	TArray<TakeId> NewTakes;

	FNewTakesAddedEvent(TArray<TakeId> InNewTakes) : FCaptureEvent(Name), NewTakes(MoveTemp(InNewTakes))
	{
	}

	FNewTakesAddedEvent(TakeId InNewTake) : FCaptureEvent(Name), NewTakes({ InNewTake })
	{
	}
};

struct METAHUMANCAPTURESOURCE_API FTakesRemovedEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("Takes Removed");

	TArray<TakeId> TakesRemoved;

	FTakesRemovedEvent(TArray<TakeId> InTakesRemoved) : FCaptureEvent(Name), TakesRemoved(MoveTemp(InTakesRemoved))
	{
	}

	FTakesRemovedEvent(TakeId InTakeRemoved) : FCaptureEvent(Name), TakesRemoved({ InTakeRemoved })
	{
	}
};

struct METAHUMANCAPTURESOURCE_API FThumbnailChangedEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("Thumbnail Changed");

	TakeId ChangedTake;

	FThumbnailChangedEvent(TakeId InChangedTake) : FCaptureEvent(Name), ChangedTake(InChangedTake)
	{
	}
};

struct METAHUMANCAPTURESOURCE_API FConnectionChangedEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("Connection Changed");

	enum class EState
	{
		Unknown = 0,
		Connected,
		Disconnected
	};

	EState ConnectionState;

	FConnectionChangedEvent(EState InConnectionState) : FCaptureEvent(Name), ConnectionState(InConnectionState)
	{
	}
};

struct METAHUMANCAPTURESOURCE_API FRecordingStatusChangedEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("Recording Status Changed");

	bool bIsRecording;

	FRecordingStatusChangedEvent(bool bInIsRecording) : FCaptureEvent(Name), bIsRecording(bInIsRecording)
	{
	}
};

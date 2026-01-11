// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateEventSchemaHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "SceneStateEventTemplate.generated.h"

/** Defines a handle to an event schema and the payload event data to copy to events to push */
USTRUCT(BlueprintType)
struct FSceneStateEventTemplate
{
	GENERATED_BODY()

	const FSceneStateEventSchemaHandle& GetEventSchemaHandle() const
	{
		return EventSchemaHandle;
	}

	const FInstancedStruct& GetEventData() const
	{
		return EventData;
	}

#if WITH_EDITOR
	/** Syncs the event data to match the event schema struct */
	SCENESTATEEVENT_API void SyncEventData();
#endif

	static FName GetEventSchemaHandlePropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FSceneStateEventTemplate, EventSchemaHandle);
	}

	static FName GetEventDataPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FSceneStateEventTemplate, EventData);
	}

private:
	/** Handle to the event schema to use */
	UPROPERTY(EditAnywhere, Category="Event")
	FSceneStateEventSchemaHandle EventSchemaHandle;

	/** Data initialized to the event schema, if it has a valid event struct */
	UPROPERTY(EditAnywhere, Category="Event", meta=(StructTypeConst))
	FInstancedStruct EventData;
};

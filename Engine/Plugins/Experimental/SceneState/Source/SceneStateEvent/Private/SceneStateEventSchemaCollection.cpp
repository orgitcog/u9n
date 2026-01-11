// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateEventSchemaCollection.h"
#include "SceneStateEventSchema.h"

USceneStateEventSchemaObject* USceneStateEventSchemaCollection::GetEventSchema(const FGuid& InEventSchemaId) const
{
	if (!InEventSchemaId.IsValid())
	{
		return nullptr;
	}

	for (USceneStateEventSchemaObject* EventSchema : EventSchemas)
	{
		if (EventSchema && EventSchema->Id == InEventSchemaId)
		{
			return EventSchema;
		}
	}

	return nullptr;
}

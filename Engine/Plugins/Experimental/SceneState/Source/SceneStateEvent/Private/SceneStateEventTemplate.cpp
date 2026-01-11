// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateEventTemplate.h"
#include "StructUtils/UserDefinedStruct.h"

#if WITH_EDITOR
void FSceneStateEventTemplate::SyncEventData()
{
	const UUserDefinedStruct* EventStruct = EventSchemaHandle.GetEventStruct();
	if (EventData.GetScriptStruct() != EventStruct)
	{
		EventData.InitializeAs(EventStruct);
	}
}
#endif

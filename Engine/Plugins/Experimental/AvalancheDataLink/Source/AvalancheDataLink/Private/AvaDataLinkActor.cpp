// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaDataLinkActor.h"
#include "AvaDataLinkInstance.h"

void AAvaDataLinkActor::ExecuteDataLinkInstances()
{
	for (UAvaDataLinkInstance* DataLinkInstance: DataLinkInstances)
	{
		if (DataLinkInstance)
		{
			DataLinkInstance->Execute();
		}
	}
}

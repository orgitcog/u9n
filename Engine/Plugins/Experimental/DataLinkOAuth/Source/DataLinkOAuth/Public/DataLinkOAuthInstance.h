// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataLinkOAuthHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "DataLinkOAuthInstance.generated.h"

USTRUCT()
struct FDataLinkNodeOAuthInstance
{
	GENERATED_BODY()

	DATALINKOAUTH_API ~FDataLinkNodeOAuthInstance();

	void StopListening();

	FDataLinkOAuthHandle ListenHandle;

	UPROPERTY()
	int32 ListenPort = 0;

	UPROPERTY()
	FInstancedStruct SharedData;
};

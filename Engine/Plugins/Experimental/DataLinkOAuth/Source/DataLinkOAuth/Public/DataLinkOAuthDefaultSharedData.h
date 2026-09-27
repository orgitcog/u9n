// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "DataLinkOAuthDefaultSharedData.generated.h"

USTRUCT()
struct FDataLinkOAuthDefaultSharedData
{
	GENERATED_BODY()

	DATALINKOAUTH_API FDataLinkOAuthDefaultSharedData();

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	FString State;
};

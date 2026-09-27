// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "DataLinkOAuthToken.generated.h"

USTRUCT()
struct FDataLinkOAuthToken
{
	GENERATED_BODY()

	UPROPERTY()
	FString TokenType;

	UPROPERTY()
	FString AccessToken;

	UPROPERTY()
	FString RefreshToken;

	UPROPERTY()
	FDateTime ExpirationDate;
};

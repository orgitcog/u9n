// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptionalPropertyTestObject.generated.h"

#if WITH_TESTS

UCLASS()
class UOptionalPropertyTestObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TOptional<FString> OptionalString;

    UPROPERTY()
    TOptional<FText> OptionalText;

    UPROPERTY()
    TOptional<FName> OptionalName;

    UPROPERTY()
    TOptional<int32> OptionalInt;
};

#endif // WITH_TESTS

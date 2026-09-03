// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MyAutoRTFMTestObject.generated.h"

UCLASS()
class UMyAutoRTFMTestObject : public UObject
{
	GENERATED_BODY()

public:
	using FConstructorCallback = void(const FObjectInitializer&, UMyAutoRTFMTestObject&);

	UMyAutoRTFMTestObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int Value;

	static FConstructorCallback* ConstructorCallback;

	void DoNothing() const {}
};

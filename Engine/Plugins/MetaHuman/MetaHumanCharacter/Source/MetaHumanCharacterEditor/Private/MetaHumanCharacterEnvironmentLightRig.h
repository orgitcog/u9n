// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "MetaHumanCharacterEnvironmentLightRig.generated.h"

UINTERFACE(Blueprintable)
class UMetaHumanCharacterEnvironmentLightRig : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface implemented by actors that provide light rig functionality in
 * the lighting environments of the Character editor
 */
class IMetaHumanCharacterEnvironmentLightRig
{
	GENERATED_BODY()

public:

	/**
	 * Sets the rotation of the light rig
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Lighting")
	void SetRotation(float InRotation);
};

UINTERFACE(Blueprintable)
class UMetaHumanCharacterEnvironmentBackground : public UInterface
{
	GENERATED_BODY()
};

class IMetaHumanCharacterEnvironmentBackground
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Background")
	void SetBackgroundColor(const FLinearColor& BackgroundColor);
};
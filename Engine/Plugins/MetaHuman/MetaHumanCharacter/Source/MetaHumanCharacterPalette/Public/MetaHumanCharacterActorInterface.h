// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "MetaHumanCharacterActorInterface.generated.h"

class UMetaHumanCharacterInstance;

/**
 * Interface for actors that can be initialized from a UMetaHumanCharacterInstance.
 *
 * An actor implementing this interface can be used as a preview actor in the Character editor.
 */
UINTERFACE(BlueprintType)
class METAHUMANCHARACTERPALETTE_API UMetaHumanCharacterActorInterface : public UInterface
{
	GENERATED_BODY()
};

class METAHUMANCHARACTERPALETTE_API IMetaHumanCharacterActorInterface
{
	GENERATED_BODY()

public:
	/** Initializes the actor from the given Character Instance */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category = "Character")
	void SetCharacterInstance(UMetaHumanCharacterInstance* CharacterInstance);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStatePlayer.h"
#include "SceneStateComponentPlayer.generated.h"

class USceneStateComponent;

/** Scene State Player for Scene State Components */
UCLASS(MinimalAPI)
class USceneStateComponentPlayer : public USceneStatePlayer
{
	GENERATED_BODY()

protected:
	SCENESTATEGAMEPLAY_API AActor* GetActor() const;

	//~ Begin USceneStatePlayer
	SCENESTATEGAMEPLAY_API virtual bool OnGetContextName(FString& OutContextName) const override;
	SCENESTATEGAMEPLAY_API virtual bool OnGetContextObject(UObject*& OutContextObject) const override;
	//~ End USceneStatePlayer
};

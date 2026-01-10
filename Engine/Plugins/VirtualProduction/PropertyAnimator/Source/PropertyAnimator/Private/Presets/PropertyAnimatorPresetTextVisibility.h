// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorPresetVisibility.h"
#include "PropertyAnimatorPresetTextVisibility.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Preset for text character visibility on scene component
 */
UCLASS()
class UPropertyAnimatorPresetTextVisibility : public UPropertyAnimatorPresetVisibility
{
	GENERATED_BODY()

public:
	UPropertyAnimatorPresetTextVisibility()
	{
		PresetName = TEXT("TextCharacterVisibility");
	}

protected:
	//~ Begin UPropertyAnimatorCorePresetBase
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const override;
	//~ End UPropertyAnimatorCorePresetBase
};
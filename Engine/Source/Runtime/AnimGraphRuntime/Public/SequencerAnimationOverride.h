// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreTypes.h"
#include "HAL/Platform.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include "SequencerAnimationOverride.generated.h"

#define UE_API ANIMGRAPHRUNTIME_API

/**
 * Sequencer Animation Track Override interface.
 * Anim blueprints can override this to provide Sequencer with instructions on how to override this blueprint during Sequencer takeover.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USequencerAnimationOverride : public UInterface
{
	GENERATED_BODY()
};

class ISequencerAnimationOverride
{
	GENERATED_BODY()

public:

	// Whether this animation blueprint allows Sequencer to nuke this anim instance and replace it during Sequencer playback.
	UFUNCTION(BlueprintNativeEvent, Category = "Sequencer", meta = (CallInEditor = "true"))
	UE_API bool AllowsCinematicOverride() const;
	
	virtual bool AllowsCinematicOverride_Implementation() const { return false; }

	// Should return a list of valid slot names for Sequencer to output to in the case that Sequencer is not permitted to nuke the anim instance.
	// Will be chosen by the user in drop down on the skeletal animation section properties. Should be named descriptively, as in some contexts (UEFN), the user
	// will not be able to view the animation blueprint itself to determine the mixing behavior of the slot.
	UFUNCTION(BlueprintNativeEvent, Category = "Sequencer", meta = (CallInEditor = "true"))
	UE_API TArray<FName> GetSequencerAnimSlotNames() const;

	virtual	TArray<FName> GetSequencerAnimSlotName_Implementation() const { return TArray<FName>(); }

	static TScriptInterface<ISequencerAnimationOverride> GetSequencerAnimOverride(USkeletalMeshComponent* SkeletalMeshComponent)
	{
		if (TSubclassOf<UAnimInstance> AnimInstanceClass = SkeletalMeshComponent->GetAnimClass())
		{
			if (UAnimInstance* AnimInstance = AnimInstanceClass->GetDefaultObject<UAnimInstance>())
			{
				if (AnimInstance->Implements<USequencerAnimationOverride>())
				{
					TScriptInterface<ISequencerAnimationOverride> AnimOverride = AnimInstance;
					if (AnimOverride.GetObject())
					{
						return AnimOverride;
					}
				}
			}
		}
		return nullptr;
	}
};


#undef UE_API

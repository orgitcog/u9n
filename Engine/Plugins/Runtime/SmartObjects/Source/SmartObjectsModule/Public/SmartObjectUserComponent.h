// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "SmartObjectTypes.h"
#include "SmartObjectUserComponent.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

/**
 * Smart Object User Component defines common settings for a Smart Object user.
 *
 * The validation settings for entries and exits are separate to allow to have more lax exit settings.
 * For example the entry settings might prevent to use Smart Object slots which are on water, but we could allow to exit in water.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = Gameplay, meta = (BlueprintSpawnableComponent), config = Game, HideCategories = (Activation, AssetUserData, Collision, Cooking, HLOD, Lighting, LOD, Mobile, Mobility, Navigation, Physics, RayTracing, Rendering, Tags, TextureStreaming))
class USmartObjectUserComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API explicit USmartObjectUserComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** @return validation filter to be used for entries. */
	TSubclassOf<USmartObjectSlotValidationFilter> GetValidationFilter() const
	{
		return ValidationFilter;
	}

protected:

	/** Validation filter used for entering testing entries for a Smart Object slot. */
	UPROPERTY(EditAnywhere, Category = "SmartObject", BlueprintReadWrite)
	TSubclassOf<USmartObjectSlotValidationFilter> ValidationFilter = nullptr;
};

#undef UE_API

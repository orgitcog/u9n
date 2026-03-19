// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGGenSourceBase.h"

#include "PCGGenSourcePlayer.generated.h"

#define UE_API PCG_API

class APlayerController;

/**
 * A UPCGGenSourcePlayer is automatically captured for all PlayerControllers in the level on Login/Logout.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGenSourcePlayer : public UObject, public IPCGGenSourceBase
{
	GENERATED_BODY()

public:
	/** Update the generation source so that it can cache data that is queried often (e.g. view frustum). Should be called every tick on any active generation sources. */
	UE_API virtual void Tick() override;

	/** Returns the world space position of this gen source. */
	UE_API virtual TOptional<FVector> GetPosition() const override;

	/** Returns the normalized forward vector of this gen source. */
	UE_API virtual TOptional<FVector> GetDirection() const override;

	/** Returns the view frustum of this gen source. */
	virtual TOptional<FConvexVolume> GetViewFrustum(bool bIs2DGrid) const override { return ViewFrustum; }

	TWeakObjectPtr<const APlayerController> GetPlayerController() const { return PlayerController; }
	UE_API void SetPlayerController(const APlayerController* InPlayerController);

	bool IsValid() const { return PlayerController.IsValid(); }

protected:
	TWeakObjectPtr<const APlayerController> PlayerController;
	TOptional<FConvexVolume> ViewFrustum;
};

#undef UE_API

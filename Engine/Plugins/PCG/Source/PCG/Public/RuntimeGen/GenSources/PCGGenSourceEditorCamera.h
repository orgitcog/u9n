// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGGenSourceBase.h"

#include "PCGGenSourceEditorCamera.generated.h"

#define UE_API PCG_API

#if WITH_EDITOR
class FEditorViewportClient;
#endif

/**
 * This GenerationSource captures active Editor Viewports per tick to provoke RuntimeGeneration. Editor Viewports
 * are not captured by default, but can be enabled on the PCGWorldActor via bTreatEditorViewportAsGenerationSource.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGenSourceEditorCamera : public UObject, public IPCGGenSourceBase
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
	UE_API virtual TOptional<FConvexVolume> GetViewFrustum(bool bIs2DGrid) const override;

public:
#if WITH_EDITORONLY_DATA
	FEditorViewportClient* EditorViewportClient = nullptr;
	TOptional<FConvexVolume> ViewFrustum;
#endif
};

#undef UE_API

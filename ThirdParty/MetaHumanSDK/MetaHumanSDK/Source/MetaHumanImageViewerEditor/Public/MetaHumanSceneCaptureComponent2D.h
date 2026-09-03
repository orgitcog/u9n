// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneCaptureComponent2D.h"
#include "MetaHumanSceneCaptureComponent2D.generated.h"

class FEditorViewportClient;

UCLASS()
class METAHUMANIMAGEVIEWEREDITOR_API UMetaHumanSceneCaptureComponent2D : public USceneCaptureComponent2D
{
public:
	GENERATED_BODY()

	UMetaHumanSceneCaptureComponent2D(const FObjectInitializer& InObjectInitializer);

	//~ USceneCaptureComponent2D interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	// Sets the viewport client that controls this component
	void SetViewportClient(TWeakPtr<class FEditorViewportClient> InPerformerViewportClient);

	/** Set the ShowFlags for this component based a view mode index */
	void SetViewMode(EViewModeIndex InViewMode);

	void InvalidateCache();

private:

	// A reference to the viewport client that controls this component
	TWeakPtr<FEditorViewportClient> ViewportClientRef;

	static constexpr int32 NumTicksAfterCacheInvalidation = 2;
	int32 CurrentNumTicksAfterCacheInvalidation = 0;
	float CachedFOVAngle = -1;
	float CachedCustomNearClippingPlane = -1;
	FRotator CachedViewRotation = FRotator(0, 0, 0);
	FVector CachedViewLocation = FVector(0, 0, 0);
};

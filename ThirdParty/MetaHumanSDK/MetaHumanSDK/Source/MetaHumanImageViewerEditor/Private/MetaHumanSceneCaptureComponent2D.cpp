// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSceneCaptureComponent2D.h"
#include "EditorViewportClient.h"
#include "Settings/LevelEditorViewportSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanSceneCaptureComponent2D)

namespace
{
	TAutoConsoleVariable<bool> CVarEnableSceneCaptureCache
	{
		TEXT("mh.ImageViewer.EnableSceneCaptureCache"),
		false,
		TEXT("Enable caching for Scene Capture Components used in the AB viewports"),
		ECVF_Default
	};
}

UMetaHumanSceneCaptureComponent2D::UMetaHumanSceneCaptureComponent2D(const FObjectInitializer& InObjectInitializer) : Super(InObjectInitializer)
{
	CaptureMesh = nullptr;
}

void UMetaHumanSceneCaptureComponent2D::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	check(ViewportClientRef.IsValid());

	TSharedPtr<FEditorViewportClient> ViewportClient = ViewportClientRef.Pin();

	// Need to disable orbit camera before getting or setting actor position so that the viewport camera location is converted back
	// The view transform need to be saved restored so we don't have glitches when orbiting at a 90 degree angle
	// This happens as this component will tick so if the user is orbiting, need to keep orbiting enabled
	const FViewportCameraTransform SavedViewTransform = ViewportClient->GetViewTransform();
	const bool bIsOrbiting = ViewportClient->bUsingOrbitCamera;

	if (bIsOrbiting)
	{
		ViewportClient->ToggleOrbitCamera(false);
	}

	float CurrentFOVAngle = ViewportClient->ViewFOV;
	float CurrentCustomNearClippingPlane = ViewportClient->GetNearClipPlane();
	FRotator CurrentViewRotation = ViewportClient->GetViewRotation();
	FVector CurrentViewLocation = ViewportClient->GetViewLocation();

	if (bIsOrbiting)
	{
		ViewportClient->ToggleOrbitCamera(true);
	}

	ViewportClient->GetViewTransform() = SavedViewTransform;

	const bool bUseCaching = CVarEnableSceneCaptureCache.GetValueOnAnyThread();
	const bool bShouldTick = CurrentNumTicksAfterCacheInvalidation <= NumTicksAfterCacheInvalidation;

	if (bShouldTick || !bUseCaching || CurrentFOVAngle != CachedFOVAngle || CurrentCustomNearClippingPlane != CachedCustomNearClippingPlane || CurrentViewRotation != CachedViewRotation || CurrentViewLocation != CachedViewLocation)
	{
		CurrentNumTicksAfterCacheInvalidation++;

		CachedFOVAngle = CurrentFOVAngle;
		CachedCustomNearClippingPlane = CurrentCustomNearClippingPlane;
		CachedViewRotation = CurrentViewRotation;
		CachedViewLocation = CurrentViewLocation;

		FOVAngle = CurrentFOVAngle;
		bOverride_CustomNearClippingPlane = true;
		CustomNearClippingPlane = CurrentCustomNearClippingPlane;

		SetWorldTransform(FTransform{ CurrentViewRotation, CurrentViewLocation });

		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}
}

void UMetaHumanSceneCaptureComponent2D::SetViewportClient(TWeakPtr<FEditorViewportClient> InViewportClient)
{
	ViewportClientRef = InViewportClient;
}

void UMetaHumanSceneCaptureComponent2D::SetViewMode(EViewModeIndex InViewMode)
{
	// Scene capture component does not support other modes
	ensure(InViewMode == EViewModeIndex::VMI_Lit || InViewMode == EViewModeIndex::VMI_Unlit || InViewMode == EViewModeIndex::VMI_LightingOnly);

	// Recreate the ShowFlags for the scene capture component to avoid getting in a state where flags are not reset properly
	ShowFlags = FEngineShowFlags{ ESFIM_Editor };
	ShowFlags.SetSelectionOutline(GetDefault<ULevelEditorViewportSettings>()->bUseSelectionOutline);
	ShowFlags.SetAntiAliasing(false);

	constexpr bool bCanDisableToneMapping = false;
	EngineShowFlagOverride(ESFIM_Editor, InViewMode, ShowFlags, bCanDisableToneMapping);
}

void UMetaHumanSceneCaptureComponent2D::InvalidateCache()
{
	CachedFOVAngle = -1;
	CurrentNumTicksAfterCacheInvalidation = 0;
}


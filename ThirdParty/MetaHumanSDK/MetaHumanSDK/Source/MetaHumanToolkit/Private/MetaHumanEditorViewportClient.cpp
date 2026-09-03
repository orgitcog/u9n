// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanEditorViewportClient.h"

#include "MetaHumanViewportSettings.h"
#include "MetaHumanFootageComponent.h"
#include "SMetaHumanEditorViewport.h"
#include "MetaHumanSceneCaptureComponent2D.h"
#include "MetaHumanDepthMeshComponent.h"

#include "CameraController.h"
#include "EngineUtils.h"
#include "RenderUtils.h"
#include "ScopedTransaction.h"
#include "UnrealEdGlobals.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Editor/UnrealEdEngine.h"
#include "Algo/AllOf.h"
#include "AdvancedPreviewScene.h"

#define LOCTEXT_NAMESPACE "MetaHumanEditorViewportClient"

FMetaHumanEditorViewportClient::FMetaHumanEditorViewportClient(FPreviewScene* InPreviewScene, UMetaHumanViewportSettings* InViewportSettings)
	: FEditorViewportClient{ nullptr, InPreviewScene }
{
	OverrideNearClipPlane(0.1f);
	WidgetMode = UE::Widget::WM_None;
	bIsManipulating = false;

	AddRealtimeOverride(true, LOCTEXT("RealtimeOverrideMessage_MetaHumanViewport", "MetaHuman"));
	SetRealtime(true);

	EngineShowFlags.SetSelectionOutline(GetDefault<ULevelEditorViewportSettings>()->bUseSelectionOutline);
	EngineShowFlags.SetAntiAliasing(false);

	ViewportSettings = InViewportSettings != nullptr ? InViewportSettings : NewObject<UMetaHumanViewportSettings>();

	ABSceneCaptureComponents.Add(EABImageViewMode::A);
	ABSceneCaptureComponents.Add(EABImageViewMode::B);

	SetViewLocation(ViewportSettings->CameraState.Location);
	SetViewRotation(ViewportSettings->CameraState.Rotation);
	SetLookAtLocation(ViewportSettings->CameraState.LookAt);
	ViewFOV = ViewportSettings->CameraState.ViewFOV;

	CameraSpeedSetting = ViewportSettings->CameraState.SpeedSetting;
	CameraSpeedScalar = ViewportSettings->CameraState.SpeedScalar;

	DepthDataFootage.SetNear(ViewportSettings->DepthNear);
	DepthDataFootage.SetFar(ViewportSettings->DepthFar);

	ExposureSettings.bFixed = true;
	ExposureSettings.FixedEV100 = InViewportSettings->GetEV100(EABImageViewMode::Current);

	PostProcessComponent = NewObject<UPostProcessComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	PostProcessComponent->Settings = GetDefaultPostProcessSettings();

	// The scene takes ownership of the component
	PreviewScene->AddComponent(PostProcessComponent, FTransform::Identity);

	EngineShowFlags.DisableAdvancedFeatures();

	// Binds default lambdas to the delegates to the class can be used on its own. These can be overridden after construction
	OnGetAllPrimitiveComponentsDelegate.BindLambda([]
	{
		return TArray<UPrimitiveComponent*>{};
	});

	OnGetPrimitiveComponentInstanceDelegate.BindLambda([](UPrimitiveComponent* InComponent)
	{
		return InComponent;
	});
}

FMetaHumanEditorViewportClient::~FMetaHumanEditorViewportClient() = default;

void FMetaHumanEditorViewportClient::Tick(float InDeltaSeconds)
{
	if (!IsNavigationLocked())
	{
		// Don't tick the parent class due to UE-181656
		// This will process the keyboard events and update the camera regardless of navigation being locked or not
		FEditorViewportClient::Tick(InDeltaSeconds);
	}

	if (!GIntraFrameDebuggingGameThread && GetPreviewScene() != nullptr)
	{
		// Tick the scene so the scene capture components in the scene can update
		// when moving the camera in AB dual or wipe modes and in 2D navigation
		GetPreviewScene()->GetWorld()->Tick(LEVELTICK_ViewportsOnly, InDeltaSeconds);
	}
}

void FMetaHumanEditorViewportClient::ProcessClick(FSceneView& InView, HHitProxy* InHitProxy, FKey InKey, EInputEvent InEvent, uint32 InHitX, uint32 InHitY)
{
	const FViewportClick Click(&InView, this, InKey, InEvent, InHitX, InHitY);

	if (InHitProxy != nullptr)
	{
		if (HActor* ActorProxy = HitProxyCast<HActor>(InHitProxy))
		{
			if (ActorProxy->PrimComponent)
			{
				OnPrimitiveComponentClickedDelegate.ExecuteIfBound(ActorProxy->PrimComponent);
			}
		}
		check(InHitProxy->GetRefCount() > 0);
	}

	FEditorViewportClient::ProcessClick(InView, InHitProxy, InKey, InEvent, InHitX, InHitY);
}

UE::Widget::EWidgetMode FMetaHumanEditorViewportClient::GetWidgetMode() const
{
	return GetSelectedPrimitiveComponents().IsEmpty() ? UE::Widget::WM_None : WidgetMode;
}

void FMetaHumanEditorViewportClient::SetWidgetMode(UE::Widget::EWidgetMode InWidgetMode)
{
	WidgetMode = InWidgetMode;

	const TArray<UPrimitiveComponent*> SelectedComponents = GetSelectedPrimitiveComponents();

	if (WidgetMode == UE::Widget::WM_Scale && !SelectedComponents.IsEmpty())
	{
		InitialPivotLocation = GetComponentsBoundingBox(SelectedComponents).GetCenter();
	}
}

FVector FMetaHumanEditorViewportClient::GetWidgetLocation() const
{
	FVector Location = FVector::ZeroVector;

	if (WidgetMode == UE::Widget::WM_Scale)
	{
		Location = InitialPivotLocation;
	}
	else
	{
		const TArray<UPrimitiveComponent*> SelectedComponents = GetSelectedPrimitiveComponents();
		Location = GetComponentsBoundingBox(SelectedComponents).GetCenter();
	}

	return Location;
}

void FMetaHumanEditorViewportClient::TrackingStarted(const FInputEventState& InInputState, bool bInIsDraggingWidget, bool bInNudge)
{
	if (!bIsManipulating && bInIsDraggingWidget)
	{
		// Prevent the editor from emitting notifications for each delta change in when manipulating components using gizmos.
		// This avoid recording intermediate steps reducing overhead of the undo system
		GEditor->DisableDeltaModification(true);

		const TArray<UPrimitiveComponent*> SelectedComponents = GetSelectedPrimitiveComponents();

		if (!ScopedTransaction.IsValid() && !SelectedComponents.IsEmpty())
		{
			ScopedTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("MoveMultipleIdentityComponents", "Modify Multiple"));
		}

		for (UPrimitiveComponent* SelectedComponentPtr : SelectedComponents)
		{
			TWeakObjectPtr<UPrimitiveComponent> SelectedComponent(SelectedComponentPtr);
			if (SelectedComponent.IsValid())
			{
				SelectedComponent->Modify();
			}
		}

		bIsManipulating = true;
	}
}

void FMetaHumanEditorViewportClient::TrackingStopped()
{
	if (bIsManipulating)
	{
		// resetting the scoped transaction will call its destructor, thus, registering the transaction in the undo history
		ScopedTransaction.Reset();

		// Restore delta notifications
		GEditor->DisableDeltaModification(false);

		// Reset the initial pivot location in case we were scaling
		const TArray<UPrimitiveComponent*> SelectedComponents = GetSelectedPrimitiveComponents();
		InitialPivotLocation = GetComponentsBoundingBox(SelectedComponents).GetCenter();

		bIsManipulating = false;
	}
}

bool FMetaHumanEditorViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type InCurrentAxis, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	bool bHandled = false;

	const TArray<UPrimitiveComponent*> SelectedComponents = GetSelectedPrimitiveComponents();
	TArray<UPrimitiveComponent*> SelectedComponentInstances;
	Algo::Transform(SelectedComponents, SelectedComponentInstances, [this](UPrimitiveComponent* InComponent)
	{
		return OnGetPrimitiveComponentInstanceDelegate.IsBound() ? OnGetPrimitiveComponentInstanceDelegate.Execute(InComponent) : nullptr;
	});

	if (bIsManipulating && InCurrentAxis != EAxisList::None && !SelectedComponents.IsEmpty())
	{
		// If the scale is being changed we keep the pivot in its original location
		const FVector PivotLocation = WidgetMode == UE::Widget::WM_Scale ? InitialPivotLocation : GetComponentsBoundingBox(SelectedComponents).GetCenter();

		for (int32 ComponentIndex = 0; ComponentIndex < SelectedComponents.Num(); ++ComponentIndex)
		{
			UPrimitiveComponent* SceneComponent = SelectedComponents[ComponentIndex];
			UPrimitiveComponent* SceneComponentInstance = SelectedComponentInstances[ComponentIndex];

			// This takes into account parent components, if any
			FComponentEditorUtils::AdjustComponentDelta(SceneComponent, InDrag, InRot);

			// Finally we change the component transform
			const bool bDelta = true;
			GUnrealEd->ApplyDeltaToComponent(SceneComponent,
											 bDelta,
											 &InDrag,
											 &InRot,
											 &InScale,
											 PivotLocation);

			if (SceneComponentInstance != nullptr)
			{
				SceneComponentInstance->SetWorldTransform(SceneComponent->GetComponentTransform());
			}

			SceneComponent->TransformUpdated.Broadcast(SceneComponent, EUpdateTransformFlags::None, ETeleportType::None);
		}

		bHandled = true;
	}

	return bHandled;
}

void FMetaHumanEditorViewportClient::PerspectiveCameraMoved()
{
	bIsCameraMoving = true;

	OnCameraMovedDelegate.Broadcast();

	StoreCameraStateInViewportSettings();
}

void FMetaHumanEditorViewportClient::EndCameraMovement()
{
	if (bIsCameraMoving && !bIsTracking)
	{
		OnCameraStoppedDelegate.Broadcast();

		bIsCameraMoving = false;
	}
}

void FMetaHumanEditorViewportClient::UpdateMouseDelta()
{
	if (!IsNavigationLocked())
	{
		FEditorViewportClient::UpdateMouseDelta();
	}
}

void FMetaHumanEditorViewportClient::SetCameraSpeedSetting(int32 InSpeedSetting)
{
	FEditorViewportClient::SetCameraSpeedSetting(InSpeedSetting);
	ViewportSettings->CameraState.SpeedSetting = InSpeedSetting;
}

void FMetaHumanEditorViewportClient::SetCameraSpeedScalar(float InSpeedScalar)
{
	FEditorViewportClient::SetCameraSpeedScalar(InSpeedScalar);
	ViewportSettings->CameraState.SpeedScalar = InSpeedScalar;
}

void FMetaHumanEditorViewportClient::AddReferencedObjects(FReferenceCollector& InCollector)
{
	FEditorViewportClient::AddReferencedObjects(InCollector);

	if (ViewportSettings != nullptr)
	{
		InCollector.AddReferencedObject(ViewportSettings);
	}
}

FString FMetaHumanEditorViewportClient::GetReferencerName() const
{
	return TEXT("FMetaHumanEditorViewportClient");
}

void FMetaHumanEditorViewportClient::FocusViewportOnSelection()
{
	const TArray<UPrimitiveComponent*> SelectedComponents = GetSelectedPrimitiveComponents();
	const FBox BoundingBox = GetComponentsBoundingBox(SelectedComponents);
	FocusViewportOnBox(BoundingBox);
}

void FMetaHumanEditorViewportClient::UpdateABVisibility(bool bInSetViewpoint)
{
	if (!EditorViewportWidget.IsValid())
	{
		return;
	}

	// Retrieve visibility info for all components
	TArray<UPrimitiveComponent*> AllComponents;
	TMap<EABImageViewMode, TArray<UPrimitiveComponent*>> HiddenComponentsForView;
	GetAllComponentsAndComponentsHiddenForView(AllComponents, HiddenComponentsForView);
	const bool bIsAnyFootageComponentVisible = IsAnyFootageComponentVisible(AllComponents, HiddenComponentsForView);

	UpdateCameraViewportFromFootage(AllComponents, bIsAnyFootageComponentVisible, bInSetViewpoint);

	// Hide the environment if we have a footage component visible in the scene
	constexpr bool bDirect = true;
	FAdvancedPreviewScene* AdvancedPreviewScene = static_cast<FAdvancedPreviewScene*>(GetPreviewScene());
	AdvancedPreviewScene->SetEnvironmentVisibility(!bIsAnyFootageComponentVisible, bDirect);
	AdvancedPreviewScene->SetFloorVisibility(!bIsAnyFootageComponentVisible, bDirect);

	// Invalidates the cache of the scene capture components and clear its hidden components so we can update what is visible below
	constexpr bool bClearHiddenComponents = true;
	UpdateSceneCaptureComponents(bClearHiddenComponents);

	constexpr bool bPropagateToChildren = true;

	// Set the visibility of all components and show then as necessary
	for (UPrimitiveComponent* Component : AllComponents)
	{
		if (OnGetPrimitiveComponentInstanceDelegate.IsBound())
		{
			if (UPrimitiveComponent* ComponentInstance = OnGetPrimitiveComponentInstanceDelegate.Execute(Component))
			{
				ComponentInstance->SetVisibility(true, bPropagateToChildren);
			}
		}

		Component->SetVisibility(true, bPropagateToChildren);
	}

	bool bDepthMapIsVisible = false;
	if (IsShowingSingleView())
	{
		// In single view mode set the visibility directly in the components

		const EABImageViewMode CurrentABViewMode = GetABViewMode();
		const EABImageViewMode OppositeABViewMode = CurrentABViewMode == EABImageViewMode::A ? EABImageViewMode::B : EABImageViewMode::A;
		bDepthMapIsVisible = IsDepthMeshVisible(CurrentABViewMode);

		SetViewMode(ViewportSettings->GetViewModeIndex(CurrentABViewMode));
		ExposureSettings.FixedEV100 = ViewportSettings->GetEV100(CurrentABViewMode);

		if (GetTrackerImageViewer()->IsTextureView())
		{
			UMetaHumanSceneCaptureComponent2D* SceneCaptureComponent = ABSceneCaptureComponents[CurrentABViewMode];
			// If Default Luminance Range is disabled the system expects EV setting in luminance
			if (!UMetaHumanViewportSettings::IsExtendDefaultLuminanceRangeEnabled())
			{
				ExposureSettings.FixedEV100 = EV100ToLuminance(ExposureSettings.FixedEV100);
			}

			// When in texture view, update the view mode of the scene capture component as well
			// as texture view uses the scene capture component for display
			SceneCaptureComponent->SetViewMode(ViewportSettings->GetViewModeIndex(CurrentABViewMode));

			SceneCaptureComponent->PostProcessSettings.AutoExposureMinBrightness = ExposureSettings.FixedEV100;
			SceneCaptureComponent->PostProcessSettings.AutoExposureMaxBrightness = ExposureSettings.FixedEV100;
		}

		const TArray<UPrimitiveComponent*>& HiddenComponents = HiddenComponentsForView[CurrentABViewMode];
		for (UPrimitiveComponent* Component : HiddenComponents)
		{
			if (OnGetPrimitiveComponentInstanceDelegate.IsBound())
			{
				if (UPrimitiveComponent* InstanceComponent = OnGetPrimitiveComponentInstanceDelegate.Execute(Component))
				{
					InstanceComponent->SetVisibility(false, bPropagateToChildren);
				}
			}

			Component->SetVisibility(false, bPropagateToChildren);
		}

		for (UPrimitiveComponent* Component : AllComponents)
		{
			// When in single view mode we need to hide the footage component from the opposite view because
			// the user might be displaying on one side and depth on the other
			if (UMetaHumanFootageComponent* FootageComponent = Cast<UMetaHumanFootageComponent>(Component))
			{
				const bool bIsVisibleInCurrentView = !HiddenComponents.Contains(FootageComponent);

				if (OnGetPrimitiveComponentInstanceDelegate.IsBound())
				{
					if (UMetaHumanFootageComponent* FootageComponentInstance = Cast<UMetaHumanFootageComponent>(OnGetPrimitiveComponentInstanceDelegate.Execute(FootageComponent)))
					{
						FootageComponentInstance->SetFootageVisible(CurrentABViewMode, bIsVisibleInCurrentView);
						FootageComponentInstance->SetFootageVisible(OppositeABViewMode, false);
					}
				}

				FootageComponent->SetFootageVisible(CurrentABViewMode, bIsVisibleInCurrentView);
				FootageComponent->SetFootageVisible(OppositeABViewMode, false);
			}
		}
	}
	else
	{
		bDepthMapIsVisible = IsDepthMeshVisible(EABImageViewMode::A) || IsDepthMeshVisible(EABImageViewMode::B);

		for (const TPair<EABImageViewMode, TObjectPtr<UMetaHumanSceneCaptureComponent2D>>& ViewStatePair : ABSceneCaptureComponents)
		{
			const EABImageViewMode CurrentABViewMode = ViewStatePair.Key;
			UMetaHumanSceneCaptureComponent2D* SceneCaptureComponent = ViewStatePair.Value;

			// Apply the ViewMode from the ViewportClient to make sure the capture is consistent with what is in the view
			const EViewModeIndex ViewMode = GetViewModeIndexForABViewMode(CurrentABViewMode);
			SceneCaptureComponent->SetViewMode(ViewMode);

			// In multiview we need to hide the components in the scene capture components
			SceneCaptureComponent->ClearHiddenComponents();

			if (OnGetPrimitiveComponentInstanceDelegate.IsBound())
			{
				const TArray<UPrimitiveComponent*>& HiddenComponents = HiddenComponentsForView[CurrentABViewMode];
				for (UPrimitiveComponent* Component : HiddenComponents)
				{
					if (UPrimitiveComponent* ComponentInstance = OnGetPrimitiveComponentInstanceDelegate.Execute(Component))
					{
						SceneCaptureComponent->HideComponent(ComponentInstance);

						constexpr bool bIncludeAllDescendants = true;
						TArray<USceneComponent*> ChildComponents;
						ComponentInstance->GetChildrenComponents(bIncludeAllDescendants, ChildComponents);

						for (USceneComponent* ChildComponent : ChildComponents)
						{
							if (UPrimitiveComponent* ChildPrimitiveComponent = Cast<UPrimitiveComponent>(ChildComponent))
							{
								SceneCaptureComponent->HideComponent(ChildPrimitiveComponent);
							}
						}
					}
					else
					{
						SceneCaptureComponent->HideComponent(Component);

						constexpr bool bIncludeAllDescendants = true;
						TArray<USceneComponent*> ChildComponents;
						Component->GetChildrenComponents(bIncludeAllDescendants, ChildComponents);

						for (USceneComponent* ChildComponent : ChildComponents)
						{
							if (UPrimitiveComponent* ChildPrimitiveComponent = Cast<UPrimitiveComponent>(ChildComponent))
							{
								SceneCaptureComponent->HideComponent(ChildPrimitiveComponent);
							}
						}
					}
				}

				// The footage component currently needs some special treatment because of its ability to display color on one side and depth in the other
				// This requires the individual plane components to be hidden in the respective views
				for (UPrimitiveComponent* Component : AllComponents)
				{
					if (UMetaHumanFootageComponent* FootageSceneComponent = Cast<UMetaHumanFootageComponent>(Component))
					{
						if (UMetaHumanFootageComponent* FootageComponentInstance = Cast<UMetaHumanFootageComponent>(OnGetPrimitiveComponentInstanceDelegate.Execute(FootageSceneComponent)))
						{
							const bool bIsVisibleInCurrentView = !HiddenComponents.Contains(FootageSceneComponent);

							FootageComponentInstance->SetFootageVisible(CurrentABViewMode, bIsVisibleInCurrentView);

							if (!bIsVisibleInCurrentView)
							{
								SceneCaptureComponent->HideComponent(FootageComponentInstance->GetFootagePlaneComponent(CurrentABViewMode));
							}

							// Hide the plane component of the opposite view in this view scene capture component
							const EABImageViewMode OppositeABViewMode = CurrentABViewMode == EABImageViewMode::A ? EABImageViewMode::B : EABImageViewMode::A;
							SceneCaptureComponent->HideComponent(FootageComponentInstance->GetFootagePlaneComponent(OppositeABViewMode));
						}
					}
				}
			}
		}
	}

	// broadcast the visibility of the depth mesh 
	OnUpdateDepthMapVisibilityDelegate.ExecuteIfBound(bDepthMapIsVisible);
	Invalidate();
}

bool FMetaHumanEditorViewportClient::CanChangeEV100(EABImageViewMode InViewMode) const
{
	return true;
}

bool FMetaHumanEditorViewportClient::CanChangeViewMode(EABImageViewMode InViewMode) const
{
	return true;
}

UMetaHumanFootageComponent* FMetaHumanEditorViewportClient::GetActiveFootageComponent(const TArray<UPrimitiveComponent*>& InAllComponents) const
{
	UMetaHumanFootageComponent* FootageComponent = nullptr;

	InAllComponents.FindItemByClass<UMetaHumanFootageComponent>(&FootageComponent);

	return FootageComponent;
}

bool FMetaHumanEditorViewportClient::GetSetViewpoint() const
{
	return true;
}

void FMetaHumanEditorViewportClient::UpdateCameraViewportFromFootage(const TArray<UPrimitiveComponent*>& InAllComponents, bool bInIsAnyFootageComponentVisible, bool bInSetViewpoint)
{
	TSharedRef<STrackerImageViewer> ABImage = GetTrackerImageViewer();

	UMetaHumanFootageComponent* FootageComponent = GetActiveFootageComponent(InAllComponents);
	FBox2D FootageScreenRect = {};
	FTransform CameraTransform = FTransform::Identity;

	if (FootageComponent && bInIsAnyFootageComponentVisible)
	{
		const FVector2D WidgetSize = GetTrackerImageViewer()->GetCachedGeometry().GetLocalSize();
		// The following function updates the camera FOV based on the UI widget size and the footage image dimensions, 
		// while computing the desired screen size to display the footage image at the same time.
		FootageComponent->GetFootageScreenRect(WidgetSize, ViewFOV, FootageScreenRect, CameraTransform);
	}

	// This logic for when to set the viewpoint is not great!
	// Firstly, if there is footage visible then we will need to set the viewpoint so footage and geom is aligned.
	// But if footage is not displayed, we may need to set the viewpoint anyway, eg you have switched cameras or footage.
	// GetSetViewpoint defines a "global" default behaviour for this case, which is to set the viewpoint when
	// using an identity based on footage but not set the viewpoint when using an identity based on mesh.
	// But then there is a further, local, way of modifying this since some updates, like switching what is visible
	// in the viewport or lighting settings dont require a viewpoint update but others do (like changing camera). 
	if (bInIsAnyFootageComponentVisible || (GetSetViewpoint() && bInSetViewpoint))
	{
		// Set camera transform
		SetViewRotation(CameraTransform.GetRotation().Rotator());
		SetViewLocation(CameraTransform.GetLocation());
		StoreCameraStateInViewportSettings();
	}

	// Check if any footage component is currently visible, then set the navigation mode to 2D as there is an image plane visible
	if (bInIsAnyFootageComponentVisible)
	{
		// If there is a footage component visible in the viewport, lock the navigation
		ABImage->SetNavigationMode(EABImageNavigationMode::TwoD);

		// To best fit the footage into a dual view, we may have to zoom the image
		if (FootageScreenRect.GetSize().X > 0 && ABImage->GetViewMode() == EABImageViewMode::ABSide)
		{
			ABImage->AdjustZoomForFootageInDualView(FootageScreenRect.GetSize().Y / FootageScreenRect.GetSize().X);
		}

		// Also set the footage screen rect in the tracker image viewer to it can position
		if (FootageComponent)
		{
			GetTrackerImageViewer()->ResetTrackerImageScreenRect(FootageScreenRect);
		}
	}
	else
	{
		// Reset the tracker image rect to be the whole screen the viewport occupies
		GetTrackerImageViewer()->ResetTrackerImageScreenRect();

		// If there isn't a footage component visible, ask if the navigation should be unlocked
		if (OnShouldUnlockNavigationDelegate.IsBound())
		{
			if (OnShouldUnlockNavigationDelegate.Execute())
			{
				ABImage->SetNavigationMode(EABImageNavigationMode::ThreeD);
			}
		}
		else
		{
			// If OnShouldUnlockNavigationDelegate is not bound, default to 3D navigation mode if no footage component is visible
			ABImage->SetNavigationMode(EABImageNavigationMode::ThreeD);
		}
	}
}

void FMetaHumanEditorViewportClient::UpdateSceneCaptureComponents(bool bInClearHiddenComponents)
{
	if (!EditorViewportWidget.IsValid())
	{
		return;
	}

	const bool bIsTextureView = GetTrackerImageViewer()->IsTextureView();

	for (const TPair<EABImageViewMode, TObjectPtr<UMetaHumanSceneCaptureComponent2D>>& ViewStatePair : ABSceneCaptureComponents)
	{
		UMetaHumanSceneCaptureComponent2D* SceneCaptureComponent = ViewStatePair.Value;
		SceneCaptureComponent->bCaptureEveryFrame = bIsTextureView;
		SceneCaptureComponent->bCaptureOnMovement = bIsTextureView;
		SceneCaptureComponent->InvalidateCache();

		if (bInClearHiddenComponents)
		{
			SceneCaptureComponent->ClearHiddenComponents();
		}
	}
}

void FMetaHumanEditorViewportClient::SetEditorViewportWidget(TSharedRef<SMetaHumanEditorViewport> InEditorViewportWidget)
{
	EditorViewportWidget = InEditorViewportWidget;

	// Setup the scene capture components. By the time this function gets called we have the ABImage widget is created and initialized
	for (TPair<EABImageViewMode, TObjectPtr<UMetaHumanSceneCaptureComponent2D>>& ViewStatePair : ABSceneCaptureComponents)
	{
		const EABImageViewMode ViewMode = ViewStatePair.Key;
		TObjectPtr<UMetaHumanSceneCaptureComponent2D>& SceneCaptureComponent = ViewStatePair.Value;

		const FString ViewModeName = StaticEnum<EABImageViewMode>()->GetNameStringByValue(static_cast<int64>(ViewMode));

		SceneCaptureComponent = NewObject<UMetaHumanSceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
		SceneCaptureComponent->SetViewportClient(SharedThis(this));
		SceneCaptureComponent->TextureTarget = GetTrackerImageViewer()->GetRenderTarget(ViewMode);
		SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
		SceneCaptureComponent->bCaptureEveryFrame = false;
		SceneCaptureComponent->bAlwaysPersistRenderingState = true;
		SceneCaptureComponent->PostProcessSettings = GetDefaultPostProcessSettings();

		// Finally add the component to the preview scene
		GetPreviewScene()->AddComponent(SceneCaptureComponent, FTransform::Identity);
	}

	ViewportSettings->OnSettingsChangedDelegate.AddSP(this, &FMetaHumanEditorViewportClient::UpdateABVisibility, false);

	// Restore the view mode stored in the settings object
	GetTrackerImageViewer()->SetViewMode(ViewportSettings->CurrentViewMode);

	UpdateABVisibility();
}

bool FMetaHumanEditorViewportClient::IsViewModeIndexEnabled(EABImageViewMode InViewMode, EViewModeIndex InViewModeIndex, bool) const
{
	return ViewportSettings->GetViewModeIndex(InViewMode) == InViewModeIndex;
}

void FMetaHumanEditorViewportClient::SetViewModeIndex(EABImageViewMode InViewMode, EViewModeIndex InViewModeIndex, bool bInNotify)
{
	ViewportSettings->SetViewModeIndex(InViewMode, InViewModeIndex, bInNotify);
}

float FMetaHumanEditorViewportClient::GetEV100(EABImageViewMode InViewMode) const
{
	return ViewportSettings->GetEV100(InViewMode);
}

void FMetaHumanEditorViewportClient::SetEV100(float InValue, EABImageViewMode InViewMode, bool bInNotify)
{
	ViewportSettings->SetEV100(InViewMode, InValue, bInNotify);
}

FPostProcessSettings FMetaHumanEditorViewportClient::GetDefaultPostProcessSettings()
{
	FPostProcessSettings PostProcessSettings;

	const float DefaultBrightness = UMetaHumanViewportSettings::GetDefaultViewportBrightness();

	PostProcessSettings.bOverride_AutoExposureBias = 1;
	PostProcessSettings.AutoExposureBias = 0;
	PostProcessSettings.bOverride_AutoExposureMinBrightness = 1;
	PostProcessSettings.AutoExposureMinBrightness = DefaultBrightness;
	PostProcessSettings.bOverride_AutoExposureMaxBrightness = 1;
	PostProcessSettings.AutoExposureMaxBrightness = DefaultBrightness;
	PostProcessSettings.bOverride_ToneCurveAmount = 1;
	PostProcessSettings.ToneCurveAmount = 0;
	PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = 1;
	PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;

	return PostProcessSettings;
}

const FPostProcessSettings& FMetaHumanEditorViewportClient::GetPostProcessSettingsForCurrentView() const
{
	const EABImageViewMode ABViewMode = IsShowingSingleView() ? GetABViewMode() : EABImageViewMode::A;
	return ABSceneCaptureComponents[ABViewMode]->PostProcessSettings;
}

void FMetaHumanEditorViewportClient::SetDepthMeshComponent(UMetaHumanDepthMeshComponent* InDepthMeshComponent)
{
	DepthMeshComponent = InDepthMeshComponent;

	SetFootageDepthData(DepthDataFootage);
}

void FMetaHumanEditorViewportClient::SetFootageDepthData(const FMetaHumanViewportClientDepthData& InDepthData)
{
	DepthDataFootage = InDepthData;

	ViewportSettings->DepthNear = DepthDataFootage.GetNear();
	ViewportSettings->DepthFar = DepthDataFootage.GetFar();

	// TODO: This will be removed as the display of depth data will also be removed
	if (DepthMeshComponent.IsValid())
	{
		DepthMeshComponent->SetDepthRange(DepthDataFootage.GetNear(), DepthDataFootage.GetFar());
	}

	OnUpdateFootageDepthDataDelegate.ExecuteIfBound(DepthDataFootage.GetNear(), DepthDataFootage.GetFar());
}

FMetaHumanViewportClientDepthData FMetaHumanEditorViewportClient::GetFootageDepthData() const
{
	return DepthDataFootage;
}

void FMetaHumanEditorViewportClient::SetMeshDepthData(const FMetaHumanViewportClientDepthData& InDepthData)
{
	DepthDataMesh = InDepthData;

	if (DepthMeshComponent.IsValid())
	{
		DepthMeshComponent->SetDepthRange(DepthDataFootage.GetNear(), DepthDataFootage.GetFar());
	}

	OnUpdateMeshDepthDataDelegate.ExecuteIfBound(DepthDataMesh.GetNear(), DepthDataMesh.GetFar());
}

FMetaHumanViewportClientDepthData FMetaHumanEditorViewportClient::GetMeshDepthData() const
{
	return DepthDataMesh;
}

EViewModeIndex FMetaHumanEditorViewportClient::GetViewModeIndexForABViewMode(EABImageViewMode InViewMode) const
{
	return ViewportSettings->GetViewModeIndex(InViewMode);
}

EABImageViewMode FMetaHumanEditorViewportClient::GetABViewMode() const
{
	return GetTrackerImageViewer()->GetViewMode();
}

void FMetaHumanEditorViewportClient::SetNavigationLocked(bool bIsLocked)
{
	GetTrackerImageViewer()->SetNavigationMode(bIsLocked ? EABImageNavigationMode::TwoD : EABImageNavigationMode::ThreeD);

	RefreshTrackerImageViewer();
	UpdateABVisibility();
}

bool FMetaHumanEditorViewportClient::IsNavigationLocked() const
{
	return GetTrackerImageViewer()->GetNavigationMode() == EABImageNavigationMode::TwoD;
}

bool FMetaHumanEditorViewportClient::IsCameraMoving() const
{
	return bIsCameraMoving;
}

void FMetaHumanEditorViewportClient::SetCurveDataController(const TSharedPtr<FMetaHumanCurveDataController> InCurveDataController)
{
	GetTrackerImageViewer()->SetDataControllerForCurrentFrame(InCurveDataController);
}

void FMetaHumanEditorViewportClient::SetTrackerImageSize(const FIntPoint& InTrackerImageSize)
{
	GetTrackerImageViewer()->SetTrackerImageSize(InTrackerImageSize);
}

void FMetaHumanEditorViewportClient::SetEditCurvesAndPointsEnabled(bool bInCanEdit)
{
	GetTrackerImageViewer()->SetEditCurvesAndPointsEnabled(bInCanEdit);
}

void FMetaHumanEditorViewportClient::RefreshTrackerImageViewer()
{
	if (EditorViewportWidget.IsValid())
	{
		GetTrackerImageViewer()->ResetView();
	}
}

void FMetaHumanEditorViewportClient::ResetABWipePostion()
{
	if (EditorViewportWidget.IsValid())
	{
		GetTrackerImageViewer()->ResetABWipePostion();
	}
}

void FMetaHumanEditorViewportClient::StoreCameraStateInViewportSettings()
{
	ToggleOrbitCamera(false);
	ViewportSettings->CameraState.Location = GetViewLocation();
	ViewportSettings->CameraState.Rotation = GetViewRotation();
	ViewportSettings->CameraState.LookAt = GetLookAtLocation();
	ViewportSettings->CameraState.ViewFOV = ViewFOV;
	ViewportSettings->CameraState.SpeedSetting = CameraSpeedSetting;
	ViewportSettings->CameraState.SpeedScalar = CameraSpeedScalar;
}

void FMetaHumanEditorViewportClient::SetABViewMode(EABImageViewMode InViewMode)
{
	if (GetABViewMode() != InViewMode)
	{
		const bool bWasDualView = IsShowingDualView();

		GetTrackerImageViewer()->SetViewMode(InViewMode);

		ViewportSettings->PreEditChange(nullptr);
		ViewportSettings->CurrentViewMode = InViewMode;
		ViewportSettings->PostEditChange();

		if (bWasDualView)
		{
			// If switching from dual view reset the view to restore the original zoom level
			// If switching from single to wipe or vice-versa this doesn't need to be done
			// as the zoom level can be preserved without issues
			RefreshTrackerImageViewer();
		}
	}
}

void FMetaHumanEditorViewportClient::NotifyViewportSettingsChanged() const
{
	ViewportSettings->NotifySettingsChanged();
}

bool FMetaHumanEditorViewportClient::IsShowingSingleView() const
{
	return GetTrackerImageViewer()->IsSingleView();
}

bool FMetaHumanEditorViewportClient::IsShowingDualView() const
{
	return GetTrackerImageViewer()->GetViewMode() == EABImageViewMode::ABSide;
}

bool FMetaHumanEditorViewportClient::IsShowingWipeView() const
{
	return GetTrackerImageViewer()->GetViewMode() == EABImageViewMode::ABSplit;
}

bool FMetaHumanEditorViewportClient::IsShowingViewA() const
{
	return GetABViewMode() == EABImageViewMode::A;
}

bool FMetaHumanEditorViewportClient::IsShowingViewB() const
{
	return GetABViewMode() == EABImageViewMode::B;
}

void FMetaHumanEditorViewportClient::ToggleABViews()
{
	TSharedRef<SABImage> ABImageWidget = GetTrackerImageViewer();

	if (IsShowingSingleView())
	{
		ViewportSettings->PreEditChange(nullptr);

		if (IsShowingViewA())
		{
			ABImageWidget->SetViewMode(EABImageViewMode::B);
			ViewportSettings->CurrentViewMode = EABImageViewMode::B;
		}
		else if (IsShowingViewB())
		{
			ABImageWidget->SetViewMode(EABImageViewMode::A);
			ViewportSettings->CurrentViewMode = EABImageViewMode::A;
		}

		ViewportSettings->PostEditChange();
	}

	CameraController->ResetVelocity();
}

void FMetaHumanEditorViewportClient::ToggleShowCurves(EABImageViewMode InViewMode)
{
	ViewportSettings->ToggleShowCurves(InViewMode);
}

void FMetaHumanEditorViewportClient::ToggleShowControlVertices(EABImageViewMode InViewMode)
{
	ViewportSettings->ToggleShowControlVertices(InViewMode);
}

bool FMetaHumanEditorViewportClient::CanToggleShowCurves(EABImageViewMode InViewMode) const
{
	// For now, tracking data can only be displayed when in single view mode and not viewing undistorted footage
	return IsShowingSingleView() && !IsShowingUndistorted(InViewMode);
}

bool FMetaHumanEditorViewportClient::CanToggleShowControlVertices(EABImageViewMode InViewMode) const
{
	// For now, tracking data can only be displayed when in single view mode and not viewing undistorted footage
	return IsShowingSingleView() && !IsShowingUndistorted(InViewMode);
}

bool FMetaHumanEditorViewportClient::IsShowingCurves(EABImageViewMode InViewMode) const
{
	return ViewportSettings->IsShowingCurves(InViewMode);
}

bool FMetaHumanEditorViewportClient::IsShowingControlVertices(EABImageViewMode InViewMode) const
{
	return ViewportSettings->IsShowingControlVertices(InViewMode);
}

bool FMetaHumanEditorViewportClient::ShouldShowCurves(EABImageViewMode InViewMode) const
{
	return IsShowingCurves(InViewMode);
}

bool FMetaHumanEditorViewportClient::ShouldShowControlVertices(EABImageViewMode InViewMode) const
{
	return IsShowingControlVertices(InViewMode);
}

bool FMetaHumanEditorViewportClient::IsRigVisible(EABImageViewMode InViewMode) const
{
	return ViewportSettings->IsSkeletalMeshVisible(InViewMode);
}

bool FMetaHumanEditorViewportClient::IsFootageVisible(EABImageViewMode InViewMode) const
{
	return ViewportSettings->IsFootageVisible(InViewMode);
}

bool FMetaHumanEditorViewportClient::IsDepthMeshVisible(EABImageViewMode InViewMode) const
{
	return ViewportSettings->IsDepthMeshVisible(InViewMode);
}

bool FMetaHumanEditorViewportClient::IsShowingUndistorted(EABImageViewMode InViewMode) const
{
	return ViewportSettings->IsShowingUndistorted(InViewMode);
}

void FMetaHumanEditorViewportClient::ToggleRigVisibility(EABImageViewMode InViewMode)
{
	ViewportSettings->ToggleSkeletalMeshVisibility(InViewMode);
}

void FMetaHumanEditorViewportClient::ToggleFootageVisibility(EABImageViewMode InViewMode)
{
	ViewportSettings->ToggleFootageVisibility(InViewMode);
}

void FMetaHumanEditorViewportClient::ToggleDepthMeshVisible(EABImageViewMode InViewMode)
{
	ViewportSettings->ToggleDepthMeshVisibility(InViewMode);
}

void FMetaHumanEditorViewportClient::ToggleDistortion(EABImageViewMode InViewMode)
{
	ViewportSettings->ToggleDistortion(InViewMode);
}

FVector2D FMetaHumanEditorViewportClient::GetWidgetSize() const
{
	return GetTrackerImageViewer()->GetCachedGeometry().GetLocalSize();
}

FVector2D FMetaHumanEditorViewportClient::GetPointPositionOnImage(const FVector2D& InScreenPosition) const
{
	const bool bUseImageUV = false;
	return GetTrackerImageViewer()->GetPointPositionOnImage(InScreenPosition, bUseImageUV);
}

void FMetaHumanEditorViewportClient::SetOverlay(const FText& InOverlay) const
{
	GetTrackerImageViewer()->SetOverlay(InOverlay);
}

TSharedRef<SMetaHumanEditorViewport> FMetaHumanEditorViewportClient::GetMetaHumanEditorViewport() const
{
	check(EditorViewportWidget.IsValid());
	return StaticCastSharedPtr<SMetaHumanEditorViewport>(GetEditorViewportWidget()).ToSharedRef();
}

TSharedRef<SMetaHumanOverlayWidget<STrackerImageViewer>> FMetaHumanEditorViewportClient::GetTrackerImageViewer() const
{
	return GetMetaHumanEditorViewport()->GetTrackerImageViewer();
}

TArray<UPrimitiveComponent*> FMetaHumanEditorViewportClient::GetSelectedPrimitiveComponents() const
{
	TArray<UPrimitiveComponent*> Components;

	if (OnGetSelectedPrimitivesComponentsDelegate.IsBound())
	{
		Components = OnGetSelectedPrimitivesComponentsDelegate.Execute();
	}

	return Components;
}

FBox FMetaHumanEditorViewportClient::GetComponentsBoundingBox(const TArray<UPrimitiveComponent*>& InComponents) const
{
	FBoxSphereBounds ComponentBounds = {};

	if (!InComponents.IsEmpty())
	{
		InComponents[0]->UpdateBounds();

		ComponentBounds = InComponents[0]->Bounds;

		for (int32 CompIndex = 1; CompIndex < InComponents.Num(); ++CompIndex)
		{
			UPrimitiveComponent* Component = InComponents[CompIndex];
			Component->UpdateBounds();
			ComponentBounds = ComponentBounds + Component->Bounds;
		}
	}

	return ComponentBounds.GetBox();
}

void FMetaHumanEditorViewportClient::GetAllComponentsAndComponentsHiddenForView(TArray<class UPrimitiveComponent*>& OutAllComponents, TMap<EABImageViewMode, TArray<class UPrimitiveComponent*>>& OutHiddenComponentsForView) const
{
	if (OnGetAllPrimitiveComponentsDelegate.IsBound())
	{
		OutAllComponents = OnGetAllPrimitiveComponentsDelegate.Execute();
	}

	OutHiddenComponentsForView =
	{
		{ EABImageViewMode::A, GetHiddenComponentsForView(EABImageViewMode::A) },
		{ EABImageViewMode::B, GetHiddenComponentsForView(EABImageViewMode::B) },
	};

	if (DepthMeshComponent.IsValid())
	{
		OutAllComponents.Add(DepthMeshComponent.Get());

		if (!IsDepthMeshVisible(EABImageViewMode::A))
		{
			OutHiddenComponentsForView[EABImageViewMode::A].Add(DepthMeshComponent.Get());
		}

		if (!IsDepthMeshVisible(EABImageViewMode::B))
		{
			OutHiddenComponentsForView[EABImageViewMode::B].Add(DepthMeshComponent.Get());
		}
	}
}

bool FMetaHumanEditorViewportClient::IsAnyFootageComponentVisible(const TArray<UPrimitiveComponent*>& InAllComponents, const TMap<EABImageViewMode, TArray<UPrimitiveComponent*>>& InHiddenComponentsForView) const
{
	bool bIsAnyFootageComponentVisible = false;
	if (IsShowingSingleView())
	{
		// Only check the current A or B view
		bIsAnyFootageComponentVisible = Algo::AnyOf(InAllComponents, [CurrentABViewMode = GetABViewMode(), &InHiddenComponentsForView](const UPrimitiveComponent* InComponent)
		{
			return InComponent->IsA<UMetaHumanFootageComponent>() &&
			!InHiddenComponentsForView[CurrentABViewMode].Contains(InComponent);
		});
	}
	else
	{
		// Check both views
		bIsAnyFootageComponentVisible = Algo::AnyOf(InAllComponents, [&InHiddenComponentsForView](const UPrimitiveComponent* InComponent)
		{
			const bool bIsVisibleViewA = !InHiddenComponentsForView[EABImageViewMode::A].Contains(InComponent);
			const bool bIsVisibleViewB = !InHiddenComponentsForView[EABImageViewMode::B].Contains(InComponent);
			return InComponent->IsA<UMetaHumanFootageComponent>() && (bIsVisibleViewA || bIsVisibleViewB);
		});
	}

	return bIsAnyFootageComponentVisible;
}

#undef LOCTEXT_NAMESPACE
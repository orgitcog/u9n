// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanIdentityViewportClient.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityViewportSettings.h"
#include "MetaHumanTemplateMeshComponent.h"
#include "CaptureData.h"

#include "MetaHumanFootageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CameraController.h"

FMetaHumanIdentityViewportClient::FMetaHumanIdentityViewportClient(FPreviewScene* InPreviewScene, UMetaHumanIdentity* InIdentity)
	: FMetaHumanEditorViewportClient{ InPreviewScene, InIdentity->ViewportSettings }
	, Identity{ InIdentity }
{
	check(Identity);
	check(Identity->ViewportSettings);
}

TArray<UPrimitiveComponent*> FMetaHumanIdentityViewportClient::GetHiddenComponentsForView(EABImageViewMode InViewMode) const
{
	TArray<UPrimitiveComponent*> HiddenComponents;

	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		EIdentityPoseType SelectedPoseType = EIdentityPoseType::Invalid;
		if (OnGetSelectedPoseTypeDelegate.IsBound())
		{
			SelectedPoseType = OnGetSelectedPoseTypeDelegate.Execute();
			if (SelectedPoseType != EIdentityPoseType::Invalid)
			{
				if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
				{
					if (SelectedPoseType != EIdentityPoseType::Neutral || !IsCurrentPoseVisible(InViewMode) && NeutralPose->IsCaptureDataValid())
					{
						if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(NeutralPose->CaptureDataSceneComponent))
						{
							HiddenComponents.Add(Component);
						}
					}
				}

				if (UMetaHumanIdentityPose* TeethPose = Face->FindPoseByType(EIdentityPoseType::Teeth))
				{
					if (SelectedPoseType != EIdentityPoseType::Teeth || !IsCurrentPoseVisible(InViewMode) && TeethPose->IsCaptureDataValid())
					{
						if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(TeethPose->CaptureDataSceneComponent))
						{
							HiddenComponents.Add(Component);
						}
					}
				}
			}
		}

		if (!IsRigVisible(InViewMode) && Face->IsConformalRigValid())
		{
			HiddenComponents.Add(Face->RigComponent);
		}

		if (!IsTemplateMeshVisible(InViewMode))
		{
			HiddenComponents.Add(Face->TemplateMeshComponent);
		}
	}

	return HiddenComponents;
}

void FMetaHumanIdentityViewportClient::UpdateABVisibility(bool bInSetViewpoint)
{
	if (!EditorViewportWidget.IsValid())
	{
		return;
	}

	FMetaHumanEditorViewportClient::UpdateABVisibility(bInSetViewpoint);

	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		// Updates the eye mesh and teeth mesh visibility of the instance component directly as FMetaHumanEditorViewportClient::UpdateABVisibility will always set it to be visible
		if (UMetaHumanTemplateMeshComponent* TemplateMeshComponentInstance = Cast<UMetaHumanTemplateMeshComponent>(OnGetPrimitiveComponentInstanceDelegate.Execute(Face->TemplateMeshComponent)))
		{
			TemplateMeshComponentInstance->SetEyeMeshesVisibility(Face->TemplateMeshComponent->bShowEyes);
			TemplateMeshComponentInstance->SetTeethMeshVisibility(Face->TemplateMeshComponent->bShowTeethMesh);
		}

		EIdentityPoseType PoseType = EIdentityPoseType::Invalid;

		if (OnGetSelectedPoseTypeDelegate.IsBound())
		{
			PoseType = OnGetSelectedPoseTypeDelegate.Execute();
		}

		if (UMetaHumanIdentityPose* Pose = Face->FindPoseByType(PoseType))
		{
			if (Pose->IsCaptureDataValid())
			{
				if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(Pose->GetCaptureData()))
				{
					if (UMetaHumanFootageComponent* FootageSceneComponent = Cast<UMetaHumanFootageComponent>(Pose->CaptureDataSceneComponent))
					{
						// Toggle channel visibility
						if (UMetaHumanFootageComponent* FootageSceneComponentInstance = Cast<UMetaHumanFootageComponent>(OnGetPrimitiveComponentInstanceDelegate.Execute(FootageSceneComponent)))
						{
							for (EABImageViewMode ViewMode : { EABImageViewMode::A, EABImageViewMode::B })
							{
								if (IsFootageVisible(ViewMode))
								{
									FootageSceneComponent->ShowColorChannel(ViewMode);
									FootageSceneComponentInstance->ShowColorChannel(ViewMode);
								}

								FootageSceneComponent->SetUndistortionEnabled(ViewMode, IsShowingUndistorted(ViewMode));
								FootageSceneComponentInstance->SetUndistortionEnabled(ViewMode, IsShowingUndistorted(ViewMode));
							}
						}
					}
				}
			}
		}
	}
}

void FMetaHumanIdentityViewportClient::Tick(float InDeltaSeconds)
{
	FMetaHumanEditorViewportClient::Tick(InDeltaSeconds);

	// If the navigation is locked, i.e., we are in 2D navigation mode and the rig component is playing its animation
	// call update the scene capture components so we can see the animation playing in locked modes
	if (IsNavigationLocked() && IsRigVisible(EABImageViewMode::Current))
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->RigComponent->AnimationData.bSavedPlaying)
			{
				UpdateSceneCaptureComponents();
			}
		}
	}
}

bool FMetaHumanIdentityViewportClient::CanToggleShowCurves(EABImageViewMode InViewMode) const
{
	// Enable curves toggle only when focusing on a promoted frame
	return FMetaHumanEditorViewportClient::CanToggleShowCurves(InViewMode) && HasSelectedPromotedFrame();
}

bool FMetaHumanIdentityViewportClient::CanToggleShowControlVertices(EABImageViewMode InViewMode) const
{
	// Enable control points toggle only when focusing on a promoted frame
	return FMetaHumanEditorViewportClient::CanToggleShowControlVertices(InViewMode) && HasSelectedPromotedFrame();
}

bool FMetaHumanIdentityViewportClient::CanChangeViewMode(EABImageViewMode InViewMode) const
{
	if (OnGetSelectedPromotedFrameDelegate.IsBound())
	{
		if (UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = Cast<UMetaHumanIdentityPromotedFrame>(OnGetSelectedPromotedFrameDelegate.Execute()))
		{
			return !SelectedPromotedFrame->IsNavigationLocked();
		}
	}

	return FMetaHumanEditorViewportClient::CanChangeViewMode(InViewMode);
}

bool FMetaHumanIdentityViewportClient::CanChangeEV100(EABImageViewMode InViewMode) const
{
	if (OnGetSelectedPromotedFrameDelegate.IsBound())
	{
		if (UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = Cast<UMetaHumanIdentityPromotedFrame>(OnGetSelectedPromotedFrameDelegate.Execute()))
		{
			return !SelectedPromotedFrame->IsNavigationLocked();
		}
	}

	return FMetaHumanEditorViewportClient::CanChangeEV100(InViewMode);
}

UMetaHumanFootageComponent* FMetaHumanIdentityViewportClient::GetActiveFootageComponent(const TArray<UPrimitiveComponent*>& InAllComponents) const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		EIdentityPoseType PoseType = EIdentityPoseType::Invalid;

		if (OnGetSelectedPoseTypeDelegate.IsBound())
		{
			PoseType = OnGetSelectedPoseTypeDelegate.Execute();
		}

		if (UMetaHumanIdentityPose* Pose = Face->FindPoseByType(PoseType))
		{
			if (Pose->IsCaptureDataValid())
			{
				if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(Pose->GetCaptureData()))
				{
					return Cast<UMetaHumanFootageComponent>(Pose->CaptureDataSceneComponent);
				}
			}
		}
	}

	return FMetaHumanEditorViewportClient::GetActiveFootageComponent(InAllComponents);
}

bool FMetaHumanIdentityViewportClient::GetSetViewpoint() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		EIdentityPoseType PoseType = EIdentityPoseType::Invalid;

		if (OnGetSelectedPoseTypeDelegate.IsBound())
		{
			PoseType = OnGetSelectedPoseTypeDelegate.Execute();
		}

		if (UMetaHumanIdentityPose* Pose = Face->FindPoseByType(PoseType))
		{
			if (Pose->IsCaptureDataValid())
			{
				if (Pose->GetCaptureData()->IsA<UMeshCaptureData>())
				{
					return false;
				}
			}
		}
	}

	return FMetaHumanEditorViewportClient::GetSetViewpoint();
}

bool FMetaHumanIdentityViewportClient::ShouldShowCurves(EABImageViewMode InViewMode) const
{
	return FMetaHumanEditorViewportClient::ShouldShowCurves(InViewMode) && IsCurrentPoseVisible(InViewMode) && HasSelectedPromotedFrame();
}

bool FMetaHumanIdentityViewportClient::ShouldShowControlVertices(EABImageViewMode InViewMode) const
{
	return FMetaHumanEditorViewportClient::ShouldShowControlVertices(InViewMode) && IsCurrentPoseVisible(InViewMode) && HasSelectedPromotedFrame();
}

bool FMetaHumanIdentityViewportClient::IsFootageVisible(EABImageViewMode InViewMode) const
{
	// In the case of the identity there is no explicit IsFootageVisible but there is the current pose.
	// The idea here is to check if the current pose is visible if its footage, this is used
	// by the camera toolbar button to enable/disable the fov slider

	if (IsCurrentPoseVisible(InViewMode))
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
			{
				if (NeutralPose->IsCaptureDataValid())
				{
					return NeutralPose->GetCaptureData()->IsA<UFootageCaptureData>();
				}
			}
		}
	}

	return false;
}

void FMetaHumanIdentityViewportClient::FocusViewportOnSelection()
{
	if (IsFootageVisible(EABImageViewMode::Current))
	{
		RefreshTrackerImageViewer();

		CameraController->ResetVelocity();

		FViewportCameraTransform& ViewTransform = GetViewTransform();
		ViewTransform.SetLookAt(FVector::ZeroVector);

		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->bIsConformed && OnGetPrimitiveComponentInstanceDelegate.IsBound())
			{
				UPrimitiveComponent* RigComponentInstance = OnGetPrimitiveComponentInstanceDelegate.Execute(Face->RigComponent);
				const FVector Center = RigComponentInstance->Bounds.GetBox().GetCenter();
				ViewTransform.SetLookAt(FVector{ Center.X, 0.0, 0.0 });
				ViewTransform.SetLocation(FVector::ZeroVector);
				ViewTransform.SetRotation(FRotator::ZeroRotator);
				StoreCameraStateInViewportSettings();
			}
			else
			{
				// If the face is not conformed we don't have a calibrated camera to use so just focus on the current selection
				FMetaHumanEditorViewportClient::FocusViewportOnSelection();
			}
		}
	}
	else
	{
		FMetaHumanEditorViewportClient::FocusViewportOnSelection();
	}

	UpdateABVisibility();
}

bool FMetaHumanIdentityViewportClient::HasSelectedPromotedFrame() const
{
	return OnGetSelectedPromotedFrameDelegate.IsBound() && OnGetSelectedPromotedFrameDelegate.Execute() != nullptr;
}

bool FMetaHumanIdentityViewportClient::IsCurrentPoseVisible(EABImageViewMode InViewMode) const
{
	return Identity->ViewportSettings->IsCurrentPoseVisible(InViewMode);
}

bool FMetaHumanIdentityViewportClient::IsTemplateMeshVisible(EABImageViewMode InViewMode) const
{
	return Identity->ViewportSettings->IsTemplateMeshVisible(InViewMode);
}

void FMetaHumanIdentityViewportClient::ToggleCurrentPoseVisibility(EABImageViewMode InViewMode)
{
	Identity->ViewportSettings->ToggleCurrentPoseVisibility(InViewMode);
}

void FMetaHumanIdentityViewportClient::ToggleConformalMeshVisibility(EABImageViewMode InViewMode)
{
	Identity->ViewportSettings->ToggleTemplateMeshVisibility(InViewMode);
}

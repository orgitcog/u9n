// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanPerformanceViewportClient.h"
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceViewportSettings.h"
#include "MetaHumanFootageComponent.h"
#include "MetaHumanPerformanceControlRigComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CameraController.h"
#include "ControlRigGizmoActor.h"

FMetaHumanPerformanceViewportClient::FMetaHumanPerformanceViewportClient(FPreviewScene* InPreviewScene, class UMetaHumanPerformance* InPerformance)
	: FMetaHumanEditorViewportClient{ InPreviewScene, InPerformance->ViewportSettings }
	, Performance{ InPerformance }
{
	check(Performance);
	check(Performance->ViewportSettings);

	OnGetAllPrimitiveComponentsDelegate.BindLambda([this]
	{
		TArray<UPrimitiveComponent*> Components;

		if (RigComponent.IsBound() && RigComponent.Get() != nullptr)
		{
			Components.Add(RigComponent.Get());
		}

		if (FootageComponent.IsBound() && FootageComponent.Get() != nullptr)
		{
			Components.Add(FootageComponent.Get());
		}

		if (ControlRigComponent.IsBound() && ControlRigComponent.Get() != nullptr)
		{
			// See the comment on GetHiddenComponentsForView for why this is necessary
			for (AControlRigShapeActor* ShapeActor : ControlRigComponent.Get()->ShapeActors)
			{
				Components.Add(ShapeActor->StaticMeshComponent);
			}

			Components.Add(ControlRigComponent.Get());
		}

		return Components;
	});
}

TArray<UPrimitiveComponent*> FMetaHumanPerformanceViewportClient::GetHiddenComponentsForView(EABImageViewMode InViewMode) const
{
	TArray<UPrimitiveComponent*> HiddenComponents;

	if (!IsRigVisible(InViewMode))
	{
		if (RigComponent.IsBound() && RigComponent.Get() != nullptr)
		{
			HiddenComponents.Add(RigComponent.Get());
		}
	}

	if (!IsFootageVisible(InViewMode))
	{
		if (FootageComponent.IsBound() && FootageComponent.Get() != nullptr)
		{
			HiddenComponents.Add(FootageComponent.Get());
		}
	}

	if (!IsControlRigVisible(InViewMode))
	{
		if (ControlRigComponent.IsBound() && ControlRigComponent.Get() != nullptr)
		{
			// The MetaHuman Control Rig component uses a mechanism similar to Child Actor Components where it spawns
			// actors when its registered. Child Actor Components don't seem to work property with hidden components
			// of scene captures so we explicitly add all components that form the active control rig to be hidden
			for (AControlRigShapeActor* ShapeActor : ControlRigComponent.Get()->ShapeActors)
			{
				HiddenComponents.Add(ShapeActor->StaticMeshComponent);
			}

			HiddenComponents.Add(ControlRigComponent.Get());
		}
	}

	return HiddenComponents;
}

void FMetaHumanPerformanceViewportClient::UpdateABVisibility(bool bInSetViewpoint)
{
	FMetaHumanEditorViewportClient::UpdateABVisibility(bInSetViewpoint);

	if (FootageComponent.IsBound() && FootageComponent.Get() != nullptr)
	{
		for (EABImageViewMode ViewMode : { EABImageViewMode::A, EABImageViewMode::B })
		{
			if (IsFootageVisible(ViewMode))
			{
				FootageComponent.Get()->ShowColorChannel(ViewMode);
			}

			FootageComponent.Get()->SetUndistortionEnabled(ViewMode, IsShowingUndistorted(ViewMode));
		}
	}
}

bool FMetaHumanPerformanceViewportClient::ShouldShowCurves(EABImageViewMode InViewMode) const
{
	return FMetaHumanEditorViewportClient::ShouldShowCurves(InViewMode) && IsFootageVisible(InViewMode);
}

bool FMetaHumanPerformanceViewportClient::ShouldShowControlVertices(EABImageViewMode InViewMode) const
{
	return FMetaHumanEditorViewportClient::ShouldShowControlVertices(InViewMode) && IsFootageVisible(InViewMode);
}

void FMetaHumanPerformanceViewportClient::FocusViewportOnSelection()
{
	if (IsFootageVisible(EABImageViewMode::Current))
	{
		RefreshTrackerImageViewer();
	}
	else
	{
		CameraController->ResetVelocity();

		FViewportCameraTransform& ViewTransform = GetViewTransform();
		ViewTransform.SetLookAt(FVector::ZeroVector);

		if (RigComponent.IsBound())
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = RigComponent.Get())
			{
				SkeletalMeshComponent->UpdateBounds();
				const FVector Center = SkeletalMeshComponent->Bounds.GetBox().GetCenter();
				ViewTransform.SetLookAt(FVector{ Center.X, 0.0, 0.0 });
			}
		}

		ViewTransform.SetLocation(FVector::ZeroVector);
		ViewTransform.SetRotation(FRotator::ZeroRotator);

		StoreCameraStateInViewportSettings();
	}

	UpdateABVisibility();
}

bool FMetaHumanPerformanceViewportClient::IsControlRigVisible(EABImageViewMode InViewMode) const
{
	return Performance->ViewportSettings->IsControlRigVisible(InViewMode);
}

void FMetaHumanPerformanceViewportClient::ToggleControlRigVisibility(EABImageViewMode InViewMode)
{
	Performance->ViewportSettings->ToggleControlRigVisibility(InViewMode);
}

void FMetaHumanPerformanceViewportClient::SetRigComponent(TAttribute<USkeletalMeshComponent*> InRigComponent)
{
	RigComponent = InRigComponent;
}

void FMetaHumanPerformanceViewportClient::SetFootageComponent(TAttribute<UMetaHumanFootageComponent*> InFootageComponent)
{
	FootageComponent = InFootageComponent;
}

void FMetaHumanPerformanceViewportClient::SetControlRigComponent(TAttribute<UMetaHumanPerformanceControlRigComponent*> InControlRigComponent)
{
	ControlRigComponent = InControlRigComponent;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceViewportSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanPerformanceViewportSettings)

UMetaHumanPerformanceViewportSettings::UMetaHumanPerformanceViewportSettings()
{
	FMetaHumanViewportState& ViewAState = ViewportState[EABImageViewMode::A];
	ViewAState.bShowSkeletalMesh = true;
	ViewAState.bShowFootage = true;
	ViewAState.bShowDepthMesh = false;
	ViewAState.bShowUndistorted = true;

	FMetaHumanViewportState& ViewBState = ViewportState[EABImageViewMode::B];
	ViewBState.bShowSkeletalMesh = true;
	ViewBState.bShowFootage = false;
	ViewBState.bShowDepthMesh = false;
	ViewBState.bShowUndistorted = true;

	CurrentViewMode = EABImageViewMode::B;

	CameraState.Location = FVector::ZeroVector;
	CameraState.LookAt = FVector::ZeroVector;
	CameraState.Rotation = FRotator::ZeroRotator;

	FMetaHumanPerformanceViewportState PerformanceViewAState;
	PerformanceViewAState.bShowControlRig = false;

	FMetaHumanPerformanceViewportState PerformanceViewBState;
	PerformanceViewBState.bShowControlRig = false;

	PerformanceViewportState =
	{
		{ EABImageViewMode::A, MoveTemp(PerformanceViewAState) },
		{ EABImageViewMode::B, MoveTemp(PerformanceViewBState) }
	};
}

bool UMetaHumanPerformanceViewportSettings::IsControlRigVisible(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsControlRigVisible(CurrentViewMode);
		}
		else
		{
			return IsControlRigVisible(EABImageViewMode::A) || IsControlRigVisible(EABImageViewMode::B);
		}
	}
	else
	{
		return PerformanceViewportState[InView].bShowControlRig;
	}
}

void UMetaHumanPerformanceViewportSettings::ToggleControlRigVisibility(EABImageViewMode InView)
{
	check(PerformanceViewportState.Contains(InView));

	PerformanceViewportState[InView].bShowControlRig = ~PerformanceViewportState[InView].bShowControlRig;

	NotifySettingsChanged();
}
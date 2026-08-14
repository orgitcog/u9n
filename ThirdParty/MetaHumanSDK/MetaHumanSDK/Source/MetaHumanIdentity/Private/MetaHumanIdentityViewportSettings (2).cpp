// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityViewportSettings.h"

/////////////////////////////////////////////////////
// UMetaHumanIdentityViewportSettings

UMetaHumanIdentityViewportSettings::UMetaHumanIdentityViewportSettings()
{
	FMetaHumanIdentityABViewportState ViewAState;
	ViewAState.bShowCurrentPose = true;
	ViewAState.bShowTemplateMesh = false;

	FMetaHumanIdentityABViewportState ViewBState;
	ViewBState.bShowCurrentPose = false;
	ViewBState.bShowTemplateMesh = true;

	IdentityViewportState =
	{
		{ EABImageViewMode::A, MoveTemp(ViewAState) },
		{ EABImageViewMode::B, MoveTemp(ViewBState) }
	};
}

void UMetaHumanIdentityViewportSettings::ToggleCurrentPoseVisibility(EABImageViewMode InView)
{
	check(IdentityViewportState.Contains(InView));

	IdentityViewportState[InView].bShowCurrentPose = ~IdentityViewportState[InView].bShowCurrentPose;

	NotifySettingsChanged();
}

bool UMetaHumanIdentityViewportSettings::IsCurrentPoseVisible(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsCurrentPoseVisible(CurrentViewMode);
		}
		else
		{
			return IsCurrentPoseVisible(EABImageViewMode::A) || IsCurrentPoseVisible(EABImageViewMode::B);
		}
	}
	else
	{
		return IdentityViewportState[InView].bShowCurrentPose;
	}
}

void UMetaHumanIdentityViewportSettings::ToggleTemplateMeshVisibility(EABImageViewMode InView)
{
	check(IdentityViewportState.Contains(InView));

	IdentityViewportState[InView].bShowTemplateMesh = ~IdentityViewportState[InView].bShowTemplateMesh;

	NotifySettingsChanged();
}

bool UMetaHumanIdentityViewportSettings::IsTemplateMeshVisible(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsTemplateMeshVisible(CurrentViewMode);
		}
		else
		{
			return IsTemplateMeshVisible(EABImageViewMode::A) || IsTemplateMeshVisible(EABImageViewMode::B);
		}
	}
	else
	{
		return IdentityViewportState[InView].bShowTemplateMesh;
	}
}

void UMetaHumanIdentityViewportSettings::SetSelectedPromotedFrame(EIdentityPoseType InPoseType, int32 InPromotedFrameIndex)
{
	IdentityPosesState.FindOrAdd(InPoseType).SelectedFrame = InPromotedFrameIndex;

	NotifySettingsChanged();
}

int32 UMetaHumanIdentityViewportSettings::GetSelectedPromotedFrame(EIdentityPoseType InPoseType) const
{
	if (IdentityPosesState.Contains(InPoseType))
	{
		return IdentityPosesState[InPoseType].SelectedFrame;
	}

	return INDEX_NONE;
}

void UMetaHumanIdentityViewportSettings::SetFrameTimeForPose(EIdentityPoseType InPoseType, const FFrameTime& InFrameTime)
{
	IdentityPosesState.FindOrAdd(InPoseType).CurrentFrameTime = InFrameTime;

	NotifySettingsChanged();
}

FFrameTime UMetaHumanIdentityViewportSettings::GetFrameTimeForPose(EIdentityPoseType InPoseType) const
{
	if (IdentityPosesState.Contains(InPoseType))
	{
		return IdentityPosesState[InPoseType].CurrentFrameTime;
	}

	return FFrameTime{};
}
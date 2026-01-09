// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanViewportSettings.h"

#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "Viewports.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanViewportSettings)

UMetaHumanViewportSettings::UMetaHumanViewportSettings()
{
	CurrentViewMode = EABImageViewMode::A;

#if WITH_EDITOR
	CameraState.Location = EditorViewportDefs::DefaultPerspectiveViewLocation;
	CameraState.Rotation = EditorViewportDefs::DefaultPerspectiveViewRotation;
#endif

	CameraState.LookAt = FVector::ZeroVector;
	CameraState.ViewFOV = 45.0f;
	CameraState.SpeedSetting = 2;
	CameraState.SpeedScalar = 1.0f;

	DepthNear = 10.0f;
	DepthFar = 100.0f;

	FMetaHumanViewportState ViewportAState;
	ViewportAState.bShowCurves = true;
	ViewportAState.bShowControlVertices = true;
	ViewportAState.bShowFootage = true;
	ViewportAState.bShowSkeletalMesh = false;
	ViewportAState.bShowDepthMesh = false;
	ViewportAState.ViewModeIndex = VMI_Lit;
	ViewportAState.FixedEV100 = GetDefaultViewportBrightness();

	FMetaHumanViewportState ViewportBState;
	ViewportBState.bShowCurves = true;
	ViewportBState.bShowControlVertices = true;
	ViewportBState.bShowFootage = false;
	ViewportBState.bShowSkeletalMesh = true;
	ViewportBState.bShowDepthMesh = false;
	ViewportBState.ViewModeIndex = VMI_Lit;
	ViewportBState.FixedEV100 = GetDefaultViewportBrightness();

	ViewportState =
	{
		{ EABImageViewMode::A, MoveTemp(ViewportAState) },
		{ EABImageViewMode::B, MoveTemp(ViewportBState) }
	};
}

#if WITH_EDITOR

void UMetaHumanViewportSettings::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	NotifySettingsChanged();
}

#endif

bool UMetaHumanViewportSettings::IsExtendDefaultLuminanceRangeEnabled()
{
	// In UE5.1 new projects always have this set to true and the user cannot turn it off unless settings the CVar directly, but old projects might have this set to false
	// so read the value here for backward compatibility
	static const TConsoleVariableData<int32>* VarDefaultAutoExposureExtendDefaultLuminanceRange = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	return VarDefaultAutoExposureExtendDefaultLuminanceRange->GetValueOnAnyThread() == 1;
}

float UMetaHumanViewportSettings::GetDefaultViewportBrightness()
{
	const bool bExtendedLuminanceRange = IsExtendDefaultLuminanceRangeEnabled();

	// If extended luminance range is set to true, brightness is set to use EV100 values, so default to 0
	// if false, use a value of 1 for brightness to keep the scene as is
	return bExtendedLuminanceRange ? 0.0f : 1.0f;
}

EViewModeIndex UMetaHumanViewportSettings::GetViewModeIndex(EABImageViewMode InView)
{
	check(ViewportState.Contains(InView));
	return ViewportState[InView].ViewModeIndex;
}

float UMetaHumanViewportSettings::GetEV100(EABImageViewMode InView)
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return ViewportState[CurrentViewMode].FixedEV100;
		}
		else
		{
			return ViewportState[EABImageViewMode::A].FixedEV100;
		}
	}
	else
	{
		return ViewportState[InView].FixedEV100;
	}
}

void UMetaHumanViewportSettings::SetEV100(EABImageViewMode InView, float InValue, bool bInNotify)
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			float& FixedEV100 = ViewportState[CurrentViewMode].FixedEV100;
			if (FixedEV100 != InValue)
			{
				FixedEV100 = InValue;
			}
		}
		else
		{
			ViewportState[EABImageViewMode::A].FixedEV100 = InValue;
			ViewportState[EABImageViewMode::B].FixedEV100 = InValue;
		}
	}
	else
	{
		float& FixedEV100 = ViewportState[InView].FixedEV100;
		if (FixedEV100 != InValue)
		{
			FixedEV100 = InValue;
		}
	}

	if (bInNotify)
	{
		NotifySettingsChanged();
	}
}

void UMetaHumanViewportSettings::SetViewModeIndex(EABImageViewMode InView, EViewModeIndex InViewModeIndex, bool bInNotify)
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			TEnumAsByte<EViewModeIndex>& ViewModeIndex = ViewportState[CurrentViewMode].ViewModeIndex;
			if (ViewModeIndex != InViewModeIndex)
			{
				ViewModeIndex = InViewModeIndex;
			}
		}
		else
		{
			// Set in both views
			ViewportState[EABImageViewMode::A].ViewModeIndex = InViewModeIndex;
			ViewportState[EABImageViewMode::B].ViewModeIndex = InViewModeIndex;
		}
	}
	else
	{
		TEnumAsByte<EViewModeIndex>& ViewModeIndex = ViewportState[InView].ViewModeIndex;
		if (ViewModeIndex != InViewModeIndex)
		{
			ViewModeIndex = InViewModeIndex;
		}
	}

	if (bInNotify)
	{
		NotifySettingsChanged();
	}
}

bool UMetaHumanViewportSettings::IsShowingSingleView() const
{
	return CurrentViewMode == EABImageViewMode::A || CurrentViewMode == EABImageViewMode::B;
}

void UMetaHumanViewportSettings::ToggleShowCurves(EABImageViewMode InView)
{
	check(ViewportState.Contains(InView));

	ViewportState[InView].bShowCurves = ~ViewportState[InView].bShowCurves;

	NotifySettingsChanged();
}

bool UMetaHumanViewportSettings::IsShowingCurves(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsShowingCurves(CurrentViewMode);
		}
		else
		{
			return IsShowingCurves(EABImageViewMode::A) || IsShowingCurves(EABImageViewMode::B);
		}
	}

	// Do not show points if in undistorted mode
	return ViewportState[InView].bShowCurves && !IsShowingUndistorted(InView);
}

void UMetaHumanViewportSettings::ToggleShowControlVertices(EABImageViewMode InView)
{
	check(ViewportState.Contains(InView));

	ViewportState[InView].bShowControlVertices = ~ViewportState[InView].bShowControlVertices;

	NotifySettingsChanged();
}

bool UMetaHumanViewportSettings::IsShowingControlVertices(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsShowingControlVertices(CurrentViewMode);
		}
		else
		{
			return IsShowingControlVertices(EABImageViewMode::A) || IsShowingControlVertices(EABImageViewMode::B);
		}
	}

	// Do not show points if in undistorted mode
	return ViewportState[InView].bShowControlVertices && !IsShowingUndistorted(InView);
}

void UMetaHumanViewportSettings::ToggleSkeletalMeshVisibility(EABImageViewMode InView)
{
	check(ViewportState.Contains(InView));

	ViewportState[InView].bShowSkeletalMesh = ~ViewportState[InView].bShowSkeletalMesh;

	NotifySettingsChanged();
}

bool UMetaHumanViewportSettings::IsSkeletalMeshVisible(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsSkeletalMeshVisible(CurrentViewMode);
		}
		else
		{
			return IsSkeletalMeshVisible(EABImageViewMode::A) || IsSkeletalMeshVisible(EABImageViewMode::B);
		}
	}

	return ViewportState[InView].bShowSkeletalMesh;
}

void UMetaHumanViewportSettings::ToggleFootageVisibility(EABImageViewMode InView)
{
	check(ViewportState.Contains(InView));

	ViewportState[InView].bShowFootage = ~ViewportState[InView].bShowFootage;

	NotifySettingsChanged();
}

bool UMetaHumanViewportSettings::IsFootageVisible(EABImageViewMode InView)
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsFootageVisible(CurrentViewMode);
		}
		else
		{
			return IsFootageVisible(EABImageViewMode::A) || IsFootageVisible(EABImageViewMode::B);
		}
	}

	return ViewportState[InView].bShowFootage;
}

void UMetaHumanViewportSettings::ToggleDepthMeshVisibility(EABImageViewMode InView)
{
	check(ViewportState.Contains(InView));

	ViewportState[InView].bShowDepthMesh = ~ViewportState[InView].bShowDepthMesh;

	NotifySettingsChanged();
}

bool UMetaHumanViewportSettings::IsDepthMeshVisible(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsDepthMeshVisible(CurrentViewMode);
		}
		else
		{
			return IsDepthMeshVisible(EABImageViewMode::A) || IsDepthMeshVisible(EABImageViewMode::B);
		}
	}
	else
	{
		return ViewportState[InView].bShowDepthMesh;
	}
}

void UMetaHumanViewportSettings::ToggleDistortion(EABImageViewMode InView)
{
	check(ViewportState.Contains(InView));

	ViewportState[InView].bShowUndistorted = ~ViewportState[InView].bShowUndistorted;

	NotifySettingsChanged();
}

bool UMetaHumanViewportSettings::IsShowingUndistorted(EABImageViewMode InView) const
{
	if (InView == EABImageViewMode::Current)
	{
		if (IsShowingSingleView())
		{
			return IsShowingUndistorted(CurrentViewMode);
		}
		else
		{
			return IsShowingUndistorted(EABImageViewMode::A) || IsShowingUndistorted(EABImageViewMode::B);
		}
	}

	return ViewportState[InView].bShowUndistorted;
}

void UMetaHumanViewportSettings::NotifySettingsChanged()
{
	OnSettingsChangedDelegate.Broadcast();
}
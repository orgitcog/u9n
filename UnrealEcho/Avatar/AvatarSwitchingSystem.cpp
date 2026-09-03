// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - 2D/3D Avatar Switching Implementation

#include "AvatarSwitchingSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Live2DCubism/Live2DCubismAvatarComponent.h"
#include "Live2DCubism/Live2DCubismCore.h"
#include "Cosmetics/DeepTreeEchoCosmeticsComponent.h"
#include "Engine/World.h"

UAvatarSwitchingSystem::UAvatarSwitchingSystem()
	: DefaultMode(EAvatarRenderMode::Mode3D)
	, bAutoSelectMode(false)
	, PerformanceTargetFPS(60.0f)
	, ModeSwitchCooldown(2.0f)
	, CurrentMode(EAvatarRenderMode::Mode3D)
	, TargetMode(EAvatarRenderMode::Mode3D)
	, bIsTransitioning(false)
	, TransitionProgress(0.0f)
	, TransitionDuration(0.5f)
	, TimeSinceLastModeSwitch(0.0f)
	, FrameTimeHistoryIndex(0)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.016f; // ~60 FPS

	// Initialize frame time history
	FrameTimeHistory.SetNum(60); // Track last 60 frames
}

void UAvatarSwitchingSystem::BeginPlay()
{
	Super::BeginPlay();

	// Set initial mode
	SetRenderMode(DefaultMode);
}

void UAvatarSwitchingSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update transition
	if (bIsTransitioning)
	{
		UpdateTransition(DeltaTime);
	}

	// Update performance tracking
	UpdatePerformanceTracking(DeltaTime);

	// Check for auto mode switch
	TimeSinceLastModeSwitch += DeltaTime;
	if (bAutoSelectMode && TimeSinceLastModeSwitch >= ModeSwitchCooldown)
	{
		CheckAutoModeSwitch();
	}

	// Synchronize state between representations
	if (CurrentMode == EAvatarRenderMode::ModeHybrid)
	{
		// In hybrid mode, sync both ways
		SyncFrom3D();
		ApplySyncStateTo2D();
	}
}

void UAvatarSwitchingSystem::Initialize(USkeletalMeshComponent* In3DMesh, ULive2DCubismAvatarComponent* In2DAvatar)
{
	Mesh3D = In3DMesh;
	Avatar2D = In2DAvatar;

	// Try to find cosmetics component
	if (GetOwner())
	{
		CosmeticsComponent = GetOwner()->FindComponentByClass<UDeepTreeEchoCosmeticsComponent>();
	}

	// Set initial visibility based on mode
	SetComponentVisibility(CurrentMode);
}

void UAvatarSwitchingSystem::SetRenderMode(EAvatarRenderMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	EAvatarRenderMode OldMode = CurrentMode;
	CurrentMode = NewMode;
	TargetMode = NewMode;

	// Update component visibility
	SetComponentVisibility(NewMode);

	// Sync state to new representation
	switch (NewMode)
	{
	case EAvatarRenderMode::Mode3D:
		ApplySyncStateTo3D();
		break;
	case EAvatarRenderMode::Mode2D:
		ApplySyncStateTo2D();
		break;
	case EAvatarRenderMode::ModeHybrid:
		ApplySyncStateTo3D();
		ApplySyncStateTo2D();
		break;
	default:
		break;
	}

	TimeSinceLastModeSwitch = 0.0f;

	// Broadcast event
	OnAvatarModeChanged.Broadcast(OldMode, NewMode);
}

void UAvatarSwitchingSystem::TransitionToMode(EAvatarRenderMode NewMode, float TransitionTime)
{
	if (CurrentMode == NewMode || bIsTransitioning)
	{
		return;
	}

	TargetMode = NewMode;
	TransitionDuration = FMath::Max(TransitionTime, 0.1f);
	TransitionProgress = 0.0f;
	bIsTransitioning = true;
}

void UAvatarSwitchingSystem::SetSyncState(const FAvatarSyncState& State)
{
	CurrentSyncState = State;

	// Apply to current representation(s)
	switch (CurrentMode)
	{
	case EAvatarRenderMode::Mode3D:
		ApplySyncStateTo3D();
		break;
	case EAvatarRenderMode::Mode2D:
		ApplySyncStateTo2D();
		break;
	case EAvatarRenderMode::ModeHybrid:
		ApplySyncStateTo3D();
		ApplySyncStateTo2D();
		break;
	default:
		break;
	}

	OnAvatarSyncStateUpdated.Broadcast(CurrentSyncState);
}

void UAvatarSwitchingSystem::SyncFrom3D()
{
	if (!Mesh3D)
	{
		return;
	}

	UAnimInstance* AnimInstance = Mesh3D->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	// Extract facial morph targets
	// Note: These would be actual morph target names from the skeletal mesh
	CurrentSyncState.EyeOpenLeft = Mesh3D->GetMorphTarget(TEXT("EyeBlinkLeft"));
	CurrentSyncState.EyeOpenRight = Mesh3D->GetMorphTarget(TEXT("EyeBlinkRight"));
	CurrentSyncState.MouthOpen = Mesh3D->GetMorphTarget(TEXT("JawOpen"));
	CurrentSyncState.MouthForm = Mesh3D->GetMorphTarget(TEXT("MouthSmile")) - Mesh3D->GetMorphTarget(TEXT("MouthFrown"));
	CurrentSyncState.BrowLeftY = Mesh3D->GetMorphTarget(TEXT("BrowInnerUp_L"));
	CurrentSyncState.BrowRightY = Mesh3D->GetMorphTarget(TEXT("BrowInnerUp_R"));

	// Extract head rotation from bone
	FTransform HeadTransform = Mesh3D->GetSocketTransform(TEXT("head"), RTS_Component);
	CurrentSyncState.HeadRotation = HeadTransform.Rotator();

	// Extract body rotation
	CurrentSyncState.BodyRotation = Mesh3D->GetComponentRotation();
}

void UAvatarSwitchingSystem::SyncFrom2D()
{
	if (!Avatar2D)
	{
		return;
	}

	// Extract parameters from Live2D model
	CurrentSyncState.EyeOpenLeft = Avatar2D->GetParameterValue(TEXT("ParamEyeLOpen"));
	CurrentSyncState.EyeOpenRight = Avatar2D->GetParameterValue(TEXT("ParamEyeROpen"));
	CurrentSyncState.MouthOpen = Avatar2D->GetParameterValue(TEXT("ParamMouthOpenY"));
	CurrentSyncState.MouthForm = Avatar2D->GetParameterValue(TEXT("ParamMouthForm"));
	CurrentSyncState.BrowLeftY = Avatar2D->GetParameterValue(TEXT("ParamBrowLY"));
	CurrentSyncState.BrowRightY = Avatar2D->GetParameterValue(TEXT("ParamBrowRY"));
	CurrentSyncState.EyeGaze.X = Avatar2D->GetParameterValue(TEXT("ParamEyeBallX"));
	CurrentSyncState.EyeGaze.Y = Avatar2D->GetParameterValue(TEXT("ParamEyeBallY"));
	CurrentSyncState.Blush = Avatar2D->GetParameterValue(TEXT("ParamCheek"));

	// Head rotation from angle parameters
	CurrentSyncState.HeadRotation.Yaw = Avatar2D->GetParameterValue(TEXT("ParamAngleX"));
	CurrentSyncState.HeadRotation.Pitch = Avatar2D->GetParameterValue(TEXT("ParamAngleY"));
	CurrentSyncState.HeadRotation.Roll = Avatar2D->GetParameterValue(TEXT("ParamAngleZ"));

	// Body rotation
	CurrentSyncState.BodyRotation.Yaw = Avatar2D->GetParameterValue(TEXT("ParamBodyAngleX"));
	CurrentSyncState.BodyRotation.Pitch = Avatar2D->GetParameterValue(TEXT("ParamBodyAngleY"));
	CurrentSyncState.BodyRotation.Roll = Avatar2D->GetParameterValue(TEXT("ParamBodyAngleZ"));

	CurrentSyncState.BreathValue = Avatar2D->GetParameterValue(TEXT("ParamBreath"));
}

void UAvatarSwitchingSystem::SetAutoModeSelection(bool bEnable)
{
	bAutoSelectMode = bEnable;
}

void UAvatarSwitchingSystem::SetPerformanceTarget(float TargetFPS)
{
	PerformanceTargetFPS = FMath::Max(TargetFPS, 15.0f);
}

void UAvatarSwitchingSystem::LoadLive2DModel(const FString& ModelPath)
{
	if (Avatar2D)
	{
		Avatar2D->LoadLive2DModel(ModelPath);
	}
}

bool UAvatarSwitchingSystem::IsLive2DModelLoaded() const
{
	return Avatar2D && Avatar2D->IsModelLoaded();
}

void UAvatarSwitchingSystem::UpdateTransition(float DeltaTime)
{
	TransitionProgress += DeltaTime / TransitionDuration;

	if (TransitionProgress >= 1.0f)
	{
		// Transition complete
		TransitionProgress = 1.0f;
		bIsTransitioning = false;
		SetRenderMode(TargetMode);
	}
	else
	{
		// Interpolate visibility
		float Alpha = FMath::SmoothStep(0.0f, 1.0f, TransitionProgress);

		// Fade out current, fade in target
		switch (CurrentMode)
		{
		case EAvatarRenderMode::Mode3D:
			if (Mesh3D)
			{
				// Mesh3D->SetScalarParameterValueOnMaterials(TEXT("Opacity"), 1.0f - Alpha);
			}
			break;
		case EAvatarRenderMode::Mode2D:
			if (Avatar2D)
			{
				Avatar2D->SetOpacity(1.0f - Alpha);
			}
			break;
		default:
			break;
		}

		switch (TargetMode)
		{
		case EAvatarRenderMode::Mode3D:
			if (Mesh3D)
			{
				Mesh3D->SetVisibility(true);
				// Mesh3D->SetScalarParameterValueOnMaterials(TEXT("Opacity"), Alpha);
			}
			break;
		case EAvatarRenderMode::Mode2D:
			if (Avatar2D)
			{
				Avatar2D->SetVisibility(true);
				Avatar2D->SetOpacity(Alpha);
			}
			break;
		default:
			break;
		}
	}
}

void UAvatarSwitchingSystem::ApplySyncStateTo3D()
{
	if (!Mesh3D)
	{
		return;
	}

	// Apply morph targets
	Mesh3D->SetMorphTarget(TEXT("EyeBlinkLeft"), 1.0f - CurrentSyncState.EyeOpenLeft);
	Mesh3D->SetMorphTarget(TEXT("EyeBlinkRight"), 1.0f - CurrentSyncState.EyeOpenRight);
	Mesh3D->SetMorphTarget(TEXT("JawOpen"), CurrentSyncState.MouthOpen);

	if (CurrentSyncState.MouthForm >= 0.0f)
	{
		Mesh3D->SetMorphTarget(TEXT("MouthSmile"), CurrentSyncState.MouthForm);
		Mesh3D->SetMorphTarget(TEXT("MouthFrown"), 0.0f);
	}
	else
	{
		Mesh3D->SetMorphTarget(TEXT("MouthSmile"), 0.0f);
		Mesh3D->SetMorphTarget(TEXT("MouthFrown"), -CurrentSyncState.MouthForm);
	}

	Mesh3D->SetMorphTarget(TEXT("BrowInnerUp_L"), CurrentSyncState.BrowLeftY);
	Mesh3D->SetMorphTarget(TEXT("BrowInnerUp_R"), CurrentSyncState.BrowRightY);

	// Apply blush through cosmetics component
	if (CosmeticsComponent)
	{
		CosmeticsComponent->SetBlushIntensity(CurrentSyncState.Blush);
	}
}

void UAvatarSwitchingSystem::ApplySyncStateTo2D()
{
	if (!Avatar2D)
	{
		return;
	}

	// Apply parameters to Live2D model
	Avatar2D->SetParameterValue(TEXT("ParamEyeLOpen"), CurrentSyncState.EyeOpenLeft);
	Avatar2D->SetParameterValue(TEXT("ParamEyeROpen"), CurrentSyncState.EyeOpenRight);
	Avatar2D->SetParameterValue(TEXT("ParamMouthOpenY"), CurrentSyncState.MouthOpen);
	Avatar2D->SetParameterValue(TEXT("ParamMouthForm"), CurrentSyncState.MouthForm);
	Avatar2D->SetParameterValue(TEXT("ParamBrowLY"), CurrentSyncState.BrowLeftY);
	Avatar2D->SetParameterValue(TEXT("ParamBrowRY"), CurrentSyncState.BrowRightY);
	Avatar2D->SetParameterValue(TEXT("ParamEyeBallX"), CurrentSyncState.EyeGaze.X);
	Avatar2D->SetParameterValue(TEXT("ParamEyeBallY"), CurrentSyncState.EyeGaze.Y);
	Avatar2D->SetParameterValue(TEXT("ParamCheek"), CurrentSyncState.Blush);

	// Apply rotations
	Avatar2D->SetParameterValue(TEXT("ParamAngleX"), CurrentSyncState.HeadRotation.Yaw);
	Avatar2D->SetParameterValue(TEXT("ParamAngleY"), CurrentSyncState.HeadRotation.Pitch);
	Avatar2D->SetParameterValue(TEXT("ParamAngleZ"), CurrentSyncState.HeadRotation.Roll);
	Avatar2D->SetParameterValue(TEXT("ParamBodyAngleX"), CurrentSyncState.BodyRotation.Yaw);
	Avatar2D->SetParameterValue(TEXT("ParamBodyAngleY"), CurrentSyncState.BodyRotation.Pitch);
	Avatar2D->SetParameterValue(TEXT("ParamBodyAngleZ"), CurrentSyncState.BodyRotation.Roll);

	Avatar2D->SetParameterValue(TEXT("ParamBreath"), CurrentSyncState.BreathValue);
}

void UAvatarSwitchingSystem::UpdatePerformanceTracking(float DeltaTime)
{
	// Store frame time
	FrameTimeHistory[FrameTimeHistoryIndex] = DeltaTime;
	FrameTimeHistoryIndex = (FrameTimeHistoryIndex + 1) % FrameTimeHistory.Num();
}

void UAvatarSwitchingSystem::CheckAutoModeSwitch()
{
	float AverageFrameTime = GetAverageFrameTime();
	float CurrentFPS = 1.0f / FMath::Max(AverageFrameTime, 0.001f);

	EAvatarRenderMode OptimalMode = SelectOptimalMode();

	if (OptimalMode != CurrentMode)
	{
		TransitionToMode(OptimalMode, 1.0f);
	}
}

void UAvatarSwitchingSystem::SetComponentVisibility(EAvatarRenderMode Mode, float Alpha)
{
	switch (Mode)
	{
	case EAvatarRenderMode::Mode3D:
		if (Mesh3D)
		{
			Mesh3D->SetVisibility(true);
		}
		if (Avatar2D)
		{
			Avatar2D->SetVisibility(false);
		}
		break;

	case EAvatarRenderMode::Mode2D:
		if (Mesh3D)
		{
			Mesh3D->SetVisibility(false);
		}
		if (Avatar2D)
		{
			Avatar2D->SetVisibility(true);
		}
		break;

	case EAvatarRenderMode::ModeHybrid:
		if (Mesh3D)
		{
			Mesh3D->SetVisibility(true);
		}
		if (Avatar2D)
		{
			Avatar2D->SetVisibility(true);
		}
		break;

	case EAvatarRenderMode::ModeMinimal:
		if (Mesh3D)
		{
			Mesh3D->SetVisibility(false);
		}
		if (Avatar2D)
		{
			Avatar2D->SetVisibility(false);
		}
		break;
	}
}

float UAvatarSwitchingSystem::GetAverageFrameTime() const
{
	float Total = 0.0f;
	for (float FrameTime : FrameTimeHistory)
	{
		Total += FrameTime;
	}
	return Total / FrameTimeHistory.Num();
}

EAvatarRenderMode UAvatarSwitchingSystem::SelectOptimalMode() const
{
	float AverageFrameTime = GetAverageFrameTime();
	float CurrentFPS = 1.0f / FMath::Max(AverageFrameTime, 0.001f);

	// If performance is good, prefer 3D
	if (CurrentFPS >= PerformanceTargetFPS)
	{
		return EAvatarRenderMode::Mode3D;
	}
	// If performance is moderate, use 2D
	else if (CurrentFPS >= PerformanceTargetFPS * 0.5f)
	{
		return EAvatarRenderMode::Mode2D;
	}
	// If performance is poor, use minimal
	else
	{
		return EAvatarRenderMode::ModeMinimal;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - 2D/3D Avatar Switching System

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AvatarSwitchingSystem.generated.h"

class USkeletalMeshComponent;
class ULive2DCubismAvatarComponent;
class UCubismModelWrapper;
class UDeepTreeEchoCosmeticsComponent;

/**
 * EAvatarRenderMode
 * 
 * The rendering mode for the avatar.
 */
UENUM(BlueprintType)
enum class EAvatarRenderMode : uint8
{
	Mode3D,         // Full 3D skeletal mesh
	Mode2D,         // Live2D Cubism 2D avatar
	ModeHybrid,     // 3D body with 2D face overlay
	ModeMinimal     // Simplified representation for performance
};

/**
 * FAvatarSyncState
 * 
 * Synchronized state between 2D and 3D avatars.
 */
USTRUCT(BlueprintType)
struct FAvatarSyncState
{
	GENERATED_BODY()

	// Facial expression
	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float EyeOpenLeft;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float EyeOpenRight;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float MouthOpen;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float MouthForm; // -1 = frown, 1 = smile

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float BrowLeftY;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float BrowRightY;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	FVector2D EyeGaze;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float Blush;

	// Head/Body orientation
	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	FRotator HeadRotation;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	FRotator BodyRotation;

	// Breathing
	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float BreathValue;

	// Emotional state
	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float EmotionalValence;

	UPROPERTY(BlueprintReadWrite, Category = "Sync")
	float EmotionalArousal;

	FAvatarSyncState()
		: EyeOpenLeft(1.0f)
		, EyeOpenRight(1.0f)
		, MouthOpen(0.0f)
		, MouthForm(0.0f)
		, BrowLeftY(0.0f)
		, BrowRightY(0.0f)
		, EyeGaze(FVector2D::ZeroVector)
		, Blush(0.0f)
		, HeadRotation(FRotator::ZeroRotator)
		, BodyRotation(FRotator::ZeroRotator)
		, BreathValue(0.0f)
		, EmotionalValence(0.5f)
		, EmotionalArousal(0.5f)
	{}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAvatarModeChanged, EAvatarRenderMode, OldMode, EAvatarRenderMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvatarSyncStateUpdated, const FAvatarSyncState&, State);

/**
 * UAvatarSwitchingSystem
 *
 * Manages switching between 2D and 3D avatar representations while maintaining
 * synchronized state. Enables seamless transitions and hybrid rendering modes.
 *
 * Key Features:
 * - Seamless 2D/3D mode switching
 * - State synchronization between representations
 * - Hybrid mode for 3D body with 2D face
 * - Performance-based automatic mode selection
 * - Transition animations and effects
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class UNREALENGINE_API UAvatarSwitchingSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UAvatarSwitchingSystem();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Initialize with avatar components
	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void Initialize(USkeletalMeshComponent* In3DMesh, ULive2DCubismAvatarComponent* In2DAvatar);

	// Mode switching
	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void SetRenderMode(EAvatarRenderMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Avatar|Switching")
	EAvatarRenderMode GetCurrentRenderMode() const { return CurrentMode; }

	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void TransitionToMode(EAvatarRenderMode NewMode, float TransitionTime = 0.5f);

	UFUNCTION(BlueprintPure, Category = "Avatar|Switching")
	bool IsTransitioning() const { return bIsTransitioning; }

	// State synchronization
	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void SetSyncState(const FAvatarSyncState& State);

	UFUNCTION(BlueprintPure, Category = "Avatar|Switching")
	FAvatarSyncState GetSyncState() const { return CurrentSyncState; }

	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void SyncFrom3D();

	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void SyncFrom2D();

	// Performance management
	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void SetAutoModeSelection(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void SetPerformanceTarget(float TargetFPS);

	// Live2D model management
	UFUNCTION(BlueprintCallable, Category = "Avatar|Switching")
	void LoadLive2DModel(const FString& ModelPath);

	UFUNCTION(BlueprintPure, Category = "Avatar|Switching")
	bool IsLive2DModelLoaded() const;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Avatar|Switching")
	FOnAvatarModeChanged OnAvatarModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Avatar|Switching")
	FOnAvatarSyncStateUpdated OnAvatarSyncStateUpdated;

protected:
	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Switching|Config")
	EAvatarRenderMode DefaultMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Switching|Config")
	bool bAutoSelectMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Switching|Config")
	float PerformanceTargetFPS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Switching|Config")
	float ModeSwitchCooldown;

	// References
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh3D;

	UPROPERTY()
	TObjectPtr<ULive2DCubismAvatarComponent> Avatar2D;

	UPROPERTY()
	TObjectPtr<UCubismModelWrapper> CubismModel;

	UPROPERTY()
	TObjectPtr<UDeepTreeEchoCosmeticsComponent> CosmeticsComponent;

	// State
	UPROPERTY()
	EAvatarRenderMode CurrentMode;

	UPROPERTY()
	EAvatarRenderMode TargetMode;

	UPROPERTY()
	FAvatarSyncState CurrentSyncState;

	UPROPERTY()
	bool bIsTransitioning;

	UPROPERTY()
	float TransitionProgress;

	UPROPERTY()
	float TransitionDuration;

	UPROPERTY()
	float TimeSinceLastModeSwitch;

	// Performance tracking
	UPROPERTY()
	TArray<float> FrameTimeHistory;

	UPROPERTY()
	int32 FrameTimeHistoryIndex;

	// Internal functions
	void UpdateTransition(float DeltaTime);
	void ApplySyncStateTo3D();
	void ApplySyncStateTo2D();
	void UpdatePerformanceTracking(float DeltaTime);
	void CheckAutoModeSwitch();
	void SetComponentVisibility(EAvatarRenderMode Mode, float Alpha = 1.0f);
	float GetAverageFrameTime() const;
	EAvatarRenderMode SelectOptimalMode() const;
};

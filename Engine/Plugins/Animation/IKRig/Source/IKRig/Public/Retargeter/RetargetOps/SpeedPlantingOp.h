// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Retargeter/IKRetargetOps.h"
#include "Kismet/KismetMathLibrary.h"

#include "SpeedPlantingOp.generated.h"

#define UE_API IKRIG_API

#define LOCTEXT_NAMESPACE "SpeedPlantingOp"

USTRUCT(BlueprintType)
struct FRetargetSpeedPlantingSettings
{
	GENERATED_BODY()
	
	FRetargetSpeedPlantingSettings() = default;
	FRetargetSpeedPlantingSettings(const FName InTargetChainName) : TargetChainName(InTargetChainName) {}

	/** The name of the target chain to plant the IK on.
	 * NOTE: this chain must have an IK Goal assigned to it! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Planting", meta=(ReinitializeOnEdit))
	FName TargetChainName;

	/** The name of the curve on the source animation that contains the speed of the end effector bone.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintReadWrite, Category = "Plant IK by Speed", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	FName SpeedCurveName;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Speed Plant Settings"))
struct FIKRetargetSpeedPlantingOpSettings : public FIKRetargetOpSettingsBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup", meta=(ReinitializeOnEdit))
	TArray<FRetargetSpeedPlantingSettings> ChainsToSpeedPlant;

	/** Range 0 to 100. Default 15. The maximum speed a source bone can be moving while being considered 'planted'.
	*  The target IK goal will not be allowed to move whenever the source bone speed drops below this threshold speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Planting", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	double SpeedThreshold = 15.0f;

	// How stiff the spring model is that smoothly pulls the IK position after unplanting (more stiffness means more oscillation around the target value)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Planting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	double Stiffness = 250.0f;

	// How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Planting", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	double CriticalDamping = 1.0f;

	UE_API virtual const UClass* GetControllerType() const override;

	UE_API virtual void CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom) override;
};

struct FSpeedPlantingGoalState
{
	FSpeedPlantingGoalState(
		const FName InGoalName,
		const FRetargetSpeedPlantingSettings* InSettings,
		const FVector& InInitialGoalPosition) :
		GoalName(InGoalName),
		Settings(InSettings),
		PrevGoalPosition(InInitialGoalPosition) {};
	
	FName GoalName;
	const FRetargetSpeedPlantingSettings *Settings;
	FVectorSpringState PositionSpring;
	FVector PrevGoalPosition;
	double CurrentSpeedValue = -1.0f;

#if WITH_EDITOR
	bool bFoundCurveInSourceComponent = false;
	bool bFoundCurveInTargetComponent = false;
#endif
};

USTRUCT(BlueprintType, meta = (DisplayName = "Speed Plant IK Goals"))
struct FIKRetargetSpeedPlantingOp : public FIKRetargetOpBase
{
	GENERATED_BODY()
	
	UE_API virtual bool Initialize(
		const FIKRetargetProcessor& InProcessor,
		const FRetargetSkeleton& InSourceSkeleton,
		const FTargetSkeleton& InTargetSkeleton,
		const FIKRetargetOpBase* InParentOp,
		FIKRigLogger& InLog) override;
	
	UE_API virtual void Run(
		FIKRetargetProcessor& InProcessor,
		const double InDeltaTime,
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose) override;

	UE_API virtual void OnAddedToStack(const UIKRetargeter* InRetargetAsset, const FIKRetargetOpBase* InParentOp) override;

	UE_API virtual FIKRetargetOpSettingsBase* GetSettings() override;
	
	UE_API virtual const UScriptStruct* GetSettingsType() const override;
	
	UE_API virtual const UScriptStruct* GetType() const override;

	UE_API virtual void OnPlaybackReset() override;

	UE_API virtual void AnimGraphPreUpdateMainThread(USkeletalMeshComponent& SourceMeshComponent, USkeletalMeshComponent& TargetMeshComponent) override;

	UE_API virtual void AnimGraphEvaluateAnyThread(FPoseContext& Output) override;

	UE_API virtual const UScriptStruct* GetParentOpType() const override;

	UE_API virtual void OnTargetChainRenamed(const FName InOldChainName, const FName InNewChainName) override;

	UE_API virtual void OnParentReinitPropertyEdited(const FIKRetargetOpBase& InParentOp, const FPropertyChangedEvent* InPropertyChangedEvent) override;

	/** return array of names of all the speed curves referenced by this op. */
	UE_API TArray<FName> GetRequiredSpeedCurves() const;

	UPROPERTY()
	FIKRetargetSpeedPlantingOpSettings Settings;

#if WITH_EDITOR
public:
	UE_API virtual FText GetWarningMessage() const override;
#endif

	bool ResetThisTick;

private:
	
	void RegenerateChainSettings(const FIKRetargetOpBase* InParentOp);
	
	TArray<FSpeedPlantingGoalState> GoalsToPlant;
};

/* The blueprint/python API for editing a Speed Planting Op */
UCLASS(MinimalAPI, BlueprintType)
class UIKRetargetSpeedPlantingController : public UIKRetargetOpControllerBase
{
	GENERATED_BODY()
	
public:
	/* Get the current op settings as a struct.
	 * @return FIKRetargetSpeedPlantingOpSettings struct with the current settings used by the op. */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API FIKRetargetSpeedPlantingOpSettings GetSettings();

	/* Set the op settings. Input is a custom struct type for this op.
	 * @param InSettings a FIKRetargetSpeedPlantingOpSettings struct containing all the settings to apply to this op */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API void SetSettings(FIKRetargetSpeedPlantingOpSettings InSettings);
};

#undef LOCTEXT_NAMESPACE

#undef UE_API

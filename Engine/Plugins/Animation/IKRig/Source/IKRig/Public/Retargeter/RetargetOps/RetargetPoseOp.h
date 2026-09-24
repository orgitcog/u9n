// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Retargeter/IKRetargetOps.h"
#include "Retargeter/IKRetargetSettings.h"

#include "RetargetPoseOp.generated.h"

#define LOCTEXT_NAMESPACE "RetargetPoseOp"

USTRUCT(BlueprintType, meta = (DisplayName = "Additive Pose Op Settings"))
struct IKRIG_API FIKRetargetAdditivePoseOpSettings : public FIKRetargetOpSettingsBase
{
	GENERATED_BODY()
	
	// a retarget pose that is applied additively to the output pose of the target skeleton
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose")
	FName PoseToApply;

	// blend the amount of the pose to apply
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose", meta=(ClampMin=0, ClampMax=1))
	float Alpha = 1.0f;
	
	virtual const UClass* GetControllerType() const override;
	
	virtual void CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom) override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Additive Pose"))
struct IKRIG_API FIKRetargetAdditivePoseOp : public FIKRetargetOpBase
{
	GENERATED_BODY()
	
	virtual bool Initialize(
		const FIKRetargetProcessor& InProcessor,
		const FRetargetSkeleton& InSourceSkeleton,
		const FTargetSkeleton& InTargetSkeleton,
		const FIKRetargetOpBase* InParentOp,
		FIKRigLogger& Log) override;
	
	virtual void Run(
		FIKRetargetProcessor& InProcessor,
		const double InDeltaTime,
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose) override;

	virtual FIKRetargetOpSettingsBase* GetSettings() override;

	virtual void SetSettings(const FIKRetargetOpSettingsBase* InSettings) override;
	
	virtual const UScriptStruct* GetSettingsType() const override;
	
	virtual const UScriptStruct* GetType() const override;

private:
	
	void ApplyAdditivePose(FIKRetargetProcessor& InProcessor, TArray<FTransform>& OutTargetGlobalPose);

	// cached in Initialize()
	FName PelvisBoneName;

	UPROPERTY()
	FIKRetargetAdditivePoseOpSettings Settings;
};

/* The blueprint/python API for editing a Retarget Pose Op */
UCLASS(BlueprintType)
class IKRIG_API UIKRetargetAdditivePoseController : public UIKRetargetOpControllerBase
{
	GENERATED_BODY()
	
public:
	/* Get the current op settings as a struct.
	 * @return FIKRetargetPoseOpSettings struct with the current settings used by the op. */
	UFUNCTION(BlueprintCallable, Category = Settings)
	FIKRetargetAdditivePoseOpSettings GetSettings();

	/* Set the op settings. Input is a custom struct type for this op.
	 * @param InSettings a FIKRetargetPoseOpSettings struct containing all the settings to apply to this op */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetSettings(FIKRetargetAdditivePoseOpSettings InSettings);
};

#undef LOCTEXT_NAMESPACE

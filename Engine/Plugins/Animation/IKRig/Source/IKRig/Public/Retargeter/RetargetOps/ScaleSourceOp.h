// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Retargeter/IKRetargetOps.h"

#include "ScaleSourceOp.generated.h"

#define UE_API IKRIG_API

#define LOCTEXT_NAMESPACE "ScaleSourceOp"

USTRUCT(BlueprintType, meta = (DisplayName = "Scale Source Settings"))
struct FIKRetargetScaleSourceOpSettings : public FIKRetargetOpSettingsBase
{
	GENERATED_BODY()
	
	/** Range 0.01 to +inf. Default 1. Scales the incoming source pose. Affects entire skeleton and all IK goals.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Scale, meta = (ReinitializeOnEdit, ClampMin = "0.01", UIMin = "0.01", UIMax = "10.0"))
	double SourceScaleFactor = 1.0f;

	UE_API virtual const UClass* GetControllerType() const override;

	UE_API virtual void CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom) override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Scale Source"))
struct FIKRetargetScaleSourceOp : public FIKRetargetOpBase
{
	GENERATED_BODY()
	
	// NOTE: this op does not do anything in Initialize() or Run().
	// It is a special case op that the retargeter reads from when it needs to scale the source pose.
	
	UE_API virtual bool Initialize(
		const FIKRetargetProcessor& InProcessor,
		const FRetargetSkeleton& InSourceSkeleton,
		const FTargetSkeleton& InTargetSkeleton,
		const FIKRetargetOpBase* InParentOp,
		FIKRigLogger& Log) override;
	
	virtual void Run(
		FIKRetargetProcessor& InProcessor,
		const double InDeltaTime,
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose) override {};

	UE_API virtual FIKRetargetOpSettingsBase* GetSettings() override;
	
	UE_API virtual const UScriptStruct* GetSettingsType() const override;
	
	UE_API virtual const UScriptStruct* GetType() const override;

	virtual bool IsSingleton() const override { return true; };

	UPROPERTY()
	FIKRetargetScaleSourceOpSettings Settings;
};

/* The blueprint/python API for editing a Scale Source Op */
UCLASS(MinimalAPI, BlueprintType)
class UIKRetargetScaleSourceController : public UIKRetargetOpControllerBase
{
	GENERATED_BODY()
	
public:
	/* Get the current op settings as a struct.
	 * @return FIKRetargetScaleSourceOpSettings struct with the current settings used by the op. */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API FIKRetargetScaleSourceOpSettings GetSettings();

	/* Set the op settings. Input is a custom struct type for this op.
	 * @param InSettings a FIKRetargetScaleSourceOpSettings struct containing all the settings to apply to this op */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API void SetSettings(FIKRetargetScaleSourceOpSettings InSettings);
};

#undef LOCTEXT_NAMESPACE

#undef UE_API

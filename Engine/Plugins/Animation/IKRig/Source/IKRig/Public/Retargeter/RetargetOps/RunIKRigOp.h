// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Retargeter/IKRetargetChainMapping.h"
#include "Retargeter/IKRetargetOps.h"
#include "Rig/IKRigProcessor.h"

#include "RunIKRigOp.generated.h"

#define UE_API IKRIG_API

#define LOCTEXT_NAMESPACE "RunIKRigOp"

USTRUCT(BlueprintType, meta = (DisplayName = "Solve IK Goal Settings"))
struct FIKRetargetRunIKRigOpSettings : public FIKRetargetOpSettingsBase
{
	GENERATED_BODY()

	/** The IK Rig asset to run when this op is executed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Rig Asset", meta=(ReinitializeOnEdit))
	TObjectPtr<const UIKRigDefinition> IKRigAsset = nullptr;

	/** Goals in this list will be excluded from the rig */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK Rig Asset")
	TArray<FName> ExcludedGoals;
	
	// Draw IK goal locations. 
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDrawGoals = true;

	// Draw locations of the source bone (pre-solve)
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDrawGoalBoneLocations = true;

	// Adjust size of goal debug drawing in viewport
	UPROPERTY(EditAnywhere, Category = Debug)
	double GoalDrawSize = 5.0;
	
	// Adjust thickness of goal debug drawing in viewport
	UPROPERTY(EditAnywhere, Category = Debug)
	double GoalDrawThickness = 1.0;

	UE_API virtual const UClass* GetControllerType() const override;

	UE_API virtual void CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom) override;
};

#if WITH_EDITOR
struct FRunIKRigOpGoalDebugData
{
	FName GoalName;
	FTransform InitialGoal;
	FTransform CurrentGoal;
};
#endif

USTRUCT(BlueprintType, meta = (DisplayName = "Run IK Rig"))
struct FIKRetargetRunIKRigOp : public FIKRetargetOpBase
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
	
	virtual bool CanHaveChildOps() const override { return true; };

	UE_API virtual const UIKRigDefinition* GetCustomTargetIKRig() const override;

	UE_API virtual FRetargetChainMapping* GetChainMapping() override;
	
	UE_API virtual void OnReinitPropertyEdited(const FPropertyChangedEvent* InPropertyChangedEvent) override;

	UPROPERTY()
	FIKRetargetRunIKRigOpSettings Settings;
	
	/* This op maintains its own chain mapping table to allow per-op mapping */
	UPROPERTY()
	FRetargetChainMapping ChainMapping;

#if WITH_EDITOR
	UE_API virtual void DebugDraw(
		FPrimitiveDrawInterface* InPDI,
		const FTransform& InComponentTransform,
		const double InComponentScale,
		const FIKRetargetDebugDrawState& InEditorState) const override;
	UE_API void SaveInitialGoalTransformsIntoDebugData(const FIKRetargetProcessor& InProcessor, const TArray<FTransform>& InTargetGlobalPose);
	UE_API void SaveCurrentGoalTransformsIntoDebugData();
	TArray<FRunIKRigOpGoalDebugData> GoalDebugData;
	static UE_API FCriticalSection DebugDataMutex;
#endif

	UE_API TArray<FName> GetRequiredTargetChains() const;
	
private:

	/** The IK Rig processor for running IK on the target */
	FIKRigProcessor IKRigProcessor;
};

/* The blueprint/python API for editing a Run IK Rig Op */
UCLASS(MinimalAPI, BlueprintType)
class UIKRetargetRunIKRigController : public UIKRetargetOpControllerBase
{
	GENERATED_BODY()
	
public:
	/* Get the current op settings as a struct.
	 * @return FIKRetargetRunIKRigOpSettings struct with the current settings used by the op. */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API FIKRetargetRunIKRigOpSettings GetSettings();

	/* Set the op settings. Input is a custom struct type for this op.
	 * @param InSettings a FIKRetargetRunIKRigOpSettings struct containing all the settings to apply to this op */
	UFUNCTION(BlueprintCallable, Category = Settings)
	UE_API void SetSettings(FIKRetargetRunIKRigOpSettings InSettings);
};

#undef LOCTEXT_NAMESPACE

#undef UE_API

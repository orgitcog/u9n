// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IKRetargetSettings.h"
#include "RetargetOps/StrideWarpingOp.h"

#include "IKRetargetDeprecated.generated.h"

#define UE_API IKRIG_API


enum class EWarpingDirectionSource;

//
// NOTE: These are legacy types that are exposed to UEFN and so cannot be actually deprecated.
//

// NOTE: Replaced by Speed Planting Op
USTRUCT(BlueprintType)
struct FTargetChainSpeedPlantSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool EnableSpeedPlanting = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FName SpeedCurveName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float SpeedThreshold = 15.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float UnplantStiffness = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float UnplantCriticalDamping = 1.0f;
};

// NOTE: Replaced by FK Chain Op
UENUM(BlueprintType)
enum class ERetargetTranslationMode : uint8
{
	None							UMETA(DisplayName = "None"),
	GloballyScaled					UMETA(DisplayName = "Globally Scaled"),
	Absolute						UMETA(DisplayName = "Absolute"),
	StretchBoneLengthUniformly		UMETA(DisplayName = "Stretch Bone Length Uniformly"),
	StretchBoneLengthNonUniformly	UMETA(DisplayName = "Stretch Bone Length Non-Uniformly"),
};

// NOTE: Replaced by FK Chain Op
UENUM(BlueprintType)
enum class ERetargetRotationMode : uint8
{
	Interpolated		UMETA(DisplayName = "Interpolated"),
	OneToOne			UMETA(DisplayName = "One to One"),
	OneToOneReversed	UMETA(DisplayName = "One to One Reversed"),
	MatchChain			UMETA(DisplayName = "Match Chain"),
	MatchScaledChain	UMETA(DisplayName = "Match Scaled Chain"),
	None				UMETA(DisplayName = "None"),
};

// NOTE: Replaced by FK Chain Op
USTRUCT(BlueprintType)
struct FTargetChainFKSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool EnableFK = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	ERetargetRotationMode RotationMode = ERetargetRotationMode::Interpolated;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float RotationAlpha = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	ERetargetTranslationMode TranslationMode = ERetargetTranslationMode::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float TranslationAlpha = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float PoleVectorMatching = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool PoleVectorMaintainOffset = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float PoleVectorOffset = 0.0f;
};

// NOTE: Replaced by IK Chain Op
USTRUCT(BlueprintType)
struct FTargetChainIKSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool EnableIK = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float BlendToSource = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float BlendToSourceTranslation = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float BlendToSourceRotation = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FVector BlendToSourceWeights = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FVector StaticOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FVector StaticLocalOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FRotator StaticRotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float ScaleVertical = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float Extension = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool bAffectedByIKWarping = true;
};

// NOTE: Replaced by FK/IK Chain Ops and Speed Planting Op
USTRUCT(BlueprintType)
struct FTargetChainSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FTargetChainFKSettings FK;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FTargetChainIKSettings IK;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FTargetChainSpeedPlantSettings SpeedPlanting;
};

// NOTE: Replaced by Pelvis Motion Op
USTRUCT(BlueprintType)
struct FTargetRootSettings
{
	GENERATED_BODY()
	public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float RotationAlpha = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float TranslationAlpha = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float BlendToSource = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FVector BlendToSourceWeights = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float ScaleHorizontal = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float ScaleVertical = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FVector TranslationOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FRotator RotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float AffectIKHorizontal = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float AffectIKVertical = 0.0f;
};

// NOTE: Replaced IK/FK/Post toggle flags with Op-enabled flags. Warping settings now in Stride Warp Op.
USTRUCT(BlueprintType)
struct FRetargetGlobalSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool bEnableRoot = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool bEnableFK = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool bEnableIK = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool bEnablePost = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool bCopyBasePose = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FName CopyBasePoseRoot = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float SourceScaleFactor = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	bool bWarping = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	EWarpingDirectionSource DirectionSource = EWarpingDirectionSource::Goals;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	EBasicAxis ForwardDirection = EBasicAxis::Y;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	FName DirectionChain;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float WarpForwards = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float SidewaysOffset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DeprecatedSettings, meta=(DeprecatedProperty))
	float WarpSplay = 1.0f;
};

// NOTE: Replaced with IK/FK Chains Op and new mapping data.
UCLASS(MinimalAPI, BlueprintType)
class URetargetChainSettings : public UObject
{
	GENERATED_BODY()

public:

	// UObject 
	UE_API virtual void Serialize(FArchive& Ar) override;
	// END UObject 
	
	//
	// Deprecated properties from 5.6 Op-stack refactor
	//
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(DeprecatedProperty))
	FName SourceChain;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(DeprecatedProperty))
	FName TargetChain;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(DeprecatedProperty))
	FTargetChainSettings Settings;

	//
	// Deprecated properties from before FTargetChainSettings / profile refactor  (July 2022)
	//
	
	UPROPERTY(meta=(DeprecatedProperty))
	bool CopyPoseUsingFK_DEPRECATED = true;
	UPROPERTY(meta=(DeprecatedProperty))
	ERetargetRotationMode RotationMode_DEPRECATED;
	UPROPERTY(meta=(DeprecatedProperty))
	float RotationAlpha_DEPRECATED = 1.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	ERetargetTranslationMode TranslationMode_DEPRECATED;
	UPROPERTY(meta=(DeprecatedProperty))
	float TranslationAlpha_DEPRECATED = 1.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	bool DriveIKGoal_DEPRECATED = true;
	UPROPERTY(meta=(DeprecatedProperty))
	float BlendToSource_DEPRECATED = 0.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	FVector BlendToSourceWeights_DEPRECATED = FVector::OneVector;
	UPROPERTY(meta=(DeprecatedProperty))
	FVector StaticOffset_DEPRECATED;
	UPROPERTY(meta=(DeprecatedProperty))
	FVector StaticLocalOffset_DEPRECATED;
	UPROPERTY(meta=(DeprecatedProperty))
	FRotator StaticRotationOffset_DEPRECATED;
	UPROPERTY(meta=(DeprecatedProperty))
	float Extension_DEPRECATED = 1.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	bool UseSpeedCurveToPlantIK_DEPRECATED = false;
	UPROPERTY(meta=(DeprecatedProperty))
	FName SpeedCurveName_DEPRECATED;
	UPROPERTY(meta=(DeprecatedProperty))
	float VelocityThreshold_DEPRECATED = 15.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	float UnplantStiffness_DEPRECATED = 250.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	float UnplantCriticalDamping_DEPRECATED = 1.0f;
	// END deprecated properties 
};

// NOTE: Replaced by Pelvis Op and it's settings.
UCLASS(MinimalAPI, BlueprintType)
class URetargetRootSettings: public UObject
{
	GENERATED_BODY()
	
public:

	UE_API virtual void Serialize(FArchive& Ar) override;

	// deprecated in 5.6 op refactor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(DeprecatedProperty))
	FTargetRootSettings Settings;
	// Deprecated properties from before FTargetRootSettings / profile refactor 
	UPROPERTY(meta=(DeprecatedProperty))
	bool RetargetRootTranslation_DEPRECATED = true;
	UPROPERTY(meta=(DeprecatedProperty))
	float GlobalScaleHorizontal_DEPRECATED = 1.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	float GlobalScaleVertical_DEPRECATED = 1.0f;
	UPROPERTY(meta=(DeprecatedProperty))
	FVector BlendToSource_DEPRECATED = FVector::ZeroVector;
	UPROPERTY(meta=(DeprecatedProperty))
	FVector StaticOffset_DEPRECATED = FVector::ZeroVector;
	UPROPERTY(meta=(DeprecatedProperty))
	FRotator StaticRotationOffset_DEPRECATED = FRotator::ZeroRotator;
	// END deprecated properties 
};

// NOTE: Phase toggles replaced by op enabled flags. Stride warp settings replaced by Stride Warp op.
UCLASS(MinimalAPI, BlueprintType)
class UIKRetargetGlobalSettings: public UObject
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(DeprecatedProperty))
	FRetargetGlobalSettings Settings;
};

#undef UE_API

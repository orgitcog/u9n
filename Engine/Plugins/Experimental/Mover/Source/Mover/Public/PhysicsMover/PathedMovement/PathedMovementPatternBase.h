// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PathedMovementTypes.h"

#include "PathedMovementPatternBase.generated.h"

class UPathedPhysicsMoverComponent;
struct FPhysicsMoverAsyncInput;
class UCurveFloat;
enum class EAlphaBlendOption : uint8;
class UPathedPhysicsMovementMode;
class UPathedPhysicsDebugDrawComponent;

//@todo DanH: What's the Flags specifier for? Does that do everything now or do I still need the Bitflags meta?
UENUM(Flags, meta = (Bitflags))
enum class EPatternAxisMaskFlags : uint8
{
	None = 0 UMETA(Hidden),

	X = 1 UMETA(DisplayName = "X / Roll"),
	Y = 1 << 1 UMETA(DisplayName = "Y / Pitch"),
	Z = 1 << 2 UMETA(DisplayName = "Z / Yaw)"),

	All = X | Y | Z UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EPatternAxisMaskFlags);

UCLASS(Abstract, Within = PathedPhysicsMovementMode, DefaultToInstanced, Blueprintable, BlueprintType, EditInlineNew)
class UPathedMovementPatternBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void InitializePattern();
	virtual void ProduceInputs_External(OUT FPhysicsMoverAsyncInput& Input);
	
	FTransform CalcTargetRelativeTransform(float OverallPathProgress, const FTransform& CurTargetTransform) const;

	UPathedPhysicsMovementMode& GetMovementMode() const;
	UPathedPhysicsMoverComponent& GetPathedMoverComp() const;

	virtual bool DebugDrawUsingStepSamples() const { return bDebugDrawPattern; }
	virtual void AppendDebugDrawElements(UPathedPhysicsDebugDrawComponent& DebugDrawComp, FBoxSphereBounds::Builder& InOutDebugBoundsBuilder) {}

#if WITH_EDITOR
	//@todo DanH: Pipe this through (I wonder if there should be a UPackage::IsDataValid() override that auto-pipes it to all UObjects in the package?)
	// virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	friend class FPatternPostChangeMovementModeHelper;
#endif
	
protected:
	/**
	* Where patterns calculate their target transforms at the given progress, optionally relative to the current aggregate target.
	* Axis masking is applied to this result before it's actually used.
	*/
	virtual FTransform CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const { return FTransform::Identity; }

	//
	// NOTE: Making any of these properties mutable at game time will require either replicating them (easier, but not as good) or pushing them as inputs to the PT (harder, more correct)
	//

	/** Along which axes is this pattern disallowed from modifying the translation/location of the updated component? */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (Bitmask, BitmaskEnum = "/Script/Mover.EPatternAxisMaskFlags"))
	uint8 TranslationMasks = (uint8)EPatternAxisMaskFlags::None;

	/** Along which axes is this pattern disallowed from modifying the rotation of the updated component? */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (Bitmask, BitmaskEnum = "/Script/Mover.EPatternAxisMaskFlags"))
	uint8 RotationMasks = (uint8)EPatternAxisMaskFlags::None;
	
	/** Along which axes is this pattern disallowed from modifying the scale of the updated component? */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (Bitmask, BitmaskEnum = "/Script/Mover.EPatternAxisMaskFlags"))
	uint8 ScaleMasks = (uint8)EPatternAxisMaskFlags::None;

	/**
	 * If true, this pattern will not begin to take effect until the previous pattern has completed.
	 * Note: If true and the previous pattern's EndAtPathProgress is 1, this pattern will never start.
	 */
	UPROPERTY(EditAnywhere, Category = "Path Pattern")
	bool bStartAfterPreviousPattern = false;

	// /**
	//  * The desired duration (in overall path progress) for this pattern once it begins.
	//  * If this desired duration is longer than the progress remaining when the pattern starts, it will be clamped to fill what remains
	//  */
	// UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (EditCondition = "bStartAfterPreviousPattern", UIMin = 0.f, ClampMin = 0.f, UIMax = 1.f, ClampMax = 1.f))
	// float DesiredPatternDuration = 1.f;

	/** The overall path progress when this pattern should begin, where 0 is the start of the path and 1 is the end. Must be less than EndAtProgress. */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (EditCondition = "!bStartAfterPreviousPattern", UIMin = 0.f, ClampMin = 0.f, UIMax = 1.f, ClampMax = 1.f))
	float StartAtPathProgress = 0.f;

	/** The overall path progress when this pattern should complete, where 0 is the start of the path and 1 is the end. Must be greater than StartAtProgress. */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (UIMin = 0.f, ClampMin = 0.f, UIMax = 1.f, ClampMax = 1.f))
	float EndAtPathProgress = 1.f;

	//@todo DanH: Maybe this should be a float and allow partial loops?
	/**
	 * The number of loops to complete within the active span of this pattern (i.e. between StartAtProgress and EndAtProgress)
	 * on a single run along the full aggregate path. Setting to 0 effectively disables this pattern.
	 */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (UIMin = 0, ClampMin = 0))
	int32 NumLoopsPerPath = 1;

	/** Playback behavior per loop of this pattern */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (InvalidEnumValues = "Looping,PingPong"))
	EPathedPhysicsPlaybackBehavior PerLoopBehavior = EPathedPhysicsPlaybackBehavior::OneShot;
	
	/** 
	 * If true, the component will be rotated to face in the direction of this pattern's motion.
	 * To have the component face in the direction of the aggregate path, enable this on all movement patterns.
	 */
	UPROPERTY(EditAnywhere, Category = "Path Pattern")
	bool bOrientComponentToPath = false;

	/** The kind of easing to apply when traveling along the path */
	UPROPERTY(EditAnywhere, Category = "Path Pattern")
	EAlphaBlendOption Easing;

	/** If using a custom ease, this is the curve that will be used. If blank, will fall back to standard linear interpolation. */
	UPROPERTY(EditAnywhere, Category = "Path Pattern", meta = (EditCondition = "Easing==EAlphaBlendOption::Custom"))
	TObjectPtr<UCurveFloat> CustomEasingCurve = nullptr;

	/** True to draw debug lines for this specific pattern in editor views */
	UPROPERTY(EditAnywhere, Category = "Path Pattern|Debug")
	bool bDebugDrawPattern = true;

	//@todo DanH: More central setting for color per pattern index maybe?
	/** The color used for debug draws of this pattern */
	UPROPERTY(EditAnywhere, Category = "Path Pattern|Debug", meta = (EditCondition = bDrawDebug))
	FColor PatternDebugDrawColor;

	friend class UPathedPhysicsDebugDrawComponent;
};

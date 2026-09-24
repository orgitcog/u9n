// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveFloat.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PoseSearchTrajectoryLibrary.generated.h"

#define UE_API POSESEARCH_API

class UAnimInstance;
class IPoseSearchTrajectoryPredictorInterface;
struct FTransformTrajectory;
struct FTransformTrajectorySample;

USTRUCT(BlueprintType)
struct FPoseSearchTrajectoryData
{
public:
	GENERATED_BODY()

	struct FState
	{
		float DesiredControllerYawLastUpdate = 0.f;
	};

	struct FDerived
	{
		float ControllerYawRate = 0.f;

		float MaxSpeed = 0.f;
		float BrakingDeceleration = 0.f;
		float BrakingSubStepTime = 0.f;
		float Friction = 0.f;

		FVector Velocity = FVector::ZeroVector;
		FVector Acceleration = FVector::ZeroVector;
		
		FVector Position = FVector::ZeroVector;
		FQuat Facing = FQuat::Identity;
		FQuat MeshCompRelativeRotation = FQuat::Identity;
		bool bOrientRotationToMovement = false;
		bool bStepGroundPrediction = true;
	};

	struct FSampling
	{
		int32 NumHistorySamples = 0;
		// if SecondsPerHistorySample <= 0, then we collect every update
		float SecondsPerHistorySample = 0.f;

		int32 NumPredictionSamples = 0;
		float SecondsPerPredictionSample = 0.f;
	};

	UE_API bool UpdateData(float DeltaTime, const FAnimInstanceProxy& AnimInstanceProxy, FDerived& TrajectoryDataDerived, FState& TrajectoryDataState) const;
	UE_API bool UpdateData(float DeltaTime, const UObject* Context, FDerived& TrajectoryDataDerived, FState& TrajectoryDataState) const;
	UE_API FVector StepCharacterMovementGroundPrediction(float DeltaTime, const FVector& InVelocity, const FVector& InAcceleration, const FDerived& TrajectoryDataDerived) const;
	
	// If the character is forward facing (i.e. bOrientRotationToMovement is true), this controls how quickly the trajectory will rotate
	// to face acceleration. It's common for this to differ from the rotation rate of the character, because animations are often authored 
	// with different rotation speeds than the character. This is especially true in cases where the character rotation snaps to movement.
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings")
	float RotateTowardsMovementSpeed = 10.f;

	// Maximum controller yaw  rate in degrees per second used to clamp the character owner controller desired yaw to generate the prediction trajectory.
	// Negative values disable the clamping behavior
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings")
	float MaxControllerYawRate = 70.f;

	// artificially bend character velocity towards acceleration direction to compute trajectory prediction, to get sharper turns
	// 0: character velocity is used with no alteration, 1: the acceleration direction is used as velocity direction
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float BendVelocityTowardsAcceleration = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (InlineEditConditionToggle))
	bool bUseSpeedRemappingCurve = false;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (EditCondition = "bUseSpeedRemappingCurve"))
	FRuntimeFloatCurve SpeedRemappingCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (InlineEditConditionToggle))
	bool bUseAccelerationRemappingCurve = false;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (EditCondition = "bUseAccelerationRemappingCurve"))
	FRuntimeFloatCurve AccelerationRemappingCurve;
};

USTRUCT(BlueprintType)
struct FPoseSearchTrajectory_WorldCollisionResults
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Trajectory Settings")
	float TimeToLand  = 0.0f;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Trajectory Settings")
	float LandSpeed  = 0.0f;
};

/**
 * Set of functions to help populate a FPoseSearchQueryTrajectory for motion matching.
 */
UCLASS(MinimalAPI)
class UPoseSearchTrajectoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	///////////////////////////////////////////////////////////////////////////
	// DEPRECATED /////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.5, "Use the InitTrajectorySamples function that takes DefaultPosition and DefaultFacing arguments instead of TrajectoryDataDerived")
	static UE_API void InitTrajectorySamples(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Initialize history and predicted samples based on sampling settings and a default state
	UE_DEPRECATED(5.6, "Use the InitTrajectorySamples function that doesn't take FPoseSearchTrajectoryData")
	static UE_API void InitTrajectorySamples(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, FVector DefaultPosition, FQuat DefaultFacing, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Initialize history and predicted samples based on sampling settings and a default state
	UE_DEPRECATED(5.6, "Use the IniTrajctorySamples function that takes in FTransformTrajectory instead of FPoseSearchQueryTrajectory.")
	static UE_API void InitTrajectorySamples(FPoseSearchQueryTrajectory& Trajectory, FVector DefaultPosition, FQuat DefaultFacing, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	UE_DEPRECATED(5.5, "Use the UpdateHistory_TransformHistory function that takes CurrentPosition and CurrentVelocity arguments instead of TrajectoryDataDerived")
	static UE_API void UpdateHistory_TransformHistory(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	UE_DEPRECATED(5.6, "Use the UpdateHistory_TransformHistory function that doesn't take FPoseSearchTrajectoryData")
	static UE_API void UpdateHistory_TransformHistory(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, FVector CurrentPosition, FVector CurrentVelocity, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);
	
	UE_DEPRECATED(5.6, "Use the UpdateHistory_TransformHistory function thats in FTransformTrajectory instead of FPoseSearchQueryTrajectory.")
	static UE_API void UpdateHistory_TransformHistory(FPoseSearchQueryTrajectory& Trajectory, FVector CurrentPosition, FVector CurrentVelocity, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	UE_DEPRECATED(5.6, "Use the UpdatePrediction_SimulateCharacterMovement function thats in FTransformTrajectory instead of FPoseSearchQueryTrajectory.")
	static UE_API void UpdatePrediction_SimulateCharacterMovement(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Generates a prediction trajectory based of the current character intent. For use with Character actors.
	UE_DEPRECATED(5.6, "Use PoseSearchGenerateTrajectory function that takes in FTransformTrajectory.")
	static UE_API void PoseSearchGenerateTrajectory(const UObject* InAnimInstance, 
		UPARAM(ref) const FPoseSearchTrajectoryData& InTrajectoryData, float InDeltaTime,
		UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory, UPARAM(ref) float& InOutDesiredControllerYawLastUpdate, FPoseSearchQueryTrajectory& OutTrajectory,
		float InHistorySamplingInterval = 0.04f, int32 InTrajectoryHistoryCount = 10, float InPredictionSamplingInterval = 0.2f, int32 InTrajectoryPredictionCount = 8);

	// Generates a prediction trajectory based of the current movement intent. For use with predictors. InPredictor must implement IPoseSearchTrajectoryPredictorInterface
	UE_DEPRECATED(5.6, "Use PoseSearchGenerateTrajectoryWithPredictor function that doesn't take FPoseSearchTrajectoryData and also takes in FTransformTrajectory")
	static UE_API void PoseSearchGeneratePredictorTrajectory(UObject* InPredictor,
		UPARAM(ref) const FPoseSearchTrajectoryData& InTrajectoryData, float InDeltaTime,
		UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory, UPARAM(ref) float& InOutDesiredControllerYawLastUpdate, FPoseSearchQueryTrajectory& OutTrajectory,
		float InHistorySamplingInterval = 0.04f, int32 InTrajectoryHistoryCount = 10, float InPredictionSamplingInterval = 0.2f, int32 InTrajectoryPredictionCount = 8);

	// Generates a prediction trajectory based of the current movement intent. For use with predictors. InPredictor must implement IPoseSearchTrajectoryPredictorInterface
	UE_DEPRECATED(5.6, "Use PoseSearchGenerateTrajectoryWithPredictor function that takes in FTransformTrajectory")
	static UE_API void PoseSearchGenerateTrajectoryWithPredictor(TScriptInterface<IPoseSearchTrajectoryPredictorInterface> InPredictor,
		float InDeltaTime,
		UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory, UPARAM(ref) float& InOutDesiredControllerYawLastUpdate, FPoseSearchQueryTrajectory& OutTrajectory,
		float InHistorySamplingInterval = 0.04f, int32 InTrajectoryHistoryCount = 10, float InPredictionSamplingInterval = 0.2f, int32 InTrajectoryPredictionCount = 8);

	UE_DEPRECATED(5.6, "Use HandleTrajectoryWorldCollisions function that takes in FTransformTrajectory")
	static UE_API void HandleTrajectoryWorldCollisions(const UObject* WorldContextObject, const UAnimInstance* AnimInstance, UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, bool bApplyGravity, float FloorCollisionsOffset, FPoseSearchQueryTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
		ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf = true, float MaxObstacleHeight = 10000.f, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	UE_DEPRECATED(5.6, "Use HandleTrajectoryWorldCollisionsWithGravity that takes in FTransformTrajectory.")
	static UE_API void HandleTrajectoryWorldCollisionsWithGravity(const UObject* WorldContextObject, 
		UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, FVector StartingVelocity, bool bApplyGravity, FVector GravityAccel, float FloorCollisionsOffset, FPoseSearchQueryTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
		ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf = true, float MaxObstacleHeight = 10000.f, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	UE_DEPRECATED(5.6, "Use GetTrajectorySampleAtTime that takes in FTransformTrajectory and FTransformTrajectorySample.")
	static UE_API void GetTrajectorySampleAtTime(UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, float Time, FPoseSearchQueryTrajectorySample& OutTrajectorySample, bool bExtrapolate = false);

	UE_DEPRECATED(5.6, "Use GetTrajectorySampleAtTime that takes in FTransformTrajectory.")
	static UE_API void GetTrajectoryVelocity(UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, float Time1, float Time2, FVector& OutVelocity, bool bExtrapolate = false);

	UE_DEPRECATED(5.6, "Use GetTrajectorySampleAtTime that takes in FTransformTrajectory.")
	static UE_API void GetTrajectoryAngularVelocity(UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, float Time1, float Time2, FVector& OutAngularVelocity, bool bExtrapolate = false);

	UE_DEPRECATED(5.6, "Use GetTrajectorySampleAtTime that takes in FTransformTrajectorySample.")
	static UE_API FTransform GetTransform(UPARAM(ref) const FPoseSearchQueryTrajectorySample& InTrajectorySample);

	UE_DEPRECATED(5.6, "Use DrawTrajectory that takes in FTransformTrajectory.")
	static UE_API void DrawTrajectory(const UObject* WorldContextObject, UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, const float DebugThickness, float HeightOffset);
	UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

	///////////////////////////////////////////////////////////////////////////
	// NON- DEPRECATED ////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	
	// Initialize history and predicted samples based on sampling settings and a default state
	static void InitTrajectorySamples(FTransformTrajectory& Trajectory, FVector DefaultPosition, FQuat DefaultFacing, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Update history by tracking offsets that result from character intent (e.g. movement component velocity) and applying
	// that to the current world transform. This works well on moving platforms as it only stores a history of movement
	// that results from character intent, not movement from platforms.
	// Important: CurrentVelocity should be the velocity relative to the ground as reported by the character movement component or character mover etc
	static UE_API void UpdateHistory_TransformHistory(FTransformTrajectory& Trajectory,
	                                           FVector CurrentPosition,
	                                           FVector CurrentVelocity,
	                                           const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	                                           float DeltaTime,
	                                           float CurrentTime);

	static FORCEINLINE void UpdateHistory_TransformHistory(FTransformTrajectory& Trajectory,
	                                                       FVector CurrentPosition,
	                                                       FVector CurrentVelocity,
	                                                       const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	                                                       float DeltaTime)
	{
		// Old behaviour: assume this is called after position has been updated (e.g. post Character Movement), so use DeltaTime for current time
		UpdateHistory_TransformHistory(Trajectory, CurrentPosition, CurrentVelocity, TrajectoryDataSampling, DeltaTime, DeltaTime);
	}

	// Update prediction by simulating the movement math for ground locomotion from UCharacterMovementComponent.
	static UE_API void UpdatePrediction_SimulateCharacterMovement(FTransformTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Experimental: Update the history purely based on current position, without taking into account ground velocities
	// DeltaTime is the time since the last call to this method
	UE_EXPERIMENTAL(5.6, "Experimental pose search function, API may change in the future")
	static UE_API void UpdateHistory_WorldSpace(FTransformTrajectory& Trajectory,
	                                     FVector CurrentPosition,
	                                     FQuat CurrentRotation,
	                                     const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	                                     float DeltaTime,
	                                     float CurrentTime);

	// Experimental: Update the history purely based on current position, without taking into account ground velocities
	UE_EXPERIMENTAL(5.6, "Experimental pose search function, API may change in the future")
	static void UpdateHistory_WorldSpace(FTransformTrajectory& Trajectory,
	                                     FVector CurrentPosition,
	                                     FQuat CurrentRotation,
	                                     const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	                                     float DeltaTime)
	{
		// Old behaviour: assume this is called after position has been updated (e.g. post CMC)
		PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
		UpdateHistory_WorldSpace(Trajectory, CurrentPosition, CurrentRotation, TrajectoryDataSampling, DeltaTime, DeltaTime);
		PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
	}

	// Generates a prediction trajectory based of the current character intent. For use with Character actors.
	UFUNCTION(BlueprintCallable, Category = "Animation|PoseSearch", meta = (BlueprintThreadSafe, DisplayName = "Pose Search Generate Trajectory (for Character)"))
	static UE_API void PoseSearchGenerateTransformTrajectory(const UObject* InAnimInstance, 
		UPARAM(ref) const FPoseSearchTrajectoryData& InTrajectoryData, float InDeltaTime,
		UPARAM(ref) FTransformTrajectory& InOutTrajectory, UPARAM(ref) float& InOutDesiredControllerYawLastUpdate, FTransformTrajectory& OutTrajectory,
		float InHistorySamplingInterval = 0.04f, int32 InTrajectoryHistoryCount = 10, float InPredictionSamplingInterval = 0.2f, int32 InTrajectoryPredictionCount = 8);

	// Generates a prediction trajectory based of the current movement intent. For use with predictors. InPredictor must implement IPoseSearchTrajectoryPredictorInterface
	UFUNCTION(BlueprintCallable, Category = "Animation|PoseSearch|Experimental", meta = (BlueprintThreadSafe, DisplayName = "Pose Search Generate Trajectory (using Predictor)", DeprecatedFunction, DeprecationMessage="Use PoseSearchGeneratePredictorTrajectory that does not take FPoseSearchTrajectoryData."))
	static UE_API void PoseSearchGeneratePredictorTransformTrajectory(UObject* InPredictor,
	UPARAM(ref) const FPoseSearchTrajectoryData& InTrajectoryData, float InDeltaTime,
	UPARAM(ref) FTransformTrajectory& InOutTrajectory, UPARAM(ref) float& InOutDesiredControllerYawLastUpdate, FTransformTrajectory& OutTrajectory,
	float InHistorySamplingInterval = 0.04f, int32 InTrajectoryHistoryCount = 10, float InPredictionSamplingInterval = 0.2f, int32 InTrajectoryPredictionCount = 8);
	
	// Generates a prediction trajectory based of the current movement intent. For use with predictors. InPredictor must implement IPoseSearchTrajectoryPredictorInterface
	UFUNCTION(BlueprintCallable, Category = "Animation|PoseSearch|Experimental", meta = (BlueprintThreadSafe, DisplayName = "Pose Search Generate Trajectory (using Predictor)"))
	static UE_API void PoseSearchGenerateTransformTrajectoryWithPredictor(TScriptInterface<IPoseSearchTrajectoryPredictorInterface> InPredictor,
		float InDeltaTime,
		UPARAM(ref) FTransformTrajectory& InOutTrajectory, UPARAM(ref) float& InOutDesiredControllerYawLastUpdate, FTransformTrajectory& OutTrajectory,
		float InHistorySamplingInterval = 0.04f, int32 InTrajectoryHistoryCount = 10, float InPredictionSamplingInterval = 0.2f, int32 InTrajectoryPredictionCount = 8);

	// Experimental: Process InTrajectory to apply gravity and handle collisions. Eventually returns the modified OutTrajectory.
	// If bApplyGravity is true, gravity from the UCharacterMovementComponent will be applied.
	// If FloorCollisionsOffset > 0, vertical collision will be performed to every sample of the trajectory to have the samples float over the geometry (by FloorCollisionsOffset).
	UE_EXPERIMENTAL(5.6, "Solution for CMC based workflow not Mover.")
	UFUNCTION(BlueprintCallable, Category="Animation|PoseSearch|Experimental", meta=(BlueprintThreadSafe, DisplayName="HandleTrajectoryWorldCollisions", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceChannel,bTraceComplex,ActorsToIgnore,DrawDebugType,bIgnoreSelf,MaxObstacleHeight,TraceColor,TraceHitColor,DrawTime"))
	static UE_API void HandleTransformTrajectoryWorldCollisions(const UObject* WorldContextObject, const UAnimInstance* AnimInstance, UPARAM(ref) const FTransformTrajectory& InTrajectory, bool bApplyGravity, float FloorCollisionsOffset, FTransformTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
		ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf = true, float MaxObstacleHeight = 10000.f, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	// Experimental: Process InTrajectory to apply gravity and handle collisions. Eventually returns the modified OutTrajectory.
	// If bApplyGravity is true, GravityAccel will be applied.
	// If FloorCollisionsOffset > 0, vertical collision will be performed to every sample of the trajectory to have the samples float over the geometry (by FloorCollisionsOffset).
	UE_EXPERIMENTAL(5.6, "Solution for CMC based workflow not Mover.")
	UFUNCTION(BlueprintCallable, Category="Animation|PoseSearch|Experimental", meta=(BlueprintThreadSafe, DisplayName="HandleTrajectoryWorldCollisionsWithGravity", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceChannel,bTraceComplex,ActorsToIgnore,DrawDebugType,bIgnoreSelf,MaxObstacleHeight,TraceColor,TraceHitColor,DrawTime"))
	static UE_API void HandleTransformTrajectoryWorldCollisionsWithGravity(const UObject* WorldContextObject, 
		UPARAM(ref) const FTransformTrajectory& InTrajectory, FVector StartingVelocity, bool bApplyGravity, FVector GravityAccel, float FloorCollisionsOffset, FTransformTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
		ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf = true, float MaxObstacleHeight = 10000.f, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	UFUNCTION(BlueprintPure, Category="Animation|PoseSearch", meta = (BlueprintThreadSafe, DisplayName="GetTrajectorySampleAtTime"))
	static UE_API void GetTransformTrajectorySampleAtTime(UPARAM(ref) const FTransformTrajectory& InTrajectory, float Time, FTransformTrajectorySample& OutTrajectorySample, bool bExtrapolate = false);
	
	UFUNCTION(BlueprintPure, Category="Animation|PoseSearch", meta = (BlueprintThreadSafe, DisplayName="GetTrajectoryVelocity"))
	static UE_API void GetTransformTrajectoryVelocity(UPARAM(ref) const FTransformTrajectory& InTrajectory, float Time1, float Time2, FVector& OutVelocity, bool bExtrapolate = false);
	
	UFUNCTION(BlueprintPure, Category="Animation|PoseSearch", meta = (BlueprintThreadSafe, DisplayName="GetTrajectoryAngularVelocity"))
	static UE_API void GetTransformTrajectoryAngularVelocity(UPARAM(ref) const FTransformTrajectory& InTrajectory, float Time1, float Time2, FVector& OutAngularVelocity, bool bExtrapolate = false);
	
	UFUNCTION(BlueprintPure, Category="Animation|PoseSearch", meta = (BlueprintThreadSafe, DisplayName="GetTrajectorySampleTransform"))
	static UE_API FTransform GetTransformTrajectorySampleTransform(UPARAM(ref) const FTransformTrajectorySample& InTrajectorySample);
	
	UFUNCTION(BlueprintCallable, Category="Animation|PoseSearch|Experimental", meta=(DisplayName="DrawTrajectory", WorldContext="WorldContextObject"))
	static UE_API void DrawTransformTrajectory(const UObject* WorldContextObject, UPARAM(ref) const FTransformTrajectory& InTrajectory, const float DebugThickness, float HeightOffset);
	
private:
	
	static UE_API FVector RemapVectorMagnitudeWithCurve(const FVector& Vector, bool bUseCurve, const FRuntimeFloatCurve& Curve);
};

#undef UE_API

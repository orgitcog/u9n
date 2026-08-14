// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryPredictor.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/TrajectoryTypes.h"

bool FPoseSearchTrajectoryData::UpdateData(
	float DeltaTime,
	const FAnimInstanceProxy& AnimInstanceProxy,
	FDerived& TrajectoryDataDerived,
	FState& TrajectoryDataState) const
{
	return UpdateData(DeltaTime, AnimInstanceProxy.GetAnimInstanceObject(), TrajectoryDataDerived, TrajectoryDataState);
}

bool FPoseSearchTrajectoryData::UpdateData(
	float DeltaTime,
	const UObject* Context,
	FDerived& TrajectoryDataDerived,
	FState& TrajectoryDataState) const
{
	const ACharacter* Character = Cast<ACharacter>(Context);
	if (!Character)
	{
		if (const UAnimInstance* AnimInstance = Cast<UAnimInstance>(Context))
		{
			Character = Cast<ACharacter>(AnimInstance->GetOwningActor());
		}
		else if (const UActorComponent* AnimNextComponent = Cast<UActorComponent>(Context))
		{
			Character = Cast<ACharacter>(AnimNextComponent->GetOwner());
		}
		
		if (!Character)
		{
			return false;
		}
	}

	const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	const USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MoveComp || !MeshComp)
	{
		return false;
	}

	TrajectoryDataDerived.MaxSpeed = FMath::Max(MoveComp->GetMaxSpeed() * MoveComp->GetAnalogInputModifier(), MoveComp->GetMinAnalogSpeed());
	TrajectoryDataDerived.BrakingDeceleration = FMath::Max(0.f, MoveComp->GetMaxBrakingDeceleration());
	TrajectoryDataDerived.BrakingSubStepTime = MoveComp->BrakingSubStepTime;
	TrajectoryDataDerived.bOrientRotationToMovement = MoveComp->bOrientRotationToMovement;

	TrajectoryDataDerived.Velocity = MoveComp->Velocity;
	TrajectoryDataDerived.Acceleration = MoveComp->GetCurrentAcceleration();
		
	TrajectoryDataDerived.bStepGroundPrediction = !MoveComp->IsFalling() && !MoveComp->IsFlying();

	if (TrajectoryDataDerived.Acceleration.IsZero())
	{
		TrajectoryDataDerived.Friction = MoveComp->bUseSeparateBrakingFriction ? MoveComp->BrakingFriction : MoveComp->GroundFriction;
		const float FrictionFactor = FMath::Max(0.f, MoveComp->BrakingFrictionFactor);
		TrajectoryDataDerived.Friction = FMath::Max(0.f, TrajectoryDataDerived.Friction * FrictionFactor);
	}
	else
	{
		TrajectoryDataDerived.Friction = MoveComp->GroundFriction;
	}

	const float DesiredControllerYaw = Character->GetViewRotation().Yaw;
		
	const float DesiredYawDelta = DesiredControllerYaw - TrajectoryDataState.DesiredControllerYawLastUpdate;
	TrajectoryDataState.DesiredControllerYawLastUpdate = DesiredControllerYaw;

	if (DeltaTime > UE_SMALL_NUMBER)
	{
		// An AnimInstance might call this during an AnimBP recompile with 0 delta time, so we don't update ControllerYawRate
		TrajectoryDataDerived.ControllerYawRate = FRotator::NormalizeAxis(DesiredYawDelta) / DeltaTime;
		if (MaxControllerYawRate >= 0.f)
		{
			TrajectoryDataDerived.ControllerYawRate = FMath::Sign(TrajectoryDataDerived.ControllerYawRate) * FMath::Min(FMath::Abs(TrajectoryDataDerived.ControllerYawRate), MaxControllerYawRate);
		}
	}

	TrajectoryDataDerived.Position = MeshComp->GetComponentLocation();
	TrajectoryDataDerived.MeshCompRelativeRotation = MeshComp->GetRelativeRotation().Quaternion();
		
	if (TrajectoryDataDerived.bOrientRotationToMovement)
	{
		TrajectoryDataDerived.Facing = MeshComp->GetComponentRotation().Quaternion();
	}
	else
	{
		TrajectoryDataDerived.Facing = FQuat::MakeFromRotator(FRotator(0,TrajectoryDataState.DesiredControllerYawLastUpdate,0)) * TrajectoryDataDerived.MeshCompRelativeRotation;
	}

	return true;
}

FVector FPoseSearchTrajectoryData::StepCharacterMovementGroundPrediction(
	float DeltaTime,
	const FVector& InVelocity,
	const FVector& InAcceleration,
	const FDerived& TrajectoryDataDerived) const
{
	FVector OutVelocity = InVelocity;

	// Braking logic is copied from UCharacterMovementComponent::ApplyVelocityBraking()
	if (InAcceleration.IsZero())
	{
		if (InVelocity.IsZero())
		{
			return FVector::ZeroVector;
		}

		const bool bZeroFriction = (TrajectoryDataDerived.Friction == 0.f);
		const bool bZeroBraking = (TrajectoryDataDerived.BrakingDeceleration == 0.f);

		if (bZeroFriction && bZeroBraking)
		{
			return InVelocity;
		}

		float RemainingTime = DeltaTime;
		const float MaxTimeStep = FMath::Clamp(TrajectoryDataDerived.BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

		const FVector PrevLinearVelocity = OutVelocity;
		const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-TrajectoryDataDerived.BrakingDeceleration * OutVelocity.GetSafeNormal()));

		// Decelerate to brake to a stop
		while (RemainingTime >= UCharacterMovementComponent::MIN_TICK_TIME)
		{
			// Zero friction uses constant deceleration, so no need for iteration.
			const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
			RemainingTime -= dt;

			// apply friction and braking
			OutVelocity = OutVelocity + ((-TrajectoryDataDerived.Friction) * OutVelocity + RevAccel) * dt;

			// Don't reverse direction
			if ((OutVelocity | PrevLinearVelocity) <= 0.f)
			{
				OutVelocity = FVector::ZeroVector;
				return OutVelocity;
			}
		}

		// Clamp to zero if nearly zero, or if below min threshold and braking
		const float VSizeSq = OutVelocity.SizeSquared();
		if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(UCharacterMovementComponent::BRAKE_TO_STOP_VELOCITY)))
		{
			OutVelocity = FVector::ZeroVector;
		}
	}
	// Acceleration logic is copied from  UCharacterMovementComponent::CalcVelocity
	else
	{
		const FVector AccelDir = InAcceleration.GetSafeNormal();
		const float VelSize = OutVelocity.Size();

		OutVelocity = OutVelocity - (OutVelocity - AccelDir * VelSize) * FMath::Min(DeltaTime * TrajectoryDataDerived.Friction, 1.f);

		OutVelocity += InAcceleration * DeltaTime;
		OutVelocity = OutVelocity.GetClampedToMaxSize(TrajectoryDataDerived.MaxSpeed);
	}

	return OutVelocity;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::InitTrajectorySamples(
	FPoseSearchQueryTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData,
	const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	InitTrajectorySamples(
		Trajectory,
		TrajectoryDataDerived.Position, TrajectoryDataDerived.Facing,
		TrajectoryDataSampling, 
		DeltaTime );
}

// Deprecated
void UPoseSearchTrajectoryLibrary::InitTrajectorySamples(
	FPoseSearchQueryTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData,	// Unused parameter
	FVector DefaultPosition, FQuat DefaultFacing,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	InitTrajectorySamples(
		Trajectory,
		DefaultPosition,
		DefaultFacing,
		TrajectoryDataSampling,
		DeltaTime);
}

// Deprecated
void UPoseSearchTrajectoryLibrary::InitTrajectorySamples(
	FPoseSearchQueryTrajectory& Trajectory,
	FVector DefaultPosition, FQuat DefaultFacing,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	FTransformTrajectory TransformTrajectory = Trajectory;
	InitTrajectorySamples(TransformTrajectory, DefaultPosition, DefaultFacing, TrajectoryDataSampling, DeltaTime);
	Trajectory = TransformTrajectory;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::InitTrajectorySamples(FTransformTrajectory& Trajectory, FVector DefaultPosition, FQuat DefaultFacing, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime)
{
	const int32 NumHistorySamples = TrajectoryDataSampling.NumHistorySamples;
	const int32 NumPredictionSamples = TrajectoryDataSampling.NumPredictionSamples;

	// History + current sample + prediction
	const int32 TotalNumSamples = NumHistorySamples + 1 + NumPredictionSamples;

	if (Trajectory.Samples.Num() != TotalNumSamples)
	{
		Trajectory.Samples.SetNumUninitialized(TotalNumSamples);

		// Initialize history samples
		const float SecondsPerHistorySample = FMath::Max(TrajectoryDataSampling.SecondsPerHistorySample, 0.f);
		for (int32 i = 0; i < NumHistorySamples; ++i)
		{
			Trajectory.Samples[i].Position = DefaultPosition;
			Trajectory.Samples[i].Facing = DefaultFacing;
			Trajectory.Samples[i].TimeInSeconds = SecondsPerHistorySample * (i - NumHistorySamples - 1);
		}

		// Initialize current sample and prediction
		const float SecondsPerPredictionSample = FMath::Max(TrajectoryDataSampling.SecondsPerPredictionSample, 0.f);
		for (int32 i = NumHistorySamples; i < Trajectory.Samples.Num(); ++i)
		{
			Trajectory.Samples[i].Position = DefaultPosition;
			Trajectory.Samples[i].Facing = DefaultFacing;
			Trajectory.Samples[i].TimeInSeconds = SecondsPerPredictionSample * (i - NumHistorySamples) + DeltaTime;
		}
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::UpdateHistory_TransformHistory(
	FPoseSearchQueryTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData,	// Unused parameter
	FVector CurrentPosition,
	FVector CurrentVelocity,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	UpdateHistory_TransformHistory(
		Trajectory,
		CurrentPosition,
		CurrentVelocity,
		TrajectoryDataSampling,
		DeltaTime);
}

// Deprecated
void UPoseSearchTrajectoryLibrary::UpdateHistory_TransformHistory(
	FPoseSearchQueryTrajectory& Trajectory,
	FVector CurrentPosition,
	FVector CurrentVelocity, 
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	FTransformTrajectory TransformTrajectory = Trajectory;
	UpdateHistory_TransformHistory(TransformTrajectory, CurrentPosition, CurrentVelocity, TrajectoryDataSampling, DeltaTime);
	Trajectory = TransformTrajectory;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::UpdateHistory_TransformHistory(FTransformTrajectory& Trajectory,
                                                                  FVector CurrentPosition,
                                                                  FVector CurrentVelocity,
                                                                  const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
                                                                  float DeltaTime,
                                                                  float CurrentTime)
{
	const int32 NumHistorySamples = TrajectoryDataSampling.NumHistorySamples;
	if (NumHistorySamples > 0)
	{
		const float SecondsPerHistorySample = TrajectoryDataSampling.SecondsPerHistorySample;

		check(NumHistorySamples <= Trajectory.Samples.Num());

		// Trajectory.Samples[NumHistorySamples] is last frame position! (assuming this is called every frame)
		const FVector CurrentTranslationFromMover = CurrentVelocity * DeltaTime;
		const FVector TranslationSinceLastFrame = CurrentPosition - Trajectory.Samples[NumHistorySamples].Position;
		const FVector InferredGroundTranslation = TranslationSinceLastFrame - CurrentTranslationFromMover;

		// Shift history Samples when it's time to record a new one.
		if (SecondsPerHistorySample <= 0.f || FMath::Abs(Trajectory.Samples[NumHistorySamples - 1].TimeInSeconds) >= SecondsPerHistorySample)
		{
			for (int32 Index = 0; Index < NumHistorySamples - 1; ++Index)
			{
				Trajectory.Samples[Index].TimeInSeconds = Trajectory.Samples[Index + 1].TimeInSeconds - DeltaTime;
				Trajectory.Samples[Index].Position = Trajectory.Samples[Index + 1].Position + InferredGroundTranslation;
				Trajectory.Samples[Index].Facing = Trajectory.Samples[Index + 1].Facing;
			}

			// Adding a new history record
			// Note we add current frame at delta time in the future since t = 0 should be the previous frame
			Trajectory.Samples[NumHistorySamples - 1].TimeInSeconds = CurrentTime;
			Trajectory.Samples[NumHistorySamples - 1].Position = CurrentPosition;
			Trajectory.Samples[NumHistorySamples - 1].Facing = Trajectory.Samples[NumHistorySamples].Facing;
		}
		else
		{
			// Didn't record a new history position, update timers and shift by ground translation

			for (int32 Index = 0; Index < NumHistorySamples; ++Index)
			{
				Trajectory.Samples[Index].TimeInSeconds -= DeltaTime;
				Trajectory.Samples[Index].Position += InferredGroundTranslation;
			}
		}
	}
}

void UPoseSearchTrajectoryLibrary::UpdateHistory_WorldSpace(FTransformTrajectory& Trajectory,
                                                            FVector CurrentPosition,
                                                            FQuat CurrentRotation,
                                                            const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
                                                            float DeltaTime,
                                                            float CurrentTime)
{
	const int32 NumHistorySamples = TrajectoryDataSampling.NumHistorySamples;
	if (NumHistorySamples > 0)
	{
		const float SecondsPerHistorySample = TrajectoryDataSampling.SecondsPerHistorySample;

		check(NumHistorySamples <= Trajectory.Samples.Num());

		// Shift history Samples when it's time to record a new one.
		if (SecondsPerHistorySample <= 0.f || FMath::Abs(Trajectory.Samples[NumHistorySamples - 1].TimeInSeconds) >= SecondsPerHistorySample)
		{
			for (int32 Index = 0; Index < NumHistorySamples - 1; ++Index)
			{
				Trajectory.Samples[Index].TimeInSeconds = Trajectory.Samples[Index + 1].TimeInSeconds - DeltaTime;
				Trajectory.Samples[Index].Position = Trajectory.Samples[Index + 1].Position;
				Trajectory.Samples[Index].Facing = Trajectory.Samples[Index + 1].Facing;
			}

			// Adding a new history record
			// Note we add current frame at delta time in the future since t = 0 should be the previous frame
			Trajectory.Samples[NumHistorySamples - 1].TimeInSeconds = CurrentTime;
			Trajectory.Samples[NumHistorySamples - 1].Position = CurrentPosition;
			Trajectory.Samples[NumHistorySamples - 1].Facing = CurrentRotation;
		}
		else
		{
			// Didn't record a new history position, update timers and shift by ground translation
			for (int32 Index = 0; Index < NumHistorySamples; ++Index)
			{
				Trajectory.Samples[Index].TimeInSeconds -= DeltaTime;
			}
		}
	}	
}

FVector UPoseSearchTrajectoryLibrary::RemapVectorMagnitudeWithCurve(
	const FVector& Vector,
	bool bUseCurve,
	const FRuntimeFloatCurve& Curve)
{
	if (bUseCurve)
	{
		const float Length = Vector.Length();
		if (Length > UE_KINDA_SMALL_NUMBER)
		{
			const float RemappedLength = Curve.GetRichCurveConst()->Eval(Length);
			return Vector * (RemappedLength / Length);
		}
	}

	return Vector;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::UpdatePrediction_SimulateCharacterMovement(
	FPoseSearchQueryTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData,
	const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling,
	float DeltaTime)
{
	FTransformTrajectory TransformTrajectory = Trajectory;
	UpdatePrediction_SimulateCharacterMovement(TransformTrajectory, TrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, DeltaTime);
	Trajectory = TransformTrajectory;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::UpdatePrediction_SimulateCharacterMovement(FTransformTrajectory& Trajectory,
	const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived,
	const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime)
{
		FVector CurrentPositionWS = TrajectoryDataDerived.Position;
	FVector CurrentVelocityWS = RemapVectorMagnitudeWithCurve(TrajectoryDataDerived.Velocity, TrajectoryData.bUseSpeedRemappingCurve, TrajectoryData.SpeedRemappingCurve);
	FVector CurrentAccelerationWS = RemapVectorMagnitudeWithCurve(TrajectoryDataDerived.Acceleration, TrajectoryData.bUseAccelerationRemappingCurve, TrajectoryData.AccelerationRemappingCurve);

	// Bending CurrentVelocityWS towards CurrentAccelerationWS
	if (TrajectoryData.BendVelocityTowardsAcceleration > UE_KINDA_SMALL_NUMBER && !CurrentAccelerationWS.IsNearlyZero())
	{
		const float CurrentSpeed = CurrentVelocityWS.Length();
		const FVector VelocityWSAlongAcceleration = CurrentAccelerationWS.GetUnsafeNormal() * CurrentSpeed;
		if (TrajectoryData.BendVelocityTowardsAcceleration < 1.f - UE_KINDA_SMALL_NUMBER)
		{
			CurrentVelocityWS = FMath::Lerp(CurrentVelocityWS, VelocityWSAlongAcceleration, TrajectoryData.BendVelocityTowardsAcceleration);

			const float NewLength = CurrentVelocityWS.Length();
			if (NewLength > UE_KINDA_SMALL_NUMBER)
			{
				CurrentVelocityWS *= CurrentSpeed / NewLength;
			}
			else
			{
				// @todo: consider setting the CurrentVelocityWS = VelocityWSAlongAcceleration if vel and acc are in opposite directions
			}
		}
		else
		{
			CurrentVelocityWS = VelocityWSAlongAcceleration;
		}
	}

	FQuat CurrentFacingWS = TrajectoryDataDerived.Facing;
	
	const int32 NumHistorySamples = TrajectoryDataSampling.NumHistorySamples;
	const float SecondsPerPredictionSample = TrajectoryDataSampling.SecondsPerPredictionSample;
	const FQuat ControllerRotationPerStep = FQuat::MakeFromEuler(FVector(0.f, 0.f, TrajectoryDataDerived.ControllerYawRate * SecondsPerPredictionSample));

	float AccumulatedSeconds = DeltaTime;

	const int32 LastIndex = Trajectory.Samples.Num() - 1;
	if (NumHistorySamples <= LastIndex)
	{
		for (int32 Index = NumHistorySamples; ; ++Index)
		{
			Trajectory.Samples[Index].Position = CurrentPositionWS;
			Trajectory.Samples[Index].Facing = CurrentFacingWS;
			Trajectory.Samples[Index].TimeInSeconds = AccumulatedSeconds;

			if (Index == LastIndex)
			{
				break;
			}

			CurrentPositionWS += CurrentVelocityWS * SecondsPerPredictionSample;
			AccumulatedSeconds += SecondsPerPredictionSample;

			if (TrajectoryDataDerived.bStepGroundPrediction)
			{
				CurrentAccelerationWS = RemapVectorMagnitudeWithCurve(ControllerRotationPerStep * CurrentAccelerationWS,
					TrajectoryData.bUseAccelerationRemappingCurve, TrajectoryData.AccelerationRemappingCurve);
				const FVector NewVelocityWS = TrajectoryData.StepCharacterMovementGroundPrediction(SecondsPerPredictionSample, CurrentVelocityWS, CurrentAccelerationWS, TrajectoryDataDerived);
				CurrentVelocityWS = RemapVectorMagnitudeWithCurve(NewVelocityWS, TrajectoryData.bUseSpeedRemappingCurve, TrajectoryData.SpeedRemappingCurve);

				// Account for the controller (e.g. the camera) rotating.
				CurrentFacingWS = ControllerRotationPerStep * CurrentFacingWS;
				if (TrajectoryDataDerived.bOrientRotationToMovement && !CurrentAccelerationWS.IsNearlyZero())
				{
					// Rotate towards acceleration.
					const FVector CurrentAccelerationCS = TrajectoryDataDerived.MeshCompRelativeRotation.RotateVector(CurrentAccelerationWS);
					CurrentFacingWS = FMath::QInterpConstantTo(CurrentFacingWS, CurrentAccelerationCS.ToOrientationQuat(), SecondsPerPredictionSample, TrajectoryData.RotateTowardsMovementSpeed);
				}
			}
		}
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::PoseSearchGenerateTrajectory(
	const UObject* Context, 
	UPARAM(ref)	const FPoseSearchTrajectoryData& InTrajectoryData,
	float InDeltaTime,
	UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory,
	UPARAM(ref) float& InOutDesiredControllerYawLastUpdate,
	FPoseSearchQueryTrajectory& OutTrajectory,
	float InHistorySamplingInterval,
	int32 InTrajectoryHistoryCount,
	float InPredictionSamplingInterval,
	int32 InTrajectoryPredictionCount)
{
	FTransformTrajectory InOutTransformTrajectory = InOutTrajectory;
	FTransformTrajectory OutTransformTrajectory;
	
	PoseSearchGenerateTransformTrajectory(Context, InTrajectoryData, InDeltaTime, InOutTransformTrajectory, InOutDesiredControllerYawLastUpdate,
		OutTransformTrajectory, InHistorySamplingInterval, InTrajectoryHistoryCount, InPredictionSamplingInterval, InTrajectoryPredictionCount);

	OutTrajectory = OutTransformTrajectory;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(const UObject* InContext, const FPoseSearchTrajectoryData& InTrajectoryData,
	float InDeltaTime, FTransformTrajectory& InOutTrajectory, float& InOutDesiredControllerYawLastUpdate, FTransformTrajectory& OutTrajectory,
	float InHistorySamplingInterval, int32 InTrajectoryHistoryCount, float InPredictionSamplingInterval, int32 InTrajectoryPredictionCount)
{
	FPoseSearchTrajectoryData::FSampling TrajectoryDataSampling;
	TrajectoryDataSampling.NumHistorySamples = InTrajectoryHistoryCount;
	TrajectoryDataSampling.SecondsPerHistorySample = InHistorySamplingInterval;
	TrajectoryDataSampling.NumPredictionSamples = InTrajectoryPredictionCount;
	TrajectoryDataSampling.SecondsPerPredictionSample = InPredictionSamplingInterval;

	FPoseSearchTrajectoryData::FState TrajectoryDataState;
	TrajectoryDataState.DesiredControllerYawLastUpdate = InOutDesiredControllerYawLastUpdate;

	FPoseSearchTrajectoryData::FDerived TrajectoryDataDerived;
	InTrajectoryData.UpdateData(InDeltaTime, InContext, TrajectoryDataDerived, TrajectoryDataState);
	InitTrajectorySamples(InOutTrajectory, TrajectoryDataDerived.Position, TrajectoryDataDerived.Facing, TrajectoryDataSampling, InDeltaTime);
	UpdateHistory_TransformHistory(InOutTrajectory, TrajectoryDataDerived.Position, TrajectoryDataDerived.Velocity, TrajectoryDataSampling, InDeltaTime);
	UpdatePrediction_SimulateCharacterMovement(InOutTrajectory, InTrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, InDeltaTime);

	InOutDesiredControllerYawLastUpdate = TrajectoryDataState.DesiredControllerYawLastUpdate;

	OutTrajectory = InOutTrajectory;
}

void UPoseSearchTrajectoryLibrary::PoseSearchGeneratePredictorTransformTrajectory(UObject* InPredictor,
	const FPoseSearchTrajectoryData& InTrajectoryData, float InDeltaTime, FTransformTrajectory& InOutTrajectory,
	float& InOutDesiredControllerYawLastUpdate, FTransformTrajectory& OutTrajectory, float InHistorySamplingInterval,
	int32 InTrajectoryHistoryCount, float InPredictionSamplingInterval, int32 InTrajectoryPredictionCount)
{
	IPoseSearchTrajectoryPredictorInterface* Predictor = Cast<IPoseSearchTrajectoryPredictorInterface>(InPredictor);
	if (Predictor != nullptr)
	{
		PoseSearchGenerateTransformTrajectoryWithPredictor(InPredictor,
			InDeltaTime,
			InOutTrajectory,
			InOutDesiredControllerYawLastUpdate,
			OutTrajectory,
			InHistorySamplingInterval,
			InTrajectoryHistoryCount,
			InPredictionSamplingInterval,
			InTrajectoryPredictionCount);
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::PoseSearchGeneratePredictorTrajectory(
	UObject* InPredictor,	// must implement IPoseSearchTrajectoryPredictorInterface
	UPARAM(ref)	const FPoseSearchTrajectoryData& InTrajectoryData,
	float InDeltaTime,
	UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory,
	UPARAM(ref) float& InOutDesiredControllerYawLastUpdate,
	FPoseSearchQueryTrajectory& OutTrajectory,
	float InHistorySamplingInterval,
	int32 InTrajectoryHistoryCount,
	float InPredictionSamplingInterval,
	int32 InTrajectoryPredictionCount)
{
	IPoseSearchTrajectoryPredictorInterface* predictor = Cast<IPoseSearchTrajectoryPredictorInterface>(InPredictor);
	if (predictor != nullptr)
	{
		PoseSearchGenerateTrajectoryWithPredictor(InPredictor,
			InDeltaTime,
			InOutTrajectory,
			InOutDesiredControllerYawLastUpdate,
			OutTrajectory,
			InHistorySamplingInterval,
			InTrajectoryHistoryCount,
			InPredictionSamplingInterval,
			InTrajectoryPredictionCount);
	}
}

// Deprecated
void UPoseSearchTrajectoryLibrary::PoseSearchGenerateTrajectoryWithPredictor(
	TScriptInterface<IPoseSearchTrajectoryPredictorInterface> InPredictor,	// must implement IPoseSearchTrajectoryPredictorInterface
	float InDeltaTime,
	UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory,
	UPARAM(ref) float& InOutDesiredControllerYawLastUpdate,
	FPoseSearchQueryTrajectory& OutTrajectory,
	float InHistorySamplingInterval,
	int32 InTrajectoryHistoryCount,
	float InPredictionSamplingInterval,
	int32 InTrajectoryPredictionCount)
{
	FTransformTrajectory InOutTransformTrajectory = InOutTrajectory;
	FTransformTrajectory OutTransformTrajectory;
	
	PoseSearchGenerateTransformTrajectoryWithPredictor(InPredictor, InDeltaTime, InOutTransformTrajectory, InOutDesiredControllerYawLastUpdate,
		OutTransformTrajectory, InHistorySamplingInterval, InTrajectoryHistoryCount, InPredictionSamplingInterval, InTrajectoryPredictionCount);

	OutTrajectory = OutTransformTrajectory;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(TScriptInterface<IPoseSearchTrajectoryPredictorInterface> InPredictor,
	float InDeltaTime, FTransformTrajectory& InOutTrajectory, float& InOutDesiredControllerYawLastUpdate, FTransformTrajectory& OutTrajectory,
	float InHistorySamplingInterval, int32 InTrajectoryHistoryCount, float InPredictionSamplingInterval, int32 InTrajectoryPredictionCount)
{
	FPoseSearchTrajectoryData::FSampling TrajectoryDataSampling;
	TrajectoryDataSampling.NumHistorySamples = InTrajectoryHistoryCount;
	TrajectoryDataSampling.SecondsPerHistorySample = InHistorySamplingInterval;
	TrajectoryDataSampling.NumPredictionSamples = InTrajectoryPredictionCount;
	TrajectoryDataSampling.SecondsPerPredictionSample = InPredictionSamplingInterval;

	// TODO: handle controller yaw
	//TrajectoryDataState.DesiredControllerYawLastUpdate = InOutDesiredControllerYawLastUpdate;

	FVector CurrentPosition = FVector::ZeroVector;
	FVector CurrentVelocity = FVector::ZeroVector;
	FQuat CurrentFacing = FQuat::Identity;

	if (InPredictor != nullptr)
	{
		InPredictor->GetCurrentState(CurrentPosition, CurrentFacing, CurrentVelocity);
	}

	InitTrajectorySamples(InOutTrajectory, CurrentPosition, CurrentFacing, TrajectoryDataSampling, InDeltaTime);
	UpdateHistory_TransformHistory(InOutTrajectory, CurrentPosition, CurrentVelocity, TrajectoryDataSampling, InDeltaTime);

	if (InPredictor != nullptr)
	{
		InPredictor->Predict(InOutTrajectory, InTrajectoryPredictionCount + 1, InPredictionSamplingInterval, InTrajectoryHistoryCount);
	}

	//InOutDesiredControllerYawLastUpdate = TrajectoryDataState.DesiredControllerYawLastUpdate;

	OutTrajectory = InOutTrajectory;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::HandleTrajectoryWorldCollisions(const UObject* WorldContextObject, const UAnimInstance* AnimInstance, UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, bool bApplyGravity, float FloorCollisionsOffset, FPoseSearchQueryTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
                                                                   ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf, float MaxObstacleHeight, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	FTransformTrajectory InTransformTrajectory = InTrajectory;
	FTransformTrajectory OutTransformTrajectory;
	
	HandleTransformTrajectoryWorldCollisions(WorldContextObject, AnimInstance, InTransformTrajectory, bApplyGravity, 
        FloorCollisionsOffset, OutTransformTrajectory, CollisionResult, TraceChannel, bTraceComplex, ActorsToIgnore, 
        DrawDebugType, bIgnoreSelf, MaxObstacleHeight, TraceColor, TraceHitColor, DrawTime);

	OutTrajectory = OutTransformTrajectory;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::HandleTransformTrajectoryWorldCollisions(const UObject* WorldContextObject, const UAnimInstance* AnimInstance,
	const FTransformTrajectory& InTrajectory, bool bApplyGravity, float FloorCollisionsOffset, FTransformTrajectory& OutTrajectory,
	FPoseSearchTrajectory_WorldCollisionResults& CollisionResult, ETraceTypeQuery TraceChannel, bool bTraceComplex,
	const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf, float MaxObstacleHeight, FLinearColor TraceColor,
	FLinearColor TraceHitColor, float DrawTime)
{
	FVector StartingVelocity = FVector::ZeroVector;
	FVector GravityAccel = FVector::ZeroVector;
	if (bApplyGravity && AnimInstance)
	{
		if (const ACharacter* Character = Cast<ACharacter>(AnimInstance->GetOwningActor()))
		{
			if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				GravityAccel = MoveComp->GetGravityDirection() * -MoveComp->GetGravityZ();
				StartingVelocity = Character->GetVelocity();
			}
		}
	}

	HandleTransformTrajectoryWorldCollisionsWithGravity(WorldContextObject, InTrajectory, StartingVelocity, bApplyGravity, GravityAccel, 
		FloorCollisionsOffset, OutTrajectory, CollisionResult, TraceChannel, bTraceComplex, ActorsToIgnore, 
		DrawDebugType, bIgnoreSelf, MaxObstacleHeight, TraceColor, TraceHitColor, DrawTime);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::HandleTrajectoryWorldCollisionsWithGravity(const UObject* WorldContextObject,
                                                                              UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, FVector StartingVelocity, bool bApplyGravity, FVector GravityAccel, float FloorCollisionsOffset, FPoseSearchQueryTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
                                                                              ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf, float MaxObstacleHeight, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	FTransformTrajectory InTransformTrajectory = InTrajectory;
	FTransformTrajectory OutTransformTrajectory;
	
	HandleTransformTrajectoryWorldCollisionsWithGravity(WorldContextObject, InTransformTrajectory, StartingVelocity, bApplyGravity, GravityAccel, 
		FloorCollisionsOffset, OutTransformTrajectory, CollisionResult, TraceChannel, bTraceComplex, ActorsToIgnore, 
		DrawDebugType, bIgnoreSelf, MaxObstacleHeight, TraceColor, TraceHitColor, DrawTime);

	OutTrajectory = OutTransformTrajectory;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::HandleTransformTrajectoryWorldCollisionsWithGravity(const UObject* WorldContextObject,
	const FTransformTrajectory& InTrajectory, FVector StartingVelocity, bool bApplyGravity, FVector GravityAccel, float FloorCollisionsOffset,
	FTransformTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult, ETraceTypeQuery TraceChannel,
	bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf, float MaxObstacleHeight,
	FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	OutTrajectory = InTrajectory;

	TArray<FTransformTrajectorySample>& Samples = OutTrajectory.Samples;
	const int32 NumSamples = Samples.Num();

	FVector GravityDirection = FVector::ZeroVector;
	float GravityZ = 0.f;
	float InitialVelocityZ = StartingVelocity.Z;

	if (bApplyGravity && !GravityAccel.IsNearlyZero())
	{
		GravityAccel.ToDirectionAndLength(GravityDirection, GravityZ);
		GravityZ = -GravityZ;
		const FVector VelocityOnGravityAxis = StartingVelocity.ProjectOnTo(GravityDirection);
		
		InitialVelocityZ = VelocityOnGravityAxis.Length() * -FMath::Sign(GravityDirection.Dot(VelocityOnGravityAxis));
	}

	CollisionResult.TimeToLand = OutTrajectory.Samples.Last().TimeInSeconds;

	if (!FMath::IsNearlyZero(GravityZ))
	{
		FVector LastImpactPoint;
		FVector LastImpactNormal;
		bool bIsLastImpactValid = false;
		bool bIsFirstFall = true;

		const FVector Gravity = GravityDirection * -GravityZ;
		float FreeFallAccumulatedSeconds = 0.f;
		for (int32 SampleIndex = 1; SampleIndex < NumSamples; ++SampleIndex)
		{
			FTransformTrajectorySample& Sample = Samples[SampleIndex];
			if (Sample.TimeInSeconds > 0.f)
			{
				const int32 PrevSampleIndex = SampleIndex - 1;
				const FTransformTrajectorySample& PrevSample = Samples[PrevSampleIndex];

				FreeFallAccumulatedSeconds += Sample.TimeInSeconds - PrevSample.TimeInSeconds;

				if (bIsLastImpactValid)
				{
					const FPlane GroundPlane = FPlane(PrevSample.Position, -GravityDirection);
					Sample.Position = FPlane::PointPlaneProject(Sample.Position, GroundPlane);
				}

				// applying gravity
				const FVector FreeFallOffset =  Gravity * (0.5f * FreeFallAccumulatedSeconds * FreeFallAccumulatedSeconds);
				Sample.Position += FreeFallOffset;

				FHitResult HitResult;
				if (FloorCollisionsOffset > 0.f && UKismetSystemLibrary::LineTraceSingle(WorldContextObject, Sample.Position + (GravityDirection * -MaxObstacleHeight), Sample.Position, TraceChannel, bTraceComplex, ActorsToIgnore, DrawDebugType, HitResult, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime))
				{
					// Only allow our trace to move trajectory along gravity direction.
					LastImpactPoint = UKismetMathLibrary::FindClosestPointOnLine(HitResult.ImpactPoint, Sample.Position, GravityDirection);
					LastImpactNormal = HitResult.Normal;
					bIsLastImpactValid = true;

					Sample.Position = LastImpactPoint - GravityDirection * FloorCollisionsOffset;

					if (bIsFirstFall)
					{
						const float InitialHeight = OutTrajectory.GetSampleAtTime(0.0f).Position.Z;
						const float FinalHeight = Sample.Position.Z;
						const float FallHeight = FMath::Abs(FinalHeight - InitialHeight);

						bIsFirstFall = false;
						CollisionResult.TimeToLand = (InitialVelocityZ / -GravityZ) + ((FMath::Sqrt(FMath::Square(InitialVelocityZ) + (2.f * -GravityZ * FallHeight))) / -GravityZ);
						CollisionResult.LandSpeed = InitialVelocityZ + GravityZ * CollisionResult.TimeToLand;
					}

					FreeFallAccumulatedSeconds = 0.f;
				}
			}
		}
	}
	else if (FloorCollisionsOffset > 0.f)
	{
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			FTransformTrajectorySample& Sample = OutTrajectory.Samples[SampleIndex];
			if (Sample.TimeInSeconds > 0.f)
			{
				FHitResult HitResult;
				if (UKismetSystemLibrary::LineTraceSingle(WorldContextObject, Sample.Position + FVector::UpVector * 3000.f, Sample.Position, TraceChannel, bTraceComplex, ActorsToIgnore, DrawDebugType, HitResult, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime))
				{
					Sample.Position.Z = HitResult.ImpactPoint.Z + FloorCollisionsOffset;
				}
			}
		}
	}

	CollisionResult.LandSpeed = InitialVelocityZ + GravityZ * CollisionResult.TimeToLand;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Deprecated
void UPoseSearchTrajectoryLibrary::GetTrajectorySampleAtTime(UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, float Time, FPoseSearchQueryTrajectorySample& OutTrajectorySample, bool bExtrapolate)
{
	OutTrajectorySample = InTrajectory.GetSampleAtTime(Time, bExtrapolate);
}

// Deprecated
void UPoseSearchTrajectoryLibrary::GetTrajectoryVelocity(UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, float Time1, float Time2, FVector& OutVelocity, bool bExtrapolate)
{
	GetTransformTrajectoryVelocity(FTransformTrajectory(InTrajectory), Time1, Time2, OutVelocity, bExtrapolate);
}

// Deprecated
void UPoseSearchTrajectoryLibrary::GetTrajectoryAngularVelocity(const FPoseSearchQueryTrajectory& InTrajectory, float Time1, float Time2, FVector& OutAngularVelocity, bool bExtrapolate /*= false*/)
{
	GetTransformTrajectoryAngularVelocity(FTransformTrajectory(InTrajectory), Time1, Time2, OutAngularVelocity, bExtrapolate);
}

// Deprecated
FTransform UPoseSearchTrajectoryLibrary::GetTransform(const FPoseSearchQueryTrajectorySample& InTrajectorySample)
{
	return InTrajectorySample.GetTransform();
}

// Deprecated
void UPoseSearchTrajectoryLibrary::DrawTrajectory(const UObject* WorldContextObject, const FPoseSearchQueryTrajectory& InTrajectory, const float DebugThickness, float HeightOffset)
{
	DrawTransformTrajectory(WorldContextObject, FTransformTrajectory(InTrajectory), DebugThickness, HeightOffset);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(const FTransformTrajectory& InTrajectory, float Time,
	FTransformTrajectorySample& OutTrajectorySample, bool bExtrapolate)
{
	OutTrajectorySample = InTrajectory.GetSampleAtTime(Time, bExtrapolate);
}

void UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(const FTransformTrajectory& InTrajectory, float Time1, float Time2, FVector& OutVelocity,
	bool bExtrapolate)
{
	if (FMath::IsNearlyEqual(Time1, Time2))
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchTrajectoryLibrary::GetTrajectoryVelocity - Time1 is same as Time2. Invalid time horizon."));
		OutVelocity = FVector::ZeroVector;
		return;
	}

	FTransformTrajectorySample Sample1 = InTrajectory.GetSampleAtTime(Time1, bExtrapolate);
	FTransformTrajectorySample Sample2 = InTrajectory.GetSampleAtTime(Time2, bExtrapolate);

	OutVelocity = (Sample2.Position - Sample1.Position) / (Time2 - Time1);
}

void UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(const FTransformTrajectory& InTrajectory, float Time1, float Time2,
	FVector& OutAngularVelocity, bool bExtrapolate)
{
	if (FMath::IsNearlyEqual(Time1, Time2))
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchTrajectoryLibrary::GetTrajectoryAngularVelocity - Time1 is same as Time2. Invalid time horizon."));
		OutAngularVelocity = FVector::ZeroVector;
		return;
	}

	FTransformTrajectorySample Sample1 = InTrajectory.GetSampleAtTime(Time1, bExtrapolate);
	FTransformTrajectorySample Sample2 = InTrajectory.GetSampleAtTime(Time2, bExtrapolate);

	const FQuat DeltaRotation = (Sample2.Facing * Sample1.Facing.Inverse()).GetShortestArcWith(FQuat::Identity);
	const FVector AngularVelocityInRadians = DeltaRotation.ToRotationVector() / (Time2 - Time1);

	OutAngularVelocity = FVector(
		FMath::RadiansToDegrees(AngularVelocityInRadians.X),
		FMath::RadiansToDegrees(AngularVelocityInRadians.Y),
		FMath::RadiansToDegrees(AngularVelocityInRadians.Z));
}

FTransform UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleTransform(const FTransformTrajectorySample& InTrajectorySample)
{
	return InTrajectorySample.GetTransform();
}

void UPoseSearchTrajectoryLibrary::DrawTransformTrajectory(const UObject* WorldContextObject, const FTransformTrajectory& InTrajectory,
	const float DebugThickness, float HeightOffset)
{
#if ENABLE_ANIM_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UTransformTrajectoryBlueprintLibrary::DebugDrawTrajectory(InTrajectory, World, DebugThickness, HeightOffset);
	}
#endif // ENABLE_ANIM_DEBUG
}


// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/PathedMovementMode.h"
#include "PhysicsMover/PathedMovement/PathedMovementPatternBase.h"
#include "MoverComponent.h"
#include "Backends/MoverPathedPhysicsLiaison.h"
#include "Framework/Threading.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PathedMovementMode)

FAutoConsoleVariable CVarEnablePathedPhysicsMovementDebug(
	TEXT("Mover.PathedPhysicsMovement.EnableDebug"),
	false,
	TEXT("True to enable a log firehose of position update debug info")
);

UPathedPhysicsMovementMode::UPathedPhysicsMovementMode()
{
	//@todo DanH: A number of these should be able to auto-configure based on mass and gravity 
	
	JointConstraintProperties.bLinearBreakable = false;
	JointConstraintProperties.bAngularBreakable = false;
	JointConstraintProperties.bDisableCollision = true;

	// Free the linear joints and set the max allowed distance from the target
	static constexpr float DefaultMaxLinearDistance = 100.f;
	JointConstraintProperties.LinearLimit.XMotion = LCM_Free;
	JointConstraintProperties.LinearLimit.YMotion = LCM_Free;
	JointConstraintProperties.LinearLimit.ZMotion = LCM_Free;
	JointConstraintProperties.LinearLimit.Limit = DefaultMaxLinearDistance;

	static constexpr float DefaultDriveMaxForce = 5000.f;
	
	// Linear drive config
	static constexpr float LinearDriveStiffness = 750.f;
	static const float LinearDriveCriticalDamping = 2 * FMath::Sqrt(LinearDriveStiffness);
	
	FConstraintDrive DefaultLinearDriveConstraint;
	DefaultLinearDriveConstraint.bEnablePositionDrive = true;
	DefaultLinearDriveConstraint.bEnableVelocityDrive = true;
	DefaultLinearDriveConstraint.Stiffness = LinearDriveStiffness;
	DefaultLinearDriveConstraint.Damping = LinearDriveCriticalDamping;
	DefaultLinearDriveConstraint.MaxForce = DefaultDriveMaxForce;

	JointConstraintProperties.LinearDrive.XDrive = DefaultLinearDriveConstraint;
	JointConstraintProperties.LinearDrive.YDrive = DefaultLinearDriveConstraint;
	JointConstraintProperties.LinearDrive.ZDrive = DefaultLinearDriveConstraint;

	// Free the angular joints and set degree limits relative to the target
	static constexpr float DefaultAngularLimitDegrees = 15.f;
	JointConstraintProperties.ConeLimit.Swing1Motion = ACM_Free;
	JointConstraintProperties.ConeLimit.Swing1LimitDegrees = DefaultAngularLimitDegrees;
	JointConstraintProperties.ConeLimit.Swing2Motion = ACM_Free;
	JointConstraintProperties.ConeLimit.Swing2LimitDegrees = DefaultAngularLimitDegrees;
	JointConstraintProperties.TwistLimit.TwistMotion = ACM_Free;
	JointConstraintProperties.TwistLimit.TwistLimitDegrees = DefaultAngularLimitDegrees;
	
	// Angular drive config
	static constexpr float AngularDriveStiffness = 1500.f;
	static const float AngularDriveCriticalDamping = 2 * FMath::Sqrt(AngularDriveStiffness);

	JointConstraintProperties.AngularDrive.AngularDriveMode = EAngularDriveMode::SLERP;
	JointConstraintProperties.AngularDrive.SlerpDrive.bEnablePositionDrive = true;
	JointConstraintProperties.AngularDrive.SlerpDrive.bEnableVelocityDrive = true;
	JointConstraintProperties.AngularDrive.SlerpDrive.Stiffness = AngularDriveStiffness;
	JointConstraintProperties.AngularDrive.SlerpDrive.Damping = AngularDriveCriticalDamping;
	JointConstraintProperties.AngularDrive.SlerpDrive.MaxForce = DefaultDriveMaxForce;
}

#if WITH_EDITOR
void UPathedPhysicsMovementMode::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	FPatternPostChangeMovementModeHelper::ProcessPostEditChangeChain(*this, PropertyChangedChainEvent);
}
#endif

void UPathedPhysicsMovementMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	Chaos::EnsureIsInPhysicsThreadContext();
	
	OnPreSimulate_Internal(Params, OutputState);
}

void UPathedPhysicsMovementMode::OnProcessInput_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const
{
	if (const FPathedPhysicsMovementInputs* Inputs = Input.InputCmd.InputCollection.FindDataByType<FPathedPhysicsMovementInputs>())
	{
		CachedInputs = Inputs->Props;

		FPathedPhysicsMovementState& StartMoveState = Input.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FPathedPhysicsMovementState>();
		const FMutablePathedMovementProperties& CurProps = StartMoveState.MutableProps;
		
		if (CachedInputs.IsMoving() != CurProps.IsMoving())
		{
			const float PlaybackDuration = GetPlaybackDuration();
			if (StartMoveState.MutableProps.bIsInReverse && StartMoveState.LastStopPlaybackTime == 0.f)
			{
				// When starting play in reverse, jump to the end first (otherwise it'll be immediately done)
				StartMoveState.LastStopPlaybackTime = PlaybackDuration;
			}
			else if (!StartMoveState.MutableProps.bIsInReverse && StartMoveState.LastStopPlaybackTime == PlaybackDuration)
			{
				// Similarly, when we're starting at the end and want to play forward, jump to the start
				StartMoveState.LastStopPlaybackTime = 0.f;
			}
		}
	}
}

void UPathedPhysicsMovementMode::OnPreSimulate_Internal(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) const
{
	// The output state is initialized from the input state 
	FPathedPhysicsMovementState& OutputMoveState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FPathedPhysicsMovementState>();

	if (!OutputMoveState.MutableProps.IsMoving() || OutputMoveState.MutableProps.MovementStartFrame > Params.TimeStep.ServerFrame)
	{
		// Either we're not moving or we shouldn't start until a future frame
		return;
	}
	
	// How long (in s) it's been since we started moving (negative when going in reverse)
	const float TimeDirection = OutputMoveState.MutableProps.bIsInReverse ? -1.f : 1.f;
	const float TimeSinceStart = (Params.TimeStep.ServerFrame - OutputMoveState.MutableProps.MovementStartFrame + 1) * Params.TimeStep.StepMs * TimeDirection * 0.001f;

	// Offset the time since starting by wherever we started playback in the first place
	//	ex: If we last stopped at 3s and it's been 1s, the actual playback time should be 4s
	//	ex: In reverse, it's been -1s, giving the expected playback time of 2s
	const float TimeSinceLastStop = TimeSinceStart + OutputMoveState.LastStopPlaybackTime;
	
	bool bReachesEndOfPlayback = false;
	const float PlaybackTime = GetBoundedPlaybackTime(TimeSinceLastStop, bReachesEndOfPlayback);
	
	if (bReachesEndOfPlayback)
	{
		OutputMoveState.MutableProps.MovementStartFrame = INDEX_NONE;
		OutputMoveState.LastStopPlaybackTime = PlaybackTime;
	}
	
	const float BoundedTimeAlongPath = GetBoundedTimeAlongPath_Unsafe(PlaybackTime);
	
	const float Alpha = FMath::Clamp(UKismetMathLibrary::SafeDivide(BoundedTimeAlongPath , GetPathDuration()), 0.f, 1.f);
	const float ProgressAmt = FAlphaBlend::AlphaToBlendOption(Alpha, Easing, CustomEasingCurve);

	const Chaos::FRigidTransform3 TargetRelativeTransform = CalcTargetRelativeTransform(ProgressAmt);
	GetPathedMoverComp().GetSimBlackboard_Mutable()->Set(PathBlackboard::TargetRelativeTransform, TargetRelativeTransform);
	
	if (CVarEnablePathedPhysicsMovementDebug->GetBool())
	{
		const Chaos::FRigidTransform3 TargetWorldTransform = Chaos::FRigidTransform3::MultiplyNoScale(TargetRelativeTransform, OutputMoveState.MutableProps.PathOrigin);
		UE_LOG(LogMover, VeryVerbose, TEXT("Pathed mover [%s (%s)]: Setting target:\n\tRelative [%s]\n\tWorld [%s]"),
			*GetPathedMoverComp().GetOwner()->GetName(),
			GetPathedMoverComp().GetOwner()->GetNetMode() == NM_DedicatedServer ? TEXT("Server") : TEXT("Client"),
			*TargetRelativeTransform.ToString(),
			*TargetWorldTransform.ToString());
	}
}

void UPathedPhysicsMovementMode::InitializePath()
{
	for (UPathedMovementPatternBase* Pattern : PathPatterns)
	{
		if (Pattern)
		{
			Pattern->InitializePattern();
		}
	}
}

UPathedMovementPatternBase* UPathedPhysicsMovementMode::BP_FindPattern(TSubclassOf<UPathedMovementPatternBase> PatternType) const
{
	for (UPathedMovementPatternBase* Pattern : PathPatterns)
	{
		if (Pattern && Pattern->IsA(PatternType))
		{
			return Pattern;
		}
	}
	
	return nullptr;
}

void UPathedPhysicsMovementMode::SetPathDuration_BeginPlayOnly(float NewDuration)
{
	if (ensure(!GetPathedMoverComp().HasBegunPlay()))
	{
		OneWayTripDuration = FMath::Max(NewDuration, 0.f);
	}
}

void UPathedPhysicsMovementMode::SetUseJointConstraint(bool bUseJoint)
{
	if (bUseJointConstraint != bUseJoint)
	{
		bUseJointConstraint = bUseJoint;
		OnIsUsingJointChanged().Broadcast(bUseJoint);
	}
}

UPathedPhysicsMoverComponent& UPathedPhysicsMovementMode::GetPathedMoverComp() const
{
	return *GetOuterUPathedPhysicsMoverComponent();
}

Chaos::FRigidTransform3 UPathedPhysicsMovementMode::CalcTargetRelativeTransform(float ProgressAmt) const
{
	FTransform TargetRelativeTransform = FTransform::Identity;
	
	for (UPathedMovementPatternBase* PathPattern : PathPatterns)
	{
		if (PathPattern)
		{
			const FTransform PatternTransform = PathPattern->CalcTargetRelativeTransform(ProgressAmt, TargetRelativeTransform);
        	TargetRelativeTransform.Accumulate(PatternTransform);
		}
	}

	return TargetRelativeTransform;
}

float UPathedPhysicsMovementMode::GetPlaybackDuration() const
{
	// By doubling the playback time for ping-pongs, we can treat them the same as normal monodirectional playback when bounding
	return CachedInputs.IsPingPonging() ? GetPathDuration() * 2 : GetPathDuration();
}

float UPathedPhysicsMovementMode::GetBoundedPlaybackTime(float PlaybackTime, bool& bOutReachesEndOfPlayback) const
{
	bOutReachesEndOfPlayback = false;
	const float PlaybackDuration = GetPlaybackDuration();;	
	
	// FMod gets mad if you send it a tiny mod factor, so just treat 0 duration as infinite
	if (ensure(!FMath::IsNearlyZero(PlaybackDuration)))
	{
		const bool bIsLooping = CachedInputs.IsLooping();
		if (CachedInputs.bIsInReverse)
		{
			if (bIsLooping)
			{
				if (PlaybackTime < 0.f)
				{
					// Roll a negative time back around to starting at max (i.e. in a 4s path, -1s input should become 3s)
					PlaybackTime = PlaybackDuration + FMath::Fmod(PlaybackTime, PlaybackDuration);
				}
				while (PlaybackTime < 0.f)
				{
					PlaybackTime += PlaybackDuration;
				}
			}
			else if (PlaybackTime <= 0.f)
			{
				// This is a one-shot that has passed 0, so it's all done
				PlaybackTime = 0.f;
				bOutReachesEndOfPlayback = true;
			}
		}
		else if (PlaybackTime > PlaybackDuration)
		{
			if (bIsLooping)
			{
				// Loop the completed run back to the beginning
				PlaybackTime = FMath::Fmod(PlaybackTime, PlaybackDuration);
			}
			else
			{
				// Reached the end, all done
				PlaybackTime = PlaybackDuration;
				bOutReachesEndOfPlayback = true;
			}
		}
	}

	return PlaybackTime;
}

float UPathedPhysicsMovementMode::GetBoundedTimeAlongPath(float Time) const
{
	const float PathDuration = GetPathDuration();
	const EPathedPhysicsPlaybackBehavior PlaybackBehavior = CachedInputs.PlaybackBehavior;
	const bool bIsPingPonging = PlaybackBehavior == EPathedPhysicsPlaybackBehavior::ThereAndBack || PlaybackBehavior == EPathedPhysicsPlaybackBehavior::PingPong;

	if (Time < 0.f ||
		(bIsPingPonging && Time > 2 * PathDuration) ||
		(!bIsPingPonging && Time > PathDuration))
	{
		bool bThrowaway = false;
		Time = GetBoundedPlaybackTime(Time, bThrowaway);
	}

	return GetBoundedTimeAlongPath_Unsafe(Time);
}

float UPathedPhysicsMovementMode::GetBoundedTimeAlongPath_Unsafe(float BoundedPlaybackTime) const
{
	// TimeAlongPath only has the potential to differ from PlaybackTime when doing ping-pong-style movement
	if (CachedInputs.IsPingPonging())
	{
		const float PathDuration = GetPathDuration();
		if (BoundedPlaybackTime > PathDuration)
		{
			// Ex: We're 8s into a ping-pong of a 5s path. That means we did 5s there and are 3s into the trip back, with 2s to go ==> 10 - 8 = 2
			const float TimeAlongPath = PathDuration * 2 - BoundedPlaybackTime;
			return ensure(TimeAlongPath >= 0.f) ? TimeAlongPath : 0.f;
		}
	}
	
	return BoundedPlaybackTime;
}

#if WITH_EDITOR
void FPatternPostChangeMovementModeHelper::ProcessPostEditChangeChain(UPathedPhysicsMovementMode& Mode, FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	if (Mode.PathPatterns.Num() > 1 && PropertyChangedChainEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(UPathedPhysicsMovementMode, PathPatterns)))
	{
		const FProperty* ActuallyChangedProperty = PropertyChangedChainEvent.PropertyChain.GetTail()->GetValue();
		if (PropertyChangedChainEvent.ChangeType == EPropertyChangeType::ArrayRemove || 
			ActuallyChangedProperty->GetFName().IsEqual(GET_MEMBER_NAME_CHECKED(UPathedMovementPatternBase, bStartAfterPreviousPattern)) ||
			ActuallyChangedProperty->GetFName().IsEqual(GET_MEMBER_NAME_CHECKED(UPathedMovementPatternBase, StartAtPathProgress)) ||
			ActuallyChangedProperty->GetFName().IsEqual(GET_MEMBER_NAME_CHECKED(UPathedMovementPatternBase, EndAtPathProgress)))
		{
			// Don't bother figuring out exactly who or what changed at this point - just run through and make sure all the Start/End info is sound
			float PrevPatternEndProgress = 0.f;
			for (UPathedMovementPatternBase* Pattern : Mode.PathPatterns)
			{
				if (Pattern)
				{
					if (Pattern->bStartAfterPreviousPattern)
					{
						// It's possible this pattern wasn't the one that changed, so just make sure it's part of the transaction
						Pattern->Modify();
						Pattern->StartAtPathProgress = PrevPatternEndProgress;
					}
					
					if (Pattern->StartAtPathProgress > Pattern->EndAtPathProgress)
					{
						Pattern->Modify();
						Pattern->EndAtPathProgress = Pattern->StartAtPathProgress;
					}
					
					PrevPatternEndProgress = Pattern->EndAtPathProgress;
				}
			}
		}
	}
}
#endif
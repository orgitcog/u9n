// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/PathedMovementPatternBase.h"

#include "HAL/IConsoleManager.h"
#include "PhysicsMover/PathedMovement/PathedMovementMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PathedMovementPatternBase)

void UPathedMovementPatternBase::InitializePattern()
{
}

void UPathedMovementPatternBase::ProduceInputs_External(OUT FPhysicsMoverAsyncInput& Input)
{
	//@todo DanH: Push something with the total duration and playback behavior
}

template <typename T>
T ApplyAxisMask(const T& Unmasked, EPatternAxisMaskFlags Flags, float MaskedValue = 0.f)
{
	T MaskedResult = Unmasked;
	if (EnumHasAnyFlags(Flags, EPatternAxisMaskFlags::X))
	{
		MaskedResult.X = MaskedValue;
	}
	if (EnumHasAnyFlags(Flags, EPatternAxisMaskFlags::Y))
	{
		MaskedResult.Y = MaskedValue;
	}
	if (EnumHasAnyFlags(Flags, EPatternAxisMaskFlags::Z))
	{
		MaskedResult.Z = MaskedValue;
	}
	return MaskedResult;
}

template <>
FRotator ApplyAxisMask(const FRotator& Unmasked, EPatternAxisMaskFlags Flags, float MaskedValue)
{
	FRotator MaskedResult = Unmasked;
	if (EnumHasAnyFlags(Flags, EPatternAxisMaskFlags::X))
	{
		MaskedResult.Roll = MaskedValue;
	}
	if (EnumHasAnyFlags(Flags, EPatternAxisMaskFlags::Y))
	{
		MaskedResult.Pitch = MaskedValue;
	}
	if (EnumHasAnyFlags(Flags, EPatternAxisMaskFlags::Z))
	{
		MaskedResult.Yaw = MaskedValue;
	}
	return MaskedResult;
}

FTransform UPathedMovementPatternBase::CalcTargetRelativeTransform(float OverallPathProgress, const FTransform& CurTargetTransform) const
{
	//@todo DanH: When doing debug drawing, when a pattern is shorter and/or loops more, it loses resolution. Can't just always calculate as though OverallPathProgress == PatternProgress
	//		though because we still depend on the calculated result. Maybe I shouldn't bother with that and just let the aggregate one calc on its own?
	
	if (NumLoopsPerPath <= 0 || StartAtPathProgress >= EndAtPathProgress || OverallPathProgress <= StartAtPathProgress)
	{
		return FTransform();
	}
	
	// How far into the current loop of this specific pattern are we?
	const float PathProgressSinceStart = FMath::Min(EndAtPathProgress, OverallPathProgress) - StartAtPathProgress;
	const float PathProgressPerPatternLoop = (EndAtPathProgress - StartAtPathProgress) / NumLoopsPerPath;
	float CurLoopPathProgress = FMath::Fmod(PathProgressSinceStart, PathProgressPerPatternLoop);
	if (CurLoopPathProgress == 0.f && PathProgressSinceStart > 0.f)
	{
		// Treat progress that matches the per-loop span exactly as 100%, not 0%
		CurLoopPathProgress = PathProgressPerPatternLoop;
	}

	// If each loop is there and back, the actual progress needs to flip after completing half the span
	if (PerLoopBehavior == EPathedPhysicsPlaybackBehavior::ThereAndBack)
	{
		// ThereAndBack means we actually progress twice as fast as a OneShot
		CurLoopPathProgress *= 2.f;
		
		const float ReversePathProgress = CurLoopPathProgress - PathProgressPerPatternLoop;
		if (ReversePathProgress > 0.f)
		{
			CurLoopPathProgress = PathProgressPerPatternLoop - ReversePathProgress;
		}
	}

	// Now convert from overall path progress to progress for this pattern
	const float PatternProgress = CurLoopPathProgress / PathProgressPerPatternLoop;
	const FTransform UnmaskedTarget = CalcUnmaskedTargetRelativeTransform(PatternProgress, CurTargetTransform);

	if (bOrientComponentToPath)
	{
		//@todo DanH: Rotate so that the forward vector matches the translation vector direction
	}

	const FVector TargetLocation = ApplyAxisMask(UnmaskedTarget.GetLocation(), (EPatternAxisMaskFlags)TranslationMasks);
	const FRotator TargetRotation = ApplyAxisMask(UnmaskedTarget.GetRotation().Rotator(), (EPatternAxisMaskFlags)RotationMasks);
	const FVector TargetScale = ApplyAxisMask(UnmaskedTarget.GetScale3D(), (EPatternAxisMaskFlags)ScaleMasks, 1.f);
	return FTransform(TargetRotation, TargetLocation, TargetScale);
}

UPathedPhysicsMovementMode& UPathedMovementPatternBase::GetMovementMode() const
{
	UPathedPhysicsMovementMode* OuterMode = GetOuterUPathedPhysicsMovementMode();
	check(OuterMode);
	return *OuterMode;
}

UPathedPhysicsMoverComponent& UPathedMovementPatternBase::GetPathedMoverComp() const
{
	return GetMovementMode().GetPathedMoverComp();
}
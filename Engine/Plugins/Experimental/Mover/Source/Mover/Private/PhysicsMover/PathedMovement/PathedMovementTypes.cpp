// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/PathedMovementTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PathedMovementTypes)

bool FMutablePathedMovementProperties::IsMoving() const
{
	return MovementStartFrame >= 0;
}

bool FMutablePathedMovementProperties::IsLooping() const
{
	return PlaybackBehavior == EPathedPhysicsPlaybackBehavior::Looping || PlaybackBehavior == EPathedPhysicsPlaybackBehavior::PingPong;
}

bool FMutablePathedMovementProperties::IsPingPonging() const
{
	return PlaybackBehavior == EPathedPhysicsPlaybackBehavior::ThereAndBack || PlaybackBehavior == EPathedPhysicsPlaybackBehavior::PingPong;
}

void FMutablePathedMovementProperties::NetSerialize(FArchive& Ar)
{
	Ar << MovementStartFrame;
	Ar << bIsInReverse;
	Ar << bIsJointEnabled;
	Ar << PlaybackBehavior;
	Ar << PathOrigin;
}

void FMutablePathedMovementProperties::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("MovementStartFrame: %d | ", MovementStartFrame);
	Out.Appendf("bIsInReverse: %hs | ", *LexToString(bIsInReverse));
	Out.Appendf("bIsJointEnabled: %hs | ", *LexToString(bIsJointEnabled));
	Out.Appendf("PlaybackBehavior: %hs | ", *StaticEnum<EPathedPhysicsPlaybackBehavior>()->GetValueAsString(PlaybackBehavior));
	Out.Appendf("PathOrigin: %hs\n", *PathOrigin.ToHumanReadableString());
}

bool FMutablePathedMovementProperties::operator==(const FMutablePathedMovementProperties& Other) const
{
	return MovementStartFrame == Other.MovementStartFrame
		&& bIsInReverse == Other.bIsInReverse
		&& bIsJointEnabled == Other.bIsJointEnabled
		&& PlaybackBehavior == Other.PlaybackBehavior
		&& PathOrigin.Equals(Other.PathOrigin);
}

bool FPathedPhysicsMovementInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Props.NetSerialize(Ar);
	
	bOutSuccess = true;
	return true;
}

void FPathedPhysicsMovementInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Props.ToString(Out);
}

bool FPathedPhysicsMovementInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FPathedPhysicsMovementInputs& TypedAuthority = static_cast<const FPathedPhysicsMovementInputs&>(AuthorityState);
	return Props != TypedAuthority.Props;
}

void FPathedPhysicsMovementInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	if (Pct < .5f)
	{
		*this = static_cast<const FPathedPhysicsMovementInputs&>(From);
	}
	else
	{
		*this = static_cast<const FPathedPhysicsMovementInputs&>(To);
	}
}

void FPathedPhysicsMovementInputs::Merge(const FMoverDataStructBase& From)
{
	const FPathedPhysicsMovementInputs& TypedFrom = static_cast<const FPathedPhysicsMovementInputs&>(From);
	//@todo DanH: What, if anything, do I actually want to merge on the mutable props?
}

bool FPathedPhysicsMovementState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << LastStopPlaybackTime;
	Ar << CurrentProgress;
	MutableProps.NetSerialize(Ar);
	
	bOutSuccess = true;
	return true;
}

void FPathedPhysicsMovementState::ToString(FAnsiStringBuilderBase& Out) const
{
	FMoverDataStructBase::ToString(Out);

	Out.Appendf("PlaybackTime: %.2f | ", LastStopPlaybackTime);
	Out.Appendf("CurrentProgress: %.2f | ", CurrentProgress);
	MutableProps.ToString(Out);
}

bool FPathedPhysicsMovementState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FPathedPhysicsMovementState& TypedAuthority = static_cast<const FPathedPhysicsMovementState&>(AuthorityState);

	static constexpr float PlaybackTimeErrorTolerance = 0.05f;
	static constexpr float ProgressErrorTolerance = 0.05f;

	if (MutableProps != TypedAuthority.MutableProps ||
		!FMath::IsNearlyEqual(LastStopPlaybackTime, TypedAuthority.LastStopPlaybackTime, PlaybackTimeErrorTolerance) ||
		!FMath::IsNearlyEqual(CurrentProgress, TypedAuthority.CurrentProgress, ProgressErrorTolerance))
	{
		return true;
	}

	return false;
}

void FPathedPhysicsMovementState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FPathedPhysicsMovementState& TypedFrom = static_cast<const FPathedPhysicsMovementState&>(From);
	const FPathedPhysicsMovementState& TypedTo = static_cast<const FPathedPhysicsMovementState&>(To);

	MutableProps = TypedTo.MutableProps;
	LastStopPlaybackTime = FMath::Lerp(TypedFrom.LastStopPlaybackTime, TypedTo.LastStopPlaybackTime, Pct);
	CurrentProgress = FMath::Lerp(TypedFrom.CurrentProgress, TypedTo.CurrentProgress, Pct);
}
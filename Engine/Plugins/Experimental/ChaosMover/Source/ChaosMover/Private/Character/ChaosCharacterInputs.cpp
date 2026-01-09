// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/Character/ChaosCharacterInputs.h"

bool FChaosMoverLaunchInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << LaunchVelocityOrImpulse;
	Ar << Mode;

	bOutSuccess = true;
	return true;
}

void FChaosMoverLaunchInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("LaunchVelocityOrImpulse: X=%.2f Y=%.2f Z=%.2f |", LaunchVelocityOrImpulse.X, LaunchVelocityOrImpulse.Y, LaunchVelocityOrImpulse.Z);
	Out.Appendf("Mode: %u/n", Mode);
}

bool FChaosMoverLaunchInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FChaosMoverLaunchInputs& TypedAuthority = static_cast<const FChaosMoverLaunchInputs&>(AuthorityState);
	return Mode != TypedAuthority.Mode || LaunchVelocityOrImpulse != TypedAuthority.LaunchVelocityOrImpulse;
}

void FChaosMoverLaunchInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	if (Pct < .5f)
	{
		*this = static_cast<const FChaosMoverLaunchInputs&>(From);
	}
	else
	{
		*this = static_cast<const FChaosMoverLaunchInputs&>(To);
	}
}

void FChaosMoverLaunchInputs::Merge(const FMoverDataStructBase& From)
{

}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

namespace AnimationMath
{
	// Reference https://theorangeduck.com/page/spring-roll-call 

	FORCEINLINE float SpringHalfLifeToDamping(float HalfLife, float eps = 1e-5f)
	{
		return (4.0f * 0.69314718056f) / (HalfLife + eps);
	}

	FORCEINLINE FVector QuatToScaledAngleAxis(const FQuat& Q)
	{
		FQuat QLog = Q.Log();
		return 2.0f * FVector(QLog.X, QLog.Y, QLog.Z);
	}

	FORCEINLINE FQuat QuatFromScaledAngleAxis(FVector V)
	{
		V /= 2.0f;
		FQuat QLog(V.X, V.Y, V.Z, 0.0f);
		return QLog.Exp();
	}

	// Simplified version of FMath::CriticallyDampedSmoothing where v_goal is assumed to be 0
	template<typename T>
	FORCEINLINE void SimpleSpringDamper(
		T& x,
		T& v,
		T x_goal,
		float halflife,
		float dt)
	{
		float y = SpringHalfLifeToDamping(halflife) / 2.0f;
		T j0 = x - x_goal;
		T j1 = v + j0 * y;
		float eydt = FMath::InvExpApprox(y * dt);

		x = eydt * (j0 + j1 * dt) + x_goal;
		v = eydt * (v - j1 * y * dt);
	}

	// Specialized quaternion version, uses FVector for velocity
	FORCEINLINE void SimpleSpringDamperQuat(
		FQuat& InOutRotation,
		FVector& InOutAngularVelocity,
		const FQuat& TargetRotation,
		float HalfLife,
		float DeltaTime)
	{
		float y = SpringHalfLifeToDamping(HalfLife) / 2.0f;

		FQuat diff = InOutRotation * TargetRotation.Inverse();
		diff.EnforceShortestArcWith(FQuat::Identity);
		FVector j0 = QuatToScaledAngleAxis(diff);
		FVector j1 = InOutAngularVelocity + j0 * y;

		float eydt = FMath::InvExpApprox(y * DeltaTime);

		InOutRotation = QuatFromScaledAngleAxis(eydt * (j0 + j1 * DeltaTime)) * TargetRotation;
		InOutAngularVelocity = eydt * (InOutAngularVelocity - j1 * y * DeltaTime);
	}

	// Specialized quaternion damper, similar to FMath::ExponentialSmoothingApprox but for quats
	FORCEINLINE FQuat Damper(const FQuat& CurrentRotation, const FQuat& TargetRotation, float HalfLife, float DeltaTime, float Eps = 1e-5f)
	{
		if (HalfLife < Eps)
			return TargetRotation;

		return FQuat::Slerp(CurrentRotation, TargetRotation, 1.0f - FMath::InvExpApprox((0.69314718056f * DeltaTime) / (HalfLife)));
	}
}
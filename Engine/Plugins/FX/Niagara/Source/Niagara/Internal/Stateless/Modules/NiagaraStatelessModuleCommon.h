// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"

namespace NiagaraStateless
{
	//-TODO: Examine structure padding, FQuat4f / UObject*
	struct FPhysicsBuildData
	{
		static FName GetName() { return FName("FPhysicsBuildData"); }

		FNiagaraStatelessRangeFloat		MassRange = FNiagaraStatelessRangeFloat(1.0f);
		FNiagaraStatelessRangeFloat		DragRange = FNiagaraStatelessRangeFloat(0.0f);

		ENiagaraCoordinateSpace			VelocityCoordinateSpace = ENiagaraCoordinateSpace::Local;
		FNiagaraStatelessRangeVector3	VelocityRange = FNiagaraStatelessRangeVector3(FVector3f::ZeroVector);
		FNiagaraStatelessRangeFloat		LinearVelocityScale = FNiagaraStatelessRangeFloat(1.0f);

		ENiagaraCoordinateSpace			WindCoordinateSpace = ENiagaraCoordinateSpace::Local;
		FNiagaraStatelessRangeVector3	WindRange = FNiagaraStatelessRangeVector3(FVector3f::ZeroVector);

		ENiagaraCoordinateSpace			AccelerationCoordinateSpace = ENiagaraCoordinateSpace::Local;
		FNiagaraStatelessRangeVector3	AccelerationRange = FNiagaraStatelessRangeVector3(FVector3f::ZeroVector);

		FNiagaraStatelessRangeVector3	GravityRange = FNiagaraStatelessRangeVector3(FVector3f::ZeroVector);

		bool							bConeVelocity = false;
		ENiagaraCoordinateSpace			ConeCoordinateSpace = ENiagaraCoordinateSpace::Local;
		FQuat4f							ConeQuat = FQuat4f::Identity;
		FNiagaraStatelessRangeFloat		ConeVelocityRange = FNiagaraStatelessRangeFloat(0.0f);
		float							ConeOuterAngle = 0.0f;
		float							ConeInnerAngle = 0.0f;
		float							ConeVelocityFalloff = 0.0f;

		bool							bPointVelocity = false;
		ENiagaraCoordinateSpace			PointCoordinateSpace = ENiagaraCoordinateSpace::Local;
		FNiagaraStatelessRangeFloat		PointVelocityRange = FNiagaraStatelessRangeFloat(0.0f);
		float							PointVelocityMax = 0.0f;
		FVector3f						PointOrigin = FVector3f::ZeroVector;

		bool							bNoiseEnabled = false;
		float							NoiseStrength = 0.0f;
		float							NoiseFrequency = 0.0f;
	};
}

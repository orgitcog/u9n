// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Box.h"
#include "Math/BoxSphereBounds.h"
#include "Math/UnrealMathUtility.h"

namespace PCGPointHelpers
{
	FORCEINLINE FBox GetLocalBounds(const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		return FBox(InBoundsMin, InBoundsMax);
	}

	FORCEINLINE void SetLocalBounds(const FBox& InBounds, FVector& OutBoundsMin, FVector& OutBoundsMax)
	{
		OutBoundsMin = InBounds.Min;
		OutBoundsMax = InBounds.Max;
	}

	FORCEINLINE FBox GetLocalDensityBounds(float InSteepness, const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		return FBox((2 - InSteepness) * InBoundsMin, (2 - InSteepness) * InBoundsMax);
	}

	FORCEINLINE FVector GetLocalCenter(const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		return (InBoundsMax + InBoundsMin) / 2.0;
	}

	FORCEINLINE FVector GetExtents(const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		return (InBoundsMax - InBoundsMin) / 2.0;
	}

	FORCEINLINE FBoxSphereBounds GetDensityBounds(const FTransform& InTransform, float InSteepness, const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		if (InTransform.IsRotationNormalized())
		{
			return FBoxSphereBounds(PCGPointHelpers::GetLocalDensityBounds(InSteepness, InBoundsMin, InBoundsMax).TransformBy(InTransform));
		}
		else
		{
			FTransform TranslationAndScale = InTransform;
			TranslationAndScale.SetRotation(FQuat::Identity);
			return FBoxSphereBounds(PCGPointHelpers::GetLocalDensityBounds(InSteepness, InBoundsMin, InBoundsMax).TransformBy(TranslationAndScale));
		}
	}
	
	FORCEINLINE void SetExtents(const FVector& InExtents, FVector& InOutBoundsMin, FVector& InOutBoundsMax)
	{
		const FVector Center = PCGPointHelpers::GetLocalCenter(InOutBoundsMin, InOutBoundsMax);
		InOutBoundsMin = Center - InExtents;
		InOutBoundsMax = Center + InExtents;
	}
		
	FORCEINLINE FVector GetScaledExtents(const FTransform& InTransform, const FVector& InBoundsMin, const FVector& InBoundsMax)
	{ 
		return PCGPointHelpers::GetExtents(InBoundsMin, InBoundsMax) * InTransform.GetScale3D();
	}

	FORCEINLINE void SetLocalCenter(const FVector& InCenter, FVector& InOutBoundsMin, FVector& InOutBoundsMax)
	{
		const FVector Delta = InCenter - PCGPointHelpers::GetLocalCenter(InOutBoundsMin, InOutBoundsMax);
		InOutBoundsMin += Delta;
		InOutBoundsMax += Delta;
	}

	FORCEINLINE FVector GetLocalSize(const FVector& InBoundsMin, const FVector& InBoundsMax)
	{ 
		return InBoundsMax - InBoundsMin; 
	}
	
	FORCEINLINE FVector GetScaledLocalSize(const FTransform& InTransform, const FVector& InBoundsMin, const FVector& InBoundsMax)
	{ 
		return PCGPointHelpers::GetLocalSize(InBoundsMin, InBoundsMax) * InTransform.GetScale3D();
	}

	FORCEINLINE void ApplyScaleToBounds(FTransform& InOutTransform, FVector& InOutBoundsMin, FVector& InOutBoundsMax)
	{
		const FVector PointScale = InOutTransform.GetScale3D();
		InOutTransform.SetScale3D(PointScale.GetSignVector());
		InOutBoundsMin *= PointScale.GetAbs();
		InOutBoundsMax *= PointScale.GetAbs();
	}

	FORCEINLINE void ResetPointCenter(const FVector& BoundsRatio, FTransform& InOutTransform, FVector& InOutBoundsMin, FVector& InOutBoundsMax)
	{
		const FVector NewCenterLocal = FMath::Lerp(InOutBoundsMin, InOutBoundsMax, BoundsRatio);

		InOutBoundsMin -= NewCenterLocal;
		InOutBoundsMax -= NewCenterLocal;

		InOutTransform.SetLocation(InOutTransform.GetLocation() + InOutTransform.TransformVector(NewCenterLocal));
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

#if WITH_EDITOR
struct FNiagaraStatelessDrawDebugContext
{
	UWorld*		World = nullptr;
	FTransform	LocalToWorldTransform = FTransform::Identity;
	FTransform	WorldTransform = FTransform::Identity;

	NIAGARA_API void DrawArrow(const FVector& Origin, const FVector& DirectionWithLength, const FColor& Color = FColor::White) const;
	NIAGARA_API void DrawBox(const FVector& Center, const FVector& Extent, const FColor& Color = FColor::White, const FQuat& Rotation = FQuat::Identity) const;
	NIAGARA_API void DrawCone(const FVector& Origin, const FQuat& Rotation, float Angle, float Length, const FColor& Color = FColor::White) const;
	NIAGARA_API void DrawCylinder(float CylinderHeight, float CylinderRadius, float CylinderHeightMidpoint, const FColor& Color = FColor::White) const;
	NIAGARA_API void DrawCircle(const FVector& Center, const float Radius, const FColor& Color = FColor::White) const;
	NIAGARA_API void DrawSphere(const FVector& Center, const float Radius, const FColor& Color = FColor::White) const;
};
#endif

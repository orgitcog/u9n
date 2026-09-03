// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/PhysicsBodyInstanceOwnerInterface.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"

IPhysicsBodyInstanceOwner* IPhysicsBodyInstanceOwner::GetPhysicsBodyInstandeOwnerFromHitResult(const FHitResult& Result)
{
	if (IPhysicsBodyInstanceOwnerResolver* PhysicsObjectOwnerResolver = Result.PhysicsObject ? Cast<IPhysicsBodyInstanceOwnerResolver>(Result.PhysicsObjectOwner.Get()) : nullptr)
	{
		return PhysicsObjectOwnerResolver->ResolvePhysicsBodyInstanceOwner(Result.PhysicsObject);
	}
	return nullptr;
}

IPhysicsBodyInstanceOwner* IPhysicsBodyInstanceOwner::GetPhysicsBodyInstandeOwnerFromOverlapResult(const FOverlapResult& Result)
{
	if (IPhysicsBodyInstanceOwnerResolver* PhysicsObjectOwnerResolver = Result.PhysicsObject ? Cast<IPhysicsBodyInstanceOwnerResolver>(Result.PhysicsObjectOwner.Get()) : nullptr)
	{
		return PhysicsObjectOwnerResolver->ResolvePhysicsBodyInstanceOwner(Result.PhysicsObject);
	}
	return nullptr;
}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Containers/Array.h"
#include "Chaos/PhysicsObject.h"
#include "PhysicsBodyInstanceOwnerInterface.generated.h"

class UObject;
class UPhysicalMaterial;
class IPhysicsBodyInstanceOwner;
struct FHitResult;
struct FOverlapResult;
struct FPhysicalMaterialMaskParams;
enum ECollisionResponse : int;
enum ECollisionChannel : int;

UINTERFACE(MinimalAPI)
class UPhysicsBodyInstanceOwnerResolver : public UInterface
{
	GENERATED_BODY()
};

class IPhysicsBodyInstanceOwnerResolver
{
	GENERATED_BODY()

public:
	virtual IPhysicsBodyInstanceOwner* ResolvePhysicsBodyInstanceOwner(Chaos::FConstPhysicsObjectHandle PhysicsObject) = 0;
};

/** Interface representing the owner of a FBodyInstance (used when the owner is not an UPrimitiveComponent). */
class IPhysicsBodyInstanceOwner
{
public:
	virtual ~IPhysicsBodyInstanceOwner() = default;

	/** Returns the IPhysicsBodyInstanceOwner based on a given hit result. */
	static ENGINE_API IPhysicsBodyInstanceOwner* GetPhysicsBodyInstandeOwnerFromHitResult(const FHitResult& Result);

	/** Returns the IPhysicsBodyInstanceOwner based on a given overlap result. */
	static ENGINE_API IPhysicsBodyInstanceOwner* GetPhysicsBodyInstandeOwnerFromOverlapResult(const FOverlapResult& OverlapResult);

	/** Whether the physics is static. */
	virtual bool IsStaticPhysics() const = 0;

	/** Source object for this body. */
	virtual UObject* GetSourceObject() const = 0;

	/** Find the correct PhysicalMaterial for simple geometry on this body (used by FBodyInstance::GetSimplePhysicalMaterial). */
	virtual UPhysicalMaterial* GetPhysicalMaterial() const = 0;

	/** Get the complex PhysicalMaterials array for this body (used by FBodyInstance::GetComplexPhysicalMaterials). */
	virtual void GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& OutPhysMaterials, TArray<FPhysicalMaterialMaskParams>* OutPhysMaterialMasks) const = 0;

	/** Gets the response type given a specific channel. */
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const = 0;
};
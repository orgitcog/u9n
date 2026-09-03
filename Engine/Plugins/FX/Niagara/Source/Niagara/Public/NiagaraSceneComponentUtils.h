// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "Components/SceneComponent.h"
#include "UObject/Object.h"
#include "PrimitiveComponentId.h"

class FColorVertexBuffer;
class UNiagaraComponent;
class UStaticMesh;

// Helper class to abstract how we need to search the scene for components in various data interfaces
// This is temporary until scene graph is folded into the core engine, or we have more official APIs to abstract Actors/Entity/Desc
// DO NOT USE THIS IN EXTERNAL CODE as it is subject to change
class INiagaraSceneComponentUtils
{
public:
	virtual ~INiagaraSceneComponentUtils() = default;

	// Resolve the static mesh from what the interface's owner object
	virtual void ResolveStaticMesh(bool bRecurseParents, UObject*& OutComponent, UStaticMesh*& OutStaticMesh) const = 0;
	// Resolve the static mesh from the provided object which could be a static mesh / component / actor / entity / etc
	virtual void ResolveStaticMesh(UObject* ObjectFrom, bool bRecurseParents, UObject*& OutComponent, UStaticMesh*& OutStaticMesh) const = 0;
	// Get the component transform and any ISM instance transforms
	virtual bool GetStaticMeshTransforms(UObject* Component, FTransform& OutComponentTransform, TArray<FTransform>& OutInstanceTransforms) const = 0;
	// Get the statich mesh override vertex colors, if any
	virtual FColorVertexBuffer* GetStaticMeshOverrideColors(UObject* Component, int32 LODIndex) const = 0;

	// Get the FPrimitiveComponentId for the provided component
	virtual FPrimitiveComponentId GetPrimitiveSceneId(UObject* Component) const = 0;
	// Get the physics lineary velocit for the provided component
	virtual FVector GetPhysicsLinearVelocity(UObject* Component) const = 0;
};

// Implementation for AActor & UActorComponents
class FNiagaraActorSceneComponentUtils final : public INiagaraSceneComponentUtils
{
public:
	FNiagaraActorSceneComponentUtils(UNiagaraComponent* OwnerComponent);

	// Resolve the static mesh from what the interface's owner object
	virtual void ResolveStaticMesh(bool bRecurseParents, UObject*& OutComponent, UStaticMesh*& OutStaticMesh) const override;
	// Resolve the static mesh from the provided object which could be a static mesh / component / actor / entity / etc
	virtual void ResolveStaticMesh(UObject* ObjectFrom, bool bRecurseParents, UObject*& OutComponent, UStaticMesh*& OutStaticMesh) const override;
	// Get the component transform and any ISM instance transforms
	virtual bool GetStaticMeshTransforms(UObject* Component, FTransform& OutComponentTransform, TArray<FTransform>& OutInstanceTransforms) const override;
	// Get the statich mesh override vertex colors, if any
	virtual FColorVertexBuffer* GetStaticMeshOverrideColors(UObject* Component, int32 LODIndex) const override;

	// Get the FPrimitiveComponentId for the provided component
	virtual FPrimitiveComponentId GetPrimitiveSceneId(UObject* Component) const override;
	// Get the physics lineary velocit for the provided component
	virtual FVector GetPhysicsLinearVelocity(UObject* Component) const override;

private:
	TWeakObjectPtr<USceneComponent> WeakOwnerComponent;
};

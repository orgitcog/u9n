// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigPhysicsData.h"

#include "Rigs/RigPhysics.h"
#include "Rigs/RigHierarchyDefines.h"
#include "Rigs/RigHierarchyComponents.h"

#include "RigPhysicsSolverComponent.generated.h"

#define UE_API CONTROLRIGPHYSICS_API

// A solver coordinates the physical movement of bodies
USTRUCT(BlueprintType)
struct FRigPhysicsSolverComponent : public FRigBaseComponent
{
public:

	GENERATED_BODY()
	DECLARE_RIG_COMPONENT_METHODS(FRigPhysicsSolverComponent)

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Physics, meta = (ShowOnlyInnerProperties))
	FRigPhysicsSolverSettings SolverSettings;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Physics, meta = (ShowOnlyInnerProperties))
	FRigPhysicsSimulationSpaceSettings SimulationSpaceSettings;

	// If we have to reset, then we can be told to track the input kinematically for a number of
	// frames - this will be set > 0 and will be decremented each update. When it reaches 0, things
	// can go to the desired simulation state.
	int32 TrackInputCounter = 0;

	UE_API virtual void Save(FArchive& Ar) override;
	UE_API virtual void Load(FArchive& Ar) override;

#if WITH_EDITOR
	UE_API virtual const FSlateIcon& GetIconForUI() const override;
#endif

	UE_API virtual void OnAddedToHierarchy(URigHierarchy* InHierarchy, URigHierarchyController* InController) override;

	virtual FName GetDefaultComponentName() const override { return GetDefaultName(); }

	static FName GetDefaultName() { return TEXT("PhysicsSolver"); }

};

#undef UE_API

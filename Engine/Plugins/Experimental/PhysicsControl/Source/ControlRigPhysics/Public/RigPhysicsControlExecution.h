// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigPhysicsExecution.h"
#include "RigPhysicsControlComponent.h"
#include "PhysicsControlData.h"

#include "Units/Execution/RigUnit_DynamicHierarchy.h"

#include "RigPhysicsControlExecution.generated.h"

#define UE_API CONTROLRIGPHYSICS_API

/**
 * Adds a new physics control as a component on the owner element.
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta = (Category = "RigPhysics", NodeColor = "1.0 0.6 0.3", DisplayName = "Add Physics Control Component", Keywords = "Construction,Create,New,Joint,Control,Physics", Varying))
struct FRigUnit_AddPhysicsControl : public FRigUnit_PhysicsBaseMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	/**
	 * The owner of the newly created component (must be set/valid)
	 */
	UPROPERTY(meta = (Input, BoneName))
	FRigElementKey Owner;

	UPROPERTY(meta = (Output))
	FRigComponentKey ControlComponentKey;

	// The optional body that "does" the controlling - though if it is dynamic then it can move too
	UPROPERTY(meta = (Input))
	FRigComponentKey ParentBodyComponentKey;

	// The body that is controlled
	UPROPERTY(meta = (Input))
	FRigComponentKey ChildBodyComponentKey;

	/** Describes the initial strength etc of the new control */
	UPROPERTY(meta = (Input))
	FPhysicsControlData ControlData;

	/** Fine control over the control strengths etc */
	UPROPERTY(meta = (Input))
	FPhysicsControlMultiplier ControlMultiplier;

	/** Describes the initial target for the new control */
	UPROPERTY(meta = (Input))
	FPhysicsControlTarget ControlTarget;

	/** Which set to include the control in (optional). Note that it automatically gets added to the set "All" */
	//UPROPERTY(meta = (Input))
	//FName Set;
};

// Sets the control data for a physics control
USTRUCT(meta = (DisplayName = "Set Physics Control Data", Varying))
struct FRigUnit_HierarchySetControlData : public FRigUnit_PhysicsBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetControlData()
	{
		PhysicsControlComponentKey.ElementKey.Type = ERigElementType::Bone;
		PhysicsControlComponentKey.Name = FRigPhysicsControlComponent::GetDefaultName();
	}

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRigComponentKey PhysicsControlComponentKey;

	UPROPERTY(meta = (Input))
	FPhysicsControlData ControlData;
};

// Sets the target for a physics control
USTRUCT(meta = (DisplayName = "Set Physics Control Target", Varying))
struct FRigUnit_HierarchySetControlTarget : public FRigUnit_PhysicsBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetControlTarget()
	{
		PhysicsControlComponentKey.ElementKey.Type = ERigElementType::Bone;
		PhysicsControlComponentKey.Name = FRigPhysicsControlComponent::GetDefaultName();
	}

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRigComponentKey PhysicsControlComponentKey;

	UPROPERTY(meta = (Input))
	FPhysicsControlTarget ControlTarget;
};

// Sets the multipliers for a physics control
USTRUCT(meta = (DisplayName = "Set Physics Control Multiplier", Varying))
struct FRigUnit_HierarchySetControlMultiplier : public FRigUnit_PhysicsBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetControlMultiplier()
	{
		PhysicsControlComponentKey.ElementKey.Type = ERigElementType::Bone;
		PhysicsControlComponentKey.Name = FRigPhysicsControlComponent::GetDefaultName();
	}

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRigComponentKey PhysicsControlComponentKey;

	UPROPERTY(meta = (Input))
	FPhysicsControlMultiplier ControlMultiplier;
};

#undef UE_API

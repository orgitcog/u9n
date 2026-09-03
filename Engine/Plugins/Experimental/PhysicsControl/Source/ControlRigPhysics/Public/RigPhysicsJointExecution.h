// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigPhysicsExecution.h"
#include "RigPhysicsBodyComponent.h"
#include "RigPhysicsJointComponent.h"
#include "RigPhysicsSolverComponent.h"
#include "RigPhysicsSimulation.h"

#include "Units/Execution/RigUnit_DynamicHierarchy.h"

#include "RigPhysicsJointExecution.generated.h"

#define UE_API CONTROLRIGPHYSICS_API

/**
 * Adds a new physics body as a component on the owner element.
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta = (DisplayName = "Add Physics Joint Component", Keywords = "Construction,Create,New,Body,Skeleton", Varying))
struct FRigUnit_AddPhysicsJoint : public FRigUnit_PhysicsBaseMutable
{
	GENERATED_BODY()

	FRigUnit_AddPhysicsJoint()
	{
		Owner.Type = ERigElementType::Bone;
	}

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	/**
	 * The owner of the newly created component (must be set/valid)
	 */
	UPROPERTY(meta = (Input, BoneName))
	FRigElementKey Owner;

	UPROPERTY(meta = (Output))
	FRigComponentKey PhysicsJointComponentKey;

	// The parent body of the joint. If unset, then the system will try to find a suitable body by
	// looking for a parent that contains a body that is in the same solver as the child body.
	UPROPERTY(meta = (Input))
	FRigComponentKey ParentBodyComponentKey;

	// The child body of the joint. If unset, then the system will try to find a suitable body
	UPROPERTY(meta = (Input))
	FRigComponentKey ChildBodyComponentKey;

	// The properties of the joint
	UPROPERTY(meta = (Input))
	FRigPhysicsJointData JointData;

	// Optional motor/drive associated with the physics joint
	UPROPERTY(meta = (Input))
	FRigPhysicsDriveData DriveData;
};

// Sets the joint for a physics component body
USTRUCT(meta = (DisplayName = "Set Physics Joint Properties", Varying))
struct FRigUnit_HierarchySetJointData : public FRigUnit_PhysicsBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetJointData()
	{
		PhysicsJointComponentKey.ElementKey.Type = ERigElementType::Bone;
		PhysicsJointComponentKey.Name = FRigPhysicsJointComponent::GetDefaultName();
	}

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRigComponentKey PhysicsJointComponentKey;

	UPROPERTY(meta = (Input))
	FRigPhysicsJointData JointData;
};

// Sets the joint drive for a physics component body
USTRUCT(meta = (DisplayName = "Set Physics Joint Drive Properties", Varying))
struct FRigUnit_HierarchySetJointDriveData : public FRigUnit_PhysicsBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetJointDriveData()
	{
		PhysicsJointComponentKey.ElementKey.Type = ERigElementType::Bone;
		PhysicsJointComponentKey.Name = FRigPhysicsJointComponent::GetDefaultName();
	}

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRigComponentKey PhysicsJointComponentKey;

	UPROPERTY(meta = (Input))
	FRigPhysicsDriveData DriveData;
};

// Helper to simplify creation of joint data
USTRUCT(meta = (DisplayName = "Make Joint Data", Keywords = "Construction,Create,New,Body,Skeleton", Varying))
struct FRigUnit_MakeArticulationJointData : public FRigUnit_PhysicsBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	// Degrees, twist, swing1, swing2
	// -ve indicates the limit range is free
	UPROPERTY(meta = (Input, ClampMin = "-1.0"))
	FVector AngularLimit = FVector(-1, -1, -1);

	// If limited, then this will be used to control the softness
	// -ve indicates the limit is hard
	// A value of 1 is reasonably soft
	UPROPERTY(meta = (Input, ClampMin = "-1.0"))
	FVector SoftStrength = FVector(-1, -1, -1);

	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	FVector SoftDampingRatio = FVector(1, 1, 1);

	UPROPERTY(meta = (Output))
	FRigPhysicsJointData JointData;
};

// Helper to simplify creation of articulation drive data
USTRUCT(meta = (DisplayName = "Make Articulation Drive Data", Keywords = "Construction,Create,New,Body,Skeleton", Varying))
struct FRigUnit_MakeArticulationDriveData : public FRigUnit_PhysicsBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	UPROPERTY(meta = (Input))
	// Whether to enable the angular drive
	bool bEnableAngularDrive = true;

	// The type of drive. Note that SLERP drives don't work if any axis is locked
	UPROPERTY(meta = (Input))
	TEnumAsByte<enum EAngularDriveMode::Type> AngularDriveMode = EAngularDriveMode::SLERP;

	/** The strength used to drive angular motion */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float AngularStrength = 10.0f;

	/**
	 * The amount of damping associated with the angular strength. A value of 1 Results in critically
	 * damped motion where the control drives as quickly as possible to the target without overshooting.
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float AngularDampingRatio = 1.0f;

	/**
	 * The amount of additional angular damping. This is added to the damping that comes from AngularDampingRatio
	 * and can be useful when you want damping even when AngularStrength is zero.
	 */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float AngularExtraDamping = 0.0f;

	// The amount of skeletal animation velocity to use in the targets
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float SkeletalAnimationVelocityMultiplier = 1.0f;

	UPROPERTY(meta = (Output))
	FRigPhysicsDriveData DriveData;
};


// Helper to simplify creation of drive data
USTRUCT(meta = (DisplayName = "Make Drive Data", Keywords = "Construction,Create,New,Body,Skeleton", Varying))
struct FRigUnit_MakeDriveData : public FRigUnit_PhysicsBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	UE_API virtual void Execute() override;

	// Whether to enable the linear drive (not normally used for character joints)
	UPROPERTY(meta = (Input))
	bool bEnableLinearDrive = false;

	/** The strength used to drive linear motion */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float LinearStrength = 10.0f;

	/**
	 * The amount of damping associated with the linear strength. A value of 1 Results in critically
	 * damped motion where the control drives as quickly as possible to the target without overshooting.
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float LinearDampingRatio = 1.0f;

	/**
	 * The amount of additional linear damping. This is added to the damping that comes from LinearDampingRatio
	 * and can be useful when you want damping even when LinearStrength is zero.
	 */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float LinearExtraDamping = 0.0f;

	UPROPERTY(meta = (Input))
	// Whether to enable the angular drive
	bool bEnableAngularDrive = true;

	// The type of drive. Note that SLERP drives don't work if any axis is locked
	UPROPERTY(meta = (Input))
	TEnumAsByte<enum EAngularDriveMode::Type> AngularDriveMode = EAngularDriveMode::SLERP;

	/** The strength used to drive angular motion */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float AngularStrength = 10.0f;

	/**
	 * The amount of damping associated with the angular strength. A value of 1 Results in critically
	 * damped motion where the control drives as quickly as possible to the target without overshooting.
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float AngularDampingRatio = 1.0f;

	/**
	 * The amount of additional angular damping. This is added to the damping that comes from AngularDampingRatio
	 * and can be useful when you want damping even when AngularStrength is zero.
	 */
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float AngularExtraDamping = 0.0f;

	// The amount of skeletal animation velocity to use in the targets
	UPROPERTY(meta = (Input, ClampMin = "0.0"))
	float SkeletalAnimationVelocityMultiplier = 1.0f;

	UPROPERTY(meta = (Output))
	FRigPhysicsDriveData DriveData;
};


#undef UE_API

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"

#include "Rigs/RigPhysics.h"
#include "Rigs/RigHierarchyDefines.h"

#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsEngine/ConstraintDrives.h"

#include "RigPhysicsData.generated.h"

#define UE_API CONTROLRIGPHYSICS_API

struct FRigVMExecuteContext;
struct FRigBaseElement;
struct FRigControlElement;
class URigHierarchy;
class FRigElementKeyRedirector;

const float RigPhysicsDefaultShapeSize = 10.0f;

USTRUCT(BlueprintType)
struct FRigPhysicsVisualizationSettings
{
	GENERATED_BODY()

	// Enable visualization (includes enabling low-level chaos debug draw). Note that this can be
	// overridden using CVar ControlRig.Physics.VisualizationOveride
	UPROPERTY(EditAnywhere, Category = Visualization)
	bool bEnableVisualization = true;

	UPROPERTY(EditAnywhere, Category = Visualization, meta = (ClampMin = "0.0"))
	float LineThickness = 1.0f;

	// Multiplier on the size things like limit shapes
	UPROPERTY(EditAnywhere, Category = Visualization, meta = (ClampMin = "0.0"))
	int32 ShapeSize = 1;

	UPROPERTY(EditAnywhere, Category = Visualization, meta = (ClampMin = "0.0"))
	int32 ShapeDetail = 16;

	// If visualization is enabled, show active contacts. Note that this can be overridden using
	// CVar ControlRig.Physics.DrawActiveContactsOveride
	UPROPERTY(EditAnywhere, Category = Visualization)
	bool bShowActiveContacts = true;

	// If visualization is enabled, show inactive contacts. Note that this can be overridden using
	// CVar ControlRig.Physics.DrawInactiveContactsOveride
	UPROPERTY(EditAnywhere, Category = Visualization)
	bool bShowInactiveContacts = false;
};

/** 
 * Specifies what space a kinematic target is defined in 
 * Danny TODO this is much better than the similar mechanism in PCC and RBWC (which has a 
 * bUseSkeletalAnimation) - switch them to use this.
 */
UENUM()
enum class ERigPhysicsKinematicTargetSpace : uint8
{
	// Target is in world space
	World,
	// Target is in the space of the component
	Component,
	// Target is specified relative to the bone that controls the (kinematic) body, with the offset
	// applied in the space of that body.
	OffsetInBoneSpace,
	// Target is specified relative to the bone that controls the (kinematic) body, with the offset
	// applied in world space.
	OffsetInWorldSpace,
	// Target is specified relative to the bone that controls the (kinematic) body, with the offset
	// applied in component space.
	OffsetInComponentSpace,
	// Ignore the specified target, and just track the bone
	IgnoreTarget
};

/** Specifies what space the simulation should run in */
UENUM()
enum class ERigPhysicsSimulationSpace : uint8
{
	/** Simulate in world space. Moving the skeletal mesh will generate velocity changes */
	World,
	/** 
	 * Simulate in component space. Moving the entire skeletal mesh will have no affect on velocities 
	 * unless this is explicitly enabled in the simulation space settings */
	Component,
	/**
	 * Simulate in the space of the specified bone. Moving the entire skeletal mesh and/or the Bone
	 * will have no affect on velocities unless this is explicitly enabled in the simulation space settings.
	 */
	SpaceBone,
};

/**
 * Properties common to all collision shape types
 */
USTRUCT(BlueprintType)
struct FRigPhysicsCollisionShape
{
	GENERATED_BODY()

	/** Offset used when generating contact points. This allows you to smooth out
	the Minkowski sum by radius R. Useful for making objects slide smoothly
	on top of irregularities  */
	UPROPERTY(Category = Shape, EditAnywhere, meta = (ClampMin = "0.0"))
	float RestOffset = 0.0f;

	UPROPERTY(Category = Shape, EditAnywhere)
	FName Name;

	/** True if this shape should contribute to the overall mass of the body it
		belongs to. This lets you create extra collision volumes which do not affect
		the mass properties of an object. */
	UPROPERTY(Category = Shape, EditAnywhere)
	bool bContributeToMass = true;
};

/**
 * Box collision
 */
USTRUCT(BlueprintType)
struct FRigPhysicsCollisionBox : public FRigPhysicsCollisionShape
{
	GENERATED_BODY()

	FRigPhysicsCollisionBox(const FTransform& InTM, const FVector& InExtents) : 
		TM(InTM), Extents(InExtents) {}
	FRigPhysicsCollisionBox() {}

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsCollisionBox& Data)
	{
		Ar << Data.TM;
		Ar << Data.Extents;
		return Ar;
	}

	UPROPERTY(EditAnywhere, Category = Shapes)
	FTransform TM;

	// These are the full extents of the box in each axis
	UPROPERTY(EditAnywhere, Category = Shapes)
	FVector Extents = FVector::OneVector * RigPhysicsDefaultShapeSize;
};

/**
 * Sphere collision
 */
USTRUCT(BlueprintType)
struct FRigPhysicsCollisionSphere : public FRigPhysicsCollisionShape
{
	GENERATED_BODY()

	FRigPhysicsCollisionSphere(const FTransform& InTM, const float InRadius) :
		TM(InTM), Radius(InRadius) {
	}
	FRigPhysicsCollisionSphere() {}

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsCollisionSphere& Data)
	{
		Ar << Data.TM;
		Ar << Data.Radius;
		return Ar;
	}

	UPROPERTY(EditAnywhere, Category = Shapes)
	FTransform TM;

	UPROPERTY(EditAnywhere, Category = Shapes, meta = (ClampMin = "0.0"))
	float Radius = RigPhysicsDefaultShapeSize;
};

/**
 * Capsule collision
 */
USTRUCT(BlueprintType)
struct FRigPhysicsCollisionCapsule : public FRigPhysicsCollisionShape
{
	GENERATED_BODY()

	FRigPhysicsCollisionCapsule(const FTransform& InTM, const float InRadius, const float InLength)
		: TM(InTM), Radius(InRadius), Length(InLength) {}
	FRigPhysicsCollisionCapsule() {}

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsCollisionCapsule& Data)
	{
		Ar << Data.TM;
		Ar << Data.Radius;
		Ar << Data.Length;
		return Ar;
	}

	UPROPERTY(EditAnywhere, Category = Shapes)
	FTransform TM;

	UPROPERTY(EditAnywhere, Category = Shapes, meta = (ClampMin = "0.0"))
	float Radius = RigPhysicsDefaultShapeSize;

	// This is the length of the core part of the capsule. The total length will be Length + 2 * Radius
	UPROPERTY(EditAnywhere, Category = Shapes, meta = (ClampMin = "0.0"))
	float Length = RigPhysicsDefaultShapeSize;
};

// How to combine friction/restitution values. 
UENUM(BlueprintType)
enum class ERigPhysicsCombineMode : uint8
{
	Avg,
	Min,
	Multiply,
	Max
	// The values here must match those in Chaos::FChaosPhysicsMaterial::ECombineMode
};

// Material properties of a collision shapes.
USTRUCT(BlueprintType)
struct FRigPhysicsMaterial
{
	GENERATED_BODY()

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsMaterial& Data);

	// Friction is simplified here - just one value used for static and dynamic friction. 
	UPROPERTY(EditAnywhere, Category = Material, meta = (ClampMin = "0.0"))
	float Friction = 0.5f;

	UPROPERTY(EditAnywhere, Category = Material, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Restitution = 0.0f;

	// How to combine friction values. If the materials have different combine modes, then Max is
	// used over Multiply, Multiply over Min and Min over Avg.
	UPROPERTY(EditAnywhere, Category = Material)
	ERigPhysicsCombineMode FrictionCombineMode = ERigPhysicsCombineMode::Multiply;

	// How to combine restitution values. If the materials have different combine modes, then Max is
	// used over Multiply, Multiply over Min and Min over Avg.
	UPROPERTY(EditAnywhere, Category = Material)
	ERigPhysicsCombineMode RestitutionCombineMode = ERigPhysicsCombineMode::Multiply;
};

// Collection of shapes that define the collision and (optionally) mass distribution of the body
USTRUCT(BlueprintType)
struct FRigPhysicsCollision
{
	GENERATED_BODY()

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsCollision& Data);

	UPROPERTY(EditAnywhere, Category = Shapes)
	TArray<FRigPhysicsCollisionBox> Boxes;

	UPROPERTY(EditAnywhere, Category = Shapes)
	TArray<FRigPhysicsCollisionSphere> Spheres;

	UPROPERTY(EditAnywhere, Category = Shapes)
	TArray<FRigPhysicsCollisionCapsule> Capsules;

	UPROPERTY(EditAnywhere, Category = Shapes)
	FRigPhysicsMaterial Material;

	bool IsEmpty() const { return Boxes.IsEmpty() && Spheres.IsEmpty() && Capsules.IsEmpty(); }
	int32 NumShapes() const { return Boxes.Num() + Capsules.Num() + Spheres.Num(); }
};

/**
 * These settings are used when the simulation is not done in world space. They allow the 
 * movement of the simulation space (e.g. the component) to be added to the local simulation in a controlled way.
 */
USTRUCT(BlueprintType)
struct FRigPhysicsSimulationSpaceSettings
{
	GENERATED_BODY()

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsSimulationSpaceSettings& Data);

	// Overall multiplier on the effects of simulation space movement on the simulation
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpaceMovementAmount = 1.0f;

	// Multiplier on the components of vertical velocity of the simulation space that is passed to the simulation.
	// Usually from 0.0 to 1.0 to reduce the effects of jumping and crouching on the simulation, but
	// it can be higher than 1.0 if you need to exaggerate this motion for some reason.
	UPROPERTY(EditAnywhere, Category = Settings)
	float VelocityScaleZ = 1.0f;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bClampLinearVelocity = false;

	// A clamp on the effective world-space velocity that is passed to the simulation. The default
	// value effectively means "unlimited". It is not usually required to change this but you would
	// reduce this to limit the effects of drag on the bodies in the simulation (if you have bodies
	// that have LinearDrag set to non-zero in the physics asset). Expected values in this case
	// would be somewhat less than the usual velocities of your object which is commonly a few
	// hundred for a character.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", EditCondition = "bClampLinearVelocity"))
	float MaxLinearVelocity = 10000;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bClampAngularVelocity = false;

	// A clamp on the effective world-space angular velocity that is passed to the simulation. Units
	// are radian/s, so a value of about 6.0 is one rotation per second. The default value
	// effectively means "unlimited". You would reduce this (and MaxAngularAcceleration) to limit
	// how much bodies "fly out" when the actor spins on the spot. This is especially useful if you
	// have characters than can rotate very quickly and you would probably want values around or
	// less than 10 in this case.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", EditCondition = "bClampAngularVelocity"))
	float MaxAngularVelocity = 10000;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bClampLinearAcceleration = false;

	// A clamp on the effective world-space acceleration that is passed to the simulation. The
	// default value effectively means "unlimited". This property is used to stop the bodies of the
	// simulation flying out when suddenly changing linear speed. It is useful when you have
	// characters than can changes from stationary to running very quickly such as in an FPS. A
	// common value for a character might be in the few hundreds.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", EditCondition = "bClampLinearAcceleration"))
	float MaxLinearAcceleration = 10000;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bClampAngularAcceleration = false;

	// A clamp on the effective world-space angular acceleration that is passed to the simulation.
	// Units are radian/s/s. The default value effectively means "unlimited". This has a similar
	// effect to MaxAngularVelocity, except that it is related to the flying out of bodies when the
	// rotation speed suddenly changes. A typical value for a character might be around 100.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", EditCondition = "bClampAngularAcceleration"))
	float MaxAngularAcceleration = 10000;

	// If the linear acceleration is above this threshold, the movement is treated as a teleport.
	// The calculated accelerations and velocities will be set to zero, without resetting the
	// simulation state.
	// A value of zero will disable this detection
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0"))
	float LinearAccelerationThresholdForTeleport = 10000;

	// If the angular acceleration (deg/s/s) is above this threshold, the movement is treated as a teleport.
	// The calculated accelerations and velocities will be set to zero, without resetting the
	// simulation state.
	// A value of zero will disable this detection
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0"))
	float AngularAccelerationThresholdForTeleport = 10000;

	// If the linear position change is above this threshold, the movement is treated as a teleport.
	// The calculated accelerations and velocities will be set to zero, without resetting the
	// simulation state.
	// A value of zero will disable this detection
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0"))
	float PositionChangeThresholdForTeleport = 100;

	// If the angular change (degrees) is above this threshold, the movement is treated as a teleport.
	// The calculated accelerations and velocities will be set to zero, without resetting the
	// simulation state.
	// A value of zero will disable this detection
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0"))
	float OrientationChangeThresholdForTeleport = 30;

	// How much of the simulation frame's linear velocity to pass onto the bodies (linear ether
	// drag). This relies on there being drag authored on the bodies. If set to zero, the only drag
	// will be from the body's local movement (in the simulation space).
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LinearDragMultiplier = 1.0;

	// How much of the simulation frame's angular velocity to pass onto the bodies (angular ether
	// drag). This relies on there being drag authored on the bodies. If set to zero, the only drag
	// will be from the body's local movement (in the simulation space).
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AngularDragMultiplier = 1.0f;

	// Additional linear drag from movement of the simulation space, applied to every body in
	// addition to linear drag specified on them in the physics asset. When combined with
	// ExternalLinearVelocity, this can be used to add a temporary wind-blown effect without having
	// to tune linear drag on all the bodies in the physics asset. The result is that each body has
	// a force equal to -ExternalLinearDrag * ExternalLinearVelocity applied to it, in addition to
	// all other forces. The vector is in simulation local space.
	UPROPERTY(EditAnywhere, Category = Settings)
	FVector ExternalLinearDrag = FVector::ZeroVector;

	// Additional velocity that is added to the component velocity so the simulation acts as if the
	// actor is moving at speed, even when stationary. The vector is in world space. This could be
	// used for wind effects etc. Typical values are similar to the velocity of the object or
	// effect, and usually around or less than 1000 for characters/wind.
	UPROPERTY(EditAnywhere, Category = Settings)
	FVector ExternalLinearVelocity = FVector::ZeroVector;

	// Additional angular velocity that is added to the component angular velocity. This can be used
	// to make the simulation act as if the actor is rotating even when it is not. E.g., to apply
	// physics to a character on a podium as the camera rotates around it, to emulate the podium
	// itself rotating. Vector is in world space. Units are rad/s.
	UPROPERTY(EditAnywhere, Category = Settings)
	FVector ExternalAngularVelocity = FVector::ZeroVector;
};

/**
 * Configuration/settings for the physics solver
 */
USTRUCT(BlueprintType)
struct FRigPhysicsSolverSettings
{
	GENERATED_BODY()

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsSolverSettings& Data);

	// If true, then any physics component below (the element owning this one) will be automatically
	// added to this solver, if its UseAutomaticSolver flag is set.
	UPROPERTY(EditAnywhere, Category = SolverSettings)
	bool bAutomaticallyAddPhysicsComponents = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = SolverSettings)
	ERigPhysicsSimulationSpace SimulationSpace = ERigPhysicsSimulationSpace::Component;

	// The space in which collision shapes are defined
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = SolverSettings)
	ERigPhysicsSimulationSpace CollisionSpace = ERigPhysicsSimulationSpace::Component;

	/** The bone to use for the collision or simulation space (only relevant if one is set to BoneSpace) */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = SolverSettings)
	FRigElementKey SpaceBone;

	// The collision shapes defined in the collision space - e.g. for representing a ground etc
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = SolverSettings, meta = (ShowOnlyInnerProperties))
	FRigPhysicsCollision Collision;

	UPROPERTY(EditAnywhere, Category = SolverSettings)
	FVector Gravity = FVector(0, 0, -981.0);

	/**
	 * The number of position iterations to run. The position solve is responsible for de-penetration.
	 * Increasing this will improve simulation stability, but increase the cost.
	*/
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	int32 PositionIterations = 6;

	/**
	 * The number of velocity iterations to run. The velocity solve is responsible for restitution (bounce) and friction.
	 * This should usually be 1, but could be 0 if you don't care about friction and restitution.
	*/
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	int32 VelocityIterations = 1;

	/**
	 * The number of projection iterations to run. The projection phase is a final pass over the constraints, applying
	 * a semi-physical correction to any joint errors remaining after the position and velocity solves. It can be
	 * very helpful to stabilize joint chains, but can cause issues with collision response. The projection magnitude
	 * can be controlled per-constraint in the constraint settings (assuming ProjectionIteration is not zero).
	 * This should be left as 1 in almost all cases.
	*/
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	int32 ProjectionIterations = 1;

	// This sets how or if the step size should be smoothed. A value of one will disable smoothing,
	// so that the physics simulation will match the Control Rig delta time. 
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 1))
	int32 MaxNumRollingAverageStepTimes = 1;

	// This is the margin around shapes used to detect collisions. Increasing this will increase the
	// number of inactive contacts, which will reduce the likelihood of penetration, but will also
	// increase solver cost.
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	float CollisionBoundsExpansion = 2.0f;

	// Expands the shape bounds to detect potentially active contacts when moving. Increasing this
	// (typically up to 1) will increase the number of inactive contacts, which will reduce the
	// likelihood of penetration, but will also increase solver cost.
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	float BoundsVelocityMultiplier = 1.0f;

	// The maximum margin added due to movement. Reducing this can prevent excessive numbers of inactive
	// contacts being generated.
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	float MaxVelocityBoundsExpansion = 25.0f;

	// When bodies are penetrating, this is the maximum velocity delta that can be applied in one frame.
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	float MaxDepenetrationVelocity = 0.0f;

	/**
	 * The recommended fixed timestep for the RBAN solver. Set to 0 to run with variable timestep.
	 * NOTE: If this value is non-zero and less than the current frame time, the simulation will step multiple times
	 * which increases the cost.
	 */
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	float FixedTimeStep = 0.02f;

	// The maximum number of solver steps that can be made
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 1))
	int32 MaxTimeSteps = 10;

	/**
	 * If a variable timestep is in use, this is the maximum delta time that can be used. If this is smaller than the
	 * requested total delta time then multiple steps will be used.
	 */
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = 0))
	float MaxDeltaTime = 0.02f;

	/**
	 * Whether to use the linear or non-linear solver for RBAN Joints. The linear solver is significantly cheaper than
	 * the non-linear solver when you are running multiple iterations, but is more likely to suffer from jitter.
	 * In general you should try to use the linear solver and increase the PositionIterations to improve stability if
	 * possible, only using the non-linear solver as a last resort.
	 */
	UPROPERTY(EditAnywhere, Category = SolverSettings)
	bool bUseLinearJointSolver = true;

	/**
	 * When solving joints, whether to solve the positions last (as opposed to the orientations)
	 */
	UPROPERTY(EditAnywhere, Category = SolverSettings)
	bool bSolveJointPositionsLast = true;

	/**
	 * It enables the use of multi-point contact manifolds, which are created only once at the start of each tick.
	 * When disabled, a single-point contact is generated in each solver iteration which is more expensive.
	 */
	UPROPERTY(EditAnywhere, Category = CollisionSettings)
	bool bUseManifolds = true;

	// If any object in the simulation exceeds this distance from the simulation origin, we will
	// reset the pose and velocity of the entire simulation. This is to detect problems/explosions.
	// Disabled when the value is zero.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0"))
	float PositionThresholdForReset = 0;

	// If any kinematic object in the simulation exceeds this speed, we will reset the velocities
	// of all objects in the simulation.This is to detect problems - for example if the
	// target animation has teleports etc. Disabled when the value is zero.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0"))
	float KinematicSpeedThresholdForReset = 5000.0f;

	// If any kinematic object in the simulation exceeds this acceleration, we will reset the
	// velocities of all objects in the simulation.This is to detect problems - for example if the
	// target animation has teleports etc. Disabled when the value is zero.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0.0"))
	float KinematicAccelerationThresholdForReset = 40000.0f;
};

/**
 * Properties of a body affecting its dynamics (movement without regard to collision).
 */
USTRUCT(BlueprintType)
struct FRigPhysicsDynamics
{
	GENERATED_BODY()

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsDynamics& Data);

	// Density - units g per cm^3 so this defaults to "water"
	UPROPERTY(EditAnywhere, Category = Dynamics, meta = (EditCondition = "MassOverride <= 0"))
	float Density = 1.0f;

	// The total mass of this body - will override density if positive
	UPROPERTY(EditAnywhere, Category = Dynamics)
	float MassOverride = 1.0f;

	// Use this to specify the location of the centre of mass
	UPROPERTY(EditAnywhere, Category = Dynamics)
	bool bOverrideCentreOfMass = false;

	UPROPERTY(EditAnywhere, Category = Dynamics, meta = (EditCondition = "bOverrideCentreOfMass"))
	FVector CentreOfMassOverride = FVector::ZeroVector;

	// Use this to specify the moments of inertia
	UPROPERTY(EditAnywhere, Category = Dynamics)
	bool bOverrideMomentsOfInertia = false;

	UPROPERTY(EditAnywhere, Category = Dynamics, meta = (EditCondition = "bOverrideMomentsOfInertia"))
	FVector MomentsOfInertiaOverride = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = Dynamics, meta = (ClampMin = "0.0"))
	float LinearDamping = 0.0f;

	UPROPERTY(EditAnywhere, Category = Dynamics, meta = (ClampMin = "0.0"))
	float AngularDamping = 0.0f;
};

// This represents the motor drive associated with a physics joint, that can drive the bodies
// towards a target pose.
USTRUCT(BlueprintType)
struct FRigPhysicsDriveData
{
	GENERATED_BODY()

	friend FArchive& operator<<(FArchive& Ar, FRigPhysicsDriveData& Data);

	UPROPERTY(EditAnywhere, Category = LinearDrive)
	FLinearDriveConstraint LinearDriveConstraint;

	UPROPERTY(EditAnywhere, Category = AngularDrive)
	FAngularDriveConstraint AngularDriveConstraint;

	// The amount of skeletal animation velocity to use in the targets
	UPROPERTY(EditAnywhere, Category = Drive)
	float SkeletalAnimationVelocityMultiplier = 1.0f;
};

/**
 * This represents an "articulation" - a general purpose "character joint" consisting of
 *   * A linear limit (a linear constraint), which is normally used to pin two bones together
 *   * An angular limit (an angular constraint), which is normally used to allow a limited range of rotational movement.
 * 
 * The joint is defined by a parent frame, which is attached to the parent body, and a child frame which is 
 * attached to the child body. If the limits are all zero, then these two bodies will be held in a fixed pose. 
 * Normally the angular limit will be relaxed to allow rotational movement around the joint position.
 * 
 * Most bodies in a character will have one Physics Joint which connects them to their parent in the hierarchy. 
 * However
 *   * The root of the hierarchy will not have an enabled Physics Joint
 *   * There may be times when a body has additional Physics Joints.
 */
USTRUCT(BlueprintType)
struct FRigPhysicsJointData
{
	GENERATED_BODY()

	friend FArchive& operator<<(FArchive& Ar, FRigPhysicsJointData& Data);

	// The auto-calculated offset of the parent frame from the parent body places it at the 
	// location of the child body relative to the parent body in the initial pose.
	UPROPERTY(EditAnywhere, Category = Joint)
	bool bAutoCalculateParentOffset = true;

	// The parent frame offset applied after any auto-calculation
	UPROPERTY(EditAnywhere, Category = Joint)
	FTransform ExtraParentOffset;

	// The auto-calculated offset of the child frame is for it to be co-located with the child body.
	UPROPERTY(EditAnywhere, Category = Joint)
	bool bAutoCalculateChildOffset = true;

	// The child frame offset applied after any auto-calculation
	UPROPERTY(EditAnywhere, Category = Joint)
	FTransform ExtraChildOffset;

	UPROPERTY(EditAnywhere, Category = Joint)
	FLinearConstraint LinearConstraint;

	UPROPERTY(EditAnywhere, Category = Joint)
	FConeConstraint ConeConstraint;

	UPROPERTY(EditAnywhere, Category = Joint)
	FTwistConstraint TwistConstraint;

	// Disable collisions between the parent and child bodies
	UPROPERTY(EditAnywhere, Category = Joint)
	bool bDisableCollision = true;

	// The amount of linear projection to reduce joint separation when the linear constraint is
	// locked or has a hard limit. A value of one will apply full projection, but can introduce artefacts.
	UPROPERTY(EditAnywhere, Category = Joint, meta = (ClampMin = 0, ClampMax = 1))
	float LinearProjectionAmount = 0.5f;

	// The amount of angular projection to reduce joint separation when the angular constraint is
	// locked or has a hard limit. Note that projecting back to angular limits will often break the
	// position projection, so it is normally better to disable this.
	UPROPERTY(EditAnywhere, Category = Joint, meta = (ClampMin = 0, ClampMax = 1))
	float AngularProjectionAmount = 0.0f;

	// As this is reduced to 0, the parent body becomes less affected by the motion of the child
	// body, making the joint behave in one direction.
	UPROPERTY(EditAnywhere, Category = Joint, meta = (ClampMin = 0))
	float ParentInverseMassScale = 1.0f;
};

/**
 * Basic settings of the body in relation to the solver that are not covered elsewhere
 */
USTRUCT(BlueprintType)
struct FRigPhysicsBodySolverSettings
{
	GENERATED_BODY()

	FRigPhysicsBodySolverSettings(
		FRigComponentKey InPhysicsSolverComponentKey = FRigComponentKey(), FRigElementKey InTargetBone = FRigElementKey())
		: PhysicsSolverComponentKey(InPhysicsSolverComponentKey), TargetBone(InTargetBone) {}

	friend FArchive& operator <<(FArchive& Ar, FRigPhysicsBodySolverSettings& Data);

	// Note that setting the solver component, if known, has the benefit of avoiding the need to
	// search for an automatic solver.
	UPROPERTY(EditAnywhere, Category = Solver)
	FRigComponentKey PhysicsSolverComponentKey;

	// If true (and the physics solver is not explicitly set), then this component will be added to
	// any physics solver that exists above it in the hierarchy, if that solver allows automatically
	// adding physics components.
	UPROPERTY(EditAnywhere, Category = Solver)
	bool bUseAutomaticSolver = true;

	// The bone that is used to initialize physics, as well as what to track when the body is set to be kinematic.
	// Note that if this is unset, the it will default to the parent of our owner.
	UPROPERTY(EditAnywhere, Category = Solver)
	FRigElementKey SourceBone;

	// The bone that is written to following simulation. Note that if this is unset, the it will
	// default to the parent of our owner.
	UPROPERTY(EditAnywhere, Category = Solver)
	FRigElementKey TargetBone;

	UE_API void OnRigHierarchyKeyChanged(const FRigHierarchyKey& InOldKey, const FRigHierarchyKey& InNewKey);
};

#undef UE_API

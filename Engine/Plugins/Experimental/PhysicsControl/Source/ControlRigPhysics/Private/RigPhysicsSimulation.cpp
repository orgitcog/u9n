// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsSimulation.h"
#include "RigPhysicsBodyComponent.h"
#include "RigPhysicsJointComponent.h"
#include "RigPhysicsControlComponent.h"
#include "RigPhysicsSolverComponent.h"

#include "AnimNode_RigidBodyWithControl.h"
#include "PhysicsControlPoseData.h"
#include "PhysicsControlHelpers.h"

#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Units/RigUnitContext.h"
#include "RigVMCore/RigVMExecuteContext.h"

#include "Engine/World.h"

#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsAdapters.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsJointHandle.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsSimulation.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsSimulation_Chaos.h"

#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/Experimental/ChaosInterfaceUtils.h"

#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ShapeInstance.h"
#include "Chaos/Capsule.h"
#include "Chaos/ChaosScene.h"
#include "Chaos/PBDJointConstraintUtilities.h"
#include "Chaos/Evolution/SimulationSpace.h"
#include "Chaos/PBDCollisionConstraints.h"

#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_CHAOS_VISUAL_DEBUGGER
#include "ChaosVDRuntimeModule.h"
#endif

#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY(LogRigPhysics);

TAutoConsoleVariable<float> CVarControlRigPhysicsFixedTimeStepOverride(
	TEXT("ControlRig.Physics.FixedTimeStepOverride"), -1.0f,
	TEXT("-1.0 disables the override, so the timestep authored in the simulation settings will be used (which may or may not imply a fixed timestep). A value of 0 forces a variable timestep to be used. A +ve value is used to specify a fixed timestep."));

TAutoConsoleVariable<int> CVarControlRigPhysicsMaxTimeStepsOverride(
	TEXT("ControlRig.Physics.MaxTimeStepsOverride"), -1,
	TEXT("-1 disables the override, so the max timesteps authored in the simulation settings will be used. A +ve value is used to specify the maximum number of timesteps."));

constexpr int32 ConstraintChildIndex = 0;
constexpr int32 ConstraintParentIndex = 1;

//======================================================================================================================
FRigPhysicsSimulation::FRigPhysicsSimulation(TObjectPtr<UControlRig> InOwningControlRig)
	: FRigPhysicsSimulationBase(FRigPhysicsSimulation::StaticStruct()), OwningControlRig(InOwningControlRig)
{
}

//======================================================================================================================
bool FRigPhysicsSimulation::ShouldComponentBeInSimulation(
	FRigComponentKey SolverComponentKey, FRigComponentKey ComponentKey) const
{
	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();

	const FRigPhysicsBodyComponent* PhysicsComponent = Cast<FRigPhysicsBodyComponent>(
		Hierarchy->FindComponent(ComponentKey));

	const FRigPhysicsSolverComponent* PhysicsSolverComponent = Cast<FRigPhysicsSolverComponent>(
		Hierarchy->FindComponent(SolverComponentKey));

	if (!PhysicsComponent || !PhysicsSolverComponent)
	{
		return false;
	}

	if (PhysicsComponent->BodySolverSettings.PhysicsSolverComponentKey == SolverComponentKey)
	{
		return true;
	}

	if (!PhysicsSolverComponent->SolverSettings.bAutomaticallyAddPhysicsComponents)
	{
		return false;
	}

	if (PhysicsComponent->BodySolverSettings.bUseAutomaticSolver)
	{
		FRigElementKey ElementKey = PhysicsComponent->GetElementKey();
		while (ElementKey.IsValid())
		{
			for (FRigComponentKey CK : Hierarchy->GetComponentKeys(ElementKey))
			{
				if (CK == SolverComponentKey)
				{
					return true;
				}
			}
			// Note that getting the parent of an element at the root doesn't return the top-level element
			ElementKey = Hierarchy->GetFirstParent(ElementKey);
		}

		for (FRigComponentKey CK : Hierarchy->GetTopLevelComponentKeys())
		{
			if (CK == SolverComponentKey)
			{
				return true;
			}
		}
	}
	return false;
}


//======================================================================================================================
void FRigPhysicsSimulation::InitialiseSimulation(const FRigPhysicsSolverComponent* SolverComponent)
{
	DestroyPhysicsSimulation();

	Simulation = MakeShared<ImmediatePhysics::FSimulation>();

#if CHAOS_DEBUG_NAME
	FString SimName = TEXT("ControlRigPhysics-");
	SimName += OwningControlRig->GetName();
	Simulation->SetDebugName(FName(SimName));
#endif

#if WITH_CHAOS_VISUAL_DEBUGGER
	Simulation->GetChaosVDContextData().Id = FChaosVDRuntimeModule::Get().GenerateUniqueID();
	Simulation->GetChaosVDContextData().Type = static_cast<int32>(EChaosVDContextType::Solver);
#endif

	// This is needed so that when using a fixed timestep, velocities are rewound as well as
	// positions. This is not only more accurate, but it's needed in order to get soft constraint
	// behavior (in particular, for controls) that behave fairly independently of the control-rig
	// tick rate.
	Simulation->SetRewindVelocities(true);

	// Always create a world actor at the origin, for attaching controls to. 
	SimulationActorHandle = CreateBody(
		FName(TEXT("Simulation")), FRigPhysicsCollision(), nullptr, nullptr, FTransform());
}

//======================================================================================================================
void FRigPhysicsSimulation::InitialiseControlRecords(const FRigPhysicsSolverComponent* SolverComponent)
{
	ensure(ControlRecords.IsEmpty());

	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();
	if (!Hierarchy || !SolverComponent)
	{
		return;
	}

	FRigComponentKey SolverComponentKey = SolverComponent->GetKey();

	TArray<FRigComponentKey> AllComponentKeys = Hierarchy->GetAllComponentKeys();

	for (FRigComponentKey ComponentKey : AllComponentKeys)
	{
		if (const FRigPhysicsControlComponent* ControlComponent = Cast<FRigPhysicsControlComponent>(
			Hierarchy->FindComponent(ComponentKey)))
		{
			// The authored body components may be blank, in which case we need to find the automatic ones.
			FRigControlRecord ControlRecord;
			ControlRecord.ParentBodyComponentKey = ControlComponent->ParentBodyComponentKey;
			ControlRecord.ChildBodyComponentKey = ControlComponent->ChildBodyComponentKey;

			// Automate the child
			if (!ControlRecord.ChildBodyComponentKey.IsValid())
			{
				TArray<FRigComponentKey> SiblingComponentKeys = Hierarchy->GetComponentKeys(ComponentKey.ElementKey);
				for (FRigComponentKey SiblingComponentKey : SiblingComponentKeys)
				{
					if (ShouldComponentBeInSimulation(SolverComponentKey, SiblingComponentKey))
					{
						ControlRecord.ChildBodyComponentKey = SiblingComponentKey;
						break;
					}
				}
			}

			if (ControlComponent->bUseParentBodyAsDefault)
			{
				if (!ControlRecord.ParentBodyComponentKey.IsValid())
				{
					FRigElementKey ParentElementKey = Hierarchy->GetFirstParent(ComponentKey.ElementKey);
					TArray<FRigComponentKey> ParentComponentKeys = Hierarchy->GetComponentKeys(ParentElementKey);
					for (FRigComponentKey ParentComponentKey : ParentComponentKeys)
					{
						if (ShouldComponentBeInSimulation(SolverComponentKey, ParentComponentKey))
						{
							ControlRecord.ParentBodyComponentKey = ParentComponentKey;
							break;
						}
					}
				}
			}

			if (ShouldComponentBeInSimulation(SolverComponentKey, ControlRecord.ChildBodyComponentKey))
			{
				// Here, an invalid parent component key indicates a sim-space control. 
				if (!ControlRecord.ParentBodyComponentKey.IsValid() ||
					ShouldComponentBeInSimulation(SolverComponentKey, ControlRecord.ParentBodyComponentKey))
				{
					// Just make the record for now - it will be instantiated later
					ControlRecords.Add(ComponentKey, ControlRecord);
				}
			}
		}
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::InitialiseJointRecords(const FRigPhysicsSolverComponent* SolverComponent)
{
	ensure(JointRecords.IsEmpty());

	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();
	if (!Hierarchy || !SolverComponent)
	{
		return;
	}

	FRigComponentKey SolverComponentKey = SolverComponent->GetKey();

	TArray<FRigComponentKey> AllComponentKeys = Hierarchy->GetAllComponentKeys();

	for (FRigComponentKey ComponentKey : AllComponentKeys)
	{
		if (const FRigPhysicsJointComponent* JointComponent = Cast<FRigPhysicsJointComponent>(
			Hierarchy->FindComponent(ComponentKey)))
		{
			// The authored body components may be blank, in which case we need to find the automatic ones.
			FRigJointRecord JointRecord;
			JointRecord.ParentBodyComponentKey = JointComponent->ParentBodyComponentKey;
			JointRecord.ChildBodyComponentKey = JointComponent->ChildBodyComponentKey;

			if (!JointRecord.ChildBodyComponentKey.IsValid())
			{
				TArray<FRigComponentKey> SiblingComponentKeys = Hierarchy->GetComponentKeys(ComponentKey.ElementKey);
				for (FRigComponentKey SiblingComponentKey : SiblingComponentKeys)
				{
					if (ShouldComponentBeInSimulation(SolverComponentKey, SiblingComponentKey))
					{
						JointRecord.ChildBodyComponentKey = SiblingComponentKey;
						break;
					}
				}
			}

			if (!JointRecord.ParentBodyComponentKey.IsValid())
			{
				FRigElementKey ParentElementKey = Hierarchy->GetFirstParent(ComponentKey.ElementKey);
				TArray<FRigComponentKey> ParentComponentKeys = Hierarchy->GetComponentKeys(ParentElementKey);
				for (FRigComponentKey ParentComponentKey : ParentComponentKeys)
				{
					if (ShouldComponentBeInSimulation(SolverComponentKey, ParentComponentKey))
					{
						JointRecord.ParentBodyComponentKey = ParentComponentKey;
						break;
					}
				}
			}

			if (ShouldComponentBeInSimulation(SolverComponentKey, JointRecord.ChildBodyComponentKey))
			{
				if (ShouldComponentBeInSimulation(SolverComponentKey, JointRecord.ParentBodyComponentKey))
				{
					// Just make the record for now - it will be instantiated later
					JointRecords.Add(ComponentKey, JointRecord);
				}
			}
		}
	}
}


//======================================================================================================================
void FRigPhysicsSimulation::InitialiseBodyRecords(const FRigPhysicsSolverComponent* SolverComponent)
{
	ensure(BodyRecords.IsEmpty());

	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();
	if (!Hierarchy || !SolverComponent)
	{
		return;
	}

	FRigComponentKey SolverComponentKey = SolverComponent->GetKey();

	TArray<FRigComponentKey> AllComponentKeys = Hierarchy->GetAllComponentKeys();

	// All the components in this simulation
	TArray<FRigComponentKey> UnsortedBodyComponentKeys;

	for (FRigComponentKey ComponentKey : AllComponentKeys)
	{
		if (ShouldComponentBeInSimulation(SolverComponentKey, ComponentKey))
		{
			// Just make the record for now - it will be instantiated later
			BodyRecords.Add(ComponentKey);
			UnsortedBodyComponentKeys.Add(ComponentKey);
		}
	}

	// Sort the component keys according to the traversal of their element (i.e. from root to leaf)
	SortedBodyComponentKeys.Empty(UnsortedBodyComponentKeys.Num());
	Hierarchy->Traverse([this, &UnsortedBodyComponentKeys](FRigBaseElement* Element, bool& bContinue)
		{
			const FRigElementKey& Key = Element->GetKey();
			for (const FRigComponentKey& ComponentKey : UnsortedBodyComponentKeys)
			{
				if (ComponentKey.ElementKey == Key)
				{
					SortedBodyComponentKeys.Push(ComponentKey);
				}
			}
		});
}

//======================================================================================================================
void FRigPhysicsSimulation::DestroyPhysicsSimulation()
{
	for (TPair<FRigComponentKey, FRigBodyRecord>& BodyRecordPair : BodyRecords)
	{
		FRigBodyRecord& Record = BodyRecordPair.Value;
		if (Record.ActorHandle)
		{
			Simulation->DestroyActor(Record.ActorHandle);
		}
	}
	BodyRecords.Empty();


	for (TPair<FRigComponentKey, FRigJointRecord>& JointRecordPair : JointRecords)
	{
		FRigJointRecord& Record = JointRecordPair.Value;
		if (Record.JointHandle)
		{
			Simulation->DestroyJoint(Record.JointHandle);
		}
	}
	JointRecords.Empty();

	for (TPair<FRigComponentKey, FRigControlRecord>& ControlRecordPair : ControlRecords)
	{
		FRigControlRecord& Record = ControlRecordPair.Value;
		if (Record.JointHandle)
		{
			Simulation->DestroyJoint(Record.JointHandle);
		}
	}
	ControlRecords.Empty();

	if (CollisionActorHandle)
	{
		Simulation->DestroyActor(CollisionActorHandle);
	}
	CollisionActorHandle = nullptr;

	if (SimulationActorHandle)
	{
		Simulation->DestroyActor(SimulationActorHandle);
	}
	SimulationActorHandle = nullptr;

	Simulation.Reset();
}

//======================================================================================================================
static void SetCommonProperties(const FRigPhysicsCollisionShape& Shape, FKShapeElem& ShapeElem)
{
	ShapeElem.RestOffset = Shape.RestOffset;
	ShapeElem.SetName(Shape.Name);
	ShapeElem.SetContributeToMass(Shape.bContributeToMass);
#ifdef PER_SHAPE_COLLISION
	// Note that FKShapeElem supports enabling/disabling collision per shape, but this is discarded by the immediate
	// solver.
	ShapeElem.SetCollisionEnabled(Shape.CollisionEnabled);
#endif
}

//======================================================================================================================
static Chaos::EJointMotionType GetMotionTypeFromLimitValue(const float Value)
{
	if (Value > 0.0f)
	{
		return Chaos::EJointMotionType::Limited;
	}
	else if (Value < 0.0f)
	{
		return Chaos::EJointMotionType::Free;
	}
	else
	{
		return Chaos::EJointMotionType::Locked;
	}
}

//======================================================================================================================
static bool CreateGeometry(
	const FRigPhysicsCollision&               Collision,
	const FRigPhysicsDynamics*                Dynamics,
	const Chaos::FReal                        Density,
	Chaos::FReal&                             OutMass, 
	Chaos::FVec3&                             OutInertia, 
	Chaos::FRigidTransform3&                  OutCoMTransform, 
	Chaos::FImplicitObjectPtr&                OutGeom, 
	TArray<TUniquePtr<Chaos::FPerShapeData>>& OutShapes)
{
	using namespace Chaos;

	OutMass = 0.0f;
	OutInertia = FVector::ZeroVector;
	OutCoMTransform = FTransform::Identity;

	// Set the filter to collide with everything (we use a broad phase that only contains particle
	// pairs that are explicitly set to collide)
	FBodyCollisionData BodyCollisionData;
	// @todo(chaos): we need an API for setting up filters
	BodyCollisionData.CollisionFilterData.SimFilter.Word1 = 0xFFFF;
	BodyCollisionData.CollisionFilterData.SimFilter.Word3 = 0xFFFF;

	// See FBodyInstance::BuildBodyCollisionFlags
	BodyCollisionData.CollisionFlags.bEnableQueryCollision = false;
	BodyCollisionData.CollisionFlags.bEnableSimCollisionSimple = true;
	BodyCollisionData.CollisionFlags.bEnableSimCollisionComplex = false;
	BodyCollisionData.CollisionFlags.bEnableProbeCollision = false;

	FKAggregateGeom AggGeom;
	for (const FRigPhysicsCollisionBox& Shape : Collision.Boxes)
	{
		FKBoxElem Elem(Shape.Extents.X, Shape.Extents.Y, Shape.Extents.Z);
		SetCommonProperties(Shape, Elem);
		Elem.Center = Shape.TM.GetTranslation();
		Elem.Rotation = Shape.TM.Rotator();
		AggGeom.BoxElems.Add(Elem);
	}

	for (const FRigPhysicsCollisionSphere& Shape : Collision.Spheres)
	{
		FKSphereElem Elem(Shape.Radius);
		SetCommonProperties(Shape, Elem);
		Elem.Center = Shape.TM.GetTranslation();
		// Note that there is no rotation
		AggGeom.SphereElems.Add(Elem);
	}

	for (const FRigPhysicsCollisionCapsule& Shape : Collision.Capsules)
	{
		FKSphylElem Elem(Shape.Radius, Shape.Length);
		SetCommonProperties(Shape, Elem);
		Elem.Center = Shape.TM.GetTranslation();
		Elem.Rotation = Shape.TM.Rotator();
		AggGeom.SphylElems.Add(Elem);
	}

	FGeometryAddParams AddParams;
	AddParams.CollisionData = BodyCollisionData;
	AddParams.CollisionTraceType = ECollisionTraceFlag::CTF_UseSimpleAsComplex;
	AddParams.Scale = FVector(1, 1, 1);
	AddParams.LocalTransform = FTransform::Identity; // How are these used? We will just set TM afterwards anyway
	AddParams.WorldTransform = FTransform::Identity;
	AddParams.Geometry = &AggGeom;

	TArray<Chaos::FImplicitObjectPtr> Geoms;
	FShapesArray Shapes;
	ChaosInterface::CreateGeometry(AddParams, Geoms, Shapes);

	if (Geoms.Num() == 0)
	{
		return false;
	}

	// Calculate mass properties, if we have dynamics
	if (Dynamics)
	{
		// Whether each shape contributes to mass. It would be easier if ComputeMassProperties knew
		// how to extract this info. Maybe it should be a flag in PerShapeData
		TArray<bool> bContributesToMass;
		bContributesToMass.Reserve(Shapes.Num());
		for (int32 ShapeIndex = 0; ShapeIndex < Shapes.Num(); ++ShapeIndex)
		{
			const TUniquePtr<FPerShapeData>& Shape = Shapes[ShapeIndex];
			const FKShapeElem* ShapeElem = FChaosUserData::Get<FKShapeElem>(Shape->GetUserData());
			bool bHasMass = ShapeElem && ShapeElem->GetContributeToMass();
			bContributesToMass.Add(bHasMass);
		}

		Chaos::FMassProperties MassProperties;
		ChaosInterface::CalculateMassPropertiesFromShapeCollection(
			MassProperties, Shapes, bContributesToMass, Density);

		OutMass = MassProperties.Mass;
		OutInertia = MassProperties.InertiaTensor.GetDiagonal();
		OutCoMTransform = FTransform(MassProperties.RotationOfMass, MassProperties.CenterOfMass);
	}

	// If we have multiple root shapes, wrap them in a union
	if (Geoms.Num() == 1)
	{
		OutGeom = MoveTemp(Geoms[0]);
	}
	else
	{
		OutGeom = MakeImplicitObjectPtr<FImplicitObjectUnion>(MoveTemp(Geoms));
	}

	for (TUniquePtr<FPerShapeData>& Shape : Shapes)
	{
		OutShapes.Emplace(MoveTemp(Shape));
	}

	return true;
}

//======================================================================================================================
ImmediatePhysics::FActorHandle* FRigPhysicsSimulation::CreateBody(
	const FName                        BodyName,
	const FRigPhysicsCollision&        Collision,
	const FRigPhysicsDynamics*         Dynamics,
	const FPhysicsControlModifierData* BodyData,
	const FTransform&                  BodyRelSimSpaceTM) const
{
	ImmediatePhysics::FActorSetup ActorSetup;

	if (Dynamics)
	{
		ActorSetup.ActorType = ImmediatePhysics::EActorType::DynamicActor;
		ActorSetup.bEnableGravity = true;
		ActorSetup.LinearDamping = Dynamics->LinearDamping;
		ActorSetup.AngularDamping = Dynamics->AngularDamping;
	}
	else
	{
		ActorSetup.ActorType = ImmediatePhysics::EActorType::KinematicActor;
	}

	if (BodyData)
	{
		ActorSetup.bUpdateKinematicFromSimulation = BodyData->bUpdateKinematicFromSimulation;
	}
	else
	{
		ActorSetup.bUpdateKinematicFromSimulation = false;
	}

	Chaos::FVec3 Inertia;
	Chaos::FRigidTransform3 CoMTransform;
	Chaos::FReal Mass;
	Chaos::FImplicitObjectPtr BodyGeom;
	TArray<TUniquePtr<Chaos::FPerShapeData>> BodyShapes;
	Chaos::FReal Density = Dynamics ? Dynamics->Density : 1.0f;
	// Convert from g/cm^3 to kg/cm^3
	Density *= 1e-6;

	bool bGeometryCreated = CreateGeometry(
		Collision, Dynamics, Density, Mass, Inertia, CoMTransform, BodyGeom, BodyShapes);

	// We will have created with an arbitrary density - adjust to result in the desired mass.
	ActorSetup.Mass = Mass;
	ActorSetup.Inertia = Inertia;
	if (Mass > 0.0f && Dynamics && Dynamics->MassOverride > 0.0f)
	{
		ActorSetup.Mass = Dynamics->MassOverride;
		ActorSetup.Inertia = (Dynamics->MassOverride / Mass) * Inertia;
	}

	ActorSetup.Transform = BodyRelSimSpaceTM;
	ActorSetup.CoMTransform = CoMTransform;
	ActorSetup.Geometry = MoveTemp(BodyGeom);
	ActorSetup.Shapes = MoveTemp(BodyShapes);

	ActorSetup.Material = MakeUnique<Chaos::FChaosPhysicsMaterial>();
	ActorSetup.Material->Friction = Collision.Material.Friction;
	ActorSetup.Material->StaticFriction = Collision.Material.Friction;
	ActorSetup.Material->Restitution = Collision.Material.Restitution;
	ActorSetup.Material->FrictionCombineMode =
		(Chaos::FChaosPhysicsMaterial::ECombineMode)Collision.Material.FrictionCombineMode;
	ActorSetup.Material->RestitutionCombineMode =
		(Chaos::FChaosPhysicsMaterial::ECombineMode)Collision.Material.RestitutionCombineMode;

	ImmediatePhysics::FActorHandle* ActorHandle = Simulation->CreateActor(MoveTemp(ActorSetup));
	if (!ActorHandle)
	{
		UE_LOG(LogRigPhysics, Warning,
			TEXT("Unable to create body %s"), *BodyName.ToString());
		return nullptr;
	}

	ActorHandle->SetName(BodyName);
#if CHAOS_DEBUG_NAME
	if (Chaos::FGeometryParticleHandle* ParticleHandle = ActorHandle->GetParticle())
	{
		ParticleHandle->SetDebugName(MakeShared<FString>(BodyName.ToString()));
	}
#endif

	if (bGeometryCreated)
	{
		Simulation->AddToCollidingPairs(ActorHandle);
		if (Dynamics)
		{
			// Note that particles are always created disabled. They will simulate when disabled,
			// but won't collide!
			ActorHandle->SetEnabled(true);
		}
	}
	else
	{
		Simulation->SetHasCollision(ActorHandle, false);
	}

	return ActorHandle;
}

//======================================================================================================================
void FRigPhysicsSimulation::Instantiate(const FRigPhysicsSolverComponent* SolverComponent)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_Instantiate);

	if (!bNeedToInstantiate || !OwningControlRig)
	{
		return;
	}

	// We need the simulation space in order to instantiate properly. This is not ideal, as we may
	// end up updating the simulation space data twice (thus inserting data into the history). This
	// shouldn't really matter as it will only be on the first step.
	UpdateSimulationSpaceStateAndCalculateData(SolverComponent, 0.0f);

	InitialiseSimulation(SolverComponent);

	InitialiseBodyRecords(SolverComponent);

	InitialiseJointRecords(SolverComponent);

	InitialiseControlRecords(SolverComponent);

	FRigPhysicsIgnorePairs IgnorePairs;

	InstantiatePhysicsBodies(SolverComponent, IgnorePairs);

	InstantiatePhysicsJoints(SolverComponent, IgnorePairs);

	InstantiateControls(SolverComponent, IgnorePairs);

	// This is done last as it applies IgnorePairs
	InstantiateSolverCollision(SolverComponent, IgnorePairs);

	bNeedToInstantiate = false;
}

//======================================================================================================================
ImmediatePhysics::FActorHandle* FRigPhysicsSimulation::GetActor(const FRigComponentKey& ComponentKey) const
{
	if (const FRigBodyRecord* BodyRecord = BodyRecords.Find(ComponentKey))
	{
		return BodyRecord->ActorHandle;
	}
	if (ComponentKey == PhysicsSolverComponentKey)
	{
		return SimulationActorHandle;
	}
	return nullptr;
}

//======================================================================================================================
void FRigPhysicsSimulation::InstantiateSolverCollision(
	const FRigPhysicsSolverComponent* SolverComponent,
	FRigPhysicsIgnorePairs&           IgnorePairs)
{
	// Optionally create an object to contain environment collision
	if (!SolverComponent->SolverSettings.Collision.IsEmpty())
	{
		// When we make these additional collision shapes, their actors are all considered to be at
		// the origin, with the offsets being contained in the collision shapes.
		FTransform BodyRelSimSpaceTM = ConvertCollisionSpaceTransformToSimSpace(
			SolverComponent->SolverSettings, FTransform());

		CollisionActorHandle = CreateBody(
			FName(TEXT("Environment")), SolverComponent->SolverSettings.Collision, nullptr, nullptr, BodyRelSimSpaceTM);
	}

	// Add no-collision pairs
	TArray<ImmediatePhysics_Chaos::FSimulation::FIgnorePair> ChaosIgnorePairs;

	for (const FRigPhysicsIgnorePair& IgnorePair : IgnorePairs)
	{
		ImmediatePhysics::FSimulation::FIgnorePair ChaosPair;
		ChaosPair.A = GetActor(IgnorePair.A);
		ChaosPair.B = GetActor(IgnorePair.B);
		if (ChaosPair.A && ChaosPair.B)
		{
			ChaosIgnorePairs.Add(ChaosPair);
		}
	}
	Simulation->SetIgnoreCollisionPairTable(ChaosIgnorePairs);

}

//======================================================================================================================
void FRigPhysicsSimulation::InstantiatePhysicsBodies(
	const FRigPhysicsSolverComponent* SolverComponent,
	FRigPhysicsIgnorePairs&           IgnorePairs)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_InstantiatePhysicsBodies);

	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();

	const FRigPhysicsSolverSettings& SolverSettings = SolverComponent->SolverSettings;
	const FRigPhysicsSimulationSpaceSettings& SimulationSpaceSettings = SolverComponent->SimulationSpaceSettings;

	for (TPair<FRigComponentKey, FRigBodyRecord>& BodyRecordPair : BodyRecords)
	{
		const FRigComponentKey& ComponentKey = BodyRecordPair.Key;
		FRigBodyRecord& Record = BodyRecordPair.Value;

		if (const FRigPhysicsBodyComponent* PhysicsComponent = Cast<FRigPhysicsBodyComponent>(
			Hierarchy->FindComponent(ComponentKey)))
		{
			const FRigPhysicsCollision& Collision = PhysicsComponent->Collision;
			const FRigPhysicsDynamics& Dynamics = PhysicsComponent->Dynamics;
			const FPhysicsControlModifierData& BodyData = PhysicsComponent->BodyData;

			FRigElementKey SourceKey = PhysicsComponent->BodySolverSettings.SourceBone;
			if (!SourceKey.IsValid())
			{
				SourceKey = ComponentKey.ElementKey;
			}

			// What should we do if the key is not valid? 
			if (SourceKey.IsValid())
			{
				FTransform SourceComponentSpaceTM = Hierarchy->GetGlobalTransform(SourceKey);
				const FTransform SourceSimulationSpaceTM =
					ConvertComponentSpaceTransformToSimSpace(SolverComponent->SolverSettings, SourceComponentSpaceTM);
				Record.ActorHandle = CreateBody(
					ComponentKey.ElementKey.Name, Collision, &Dynamics, &BodyData, SourceSimulationSpaceTM);
			}

			Record.TargetElementKey = PhysicsComponent->BodySolverSettings.TargetBone;
			if (!Record.TargetElementKey.IsValid())
			{
				Record.TargetElementKey = ComponentKey.ElementKey;
			}

			for (const FRigComponentKey& NoCollisionKey : PhysicsComponent->NoCollisionBodies)
			{
				IgnorePairs.Add(FRigPhysicsIgnorePair(ComponentKey, NoCollisionKey));
			}
		}
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::InstantiatePhysicsJoints(
	const FRigPhysicsSolverComponent* SolverComponent,
	FRigPhysicsIgnorePairs&           IgnorePairs)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_InstantiatePhysicsJoints);

	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();

	const FRigPhysicsSolverSettings& SolverSettings = SolverComponent->SolverSettings;
	const FRigPhysicsSimulationSpaceSettings& SimulationSpaceSettings = SolverComponent->SimulationSpaceSettings;

	// Once all the bodies are created, we can make their physics joints
	for (TPair<FRigComponentKey, FRigJointRecord>& JointRecordPair : JointRecords)
	{
		const FRigComponentKey& JointComponentKey = JointRecordPair.Key;
		FRigJointRecord& JointRecord = JointRecordPair.Value;

		if (const FRigPhysicsJointComponent* PhysicsJointComponent = Cast<FRigPhysicsJointComponent>(
			Hierarchy->FindComponent(JointComponentKey)))
		{
			const FRigPhysicsJointData& JointData = PhysicsJointComponent->JointData;

			FRigComponentKey ChildBodyComponentKey = JointRecord.ChildBodyComponentKey;
			FRigElementKey ChildBoneKey = ChildBodyComponentKey.ElementKey;

			FRigComponentKey ParentBodyComponentKey = JointRecord.ParentBodyComponentKey;
			FRigElementKey ParentBoneKey = ParentBodyComponentKey.ElementKey;

			// Joints require both parent and child to exist
			const FRigPhysicsBodyComponent* ChildPhysicsComponent =
				Cast<FRigPhysicsBodyComponent>(Hierarchy->FindComponent(ChildBodyComponentKey));
			FRigBodyRecord* ChildBodyRecord = BodyRecords.Find(ChildBodyComponentKey);
			ImmediatePhysics::FActorHandle* ChildActorHandle = ChildBodyRecord ? ChildBodyRecord->ActorHandle : nullptr;
			if (!ChildActorHandle)
			{
				continue;
			}

			const FRigPhysicsBodyComponent* ParentPhysicsComponent =
				Cast<FRigPhysicsBodyComponent>(Hierarchy->FindComponent(ParentBodyComponentKey));
			FRigBodyRecord* ParentBodyRecord = BodyRecords.Find(ParentBodyComponentKey);
			ImmediatePhysics::FActorHandle* ParentActorHandle = ParentBodyRecord ? ParentBodyRecord->ActorHandle : nullptr;
			if (!ParentActorHandle)
			{
				continue;
			}

			// Make the physics joint (joint constraint). 
			// UE likes to treat Body1 (index 0) as the child, and Body2 (index 1) as the parent
			{
				const FTransform ParentCoMTransform = ParentActorHandle->GetLocalCoMTransform();
				const FTransform ChildCoMTransform = ChildActorHandle->GetLocalCoMTransform();

				Chaos::FPBDJointSettings JointSettings;
				if (JointData.bAutoCalculateChildOffset)
				{
					JointSettings.ConnectorTransforms[0] = FTransform();
				}
				JointSettings.ConnectorTransforms[0] =
					JointData.ExtraChildOffset * JointSettings.ConnectorTransforms[0];

				if (JointData.bAutoCalculateParentOffset)
				{
					JointSettings.ConnectorTransforms[1] = Hierarchy->GetLocalTransform(ChildBoneKey, true);
				}
				JointSettings.ConnectorTransforms[1] =
					JointData.ExtraParentOffset * JointSettings.ConnectorTransforms[1];

				ImmediatePhysics::UpdateJointSettingsFromLinearConstraint(JointData.LinearConstraint, JointSettings);
				ImmediatePhysics::UpdateJointSettingsFromConeConstraint(JointData.ConeConstraint, JointSettings);
				ImmediatePhysics::UpdateJointSettingsFromTwistConstraint(JointData.TwistConstraint, JointSettings);

				// The physics setting is backwards, because we can't enable collision on bodies that
				// are set to not collide for other reasons.
				JointSettings.bCollisionEnabled = !JointData.bDisableCollision;
				JointSettings.bProjectionEnabled = 
					JointData.LinearProjectionAmount > 0.0f || JointData.AngularProjectionAmount > 0.0f;
				JointSettings.AngularProjection = JointData.AngularProjectionAmount;
				JointSettings.LinearProjection = JointData.LinearProjectionAmount;
				JointSettings.ParentInvMassScale = JointData.ParentInverseMassScale;

				JointSettings.bUseLinearSolver = SolverSettings.bUseLinearJointSolver;

				JointRecord.JointHandle = Simulation->CreateJoint(
					ImmediatePhysics::FJointSetup(JointSettings, ChildActorHandle, ParentActorHandle));

				if (Chaos::FPBDJointConstraintHandle* Constraint = JointRecord.JointHandle->GetConstraint())
				{
					Chaos::FPBDJointSettings Settings = Constraint->GetSettings();
					if (!Settings.bCollisionEnabled)
					{
						IgnorePairs.Add(FRigPhysicsIgnorePair(
							JointRecord.ChildBodyComponentKey, JointRecord.ParentBodyComponentKey));
					}
				}
			}
		}
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::InstantiateControls(
	const FRigPhysicsSolverComponent* SolverComponent,
	FRigPhysicsIgnorePairs&           IgnorePairs)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_InstantiateControls);

	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();

	for (TPair<FRigComponentKey, FRigControlRecord>& ControlRecordPair : ControlRecords)
	{
		const FRigComponentKey& ComponentKey = ControlRecordPair.Key;
		FRigControlRecord& ControlRecord = ControlRecordPair.Value;

		if (const FRigPhysicsControlComponent* ControlComponent = Cast<FRigPhysicsControlComponent>(
			Hierarchy->FindComponent(ComponentKey)))
		{
			if (FRigBodyRecord* ChildBodyRecord = BodyRecords.Find(ControlRecord.ChildBodyComponentKey))
			{
				// this can be nullptr - just means a global control
				FRigBodyRecord* ParentBodyRecord = BodyRecords.Find(ControlRecord.ParentBodyComponentKey);

				ImmediatePhysics::FActorHandle* const ChildBodyHandle = ChildBodyRecord->ActorHandle;
				ImmediatePhysics::FActorHandle* const ParentBodyHandle = 
					ParentBodyRecord ? ParentBodyRecord->ActorHandle : SimulationActorHandle;

				// This handles nullptrs. The constraint is created disabled - it will be updated in pre-physics
				ControlRecord.JointHandle = CreatePhysicsJoint(Simulation.Get(), ChildBodyHandle, ParentBodyHandle);

				if (!ControlRecord.JointHandle)
				{
					UE_LOG(LogRigPhysics, Warning,
						TEXT("Unable to create control constraint for %s"), *ControlComponent->GetKey().ToString());
				}

				if (ControlComponent->ControlData.bDisableCollision)
				{
					IgnorePairs.Add(FRigPhysicsIgnorePair(
						ControlRecord.ChildBodyComponentKey, ControlRecord.ParentBodyComponentKey));
				}
			}
		}
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::UpdateBodyRecordPrePhysics(
	const FRigPhysicsSolverComponent* SolverComponent,
	const float                       DeltaTime,
	FRigBodyRecord&                   Record, 
	const FRigPhysicsBodyComponent*   PhysicsComponent)
{
	URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();
	if (!Record.ActorHandle || !Hierarchy)
	{
		return;
	}
	const FRigComponentKey ComponentKey = PhysicsComponent->GetKey();

	// Shuffle the record data
	Record.PrevSourceComponentSpaceVelocity = Record.SourceComponentSpaceVelocity;
	Record.PrevSourceComponentSpaceAngularVelocity = Record.SourceComponentSpaceAngularVelocity;
	Record.PrevSourceComponentSpaceTM = Record.SourceComponentSpaceTM;

	FRigElementKey SourceKey = PhysicsComponent->BodySolverSettings.SourceBone;
	if (!SourceKey.IsValid())
	{
		SourceKey = ComponentKey.ElementKey;
	}
	if (SourceKey.IsValid())
	{
		Record.SourceComponentSpaceTM = Hierarchy->GetGlobalTransform(SourceKey);
	}

	if (UpdateCounter == PreviousUpdateCounter + 1)
	{
		Record.SourceComponentSpaceVelocity = UE::PhysicsControl::CalculateLinearVelocity(
			Record.PrevSourceComponentSpaceTM.GetTranslation(), Record.SourceComponentSpaceTM.GetTranslation(), DeltaTime);
		Record.SourceComponentSpaceAngularVelocity = UE::PhysicsControl::CalculateAngularVelocity(
			Record.PrevSourceComponentSpaceTM.GetRotation(), Record.SourceComponentSpaceTM.GetRotation(), DeltaTime);
	}
	else
	{
		Record.PrevSourceComponentSpaceTM = Record.SourceComponentSpaceTM;
		Record.SourceComponentSpaceVelocity = FVector::ZeroVector;
		Record.SourceComponentSpaceAngularVelocity = FVector::ZeroVector;
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::UpdateBodyPrePhysics(
	const FRigVMExecuteContext&       ExecuteContext, 
	const FRigPhysicsSolverComponent* SolverComponent,
	const FRigBodyRecord&             Record, 
	const FRigPhysicsBodyComponent*   PhysicsComponent)
{
	const FRigComponentKey ComponentKey = PhysicsComponent->GetKey();

	FPhysicsControlModifierData BodyData = PhysicsComponent->BodyData;
	ERigPhysicsKinematicTargetSpace KinematicTargetSpace = PhysicsComponent->KinematicTargetSpace;

	if (SolverComponent->TrackInputCounter > 0)
	{
		BodyData.MovementType = EPhysicsMovementType::Kinematic;
		KinematicTargetSpace = ERigPhysicsKinematicTargetSpace::IgnoreTarget;
	}

	UpdateBodyFromModifierData(
		Record.ActorHandle, Simulation.Get(), BodyData, SimulationSpaceData.Gravity);

	if (Record.ActorHandle->GetIsKinematic())
	{
		// Get the target in component space, and then convert it into sim space if necessary.

		// If the target is already in component space, then that's all we need
		FTransform KinematicTargetCS;
		switch (PhysicsComponent->KinematicTargetSpace)
		{
		case ERigPhysicsKinematicTargetSpace::Component:
		{
			KinematicTargetCS = PhysicsComponent->KinematicTarget;
			break;
		}
		case ERigPhysicsKinematicTargetSpace::World:
		{
			// Record.KinematicTarget * SimulationSpaceState.ComponentTM.Inverse();
			KinematicTargetCS = PhysicsComponent->KinematicTarget.GetRelativeTransform(
				SimulationSpaceState.ComponentTM);
			break;
		}
		default:
		{
			// All the other options are relative to a bone, so the first task is to get
			// that, which will be in component space
			FRigElementKey SourceKey = PhysicsComponent->BodySolverSettings.SourceBone;
			if (!SourceKey.IsValid())
			{
				SourceKey = ComponentKey.ElementKey;
			}
			if (SourceKey.IsValid())
			{
				switch (PhysicsComponent->KinematicTargetSpace)
				{
				case ERigPhysicsKinematicTargetSpace::OffsetInBoneSpace:
				{
					KinematicTargetCS = PhysicsComponent->KinematicTarget * Record.SourceComponentSpaceTM.ToTransform();
					break;
				}
				case ERigPhysicsKinematicTargetSpace::OffsetInWorldSpace:
				{
					// Convert the bone to WS, apply the target, and convert back
					FTransform BoneWS = Record.SourceComponentSpaceTM.ToTransform() * SimulationSpaceState.ComponentTM;
					FTransform KinematicTargetWS = BoneWS * PhysicsComponent->KinematicTarget;
					// Danny TODO figure out which of the GetRelativeTransform versions this is
					KinematicTargetCS = SimulationSpaceState.ComponentTM.Inverse() * KinematicTargetWS;
					break;
				}
				case ERigPhysicsKinematicTargetSpace::OffsetInComponentSpace:
				{
					KinematicTargetCS = Record.SourceComponentSpaceTM.ToTransform() * PhysicsComponent->KinematicTarget;
					break;
				}
				case ERigPhysicsKinematicTargetSpace::IgnoreTarget:
				{
					KinematicTargetCS = Record.SourceComponentSpaceTM.ToTransform();
					break;
				}
				default:
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Kinematic target space is not valid"));
				}
				}
			}
		}
		}
		FTransform KinematicTargetTM = ConvertComponentSpaceTransformToSimSpace(
			SolverComponent->SolverSettings, KinematicTargetCS);
		Record.ActorHandle->SetKinematicTarget(KinematicTargetTM);
	}
	else
	{
		// Danny TODO move damping into BodyData - any PhysicsControl system should be able to use it
		Record.ActorHandle->SetLinearDamping(PhysicsComponent->Dynamics.LinearDamping);
		Record.ActorHandle->SetAngularDamping(PhysicsComponent->Dynamics.AngularDamping);
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::UpdateJointPrePhysics(
	FRigJointRecord&                    Record, 
	const FRigPhysicsJointComponent*    PhysicsJointComponent, 
	const URigHierarchy*                Hierarchy, 
	const float                         DeltaTime)
{
	// Now update the joint targets
	if (!Record.JointHandle)
	{
		return;
	}
	if (Chaos::FPBDJointConstraintHandle* Constraint = Record.JointHandle->GetConstraint())
	{
		const FRigComponentKey ComponentKey = PhysicsJointComponent->GetKey();

		// Set the drive strength etc
		const FRigPhysicsJointData& JointData = PhysicsJointComponent->JointData;
		const FRigPhysicsDriveData& DriveData = PhysicsJointComponent->DriveData;

		Chaos::FPBDJointSettings Settings = Constraint->GetSettings();
		ImmediatePhysics::UpdateJointSettingsFromLinearDriveConstraint(DriveData.LinearDriveConstraint, Settings);
		ImmediatePhysics::UpdateJointSettingsFromAngularDriveConstraint(DriveData.AngularDriveConstraint, Settings);
		Constraint->SetSettings(Settings);

		// Now set the actual target
		if (Settings.AngularDriveStiffness.SquaredLength() > 0.0f ||
			Settings.AngularDriveDamping.SquaredLength() > 0.0f ||
			Settings.LinearDriveStiffness.SquaredLength() > 0.0f ||
			Settings.LinearDriveDamping.SquaredLength() > 0.0f)
		{
			// Multiplier on the velocity calculated from the current and previous target
			float TargetVelocityMultiplier = 1.0f;

			FRigElementKey ChildSourceKey;
			if (const FRigPhysicsBodyComponent* ChildPhysicsComponent = Cast<FRigPhysicsBodyComponent>(
				Hierarchy->FindComponent(Record.ChildBodyComponentKey)))
			{
				ChildSourceKey = ChildPhysicsComponent->BodySolverSettings.SourceBone;
				if (!ChildSourceKey.IsValid())
				{
					ChildSourceKey = ComponentKey.ElementKey;
				}
				TargetVelocityMultiplier = DriveData.SkeletalAnimationVelocityMultiplier;
			}

			FRigElementKey ParentSourceKey;
			if (const FRigPhysicsBodyComponent* ParentPhysicsComponent = Cast<FRigPhysicsBodyComponent>(
				Hierarchy->FindComponent(Record.ParentBodyComponentKey)))
			{
				ParentSourceKey = ParentPhysicsComponent->BodySolverSettings.SourceBone;
				if (!ParentSourceKey.IsValid())
				{
					ParentSourceKey = Record.ParentBodyComponentKey.ElementKey;
				}
			}

			if (!ChildSourceKey.IsValid() || !ParentSourceKey.IsValid())
			{
				return;
			}

			// Danny TODO now all transforms are being cached in the record, get them from there
			// rather than the hierarchy

			// Note that the drive operates between a parent and child part, so we
			// don't need to worry about global/component (etc) space.
			const FTransform ChildTM = Hierarchy->GetGlobalTransform(ChildSourceKey);
			const FTransform ParentTM = Hierarchy->GetGlobalTransform(ParentSourceKey);

			const UE::PhysicsControl::FPosQuat ComponentSpaceParentFrameTM = 
				Settings.ConnectorTransforms[1] * ParentTM;
			const UE::PhysicsControl::FPosQuat ComponentSpaceChildFrameTM = 
				Settings.ConnectorTransforms[0] * ChildTM;

			const UE::PhysicsControl::FPosQuat DriveTargetTM =
				ComponentSpaceParentFrameTM.Inverse() * ComponentSpaceChildFrameTM;

			Constraint->SetLinearDrivePositionTarget(DriveTargetTM.GetTranslation());
			Constraint->SetAngularDrivePositionTarget(DriveTargetTM.GetRotation());

			if (Record.PreviousDriveTargetUpdateCounter + 1 == UpdateCounter &&
				TargetVelocityMultiplier > 0.0f)
			{
				if (DeltaTime > SMALL_NUMBER)
				{
					const UE::PhysicsControl::FPosQuat DriveTargetTMDelta =
						DriveTargetTM * Record.PreviousDriveTargetTM.Inverse();
					const FVector Velocity = DriveTargetTMDelta.GetTranslation() / DeltaTime;
					const FVector AngularVelocity =
						DriveTargetTMDelta.GetRotation().GetShortestArcWith(
							FQuat::Identity).ToRotationVector() / DeltaTime;
					if (!Velocity.ContainsNaN() && !AngularVelocity.ContainsNaN())
					{
						Constraint->SetLinearDriveVelocityTarget(Velocity * TargetVelocityMultiplier);
						Constraint->SetAngularDriveVelocityTarget(AngularVelocity * TargetVelocityMultiplier);
					}
				}
			}
			else
			{
				Constraint->SetLinearDriveVelocityTarget(Chaos::FVec3(0));
				Constraint->SetAngularDriveVelocityTarget(Chaos::FVec3(0));
			}
			Record.PreviousDriveTargetUpdateCounter = UpdateCounter;
			Record.PreviousDriveTargetTM = DriveTargetTM;
		}
	}
}

//======================================================================================================================
static UE::PhysicsControl::FPosQuat CalculateTargetTM(
	const URigHierarchy*               Hierarchy,
	const Chaos::FPBDJointSettings&    JointSettings,
	const FRigControlRecord&           Record)
{
	const FTransform ChildTM = Record.ChildBodyComponentKey.IsValid() ?
		Hierarchy->GetGlobalTransform(Record.ChildBodyComponentKey.ElementKey) :
		FTransform();

	const UE::PhysicsControl::FPosQuat ChildTargetTM =
		UE::PhysicsControl::FPosQuat(ChildTM) * 
		UE::PhysicsControl::FPosQuat(JointSettings.ConnectorTransforms[ConstraintChildIndex]);

	if (Record.ParentBodyComponentKey.IsValid())
	{
		const FTransform ParentTM = Hierarchy->GetGlobalTransform(Record.ParentBodyComponentKey.ElementKey);
		const UE::PhysicsControl::FPosQuat ParentTargetTM =
			UE::PhysicsControl::FPosQuat(ParentTM) *
			UE::PhysicsControl::FPosQuat(JointSettings.ConnectorTransforms[ConstraintParentIndex]);
		return ParentTargetTM.Inverse() * ChildTargetTM;
	}
	return ChildTargetTM;
}

//======================================================================================================================
void FRigPhysicsSimulation::CheckForResetsPrePhysics(
	FRigPhysicsSolverComponent* SolverComponent, const float DeltaTime)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_CheckForResetsPrePhysics);
	const FRigPhysicsSolverSettings& SolverSettings = SolverComponent->SolverSettings;

	double SpeedThresholdForResetSquared = SolverSettings.KinematicSpeedThresholdForReset > 0 ?
		FMath::Square(SolverSettings.KinematicSpeedThresholdForReset) : TNumericLimits<double>::Max();

	double AccelerationThresholdForResetSquared = SolverSettings.KinematicAccelerationThresholdForReset > 0 ?
		FMath::Square(SolverSettings.KinematicAccelerationThresholdForReset) : TNumericLimits<double>::Max();

	double HighestSpeedSq = -1.0;
	double HighestAccelerationSq = -1.0;

	for (TPair<FRigComponentKey, FRigBodyRecord>& BodyRecordPair : BodyRecords)
	{
		const FRigComponentKey& ComponentKey = BodyRecordPair.Key;
		FRigBodyRecord& Record = BodyRecordPair.Value;

		if (Record.ActorHandle && Record.ActorHandle->GetIsKinematic())
		{
			const FVector Velocity = Record.SourceComponentSpaceVelocity;
			const FVector Acceleration =
				DeltaTime > SMALL_NUMBER && (UpdateCounter == PreviousUpdateCounter + 1)
				? (Record.SourceComponentSpaceVelocity - Record.PrevSourceComponentSpaceVelocity) / DeltaTime
				: FVector::ZeroVector;

			HighestSpeedSq = FMath::Max(HighestSpeedSq, Velocity.SquaredLength());
			HighestAccelerationSq = FMath::Max(HighestAccelerationSq, Acceleration.SquaredLength());
		}
	}

	if (HighestSpeedSq > SpeedThresholdForResetSquared || HighestAccelerationSq > AccelerationThresholdForResetSquared)
	{
		if (HighestSpeedSq > SpeedThresholdForResetSquared)
		{
			UE_LOG(LogRigPhysics, Log, TEXT("Speed %f triggered reset in %s"),
				FMath::Sqrt(HighestSpeedSq), *OwningControlRig->GetName());
		}
		if (HighestAccelerationSq > AccelerationThresholdForResetSquared)
		{
			UE_LOG(LogRigPhysics, Log, TEXT("Acceleration %f triggered reset in %s"),
				FMath::Sqrt(HighestAccelerationSq), *OwningControlRig->GetName());
		}
		SolverComponent->TrackInputCounter = FMath::Max(SolverComponent->TrackInputCounter, 3);
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::UpdatePrePhysics(
	const FRigVMExecuteContext& ExecuteContext, 
	FRigPhysicsSolverComponent* SolverComponent, 
	const float                 DeltaTime)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_UpdatePrePhysics);

	URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();

	if (CollisionActorHandle)
	{
		FTransform BodyRelSimSpaceTM = ConvertCollisionSpaceTransformToSimSpace(
			SolverComponent->SolverSettings, FTransform());
		CollisionActorHandle->SetKinematicTarget(BodyRelSimSpaceTM);
	}

	for (TPair<FRigComponentKey, FRigBodyRecord>& BodyRecordPair : BodyRecords)
	{
		const FRigComponentKey& ComponentKey = BodyRecordPair.Key;
		FRigBodyRecord& Record = BodyRecordPair.Value;
		if (const FRigPhysicsBodyComponent* PhysicsComponent = Cast<FRigPhysicsBodyComponent>(
			Hierarchy->FindComponent(ComponentKey)))
		{
			UpdateBodyRecordPrePhysics(SolverComponent, DeltaTime, Record, PhysicsComponent);
		}
	}

	CheckForResetsPrePhysics(SolverComponent, DeltaTime);
	if (SolverComponent->TrackInputCounter > 0)
	{
		UE_LOG(LogRigPhysics, Log, TEXT("Forcing tracking (counter = %d) of input for %s"), 
			SolverComponent->TrackInputCounter, *OwningControlRig->GetName());
	}

	{
		DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_UpdateBodiesPrePhysics);
		for (TPair<FRigComponentKey, FRigBodyRecord>& BodyRecordPair : BodyRecords)
		{
			const FRigComponentKey& ComponentKey = BodyRecordPair.Key;
			FRigBodyRecord& Record = BodyRecordPair.Value;
			if (const FRigPhysicsBodyComponent* PhysicsComponent = Cast<FRigPhysicsBodyComponent>(
				Hierarchy->FindComponent(ComponentKey)))
			{
				UpdateBodyPrePhysics(ExecuteContext, SolverComponent, Record, PhysicsComponent);
			}
		}
	}

	{
		DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_UpdateJointsPrePhysics);
		for (TPair<FRigComponentKey, FRigJointRecord>& JointRecordPair : JointRecords)
		{
			const FRigComponentKey& ComponentKey = JointRecordPair.Key;
			FRigJointRecord& JointRecord = JointRecordPair.Value;

			if (const FRigPhysicsJointComponent* JointComponent = Cast<FRigPhysicsJointComponent>(
				Hierarchy->FindComponent(ComponentKey)))
			{
				UpdateJointPrePhysics(JointRecord, JointComponent, Hierarchy, DeltaTime);
			}
		}
	}

	{
		DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_UpdateControlsPrePhysics);
		for (TPair<FRigComponentKey, FRigControlRecord>& ControlRecordPair : ControlRecords)
		{
			const FRigComponentKey& ComponentKey = ControlRecordPair.Key;
			FRigControlRecord& ControlRecord = ControlRecordPair.Value;

			if (const FRigPhysicsControlComponent* ControlComponent = Cast<FRigPhysicsControlComponent>(
				Hierarchy->FindComponent(ComponentKey)))
			{
				UpdateControlPrePhysics(ControlRecord, ControlComponent, SolverComponent, Hierarchy, DeltaTime);
			}
		}
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::UpdateControlPrePhysics(
	FRigControlRecord&                 ControlRecord,
	const FRigPhysicsControlComponent* ControlComponent,
	const FRigPhysicsSolverComponent*  SolverComponent,
	const URigHierarchy*               Hierarchy,
	const float                        DeltaTime)
{
	const FRigPhysicsSolverSettings& SolverSettings = SolverComponent->SolverSettings;

	ImmediatePhysics::FJointHandle* JointHandle = ControlRecord.JointHandle;
	if (!JointHandle)
	{
		return;
	}
	Chaos::FPBDJointConstraintHandle* Constraint = JointHandle->GetConstraint();
	if (!Constraint)
	{
		return;
	}
	float ThisDeltaTime = DeltaTime;

	if (ControlRecord.PreviousTargetUpdateCounter + 1 != UpdateCounter)
	{
		// If we missed some intermediate updates, then we don't want to use the previous
		// positions etc to calculate velocities. This will mean velocity/damping will be
		// incorrect for one frame, but that's probably OK.
		ThisDeltaTime = 0.0f;
	}

	Constraint->SetCollisionEnabled(!ControlComponent->ControlData.bDisableCollision);
	Constraint->SetParentInvMassScale(ControlComponent->ControlData.bOnlyControlChildObject ? 0 : 1);

	const Chaos::FPBDJointSettings& JointSettings = Constraint->GetSettings();
	if (UpdateDriveSpringDamperSettings(
		JointHandle, JointSettings, ControlComponent->ControlData, ControlComponent->ControlMultiplier))
	{
		const ImmediatePhysics::FActorHandle* ChildActorHandle =
			JointHandle->GetActorHandles()[ConstraintChildIndex];
		const ImmediatePhysics::FActorHandle* ParentActorHandle =
			JointHandle->GetActorHandles()[ConstraintParentIndex];

		if (ChildActorHandle && ParentActorHandle)
		{
			// TODO
			// - cache settings / previous input parameters to avoid unnecessary repeating
			//   calculations and making physics API calls every update.

			// Update the target point on the child
			Constraint->SetChildConnectorLocation(
				ControlComponent->ControlData.GetControlPoint(ChildActorHandle));

			FTransform TargetTM(
				ControlComponent->ControlTarget.TargetOrientation,
				ControlComponent->ControlTarget.TargetPosition);

			if (ControlComponent->ControlData.bUseSkeletalAnimation)
			{
				const FTransform ComponentSpaceAnimTargetTM = CalculateTargetTM(
					Hierarchy, JointSettings, ControlRecord).ToTransform();
				const FTransform SimSpaceAnimTargetTM = ConvertComponentSpaceTransformToSimSpace(
					SolverSettings, ComponentSpaceAnimTargetTM);
				TargetTM = TargetTM * SimSpaceAnimTargetTM;
			}

			Constraint->SetLinearDrivePositionTarget(TargetTM.GetTranslation());
			Constraint->SetAngularDrivePositionTarget(TargetTM.GetRotation());

			if ((ThisDeltaTime * ControlComponent->ControlData.LinearTargetVelocityMultiplier) != 0)
			{
				FVector Velocity =
					(TargetTM.GetTranslation() - ControlRecord.PreviousTargetTM.GetTranslation()) / ThisDeltaTime;
				Constraint->SetLinearDriveVelocityTarget(
					Velocity * ControlComponent->ControlData.LinearTargetVelocityMultiplier);
			}
			else
			{
				Constraint->SetLinearDriveVelocityTarget(Chaos::FVec3(0));
			}

			if ((ThisDeltaTime * ControlComponent->ControlData.AngularTargetVelocityMultiplier) != 0)
			{
				// Note that quats multiply in the opposite order to TMs, and must be in the same hemisphere.
				const FQuat Q = TargetTM.GetRotation();
				FQuat PrevQ = ControlRecord.PreviousTargetTM.GetRotation();
				PrevQ.EnforceShortestArcWith(Q);
				const FQuat DeltaQ = Q * PrevQ.Inverse();
				const FVector AngularVelocity = DeltaQ.ToRotationVector() / ThisDeltaTime;

				Constraint->SetAngularDriveVelocityTarget(
					AngularVelocity * ControlComponent->ControlData.AngularTargetVelocityMultiplier);
			}
			else
			{
				Constraint->SetAngularDriveVelocityTarget(Chaos::FVec3(0));
			}


			ControlRecord.PreviousTargetTM = TargetTM;
			ControlRecord.PreviousTargetUpdateCounter = UpdateCounter;
		}
		else
		{
			// Note that if we don't have any strength, then we don't calculate the targets.
			// However, make sure that we don't apply velocities using the wrong calculation
			// when the strength/damping is increased in the future
		}
	}
}

//======================================================================================================================
// Note that we read back into a target bone, which may have been specified explicitly, or will
// otherwise default to the physics element parent.
void FRigPhysicsSimulation::UpdatePostPhysics(
	FRigPhysicsSolverComponent* SolverComponent, const float Alpha, const float DeltaTime)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_UpdatePostPhysics);

	if (Alpha == 0.0f)
	{
		return;
	}
	const FRigPhysicsSolverSettings& SolverSettings = SolverComponent->SolverSettings;
	bool bGotInvalidSimulationData = false;
	URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();

	double PositionThresholdForResetSquared = SolverSettings.PositionThresholdForReset > 0 ?
		FMath::Square(SolverSettings.PositionThresholdForReset) : TNumericLimits<double>::Max();
	double HighestPosition = -1.0;

	// Traverse using the sorted keys
	for (const FRigComponentKey& ComponentKey : SortedBodyComponentKeys)
	{
		FRigBodyRecord& Record = *BodyRecords.Find(ComponentKey);
		if (Record.ActorHandle && Record.TargetElementKey.IsValid())
		{
			// Check the simulation output
			FTransform SimSpaceTM = Record.ActorHandle->GetWorldTransform();
			double DistSq = SimSpaceTM.GetTranslation().SquaredLength();
			if (!SimSpaceTM.IsValid() || DistSq > PositionThresholdForResetSquared)
			{
				HighestPosition = FMath::Max(HighestPosition, SimSpaceTM.GetTranslation().Length());
				bGotInvalidSimulationData = true;
			}

			// Calculate the target TM even if we're going to reset - it's likely useful for
			// debugging (and this should be rare!)
			Record.TargetComponentSpaceTM = ConvertSimSpaceTransformToComponentSpace(
				SolverSettings, SimSpaceTM);
			if (Alpha < 0.999f)
			{
				// Danny TODO Note that this uses Alpha to blend in component space.
				// This can cause joint separation. We probably want an option to blend
				// in local (joint) space, perhaps splitting the alpha into orientation
				// and position.
				FTransform CurrentTM = Record.SourceComponentSpaceTM.ToTransform();
				FQuat TargetQ = FQuat::Slerp(
					CurrentTM.GetRotation(), Record.TargetComponentSpaceTM.GetRotation(), Alpha);
				FVector TargetT = FMath::Lerp(
					CurrentTM.GetTranslation(), Record.TargetComponentSpaceTM.GetTranslation(), Alpha);
				Record.TargetComponentSpaceTM.SetRotation(TargetQ);
				Record.TargetComponentSpaceTM.SetTranslation(TargetT);
			}
		}
	}

	if (bGotInvalidSimulationData)
	{
		if (HighestPosition > 0)
		{
			UE_LOG(LogRigPhysics, Log, TEXT("Position %f triggered teleport in %s - resetting pose"),
				HighestPosition, *OwningControlRig->GetName());
		}
		// Avoid cached transforms being used in controls by bumping the update counter. 
		UpdateCounter += 1;
		// Set this to 3 since it gets decremented at the end of the update, and we need it to take
		// effect at the start of the next update.
		SolverComponent->TrackInputCounter = FMath::Max(SolverComponent->TrackInputCounter, 3);
	}

	// If we found something invalid then we force the simulation to be as good as we can make it,
	// and we don't write back to the hierarchy.
	if (HighestPosition > 0)
	{
		UE_LOG(LogRigPhysics, Log, TEXT("Resetting state to input pose in %s"), 
			*OwningControlRig->GetName());
		for (TPair<FRigComponentKey, FRigBodyRecord>& BodyRecordPair : BodyRecords)
		{
			const FRigComponentKey& ComponentKey = BodyRecordPair.Key;
			const FRigBodyRecord& Record = BodyRecordPair.Value;

			if (const FRigPhysicsBodyComponent* PhysicsComponent = Cast<FRigPhysicsBodyComponent>(
				Hierarchy->FindComponent(ComponentKey)))
			{
				if (Record.ActorHandle && !Record.ActorHandle->GetIsKinematic())
				{
					// Get the TM in component space, and then convert it into sim space.
					FRigElementKey SourceKey = PhysicsComponent->BodySolverSettings.SourceBone;
					if (!SourceKey.IsValid())
					{
						SourceKey = ComponentKey.ElementKey;
					}
					if (SourceKey.IsValid())
					{
						FTransform SourceComponentSpaceTM = Record.SourceComponentSpaceTM.ToTransform();
						const FTransform SourceSimulationSpaceTM =
							ConvertComponentSpaceTransformToSimSpace(SolverSettings, SourceComponentSpaceTM);
						Record.ActorHandle->SetWorldTransform(SourceSimulationSpaceTM);
					}
					Record.ActorHandle->SetLinearVelocity(FVector::ZeroVector);
					Record.ActorHandle->SetAngularVelocity(FVector::ZeroVector);
				}
			}
		}
	}
	else
	{
		// All is good - write the transforms we cached
		for (const FRigComponentKey& ComponentKey : SortedBodyComponentKeys)
		{
			const FRigBodyRecord& Record = *BodyRecords.Find(ComponentKey);
			if (Record.ActorHandle && Record.TargetElementKey.IsValid())
			{
				// Note that we set bAffectChildren = true (i.e. don't counter-animate
				// children), so that attached animation bones will follow physics, but we
				// rely on our bodies being sorted so we work out from the leaf nodes so as
				// not to disturb previously set bodies.
				Hierarchy->SetGlobalTransform(Record.TargetElementKey, Record.TargetComponentSpaceTM, false, true);
			}
		}
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::StepSimulation(
	const FRigVMExecuteContext& ExecuteContext,
	FRigPhysicsSolverComponent* SolverComponent,
	const float                 DeltaTimeOverride,
	const float                 SimulationSpaceDeltaTimeOverride,
	const float                 Alpha)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_StepSimulation);

	// Increment the update counter at the start - and always update it so this tells us our "frame number".
	++UpdateCounter;

	float PhysicsDeltaTime = ExecuteContext.GetDeltaTime();
	if (DeltaTimeOverride > 0.0f)
	{
		PhysicsDeltaTime = DeltaTimeOverride;
	}
	else if (DeltaTimeOverride < 0.0f)
	{
		PhysicsDeltaTime = 0.0f;
	}

	float PhysicsSimulationSpaceDeltaTime = PhysicsDeltaTime;
	if (SimulationSpaceDeltaTimeOverride > 0.0f)
	{
		PhysicsSimulationSpaceDeltaTime = SimulationSpaceDeltaTimeOverride;
	}

	// We need to know about the simulation space etc before we can instantiate anything into the right place
	SimulationSpaceData = UpdateSimulationSpaceStateAndCalculateData(SolverComponent, PhysicsSimulationSpaceDeltaTime);

	// We instantiate when we do the first simulation - this makes sure any changes applied by
	// the user have been made. It also means there is no overhead if physics is never stepped.
	// However, there may be a hitch due to the creation, so it may also happen during construction.
	Instantiate(SolverComponent); // There is an early out if it's already been done

	if (!Simulation)
	{
		return;
	}

	const FRigPhysicsSolverSettings& SolverSettings = SolverComponent->SolverSettings;
	const FRigPhysicsSimulationSpaceSettings& SimulationSpaceSettings = SolverComponent->SimulationSpaceSettings;

	float FixedTimeStep = CVarControlRigPhysicsFixedTimeStepOverride.GetValueOnAnyThread() < 0 
		? SolverSettings.FixedTimeStep : CVarControlRigPhysicsFixedTimeStepOverride.GetValueOnAnyThread();
	int MaxTimeSteps = CVarControlRigPhysicsMaxTimeStepsOverride.GetValueOnAnyThread() < 0
		? SolverSettings.MaxTimeSteps : CVarControlRigPhysicsMaxTimeStepsOverride.GetValueOnAnyThread();

	// Set settings that might change
	Simulation->SetSolverSettings(
		FixedTimeStep,
		SolverSettings.CollisionBoundsExpansion,
		SolverSettings.MaxDepenetrationVelocity,
		SolverSettings.bUseLinearJointSolver,
		SolverSettings.PositionIterations,
		SolverSettings.VelocityIterations,
		SolverSettings.ProjectionIterations,
		SolverSettings.bUseManifolds);

	Chaos::FCollisionDetectorSettings CollisionDetectorSettings = Simulation->GetCollisionDetectorSettings();
	CollisionDetectorSettings.BoundsVelocityInflation = SolverSettings.BoundsVelocityMultiplier;
	CollisionDetectorSettings.MaxVelocityBoundsExpansion = SolverSettings.MaxVelocityBoundsExpansion;
	Simulation->SetCollisionDetectorSettings(CollisionDetectorSettings);

	// This gets reset to 100 after every simulation step!
	Simulation->SetMaxNumRollingAverageStepTimes(SolverSettings.MaxNumRollingAverageStepTimes);

	// Other settings - would normally be static (so Danny TODO move this)
	ChaosJointSolverSettings.bSolvePositionLast = SolverSettings.bSolveJointPositionsLast;
	ChaosJointSolverSettings.bSortEnabled = true;

	// Simulation space
	Chaos::FSimulationSpaceSettings ChaosSimulationSpaceSettings = Simulation->GetSimulationSpaceSettings();
	ChaosSimulationSpaceSettings.bEnabled = (SimulationSpaceSettings.SpaceMovementAmount > 0.0f);
	ChaosSimulationSpaceSettings.ExternalLinearEtherDrag = SimulationSpaceSettings.ExternalLinearDrag;
	ChaosSimulationSpaceSettings.LinearVelocityAlpha = SimulationSpaceSettings.LinearDragMultiplier;
	ChaosSimulationSpaceSettings.AngularVelocityAlpha = SimulationSpaceSettings.AngularDragMultiplier;
	Simulation->SetSimulationSpaceSettings(ChaosSimulationSpaceSettings);
	Simulation->UpdateSimulationSpace(
		SimulationSpaceState.SimulationSpaceTM,
		SimulationSpaceSettings.SpaceMovementAmount * SimulationSpaceData.LinearVelocity,
		SimulationSpaceSettings.SpaceMovementAmount * SimulationSpaceData.AngularVelocity,
		SimulationSpaceSettings.SpaceMovementAmount * SimulationSpaceData.LinearAcceleration,
		SimulationSpaceSettings.SpaceMovementAmount * SimulationSpaceData.AngularAcceleration);

	// Only update if there is a delta time:
	// * We don't want to update our previous TMs and store the dt - because that would end up
	//   implying infinite velocities
	// * We don't to update kinematic bodies with the new TMs because, since the simulated ones
	//   won't move, that would break the pose.
	// * We can't actually simulate with dt = 0
	if (PhysicsDeltaTime > 0.0f)
	{
		UpdatePrePhysics(ExecuteContext, SolverComponent, PhysicsDeltaTime);

		Simulation->Simulate(
			PhysicsDeltaTime, SolverSettings.MaxDeltaTime, MaxTimeSteps,
			SimulationSpaceData.Gravity, &ChaosJointSolverSettings);

		PreviousUpdateCounter = UpdateCounter;
	}

	// Always do a read-back, even for zero Dt
	UpdatePostPhysics(SolverComponent, Alpha, PhysicsDeltaTime);

	if (SolverComponent->TrackInputCounter > 0)
	{
		--SolverComponent->TrackInputCounter;
	}
}

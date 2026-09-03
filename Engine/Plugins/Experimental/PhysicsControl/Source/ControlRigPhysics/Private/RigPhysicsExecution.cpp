// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsExecution.h"
#include "RigPhysicsSimulation.h"
#include "RigPhysicsBodyComponent.h"
#include "RigPhysicsJointComponent.h"
#include "RigPhysicsSolverComponent.h"
#include "RigPhysicsControlComponent.h"

#include "Rigs/RigHierarchyController.h"
#include "Units/RigUnitContext.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "ControlRigPhysicsModule.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "ControlRig.h"
#if WITH_EDITOR
#include "RigVMModel/RigVMController.h"
#include "ControlRigBlueprint.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigPhysicsExecution)

TAutoConsoleVariable<int> CVarControlRigPhysicsVisualizationOverride(
	TEXT("ControlRig.Physics.VisualizationOveride"), -1,
	TEXT("-1 uses the Step Physics Solver setting, 0 forces visualization to be disabled, 1 forces it to be enabled."));

//======================================================================================================================
FRigUnit_AddPhysicsSolver_Execute()
{
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}

	if (ExecuteContext.GetEventName() != FRigUnit_PrepareForExecution::EventName)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("AddPhysicsSolver can only be used during Setup"));
	}

	if (URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController())
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(
			Controller, ExecuteContext.GetInstructionIndex());
		PhysicsSolverComponentKey = Controller->AddComponent(
			FRigPhysicsSolverComponent::StaticStruct(), TEXT("PhysicsSolver"), Owner);
		if (PhysicsSolverComponentKey.IsValid())
		{
			if (FRigPhysicsSolverComponent* Component = Cast<FRigPhysicsSolverComponent>(
				ExecuteContext.Hierarchy->FindComponent(PhysicsSolverComponentKey)))
			{
				Component->SolverSettings = SolverSettings;
				Component->SimulationSpaceSettings = SimulationSpaceSettings;
			}
		}
	}
}

//======================================================================================================================
FRigUnit_TrackInputPose_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (UControlRig* ControlRig = Cast<UControlRig>(ExecuteContext.Hierarchy->GetOuter()))
	{
		if (FRigPhysicsSolverComponent* SolverComponent = Cast<FRigPhysicsSolverComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsSolverComponentKey)))
		{
			if (bForceNumberOfFrames)
			{
				SolverComponent->TrackInputCounter = NumberOfFrames;
			}
			else
			{
				SolverComponent->TrackInputCounter = FMath::Max(SolverComponent->TrackInputCounter, NumberOfFrames);
			}
		}
	}
}

//======================================================================================================================
FRigUnit_StepPhysicsSolver_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (UControlRig* ControlRig = Cast<UControlRig>(ExecuteContext.Hierarchy->GetOuter()))
	{
		if (FRigPhysicsSolverComponent* SolverComponent = Cast<FRigPhysicsSolverComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsSolverComponentKey)))
		{
			FRigPhysicsSimulation* PhysicsSimulation = Cast<FRigPhysicsSimulation>(
				ControlRig->GetPhysicsSimulation(PhysicsSolverComponentKey));

			// Make the solver and instantiate if necessary
			if (!PhysicsSimulation)
			{
				TSharedPtr<FRigPhysicsSimulation> NewPhysicsSimulation = MakeShared<FRigPhysicsSimulation>(ControlRig);

				if (ControlRig->RegisterPhysicsSimulation(NewPhysicsSimulation, PhysicsSolverComponentKey))
				{
					NewPhysicsSimulation->Instantiate(SolverComponent);
				}
				PhysicsSimulation = NewPhysicsSimulation.Get();
			}

			if (PhysicsSimulation)
			{
				PhysicsSimulation->StepSimulation(
					ExecuteContext, SolverComponent, DeltaTimeOverride, SimulationSpaceDeltaTimeOverride, Alpha);
				int32 VisualizationOverride = CVarControlRigPhysicsVisualizationOverride.GetValueOnAnyThread();
				if (VisualizationOverride == 1 || (VisualizationOverride < 0 && VisualizationSettings.bEnableVisualization))
				{
					PhysicsSimulation->Draw(
						ExecuteContext.GetDrawInterface(), SolverComponent->SolverSettings,
						VisualizationSettings, ExecuteContext.GetWorld());
				}
			}
		}
	}
}

//======================================================================================================================
FRigUnit_InstantiatePhysics_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (UControlRig* ControlRig = Cast<UControlRig>(ExecuteContext.Hierarchy->GetOuter()))
	{
		if (FRigPhysicsSolverComponent* SolverComponent = Cast<FRigPhysicsSolverComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsSolverComponentKey)))
		{
			TSharedPtr<FRigPhysicsSimulation> PhysicsSimulation = MakeShared<FRigPhysicsSimulation>(ControlRig);

			if (ControlRig->RegisterPhysicsSimulation(PhysicsSimulation, PhysicsSolverComponentKey))
			{
				PhysicsSimulation->Instantiate(SolverComponent);
			}
		}
	}
}

//======================================================================================================================
FRigUnit_AddPhysicsComponents_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (ExecuteContext.GetEventName() != FRigUnit_PrepareForExecution::EventName)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("AddPhysicsComponents can only be used during Setup"));
	}

	if (URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController())
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(
			Controller, ExecuteContext.GetInstructionIndex());
		// Add the body
		PhysicsBodyComponentKey = Controller->AddComponent(
			FRigPhysicsBodyComponent::StaticStruct(), FRigPhysicsBodyComponent::GetDefaultName(), Owner);
		if (PhysicsBodyComponentKey.IsValid())
		{
			if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
				ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
			{
				Component->BodySolverSettings = Solver;
				Component->Dynamics = Dynamics;
				Component->BodyData = BodyData;
				Component->Collision = Collision;
				if (Collision.IsEmpty())
				{
					Component->AutoCalculateCollision(ExecuteContext.Hierarchy);
				}
			}

			if (bAddJoint)
			{
				PhysicsJointComponentKey = Controller->AddComponent(
					FRigPhysicsJointComponent::StaticStruct(), FRigPhysicsJointComponent::GetDefaultName(), Owner);
				if (PhysicsJointComponentKey.IsValid())
				{
					if (FRigPhysicsJointComponent* Component = Cast<FRigPhysicsJointComponent>(
						ExecuteContext.Hierarchy->FindComponent(PhysicsJointComponentKey)))
					{
						// Leave parent as blank to automatically find it
						Component->ChildBodyComponentKey = PhysicsBodyComponentKey;
						Component->JointData = JointData;
						Component->DriveData = DriveData;
					}
				}
			}

			if (bAddSimSpaceControl)
			{
				SimSpaceControlComponentKey = Controller->AddComponent(FRigPhysicsControlComponent::StaticStruct(),
					TEXT("SimSpaceControl"), Owner);
				if (SimSpaceControlComponentKey.IsValid())
				{
					if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
						ExecuteContext.Hierarchy->FindComponent(SimSpaceControlComponentKey)))
					{
						Component->bUseParentBodyAsDefault = false;
						Component->ChildBodyComponentKey = PhysicsBodyComponentKey;
						Component->ControlData = SimSpaceControlData;
					}
				}
			}

			if (bAddParentSpaceControl)
			{
				ParentSpaceControlComponentKey = Controller->AddComponent(FRigPhysicsControlComponent::StaticStruct(),
					TEXT("ParentSpaceControl"), Owner);
				if (ParentSpaceControlComponentKey.IsValid())
				{
					if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
						ExecuteContext.Hierarchy->FindComponent(ParentSpaceControlComponentKey)))
					{
						Component->bUseParentBodyAsDefault = true;
						Component->ChildBodyComponentKey = PhysicsBodyComponentKey;
						Component->ControlData = ParentSpaceControlData;
					}
				}
			}
		}
	}
}

//======================================================================================================================
static void SetCommonProperties(FRigPhysicsCollisionShape& Shape, const FKShapeElem& ShapeElem)
{
	Shape.RestOffset = ShapeElem.RestOffset;
	Shape.Name = ShapeElem.GetName();
	Shape.bContributeToMass = ShapeElem.GetContributeToMass();
}

//======================================================================================================================
static void SetUpJointData(
	FRigPhysicsJointData&               JointData, 
	const FConstraintInstance&          ConstraintInstance, 
	const FConstraintProfileProperties& ConstraintProfileProperties)
{
	JointData.bAutoCalculateChildOffset = false;
	JointData.bAutoCalculateParentOffset = false;

	JointData.ExtraChildOffset = ConstraintInstance.GetRefFrame(EConstraintFrame::Frame1);
	JointData.ExtraParentOffset = ConstraintInstance.GetRefFrame(EConstraintFrame::Frame2);

	JointData.ExtraParentOffset.SetRotation(JointData.ExtraParentOffset.GetRotation() *
		ConstraintInstance.AngularRotationOffset.Quaternion());

	JointData.LinearConstraint = ConstraintProfileProperties.LinearLimit;
	JointData.ConeConstraint = ConstraintProfileProperties.ConeLimit;
	JointData.TwistConstraint = ConstraintProfileProperties.TwistLimit;
}

//======================================================================================================================
static void SetUpDriveData(
	FRigPhysicsDriveData&               DriveData, 
	const FConstraintProfileProperties& ConstraintProfileProperties)
{
	DriveData.LinearDriveConstraint = ConstraintProfileProperties.LinearDrive;
	DriveData.AngularDriveConstraint = ConstraintProfileProperties.AngularDrive;
}

//======================================================================================================================
FRigUnit_HierarchyInstantiateFromPhysicsAsset_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (ExecuteContext.GetEventName() != FRigUnit_PrepareForExecution::EventName)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("InstantiateFromPhysicsAsset can only be used during Setup"));
		return;
	}
	if (!PhysicsAsset)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("InstantiateFromPhysicsAsset needs a valid physics asset"));
		return;
	}

	URigHierarchyController * Controller = ExecuteContext.Hierarchy->GetController();
	if (!Controller)
	{
		return;
	}

	FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, ExecuteContext.GetInstructionIndex());

	// When we create physics joints we need to be able to refer back to the bodies that have been created
	TMap<FName, FRigComponentKey> BoneToBodyComponentKeyMap;

	for (TObjectPtr<USkeletalBodySetup> SkeletalBodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		if (!SkeletalBodySetup)
		{
			continue;
		}

		// Check we can use this based on the bone name
		FName BoneName = SkeletalBodySetup->BoneName;
		FRigElementKey OwnerElementKey(BoneName, ERigElementType::Bone);
		if (!ExecuteContext.Hierarchy->Find(OwnerElementKey))
		{
			continue;
		}
		if (!BonesToUse.IsEmpty() && BonesToUse.Find(OwnerElementKey) == INDEX_NONE)
		{
			continue;
		}

		// Collision
		FRigPhysicsCollision Collision;
		const FKAggregateGeom& AggGeom = SkeletalBodySetup->AggGeom;
		for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
		{
			FTransform TM(BoxElem.Rotation, BoxElem.Center);
			FRigPhysicsCollisionBox Box(TM, FVector(BoxElem.X, BoxElem.Y, BoxElem.Z));
			SetCommonProperties(Box, BoxElem);
			Collision.Boxes.Add(Box);
		}
		for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
		{
			FTransform TM(SphereElem.Center);
			FRigPhysicsCollisionSphere Sphere(TM, SphereElem.Radius);
			SetCommonProperties(Sphere, SphereElem);
			Collision.Spheres.Add(Sphere);
		}
		for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
		{
			FTransform TM(SphylElem.Rotation, SphylElem.Center);
			FRigPhysicsCollisionCapsule Capsule(TM, SphylElem.Radius, SphylElem.Length);
			SetCommonProperties(Capsule, SphylElem);
			Collision.Capsules.Add(Capsule);
		}

		const UPhysicalMaterial* Material = SkeletalBodySetup->GetPhysMaterial();
		Material = Material ? Material : GetDefault<UPhysicalMaterial>();
		if (Material)
		{
			Collision.Material.Friction = FMath::Max(Material->Friction, Material->StaticFriction);
			Collision.Material.Restitution = Material->Restitution;
			Collision.Material.FrictionCombineMode = (ERigPhysicsCombineMode) 
				Material->FrictionCombineMode.GetValue();
			Collision.Material.RestitutionCombineMode = (ERigPhysicsCombineMode) 
				Material->RestitutionCombineMode.GetValue();
		}

		// Dynamics
		FRigPhysicsDynamics Dynamics;
		Dynamics.MassOverride = SkeletalBodySetup->CalculateMass();
		Dynamics.LinearDamping = SkeletalBodySetup->DefaultInstance.LinearDamping;
		Dynamics.AngularDamping = SkeletalBodySetup->DefaultInstance.AngularDamping;

		// Body Data
		FPhysicsControlModifierData BodyData;
		BodyData.MovementType =
			SkeletalBodySetup->PhysicsType == EPhysicsType::PhysType_Simulated ?
			EPhysicsMovementType::Simulated : EPhysicsMovementType::Kinematic;
		BodyData.CollisionType =
			SkeletalBodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Enabled ?
			ECollisionEnabled::PhysicsOnly : ECollisionEnabled::NoCollision;

		// Add the component
		FRigComponentKey PhysicsBodyComponentKey = Controller->AddComponent(
			FRigPhysicsBodyComponent::StaticStruct(), TEXT("PhysicsBody"), OwnerElementKey);
		if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
		{
			Component->BodySolverSettings = Solver;
			Component->Dynamics = Dynamics;
			Component->BodyData = BodyData;
			Component->Collision = Collision;

			BoneToBodyComponentKeyMap.Add(BoneName, PhysicsBodyComponentKey);
			PhysicsBodyComponentKeys.Add(PhysicsBodyComponentKey);
		}

		if (bAddSimSpaceControl)
		{
			FRigComponentKey SimSpaceControlComponentKey = Controller->AddComponent(
				FRigPhysicsControlComponent::StaticStruct(),
				TEXT("SimSpaceControl"), PhysicsBodyComponentKey.ElementKey);
			if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
				ExecuteContext.Hierarchy->FindComponent(SimSpaceControlComponentKey)))
			{
				Component->bUseParentBodyAsDefault = false;
				Component->ChildBodyComponentKey = PhysicsBodyComponentKey;
				Component->ControlData = SimSpaceControlData;

				SimSpaceControlComponentKeys.Add(SimSpaceControlComponentKey);
			}
		}

		if (bAddParentSpaceControl)
		{
			FRigComponentKey ParentSpaceControlComponentKey = Controller->AddComponent(
				FRigPhysicsControlComponent::StaticStruct(),
				TEXT("ParentSpaceControl"), PhysicsBodyComponentKey.ElementKey);
			if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
				ExecuteContext.Hierarchy->FindComponent(ParentSpaceControlComponentKey)))
			{
				Component->bUseParentBodyAsDefault = true;
				Component->ChildBodyComponentKey = PhysicsBodyComponentKey;
				Component->ControlData = ParentSpaceControlData;

				ParentSpaceControlComponentKeys.Add(ParentSpaceControlComponentKey);
			}
		}
	} // Loop over SkeletalBodySetup

	// Now bodies are created we can handle no-collision
	const TMap<FRigidBodyIndexPair, bool>& CollisionDisableTable = PhysicsAsset->CollisionDisableTable;

	for (const TPair<FRigidBodyIndexPair, bool>& PairPair : CollisionDisableTable)
	{
		if (!PairPair.Value)
		{
			const FRigidBodyIndexPair& Pair = PairPair.Key;
			int32 I1 = Pair.Indices[0];
			int32 I2 = Pair.Indices[1];
			if (I1 >= 0 && I1 < PhysicsAsset->SkeletalBodySetups.Num() && 
				I2 >= 0 && I2 < PhysicsAsset->SkeletalBodySetups.Num())
			{
				if (PhysicsAsset->SkeletalBodySetups[I1] && PhysicsAsset->SkeletalBodySetups[I2])
				{
					const FName BoneName1 = PhysicsAsset->SkeletalBodySetups[I1]->BoneName;
					const FName BoneName2 = PhysicsAsset->SkeletalBodySetups[I2]->BoneName;

					if (FRigComponentKey* BodyComponentKey1 = BoneToBodyComponentKeyMap.Find(BoneName1))
					{
						if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
							ExecuteContext.Hierarchy->FindComponent(*BodyComponentKey1)))

						if (FRigComponentKey* BodyComponentKey2 = BoneToBodyComponentKeyMap.Find(BoneName2))
						{
							if (ExecuteContext.Hierarchy->FindComponent(*BodyComponentKey2))
							{
								Component->NoCollisionBodies.Add(*BodyComponentKey2);
							}
						}
					}
				}
			}
		}
	}

	// Physics Joints
	if (bEnableJoints)
	{
		for (TObjectPtr<UPhysicsConstraintTemplate>& ConstraintTemplate : PhysicsAsset->ConstraintSetup)
		{
			if (!ConstraintTemplate)
			{
				continue;
			}

			// Note that physics assets are normally set up as child/parent for 1/2
			// However, users can create their own constraints, and some assets will be the other way round.
			FConstraintInstance& ConstraintInstance = ConstraintTemplate->DefaultInstance;
			const FName ChildBoneName = ConstraintInstance.ConstraintBone1;
			const FName ParentBoneName = ConstraintInstance.ConstraintBone2;

			const FRigComponentKey* ChildBodyComponentKey = BoneToBodyComponentKeyMap.Find(ChildBoneName);
			const FRigComponentKey* ParentBodyComponentKey = BoneToBodyComponentKeyMap.Find(ParentBoneName);

			if (!ChildBodyComponentKey || !ParentBodyComponentKey)
			{
				if (BonesToUse.IsEmpty())
				{
					// If BonesToUse is not empty, it is hard to say whether this is an
					// error/warning condition. However, if the physics asset is OK and BonesToUse
					// is empty, then we shouldn't get here.
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(
						TEXT("InstantiateFromPhysicsAsset - unable to make physics joint between %s and %s"),
						*ChildBoneName.ToString(), *ParentBoneName.ToString());
				}
				continue;
			}

			const FConstraintProfileProperties& ConstraintProfileProperties = 
				ConstraintTemplate->GetConstraintProfilePropertiesOrDefault(ConstraintProfileName);

			FRigPhysicsJointData JointData;
			SetUpJointData(JointData, ConstraintInstance, ConstraintProfileProperties);
			FRigPhysicsDriveData DriveData;
			if (bEnableDrives)
			{
				SetUpDriveData(DriveData, ConstraintProfileProperties);
			}

			// Add the component
			FRigComponentKey PhysicsJointComponentKey = Controller->AddComponent(
				FRigPhysicsJointComponent::StaticStruct(), TEXT("PhysicsJoint"), ChildBodyComponentKey->ElementKey);
			if (PhysicsJointComponentKey.IsValid())
			{
				if (FRigPhysicsJointComponent* Component = Cast<FRigPhysicsJointComponent>(
					ExecuteContext.Hierarchy->FindComponent(PhysicsJointComponentKey)))
				{
					Component->JointData = JointData;
					Component->DriveData = DriveData;
					Component->ParentBodyComponentKey = *ParentBodyComponentKey;
					Component->ChildBodyComponentKey = *ChildBodyComponentKey;

					PhysicsJointComponentKeys.Add(PhysicsJointComponentKey);
				}
			}
		} // Loop over constraint templates
	}
}

//======================================================================================================================
FRigUnit_GetPhysicsSolverSpaceData_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (UControlRig* ControlRig = Cast<UControlRig>(ExecuteContext.Hierarchy->GetOuter()))
	{
		if (FRigPhysicsSolverComponent* SolverComponent = Cast<FRigPhysicsSolverComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsSolverComponentKey)))
		{
			if (FRigPhysicsSimulation* PhysicsSimulation = Cast<FRigPhysicsSimulation>(
				ControlRig->GetPhysicsSimulation(PhysicsSolverComponentKey)))
			{
				const FRigPhysicsSimulation::FSimulationSpaceData& SimulationSpaceData = 
					PhysicsSimulation->GetSimulationSpaceData();

				LinearVelocity = SimulationSpaceData.LinearVelocity;
				AngularVelocity = SimulationSpaceData.AngularVelocity;
				LinearAcceleration = SimulationSpaceData.LinearAcceleration;
				AngularAcceleration = SimulationSpaceData.AngularAcceleration;
				Gravity = SimulationSpaceData.Gravity;
			}
		}
	}
}
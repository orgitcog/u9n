// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsBodyExecution.h"

#include "Rigs/RigHierarchyController.h"
#include "Units/RigUnitContext.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "ControlRigPhysicsModule.h"

#include "ControlRig.h"
#if WITH_EDITOR
#include "RigVMModel/RigVMController.h"
#include "ControlRigBlueprint.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigPhysicsBodyExecution)

//======================================================================================================================
FRigUnit_AddPhysicsBody_Execute()
{
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (ExecuteContext.GetEventName() != FRigUnit_PrepareForExecution::EventName)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("AddPhysicsBody can only be used during Setup"));
	}

	if (URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController())
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(
			Controller, ExecuteContext.GetInstructionIndex());
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
		}
	}
}

//======================================================================================================================
FRigUnit_HierarchyAutoCalculateCollision_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (ExecuteContext.GetEventName() != FRigUnit_PrepareForExecution::EventName)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("AutoCalculateCollision can only be used during Setup"));
	}

	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->AutoCalculateCollision(ExecuteContext.Hierarchy, MinAspectRatio, MinSize);
	}
}

//======================================================================================================================
FRigUnit_HierarchySetDynamics_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->Dynamics = Dynamics;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetCollision_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->Collision = Collision;
	}
}

//======================================================================================================================
FRigUnit_HierarchyDisableCollisionBetween_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component1 = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey1)))
	{
		Component1->NoCollisionBodies.Add(PhysicsBodyComponentKey2);
	}
}


//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodySourceBone_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodySolverSettings.SourceBone = SourceBone;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyTargetBone_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodySolverSettings.TargetBone = TargetBone;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyKinematicTarget_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->KinematicTarget = KinematicTarget;
		Component->KinematicTargetSpace = KinematicTargetSpace;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodySparseData_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodyData.UpdateFromSparseData(Data);
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyMovementType_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodyData.MovementType = MovementType;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyCollisionType_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodyData.CollisionType = CollisionType;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyGravityMultiplier_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodyData.GravityMultiplier = GravityMultiplier;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyPhysicsBlendWeight_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodyData.PhysicsBlendWeight = PhysicsBlendWeight;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyUseSkeletalAnimation_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodyData.bUseSkeletalAnimation = bUseSkeletalAnimation;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyUpdateKinematicFromSimulation_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->BodyData.bUpdateKinematicFromSimulation = bUpdateKinematicFromSimulation;
	}
}

//======================================================================================================================
FRigUnit_HierarchySetPhysicsBodyDamping_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (FRigPhysicsBodyComponent* Component = Cast<FRigPhysicsBodyComponent>(
		ExecuteContext.Hierarchy->FindComponent(PhysicsBodyComponentKey)))
	{
		Component->Dynamics.LinearDamping = LinearDamping;
		Component->Dynamics.AngularDamping = AngularDamping;
	}
}

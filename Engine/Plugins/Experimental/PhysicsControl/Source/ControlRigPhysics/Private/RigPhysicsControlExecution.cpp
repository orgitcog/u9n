// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsControlExecution.h"
#include "RigPhysicsControlComponent.h"

#include "Rigs/RigHierarchyController.h"
#include "Units/RigUnitContext.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"

#include "ControlRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigPhysicsControlExecution)

//======================================================================================================================
FRigUnit_AddPhysicsControl_Execute()
{
	if (!ExecuteContext.Hierarchy)
	{
		return;
	}
	if (ExecuteContext.GetEventName() != FRigUnit_PrepareForExecution::EventName)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("AddPhysicsControl can only be used during Setup"));
	}

	if (URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController())
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(
			Controller, ExecuteContext.GetInstructionIndex());

		ControlComponentKey = Controller->AddComponent(FRigPhysicsControlComponent::StaticStruct(), 
			FRigPhysicsControlComponent::GetDefaultName(), Owner);
		if (ControlComponentKey.IsValid())
		{
			if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
				ExecuteContext.Hierarchy->FindComponent(ControlComponentKey)))
			{
				Component->ParentBodyComponentKey = ParentBodyComponentKey;
				Component->ChildBodyComponentKey = ChildBodyComponentKey;
				Component->ControlData = ControlData;
				Component->ControlMultiplier = ControlMultiplier;
				Component->ControlTarget = ControlTarget;
			}
		}
	}
}

//======================================================================================================================
FRigUnit_HierarchySetControlData_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (ExecuteContext.Hierarchy)
	{
		if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsControlComponentKey)))
		{
			Component->ControlData = ControlData;
		}
	}
}

//======================================================================================================================
FRigUnit_HierarchySetControlTarget_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (ExecuteContext.Hierarchy)
	{
		if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsControlComponentKey)))
		{
			Component->ControlTarget = ControlTarget;
		}
	}
}

//======================================================================================================================
FRigUnit_HierarchySetControlMultiplier_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();
	if (ExecuteContext.Hierarchy)
	{
		if (FRigPhysicsControlComponent* Component = Cast<FRigPhysicsControlComponent>(
			ExecuteContext.Hierarchy->FindComponent(PhysicsControlComponentKey)))
		{
			Component->ControlMultiplier = ControlMultiplier;
		}
	}
}


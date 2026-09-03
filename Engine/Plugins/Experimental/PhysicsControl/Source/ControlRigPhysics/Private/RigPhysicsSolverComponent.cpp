// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsSolverComponent.h"
#include "PhysicsControlObjectVersion.h"
#include "Rigs/RigHierarchy.h"
#if WITH_EDITOR
#include "ControlRigPhysicsEditorStyle.h"
#endif

//======================================================================================================================
void FRigPhysicsSolverComponent::Save(FArchive& Ar)
{
	Ar.UsingCustomVersion(FPhysicsControlObjectVersion::GUID);

	FRigBaseComponent::Save(Ar);
	Ar << SolverSettings;
	Ar << SimulationSpaceSettings;
}

//======================================================================================================================
void FRigPhysicsSolverComponent::Load(FArchive& Ar)
{
	FRigBaseComponent::Load(Ar);
	Ar << SolverSettings;
	Ar << SimulationSpaceSettings;
}

//======================================================================================================================
void FRigPhysicsSolverComponent::OnAddedToHierarchy(URigHierarchy* InHierarchy, URigHierarchyController* InController)
{
	if (!IsProcedural())
	{
		// Default the material here to have friction and restitution. Then the interactions are
		// easily adjusted on the dynamic bodies.
		SolverSettings.Collision.Material.Friction = 1.0f;
		SolverSettings.Collision.Material.Restitution = 1.0f;

		InHierarchy->Notify(ERigHierarchyNotification::ComponentContentChanged, this);
	}
}

//======================================================================================================================
#if WITH_EDITOR
const FSlateIcon& FRigPhysicsSolverComponent::GetIconForUI() const
{
	static const FSlateIcon SolverIcon = FSlateIcon(
		FControlRigPhysicsEditorStyle::Get().GetStyleSetName(), "ControlRigPhysics.Component.Solver");
	return SolverIcon;
}
#endif


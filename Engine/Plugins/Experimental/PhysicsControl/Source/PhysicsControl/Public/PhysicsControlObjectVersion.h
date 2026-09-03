// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"

#define UE_API PHYSICSCONTROL_API

struct FPhysicsControlObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded,

		ControlRigIncludeDriveInJoint,

		ControlRigRemoveCurrentDataFromPhysicsComponent,

		ControlRigUseAutomaticSolver,

		ControlRigAutomaticallyAddPhysicsComponents,

		ControlRigControlAddChildBodyComponentKey,

		ControlRigControlAddControlMultiplier,

		ControlRigSeparateOutJointFromBody,

		ControlRigSupportFullConstraintData,

		ControlRigSupportFullDriveConstraintData,

		ControlRigSupportNoCollisionBodies,

		ControlRigSupportBodyDamping,

		ControlRigCollisionHasMaterial,

		ControlRigBodyDynamicsHasDensity,

		ControlRigSolverSettingsIncludesCollisionBoundsExpansion,

		ControlRigDetectTeleportFromDistanceChange,

		ControlRigSpeedThresholdForReset,

		ControlRigResetCooldownFrames,

		ControlRigSimSpaceDragMultipliers,

		ControlRigAccelerationThresholdForReset,

		ControlRigRemoveResetCooldownFrames,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	UE_API const static FGuid GUID;

private:
	FPhysicsControlObjectVersion() {}
};

#undef UE_API

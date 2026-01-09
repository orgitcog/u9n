// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_IsRecentlyRendered.h"

FRigUnit_IsRecentlyRendered_Execute()
{
	if (const USkinnedMeshComponent* Component = MeshComponent)
	{
		Result = Component->bRecentlyRendered;
	}
	else
	{
		Result = false;
	}
}

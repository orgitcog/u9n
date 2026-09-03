// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_PoseSearchDatabaseGetTags.h"
#include "PoseSearch/PoseSearchLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_PoseSearchDatabaseGetTags)

FRigUnit_PoseSearchDatabaseGetTags_Execute()
{
	if (Database)
	{
		UPoseSearchLibrary::GetDatabaseTags(Database, OutTags);
	}
}
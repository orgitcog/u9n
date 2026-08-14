// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanHeadTransform.h"

// In order to do this transformation we need to know the transformation from root bone
// to the head bone. Now there are a number of ways in which we could do that. We could obtain
// it from the face archetype (commented out code below) which would work but requires
// access to the skeleton which may not always be possible (a copy of this code may be used outside
// of UE, eg by the realtime on phone work). An alternative is to do the above in UE and dump out the 
// values and use these directly. I've done this and hardwired the values into the code for now. 
// 
// I considered storing these values in a file so they could be updated
// if needed without a new build, but rejected that for now since I feel a change of skeleton would
// be rare and have bigger implications system wide than just this issue.
// 
// All in all, hardwired values seem the pragmatic way forward right now.
// Any more complex approach feels like a sledgehammer to crack a nut.
//
// USkeleton* MetaHumanSkeleton = LoadObject<USkeleton>(nullptr, TEXT("/MetaHuman/IdentityTemplate/Face_Archetype_Skeleton.Face_Archetype_Skeleton"));
// 
// if (MetaHumanSkeleton)
// {
// 	  const FName HeadBoneName = TEXT("head");
// 	  if (!UMetaHumanPerformanceExportUtils::GetBoneGlobalTransform(MetaHumanSkeleton, HeadBoneName, HeadBoneInitialTransform))
// 	  {
// 		  UE_LOG(LogTemp, Warning, TEXT("Failed to find bone"));
// 	  }
// }
// else
// {
// 	  UE_LOG(LogTemp, Warning, TEXT("Failed to load a MetaHuman plugin skeleton"));
// }
// 
// UE_LOG(LogTemp, Warning, TEXT("%s"), *HeadBoneInitialTransform.GetRotation().ToString());
// UE_LOG(LogTemp, Warning, TEXT("%s"), *HeadBoneInitialTransform.GetLocation().ToString());

static FVector HeadBonePosition = FVector(0.000469, 0.133260, 143.358240);
static FQuat HeadBoneRotation = FQuat(0.000000211, 0.707103276, 0.000000083, -0.707110286);

static FTransform HeadBoneInitialTransform = FTransform(HeadBoneRotation, HeadBonePosition);
static FTransform HeadBoneInitialTransformInverse = HeadBoneInitialTransform.Inverse();



FTransform FMetaHumanHeadTransform::MeshToBone(const FTransform& InTransform)
{
	// Mesh pose is orientated so that its correct in Performance viewer where head looks down -X
	// A 90 degree yaw (around Z) is needed to correct it for applying to a MetaHuman where head looks down +Y
	// to give the root bone transformation of MH
	FTransform RootTransform = InTransform * FTransform(FRotator(0, -90, 0));

	// Set the root bone translation such that the head bone remains fixed at its initial position.
	RootTransform.SetLocation(-RootTransform.TransformPosition(HeadBoneInitialTransform.GetLocation()) + HeadBoneInitialTransform.GetLocation());

	// Make transform relative to the head bone
	return HeadBoneInitialTransform * RootTransform * HeadBoneInitialTransformInverse;
}

FTransform FMetaHumanHeadTransform::BoneToMesh(const FTransform& InTransform)
{
	// Opposite of above
	FTransform RootTransform = HeadBoneInitialTransformInverse * InTransform * HeadBoneInitialTransform;

	RootTransform.SetLocation(RootTransform.TransformPosition(HeadBoneInitialTransform.GetLocation()) - HeadBoneInitialTransform.GetLocation());

	return RootTransform * FTransform(FRotator(0, 90, 0));
}

FTransform FMetaHumanHeadTransform::HeadToRoot(const FTransform& InTransform)
{
	return FTransform(-HeadBonePosition) * InTransform;
}

FTransform FMetaHumanHeadTransform::RootToHead(const FTransform& InTransform)
{
	return FTransform(HeadBonePosition) * InTransform;
}

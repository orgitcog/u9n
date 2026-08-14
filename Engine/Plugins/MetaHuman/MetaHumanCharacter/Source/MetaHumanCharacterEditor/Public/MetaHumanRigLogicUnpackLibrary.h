// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_PoseDriver.h"
#include "ControlRigBlueprint.h"
#include "MetaHumanCharacter.h"

/**
 * 
 */
class METAHUMANCHARACTEREDITOR_API UMetaHumanRigLogicUnpackLibrary
{
public:
	/** Unpack a DNA file's RBF logic to the specified animation blueprint*/
	static bool UnpackRBFEvaluation(UAnimBlueprint* AnimBlueprint, USkeletalMesh* SkeletalMesh, TNotNull<UObject*> GeneratedAssetOuter, bool UnpackFingerRBFToHalfRotationControlRig, TArray<uint16>& HalfRotationSolvers, TArray<FMetaHumanBodyRigLogicGeneratedAsset>& OutGeneratedAssets);

	/** Unpack a DNA file's SwingTwist logic to the specified animation blueprint*/
	static TObjectPtr<UControlRigBlueprint> UnpackControlRigEvaluation(UAnimBlueprint* AnimBlueprint, USkeletalMesh* SkeletalMesh, TObjectPtr<UControlRigBlueprint> ControlRig, TNotNull<UObject*> GeneratedAssetOuter, bool UnpackSwingTwistEvaluation, TArray<uint16>& HalfRotationSolvers);

	/** Create a new PoseDriver node inside the specified animation blueprint. If AutoConnect is true then the new node will be connected to the Result node in the anim graph.*/
	static UAnimGraphNode_PoseDriver* CreatePoseDriverNode(const UAnimBlueprint* AnimBlueprint, bool bAutoConnect = true);

	/** Try to get a specific PoseDriver node from the animation blueprint that has the specified driver joint names.*/
	static UAnimGraphNode_PoseDriver* GetPoseDriverWithDrivers(const TArray<FName>& DriverJointNames, const UAnimBlueprint* AnimBlueprint);

	/** Try to get a PoseDriver node from the animation blueprint that has the specified tag applied.*/
	static UAnimGraphNode_PoseDriver* GetPoseDriverWithTag(const FName& DriverTag, const UAnimBlueprint* AnimBlueprint);
};


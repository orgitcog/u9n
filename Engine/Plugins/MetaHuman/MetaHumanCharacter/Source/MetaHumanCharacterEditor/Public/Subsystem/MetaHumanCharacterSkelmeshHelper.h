// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UDynamicMesh.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"

namespace UE::MetaHuman::Build
{
	/** 
	 * Creates an incomplete transient skeletal mesh that only includes the skeleton and the updated mesh descriptions.
	 * Render data is not created or submitted.
	 * 
	 * code is a stripped version of UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewSkeletalMeshAssetFromMeshLODs()
	 */
	METAHUMANCHARACTEREDITOR_API USkeletalMesh* CreateNewIncompleteSkeletalIncludingMeshDescriptions(
		UObject* InOuter,
		TArray<UDynamicMesh*> FromDynamicMeshLODs,
		USkeleton* InSkeleton,
		FGeometryScriptCreateNewSkeletalMeshAssetOptions Options,
		EGeometryScriptOutcomePins& Outcome);
}
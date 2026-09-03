// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Engine/SkeletalMesh.h"
#include "Graph/AnimNextAnimGraph.h"
#include "DetailCategoryBuilder.h"

namespace UE::AnimNext::Editor
{
	class FPostProcessAnimationCustomization
	{
	public:
		static bool OnShouldFilterPostProcessAnimation(const FAssetData& AssetData);
		static FString GetCurrentPostProcessAnimationPath(USkeletalMesh* SkeletalMesh);
		static void OnSetPostProcessAnimation(const FAssetData& AssetData, TStrongObjectPtr<USkeletalMesh> SkeletalMesh);
		static void OnCustomizeMeshDetails(IDetailLayoutBuilder& DetailLayout, TWeakObjectPtr<USkeletalMesh> SkeletalMeshWeak);
	};
}

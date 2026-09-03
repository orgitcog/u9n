// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/SkeletalMeshHalfEdgeBufferAccessor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalMeshHalfEdgeBufferAccessor, Log, All);

FName SkeletalMeshHalfEdgeBufferAccessor::GetHalfEdgeRequirementAssetTagName()
{
	static FName Tag("bRequiresSkeletalMeshHalfEdgeBuffer");
	return Tag;
}

bool SkeletalMeshHalfEdgeBufferAccessor::IsHalfEdgeRequired(TSoftObjectPtr<UObject> InAssetSoftPtr)
{
	if (InAssetSoftPtr.IsNull())
	{
		return false;
	}
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");	

	FAssetData AssetData;
	AssetRegistryModule.TryGetAssetByObjectPath(InAssetSoftPtr.ToSoftObjectPath(), AssetData);

	// Asset types that implement USkeletalMeshHalfEdgeBufferAccessor should have this tag,
	// if not, it is likely old and requires a resave to have this information saved as asset tag
	bool bIsHalfEdgeRequired = false;
	if (AssetData.GetTagValue<bool>(GetHalfEdgeRequirementAssetTagName(), bIsHalfEdgeRequired))
	{
		return bIsHalfEdgeRequired;
	}
	
	// Fallback path
	//
	// If an asset do not have this tag serialized is likely old and haven't been re-saved recently.
	// Therefore, we have to simply assume that the deformer needs half-edge buffer here to make sure it still works,
	// while giving out a warning to ask for manual resave to avoid building half edge buffers unnecessarily.
	//
	// Ideally we want to load the asset and check it directly but loading isn't safe here
	// because this function can be called from worker threads during load/build.
	
	UE_LOG(LogSkeletalMeshHalfEdgeBufferAccessor, Warning,
		TEXT("Unable to determine if Skeletal Mesh Half Edge data is required for Asset %s, default to required. "
	   "Resaving the asset may help avoid building half edge data unnecessarily"), *InAssetSoftPtr.ToString());
	
	return true;

}

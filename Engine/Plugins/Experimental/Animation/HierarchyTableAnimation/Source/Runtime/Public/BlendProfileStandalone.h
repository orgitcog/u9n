// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HierarchyTable.h"
#include "HierarchyTableBlendProfile.h"

#include "BlendProfileStandalone.generated.h"

#define UE_API HIERARCHYTABLEANIMATIONRUNTIME_API

UENUM()
enum class EBlendProfileStandaloneType
{
	WeightFactor,
	TimeFactor,
	BlendMask
};

UCLASS(MinimalAPI, EditInlineNew, BlueprintType)
class UBlendProfileStandalone : public UObject
{
	GENERATED_BODY()

public:
	// Begin UObject
	UE_API virtual void PostLoad() override;
#if WITH_EDITOR
	UE_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	UE_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif
	// End UObject

#if WITH_EDITOR
	UE_API void UpdateHierarchy();

	UE_API void UpdateCachedData();
#endif

	UE_API TObjectPtr<USkeleton> GetSkeleton() const;

public:
	UPROPERTY(VisibleAnywhere, Category=Default, AssetRegistrySearchable)
	EBlendProfileStandaloneType Type;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UHierarchyTable> Table;
#endif // WITH_EDITORONLY_DATA

	// The flattened version of the table data for use at runtime instead of,
	// slowly traversing the table tree.
	UPROPERTY()
	FBlendProfileStandaloneCachedData CachedBlendProfileData;

	UPROPERTY(VisibleAnywhere, Category=Default)
	TObjectPtr<USkeleton> Skeleton;

private:
	UPROPERTY()
	FGuid SkeletonHierarchyGuid;

	UPROPERTY()
	FGuid SkeletonVirtualBonesHierarchyGuid;

};

#undef UE_API

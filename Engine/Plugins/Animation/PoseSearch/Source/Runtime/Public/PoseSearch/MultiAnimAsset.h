// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// @todo: move UMultiAnimAsset as well as IMultiAnimAssetEditor to Engine or a base plugin for multi character animation assets

#include "MultiAnimAsset.generated.h"

#define UE_API POSESEARCH_API

class UMirrorDataTable;
class UAnimationAsset;
class USkeletalMesh;

// UObject defining tuples of UAnimationAsset(s) with associated Role(s) and relative transforms from a shared reference system via GetOrigin
UCLASS(MinimalAPI, Abstract, Experimental, BlueprintType, Category = "Animation")
class UMultiAnimAsset : public UObject
{
	GENERATED_BODY()
public:

	[[nodiscard]] UE_API virtual bool IsLooping() const PURE_VIRTUAL(UMultiAnimAsset::IsLooping, return false;);
	[[nodiscard]] UE_API virtual bool HasRootMotion() const PURE_VIRTUAL(UMultiAnimAsset::HasRootMotion, return false;);
	[[nodiscard]] UE_API virtual float GetPlayLength(const FVector& BlendParameters) const PURE_VIRTUAL(UMultiAnimAsset::GetPlayLength, return 0.f;);

#if WITH_EDITOR
	[[nodiscard]] UE_API virtual USkeletalMesh* GetPreviewMesh(const FName& Role) const PURE_VIRTUAL(UMultiAnimAsset::GetPreviewMesh, return nullptr;);
#endif // WITH_EDITOR

	[[nodiscard]] UE_API virtual int32 GetNumRoles() const PURE_VIRTUAL(UMultiAnimAsset::GetNumRoles, return 0;);
	[[nodiscard]] UE_API virtual FName GetRole(int32 RoleIndex) const PURE_VIRTUAL(UMultiAnimAsset::GetRole, return FName(););
	[[nodiscard]] UE_API virtual UAnimationAsset* GetAnimationAsset(const FName& Role) const PURE_VIRTUAL(UMultiAnimAsset::GetAnimationAsset, return nullptr;);
	[[nodiscard]] UE_API virtual FTransform GetOrigin(const FName& Role) const PURE_VIRTUAL(UMultiAnimAsset::GetOrigin, return FTransform::Identity;);

	UE_API virtual void CalculateWarpTransforms(float Time, const TConstArrayView<const FTransform> ActorRootBoneTransforms, TArrayView<FTransform> FullAlignedActorRootBoneTransforms, const TConstArrayView<const UMirrorDataTable*> MirrorDataTables = TConstArrayView<const UMirrorDataTable*>(), const TConstArrayView<bool> RelevantRoleIndexes = TArrayView<bool>()) const PURE_VIRTUAL(UMultiAnimAsset::CalculateWarpTransforms, );

	UFUNCTION(BlueprintPure, Category = "Animation", meta=(BlueprintThreadSafe, DisplayName = "Get Animation Asset"))
	UAnimationAsset* BP_GetAnimationAsset(const FName& Role) const { return GetAnimationAsset(Role); }

	UFUNCTION(BlueprintPure, Category = "Animation", meta=(BlueprintThreadSafe, DisplayName = "Get Origin"))
	FTransform BP_GetOrigin(const FName& Role) const { return GetOrigin(Role); }
};

#undef UE_API

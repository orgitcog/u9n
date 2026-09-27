// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeMeshDefinitions.h"
#include "UObject/ObjectMacros.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeStaticMeshLodDataNode.generated.h"

#define UE_API INTERCHANGEFACTORYNODES_API


namespace UE
{
	namespace Interchange
	{
		struct FStaticMeshNodeLodDataStaticData : public FBaseNodeStaticData
		{
			static const FAttributeKey& GetMeshUidsBaseKey();
			static const FAttributeKey& GetBoxCollisionMeshUidsBaseKey();
			static const FAttributeKey& GetCapsuleCollisionMeshUidsBaseKey();
			static const FAttributeKey& GetSphereCollisionMeshUidsBaseKey();
			static const FAttributeKey& GetConvexCollisionMeshUidsBaseKey();
		};
	} // namespace Interchange
} // namespace UE


UCLASS(MinimalAPI, BlueprintType)
class UInterchangeStaticMeshLodDataNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	UE_API UInterchangeStaticMeshLodDataNode();

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	UE_API virtual FString GetTypeName() const override;

#if WITH_EDITOR
	UE_API virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
	UE_API virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
#endif //WITH_EDITOR

public:
	/* Mesh UIDs can be either a scene node or a mesh node UID. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API int32 GetMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveAllMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API int32 GetBoxCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API TMap<FString, FString> GetBoxCollisionMeshMap() const;

	UE_DEPRECATED(5.6, "No longer used: Collect the keys from GetBoxCollisionMeshMap() instead.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetBoxCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetBoxColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const;

	UE_DEPRECATED(5.6, "No longer used: Use the other overload that specifies the RenderMeshUid.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddBoxCollisionMeshUid(const FString& ColliderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddBoxCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveBoxCollisionMeshUid(const FString& ColliderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveAllBoxCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API int32 GetCapsuleCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API TMap<FString, FString> GetCapsuleCollisionMeshMap() const;

	UE_DEPRECATED(5.6, "No longer used: Collect the keys from GetCapsuleCollisionMeshMap() instead.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetCapsuleCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetCapsuleColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const;

	UE_DEPRECATED(5.6, "No longer used: Use the other overload that specifies the RenderMeshUid.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddCapsuleCollisionMeshUid(const FString& ColliderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddCapsuleCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveCapsuleCollisionMeshUid(const FString& ColliderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveAllCapsuleCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API int32 GetSphereCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API TMap<FString, FString> GetSphereCollisionMeshMap() const;

	UE_DEPRECATED(5.6, "No longer used: Collect the keys from GetSphereCollisionMeshMap() instead.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetSphereCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetSphereColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const;

	UE_DEPRECATED(5.6, "No longer used: Use the other overload that specifies the RenderMeshUid.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddSphereCollisionMeshUid(const FString& ColliderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddSphereCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveSphereCollisionMeshUid(const FString& ColliderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveAllSphereCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API int32 GetConvexCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API TMap<FString, FString> GetConvexCollisionMeshMap() const;

	UE_DEPRECATED(5.6, "No longer used: Collect the keys from GetConvexCollisionMeshMap() instead.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetConvexCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API void GetConvexColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const;

	UE_DEPRECATED(5.6, "No longer used: Use the other overload that specifies the RenderMeshUid.")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddConvexCollisionMeshUid(const FString& ColliderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool AddConvexCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveConvexCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool RemoveAllConvexCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool GetOneConvexHullPerUCX(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool SetOneConvexHullPerUCX(bool AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool GetImportCollisionType(EInterchangeMeshCollision& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool SetImportCollisionType(EInterchangeMeshCollision AttributeValue);

	/** 
	 * Gets whether we're generating collision primitive shapes even if the mesh data 
	 * doesn't match the desired shape very well
	 */
	 UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	 UE_API bool GetForceCollisionPrimitiveGeneration(bool& bGenerate) const;
	 
	 /** 
	  * Sets whether we're generating collision primitive shapes even if the mesh data 
	  * doesn't match the desired shape very well
	  */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool SetForceCollisionPrimitiveGeneration(bool bGenerate);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool GetImportCollision(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	UE_API bool SetImportCollision(bool AttributeValue);


private:

	bool IsEditorOnlyDataDefined();

	UE::Interchange::TArrayAttributeHelper<FString> MeshUids;
	UE::Interchange::TMapAttributeHelper<FString, FString> BoxCollisionMeshUids;
	UE::Interchange::TMapAttributeHelper<FString, FString> CapsuleCollisionMeshUids;
	UE::Interchange::TMapAttributeHelper<FString, FString> SphereCollisionMeshUids;
	UE::Interchange::TMapAttributeHelper<FString, FString> ConvexCollisionMeshUids;

	IMPLEMENT_NODE_ATTRIBUTE_KEY(OneConvexHullPerUCX)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(ForceCollisionPrimitiveGeneration)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(ImportCollision)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(ImportCollisionType)
};

#undef UE_API

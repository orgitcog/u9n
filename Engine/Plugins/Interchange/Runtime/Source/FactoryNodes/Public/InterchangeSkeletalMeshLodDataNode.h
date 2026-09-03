// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeSkeletalMeshLodDataNode.generated.h"

#define UE_API INTERCHANGEFACTORYNODES_API

//Interchange namespace
namespace UE
{
	namespace Interchange
	{

		struct FSkeletalMeshNodeLodDataStaticData : public FBaseNodeStaticData
		{
			static UE_API const FAttributeKey& GetMeshUidsBaseKey();
		};

	}//ns Interchange
}//ns UE

UCLASS(MinimalAPI, BlueprintType)
class UInterchangeSkeletalMeshLodDataNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	UE_API UInterchangeSkeletalMeshLodDataNode();

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	UE_API virtual FString GetTypeName() const override;

#if WITH_EDITOR
	UE_API virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	UE_API virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
#endif //WITH_EDITOR

public:
	/** Query the LOD skeletal mesh factory skeleton reference. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	UE_API bool GetCustomSkeletonUid(FString& AttributeValue) const;

	/** Set the LOD skeletal mesh factory skeleton reference. Return false if the attribute could not be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	UE_API bool SetCustomSkeletonUid(const FString& AttributeValue);

	/* Return the number of mesh geometries this LOD will be made from. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	UE_API int32 GetMeshUidsCount() const;

	/* Query all mesh geometry this LOD will be made from. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	UE_API void GetMeshUids(TArray<FString>& OutMeshNames) const;

	/* Add a mesh geometry used to create this LOD geometry. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	UE_API bool AddMeshUid(const FString& MeshName);

	/* Remove a mesh geometry used to create this LOD geometry. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	UE_API bool RemoveMeshUid(const FString& MeshName);

	/* Remove all mesh geometry used to create this LOD geometry. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	UE_API bool RemoveAllMeshes();

	/** Return if the import of the class is allowed at runtime.*/
	virtual bool IsRuntimeImportAllowed() const override
	{
		return false;
	}

private:

	UE_API bool IsEditorOnlyDataDefined();

	const UE::Interchange::FAttributeKey Macro_CustomSkeletonUidKey = UE::Interchange::FAttributeKey(TEXT("__SkeletonUid__Key"));

	UE::Interchange::TArrayAttributeHelper<FString> MeshUids;
protected:
};

#undef UE_API

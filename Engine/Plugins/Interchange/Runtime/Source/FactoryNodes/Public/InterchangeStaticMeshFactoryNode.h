// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeMeshFactoryNode.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#if WITH_ENGINE
#include "Engine/StaticMesh.h"
#endif

#include "InterchangeStaticMeshFactoryNode.generated.h"


namespace UE
{
	namespace Interchange
	{
		struct FStaticMeshNodeStaticData : public FBaseNodeStaticData
		{
			static INTERCHANGEFACTORYNODES_API const FAttributeKey& GetLODScreenSizeBaseKey();
			static INTERCHANGEFACTORYNODES_API const FAttributeKey& GetSocketUidsBaseKey();
		};
	} // namespace Interchange
} // namespace UE


UCLASS(MinimalAPI, BlueprintType)
class UInterchangeStaticMeshFactoryNode : public UInterchangeMeshFactoryNode
{
	GENERATED_BODY()

public:
	INTERCHANGEFACTORYNODES_API UInterchangeStaticMeshFactoryNode();

	/**
	 * Initialize node data. Also adds it to NodeContainer.
	 * @param UniqueID - The unique ID for this node.
	 * @param DisplayLabel - The name of the node.
	 * @param InAssetClass - The class the StaticMesh factory will create for this node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API void InitializeStaticMeshNode(const FString& UniqueID, const FString& DisplayLabel, const FString& InAssetClass, UInterchangeBaseNodeContainer* NodeContainer);

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	INTERCHANGEFACTORYNODES_API virtual FString GetTypeName() const override;

	/** Get the class this node creates. */
	INTERCHANGEFACTORYNODES_API virtual class UClass* GetObjectClass() const override;

#if WITH_EDITOR

	INTERCHANGEFACTORYNODES_API virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
	INTERCHANGEFACTORYNODES_API virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

#endif //WITH_EDITOR

public:
	/** Get whether the static mesh factory should auto compute LOD Screen Sizes. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomAutoComputeLODScreenSizes(bool& AttributeValue) const;

	/** Set whether the static mesh factory should auto compute LOD Screen Sizes. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomAutoComputeLODScreenSizes(const bool& AttributeValue);

	/** Returns the number of LOD Screen Sizes the static mesh has.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API int32 GetLODScreenSizeCount() const;

	/** Returns All the LOD Screen Sizes set for the static mesh.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API void GetLODScreenSizes(TArray<float>& OutLODScreenSizes) const;

	/** Sets the LOD Screen Sizes for the static mesh.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetLODScreenSizes(const TArray<float>& InLODScreenSizes);

	/** Get whether the static mesh factory should set the Nanite build setting. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomBuildNanite(bool& AttributeValue) const;

	/** Set whether the static mesh factory should set the Nanite build setting. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomBuildNanite(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Return the number of socket UIDs this static mesh has. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API int32 GetSocketUidCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API void GetSocketUids(TArray<FString>& OutSocketUids) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool AddSocketUid(const FString& SocketUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool AddSocketUids(const TArray<FString>& InSocketUids);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool RemoveSocketUd(const FString& SocketUid);

	/** Get whether the static mesh should build a reversed index buffer. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomBuildReversedIndexBuffer(bool& AttributeValue) const;

	/** Set whether the static mesh should build a reversed index buffer. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomBuildReversedIndexBuffer(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Get whether the static mesh should generate lightmap UVs. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomGenerateLightmapUVs(bool& AttributeValue) const;

	/** Set whether the static mesh should generate lightmap UVs. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomGenerateLightmapUVs(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/**
	 * Get whether to generate the distance field by treating every triangle hit as a front face.  
	 * This prevents the distance field from being discarded due to the mesh being open, but also lowers distance field ambient occlusion quality.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomGenerateDistanceFieldAsIfTwoSided(bool& AttributeValue) const;

	/**
	 * Set whether to generate the distance field by treating every triangle hit as a front face.
	 * This prevents the distance field from being discarded due to the mesh being open, but also lowers distance field ambient occlusion quality.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomGenerateDistanceFieldAsIfTwoSided(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Get whether the static mesh is set up for use with physical material masks. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomSupportFaceRemap(bool& AttributeValue) const;

	/** Set whether the static mesh is set up for use with physical material masks. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomSupportFaceRemap(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Get the amount of padding used to pack UVs for the static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomMinLightmapResolution(int32& AttributeValue) const;

	/** Set the amount of padding used to pack UVs for the static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomMinLightmapResolution(const int32& AttributeValue, bool bAddApplyDelegate = true);

	/** Get the index of the UV that is used as the source for generating lightmaps for the static mesh.  */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomSrcLightmapIndex(int32& AttributeValue) const;

	/** Set the index of the UV that is used as the source for generating lightmaps for the static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomSrcLightmapIndex(const int32& AttributeValue, bool bAddApplyDelegate = true);

	/** Get the index of the UV that is used to store generated lightmaps for the static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomDstLightmapIndex(int32& AttributeValue) const;

	/** Set the index of the UV that is used to store generated lightmaps for the static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomDstLightmapIndex(const int32& AttributeValue, bool bAddApplyDelegate = true);

	/** Get the local scale that is applied when building the static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomBuildScale3D(FVector& AttributeValue) const;

	/** Set the local scale that is applied when building the static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomBuildScale3D(const FVector& AttributeValue, bool bAddApplyDelegate = true);

	/**
	 * Get the scale to apply to the mesh when allocating the distance field volume texture.
	 * The default scale is 1, which assumes that the mesh will be placed unscaled in the world.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomDistanceFieldResolutionScale(float& AttributeValue) const;

	/**
	 * Set the scale to apply to the mesh when allocating the distance field volume texture.
	 * The default scale is 1, which assumes that the mesh will be placed unscaled in the world.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomDistanceFieldResolutionScale(const float& AttributeValue, bool bAddApplyDelegate = true);

	/** Get the static mesh asset whose distance field will be used as the distance field for the imported mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomDistanceFieldReplacementMesh(FSoftObjectPath& AttributeValue) const;

	/** Set the static mesh asset whose distance field will be used as the distance field for the imported mesh. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomDistanceFieldReplacementMesh(const FSoftObjectPath& AttributeValue, bool bAddApplyDelegate = true);

	/**
	 * Get the maximum number of Lumen mesh cards to generate for this mesh.
	 * More cards means that the surface will have better coverage, but will result in increased runtime overhead.
	 * Set this to 0 to disable mesh card generation for this mesh.
	 * The default is 12.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool GetCustomMaxLumenMeshCards(int32& AttributeValue) const;

	/**
	 * Set the maximum number of Lumen mesh cards to generate for this mesh.
	 * More cards means that the surface will have better coverage, but will result in increased runtime overhead.
	 * Set this to 0 to disable mesh card generation for this mesh.
	 * The default is 12.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	INTERCHANGEFACTORYNODES_API bool SetCustomMaxLumenMeshCards(const int32& AttributeValue, bool bAddApplyDelegate = true);

private:
	INTERCHANGEFACTORYNODES_API virtual void FillAssetClassFromAttribute() override;
	INTERCHANGEFACTORYNODES_API virtual bool SetNodeClassFromClassAttribute() override;

	const UE::Interchange::FAttributeKey Macro_CustomBuildReversedIndexBufferKey = UE::Interchange::FAttributeKey(TEXT("BuildReversedIndexBuffer"));
	const UE::Interchange::FAttributeKey Macro_CustomGenerateLightmapUVsKey = UE::Interchange::FAttributeKey(TEXT("GenerateLightmapUVs"));
	const UE::Interchange::FAttributeKey Macro_CustomGenerateDistanceFieldAsIfTwoSidedKey = UE::Interchange::FAttributeKey(TEXT("GenerateDistanceFieldAsIfTwoSided"));
	const UE::Interchange::FAttributeKey Macro_CustomSupportFaceRemapKey = UE::Interchange::FAttributeKey(TEXT("SupportFaceRemap"));
	const UE::Interchange::FAttributeKey Macro_CustomMinLightmapResolutionKey = UE::Interchange::FAttributeKey(TEXT("MinLightmapResolution"));
	const UE::Interchange::FAttributeKey Macro_CustomSrcLightmapIndexKey = UE::Interchange::FAttributeKey(TEXT("SrcLightmapIndex"));
	const UE::Interchange::FAttributeKey Macro_CustomDstLightmapIndexKey = UE::Interchange::FAttributeKey(TEXT("DstLightmapIndex"));
	const UE::Interchange::FAttributeKey Macro_CustomBuildScale3DKey = UE::Interchange::FAttributeKey(TEXT("BuildScale3D"));
	const UE::Interchange::FAttributeKey Macro_CustomDistanceFieldResolutionScaleKey = UE::Interchange::FAttributeKey(TEXT("DistanceFieldResolutionScale"));
	const UE::Interchange::FAttributeKey Macro_CustomDistanceFieldReplacementMeshKey = UE::Interchange::FAttributeKey(TEXT("DistanceFieldReplacementMesh"));
	const UE::Interchange::FAttributeKey Macro_CustomMaxLumenMeshCardsKey = UE::Interchange::FAttributeKey(TEXT("MaxLumenMeshCards"));
	const UE::Interchange::FAttributeKey Macro_CustomBuildNaniteKey = UE::Interchange::FAttributeKey(TEXT("BuildNanite"));
	const UE::Interchange::FAttributeKey Macro_CustomAutoComputeLODScreenSizesKey = UE::Interchange::FAttributeKey(TEXT("AutoComputeLODScreenSizes"));


	UE::Interchange::TArrayAttributeHelper<float> LODScreenSizes;
	UE::Interchange::TArrayAttributeHelper<FString> SocketUids;

protected:
	
#if WITH_EDITORONLY_DATA
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(BuildNanite, bool, UStaticMesh, TEXT("NaniteSettings.bEnabled"));
#endif

	INTERCHANGEFACTORYNODES_API bool ApplyCustomBuildReversedIndexBufferToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomBuildReversedIndexBufferFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomGenerateLightmapUVsToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomGenerateLightmapUVsFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomGenerateDistanceFieldAsIfTwoSidedToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomGenerateDistanceFieldAsIfTwoSidedFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomSupportFaceRemapToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomSupportFaceRemapFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomMinLightmapResolutionToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomMinLightmapResolutionFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomSrcLightmapIndexToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomSrcLightmapIndexFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomDstLightmapIndexToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomDstLightmapIndexFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomBuildScale3DToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomBuildScale3DFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomDistanceFieldResolutionScaleToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomDistanceFieldResolutionScaleFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomDistanceFieldReplacementMeshToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomDistanceFieldReplacementMeshFromAsset(UObject* Asset);
	INTERCHANGEFACTORYNODES_API bool ApplyCustomMaxLumenMeshCardsToAsset(UObject* Asset) const;
	INTERCHANGEFACTORYNODES_API bool FillCustomMaxLumenMeshCardsFromAsset(UObject* Asset);

#if WITH_ENGINE
	TSubclassOf<UStaticMesh> AssetClass = nullptr;
#endif
};

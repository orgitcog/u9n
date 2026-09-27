// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Item/MetaHumanGroomPipeline.h"

#include "MetaHumanDefaultGroomPipeline.generated.h"

struct FInstancedPropertyBag;
class UMaterialInstanceConstant;
class UTexture;

/**
 * Class defining the MetaHuman groom standard - it lists all the available parameters
 * and maps them against the material parameter name.
 * 
 * This class is not meant to be instantiated, it's only for storing properties and metadata.
 */
UCLASS()
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultGroomPipelineMaterialParameters : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (MaterialParamName = "hairMelanin", GroomCategory = "Default", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float Melanin = 0.16f;

	UPROPERTY(meta = (MaterialParamName = "hairRedness", GroomCategory = "Default", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float Redness = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "HairRoughness", GroomCategory = "Default", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float Roughness = 0.25f;

	UPROPERTY(meta = (MaterialParamName = "WhiteAmount", GroomCategory = "Default", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float Whiteness = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "LightAmount", GroomCategory = "Default", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float Lightness = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "HairDye", GroomCategory = "Default"))
	FLinearColor DyeColor;

	UPROPERTY(meta = (MaterialParamName = "Ombre", GroomCategory = "Ombre"))
	bool bUseOmbre = false;

	UPROPERTY(meta = (MaterialParamName = "OmbreMelanin", GroomCategory = "Ombre", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float OmbreU = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "OmbreRedness", GroomCategory = "Ombre", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float OmbreV = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "OmbreHairDye", GroomCategory = "Ombre"))
	FLinearColor OmbreColor = FLinearColor::White;

	UPROPERTY(meta = (MaterialParamName = "OmbreShift", GroomCategory = "Ombre", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float OmbreShift = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "OmbreContrast", GroomCategory = "Ombre", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float OmbreContrast = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "OmbreIntensity", GroomCategory = "Ombre", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float OmbreIntensity = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "Region", GroomCategory = "Regions"))
	bool bUseRegions = false;

	UPROPERTY(meta = (MaterialParamName = "RegionMelanin", GroomCategory = "Regions", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float RegionsU = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "RegionRedness", GroomCategory = "Regions", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float RegionsV = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "RegionhairDye", GroomCategory = "Regions"))
	FLinearColor RegionsColor = FLinearColor::White;

	UPROPERTY(meta = (MaterialParamName = "Highlights", GroomCategory = "Highlights"))
	bool bUseHighlights = false;

	UPROPERTY(meta = (MaterialParamName = "HighlightsMelanin", GroomCategory = "Highlights", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float HighlightsU = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "HighlightsRedness", GroomCategory = "Highlights", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float HighlightsV = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "HighlightsHairDye", GroomCategory = "Highlights"))
	FLinearColor HighlightsColor = FLinearColor::White;

	UPROPERTY(meta = (MaterialParamName = "HighlightsBlending", GroomCategory = "Highlights", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float HighlightsBlending = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "HighlightsIntensity", GroomCategory = "Highlights", Min = 0.0f, UIMin = 0.0f, Max = 1.0f, UIMax = 1.0f))
	float HighlightsIntensity = 0.0f;

	UPROPERTY(meta = (MaterialParamName = "HighlightsVariationNumber", GroomCategory = "Highlights", Min = 0.0f, UIMin = 0.0f, Max = 3.0f, UIMax = 3.0f))
	float HighlightsVariation = 0.0f;
};

UENUM()
enum class EHairLODTransition : uint8
{
	StrandsToCardsToMesh UMETA(DisplayName = "Strands->Cards->Mesh"),
	StrandsToCardsToTexture UMETA(DisplayName = "Strands->Cards->Texture"),
	StrandsToCardsAndTextureToTexture UMETA(DisplayName = "Strands->Cards & Texture->Texture"),
	StrandsToCardsAndTextureToMeshToTexture UMETA(DisplayName = "Strands->Cards & Texture->Mesh & Texture"),
	StrandsToTexture UMETA(DisplayName = "Strands->Texture"),
};

/**
 * Groom pipeline used for compatibility with the original MetaHumanCreator.
 */
UCLASS(Blueprintable, EditInlineNew)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultGroomPipeline : public UMetaHumanGroomPipeline
{
	GENERATED_BODY()

public:
	UMetaHumanDefaultGroomPipeline();

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~End UObject interface

	UPROPERTY(EditAnywhere, Category = "Support")
	EMetaHumanRuntimeMaterialParameterSlotTarget SlotTarget = EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames;

	UPROPERTY(EditAnywhere, Category = "Support", meta = (EditCondition = "SlotTarget == EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames"))
	TArray<FName> SlotNames;

	UPROPERTY(EditAnywhere, Category = "Support", meta = (EditCondition = "SlotTarget == EMetaHumanRuntimeMaterialParameterSlotTarget::SlotIndices"))
	TArray<int32> SlotIndices;

	UPROPERTY(EditAnywhere, Category = "Support")
	bool bSupportsOmbre = true;

	UPROPERTY(EditAnywhere, Category = "Support")
	bool bSupportsRegions = true;

	UPROPERTY(EditAnywhere, Category = "Support")
	bool bSupportsHightlights = true;

	UPROPERTY(EditAnywhere, Category = "Groom")
	float HighlightsRootDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Groom")
	TSoftObjectPtr<UTexture> HighlightsMask;

	/** The texture that will be used to bake the groom onto the face material at worse LODs */
	UPROPERTY(EditAnywhere, Category = "Groom")
	TSoftObjectPtr<UTexture> BakedGroomTexture;

	/** The LOD strategy for this groom */
	UPROPERTY(EditAnywhere, Category = "Groom")
	EHairLODTransition LODTransition;

	/** The best LOD (lowest index) that this groom will be baked onto the face */
	UPROPERTY(EditAnywhere, Category = "Groom")
	int32 GroomTextureMinLOD = 5;

#if WITH_EDITOR
	void SetFaceMaterialParameters(
		const TArray<UMaterialInstanceConstant*>& FaceMaterials,
		const TArray<int32>& LODToMaterial,
		FName SlotName, 
		const FInstancedPropertyBag& InstanceParameters, 
		bool bHideHair,
		int32& OutFirstLODBaked) const;
#endif

protected:
	virtual void OverrideInitialMaterialValues(TNotNull<UMaterialInstanceDynamic*> InMID, FName InSlotName, int32 SlotIndex) const override;

private:
#if WITH_EDITOR
	void SetFaceMaterialParametersForLOD(
		UMaterialInstanceConstant* FaceMaterial, 
		FName SlotName, 
		const FInstancedPropertyBag& InstanceParameters,
		UTexture* Texture) const;

	void AddRuntimeParameter(TNotNull<FProperty*> InProperty, const FName& InMaterialParameterName);
#endif

	void UpdateParameters();
};

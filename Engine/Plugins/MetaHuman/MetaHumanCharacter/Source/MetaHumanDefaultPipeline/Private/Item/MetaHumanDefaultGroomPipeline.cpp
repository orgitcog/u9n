// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanDefaultGroomPipeline.h"

#include "MetaHumanDefaultPipelineLog.h"

#include "Engine/Texture.h"
#include "Logging/StructuredLog.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StructUtils/PropertyBag.h"

namespace UE::MetaHuman::DefaultGroomPipeline::Private
{
	namespace CategoryName
	{
		static constexpr FStringView Regions = TEXTVIEW("Regions");
		static constexpr FStringView Ombre = TEXTVIEW("Ombre");
		static constexpr FStringView Highlights = TEXTVIEW("Highlights");
	}

	namespace MetaDataKey
	{
		static const FName GroomCategory = FName("GroomCategory");
		static const FName MaterialParamName = FName("MaterialParamName");
	}
}

UMetaHumanDefaultGroomPipeline::UMetaHumanDefaultGroomPipeline()
	: UMetaHumanGroomPipeline()
{
	UpdateParameters();
}

#if WITH_EDITOR
void UMetaHumanDefaultGroomPipeline::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipeline, bSupportsRegions)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipeline, bSupportsOmbre)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipeline, bSupportsHightlights)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipeline, SlotTarget)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipeline, SlotNames)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipeline, SlotIndices))
	{
		UpdateParameters();
	}
}

void UMetaHumanDefaultGroomPipeline::AddRuntimeParameter(TNotNull<FProperty*> InProperty, const FName& InMaterialParameterName)
{
	using namespace UE::MetaHuman::MaterialUtils;

	FMetaHumanMaterialParameter& Param = RuntimeMaterialParameters.AddDefaulted_GetRef();
	Param.InstanceParameterName = InProperty->GetFName();
	Param.SlotTarget = SlotTarget;
	Param.SlotNames = SlotNames;
	Param.SlotIndices = SlotIndices;
	Param.MaterialParameter.Name = InMaterialParameterName;
	Param.ParameterType = PropertyToParameterType(InProperty);
	Param.PropertyMetadata = CopyMetadataFromProperty(InProperty);
}
#endif

void UMetaHumanDefaultGroomPipeline::UpdateParameters()
{
#if WITH_EDITOR
	using namespace UE::MetaHuman::DefaultGroomPipeline::Private;

	RuntimeMaterialParameters.Empty();

	for (TFieldIterator<FProperty> PropertyIterator(UMetaHumanDefaultGroomPipelineMaterialParameters::StaticClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;

		if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			continue;
		}

		const FString GroomCategory = Property->GetMetaData(MetaDataKey::GroomCategory);

		if (GroomCategory.IsEmpty()
			|| (!bSupportsOmbre && GroomCategory == CategoryName::Ombre)
			|| (!bSupportsRegions && GroomCategory == CategoryName::Regions)
			|| (!bSupportsHightlights && GroomCategory == CategoryName::Highlights))
		{
			continue;
		}

		const FString MaterialParamName = Property->GetMetaData(MetaDataKey::MaterialParamName);

		if (MaterialParamName.IsEmpty())
		{
			continue;
		}

		AddRuntimeParameter(Property, FName(MaterialParamName));
	}
#endif
}

#if WITH_EDITOR
void UMetaHumanDefaultGroomPipeline::SetFaceMaterialParameters(
	const TArray<UMaterialInstanceConstant*>& FaceMaterials,
	const TArray<int32>& LODToMaterial,
	FName SlotName, 
	const FInstancedPropertyBag& InstanceParameters, 
	bool bHideHair,
	int32& OutFirstLODBaked) const
{
	check(FaceMaterials.Num() > 0);
	check(FaceMaterials[0]);

	OutFirstLODBaked = INDEX_NONE;

	UTexture* DefaultTexture = nullptr;
	bool bParameterExists = FaceMaterials[0]->GetTextureParameterDefaultValue(*(SlotName.ToString() + TEXT("AttributeMap")), DefaultTexture);
	// If there's no matching texture parameter this groom should not be represented as a texture
	if (!bParameterExists)
	{
		return;
	}

	UTexture* LoadedBakedGroomTexture = BakedGroomTexture.LoadSynchronous();
	
	if (LoadedBakedGroomTexture == nullptr || bHideHair)
	{
		// Not baking the texture, but the material parameter exists, so set it to the default

		for (UMaterialInstanceConstant* Material : FaceMaterials)
		{
			SetFaceMaterialParametersForLOD(Material, SlotName, InstanceParameters, DefaultTexture);
		}

		return;
	}

	OutFirstLODBaked = FMath::Max(GroomTextureMinLOD, 0);
	const bool bIsProduction = true;
	const bool bForceCards = false;

	if (!bIsProduction 
		&& bForceCards 
		&& (LODTransition == EHairLODTransition::StrandsToCardsAndTextureToTexture 
			|| LODTransition == EHairLODTransition::StrandsToCardsAndTextureToMeshToTexture 
			|| LODTransition == EHairLODTransition::StrandsToTexture))
	{
		OutFirstLODBaked = 0;
	}

	for (int32 LODIndex = 0; LODIndex < LODToMaterial.Num(); LODIndex++)
	{
		const int32 MaterialIndex = LODToMaterial[LODIndex];
		if (MaterialIndex == INDEX_NONE)
		{
			// This LOD has no material (e.g. the LOD has been removed from the mesh), so silently
			// skip it.
			continue;
		}

		if (!FaceMaterials.IsValidIndex(MaterialIndex))
		{
			UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "SetFaceMaterialParameters: Index from LODToMaterial out of range of FaceMaterials array ({Index}/{FaceMaterials})", MaterialIndex, FaceMaterials.Num());
			continue;
		}

		UMaterialInstanceConstant* Material = FaceMaterials[MaterialIndex];
		check(Material);

		if (LODIndex < OutFirstLODBaked)
		{
			SetFaceMaterialParametersForLOD(Material, SlotName, InstanceParameters, DefaultTexture);
		}
		else
		{
			SetFaceMaterialParametersForLOD(Material, SlotName, InstanceParameters, LoadedBakedGroomTexture);
		}
	}
}

static void TrySetScalarParameterFromPropertyBag(UMaterialInstanceConstant* Material, const FInstancedPropertyBag& InstanceParameters, FName ParameterName, FName PropertyName)
{
	if (const float* Value = InstanceParameters.GetValueFloat(PropertyName).TryGetValue())
	{
		Material->SetScalarParameterValueEditorOnly(ParameterName, *Value);
	}
}

static void TrySetBooleanScalarParameterFromPropertyBag(UMaterialInstanceConstant* Material, const FInstancedPropertyBag& InstanceParameters, FName ParameterName, FName PropertyName)
{
	if (const bool* Value = InstanceParameters.GetValueBool(PropertyName).TryGetValue())
	{
		Material->SetScalarParameterValueEditorOnly(ParameterName, *Value ? 1.0f : 0.0f);
	}
}

static void TrySetVectorParameterFromPropertyBag(UMaterialInstanceConstant* Material, const FInstancedPropertyBag& InstanceParameters, FName ParameterName, FName PropertyName)
{
	if (const FStructView* Value = InstanceParameters.GetValueStruct(PropertyName, TBaseStructure<FLinearColor>::Get()).TryGetValue())
	{
		Material->SetVectorParameterValueEditorOnly(ParameterName, Value->Get<FLinearColor>());
	}
}

static void TrySetTextureParameterFromPropertyBag(UMaterialInstanceConstant* Material, const FInstancedPropertyBag& InstanceParameters, FName ParameterName, FName PropertyName)
{
	if (UObject* const* Value = InstanceParameters.GetValueObject(PropertyName, UTexture::StaticClass()).TryGetValue())
	{
		Material->SetTextureParameterValueEditorOnly(ParameterName, CastChecked<UTexture>(*Value));
	}
}

void UMetaHumanDefaultGroomPipeline::SetFaceMaterialParametersForLOD(
	UMaterialInstanceConstant* FaceMaterial, 
	FName SlotName, 
	const FInstancedPropertyBag& InstanceParameters,
	UTexture* Texture) const
{
	FaceMaterial->SetTextureParameterValueEditorOnly(*(SlotName.ToString() + TEXT("AttributeMap")), Texture);
	TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, *(SlotName.ToString() + TEXT("Melanin")), GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Melanin));
	TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, *(SlotName.ToString() + TEXT("Redness")), GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Redness));
	TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, *(SlotName.ToString() + TEXT("WhiteAmount")), GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, Whiteness));
	TrySetVectorParameterFromPropertyBag(FaceMaterial, InstanceParameters, *(SlotName.ToString() + TEXT("DyeColor")), GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, DyeColor));
	FaceMaterial->SetScalarParameterValueEditorOnly(FName("ShowBakedGroomTextures"), 1.0f);

	// For now, only hair slot can have secondary colors
	if (SlotName == "Hair")
	{
		// Color regions
		if (bSupportsRegions)
		{
			TrySetBooleanScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "Region", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, bUseRegions));
			TrySetVectorParameterFromPropertyBag(FaceMaterial, InstanceParameters, "RegionhairDye", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, RegionsColor));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "RegionMelanin", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, RegionsU));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "RegionRedness", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, RegionsV));
		}

		// Ombre
		if (bSupportsOmbre)
		{
			TrySetBooleanScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "Ombre", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, bUseOmbre));
			TrySetVectorParameterFromPropertyBag(FaceMaterial, InstanceParameters, "OmbrehairDye", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreColor));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "OmbreMelanin", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreU));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "OmbreRedness", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreV));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "OmbreShift", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreShift));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "OmbreContrast", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreContrast));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "OmbreIntensity", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, OmbreIntensity));
		}

		// Highlights
		if (bSupportsHightlights)
		{
			TrySetBooleanScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "Highlights", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, bUseHighlights));
			TrySetVectorParameterFromPropertyBag(FaceMaterial, InstanceParameters, "HighlightshairDye", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsColor));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "HighlightsMelanin", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsU));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "HighlightsRedness", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsV));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "HighlightsBlending", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsBlending));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "HighlightsIntensity", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsIntensity));
			TrySetScalarParameterFromPropertyBag(FaceMaterial, InstanceParameters, "HighlightsVariationNumber", GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultGroomPipelineMaterialParameters, HighlightsVariation));
			FaceMaterial->SetScalarParameterValueEditorOnly(FName("HighlightsRootDistance"), HighlightsRootDistance);
		}

		if (UTexture* LoadedHighlightsMask = HighlightsMask.LoadSynchronous())
		{
			FaceMaterial->SetTextureParameterValueEditorOnly(FName("HighlightsMask"), LoadedHighlightsMask);
		}
		else
		{
			UTexture* DefaultHighlightsMask = nullptr;
			if (FaceMaterial->GetTextureParameterDefaultValue(FName("HighlightsMask"), DefaultHighlightsMask))
			{
				FaceMaterial->SetTextureParameterValueEditorOnly(FName("HighlightsMask"), DefaultHighlightsMask);
			}
		}
	}
}
#endif

void UMetaHumanDefaultGroomPipeline::OverrideInitialMaterialValues(TNotNull<UMaterialInstanceDynamic*> InMID, FName InSlotName, int32 SlotIndex) const
{
	InMID->SetScalarParameterValue(FName("HighlightsRootDistance"), HighlightsRootDistance);
	InMID->SetTextureParameterValue(FName("HighlightsMask"), HighlightsMask.LoadSynchronous());
}

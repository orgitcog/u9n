// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InterchangeUsdDefinitions.generated.h"

UENUM(BlueprintType)
enum class EInterchangeUsdPrimvar : uint8
{
	/** Store only the standard primvars such as UVs, VertexColors, etc.*/
	Standard = 0,

	/** Store only primvars in the Mesh Description used for baking to textures (basically <geompropvalue> node from MaterialX shadergraphs that are converted to <image>)*/
	Bake,

	/** Store all primvars in the MeshDescription, regardless*/
	All
};

namespace UE::Interchange::USD
{
	const FString USDContextTag = TEXT("USD");

	// Name of a custom attribute added to translated nodes to contain their geometry purpose (proxy, render, guide, etc.)
	const FString GeometryPurposeIdentifier = TEXT("USD_Geometry_Purpose");

	// Prefixes we use to stash some primvar mapping information as custom attributes on mesh / material nodes, so that
	// the USD Pipeline can produce primvar-compatible materials
	const FString PrimvarUVIndexAttributePrefix = TEXT("USD_PrimvarUVIndex_");
	const FString ParameterToPrimvarAttributePrefix = TEXT("USD_ParameterPrimvar_");

	// Additional suffix we add to the UID of all primvar-compatible materials
	const FString CompatibleMaterialUidSuffix = TEXT("_USD_CompatibleMaterial_");

	// Some tokens we add to the material parameter name for USD material nodes. Put here because we need to use the
	// same tokens on the translator and the USD Pipeline, when computing primvar-compatible materials
	const FString UseTextureParameterPrefix = TEXT("Use");
	const FString UseTextureParameterSuffix = TEXT("Texture");
	const FString UVIndexParameterSuffix = TEXT("UVIndex");

	// Flag indicating whether we should parse a UInterchangeShaderGraphNode on the InterchangeUSDPipeline.
	// This is now only used for the compatible primvar code
	const FString ParseMaterialIdentifier = TEXT("USD_MI_ParseMaterial");

	// Used for volumetric material parameters, whenever we assign an SVT to a material as a fallback due
	// to it's field name only
	const FString VolumeFieldNameMaterialParameterPrefix = TEXT("USD_FieldName_");

	// Custom attribute keys used to describe SVT info extracted from the USD custom schemas.
	// We add these to the Volume nodes that the USD translator emits.
	namespace SparseVolumeTexture
	{
		const FString AttributesAFormat = TEXT("USD_AttributesA_Format");
		const FString AttributesBFormat = TEXT("USD_AttributesB_Format");

		const FString AttributesAChannelR = TEXT("USD_AttributesA_R");
		const FString AttributesAChannelG = TEXT("USD_AttributesA_G");
		const FString AttributesAChannelB = TEXT("USD_AttributesA_B");
		const FString AttributesAChannelA = TEXT("USD_AttributesA_A");
		const FString AttributesBChannelR = TEXT("USD_AttributesB_R");
		const FString AttributesBChannelG = TEXT("USD_AttributesB_G");
		const FString AttributesBChannelB = TEXT("USD_AttributesB_B");
		const FString AttributesBChannelA = TEXT("USD_AttributesB_A");
	}

	namespace Primvar
	{
		const FString Number = TEXT("USD_PrimvarNumber");
		// In case of "Number" primvars, the user should concatenate the Index to this attribute
		const FString Name = TEXT("USD_PrimvarName");
		const FString TangentSpace = TEXT("USD_PrimvarTangentSpace");
		// Attribute that informs about the UID of a ShaderNode of type TextureSample
		const FString ShaderNodeTextureSample = TEXT("USD_ShaderNodeTextureSample");
		// Attribute that informs how we should handle primvars on MeshDescriptions
		const FString Import = TEXT("USD_Import_Primvars");
	}
}	 // namespace UE::Interchange::USD

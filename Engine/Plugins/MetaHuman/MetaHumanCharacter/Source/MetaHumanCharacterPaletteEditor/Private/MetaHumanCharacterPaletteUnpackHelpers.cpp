// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterPaletteUnpackHelpers.h"

#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace UE::MetaHuman::PaletteUnpackHelpers
{

UMaterialInstanceConstant* CreateMaterialInstanceCopy(TNotNull<const UMaterialInstance*> InMaterialInstance, TNotNull<UObject*> InOuter)
{
	check(InMaterialInstance->Parent);

	const FString MaterialName = InMaterialInstance->GetName();

	constexpr FStringView PrefixMID = TEXTVIEW("MID_");
	constexpr FStringView PrefixMIC = TEXTVIEW("MIC_");

	FName MaterialConstantName;

	if (MaterialName.StartsWith(PrefixMID, ESearchCase::CaseSensitive))
	{
		MaterialConstantName = FName{ PrefixMIC + InMaterialInstance->GetName().RightChop(PrefixMID.Len()) };
	}
	else
	{
		MaterialConstantName = MakeUniqueObjectName(InOuter, InMaterialInstance->GetClass(), InMaterialInstance->GetFName());
	}

	UMaterialInstanceConstant* MaterialInstanceConstant = NewObject<UMaterialInstanceConstant>(InOuter, FName{ MaterialConstantName });
	MaterialInstanceConstant->SetParentEditorOnly(InMaterialInstance->Parent);

	// Ideally we would use CopyMaterialUniformParametersEditorOnly, however, that function will override the parameters even if they are are the same.
	// This breaks the chain of material parameters for the LOD materials so we use a custom function to only copy parameters when they are actually different
	// from the material we are copying from
	auto CopyMaterialParametersIfNeeded = [](EMaterialParameterType ParamType, const UMaterialInstance* SourceMaterial, UMaterialInstanceConstant* TargetMaterial)
		{
			if (SourceMaterial && TargetMaterial)
			{
				TMap<FMaterialParameterInfo, FMaterialParameterMetadata> SourceParams;
				SourceMaterial->GetAllParametersOfType(ParamType, SourceParams);

				TMap<FMaterialParameterInfo, FMaterialParameterMetadata> TargetParams;
				TargetMaterial->GetAllParametersOfType(ParamType, TargetParams);

				for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& SourceParamPair : SourceParams)
				{
					const FMaterialParameterInfo& SourceParamInfo = SourceParamPair.Key;
					const FMaterialParameterMetadata& SourceParam = SourceParamPair.Value;

					check(TargetParams.Contains(SourceParamInfo));
					const FMaterialParameterMetadata& TargetParam = TargetParams[SourceParamInfo];

					if (SourceParam.Value != TargetParam.Value)
					{
						switch (ParamType)
						{
							case EMaterialParameterType::Scalar:
								TargetMaterial->SetScalarParameterValueEditorOnly(SourceParamInfo, SourceParam.Value.AsScalar());
								break;

							case EMaterialParameterType::Vector:
								TargetMaterial->SetVectorParameterValueEditorOnly(SourceParamInfo, SourceParam.Value.AsLinearColor());
								break;

							case EMaterialParameterType::Texture:
								TargetMaterial->SetTextureParameterValueEditorOnly(SourceParamInfo, SourceParam.Value.Texture);
								break;

							case EMaterialParameterType::StaticSwitch:
								TargetMaterial->SetStaticSwitchParameterValueEditorOnly(SourceParamInfo, SourceParam.Value.AsStaticSwitch());
								break;

							default:
								break;
						}
					}
				}
			}
		};

	CopyMaterialParametersIfNeeded(EMaterialParameterType::Scalar, InMaterialInstance, MaterialInstanceConstant);
	CopyMaterialParametersIfNeeded(EMaterialParameterType::Vector, InMaterialInstance, MaterialInstanceConstant);
	CopyMaterialParametersIfNeeded(EMaterialParameterType::Texture, InMaterialInstance, MaterialInstanceConstant);
	CopyMaterialParametersIfNeeded(EMaterialParameterType::StaticSwitch, InMaterialInstance, MaterialInstanceConstant);

	MaterialInstanceConstant->PostEditChange();

	return MaterialInstanceConstant;
}

} // UE::MetaHuman::PaletteUnpackHelpers

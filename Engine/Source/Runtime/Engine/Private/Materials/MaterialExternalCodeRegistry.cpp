// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialExternalCodeRegistry.h"
#include "Misc/Parse.h"


/*
 * FMaterialExternalCodeEnvironmentDefine
 */

void FMaterialExternalCodeEnvironmentDefine::UpdateHash(FSHA1& Hasher) const
{
	FString NameString = Name.ToString();
	Hasher.UpdateWithString(*NameString, NameString.Len());
	Hasher.Update(ShaderFrequency);
}


/*
 * FMaterialExternalCodeDeclaration 
 */

void FMaterialExternalCodeDeclaration::UpdateHash(FSHA1& Hasher) const
{
	const uint8 bIsInlinedByte = bIsInlined ? 1 : 0;
	Hasher.Update(&bIsInlinedByte, sizeof(bIsInlinedByte));

	Hasher.Update(ReturnType);

	FString NameString = Name.ToString();
	Hasher.UpdateWithString(*NameString, NameString.Len());

	Hasher.UpdateWithString(*Definition, Definition.Len());
	Hasher.Update(Derivative);
	Hasher.Update(ShaderFrequency);

	for (const TEnumAsByte<EMaterialDomain>& InDomain : Domains)
	{
		Hasher.Update(InDomain.GetValue());
	}

	for (const FMaterialExternalCodeEnvironmentDefine& InEnvironmentDefine : EnvironmentDefines)
	{
		InEnvironmentDefine.UpdateHash(Hasher);
	}
}


/*
 * UMaterialExternalCodeCollection
 */

void UMaterialExternalCodeCollection::PostInitProperties()
{
	Super::PostInitProperties();

	TStringBuilder<2048> InvalidPropertiesList;

	auto AppendInvalidProperty = [&InvalidPropertiesList](const TCHAR* PropertyDescription) -> void
		{
			if (InvalidPropertiesList.Len() > 0)
			{
				InvalidPropertiesList.Append(TEXT(", "));
			}
			InvalidPropertiesList.Append(PropertyDescription);
		};

	for (const FMaterialExternalCodeDeclaration& ExternalCode : ExternalCodeDeclarations)
	{
		InvalidPropertiesList.Reset();

		// Validate basic properties
		if (ExternalCode.Definition.IsEmpty())
		{
			AppendInvalidProperty(TEXT("Definition is empty"));
		}

		// Validate derivative configuration properties
		if (ExternalCode.Derivative == EDerivativeStatus::Valid)
		{
			if (ExternalCode.DefinitionDDX.IsEmpty())
			{
				AppendInvalidProperty(TEXT("DefinitionDDX is empty"));
			}
			if (ExternalCode.DefinitionDDY.IsEmpty())
			{
				AppendInvalidProperty(TEXT("DefinitionDDY is empty"));
			}
		}

		if (InvalidPropertiesList.Len() > 0)
		{
			UE_LOG(LogMaterial, Error, TEXT("External HLSL code declaration '%s' is invalid: %s"), *ExternalCode.Name.ToString(), *InvalidPropertiesList);
		}
	}
}


/*
 * MaterialExternalCodeRegistry
 */

MaterialExternalCodeRegistry& MaterialExternalCodeRegistry::Get()
{
	static MaterialExternalCodeRegistry Instance;
	return Instance;
}

MaterialExternalCodeRegistry::MaterialExternalCodeRegistry()
{
	BuildMapToExternalDeclarations();
}

void MaterialExternalCodeRegistry::BuildMapToExternalDeclarations()
{
	if (const UMaterialExternalCodeCollection* ExternalCodeCollection = GetDefault<UMaterialExternalCodeCollection>())
	{
		ExternalCodeDeclarationMap.Reserve(ExternalCodeCollection->ExternalCodeDeclarations.Num());

		for (const FMaterialExternalCodeDeclaration& InExternalCode : ExternalCodeCollection->ExternalCodeDeclarations)
		{
			if (ExternalCodeDeclarationMap.Contains(InExternalCode.Name))
			{
				UE_LOG(LogMaterial, Fatal, TEXT("External HLSL code declarations for materials must not be overloaded, but '%s' is defined more than once"), *InExternalCode.Name.ToString());
			}
			ExternalCodeDeclarationMap.Add(InExternalCode.Name) = &InExternalCode;
		}
	}
}

const FMaterialExternalCodeDeclaration* MaterialExternalCodeRegistry::FindExternalCode(const FName& InExternalCodeIdentifier) const
{
	if (const FMaterialExternalCodeDeclaration* const* Entry = ExternalCodeDeclarationMap.Find(InExternalCodeIdentifier))
	{
		return *Entry;
	}
	return nullptr;
}

const FMaterialExposedViewPropertyMeta& MaterialExternalCodeRegistry::GetExternalViewPropertyCode(const EMaterialExposedViewProperty InViewProperty) const
{
	// Compile time struct storing all EMaterialExposedViewProperty's enumerations' HLSL compilation specific meta information
	static const FMaterialExposedViewPropertyMeta ViewPropertyMetaArray[] =
	{
		{MEVP_BufferSize, MCT_Float2, TEXT("View.BufferSizeAndInvSize.xy"), TEXT("View.BufferSizeAndInvSize.zw")},
		{MEVP_FieldOfView, MCT_Float2, TEXT("View.<PREV>FieldOfViewWideAngles"), nullptr},
		{MEVP_TanHalfFieldOfView, MCT_Float2, TEXT("Get<PREV>TanHalfFieldOfView()"), TEXT("Get<PREV>CotanHalfFieldOfView()")},
		{MEVP_ViewSize, MCT_Float2, TEXT("View.ViewSizeAndInvSize.xy"), TEXT("View.ViewSizeAndInvSize.zw")},
		{MEVP_WorldSpaceViewPosition, MCT_LWCVector3, TEXT("Get<PREV>WorldViewOrigin(Parameters)"), nullptr},
		{MEVP_WorldSpaceCameraPosition, MCT_LWCVector3, TEXT("Get<PREV>WorldCameraOrigin(Parameters)"), nullptr},
		{MEVP_ViewportOffset, MCT_Float2, TEXT("View.ViewRectMin.xy"), nullptr},
		{MEVP_TemporalSampleCount, MCT_Float1, TEXT("View.TemporalAAParams.y"), nullptr},
		{MEVP_TemporalSampleIndex, MCT_Float1, TEXT("View.TemporalAAParams.x"), nullptr},
		{MEVP_TemporalSampleOffset, MCT_Float2, TEXT("View.TemporalAAParams.zw"), nullptr},
		{MEVP_RuntimeVirtualTextureOutputLevel, MCT_Float1, TEXT("GetRuntimeVirtualTextureMipLevel().x"), nullptr},
		{MEVP_RuntimeVirtualTextureOutputDerivative, MCT_Float2, TEXT("GetRuntimeVirtualTextureMipLevel().zw"), nullptr},
		{MEVP_PreExposure, MCT_Float1, TEXT("View.PreExposure.x"), TEXT("View.OneOverPreExposure.x")},
		{MEVP_RuntimeVirtualTextureMaxLevel, MCT_Float1, TEXT("GetRuntimeVirtualTextureMipLevel().y"), nullptr},
		{MEVP_ResolutionFraction, MCT_Float1, TEXT("View.ResolutionFractionAndInv.x"), TEXT("View.ResolutionFractionAndInv.y")},
		{MEVP_PostVolumeUserFlags, MCT_Float1, TEXT("View.PostVolumeUserFlags"), nullptr},
		{MEVP_FirstPersonFieldOfView, MCT_Float2, TEXT("View.<PREV>FirstPersonFieldOfViewWideAngles"), nullptr},
		{MEVP_FirstPersonTanHalfFieldOfView, MCT_Float2, TEXT("View.<PREV>FirstPersonTanAndInvTanHalfFOV.xy"), TEXT("View.<PREV>FirstPersonTanAndInvTanHalfFOV.zw")},
		{MEVP_FirstPersonScale, MCT_Float, TEXT("View.<PREV>FirstPersonScale"), nullptr},
		{MEVP_NearPlane, MCT_Float, TEXT("View.NearPlane"), nullptr},
	};
	static_assert((sizeof(ViewPropertyMetaArray) / sizeof(ViewPropertyMetaArray[0])) == MEVP_MAX, "incoherency between EMaterialExposedViewProperty and ViewPropertyMetaArray");

	check(InViewProperty < MEVP_MAX);

	const FMaterialExposedViewPropertyMeta& PropertyMeta = ViewPropertyMetaArray[InViewProperty];
	check(InViewProperty == PropertyMeta.EnumValue);

	return PropertyMeta;
}

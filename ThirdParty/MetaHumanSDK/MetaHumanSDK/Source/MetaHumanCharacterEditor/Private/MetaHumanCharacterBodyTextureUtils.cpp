// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterBodyTextureUtils.h"
#include "Subsystem/MetaHumanCharacterSkinMaterials.h"
#include "MetaHumanCharacterTextureSynthesis.h"
#include "MetaHumanFaceTextureSynthesizer.h"
#include "MetaHumanCharacterEditorModule.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "UObject/Package.h"
#include "TextureCompiler.h"

namespace UE::MetaHuman
{
	static int32 GetMappedBodyTextureId(int32 InBodyTextureParam)
	{
		constexpr TStaticArray<int32, 9> BodyTextureMapping =
		{
			5, 18, 23, 12, 13, 15, 35, 11, 21 
		};

		const int32 MappedBodyTextureId = BodyTextureMapping[InBodyTextureParam];
		return MappedBodyTextureId;	
	}

	static TObjectPtr<UTexture2D> GetBodyTexture(EBodyTextureType InTextureType, int32 InSkinToneIndex, int32 InSurfaceMapId)
	{
		FString TexturePath;
		switch (InTextureType)
		{
		case EBodyTextureType::Body_Basecolor:
			TexturePath = FString::Printf(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Optional/BodyTextures/T_Skin_V%d_Body_BC.T_Skin_V%d_Body_BC'"), InSkinToneIndex, InSkinToneIndex);
			break;
		case EBodyTextureType::Body_Normal:
			TexturePath = FString::Printf(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Optional/BodyTextures/SurfaceDetail/T_Chr%04d_Body_N.T_Chr%04d_Body_N'"), InSurfaceMapId, InSurfaceMapId);
			break;
		case EBodyTextureType::Body_Cavity:
			TexturePath =FString::Printf(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Optional/BodyTextures/SurfaceDetail/T_Chr%04d_Body_Ca.T_Chr%04d_Body_Ca'"), InSurfaceMapId, InSurfaceMapId);
			break;
		case EBodyTextureType::Body_Underwear_Basecolor:
			TexturePath = FString(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Shared/1K/T_Chr0000_Body_Underwear_BC.T_Chr0000_Body_Underwear_BC'"));
			break;
		case EBodyTextureType::Body_Underwear_Normal:
			TexturePath = FString(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Shared/1K/T_Chr0000_Body_Underwear_N.T_Chr0000_Body_Underwear_N'"));
			break;
		case EBodyTextureType::Body_Underwear_Mask:
			TexturePath = FString(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Shared/1K/T_Underwear_M.T_Underwear_M'"));
			break;
		case EBodyTextureType::Chest_Basecolor:
			TexturePath = FString::Printf(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Optional/BodyTextures/T_Skin_V%d_Chest_BC.T_Skin_V%d_Chest_BC'"), InSkinToneIndex, InSkinToneIndex);
			break;
		case EBodyTextureType::Chest_Normal:
			TexturePath = FString::Printf(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Optional/BodyTextures/SurfaceDetail/T_Chr%04d_Chest_N.T_Chr%04d_Chest_N'"), InSurfaceMapId, InSurfaceMapId);
			break;
		case EBodyTextureType::Chest_Cavity:
			TexturePath = FString::Printf(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Optional/BodyTextures/SurfaceDetail/T_Chr%04d_Chest_Ca.T_Chr%04d_Chest_Ca'"), InSurfaceMapId, InSurfaceMapId);
			break;
		case EBodyTextureType::Chest_Underwear_Basecolor:
			TexturePath = FString(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Shared/1K/T_Chr0000_Chest_Underwear_BC.T_Chr0000_Chest_Underwear_BC'"));
			break;
		case EBodyTextureType::Chest_Underwear_Normal:
			TexturePath = FString(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Shared/1K/T_Chr0000_Chest_Underwear_N.T_Chr0000_Chest_Underwear_N'"));
			break;
		default:
			check(false)
		}

		TObjectPtr<UTexture2D> BodyTexture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		check(BodyTexture);
		return BodyTexture;
	}

	static void GetBiasGain(const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, const FVector2f& InSkinUVFromUI, FVector3f& OutBias, FVector3f& OutGain)
	{
		FLinearColor LinearColorSkinTone = InFaceTextureSynthesizer.GetSkinTone(InSkinUVFromUI);
		OutBias[0] = FMath::Pow(LinearColorSkinTone.R, 2.2) * 256;
		OutBias[1] = FMath::Pow(LinearColorSkinTone.G, 2.2) * 256;
		OutBias[2] = FMath::Pow(LinearColorSkinTone.B, 2.2) * 256;

		OutGain = InFaceTextureSynthesizer.GetBodyAlbedoGain(InSkinUVFromUI);
	}

	static void SetBodyTextureProperties(EBodyTextureType InTextureType, TNotNull<UTexture2D*> Texture)
	{
		// Order should match the one in EBodyTextureType
		static constexpr TextureCompressionSettings TextureTypeToCompressionSettings[] =
		{
			TC_Default,			// Body_Basecolor
			TC_Normalmap,		// Body_Normal
			TC_Masks,			// Body_Cavity
			TC_Default,			// Body_Underwear_Basecolor
			TC_Normalmap,		// Body_Underwear_Normal
			TC_Masks,			// Body_Underwear_Mask
			TC_Default,			// Chest_Basecolor
			TC_Normalmap,		// Chest_Normal
			TC_Masks,			// Chest_Cavity
			TC_Default,			// Chest_Underwear_Basecolor
			TC_Normalmap		// Chest_Underwear_Normal
		};

		static constexpr TextureGroup TextureTypeToTextureGroup[] =
		{
			TEXTUREGROUP_Character,				// Body_Basecolor
			TEXTUREGROUP_CharacterNormalMap,	// Body_Normal
			TEXTUREGROUP_CharacterSpecular,		// Body_Cavity
			TEXTUREGROUP_Character,				// Body_Underwear_Basecolor
			TEXTUREGROUP_CharacterNormalMap,	// Body_Underwear_Normal
			TEXTUREGROUP_Character,				// Body_Underwear_Mask
			TEXTUREGROUP_Character,				// Chest_Basecolor
			TEXTUREGROUP_CharacterNormalMap,	// Chest_Normal
			TEXTUREGROUP_CharacterSpecular,		// Chest_Cavity
			TEXTUREGROUP_Character,				// Chest_Underwear_Basecolor
			TEXTUREGROUP_CharacterNormalMap		// Chest_Underwear_Normal
		};

		static_assert(UE_ARRAY_COUNT(TextureTypeToCompressionSettings) == static_cast<int32>(EBodyTextureType::Count));
		static_assert(UE_ARRAY_COUNT(TextureTypeToTextureGroup) == static_cast<int32>(EBodyTextureType::Count));

		// Set its properties
		Texture->CompressionSettings = TextureTypeToCompressionSettings[static_cast<int32>(InTextureType)];
		Texture->AlphaCoverageThresholds.W = 1.0f;

		// Set texture to the "Character" texture group (rather than the default "World")
		Texture->LODGroup = TextureTypeToTextureGroup[static_cast<int32>(InTextureType)];

		if (InTextureType == EBodyTextureType::Body_Underwear_Basecolor || InTextureType == EBodyTextureType::Chest_Underwear_Basecolor)
		{
			Texture->SRGB = true;
		}
		else
		{
			Texture->SRGB = false;
		}
	}
} // namespace UE::MetaHuman

int32 FMetaHumanCharacterBodyTextureUtils::GetSkinToneIndex(const FMetaHumanCharacterSkinProperties& InSkinProperties)
{
	return (InSkinProperties.U < 0.5) ? 1 : 2;
}

int32 FMetaHumanCharacterBodyTextureUtils::GetBodySurfaceMapId(const FMetaHumanCharacterSkinProperties& InSkinProperties)
{
	return UE::MetaHuman::GetMappedBodyTextureId(InSkinProperties.BodyTextureIndex);
}

void FMetaHumanCharacterBodyTextureUtils::InitBodyTextureData(const FMetaHumanCharacterSkinProperties& InSkinProperties, const TMap<EBodyTextureType, FMetaHumanCharacterTextureInfo>& InTextureInfo, TMap<EBodyTextureType, TObjectPtr<UTexture2D>>& OutBodyTextures)
{
	if (OutBodyTextures.IsEmpty())
	{
		const bool bMetaHumanContentAvailable = FMetaHumanCharacterEditorModule::IsOptionalMetaHumanContentInstalled();
		
		// Initialize any textures from local data (high rez textures are loaded separately)
		if (bMetaHumanContentAvailable)
		{
			for (EBodyTextureType TextureType : TEnumRange<EBodyTextureType>())
			{
				 if (!InTextureInfo.Contains(TextureType))
				{
					OutBodyTextures.FindOrAdd(TextureType) = UE::MetaHuman::GetBodyTexture(TextureType, GetSkinToneIndex(InSkinProperties), GetBodySurfaceMapId(InSkinProperties));
				}
			}
		}
	}
}

void FMetaHumanCharacterBodyTextureUtils::UpdateBodyTextureSet(const TOptional<FMetaHumanCharacterSkinSettings>& InCharacterSkinSettings, const FMetaHumanCharacterSkinProperties& InSkinProperties, TMap<EBodyTextureType, FMetaHumanCharacterTextureInfo>& InOutTextureInfo, TMap<EBodyTextureType, TObjectPtr<class UTexture2D>>& InOutBodyTextures)
{
	if (InCharacterSkinSettings.IsSet()) 
	{
		const FMetaHumanCharacterSkinProperties& OldSkinProperties = InCharacterSkinSettings.GetValue().Skin;
		if (OldSkinProperties.BodyTextureIndex != InSkinProperties.BodyTextureIndex)
		{
			static constexpr TStaticArray<EBodyTextureType, 4> TextureIndexDependentBodyTextures = {
				EBodyTextureType::Body_Normal,
				EBodyTextureType::Body_Cavity,
				EBodyTextureType::Chest_Normal,
				EBodyTextureType::Chest_Cavity
			};

			for (EBodyTextureType TextureType : TextureIndexDependentBodyTextures)
			{
				if (InOutTextureInfo.Contains(TextureType))
				{
					InOutTextureInfo.Remove(TextureType);
				}

				InOutBodyTextures[TextureType] = UE::MetaHuman::GetBodyTexture(TextureType, GetSkinToneIndex(InSkinProperties), GetBodySurfaceMapId(InSkinProperties));
			}
		}

		int32 OldSkinToneIndex  = (OldSkinProperties.U < 0.5) ? 1 : 2;
		int32 NewSkinToneIndex  = (InSkinProperties.U < 0.5) ? 1 : 2;
		if (OldSkinToneIndex != NewSkinToneIndex)
		{
			static constexpr TStaticArray<EBodyTextureType, 2>  SkinToneDependentBodyTextures = {
				EBodyTextureType::Body_Basecolor,
				EBodyTextureType::Chest_Basecolor
			};

			for (EBodyTextureType TextureType : SkinToneDependentBodyTextures)
			{
				if (InOutTextureInfo.Contains(TextureType))
				{
					InOutTextureInfo.Remove(TextureType);
				}

				InOutBodyTextures[TextureType] = UE::MetaHuman::GetBodyTexture(TextureType, GetSkinToneIndex(InSkinProperties), GetBodySurfaceMapId(InSkinProperties));
			}
		}
	}
}

void FMetaHumanCharacterBodyTextureUtils::UpdateBodySkinBiasGain(const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, FMetaHumanCharacterSkinProperties& InOutSkinProperties)
{
	if (InFaceTextureSynthesizer.IsValid())
	{
		const FVector2f SkinUVFromUI{ InOutSkinProperties.U, InOutSkinProperties.V };
		UE::MetaHuman::GetBiasGain(InFaceTextureSynthesizer, SkinUVFromUI, InOutSkinProperties.BodyBias, InOutSkinProperties.BodyGain);
	}
}

void FMetaHumanCharacterBodyTextureUtils::GetSkinToneAndUpdateMaterials(const FMetaHumanCharacterSkinProperties& InSkinProperties,
																		const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
																		const TMap<EBodyTextureType, TObjectPtr<UTexture2D>>& InBodyTextures,
																		const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet,
																		TNotNull<UMaterialInstanceDynamic*> InBodyMID)
{
	FVector3f RGBBias;
	FVector3f RGBGain;
	if (InFaceTextureSynthesizer.IsValid())
	{
		const FVector2f SkinUVFromUI{ InSkinProperties.U, InSkinProperties.V };
		UE::MetaHuman::GetBiasGain(InFaceTextureSynthesizer, SkinUVFromUI, RGBBias, RGBGain);
	}
	else
	{
		// If texture synthesis not available use last committed bias and gain from character
		RGBBias = InSkinProperties.BodyBias;
		RGBGain = InSkinProperties.BodyGain;
	}
	
	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[RGBBias, RGBGain](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetScalarParameterValue(TEXT("rbias"), RGBBias[0]);
			Material->SetScalarParameterValue(TEXT("gbias"), RGBBias[1]);
			Material->SetScalarParameterValue(TEXT("bbias"), RGBBias[2]);
			Material->SetScalarParameterValue(TEXT("rgain"), RGBGain[0]);
			Material->SetScalarParameterValue(TEXT("ggain"), RGBGain[1]);
			Material->SetScalarParameterValue(TEXT("bgain"), RGBGain[2]);
		}
	);

	InBodyMID->SetScalarParameterValue(TEXT("rbias"), RGBBias[0]);
	InBodyMID->SetScalarParameterValue(TEXT("gbias"), RGBBias[1]);
	InBodyMID->SetScalarParameterValue(TEXT("bbias"), RGBBias[2]);
	InBodyMID->SetScalarParameterValue(TEXT("rgain"), RGBGain[0]);
	InBodyMID->SetScalarParameterValue(TEXT("ggain"), RGBGain[1]);
	InBodyMID->SetScalarParameterValue(TEXT("bgain"), RGBGain[2]);

	// Set underwear and micro mask params
	float ShowTopUnderwearParamValue = InSkinProperties.bShowTopUnderwear ? 1.0f : 0.0f;
	float MicroMaskStrength = ShowTopUnderwearParamValue > 0.0f ? 1.0f : 0.0f;
	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[ShowTopUnderwearParamValue, MicroMaskStrength](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetScalarParameterValue(TEXT("Show Top Underwear"), ShowTopUnderwearParamValue);
			Material->SetScalarParameterValue(TEXT("Micro MaskC Strength"), MicroMaskStrength);
		}
	);

	InBodyMID->SetScalarParameterValue(TEXT("Show Top Underwear"), ShowTopUnderwearParamValue);
	InBodyMID->SetScalarParameterValue(TEXT("Micro MaskB Strength"), MicroMaskStrength);

	for (const TPair<EBodyTextureType, TObjectPtr<UTexture2D>>& BodyTexturePair : InBodyTextures)
	{
		const EBodyTextureType TextureType = BodyTexturePair.Key;
		const TObjectPtr<UTexture2D> Texture = BodyTexturePair.Value;

		const FName TextureParameterName = FMetaHumanCharacterSkinMaterials::GetBodyTextureParameterName(TextureType);

		// Update body and face materials - underwear mask is set on both 
		if (TextureType <= EBodyTextureType::Body_Underwear_Mask)
		{
			InBodyMID->SetTextureParameterValue(TextureParameterName, Texture);
		}

		if (TextureType >= EBodyTextureType::Body_Underwear_Mask)
		{
			InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>([&TextureParameterName, Texture](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
			{
				Material->SetTextureParameterValue(TextureParameterName, Texture);
			});
		}
	}
}

void FMetaHumanCharacterBodyTextureUtils::SetMaterialHiddenFacesTexture(TNotNull<UMaterialInstanceDynamic*> InBodyMID, const TObjectPtr<class UTexture2D>& InHiddenFacesTexture)
{
	InBodyMID->SetTextureParameterValue(TEXT("HideMaskShirt"), InHiddenFacesTexture);
}

void FMetaHumanCharacterBodyTextureUtils::SetMaterialHiddenFacesTextureNoOp(TNotNull<UMaterialInstanceDynamic*> InBodyMID)
{
	TObjectPtr<UTexture2D> NoOpTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Lookdev_UHM/Common/Textures/Placeholders/T_Flat_White_C.T_Flat_White_C'"));
	InBodyMID->SetTextureParameterValue(TEXT("HideMaskShirt"), NoOpTexture);
}

UTexture2D* FMetaHumanCharacterBodyTextureUtils::CreateBodyTextureFromSource(EBodyTextureType InTextureType, FImageView InTextureImage)
{
	if (InTextureImage.SizeX == 0 || InTextureImage.SizeY == 0)
	{
		return nullptr;
	}

	// Create a sensible unique name for the texture to allow easy identification when debugging
	const FString TextureName = StaticEnum<EBodyTextureType>()->GetAuthoredNameStringByValue((int64)InTextureType);
	const FString CandidateName = FString::Format(TEXT("T_Body_{0}"), { TextureName });
	const FName AssetName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName{ CandidateName }, EUniqueObjectNameOptions::GloballyUnique);

	// Create texture
	UTexture2D* Texture = NewObject<UTexture2D>(
		GetTransientPackage(),
		AssetName,
		RF_Transient
	);

	if (Texture)
	{
		Texture->PreEditChange(nullptr);

		Texture->Source.Init(InTextureImage);
		
		UE::MetaHuman::SetBodyTextureProperties(InTextureType, Texture);
		// TODO: Revisit mips generation, storing mips creates issues with textures in the MHC asset editor
		Texture->MipGenSettings = TMGS_NoMipmaps;

		Texture->UpdateResource();
		Texture->PostEditChange();
		FTextureCompilingManager::Get().FinishCompilation({ Texture });
	}

	return Texture;
}

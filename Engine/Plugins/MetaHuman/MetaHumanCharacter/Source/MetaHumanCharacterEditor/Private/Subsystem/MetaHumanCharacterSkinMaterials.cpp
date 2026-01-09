// Copyright Epic Games, Inc. All Rights Reserved.
#include "Subsystem/MetaHumanCharacterSkinMaterials.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorLog.h"

#include "Containers/Array.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Package.h"
#include "Logging/StructuredLog.h"
#include "Misc/CoreMiscDefines.h"


#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

namespace UE::MetaHuman
{
	static bool TrySetMaterialByName(TArray<FSkeletalMaterial>& OutMaterialSlots, FName InSlotName, UMaterialInterface* InMaterial)
	{
		FSkeletalMaterial* MaterialSlot = Algo::FindBy(OutMaterialSlots, InSlotName, &FSkeletalMaterial::MaterialSlotName);
		if (MaterialSlot)
		{
			MaterialSlot->MaterialInterface = InMaterial;
		}

		return MaterialSlot != nullptr;
	}

	static UMaterialInstanceDynamic* CreateMaterialInstance(TNotNull<UMaterialInterface*> InBaseMaterial, EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterialType)
	{
		// Give the new material a unique name based on the material type for easy debugging
		const FString MaterialTypeName = StaticEnum<EMetaHumanCharacterSkinPreviewMaterial>()->GetAuthoredNameStringByValue(static_cast<int64>(InPreviewMaterialType));
		const FString BaseNewMaterialName = FString::Format(TEXT("MID_{0}_{1}"), { InBaseMaterial->GetName(), MaterialTypeName });
		const FName NewMaterialName = MakeUniqueObjectName(GetTransientPackage(), UMaterialInstanceDynamic::StaticClass(), FName{ BaseNewMaterialName });

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(InBaseMaterial, GetTransientPackage(), NewMaterialName);
		check(MID);

		return MID;
	}

	/**
	 * Copy of UMetaHumanUtils::ShiftFoundationColor from MetaHumanTool
	 */
	static FLinearColor ShiftFoundationColor(const FLinearColor& InColor, int32 InColorIndex, int32 InShowColumns, int32 InShowRows, float InSaturationShift, float InValueShift)
	{
		float ValueIncrement = 2.0f - (float) (InColorIndex % InShowColumns);
		float SaturationIncrement = 1.0f - FMath::Floor((float) InColorIndex / (float) InShowColumns);

		FLinearColor HSVcolor = InColor.LinearRGBToHSV();
		HSVcolor.G += InSaturationShift * SaturationIncrement;
		HSVcolor.B *= 1.0f + InValueShift * ValueIncrement;

		return HSVcolor.HSVToLinearRGB().GetClamped();
	}

	static UTexture2D* GetEmptyMask()
	{
		return LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Lookdev_UHM/Common/Textures/Placeholders/T_Flat_Black_M.T_Flat_Black_M'"));
	}

	static UMaterialInterface* GetEmptyMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/M_Hide.M_Hide'"));

	}
}

const FName FMetaHumanCharacterSkinMaterials::EyeLeftSlotName = TEXT("eyeLeft_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::EyeRightSlotName = TEXT("eyeRight_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::SalivaSlotName = TEXT("saliva_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::EyeShellSlotName = TEXT("eyeshell_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::EyeEdgeSlotName = TEXT("eyeEdge_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::TeethSlotName = TEXT("teeth_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::EyelashesSlotName = TEXT("eyelashes_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::EyelashesHiLodSlotName = TEXT("eyelashes_HiLOD_shader_shader");
const FName FMetaHumanCharacterSkinMaterials::UseCavityParamName = TEXT("Use Cavity");
const FName FMetaHumanCharacterSkinMaterials::UseAnimatedMapsParamName = TEXT("Use Animated Maps");
const FName FMetaHumanCharacterSkinMaterials::UseTextureOverrideParamName = TEXT("Use Texture Override");
const FName FMetaHumanCharacterSkinMaterials::RoughnessUIMultiplyParamName = TEXT("Roughness UI Multiply");

FName FMetaHumanCharacterSkinMaterials::GetSkinMaterialSlotName(EMetaHumanCharacterSkinMaterialSlot InSlot)
{
	const TStaticArray<FName, static_cast<int32>(EMetaHumanCharacterSkinMaterialSlot::Count)> SlotToName =
	{
		TEXT("head_shader_shader"),
		TEXT("head_LOD1_shader_shader"),
		TEXT("head_LOD2_shader_shader"),
		TEXT("head_LOD3_shader_shader"),
		TEXT("head_LOD4_shader_shader"),
		TEXT("head_LOD57_shader_shader"),
	};

	return SlotToName[static_cast<int32>(InSlot)];
}

FName FMetaHumanCharacterSkinMaterials::GetFaceTextureParameterName(EFaceTextureType InTextureType)
{
	static const TMap<EFaceTextureType, FName> TextureParamNameMap =
	{
		{ EFaceTextureType::Basecolor, TEXT("Basecolor") },
		{ EFaceTextureType::Basecolor_Animated_CM1, TEXT("Basecolor Animated Delta cm1") },
		{ EFaceTextureType::Basecolor_Animated_CM2, TEXT("Basecolor Animated Delta cm2") },
		{ EFaceTextureType::Basecolor_Animated_CM3, TEXT("Basecolor Animated Delta cm3") },
		{ EFaceTextureType::Normal, TEXT("Normal") },
		{ EFaceTextureType::Normal_Animated_WM1, TEXT("Normal Animated Delta wm1") },
		{ EFaceTextureType::Normal_Animated_WM2, TEXT("Normal Animated Delta wm2") },
		{ EFaceTextureType::Normal_Animated_WM3, TEXT("Normal Animated Delta wm3") },
		{ EFaceTextureType::Cavity, TEXT("Cavity") },
	};

	return TextureParamNameMap[InTextureType];
}

FName FMetaHumanCharacterSkinMaterials::GetBodyTextureParameterName(EBodyTextureType InTextureType)
{
	static const TMap<EBodyTextureType, FName> TextureParamNameMap =
	{
		{ EBodyTextureType::Body_Basecolor, TEXT("Basecolor") },
		{ EBodyTextureType::Body_Normal, TEXT("Normal") },
		{ EBodyTextureType::Body_Cavity, TEXT("Cavity") },
		{ EBodyTextureType::Body_Underwear_Basecolor, TEXT("Underwear_Body_BaseColor") },
		{ EBodyTextureType::Body_Underwear_Normal, TEXT("Underwear_Body_Normal") },
		{ EBodyTextureType::Body_Underwear_Mask, TEXT("Underwear_Mask") },
		{ EBodyTextureType::Chest_Basecolor, TEXT("Color_CHEST") },
		{ EBodyTextureType::Chest_Normal, TEXT("Normal_CHEST") },
		{ EBodyTextureType::Chest_Cavity, TEXT("Cavity_Chest") },
		{ EBodyTextureType::Chest_Underwear_Basecolor, TEXT("Underwear_Chest_BaseColor") },
		{ EBodyTextureType::Chest_Underwear_Normal, TEXT("Underwear_Chest_Normal") },
	};
	return TextureParamNameMap[InTextureType];
}

void FMetaHumanCharacterSkinMaterials::SetHeadMaterialsOnMesh(const FMetaHumanCharacterFaceMaterialSet& InMaterialSet, TNotNull<class USkeletalMesh*> InMesh)
{
	TArray<FSkeletalMaterial> MaterialSlots = InMesh->GetMaterials();

	InMaterialSet.ForEachSkinMaterial<UMaterialInstance>(
		[&MaterialSlots](EMetaHumanCharacterSkinMaterialSlot Slot, UMaterialInstance* Material)
		{
			UE::MetaHuman::TrySetMaterialByName(MaterialSlots, GetSkinMaterialSlotName(Slot), Material);
		}
	);

	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, EyeLeftSlotName, InMaterialSet.EyeLeft);
	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, EyeRightSlotName, InMaterialSet.EyeRight);
	
	// Material is still not ready so we just assign the EmptyOne
	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, SalivaSlotName, UE::MetaHuman::GetEmptyMaterial());
	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, EyeEdgeSlotName, InMaterialSet.LacrimalFluid);
	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, EyeShellSlotName, InMaterialSet.EyeShell);

	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, TeethSlotName, InMaterialSet.Teeth);

	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, EyelashesSlotName, InMaterialSet.Eyelashes);
	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, EyelashesHiLodSlotName, InMaterialSet.EyelashesHiLods);

	InMesh->SetMaterials(MaterialSlots);
}

void FMetaHumanCharacterSkinMaterials::SetBodyMaterialOnMesh(TNotNull<class UMaterialInterface*> InBodyMaterial, TNotNull<class USkeletalMesh*> InMesh)
{
	TArray<FSkeletalMaterial> MaterialSlots = InMesh->GetMaterials();
	
	UE::MetaHuman::TrySetMaterialByName(MaterialSlots, TEXT("body_shader_shader"), InBodyMaterial);

	InMesh->SetMaterials(MaterialSlots);
}

FMetaHumanCharacterFaceMaterialSet FMetaHumanCharacterSkinMaterials::GetHeadMaterialsFromMesh(TNotNull<class USkeletalMesh*> InFaceMesh)
{
	auto GetMaterialByName = [InFaceMesh](FName SlotName) -> UMaterialInstance*
	{
		if (const FSkeletalMaterial* FoundMaterial = Algo::FindBy(InFaceMesh->GetMaterials(), SlotName, &FSkeletalMaterial::MaterialSlotName))
		{
			return Cast<UMaterialInstance>(FoundMaterial->MaterialInterface);
		}

		return nullptr;
	};

	FMetaHumanCharacterFaceMaterialSet FaceMaterialSet =
	{
		.EyeLeft = GetMaterialByName(EyeLeftSlotName),
		.EyeRight = GetMaterialByName(EyeRightSlotName),
		.EyeShell = GetMaterialByName(EyeShellSlotName),
		.LacrimalFluid = GetMaterialByName(SalivaSlotName),
		.Teeth = GetMaterialByName(TeethSlotName),
		.Eyelashes = GetMaterialByName(EyelashesSlotName),
		.EyelashesHiLods = GetMaterialByName(EyelashesHiLodSlotName),
	};

	for (EMetaHumanCharacterSkinMaterialSlot SkinMaterialSlot : TEnumRange<EMetaHumanCharacterSkinMaterialSlot>())
	{
		const FName SlotName = GetSkinMaterialSlotName(SkinMaterialSlot);
		if (UMaterialInstance* SkinMaterial = GetMaterialByName(SlotName))
		{
			FaceMaterialSet.Skin.Emplace(SkinMaterialSlot, SkinMaterial);
		}
	}

	return FaceMaterialSet;
}

void FMetaHumanCharacterSkinMaterials::ApplySkinParametersToMaterials(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, TNotNull<UMaterialInstanceDynamic*> InBodyMID, const FMetaHumanCharacterSkinSettings& InSkinSettings)
{
	ApplySkinAccentsToMaterial(InFaceMaterialSet, InSkinSettings.Accents);
	ApplyFrecklesToMaterial(InFaceMaterialSet, InSkinSettings.Freckles);
	ApplyRoughnessMultiplyToMaterials(InFaceMaterialSet, InBodyMID, InSkinSettings);
	ApplyTextureOverrideParameterToMaterials(InFaceMaterialSet, InBodyMID, InSkinSettings);
}

void FMetaHumanCharacterSkinMaterials::ApplyRoughnessMultiplyToMaterials(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, TNotNull<UMaterialInstanceDynamic*> InBodyMaterial, const FMetaHumanCharacterSkinSettings& InSkinSettings)
{
	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[&InSkinSettings](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* SkinMaterial)
		{
			SkinMaterial->SetScalarParameterValue(RoughnessUIMultiplyParamName, InSkinSettings.Skin.Roughness);
		}
	);

	InBodyMaterial->SetScalarParameterValue(RoughnessUIMultiplyParamName, InSkinSettings.Skin.Roughness);
}

void FMetaHumanCharacterSkinMaterials::ApplyTextureOverrideParameterToMaterials(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, TNotNull<UMaterialInstanceDynamic*> InBodyMaterial, const FMetaHumanCharacterSkinSettings& InSkinSettings)
{
	bool bEnableOverrides = false;

	if (InSkinSettings.bEnableTextureOverrides)
	{
		if (const TSoftObjectPtr<UTexture2D>* FoundBodyBaseColorOverride = InSkinSettings.TextureOverrides.Body.Find(EBodyTextureType::Body_Basecolor))
		{
			// Only enable overrides if overriding the Body Base color
			TSoftObjectPtr<UTexture2D> BodyBaseColorOverride = *FoundBodyBaseColorOverride;
			bEnableOverrides = !BodyBaseColorOverride.IsNull();
		}	
	}

	const float EnableTextureOverridesParam = bEnableOverrides ? 1.0f : 0.0f;

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[EnableTextureOverridesParam](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetScalarParameterValue(UseTextureOverrideParamName, EnableTextureOverridesParam);
		}
	);
	
	InBodyMaterial->SetScalarParameterValue(UseTextureOverrideParamName, EnableTextureOverridesParam);

	UDataTable* OverridesDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Script/Engine.DataTable'/" UE_PLUGIN_NAME "/Materials/DT_SkinMaterialParameterOverrides.DT_SkinMaterialParameterOverrides'"));
	check(OverridesDataTable);

	const FString ContextString = TEXT("FMetaHumanCharacterSkinMaterials::ApplyTextureOverrideParameterToMaterials");

	// List of all parameter names to reset the values of the skin materials
	static TSet<FName> AllParameterNames;

	if (AllParameterNames.IsEmpty())
	{
		TArray<FMetaHumanCharacterSkinMaterialOverrideRow*> Rows;
		OverridesDataTable->GetAllRows(ContextString, Rows);

		for (const FMetaHumanCharacterSkinMaterialOverrideRow* Row : Rows)
		{
			for (const TPair<FName, float>& ParameterPair : Row->ScalarParameterValues)
			{
				AllParameterNames.Add(ParameterPair.Key);
			}
		}
	}

	auto ResetParameterValues = [](UMaterialInstanceDynamic* SkinMaterial)
	{
		for (FName ParameterName : AllParameterNames)
		{
			float Value = 0.0f;
			if (SkinMaterial->Parent->GetScalarParameterValue(ParameterName, Value))
			{
				SkinMaterial->SetScalarParameterValue(ParameterName, Value);
			}
		}
	};

	// Restore the material parameter values from the defaults defined in the parents
	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[&ResetParameterValues](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* SkinMaterial)
		{
			ResetParameterValues(SkinMaterial);
		}
	);

	ResetParameterValues(InBodyMaterial);

	// Apply the overrides if found in the table
	const FName FaceTextureId{ FString::FromInt(InSkinSettings.Skin.FaceTextureIndex) };
	const bool bWarnIfMissing = false;
	if (const FMetaHumanCharacterSkinMaterialOverrideRow* Overrides = OverridesDataTable->FindRow<FMetaHumanCharacterSkinMaterialOverrideRow>(FaceTextureId, ContextString, bWarnIfMissing))
	{
		auto ApplyScalarParameterOverrides = [](UMaterialInstanceDynamic* SkinMaterial, const TMap<FName, float>& ScalarOverrides)
		{
			for (const TPair<FName, float>& ParameterPair : ScalarOverrides)
			{
				SkinMaterial->SetScalarParameterValue(ParameterPair.Key, ParameterPair.Value);
			}
		};

		InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
			[Overrides, &ApplyScalarParameterOverrides](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* SkinMaterial)
			{
				ApplyScalarParameterOverrides(SkinMaterial, Overrides->ScalarParameterValues);
			}
		);

		ApplyScalarParameterOverrides(InBodyMaterial, Overrides->ScalarParameterValues);
	}
}

void FMetaHumanCharacterSkinMaterials::ApplySkinAccentParameterToMaterial(
	const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet,
	EMetaHumanCharacterAccentRegion InRegion,
	EMetaHumanCharacterAccentRegionParameter InParameter,
	float InValue)
{
	const FString RegionName = StaticEnum<EMetaHumanCharacterAccentRegion>()->GetAuthoredNameStringByValue(static_cast<int64>(InRegion));
	const FString ParamName = StaticEnum<EMetaHumanCharacterAccentRegionParameter>()->GetAuthoredNameStringByValue(static_cast<int64_t>(InParameter));

	const FString MaterialParameterName = FString::Format(TEXT("SA_{0}{1}"), { RegionName, ParamName });

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[&MaterialParameterName, InValue](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetScalarParameterValue(*MaterialParameterName, InValue);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplySkinAccentsToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterAccentRegions& InAccents)
{
	// Update the Accent Region Parameters
	for (EMetaHumanCharacterAccentRegion AccentRegion : TEnumRange<EMetaHumanCharacterAccentRegion>())
	{
		const FString AccentRegionName = StaticEnum<EMetaHumanCharacterAccentRegion>()->GetAuthoredNameStringByValue(static_cast<int64>(AccentRegion));
		if (const FStructProperty* AccentRegionProperty = FindFProperty<FStructProperty>(FMetaHumanCharacterAccentRegions::StaticStruct(), *AccentRegionName))
		{
			if (const FMetaHumanCharacterAccentRegionProperties* AccentRegionValues = AccentRegionProperty->ContainerPtrToValuePtr<FMetaHumanCharacterAccentRegionProperties>(&InAccents))
			{
				ApplySkinAccentParameterToMaterial(InFaceMaterialSet, AccentRegion, EMetaHumanCharacterAccentRegionParameter::Lightness, AccentRegionValues->Lightness);
				ApplySkinAccentParameterToMaterial(InFaceMaterialSet, AccentRegion, EMetaHumanCharacterAccentRegionParameter::Redness, AccentRegionValues->Redness);
				ApplySkinAccentParameterToMaterial(InFaceMaterialSet, AccentRegion, EMetaHumanCharacterAccentRegionParameter::Saturation, AccentRegionValues->Saturation);
			}
		}
	}
}

void FMetaHumanCharacterSkinMaterials::ApplyFrecklesMaskToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, EMetaHumanCharacterFrecklesMask InMask)
{
	UTexture* FrecklesTexture = nullptr;

	switch (InMask)
	{
		case EMetaHumanCharacterFrecklesMask::Type1:
			FrecklesTexture = LoadObject<UTexture>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/ArtistDelights/Freckles/T_Freckles_001.T_Freckles_001'"));
			break;

		case EMetaHumanCharacterFrecklesMask::Type2:
			FrecklesTexture = LoadObject<UTexture>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/ArtistDelights/Freckles/T_Freckles_002.T_Freckles_002'"));
			break;

		case EMetaHumanCharacterFrecklesMask::Type3:
			FrecklesTexture = LoadObject<UTexture>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/ArtistDelights/Freckles/T_Freckles_003.T_Freckles_003'"));
			break;

		case EMetaHumanCharacterFrecklesMask::None:
		default:
			break;
	}

	const float FrecklesParam = (FrecklesTexture != nullptr) ? 1.0f : 0.0f;

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[FrecklesParam, FrecklesTexture](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetScalarParameterValue(TEXT("Freckles"), FrecklesParam);
			Material->SetTextureParameterValue(TEXT("FrecklesMask"), FrecklesTexture);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplyFrecklesParameterToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, EMetaHumanCharacterFrecklesParameter InFrecklesParam, float InValue)
{
	const FString ParamName = StaticEnum<EMetaHumanCharacterFrecklesParameter>()->GetAuthoredNameStringByValue(static_cast<int64>(InFrecklesParam));
	const FString MaterialParameterName = FString::Format(TEXT("Freckles{0}"), { ParamName });

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[&MaterialParameterName, InValue](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetScalarParameterValue(FName{ MaterialParameterName }, InValue);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplyFrecklesToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterFrecklesProperties& InFrecklesProperties)
{
	FMetaHumanCharacterSkinMaterials::ApplyFrecklesMaskToMaterial(InFaceMaterialSet, InFrecklesProperties.Mask);

	for (EMetaHumanCharacterFrecklesParameter FrecklesParam : TEnumRange<EMetaHumanCharacterFrecklesParameter>())
	{
		const FString FrecklesParamName = StaticEnum<EMetaHumanCharacterFrecklesParameter>()->GetAuthoredNameStringByValue(static_cast<int64>(FrecklesParam));
		if (const FFloatProperty* FrecklesParamProperty = FindFProperty<FFloatProperty>(FMetaHumanCharacterFrecklesProperties::StaticStruct(), *FrecklesParamName))
		{
			const float& ParamValue = FrecklesParamProperty->GetPropertyValue_InContainer(&InFrecklesProperties);
			ApplyFrecklesParameterToMaterial(InFaceMaterialSet, FrecklesParam, ParamValue);
		}
	}
}

void FMetaHumanCharacterSkinMaterials::ApplyFoundationMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterFoundationMakeupProperties& InFoundationMakeupProperties)
{
	UTexture* FoundationMask = UE::MetaHuman::GetEmptyMask();

	if (InFoundationMakeupProperties.bApplyFoundation)
	{
		FoundationMask = LoadObject<UTexture>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/ArtistDelights/Foundation/T_FoundationConcealer_001.T_FoundationConcealer_001'"));
	}

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[FoundationMask, &InFoundationMakeupProperties](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetTextureParameterValue(TEXT("Makeup FoundationConcealer Mask"), FoundationMask);
			Material->SetVectorParameterValue(TEXT("Makeup Foundation Color"), InFoundationMakeupProperties.Color);
			Material->SetScalarParameterValue(TEXT("Makeup Foundation Roughness"), InFoundationMakeupProperties.Roughness);
			Material->SetScalarParameterValue(TEXT("Makeup Foundation Opacity"), InFoundationMakeupProperties.Intensity);
			Material->SetVectorParameterValue(TEXT("Makeup Concealer Color"), UE::MetaHuman::ShiftFoundationColor(InFoundationMakeupProperties.Color, 0, 1, 1, 1.0f, 1.0f));
			Material->SetScalarParameterValue(TEXT("Makeup Concealer Opacity"), InFoundationMakeupProperties.Concealer);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplyEyeMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterEyeMakeupProperties& InEyeMakeupProperties)
{
	UTexture* EyeMaskTexture = UE::MetaHuman::GetEmptyMask();

	if (InEyeMakeupProperties.Type != EMetaHumanCharacterEyeMakeupType::None)
	{
		const FString EyeMaskTypeName = StaticEnum<EMetaHumanCharacterEyeMakeupType>()->GetAuthoredNameStringByValue(static_cast<int64>(InEyeMakeupProperties.Type));
		const FString EyeMaskTextureName = FString::Format(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/ArtistDelights/EyeMakeup/T_EyeMakeup_{0}.T_EyeMakeup_{0}'"), { EyeMaskTypeName });

		EyeMaskTexture = LoadObject<UTexture>(nullptr, *EyeMaskTextureName);
		check(EyeMaskTexture);
	}

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[EyeMaskTexture, &InEyeMakeupProperties](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetTextureParameterValue(TEXT("Makeup EyeMask"), EyeMaskTexture);
			Material->SetVectorParameterValue(TEXT("Makeup Eye Primary Color"), InEyeMakeupProperties.PrimaryColor);
			Material->SetVectorParameterValue(TEXT("Makeup Eye Secondary Color"), InEyeMakeupProperties.SecondaryColor);
			Material->SetScalarParameterValue(TEXT("Makeup Eye Primary Roughness"), InEyeMakeupProperties.Roughness);
			Material->SetScalarParameterValue(TEXT("Makeup Eye Secondary Roughness"), InEyeMakeupProperties.Roughness);
			Material->SetScalarParameterValue(TEXT("Makeup Eye Primary Opacity"), InEyeMakeupProperties.Opacity);
			Material->SetScalarParameterValue(TEXT("Makeup Eye Secondary Opacity"), InEyeMakeupProperties.Opacity);
			Material->SetScalarParameterValue(TEXT("Makeup Eye Primary Metallic"), InEyeMakeupProperties.Metalness);
			Material->SetScalarParameterValue(TEXT("Makeup Eye Secondary Metallic"), InEyeMakeupProperties.Metalness);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplyBlushMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterBlushMakeupProperties& InBlushMakeupProperties)
{
	UTexture* BlushMaskTexture = UE::MetaHuman::GetEmptyMask();

	if (InBlushMakeupProperties.Type != EMetaHumanCharacterBlushMakeupType::None)
	{
		const FString BlushMaskTypeName = StaticEnum<EMetaHumanCharacterBlushMakeupType>()->GetAuthoredNameStringByValue(static_cast<int64>(InBlushMakeupProperties.Type));
		const FString BlushMaskTexureName = FString::Format(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/ArtistDelights/BlushMakeup/T_BlushMakeup_{0}.T_BlushMakeup_{0}'"), { BlushMaskTypeName });

		BlushMaskTexture = LoadObject<UTexture>(nullptr, *BlushMaskTexureName);
		check(BlushMaskTexture);
	}

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[BlushMaskTexture, &InBlushMakeupProperties](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetTextureParameterValue(TEXT("Makeup Blusher Mask"), BlushMaskTexture);
			Material->SetVectorParameterValue(TEXT("Makeup Blusher Color"), InBlushMakeupProperties.Color);
			Material->SetScalarParameterValue(TEXT("Makeup Blusher Opacity"), InBlushMakeupProperties.Intensity);
			Material->SetScalarParameterValue(TEXT("Makeup Blusher Roughness"), InBlushMakeupProperties.Roughness);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplyLipsMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterLipsMakeupProperties& InLipsMakeupProperties)
{
	UTexture* LipsMaskTexture = UE::MetaHuman::GetEmptyMask();

	if (InLipsMakeupProperties.Type != EMetaHumanCharacterLipsMakeupType::None)
	{
		const FString LipsMaskTypeName = StaticEnum<EMetaHumanCharacterLipsMakeupType>()->GetAuthoredNameStringByValue(static_cast<int64>(InLipsMakeupProperties.Type));
		const FString LipsMaskTextureName = FString::Format(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/ArtistDelights/LipsMakeup/T_LipsMakeup_{0}.T_LipsMakeup_{0}'"), { LipsMaskTypeName } );

		LipsMaskTexture = LoadObject<UTexture>(nullptr, *LipsMaskTextureName);
		check(LipsMaskTexture);
	}

	InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[LipsMaskTexture, InLipsMakeupProperties](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetTextureParameterValue(TEXT("Makeup Lipstick Mask"), LipsMaskTexture);
			Material->SetVectorParameterValue(TEXT("Makeup Lipstick Color"), InLipsMakeupProperties.Color);
			Material->SetScalarParameterValue(TEXT("Makeup Lipstick Opacity"), InLipsMakeupProperties.Opacity);
			Material->SetScalarParameterValue(TEXT("Makeup Lipstick Roughness"), InLipsMakeupProperties.Roughness);
			Material->SetScalarParameterValue(TEXT("Makeup Lipstick Metallic"), InLipsMakeupProperties.Metalness);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplySynthesizedTexturesToFaceMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& InSynthesizedFaceTextures)
{
	for (const TPair<EFaceTextureType, TObjectPtr<UTexture2D>>& FaceTexturePair : InSynthesizedFaceTextures)
	{
		EFaceTextureType TextureType = FaceTexturePair.Key;
		UTexture2D* Texture = FaceTexturePair.Value;

		InFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
			[TextureType, Texture](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
			{
				const FName ParameterName = GetFaceTextureParameterName(TextureType);
				Material->SetTextureParameterValue(ParameterName, Texture);
			}
		);
	}
}

void FMetaHumanCharacterSkinMaterials::ApplyEyeSettingsToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterEyesSettings& InEyeSettings)
{
	auto ApplyEyeIrisPropertiesToMaterial = [](UMaterialInstanceDynamic* EyeMaterial, const FMetaHumanCharacterEyeIrisProperties& IrisProperties)
	{
		const TCHAR IrisType = TEXT('A') + static_cast<uint8>(IrisProperties.IrisPattern);

		const FString IrisMaskTextureName = FString::Format(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Lookdev_UHM/Eye/Textures/Iris/T_Iris_{0}_M.T_Iris_{0}_M'"), { FString{}.AppendChar(IrisType) });
		const FString IrisNormaTextureName = FString::Format(TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Lookdev_UHM/Eye/Textures/Iris/T_Iris_{0}_N.T_Iris_{0}_N'"), { FString{}.AppendChar(IrisType) });

		UTexture* IrisMaskTexture = LoadObject<UTexture>(nullptr, *IrisMaskTextureName);
		UTexture* IrisNormalTexture = LoadObject<UTexture>(nullptr, *IrisNormaTextureName);

		check(IrisMaskTexture);
		check(IrisNormalTexture);

		EyeMaterial->SetTextureParameterValue(TEXT("Iris Pattern Masks"), IrisMaskTexture);
		EyeMaterial->SetTextureParameterValue(TEXT("Iris Normal"), IrisNormalTexture);

		EyeMaterial->SetScalarParameterValue(TEXT("Iris Rotation"), IrisProperties.IrisRotation);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Primary Color Hue"), IrisProperties.PrimaryColorU);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Primary Color Value"), IrisProperties.PrimaryColorV);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Secondary Color Hue"), IrisProperties.SecondaryColorU);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Secondary Color Value"), IrisProperties.SecondaryColorV);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Color Blend Coverage"), IrisProperties.ColorBlend);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Color Blend Coverage Softness"), IrisProperties.ColorBlendSoftness);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Color Blend Switch"), static_cast<float>(IrisProperties.BlendMethod));
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Shadow Details Amount"), IrisProperties.ShadowDetails);
		EyeMaterial->SetScalarParameterValue(TEXT("Limbal Ring Size"), IrisProperties.LimbalRingSize);
		EyeMaterial->SetScalarParameterValue(TEXT("Limbal Ring Softness"), IrisProperties.LimbalRingSoftness);
		EyeMaterial->SetVectorParameterValue(TEXT("Limbal Ring Color (Mult)"), IrisProperties.LimbalRingColor);
		EyeMaterial->SetScalarParameterValue(TEXT("Iris Global Saturation"), IrisProperties.GlobalSaturation);
		EyeMaterial->SetVectorParameterValue(TEXT("Iris Color Multiply"), IrisProperties.GlobalTint);
	};	

	auto ApplyEyePupilPropertiesToMaterial = [](UMaterialInstanceDynamic* EyeMaterial, const FMetaHumanCharacterEyePupilProperties& PupilProperties)
	{
		EyeMaterial->SetScalarParameterValue(TEXT("Pupil Dilation"), PupilProperties.Dilation);
		EyeMaterial->SetScalarParameterValue(TEXT("Pupil Feather Strength"), PupilProperties.Feather);
	};

	auto ApplyEyeCorneaPropertiesToMaterial = [](UMaterialInstanceDynamic* EyeMaterial, const FMetaHumanCharacterEyeCorneaProperties& CorneaProperties)
	{
		EyeMaterial->SetScalarParameterValue(TEXT("Cornea Size"), CorneaProperties.Size);
		EyeMaterial->SetScalarParameterValue(TEXT("Corneal Limbus Softness"), CorneaProperties.LimbusSoftness);
		EyeMaterial->SetVectorParameterValue(TEXT("Corneal Limbus Color (Mult)"), CorneaProperties.LimbusColor);
	};

	auto ApplyEyeScleraPropertiesToMaterial = [](UMaterialInstanceDynamic* EyeMaterial, const FMetaHumanCharacterEyeScleraProperties& ScleraProperties)
	{
		EyeMaterial->SetScalarParameterValue(TEXT("Sclera Rotation"), ScleraProperties.Rotation);
		EyeMaterial->SetVectorParameterValue(TEXT("Sclera Color Multiply"), ScleraProperties.Tint);
		EyeMaterial->SetScalarParameterValue(TEXT("Sclera Transmission Spread"), ScleraProperties.TransmissionSpread);
		EyeMaterial->SetVectorParameterValue(TEXT("Sclera Transmission Color (Mult)"), ScleraProperties.TransmissionColor);
		EyeMaterial->SetScalarParameterValue(TEXT("Sclera Irritation Veins Opacity"), ScleraProperties.VascularityIntensity);
		EyeMaterial->SetScalarParameterValue(TEXT("Sclera Irritation Area Size"), ScleraProperties.VascularityCoverage);
	};

	UMaterialInstanceDynamic* LeftEyeMID = CastChecked<UMaterialInstanceDynamic>(InFaceMaterialSet.EyeLeft);
	UMaterialInstanceDynamic* RightEyeMID = CastChecked<UMaterialInstanceDynamic>(InFaceMaterialSet.EyeRight);

	ApplyEyeIrisPropertiesToMaterial(LeftEyeMID, InEyeSettings.EyeLeft.Iris);
	ApplyEyeIrisPropertiesToMaterial(RightEyeMID, InEyeSettings.EyeRight.Iris);

	ApplyEyePupilPropertiesToMaterial(LeftEyeMID, InEyeSettings.EyeLeft.Pupil);
	ApplyEyePupilPropertiesToMaterial(RightEyeMID, InEyeSettings.EyeRight.Pupil);

	ApplyEyeCorneaPropertiesToMaterial(LeftEyeMID, InEyeSettings.EyeLeft.Cornea);
	ApplyEyeCorneaPropertiesToMaterial(RightEyeMID, InEyeSettings.EyeRight.Cornea);

	ApplyEyeScleraPropertiesToMaterial(LeftEyeMID, InEyeSettings.EyeLeft.Sclera);
	ApplyEyeScleraPropertiesToMaterial(RightEyeMID, InEyeSettings.EyeRight.Sclera);
}

void FMetaHumanCharacterSkinMaterials::ApplyEyeScleraTintBasedOnSkinTone(const FMetaHumanCharacterSkinSettings& InSkinSettings, FMetaHumanCharacterEyesSettings& InOutEyeSettings)
{
	auto MapSkinToneToScleraTint = [&InSkinSettings](FMetaHumanCharacterEyeScleraProperties& Sclera)
	{
		if (!Sclera.bUseCustomTint)
		{
			Sclera.Tint = FMath::Lerp(FLinearColor::White, FLinearColor{ 0.77f, 0.71f, 0.68f }, InSkinSettings.Skin.U);
		}
	};

	MapSkinToneToScleraTint(InOutEyeSettings.EyeLeft.Sclera);
	MapSkinToneToScleraTint(InOutEyeSettings.EyeRight.Sclera);
}

void FMetaHumanCharacterSkinMaterials::GetDefaultEyeSettings(FMetaHumanCharacterEyesSettings& OutEyeSettings)
{
	// TODO: find a way to not have this hard coded values

	UMaterialInterface* LeftEyeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Eye_Left_MHC.MI_Eye_Left_MHC'"));
	check(LeftEyeMaterial);

	UMaterialInterface* RightEyeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Eye_Right_MHC.MI_Eye_Right_MHC'"));
	check(RightEyeMaterial);

	auto GetEyePropertiesFromEyeMaterial = [](UMaterialInterface* EyeMaterial, FMetaHumanCharacterEyeProperties& OutEyeProperties)
	{
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Rotation"), OutEyeProperties.Iris.IrisRotation));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Primary Color Hue"), OutEyeProperties.Iris.PrimaryColorU));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Primary Color Value"), OutEyeProperties.Iris.PrimaryColorV));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Secondary Color Hue"), OutEyeProperties.Iris.SecondaryColorU));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Secondary Color Value"), OutEyeProperties.Iris.SecondaryColorV));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Color Blend Coverage"), OutEyeProperties.Iris.ColorBlend));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Color Blend Coverage Softness"), OutEyeProperties.Iris.ColorBlendSoftness));

		float BlendMethod = 0.0f;
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Color Blend Switch"), BlendMethod));
		OutEyeProperties.Iris.BlendMethod = (BlendMethod < 0.5f) ? EMetaHumanCharacterEyesBlendMethod::Radial : EMetaHumanCharacterEyesBlendMethod::Structural;

		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Shadow Details Amount"), OutEyeProperties.Iris.ShadowDetails));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Limbal Ring Size"), OutEyeProperties.Iris.LimbalRingSize));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Limbal Ring Softness"), OutEyeProperties.Iris.LimbalRingSoftness));
		verify(EyeMaterial->GetVectorParameterValue(TEXT("Limbal Ring Color (Mult)"), OutEyeProperties.Iris.LimbalRingColor));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Limbal Ring Softness"), OutEyeProperties.Iris.LimbalRingSoftness));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Iris Global Saturation"), OutEyeProperties.Iris.GlobalSaturation));
		verify(EyeMaterial->GetVectorParameterValue(TEXT("Iris Color Multiply"), OutEyeProperties.Iris.GlobalTint));

		verify(EyeMaterial->GetScalarParameterValue(TEXT("Pupil Dilation"), OutEyeProperties.Pupil.Dilation));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Pupil Feather Strength"), OutEyeProperties.Pupil.Feather));

		verify(EyeMaterial->GetScalarParameterValue(TEXT("Cornea Size"), OutEyeProperties.Cornea.Size));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Corneal Limbus Softness"), OutEyeProperties.Cornea.LimbusSoftness));
		verify(EyeMaterial->GetVectorParameterValue(TEXT("Corneal Limbus Color (Mult)"), OutEyeProperties.Cornea.LimbusColor));

		verify(EyeMaterial->GetScalarParameterValue(TEXT("Sclera Rotation"), OutEyeProperties.Sclera.Rotation));
		verify(EyeMaterial->GetVectorParameterValue(TEXT("Sclera Color Multiply"), OutEyeProperties.Sclera.Tint));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Sclera Transmission Spread"), OutEyeProperties.Sclera.TransmissionSpread));
		verify(EyeMaterial->GetVectorParameterValue(TEXT("Sclera Transmission Color (Mult)"), OutEyeProperties.Sclera.TransmissionColor));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Sclera Irritation Veins Opacity"), OutEyeProperties.Sclera.VascularityIntensity));
		verify(EyeMaterial->GetScalarParameterValue(TEXT("Sclera Irritation Area Size"), OutEyeProperties.Sclera.VascularityCoverage));
	};

	GetEyePropertiesFromEyeMaterial(LeftEyeMaterial, OutEyeSettings.EyeLeft);
	GetEyePropertiesFromEyeMaterial(RightEyeMaterial, OutEyeSettings.EyeRight);
}

void FMetaHumanCharacterSkinMaterials::ApplyEyelashesPropertiesToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterEyelashesProperties& InEyelashesProperties)
{
	UTexture* EyelashesMask = GetEyelashesMask(InEyelashesProperties);

	InFaceMaterialSet.ForEachEyelashMaterial<UMaterialInstanceDynamic>(
		[EyelashesMask, &InEyelashesProperties](UMaterialInstanceDynamic* EyelashesMaterial)
		{
			EyelashesMaterial->SetTextureParameterValue(TEXT("Texture"), EyelashesMask);
			EyelashesMaterial->SetVectorParameterValue(TEXT("DyeColor"), InEyelashesProperties.DyeColor);
			EyelashesMaterial->SetScalarParameterValue(TEXT("Roughness"), InEyelashesProperties.Roughness);
			EyelashesMaterial->SetScalarParameterValue(TEXT("HairMelanin"), InEyelashesProperties.Melanin);
			EyelashesMaterial->SetScalarParameterValue(TEXT("HairRedness"), InEyelashesProperties.Redness);
		}
	);
}

void FMetaHumanCharacterSkinMaterials::ApplyTeethPropertiesToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterTeethProperties& InTeethProperties)
{
	if (UMaterialInstanceDynamic* TeethMaterial = Cast<UMaterialInstanceDynamic>(InFaceMaterialSet.Teeth))
	{
		TeethMaterial->SetVectorParameterValue(TEXT("Teeth Basecolor Multiply"), InTeethProperties.TeethColor);
		TeethMaterial->SetVectorParameterValue(TEXT("Gums Basecolor Multiply"), InTeethProperties.GumColor);		
		TeethMaterial->SetVectorParameterValue(TEXT("Plaque Basecolor Multiply"), InTeethProperties.PlaqueColor);
		TeethMaterial->SetScalarParameterValue(TEXT("Plaque Amount"), InTeethProperties.PlaqueAmount);
	}
}

UTexture2D* FMetaHumanCharacterSkinMaterials::GetEyelashesMask(const FMetaHumanCharacterEyelashesProperties& InEyelashesProperties)
{
	UTexture2D* EyelashesMask = UE::MetaHuman::GetEmptyMask();
	
	switch (InEyelashesProperties.Type)
	{
	case EMetaHumanCharacterEyelashesType::Sparse:
		EyelashesMask = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Eyelashes/T_Eyelashes_S_Sparse_Coverage.T_Eyelashes_S_Sparse_Coverage'"));
		break;

	case EMetaHumanCharacterEyelashesType::ShortFine:
		EyelashesMask = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Eyelashes/T_Eyelashes_S_Fine_Coverage.T_Eyelashes_S_Fine_Coverage'"));
		break;

	case EMetaHumanCharacterEyelashesType::Thin:
		EyelashesMask = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Eyelashes/T_Eyelashes_S_Thin_Coverage.T_Eyelashes_S_Thin_Coverage'"));
		break;

	case EMetaHumanCharacterEyelashesType::SlightCurl:
		EyelashesMask = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Eyelashes/T_Eyelashes_L_SlightCurl_Coverage.T_Eyelashes_L_SlightCurl_Coverage'"));
		break;

	case EMetaHumanCharacterEyelashesType::LongCurl:
		EyelashesMask = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Eyelashes/T_Eyelashes_L_Curl_Coverage.T_Eyelashes_L_Curl_Coverage'"));
		break;

	case EMetaHumanCharacterEyelashesType::ThickCurl:
		EyelashesMask = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Textures/Eyelashes/T_Eyelashes_L_ThickCurl_Coverage.T_Eyelashes_L_ThickCurl_Coverage'"));
		break;

	case EMetaHumanCharacterEyelashesType::None:
	default:
		break;
	}	
	
	return EyelashesMask;
}

FMetaHumanCharacterFaceMaterialSet FMetaHumanCharacterSkinMaterials::GetHeadPreviewMaterialInstance(EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterialType)
{
	UMaterialInterface* BaseHeadMaterialLOD0 = nullptr;
	UMaterialInterface* BaseHeadMaterialLOD1 = nullptr;
	UMaterialInterface* BaseHeadMaterialLOD2 = nullptr;
	UMaterialInterface* BaseHeadMaterialLOD3 = nullptr;
	UMaterialInterface* BaseHeadMaterialLOD4 = nullptr;
	UMaterialInterface* BaseHeadMaterialLOD57 = nullptr;
	UMaterialInterface* BaseLeftEyeMaterial = nullptr;
	UMaterialInterface* BaseRightEyeMaterial = nullptr;
	UMaterialInterface* BaseLacrimalFluidMaterial = nullptr;
	UMaterialInterface* BaseEyeOcclusionMaterial = nullptr;
	UMaterialInterface* BaseTeethMaterial = nullptr;
	UMaterialInterface* BaseEyelashesMaterialLOD0 = nullptr;
	UMaterialInterface* BaseEyelashesMaterialHiLODs = nullptr;

	// TODO: Figure out a way to not have these hard coded paths here, maybe using a data asset

	switch (InPreviewMaterialType)
	{
		case EMetaHumanCharacterSkinPreviewMaterial::Default:
			BaseHeadMaterialLOD0 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Materials/M_GrayTexture_Head.M_GrayTexture_Head'"));
			BaseHeadMaterialLOD1 = BaseHeadMaterialLOD0;
			BaseHeadMaterialLOD2 = BaseHeadMaterialLOD0;
			BaseHeadMaterialLOD3 = BaseHeadMaterialLOD0;
			BaseHeadMaterialLOD4 = BaseHeadMaterialLOD0;
			BaseHeadMaterialLOD57 = BaseHeadMaterialLOD0;
			BaseLeftEyeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Materials/M_GrayTexture_Eyes.M_GrayTexture_Eyes'"));
			BaseRightEyeMaterial = BaseLeftEyeMaterial;
			BaseTeethMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Materials/M_GrayTexture_Teeth.M_GrayTexture_Teeth'"));
			BaseLacrimalFluidMaterial = UE::MetaHuman::GetEmptyMaterial();
			BaseEyeOcclusionMaterial = UE::MetaHuman::GetEmptyMaterial();
			BaseEyelashesMaterialLOD0 = UE::MetaHuman::GetEmptyMaterial();
			BaseEyelashesMaterialHiLODs = UE::MetaHuman::GetEmptyMaterial();
			break;

		case EMetaHumanCharacterSkinPreviewMaterial::Editable:
		case EMetaHumanCharacterSkinPreviewMaterial::Clay:
			BaseHeadMaterialLOD0 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Skin_Head_UI_LOD0.MI_Skin_Head_UI_LOD0'"));
			BaseHeadMaterialLOD1 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Skin_Head_UI_LOD1.MI_Skin_Head_UI_LOD1'"));
			BaseHeadMaterialLOD2 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Skin_Head_UI_LOD2.MI_Skin_Head_UI_LOD2'"));
			BaseHeadMaterialLOD3 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Skin_Head_UI_LOD3.MI_Skin_Head_UI_LOD3'"));
			BaseHeadMaterialLOD4 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Skin_Head_UI_LOD4.MI_Skin_Head_UI_LOD4'"));
			BaseHeadMaterialLOD57 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Skin_Head_UI_LOD57.MI_Skin_Head_UI_LOD57'"));
			BaseLeftEyeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Eye_Left_MHC.MI_Eye_Left_MHC'"));
			BaseRightEyeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Eye_Right_MHC.MI_Eye_Right_MHC'"));
			BaseLacrimalFluidMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Lookdev_UHM/Eye/Materials/MI_eye_lacrimal_fluid_unified.MI_eye_lacrimal_fluid_unified'"));
			BaseEyeOcclusionMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Lookdev_UHM/Eye/Materials/MI_eye_occlusion_unified.MI_eye_occlusion_unified'"));
			BaseTeethMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Teeth_MHC_UI.MI_Teeth_MHC_UI'"));
			BaseEyelashesMaterialLOD0 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Eyelashes_LowerLODs.MI_Eyelashes_LowerLODs'"));
			BaseEyelashesMaterialHiLODs = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Eyelashes_HigherLODs.MI_Eyelashes_HigherLODs'"));
			break;

		default:
			break;
	};

	check(BaseHeadMaterialLOD0);
	check(BaseHeadMaterialLOD1);
	check(BaseHeadMaterialLOD2);
	check(BaseHeadMaterialLOD3);
	check(BaseHeadMaterialLOD4);
	check(BaseHeadMaterialLOD57);
	check(BaseLeftEyeMaterial);
	check(BaseRightEyeMaterial);
	check(BaseLacrimalFluidMaterial);
	check(BaseEyeOcclusionMaterial);
	check(BaseTeethMaterial);
	check(BaseEyelashesMaterialLOD0);
	check(BaseEyelashesMaterialHiLODs);

	return FMetaHumanCharacterFaceMaterialSet
	{
		.Skin =
		{
			{ EMetaHumanCharacterSkinMaterialSlot::LOD0, UE::MetaHuman::CreateMaterialInstance(BaseHeadMaterialLOD0, InPreviewMaterialType) },
			{ EMetaHumanCharacterSkinMaterialSlot::LOD1, UE::MetaHuman::CreateMaterialInstance(BaseHeadMaterialLOD1, InPreviewMaterialType) },
			{ EMetaHumanCharacterSkinMaterialSlot::LOD2, UE::MetaHuman::CreateMaterialInstance(BaseHeadMaterialLOD2, InPreviewMaterialType) },
			{ EMetaHumanCharacterSkinMaterialSlot::LOD3, UE::MetaHuman::CreateMaterialInstance(BaseHeadMaterialLOD3, InPreviewMaterialType) },
			{ EMetaHumanCharacterSkinMaterialSlot::LOD4, UE::MetaHuman::CreateMaterialInstance(BaseHeadMaterialLOD4, InPreviewMaterialType) },
			{ EMetaHumanCharacterSkinMaterialSlot::LOD5to7, UE::MetaHuman::CreateMaterialInstance(BaseHeadMaterialLOD57, InPreviewMaterialType) },
		},
		.EyeLeft = UE::MetaHuman::CreateMaterialInstance(BaseLeftEyeMaterial, InPreviewMaterialType),
		.EyeRight = UE::MetaHuman::CreateMaterialInstance(BaseRightEyeMaterial, InPreviewMaterialType),
		.EyeShell = UE::MetaHuman::CreateMaterialInstance(BaseEyeOcclusionMaterial, InPreviewMaterialType),
		.LacrimalFluid = UE::MetaHuman::CreateMaterialInstance(BaseLacrimalFluidMaterial, InPreviewMaterialType),
		.Teeth = UE::MetaHuman::CreateMaterialInstance(BaseTeethMaterial, InPreviewMaterialType),
		.Eyelashes = UE::MetaHuman::CreateMaterialInstance(BaseEyelashesMaterialLOD0, InPreviewMaterialType),
		.EyelashesHiLods = UE::MetaHuman::CreateMaterialInstance(BaseEyelashesMaterialHiLODs, InPreviewMaterialType),
	};
}

UMaterialInstanceDynamic* FMetaHumanCharacterSkinMaterials::GetBodyPreviewMaterialInstance(EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterialType)
{
	UMaterialInterface* BaseBodyMaterial = nullptr;

	switch (InPreviewMaterialType)
	{
		case EMetaHumanCharacterSkinPreviewMaterial::Default:
			BaseBodyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Materials/M_GrayTexture_Body.M_GrayTexture_Body'"));
			break;

		case EMetaHumanCharacterSkinPreviewMaterial::Editable:
		case EMetaHumanCharacterSkinPreviewMaterial::Clay:
			BaseBodyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/MI_Skin_Body_MHC.MI_Skin_Body_MHC'"));
			break;

		default:
			break;
	};

	check(BaseBodyMaterial);
	return UE::MetaHuman::CreateMaterialInstance(BaseBodyMaterial, InPreviewMaterialType);
}

void FMetaHumanCharacterSkinMaterials::SetMaterialInstanceParent(TNotNull<UMaterialInstanceConstant*> InMaterial, TNotNull<UMaterialInterface*> InNewParent)
{
	// Save the static switches of the material so they can be reset after the material
	TMap<FMaterialParameterInfo, FMaterialParameterMetadata> StaticSwitches;
	InMaterial->GetAllParametersOfType(EMaterialParameterType::StaticSwitch, StaticSwitches);

	TMap<FMaterialParameterInfo, FMaterialParameterMetadata> ScalarParams;
	InMaterial->GetAllParametersOfType(EMaterialParameterType::Scalar, ScalarParams);

	TMap<FMaterialParameterInfo, FMaterialParameterMetadata> ParentStaticSwitches;
	InMaterial->GetAllParametersOfType(EMaterialParameterType::StaticSwitch, ParentStaticSwitches);

	InMaterial->SetParentEditorOnly(InNewParent);

	// Reapply all static switches
	for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& StaticSwitchPair : StaticSwitches)
	{
		const FMaterialParameterInfo& Info = StaticSwitchPair.Key;
		const FMaterialParameterMetadata& Param = StaticSwitchPair.Value;
		const FMaterialParameterMetadata& ParentParam = ParentStaticSwitches[Info];

		const bool bIsEnabled = Param.Value.AsStaticSwitch();
		const bool bIsParentEnabled = ParentParam.Value.AsStaticSwitch();

		if (bIsEnabled != bIsParentEnabled)
		{
			InMaterial->SetStaticSwitchParameterValueEditorOnly(Info, bIsEnabled);
		}
	}

	// Need to be called after SetParentEditorOnly
	InMaterial->PostEditChange();
}

#undef LOCTEXT_NAMESPACE
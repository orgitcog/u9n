// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extensions/Text3DDefaultMaterialExtension.h"

#include "Engine/Texture2D.h"
#include "Extensions/Text3DLayoutExtensionBase.h"
#include "Logs/Text3DLogs.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Settings/Text3DProjectSettings.h"
#include "Text3DComponent.h"
#include "UObject/Package.h"

void UText3DDefaultMaterialExtension::SetStyle(EText3DMaterialStyle InStyle)
{
	if (Style == InStyle)
	{
		return;
	}

	Style = InStyle;
	OnCustomMaterialChanged();
}

void UText3DDefaultMaterialExtension::SetFrontColor(const FLinearColor& InColor)
{
	if (FrontColor.Equals(InColor))
	{
		return;
	}

	FrontColor = InColor;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetBackColor(const FLinearColor& InColor)
{
	if (BackColor.Equals(InColor))
	{
		return;
	}

	BackColor = InColor;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetExtrudeColor(const FLinearColor& InColor)
{
	if (ExtrudeColor.Equals(InColor))
	{
		return;
	}

	ExtrudeColor = InColor;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetBevelColor(const FLinearColor& InColor)
{
	if (BevelColor.Equals(InColor))
	{
		return;
	}

	BevelColor = InColor;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetGradientColorA(const FLinearColor& InColor)
{
	if (GradientColorA.Equals(InColor))
	{
		return;
	}
	
	GradientColorA = InColor;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetGradientColorB(const FLinearColor& InColor)
{
	if (GradientColorB.Equals(InColor))
	{
		return;
	}

	GradientColorB = InColor;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetGradientSmoothness(float InGradientSmoothness)
{
	if (FMath::IsNearlyEqual(GradientSmoothness, InGradientSmoothness))
	{
		return;
	}

	GradientSmoothness = InGradientSmoothness;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetGradientOffset(float InGradientOffset)
{
	if (FMath::IsNearlyEqual(GradientOffset, InGradientOffset))
	{
		return;
	}

	GradientOffset = InGradientOffset;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetGradientRotation(float InGradientRotation)
{
	if (FMath::IsNearlyEqual(GradientRotation, InGradientRotation))
	{
		return;
	}

	GradientRotation = InGradientRotation;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetTextureAsset(UTexture2D* InTextureAsset)
{
	if (TextureAsset == InTextureAsset)
	{
		return;
	}

	TextureAsset = InTextureAsset;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetTextureTiling(const FVector2D& InTextureTiling)
{
	if (TextureTiling.Equals(InTextureTiling))
	{
		return;
	}

	TextureTiling = InTextureTiling;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetBlendMode(EText3DMaterialBlendMode InBlendMode)
{
	if (BlendMode == InBlendMode)
	{
		return;
	}

	BlendMode = InBlendMode;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetIsUnlit(bool bInIsUnlit)
{
	if (bIsUnlit == bInIsUnlit)
	{
		return;
	}

	bIsUnlit = bInIsUnlit;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetOpacity(float InOpacity)
{
	if (FMath::IsNearlyEqual(Opacity, InOpacity))
	{
		return;
	}

	Opacity = InOpacity;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetUseMask(bool bInUseMask)
{
	if (bUseMask == bInUseMask)
	{
		return;
	}

	bUseMask = bInUseMask;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetMaskOffset(float InMaskOffset)
{
	if (FMath::IsNearlyEqual(MaskOffset, InMaskOffset))
	{
		return;
	}

	MaskOffset = InMaskOffset;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetMaskSmoothness(float InMaskSmoothness)
{
	if (FMath::IsNearlyEqual(MaskSmoothness, InMaskSmoothness))
	{
		return;
	}

	MaskSmoothness = InMaskSmoothness;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetMaskRotation(float InMaskRotation)
{
	if (FMath::IsNearlyEqual(MaskRotation, InMaskRotation))
	{
		return;
	}

	MaskRotation = InMaskRotation;
	OnMaterialOptionsChanged();
}

void UText3DDefaultMaterialExtension::SetUseSingleMaterial(bool bInUse)
{
	if (bUseSingleMaterial == bInUse)
	{
		return;
	}

	bUseSingleMaterial = bInUse;
	OnCustomMaterialChanged();
}

void UText3DDefaultMaterialExtension::SetFrontMaterial(UMaterialInterface* InMaterial)
{
	if (FrontMaterial == InMaterial)
	{
		return;
	}

	FrontMaterial = InMaterial;
	OnCustomMaterialChanged();
}

void UText3DDefaultMaterialExtension::SetBevelMaterial(UMaterialInterface* InMaterial)
{
	if (BevelMaterial == InMaterial)
	{
		return;
	}
	
	BevelMaterial = InMaterial;
	OnCustomMaterialChanged();
}

void UText3DDefaultMaterialExtension::SetExtrudeMaterial(UMaterialInterface* InMaterial)
{
	if (ExtrudeMaterial == InMaterial)
	{
		return;
	}
	
	ExtrudeMaterial	= InMaterial;
	OnCustomMaterialChanged();
}

void UText3DDefaultMaterialExtension::SetBackMaterial(UMaterialInterface* InMaterial)
{
	if (BackMaterial == InMaterial)
	{
		return;
	}
	
	BackMaterial = InMaterial;
	OnCustomMaterialChanged();
}

EText3DExtensionResult UText3DDefaultMaterialExtension::PreRendererUpdate(EText3DRendererFlags InFlag)
{
	if (InFlag != EText3DRendererFlags::Material)
	{
		return EText3DExtensionResult::Active;
	}

	if (Style == EText3DMaterialStyle::Custom || Style == EText3DMaterialStyle::Invalid)
	{
		return EText3DExtensionResult::Finished;
	}

	UMaterialInstanceDynamic* DynFrontMaterial = FindOrAdd(EText3DGroupType::Front);
	UMaterialInstanceDynamic* DynBackMaterial = FindOrAdd(EText3DGroupType::Back);
	UMaterialInstanceDynamic* DynExtrudeMaterial = FindOrAdd(EText3DGroupType::Extrude);
	UMaterialInstanceDynamic* DynBevelMaterial = FindOrAdd(EText3DGroupType::Bevel);

	if (!DynFrontMaterial || !DynBackMaterial || !DynExtrudeMaterial || !DynBevelMaterial)
	{
		UE_LOG(LogText3D, Error, TEXT("Failed to retrieve dynamic material in Text3D material extension"))
		return EText3DExtensionResult::Failed;
	}

	TArray<UMaterialInstanceDynamic*> Materials
	{
		DynFrontMaterial,
		DynBackMaterial,
		DynExtrudeMaterial,
		DynBevelMaterial
	};

	switch (Style)
	{
	case EText3DMaterialStyle::Solid:
		DynFrontMaterial->SetVectorParameterValue(UText3DProjectSettings::FMaterialParameters::SolidColor, FrontColor);
		DynBackMaterial->SetVectorParameterValue(UText3DProjectSettings::FMaterialParameters::SolidColor, BackColor);
		DynExtrudeMaterial->SetVectorParameterValue(UText3DProjectSettings::FMaterialParameters::SolidColor, ExtrudeColor);
		DynBevelMaterial->SetVectorParameterValue(UText3DProjectSettings::FMaterialParameters::SolidColor, BevelColor);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::Mode, 0);
		break;
	case EText3DMaterialStyle::Gradient:
		SetVectorParameter(Materials, UText3DProjectSettings::FMaterialParameters::GradientColorA, GradientColorA);
		SetVectorParameter(Materials, UText3DProjectSettings::FMaterialParameters::GradientColorB, GradientColorB);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::GradientOffset, GradientOffset);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::GradientSmoothness, GradientSmoothness);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::GradientRotation, GradientRotation);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::Mode, 1);
		break;
	case EText3DMaterialStyle::Texture:
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::TexturedUTiling, TextureTiling.X);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::TexturedVTiling, TextureTiling.Y);
		SetTextureParameter(Materials, UText3DProjectSettings::FMaterialParameters::MainTexture, TextureAsset);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::Mode, 2);
		break;
	default:
		break;
	}

	if (BlendMode == EText3DMaterialBlendMode::Translucent || Style == EText3DMaterialStyle::Gradient)
	{
		const UText3DComponent* Text3DComponent = GetText3DComponent();

		const FBox LocalBounds = Text3DComponent->GetBounds();
		const FVector LocalBoundsExtent = Text3DComponent->GetRelativeRotation().UnrotateVector(LocalBounds.GetSize());

		const UText3DLayoutExtensionBase* LayoutExtension = Text3DComponent->GetLayoutExtension();
		const FVector LineLocation = LayoutExtension->GetLineLocation(0);

		const FVector TextScaleFactor = LayoutExtension->GetTextScale() * Text3DComponent->GetComponentScale();
		const FVector BoundsSizeScaled = LocalBoundsExtent / TextScaleFactor;

		SetVectorParameter(Materials, UText3DProjectSettings::FMaterialParameters::BoundsOrigin, LineLocation);
		SetVectorParameter(Materials, UText3DProjectSettings::FMaterialParameters::BoundsSize, BoundsSizeScaled);
	}

	if (BlendMode == EText3DMaterialBlendMode::Translucent)
	{
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::Opacity, Opacity);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::MaskEnabled, bUseMask ? 1 : 0);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::MaskOffset, MaskOffset);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::MaskRotation, MaskRotation);
		SetScalarParameter(Materials, UText3DProjectSettings::FMaterialParameters::MaskSmoothness, MaskSmoothness);
	}

	FrontMaterial = DynFrontMaterial;
	BackMaterial = DynBackMaterial;
	ExtrudeMaterial = DynExtrudeMaterial;
	BevelMaterial = DynBevelMaterial;

	return EText3DExtensionResult::Finished;
}

EText3DExtensionResult UText3DDefaultMaterialExtension::PostRendererUpdate(EText3DRendererFlags InFlag)
{
	return EText3DExtensionResult::Active;
}

void UText3DDefaultMaterialExtension::SetMaterial(EText3DGroupType InGroup, UMaterialInterface* InMaterial)
{
	switch (InGroup)
	{
	case EText3DGroupType::Front:
		SetFrontMaterial(InMaterial);
		break;
	case EText3DGroupType::Bevel:
		SetBevelMaterial(InMaterial);
		break;
	case EText3DGroupType::Extrude:
		SetExtrudeMaterial(InMaterial);
		break;
	case EText3DGroupType::Back:
		SetBackMaterial(InMaterial);
		break;
	default:
		return;
	}
}

UMaterialInstanceDynamic* UText3DDefaultMaterialExtension::FindOrAdd(EText3DGroupType InGroup)
{
	const UText3DProjectSettings* Text3DSettings = UText3DProjectSettings::Get();
	check(!!Text3DSettings);

	const FText3DMaterialKey MaterialKey(BlendMode, bIsUnlit);
	UMaterial* ParentMaterial = Text3DSettings->GetBaseMaterial(MaterialKey);

	if (UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(GetMaterial(InGroup)))
	{
		if (DynMat->GetBaseMaterial() == ParentMaterial)
		{
			return DynMat;
		}
	}

	if (Style == EText3DMaterialStyle::Custom || Style == EText3DMaterialStyle::Invalid)
	{
		return nullptr;
	}

	FText3DMaterialGroupKey GroupKey(MaterialKey, InGroup, Style);
	if (const TObjectPtr<UMaterialInstanceDynamic>* Material = GroupDynamicMaterials.Find(GroupKey))
	{
		return *Material;
	}

	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(ParentMaterial, GetTransientPackage());
	GroupDynamicMaterials.Emplace(GroupKey, DynamicMaterial);

	return DynamicMaterial;
}

void UText3DDefaultMaterialExtension::SetVectorParameter(TConstArrayView<UMaterialInstanceDynamic*> InMaterials, FName InKey, FVector InValue)
{
	for (UMaterialInstanceDynamic* Material : InMaterials)
	{
		Material->SetVectorParameterValue(InKey, InValue);
	}
}

void UText3DDefaultMaterialExtension::SetVectorParameter(TConstArrayView<UMaterialInstanceDynamic*> InMaterials, FName InKey, FLinearColor InValue)
{
	for (UMaterialInstanceDynamic* Material : InMaterials)
	{
		Material->SetVectorParameterValue(InKey, InValue);
	}
}

void UText3DDefaultMaterialExtension::SetScalarParameter(TConstArrayView<UMaterialInstanceDynamic*> InMaterials, FName InKey, float InValue)
{
	for (UMaterialInstanceDynamic* Material : InMaterials)
	{
		Material->SetScalarParameterValue(InKey, InValue);
	}
}

void UText3DDefaultMaterialExtension::SetTextureParameter(TConstArrayView<UMaterialInstanceDynamic*> InMaterials, FName InKey, UTexture* InValue)
{
	for (UMaterialInstanceDynamic* Material : InMaterials)
	{
		Material->SetTextureParameterValue(InKey, InValue);
	}
}

void UText3DDefaultMaterialExtension::OnMaterialOptionsChanged()
{
	RequestUpdate(EText3DRendererFlags::Material);
}

void UText3DDefaultMaterialExtension::OnCustomMaterialChanged()
{
	if (bUseSingleMaterial && Style == EText3DMaterialStyle::Custom)
	{
		BackMaterial = FrontMaterial;
		BevelMaterial = FrontMaterial;
		ExtrudeMaterial = FrontMaterial;
	}

	OnMaterialOptionsChanged();
}

UMaterialInterface* UText3DDefaultMaterialExtension::GetMaterial(EText3DGroupType InGroup) const
{
	switch (InGroup)
	{
	case EText3DGroupType::Front:
		return FrontMaterial;
		break;
	case EText3DGroupType::Bevel:
		return BevelMaterial;
		break;
	case EText3DGroupType::Extrude:
		return ExtrudeMaterial;
		break;
	case EText3DGroupType::Back:
		return BackMaterial;
		break;
	default:
		return nullptr;
	}
}

FVector UText3DDefaultMaterialExtension::GetGradientDirection() const
{
	const UText3DComponent* Text3DComponent = GetText3DComponent();
	FVector GradientDir = Text3DComponent->GetUpVector().RotateAngleAxis(-GradientRotation * 360, Text3DComponent->GetForwardVector());

	/**
	 * In order to properly map gradient offset along the text surface, text bounds are not normalized (anymore) in the material function creating the gradient.
	 * Therefore, we need to remap the gradient direction, taking into account the current text actor bounds.
	 */
	FVector Origin;
	FVector Extent;
	Text3DComponent->GetBounds(Origin, Extent);

	const FVector GradientDirFixer = FVector(1.0f, Extent.Z, Extent.Y);
	GradientDir *= GradientDirFixer;

	GradientDir.Normalize();

	return GradientDir;
}

void UText3DDefaultMaterialExtension::PreCacheMaterials()
{
	if (UMaterialInstanceDynamic* NewFrontMaterial = FindOrAdd(EText3DGroupType::Front))
	{
		FrontMaterial = NewFrontMaterial;
	}

	if (UMaterialInstanceDynamic* NewBackMaterial = FindOrAdd(EText3DGroupType::Back))
	{
		BackMaterial = NewBackMaterial;
	}
	
	if (UMaterialInstanceDynamic* NewExtrudeMaterial = FindOrAdd(EText3DGroupType::Extrude))
	{
		ExtrudeMaterial = NewExtrudeMaterial;
	}
	
	if (UMaterialInstanceDynamic* NewBevelMaterial = FindOrAdd(EText3DGroupType::Bevel))
	{
		BevelMaterial = NewBevelMaterial;
	}
}

void UText3DDefaultMaterialExtension::PostLoad()
{
	Super::PostLoad();
	
	PreCacheMaterials();
}

#if WITH_EDITOR
void UText3DDefaultMaterialExtension::PostEditChangeProperty(FPropertyChangedEvent& InEvent)
{
	Super::PostEditChangeProperty(InEvent);

	static const TSet<FName> PropertyNames =
	{
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, FrontColor),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, BackColor),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, ExtrudeColor),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, BevelColor),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, GradientColorA),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, GradientColorB),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, GradientSmoothness),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, GradientOffset),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, GradientRotation),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, TextureAsset),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, TextureTiling),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, BlendMode),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, bIsUnlit),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, Opacity),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, bUseMask),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, MaskOffset),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, MaskSmoothness),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, MaskRotation),
	};

	static const TSet<FName> CustomPropertyNames =
	{
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, Style),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, bUseSingleMaterial),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, FrontMaterial),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, BevelMaterial),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, ExtrudeMaterial),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultMaterialExtension, BackMaterial),
	};

	const FName MemberPropertyName = InEvent.GetMemberPropertyName();

	if (CustomPropertyNames.Contains(MemberPropertyName))
	{
		OnCustomMaterialChanged();
	}
	else if (PropertyNames.Contains(MemberPropertyName))
	{
		OnMaterialOptionsChanged();
	}
}
#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Renderers/StaticMeshes/Text3DStaticMeshesRenderer.h"

#include "Algo/Accumulate.h"
#include "Algo/Count.h"
#include "Characters/Text3DCharacterBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Extensions/Text3DGeometryExtensionBase.h"
#include "Extensions/Text3DLayoutExtensionBase.h"
#include "Extensions/Text3DMaterialExtensionBase.h"
#include "Extensions/Text3DRenderingExtensionBase.h"
#include "LayoutBuilders/Text3DShapedGlyphText.h"
#include "LayoutBuilders/Text3DLayoutShaper.h"
#include "Logs/Text3DLogs.h"
#include "Misc/EnumerateRange.h"
#include "Subsystems/Text3DEngineSubsystem.h"
#include "Text3DComponent.h"
#include "Text3DInternalTypes.h"

void UText3DStaticMeshesRenderer::OnCreate()
{
	if (!TextRoot)
	{
		UText3DComponent* TextComponent = GetText3DComponent();
		TextRoot = NewObject<USceneComponent>(TextComponent, TEXT("TextRoot"), RF_Transient);
		TextRoot->SetupAttachment(TextComponent);
	}
}

void UText3DStaticMeshesRenderer::OnUpdate(EText3DRendererFlags InFlags)
{
	const UText3DComponent* TextComponent = GetText3DComponent();

	if (EnumHasAnyFlags(InFlags, EText3DRendererFlags::Geometry))
	{
		AllocateCharacterComponents(TextComponent->GetCharacterCount());

		const UText3DGeometryExtensionBase* GeometryExtension = TextComponent->GetGeometryExtension();

		TextComponent->ForEachCharacter([this, GeometryExtension](UText3DCharacterBase* InCharacter, uint16 InIndex, uint16 InCount)
		{
			UStaticMesh* CachedMesh = GeometryExtension->FindOrLoadGlyphMesh(InCharacter);

			if (CharacterMeshes.IsValidIndex(InIndex))
			{
				CharacterMeshes[InIndex]->SetStaticMesh(CachedMesh);
			}
			else
			{
				UE_LOG(LogText3D, Warning, TEXT("Invalid component index for character %i"), InIndex)
			}
		});

		RefreshBounds();
	}

	if (EnumHasAnyFlags(InFlags, EText3DRendererFlags::Layout))
	{
		const UText3DLayoutExtensionBase* LayoutExtension = TextComponent->GetLayoutExtension();

		TextRoot->SetRelativeScale3D(LayoutExtension->GetTextScale());

		TextComponent->ForEachCharacter([this](UText3DCharacterBase* InCharacter, uint16 InIndex, uint16 InCount)
		{
			constexpr bool bReset = false;
			const FTransform& CharacterTransform = InCharacter->GetTransform(bReset);

			if (CharacterKernings.IsValidIndex(InIndex) && CharacterMeshes.IsValidIndex(InIndex))
			{
				CharacterKernings[InIndex]->SetRelativeTransform(FTransform::Identity);
				CharacterMeshes[InIndex]->SetRelativeTransform(CharacterTransform);
			}
			else
			{
				UE_LOG(LogText3D, Warning, TEXT("Invalid component index for character %i"), InIndex)
			}
		});

		RefreshBounds();
	}

	if (EnumHasAnyFlags(InFlags, EText3DRendererFlags::Material))
	{
		using namespace UE::Text3D::Materials;

		const UText3DMaterialExtensionBase* MaterialExtension = TextComponent->GetMaterialExtension();

		TextComponent->ForEachCharacter([this, MaterialExtension](UText3DCharacterBase* InCharacter, uint16 InIndex, uint16 InCount)
		{
			if (CharacterMeshes.IsValidIndex(InIndex))
			{
				UStaticMeshComponent* StaticMeshComponent = CharacterMeshes[InIndex];
				for (int32 GroupIndex = 0; GroupIndex < static_cast<int32>(EText3DGroupType::TypeCount); GroupIndex++)
				{
					const int32 MaterialIndex = StaticMeshComponent->GetMaterialIndex(SlotNames[GroupIndex]);

					if (MaterialIndex == INDEX_NONE)
					{
						continue;
					}

					UMaterialInterface* Material = MaterialExtension->GetMaterial(static_cast<EText3DGroupType>(GroupIndex));

					if (Material != StaticMeshComponent->GetMaterial(MaterialIndex))
					{
						StaticMeshComponent->SetMaterial(MaterialIndex, Material);
					}
				}
			}
			else
			{
				UE_LOG(LogText3D, Warning, TEXT("Invalid component index for character %i"), InIndex)
			}
		});
	}

	if (EnumHasAnyFlags(InFlags, EText3DRendererFlags::Visibility))
	{
		const UText3DRenderingExtensionBase* RenderingExtension = TextComponent->GetRenderingExtension();

		TextComponent->ForEachCharacter([this, TextComponent, RenderingExtension](UText3DCharacterBase* InCharacter, uint16 InIndex, uint16 InCount)
		{
			if (CharacterMeshes.IsValidIndex(InIndex))
			{
				UStaticMeshComponent* StaticMeshComponent = CharacterMeshes[InIndex];
				StaticMeshComponent->SetHiddenInGame(TextComponent->bHiddenInGame);
				StaticMeshComponent->SetVisibility(TextComponent->GetVisibleFlag() && InCharacter->GetVisibility());
				StaticMeshComponent->SetCastShadow(RenderingExtension->GetTextCastShadow());
				StaticMeshComponent->SetCastHiddenShadow(RenderingExtension->GetTextCastHiddenShadow());
				StaticMeshComponent->SetAffectDynamicIndirectLighting(RenderingExtension->GetTextAffectDynamicIndirectLighting());
				StaticMeshComponent->SetAffectIndirectLightingWhileHidden(RenderingExtension->GetTextAffectIndirectLightingWhileHidden());
				StaticMeshComponent->SetHoldout(RenderingExtension->GetTextHoldout());
			}
			else
			{
				UE_LOG(LogText3D, Warning, TEXT("Invalid component index for character %i"), InIndex)
			}
		});
	}
}

void UText3DStaticMeshesRenderer::OnClear()
{
	for (UStaticMeshComponent* MeshComponent : CharacterMeshes)
	{
		if (MeshComponent)
		{
			MeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			MeshComponent->SetStaticMesh(nullptr);
			MeshComponent->DestroyComponent();
		}
	}
	CharacterMeshes.Reset();

	for (USceneComponent* KerningComponent : CharacterKernings)
	{
		if (KerningComponent)
		{
			KerningComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			KerningComponent->DestroyComponent();
		}
	}
	CharacterKernings.Reset();

	if (TextRoot)
	{
		TArray<USceneComponent*> ChildComponents;
		constexpr bool bIncludeChildDescendants = true;
		TextRoot->GetChildrenComponents(bIncludeChildDescendants, ChildComponents);

		for (USceneComponent* ChildComponent : ChildComponents)
		{
			if (IsValid(ChildComponent))
			{
				ChildComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				ChildComponent->DestroyComponent();
			}
		}
	}
}

void UText3DStaticMeshesRenderer::OnDestroy()
{
	if (TextRoot)
	{
		TextRoot->DestroyComponent();
		TextRoot = nullptr;
	}
}

FName UText3DStaticMeshesRenderer::GetName() const
{
	static const FName Name(TEXT("StaticMeshesRenderer"));
	return Name;
}

FBox UText3DStaticMeshesRenderer::OnCalculateBounds() const
{
	FBox Box(ForceInit);

	for (const UStaticMeshComponent* StaticMeshComponent : CharacterMeshes)
	{
		Box += StaticMeshComponent->Bounds.GetBox();
	}

	return Box;
}

bool UText3DStaticMeshesRenderer::AllocateCharacterComponents(int32 Num)
{
	int32 DeltaNum = Num - CharacterMeshes.Num();
	if (DeltaNum == 0)
	{
		return false;
	}

	// Add characters
	if (FMath::Sign(DeltaNum) > 0)
	{
		int32 GlyphId = CharacterMeshes.Num() - 1;
		for(int32 CharacterIndex = 0; CharacterIndex < DeltaNum; ++CharacterIndex)
		{
			GlyphId++;

			const FName CharacterKerningComponentName = MakeUniqueObjectName(this, USceneComponent::StaticClass(), FName(*FString::Printf(TEXT("CharacterKerning%d"), GlyphId)));
			USceneComponent* CharacterKerningComponent = NewObject<USceneComponent>(this, CharacterKerningComponentName, RF_Transient);
			CharacterKerningComponent->AttachToComponent(TextRoot, FAttachmentTransformRules::KeepRelativeTransform);
			CharacterKerningComponent->RegisterComponent();
			CharacterKernings.Add(CharacterKerningComponent);

			const FName StaticMeshComponentName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("StaticMeshComponent%d"), GlyphId)));
			UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this, StaticMeshComponentName, RF_Transient);
			StaticMeshComponent->AttachToComponent(CharacterKerningComponent, FAttachmentTransformRules::KeepRelativeTransform);
			StaticMeshComponent->RegisterComponent();
			CharacterMeshes.Add(StaticMeshComponent);
		}
	}
	// Remove characters
	else
	{
		DeltaNum = FMath::Abs(DeltaNum);
		for(int32 CharacterIndex = CharacterKernings.Num() - 1 - DeltaNum; CharacterIndex < CharacterKernings.Num(); ++CharacterIndex)
		{
			USceneComponent* CharacterKerningComponent = CharacterKernings[CharacterIndex];
			// If called in quick succession, may already be pending destruction
			if (IsValid(CharacterKerningComponent))
			{
				CharacterKerningComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				CharacterKerningComponent->UnregisterComponent();
				CharacterKerningComponent->DestroyComponent();
			}

			UStaticMeshComponent* StaticMeshComponent = CharacterMeshes[CharacterIndex];
			if (IsValid(StaticMeshComponent))
			{
				StaticMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				StaticMeshComponent->UnregisterComponent();
				StaticMeshComponent->DestroyComponent();
			}
		}

		CharacterKernings.RemoveAt(CharacterKernings.Num() - 1 - DeltaNum, DeltaNum);
		CharacterMeshes.RemoveAt(CharacterMeshes.Num() - 1 - DeltaNum, DeltaNum);
	}

	return true;
}

int32 UText3DStaticMeshesRenderer::GetGlyphCount()
{
	if (!TextRoot)
	{
		return 0;
	}

	return TextRoot->GetNumChildrenComponents();
}

USceneComponent* UText3DStaticMeshesRenderer::GetGlyphKerningComponent(int32 Index)
{
	if (!CharacterKernings.IsValidIndex(Index))
	{
		return nullptr;
	}

	return CharacterKernings[Index];
}

const TArray<USceneComponent*>& UText3DStaticMeshesRenderer::GetGlyphKerningComponents()
{
	return CharacterKernings;
}

UStaticMeshComponent* UText3DStaticMeshesRenderer::GetGlyphMeshComponent(int32 Index)
{
	if (!CharacterMeshes.IsValidIndex(Index))
	{
		return nullptr;
	}

	return CharacterMeshes[Index];
}

const TArray<UStaticMeshComponent*>& UText3DStaticMeshesRenderer::GetGlyphMeshComponents()
{
	return CharacterMeshes;
}

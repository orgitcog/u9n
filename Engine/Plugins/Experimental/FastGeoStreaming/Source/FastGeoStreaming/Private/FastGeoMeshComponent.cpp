// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoMeshComponent.h"

const FFastGeoElementType FFastGeoMeshComponent::Type(&FFastGeoMeshComponent::Type);

FFastGeoMeshComponent::FFastGeoMeshComponent(int32 InComponentIndex, FFastGeoElementType InType)
	: Super(InComponentIndex, InType)
{
}	

void FFastGeoMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize persistent data from FFastGeoMeshComponent
	Ar << OverrideMaterials;
}

#if WITH_EDITOR
void FFastGeoMeshComponent::InitializeFromComponent(UActorComponent* Component)
{
	Super::InitializeFromComponent(Component);

	UMeshComponent* MeshComponent = CastChecked<UMeshComponent>(Component);
	OverrideMaterials = MeshComponent->OverrideMaterials;
}
#endif

void FFastGeoMeshComponent::GetMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& OutMaterialSlotOverlayMaterials) const
{
	FMeshComponentHelper::GetMaterialSlotsOverlayMaterial(*this, OutMaterialSlotOverlayMaterials);
}

FMaterialRelevance FFastGeoMeshComponent::GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return FMeshComponentHelper::GetMaterialRelevance(*this, InFeatureLevel);
}

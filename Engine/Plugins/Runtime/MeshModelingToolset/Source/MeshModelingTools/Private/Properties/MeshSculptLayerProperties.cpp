// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/MeshSculptLayerProperties.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "ModelingToolExternalMeshUpdateAPI.h"
#include "Changes/MeshReplacementChange.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshSculptLayerProperties)

#define LOCTEXT_NAMESPACE "UMeshSculptLayerProperties"

void UMeshSculptLayerProperties::Init(IModelingToolExternalDynamicMeshUpdateAPI* InTool, int32 InNumLockedBaseLayers)
{
	bCanEditLayers = true;
	LayerWeights.Reset();

	Tool = InTool;
	if (!Tool)
	{
		return;
	}
	NumLockedBaseLayers = InNumLockedBaseLayers;

	Tool->ProcessToolMeshes([this](const UE::Geometry::FDynamicMesh3& Mesh, int32 MeshIdx)
		{
			// Sculpt layer UI only supports a single mesh for now
			if (MeshIdx > 0)
			{
				return;
			}
			bCanEditLayers = Mesh.HasAttributes() && Mesh.Attributes()->NumSculptLayers() > NumLockedBaseLayers;
			if (bCanEditLayers)
			{
				UpdateSettingsFromMesh(Mesh.Attributes()->GetSculptLayers());
			}
		}
	);
}

void UMeshSculptLayerProperties::UpdateSettingsFromMesh(const UE::Geometry::FDynamicMeshSculptLayers* SculptLayers)
{
	TConstArrayView<double> SourceWeights = SculptLayers->GetLayerWeights();
	int32 NumLayers = FMath::Max(0, SourceWeights.Num() - NumLockedBaseLayers);
	LayerWeights.SetNum(NumLayers);
	for (int32 Idx = 0; Idx < LayerWeights.Num(); ++Idx)
	{
		LayerWeights[Idx] = SourceWeights[Idx + NumLockedBaseLayers];
	}
	ActiveLayer = SculptLayers->GetActiveLayer();
}

void UMeshSculptLayerProperties::SetLayerWeights(UE::Geometry::FDynamicMeshSculptLayers* SculptLayers) const
{
	TArray<double> FullLayerWeights(SculptLayers->GetLayerWeights());
	FullLayerWeights.SetNum(LayerWeights.Num() + NumLockedBaseLayers);
	for (int32 Idx = 0; Idx < LayerWeights.Num(); ++Idx)
	{
		FullLayerWeights[Idx + NumLockedBaseLayers] = LayerWeights[Idx];
	}
	SculptLayers->UpdateLayerWeights(FullLayerWeights);
}

void UMeshSculptLayerProperties::EditSculptLayers(TFunctionRef<void(UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshSculptLayers* SculptLayers)> EditFn, bool bEmitChange)
{
	if (Tool && Tool->AllowToolMeshUpdates())
	{
		Tool->UpdateToolMeshes([this, &EditFn, bEmitChange](UE::Geometry::FDynamicMesh3& Mesh, int32 Idx) -> TUniquePtr<FMeshRegionChangeBase>
			{
				// Sculpt layer UI only supports a single mesh for now
				if (Idx > 0)
				{
					return nullptr;
				}

				if (!InitialMesh)
				{
					InitialMesh = MakeShared<UE::Geometry::FDynamicMesh3>(Mesh);
				}
				if (UE::Geometry::FDynamicMeshAttributeSet* Attributes = Mesh.Attributes())
				{
					if (UE::Geometry::FDynamicMeshSculptLayers* SculptLayers = Attributes->GetSculptLayers())
					{
						EditFn(Mesh, SculptLayers);
					}
				}

				if (!bEmitChange)
				{
					return nullptr;
				}
				TUniquePtr<FMeshRegionChangeBase> Result(new FMeshReplacementChange(InitialMesh, MakeShared<FDynamicMesh3>(Mesh)));
				InitialMesh = nullptr;
				return MoveTemp(Result);
			}
		);
	}
}

#if WITH_EDITOR
void UMeshSculptLayerProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	EditSculptLayers([this, &PropertyChangedEvent](UE::Geometry::FDynamicMesh3& Mesh, FDynamicMeshSculptLayers* SculptLayers)
		{
			if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UMeshSculptLayerProperties, LayerWeights))
			{
				SetLayerWeights(SculptLayers);
			}
			if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UMeshSculptLayerProperties, ActiveLayer))
			{
				ActiveLayer = FMath::Clamp(ActiveLayer, NumLockedBaseLayers, SculptLayers->NumLayers() - 1);
				SculptLayers->SetActiveLayer(ActiveLayer);
			}
		},
		PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
	);
}
#endif

void UMeshSculptLayerProperties::AddLayer()
{
	EditSculptLayers([this](UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshSculptLayers* SculptLayers)
		{
			Mesh.Attributes()->EnableSculptLayers(SculptLayers->NumLayers() + 1);
			LayerWeights.Add(1.0);
		}, true);
}

void UMeshSculptLayerProperties::RemoveLayer()
{
	if (LayerWeights.Num() <= 1)
	{
		return;
	}
	EditSculptLayers([this](UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshSculptLayers* SculptLayers)
		{
			if (!SculptLayers->DiscardSculptLayer(ActiveLayer))
			{
				return;
			}
			UpdateSettingsFromMesh(SculptLayers);
			// If the system picked a locked active layer, try to pick a different layer instead
			if (ActiveLayer < NumLockedBaseLayers)
			{
				// We shouldn't set a zero-weight layer as the active layer, so look for a non-zero weight layer to set
				int32 NonZeroLayerIdx = INDEX_NONE;
				for (int32 Idx = 0; Idx < LayerWeights.Num(); ++Idx)
				{
					if (LayerWeights[Idx] != 0.0)
					{
						NonZeroLayerIdx = Idx;
						break;
					}
				}
				// if all layers had zero weight, just pick the first layer and set its weight to 1.0 so it is ready to sculpt on
				if (NonZeroLayerIdx == INDEX_NONE)
				{
					LayerWeights[0] = 1.0;
					NonZeroLayerIdx = 0;
					SetLayerWeights(SculptLayers);
				}
				ActiveLayer = SculptLayers->SetActiveLayer(NumLockedBaseLayers + NonZeroLayerIdx);
			}
		}, true);
}


#undef LOCTEXT_NAMESPACE


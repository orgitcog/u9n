// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoStaticMeshComponent.h"
#include "FastGeoContainer.h"
#include "FastGeoLog.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "Materials/MaterialRelevance.h"
#include "SceneInterface.h"
#include "MeshComponentHelper.h"
#include "StaticMeshComponentHelper.h"
#include "Rendering/NaniteResourcesHelper.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "NaniteVertexFactory.h"
#include "StaticMeshSceneProxy.h"

#if WITH_EDITOR
#include "ObjectCacheEventSink.h"
#include "WorldPartition/HLOD/HLODActor.h"
#endif

const FFastGeoElementType FFastGeoStaticMeshComponentBase::Type(&FFastGeoPrimitiveComponent::Type);

FFastGeoStaticMeshComponentBase::FFastGeoStaticMeshComponentBase(int32 InComponentIndex, FFastGeoElementType InType)
	: Super(InComponentIndex, InType)
{
}

void FFastGeoStaticMeshComponentBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize persistent data from FStaticMeshSceneProxyDesc
	FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	Ar << SceneProxyDesc.StaticMesh;
	Ar << SceneProxyDesc.OverlayMaterial;
	Ar << SceneProxyDesc.MaterialSlotsOverlayMaterial;
	Ar << SceneProxyDesc.OverlayMaterialMaxDrawDistance;
	Ar << SceneProxyDesc.ForcedLodModel;
	Ar << SceneProxyDesc.MinLOD;
	Ar << SceneProxyDesc.WorldPositionOffsetDisableDistance;
	Ar << SceneProxyDesc.NanitePixelProgrammableDistance;
	Ar << SceneProxyDesc.DistanceFieldSelfShadowBias;
	Ar << SceneProxyDesc.DistanceFieldIndirectShadowMinVisibility;
	Ar << SceneProxyDesc.StaticLightMapResolution;
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bReverseCulling);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bEvaluateWorldPositionOffset);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bOverrideMinLOD);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastDistanceFieldIndirectShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bOverrideDistanceFieldSelfShadowBias);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bEvaluateWorldPositionOffsetInRayTracing);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bSortTriangles);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bDisallowNanite);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bForceDisableNanite);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bForceNaniteForMasked);
	Ar << bUseDefaultCollision;
}

void FFastGeoStaticMeshComponentBase::InitializeDynamicProperties()
{
#if !WITH_EDITOR
	// When using default collision, use the same collision profile as the StaticMesh
	if (bUseDefaultCollision)
	{
		if (UBodySetup* BodySetup = GetBodySetup())
		{
			BodyInstance.UseExternalCollisionProfile(BodySetup);
		}
	}
#endif

	Super::InitializeDynamicProperties();
}

const Nanite::FResources* FFastGeoStaticMeshComponentBase::GetNaniteResources() const
{
	if (GetStaticMesh() && GetStaticMesh()->GetRenderData())
	{
		return GetStaticMesh()->GetRenderData()->NaniteResourcesPtr.Get();
	}
	return nullptr;
}

UBodySetup* FFastGeoStaticMeshComponentBase::GetBodySetup() const
{
	return GetStaticMesh() ? GetStaticMesh()->GetBodySetup() : nullptr;
}

bool FFastGeoStaticMeshComponentBase::IsNavigationRelevant() const
{
	return FStaticMeshComponentHelper::IsNavigationRelevant(*this);
}

FBox FFastGeoStaticMeshComponentBase::GetNavigationBounds() const
{
	return FStaticMeshComponentHelper::GetNavigationBounds(*this);
}

void FFastGeoStaticMeshComponentBase::GetNavigationData(FNavigationRelevantData& Data) const
{
	FStaticMeshComponentHelper::GetNavigationData(*this, Data);
}

bool FFastGeoStaticMeshComponentBase::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	return FStaticMeshComponentHelper::DoCustomNavigableGeometryExport(*this, GeomExport);
}

bool FFastGeoStaticMeshComponentBase::ShouldExportAsObstacle(const UNavCollisionBase& InNavCollision) const
{
	return InNavCollision.IsDynamicObstacle();
}

int32 FFastGeoStaticMeshComponentBase::GetNumMaterials() const
{
	return GetStaticMesh() ? GetStaticMesh()->GetStaticMaterials().Num() : 0;
}

UMaterialInterface* FFastGeoStaticMeshComponentBase::GetMaterial(int32 MaterialIndex) const
{
	return GetMaterial(MaterialIndex, false);
}

UMaterialInterface* FFastGeoStaticMeshComponentBase::GetNaniteAuditMaterial(int32 MaterialIndex) const
{
	return GetMaterial(MaterialIndex, true);
}

bool FFastGeoStaticMeshComponentBase::UseNaniteOverrideMaterials(bool bDoingMaterialAudit) const
{
	return Nanite::FNaniteResourcesHelper::UseNaniteOverrideMaterials(*this, bDoingMaterialAudit);
}

bool FFastGeoStaticMeshComponentBase::ShouldCreateNaniteProxy(Nanite::FMaterialAudit* OutNaniteMaterials) const
{
	return Nanite::FNaniteResourcesHelper::ShouldCreateNaniteProxy(GetStaticMeshSceneProxyDesc(), OutNaniteMaterials);
}

UMaterialInterface* FFastGeoStaticMeshComponentBase::GetMaterial(int32 MaterialIndex, bool bDoingNaniteMaterialAudit) const
{
	return FStaticMeshComponentHelper::GetMaterial(*this, MaterialIndex, bDoingNaniteMaterialAudit);
}

const FCollisionResponseContainer& FFastGeoStaticMeshComponentBase::GetCollisionResponseToChannels() const
{
	return BodyInstance.GetResponseToChannels();
}

bool FFastGeoStaticMeshComponentBase::HasValidNaniteData() const
{
	return Nanite::FNaniteResourcesHelper::HasValidNaniteData(*this);
}

#if WITH_EDITOR
void FFastGeoStaticMeshComponentBase::InitializeSceneProxyDescFromComponent(UActorComponent* Component)
{
	UStaticMeshComponent* StaticMeshComponent = CastChecked<UStaticMeshComponent>(Component);
	FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	SceneProxyDesc.InitializeFromStaticMeshComponent(StaticMeshComponent);
}

void FFastGeoStaticMeshComponentBase::InitializeFromComponent(UActorComponent* Component)
{
	Super::InitializeFromComponent(Component);

	UStaticMeshComponent* StaticMeshComponent = CastChecked<UStaticMeshComponent>(Component);
	bUseDefaultCollision = StaticMeshComponent->bUseDefaultCollision;
	LocalBounds = GetStaticMesh()->GetBounds();
	WorldBounds = LocalBounds.TransformBy(WorldTransform);
}

UClass* FFastGeoStaticMeshComponentBase::GetEditorProxyClass() const
{
	return UFastGeoStaticMeshComponentEditorProxy::StaticClass();
}

void FFastGeoStaticMeshComponentBase::ResetSceneProxyDescUnsupportedProperties()
{
	Super::ResetSceneProxyDescUnsupportedProperties();

	// Unsupported properties
	FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	SceneProxyDesc.LODData = TArrayView<struct FStaticMeshComponentLODInfo>();
	SceneProxyDesc.LODParentPrimitive = nullptr;
	SceneProxyDesc.MeshPaintTexture = nullptr;
	SceneProxyDesc.MeshPaintTextureCoordinateIndex = 0;
#if STATICMESH_ENABLE_DEBUG_RENDERING
	SceneProxyDesc.bDrawMeshCollisionIfComplex = false;
	SceneProxyDesc.bDrawMeshCollisionIfSimple = false;
#endif
	SceneProxyDesc.bDisplayNaniteFallbackMesh = false;
	SceneProxyDesc.SectionIndexPreview = INDEX_NONE;
	SceneProxyDesc.MaterialIndexPreview = INDEX_NONE;
	SceneProxyDesc.SelectedEditorMaterial = INDEX_NONE;
	SceneProxyDesc.SelectedEditorSection = INDEX_NONE;

#if WITH_EDITORONLY_DATA
	SceneProxyDesc.MaterialStreamingRelativeBoxes = TArrayView<uint32>();
#endif

	// Properties that will be initialized by InitializeSceneProxyDescDynamicProperties
	SceneProxyDesc.NaniteResources = nullptr;
	SceneProxyDesc.BodySetup = nullptr;
	SceneProxyDesc.MaterialRelevance = FMaterialRelevance();
	SceneProxyDesc.bUseProvidedMaterialRelevance = false;
}
#endif

void FFastGeoStaticMeshComponentBase::ApplyWorldTransform(const FTransform& InTransform)
{
	Super::ApplyWorldTransform(InTransform);

	WorldBounds = LocalBounds.TransformBy(WorldTransform);
}

void FFastGeoStaticMeshComponentBase::InitializeSceneProxyDescDynamicProperties()
{
	Super::InitializeSceneProxyDescDynamicProperties();

	// Initialize non-serialized properties
	FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	SceneProxyDesc.OverrideMaterials = OverrideMaterials;
	SceneProxyDesc.NaniteResources = GetNaniteResources();
	SceneProxyDesc.BodySetup = GetBodySetup();
	SceneProxyDesc.SetMaterialRelevance(GetMaterialRelevance(GetScene()->GetFeatureLevel()));
	SceneProxyDesc.SetCollisionResponseToChannels(GetCollisionResponseToChannels());

	// Add LODData support
	// Add LODParentPrimitive support
	// Add MeshPaintTexture/MeshPaintTextureCoordinateIndex support
}

FPrimitiveSceneProxy* FFastGeoStaticMeshComponentBase::CreateSceneProxy(ESceneProxyCreationError* OutError)
{
	check(GetWorld());
	FSceneInterface* Scene = GetScene();
	UStaticMesh* StaticMesh = GetStaticMesh();
	check(Scene);
	check(StaticMesh);
	check(StaticMesh->GetRenderData());
	check(StaticMesh->GetRenderData()->IsInitialized());
	check(!StaticMesh->IsCompiling());

	if (OutError)
	{
		*OutError = ESceneProxyCreationError::None;
	}

	InitializeSceneProxyDescDynamicProperties();

	const FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	check(SceneProxyDesc.Scene);
	check(SceneProxyDesc.Scene == Scene);
	check(SceneProxyDesc.World == Scene->GetWorld());
	check(SceneProxyDesc.FeatureLevel == Scene->GetFeatureLevel());
	check(SceneProxyDesc.ComponentId == GetPrimitiveSceneId());

	FStaticMeshComponentHelper::ESceneProxyCreationError SceneProxyCreationError;
	FPrimitiveSceneProxy* SceneProxy = FStaticMeshComponentHelper::CreateSceneProxy<FFastGeoStaticMeshComponentBase, /*bRenderDataReady=*/true>(*this, &SceneProxyCreationError);

	if (SceneProxy == nullptr && OutError)
	{
		switch (SceneProxyCreationError)
		{
		case FStaticMeshComponentHelper::ESceneProxyCreationError::WaitingPSOs:
			*OutError = ESceneProxyCreationError::WaitingPSOs;
			break;

		default:
			*OutError = ESceneProxyCreationError::InvalidMesh;
			break;
		}
	}

	return SceneProxy;
}

FPrimitiveSceneProxy* FFastGeoStaticMeshComponentBase::CreateStaticMeshSceneProxy(const Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite)
{
	check(GetWorld());
	const FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	check(SceneProxyDesc.Scene);

	if (bCreateNanite)
	{
		PrimitiveSceneData.SceneProxy = ::new Nanite::FSceneProxy(NaniteMaterials, SceneProxyDesc);
	}
	else
	{
		PrimitiveSceneData.SceneProxy = ::new FStaticMeshSceneProxy(SceneProxyDesc, false);
	}
	return PrimitiveSceneData.SceneProxy;
}

void FFastGeoStaticMeshComponentBase::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	FStaticMeshComponentHelper::GetUsedMaterials(*this, OutMaterials, bGetDebugMaterials);
}

void FFastGeoStaticMeshComponentBase::GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& OutMaterialSlotOverlayMaterials) const
{
	//We add null entry for every section of every LOD, this is a requirement for the MeshComponent this class derived from
	OutMaterialSlotOverlayMaterials.Reset();
	if (GetStaticMesh())
	{
		const TArray<FStaticMaterial>& StaticMaterials = GetStaticMesh()->GetStaticMaterials();
		for (const FStaticMaterial& StaticMaterial : StaticMaterials)
		{
			OutMaterialSlotOverlayMaterials.Add(StaticMaterial.OverlayMaterialInterface);
		}
	}
}

const TArray<TObjectPtr<UMaterialInterface>>& FFastGeoStaticMeshComponentBase::GetComponentMaterialSlotsOverlayMaterial() const
{
	return GetStaticMeshSceneProxyDesc().MaterialSlotsOverlayMaterial;
}

UStaticMesh* FFastGeoStaticMeshComponentBase::GetStaticMesh() const
{
	return GetStaticMeshSceneProxyDesc().StaticMesh;
}

UMaterialInterface* FFastGeoStaticMeshComponentBase::GetOverlayMaterial() const
{
	return GetStaticMeshSceneProxyDesc().OverlayMaterial;
}

bool FFastGeoStaticMeshComponentBase::IsReverseCulling() const
{
	return GetStaticMeshSceneProxyDesc().IsReverseCulling();
}

bool FFastGeoStaticMeshComponentBase::IsDisallowNanite() const
{
	return GetStaticMeshSceneProxyDesc().IsDisallowNanite();
}

bool FFastGeoStaticMeshComponentBase::IsForceDisableNanite() const
{
	return GetStaticMeshSceneProxyDesc().IsForceDisableNanite();
}

bool FFastGeoStaticMeshComponentBase::IsForceNaniteForMasked() const
{
	return GetStaticMeshSceneProxyDesc().IsForceNaniteForMasked();
}

int32 FFastGeoStaticMeshComponentBase::GetForcedLodModel() const
{
	return GetStaticMeshSceneProxyDesc().GetForcedLodModel();
}

bool FFastGeoStaticMeshComponentBase::GetOverrideMinLOD() const
{
	return GetStaticMeshSceneProxyDesc().bOverrideMinLOD;
}

int32 FFastGeoStaticMeshComponentBase::GetMinLOD() const
{
	return GetStaticMeshSceneProxyDesc().MinLOD;
}

bool FFastGeoStaticMeshComponentBase::GetForceNaniteForMasked() const
{
	return GetStaticMeshSceneProxyDesc().bForceNaniteForMasked;
}

#if WITH_EDITORONLY_DATA
bool FFastGeoStaticMeshComponentBase::IsDisplayNaniteFallbackMesh() const
{
	return GetStaticMeshSceneProxyDesc().IsDisplayNaniteFallbackMesh();
}
#endif

//~ Begin IPhysicsBodyInstanceOwner interface
UPhysicalMaterial* FFastGeoStaticMeshComponentBase::GetPhysicalMaterial() const
{
	if (UMaterialInterface* Material = GetMaterial(0))
	{
		return Material->GetPhysicalMaterial();
	}
	return nullptr;
}

void FFastGeoStaticMeshComponentBase::GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& OutPhysMaterials, TArray<FPhysicalMaterialMaskParams>* OutPhysMaterialMasks) const
{
	if (BodyInstance.GetPhysMaterialOverride() != nullptr)
	{
		OutPhysMaterials.SetNum(1);
		OutPhysMaterials[0] = BodyInstance.GetPhysMaterialOverride();
		check(!OutPhysMaterials[0] || OutPhysMaterials[0]->IsValidLowLevel());
	}
	else
	{
		// See if the Material has a PhysicalMaterial
		const int32 NumMaterials = GetNumMaterials();
		OutPhysMaterials.SetNum(NumMaterials);

		if (OutPhysMaterialMasks)
		{
			OutPhysMaterialMasks->SetNum(NumMaterials);
		}

		for (int32 MatIdx = 0; MatIdx < NumMaterials; MatIdx++)
		{
			UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
			UMaterialInterface* Material = GetMaterial(MatIdx);
			if (Material)
			{
				PhysMat = Material->GetPhysicalMaterial();
			}

			OutPhysMaterials[MatIdx] = PhysMat;

			if (OutPhysMaterialMasks)
			{
				UPhysicalMaterialMask* PhysMatMask = nullptr;
				UMaterialInterface* PhysMatMap = nullptr;

				if (Material)
				{
					PhysMatMask = Material->GetPhysicalMaterialMask();
					if (PhysMatMask)
					{
						PhysMatMap = Material;
					}
				}

				(*OutPhysMaterialMasks)[MatIdx].PhysicalMaterialMask = PhysMatMask;
				(*OutPhysMaterialMasks)[MatIdx].PhysicalMaterialMap = PhysMatMap;
			}
		}
	}
}
//~ End IPhysicsBodyInstanceOwner interface


#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
void FFastGeoStaticMeshComponentBase::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	FStaticMeshComponentHelper::CollectPSOPrecacheData(*this, BasePrecachePSOParams, OutParams);
}
#endif

#if WITH_EDITOR

void UFastGeoStaticMeshComponentEditorProxy::NotifyRenderStateChanged()
{
	Super::NotifyRenderStateChanged();

	FObjectCacheEventSink::NotifyStaticMeshChanged_Concurrent(this);
}

//~ Begin IStaticMeshComponent interface
void UFastGeoStaticMeshComponentEditorProxy::OnMeshRebuild(bool bRenderDataChanged)
{
}

void UFastGeoStaticMeshComponentEditorProxy::PreStaticMeshCompilation()
{
}

void UFastGeoStaticMeshComponentEditorProxy::PostStaticMeshCompilation()
{
}

UStaticMesh* UFastGeoStaticMeshComponentEditorProxy::GetStaticMesh() const
{
	return GetComponent<ComponentType>().GetStaticMesh();
}
//~ End IStaticMeshComponent interface

#endif

const FFastGeoElementType FFastGeoStaticMeshComponent::Type(&FFastGeoStaticMeshComponentBase::Type);

FFastGeoStaticMeshComponent::FFastGeoStaticMeshComponent(int32 InComponentIndex, FFastGeoElementType InType)
	: Super(InComponentIndex, InType)
{
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshSceneProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/MaterialOverlayHelper.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "RayTracingInstance.h"
#include "Rendering/RenderCommandPipes.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "SkeletalDebugRendering.h"
#include "SkeletalRenderPublic.h"
#include "SkinnedMeshSceneProxyDesc.h"
#include "UnrealEngine.h"
#include "MeshCardBuild.h"
#include "ComponentRecreateRenderStateContext.h"

#if WITH_EDITOR
#include "Components/BrushComponent.h"
#include "Engine/Brush.h"
#include "MeshPaintVisualize.h"
#endif

DECLARE_CYCLE_STAT(TEXT("GetShadowShapes"), STAT_GetShadowShapes, STATGROUP_Anim);

TAutoConsoleVariable<int32> CVarDebugDrawSimpleBones(TEXT("a.DebugDrawSimpleBones"), 0, TEXT("When drawing bones (using Show Bones), draw bones as simple lines."));
TAutoConsoleVariable<int32> CVarDebugDrawBoneAxes(TEXT("a.DebugDrawBoneAxes"), 0, TEXT("When drawing bones (using Show Bones), draw bone axes."));

static TAutoConsoleVariable<int32> CVarRayTracingSkeletalMeshes(
	TEXT("r.RayTracing.Geometry.SkeletalMeshes"),
	1,
	TEXT("Include skeletal meshes in ray tracing effects (default = 1 (skeletal meshes enabled in ray tracing))"));

static TAutoConsoleVariable<int32> CVarRayTracingSupportSkeletalMeshes(
	TEXT("r.RayTracing.Geometry.SupportSkeletalMeshes"),
	1,
	TEXT("Whether the project supports skeletal meshes in ray tracing effects. ")
	TEXT("Turning this off disables creation of all skeletal mesh ray tracing GPU resources, saving GPU memory and time. ")
	TEXT("This setting is read-only at runtime. (default: 1)"),
	ECVF_ReadOnly);

bool GSkeletalMeshUseCachedMDCs = true;
static FAutoConsoleVariableRef CVarSkeletalMeshUseCachedMDCs(
	TEXT("r.SkeletalMesh.UseCachedMDCs"),
	GSkeletalMeshUseCachedMDCs,
	TEXT("Whether skeletal meshes will take the cached MDC path."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMeshCardRepresentationSkeletalMesh(
	TEXT("r.MeshCardRepresentation.SkeletalMesh"),
	1,
	TEXT("Whether to allow generating mesh cards for skeletal meshes."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable) { FGlobalComponentRecreateRenderStateContext Context; }),
	ECVF_RenderThreadSafe);

const FQuat SphylBasis(FVector(1.0f / FMath::Sqrt(2.0f), 0.0f, 1.0f / FMath::Sqrt(2.0f)), UE_PI);

bool AllowLumenCardGenerationForSkeletalMeshes(EShaderPlatform Platform)
{
	return CVarMeshCardRepresentationSkeletalMesh.GetValueOnAnyThread() != 0
		&& DoesPlatformSupportLumenGI(Platform);
}

/** 
 * Constructor. 
 * @param	Component - skeletal mesh primitive being added
 */
FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshRenderData* InSkelMeshRenderData)
	: FSkeletalMeshSceneProxy(FSkinnedMeshSceneProxyDesc(Component), InSkelMeshRenderData)
{ }

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(const FSkinnedMeshSceneProxyDesc& InMeshDesc, FSkeletalMeshRenderData* InSkelMeshRenderData)
		:	FPrimitiveSceneProxy(InMeshDesc, InMeshDesc.GetSkinnedAsset()->GetFName())
		,	Owner(Cast<AActor>(InMeshDesc.GetOwner()))
		,	MeshObject(InMeshDesc.MeshObject)
		,	SkeletalMeshRenderData(InSkelMeshRenderData)
		,	SkeletalMeshForDebug(InMeshDesc.GetSkinnedAsset())
		,	PhysicsAssetForDebug(InMeshDesc.GetPhysicsAsset())
		,	OverlayMaterial(InMeshDesc.GetOverlayMaterial())
		,	OverlayMaterialMaxDrawDistance(InMeshDesc.GetOverlayMaterialMaxDrawDistance())
#if RHI_RAYTRACING
		,	bAnySegmentUsesWorldPositionOffset(false)
#endif
		,	bForceWireframe(InMeshDesc.bForceWireframe)
		,	bCanHighlightSelectedSections(InMeshDesc.bCanHighlightSelectedSections)
		,	bRenderStatic(InMeshDesc.bRenderStatic)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		,	bDrawDebugSkeleton(InMeshDesc.ShouldDrawDebugSkeleton())
#endif
		,	FeatureLevel(GetScene().GetFeatureLevel())
		,	bMaterialsNeedMorphUsage_GameThread(false)
		,	MaterialRelevance(InMeshDesc.GetMaterialRelevance(GetScene().GetFeatureLevel()))
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		,	DebugDrawColor(InMeshDesc.GetDebugDrawColor())
#endif
#if WITH_EDITORONLY_DATA
		,	StreamingDistanceMultiplier(FMath::Max(0.0f, InMeshDesc.StreamingDistanceMultiplier))
#endif
{
	check(MeshObject);
	check(SkeletalMeshRenderData);
	check(SkeletalMeshForDebug);

#if WITH_EDITOR
	PoseWatchDynamicData = nullptr;
#endif

	// Skinning is supported by this proxy
	bSkinnedMesh = true;

	// Skeletal meshes DO deform internally, unless bRenderStatic is used to force static mesh behaviour.
	bHasDeformableMesh = !bRenderStatic;

	bIsCPUSkinned = MeshObject->IsCPUSkinned();

	bCastCapsuleDirectShadow = InMeshDesc.bCastDynamicShadow && InMeshDesc.CastShadow && InMeshDesc.bCastCapsuleDirectShadow && !InMeshDesc.bIsFirstPerson;
	bCastsDynamicIndirectShadow = InMeshDesc.bCastDynamicShadow && InMeshDesc.CastShadow && InMeshDesc.bCastCapsuleIndirectShadow && !InMeshDesc.bIsFirstPerson;

	DynamicIndirectShadowMinVisibility = FMath::Clamp(InMeshDesc.CapsuleIndirectShadowMinVisibility, 0.0f, 1.0f);

	// Force inset shadows if capsule shadows are requested, as they can't be supported with full scene shadows
	bCastInsetShadow = bCastInsetShadow || bCastCapsuleDirectShadow;

	// Get the pre-skinned local bounds
	InMeshDesc.GetPreSkinnedLocalBounds(PreSkinnedLocalBounds);

	InMeshDesc.GetMaterialSlotsOverlayMaterial(MaterialSlotsOverlayMaterial);

	if(InMeshDesc.bPerBoneMotionBlur)
	{
		bAlwaysHasVelocity = true;
	}

	const bool bForceDefaultMaterial = InMeshDesc.ShouldRenderProxyFallbackToDefaultMaterial();

	// Enable dynamic triangle reordering to remove/reduce sorting issue when rendered with a translucent material (i.e., order-independent-transparency)
	bSupportsSortedTriangles = InMeshDesc.bSortTriangles;

	// setup materials and performance classification for each LOD.
	extern bool GForceDefaultMaterial;
	const bool bCastShadow = InMeshDesc.CastShadow;
	bool bAnySectionCastsShadow = false;
	LODSections.Reserve(SkeletalMeshRenderData->LODRenderData.Num());
	LODSections.AddZeroed(SkeletalMeshRenderData->LODRenderData.Num());
	for(int32 LODIdx=0; LODIdx < SkeletalMeshRenderData->LODRenderData.Num(); LODIdx++)
	{
		const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIdx];
		const FSkeletalMeshLODInfo& Info = *(InMeshDesc.GetSkinnedAsset()->GetLODInfo(LODIdx));

		FLODSectionElements& LODSection = LODSections[LODIdx];

		// Presize the array
		LODSection.SectionElements.Empty(LODData.RenderSections.Num() );
		for(int32 SectionIndex = 0;SectionIndex < LODData.RenderSections.Num();SectionIndex++)
		{
			const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

			// If we are at a dropped LOD, route material index through the LODMaterialMap in the LODInfo struct.
			int32 UseMaterialIndex = Section.MaterialIndex;			
			{
				if(SectionIndex < Info.LODMaterialMap.Num() && InMeshDesc.GetSkinnedAsset()->IsValidMaterialIndex(Info.LODMaterialMap[SectionIndex]))
				{
					UseMaterialIndex = Info.LODMaterialMap[SectionIndex];
					UseMaterialIndex = FMath::Clamp( UseMaterialIndex, 0, InMeshDesc.GetSkinnedAsset()->GetNumMaterials());
				}
			}

			// If Section is hidden, do not cast shadow
			const bool bSectionHidden = MeshObject->IsMaterialHidden(LODIdx,UseMaterialIndex);

			// If the material is NULL, or isn't flagged for use with skeletal meshes, it will be replaced by the default material.
			UMaterialInterface* Material = InMeshDesc.GetMaterial(UseMaterialIndex);
			if (bForceDefaultMaterial || (GForceDefaultMaterial && Material && !IsTranslucentBlendMode(*Material)))
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			// if this is a clothing section, then enabled and will be drawn but the corresponding original section should be disabled
			const bool bClothSection = Section.HasClothingData();

			bool bValidUsage = Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
			if (bClothSection)
			{
				bValidUsage &= Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_Clothing);
			}

			if(!Material || !bValidUsage)
			{
				UE_CLOG(Material && !bValidUsage, LogSkeletalMesh, Warning,
					TEXT("Material with missing usage flag was applied to skeletal mesh %s"),
					*InMeshDesc.GetSkinnedAsset()->GetPathName());

				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			const bool bSectionCastsShadow = !bSectionHidden && bCastShadow &&
				(InMeshDesc.GetSkinnedAsset()->IsValidMaterialIndex(UseMaterialIndex) == false || Section.bCastShadow);

			bAnySectionCastsShadow |= bSectionCastsShadow;

#if RHI_RAYTRACING
			bAnySegmentUsesWorldPositionOffset |= MaterialRelevance.bUsesWorldPositionOffset;
#endif

			UMaterialInterface* SectionOverlayMaterial = FMaterialOverlayHelper::GetOverlayMaterial(MaterialSlotsOverlayMaterial, UseMaterialIndex);

			if (SectionOverlayMaterial != nullptr)
			{
				if (!SectionOverlayMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh))
				{
					SectionOverlayMaterial = nullptr;
					UE_LOG(LogSkeletalMesh, Warning, TEXT("Overlay material per section with missing usage flag was applied to skeletal mesh[%s] LOD %d, section index %d."), *InMeshDesc.GetSkinnedAsset()->GetPathName(), LODIdx, SectionIndex);
					FMaterialOverlayHelper::ForceMaterial(MaterialSlotsOverlayMaterial, UseMaterialIndex, SectionOverlayMaterial);
				}
				else if (bForceDefaultMaterial)
				{
					SectionOverlayMaterial = nullptr;
					FMaterialOverlayHelper::ForceMaterial(MaterialSlotsOverlayMaterial, UseMaterialIndex, SectionOverlayMaterial);
				}
			}

			LODSection.SectionElements.Add(
				FSectionElementInfo(
					Material,
					bSectionCastsShadow,
					UseMaterialIndex,
					SectionOverlayMaterial
					));
			MaterialsInUse_GameThread.Add(Material);
		}
	}

	if (OverlayMaterial != nullptr)
	{
		if (!OverlayMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh))
		{
			OverlayMaterial = nullptr;
			UE_LOG(LogSkeletalMesh, Warning, TEXT("Overlay material with missing usage flag was applied to skeletal mesh %s"),	*InMeshDesc.GetSkinnedAsset()->GetPathName());
		}
		else if (bForceDefaultMaterial)
		{
			OverlayMaterial = nullptr;
		}
	}

	bCastDynamicShadow = bCastDynamicShadow && bAnySectionCastsShadow;

	// Copy out shadow physics asset data
	if (UPhysicsAsset* ShadowPhysicsAsset = InMeshDesc.GetSkinnedAsset()->GetShadowPhysicsAsset())
	{
		if (InMeshDesc.CastShadow && (InMeshDesc.bCastCapsuleDirectShadow || InMeshDesc.bCastCapsuleIndirectShadow))
		{
			for (int32 BodyIndex = 0; BodyIndex < ShadowPhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++)
			{
				const UBodySetup* BodySetup = ShadowPhysicsAsset->SkeletalBodySetups[BodyIndex];
				const int32 BoneIndex = InMeshDesc.GetBoneIndex(BodySetup->BoneName);

				if (BoneIndex != INDEX_NONE)
				{
					const FMatrix& RefBoneMatrix = InMeshDesc.GetSkinnedAsset()->GetComposedRefPoseMatrix(BoneIndex);

					const int32 NumSpheres = BodySetup->AggGeom.SphereElems.Num();
					for (int32 SphereIndex = 0; SphereIndex < NumSpheres; SphereIndex++)
					{
						const FKSphereElem& SphereShape = BodySetup->AggGeom.SphereElems[SphereIndex];
						ShadowCapsuleData.Emplace(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphereShape.Center), SphereShape.Radius, FVector(0.0f, 0.0f, 1.0f), 0.0f));
					}

					const int32 NumCapsules = BodySetup->AggGeom.SphylElems.Num();
					for (int32 CapsuleIndex = 0; CapsuleIndex < NumCapsules; CapsuleIndex++)
					{
						const FKSphylElem& SphylShape = BodySetup->AggGeom.SphylElems[CapsuleIndex];
						ShadowCapsuleData.Emplace(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphylShape.Center), SphylShape.Radius, RefBoneMatrix.TransformVector((SphylShape.Rotation.Quaternion() * SphylBasis).Vector()), SphylShape.Length));
					}

					if (NumSpheres > 0 || NumCapsules > 0)
					{
						ShadowCapsuleBoneIndices.AddUnique(BoneIndex);
					}
				}
			}
		}
	}

	// Sort to allow merging with other bone hierarchies
	if (ShadowCapsuleBoneIndices.Num())
	{
		ShadowCapsuleBoneIndices.Sort();
	}

	EnableGPUSceneSupportFlags();

	if (IsAllowingApproximateOcclusionQueries())
	{		
		bAllowApproximateOcclusion = (bAllowApproximateOcclusion || bRenderStatic);
	}

	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;
	bOpaqueOrMasked = MaterialRelevance.bOpaque || MaterialRelevance.bMasked;
	bSupportsMaterialCache = MaterialRelevance.bSupportsMaterialCache;

	UpdateVisibleInLumenScene();
	UpdateLumenCardsFromBounds();
}

FSkeletalMeshSceneProxy::~FSkeletalMeshSceneProxy()
{
	if (CardRepresentationData)
	{
		delete CardRepresentationData;
		CardRepresentationData = nullptr;
	}
}


// FPrimitiveSceneProxy interface.

/** 
 * Iterates over sections,chunks,elements based on current instance weight usage 
 */
class FSkeletalMeshSectionIter
{
public:
	FSkeletalMeshSectionIter(const int32 InLODIdx, const FSkeletalMeshObject& InMeshObject, const FSkeletalMeshLODRenderData& InLODData, const FSkeletalMeshSceneProxy::FLODSectionElements& InLODSectionElements, bool bIgnorePreviewFilter = false)
		: SectionIndex(0)
		, MeshObject(InMeshObject)
		, LODSectionElements(InLODSectionElements)
		, Sections(InLODData.RenderSections)
#if WITH_EDITORONLY_DATA
		, SectionIndexPreview(bIgnorePreviewFilter ? INDEX_NONE : InMeshObject.SectionIndexPreview)
		, MaterialIndexPreview(bIgnorePreviewFilter ? INDEX_NONE : InMeshObject.MaterialIndexPreview)
#endif
	{
		while (NotValidPreviewSection())
		{
			SectionIndex++;
		}
	}
	FORCEINLINE FSkeletalMeshSectionIter& operator++()
	{
		do 
		{
		SectionIndex++;
		} while (NotValidPreviewSection());
		return *this;
	}
	FORCEINLINE explicit operator bool() const
	{
		return ((SectionIndex < Sections.Num()) && LODSectionElements.SectionElements.IsValidIndex(GetSectionElementIndex()));
	}
	FORCEINLINE const FSkelMeshRenderSection& GetSection() const
	{
		return Sections[SectionIndex];
	}
	FORCEINLINE const int32 GetSectionElementIndex() const
	{
		return SectionIndex;
	}
	FORCEINLINE const FSkeletalMeshSceneProxy::FSectionElementInfo& GetSectionElementInfo() const
	{
		int32 SectionElementInfoIndex = GetSectionElementIndex();
		return LODSectionElements.SectionElements[SectionElementInfoIndex];
	}
	FORCEINLINE bool NotValidPreviewSection()
	{
#if WITH_EDITORONLY_DATA
		if (MaterialIndexPreview == INDEX_NONE)
		{
			int32 ActualPreviewSectionIdx = SectionIndexPreview;

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewSectionIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
		else
		{
			int32 ActualPreviewMaterialIdx = MaterialIndexPreview;
			int32 ActualPreviewSectionIdx = INDEX_NONE;
			if (ActualPreviewMaterialIdx != INDEX_NONE && Sections.IsValidIndex(SectionIndex))
			{
				const FSkeletalMeshSceneProxy::FSectionElementInfo& SectionInfo = LODSectionElements.SectionElements[SectionIndex];
				if (SectionInfo.UseMaterialIndex == ActualPreviewMaterialIdx)
				{
					ActualPreviewSectionIdx = SectionIndex;
				}
			}

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewMaterialIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
#else
		return false;
#endif
	}
private:
	int32 SectionIndex;
	const FSkeletalMeshObject& MeshObject;
	const FSkeletalMeshSceneProxy::FLODSectionElements& LODSectionElements;
	const TArray<FSkelMeshRenderSection>& Sections;
#if WITH_EDITORONLY_DATA
	const int32 SectionIndexPreview;
	const int32 MaterialIndexPreview;
#endif
};

#if WITH_EDITOR
HHitProxy* FSkeletalMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	if ( Component->GetOwner() )
	{
		if ( LODSections.Num() > 0 )
		{
			for ( int32 LODIndex = 0; LODIndex < SkeletalMeshRenderData->LODRenderData.Num(); LODIndex++ )
			{
				const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

				FLODSectionElements& LODSection = LODSections[LODIndex];

				check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());

				for ( int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); SectionIndex++ )
				{
					HHitProxy* ActorHitProxy;

					int32 MaterialIndex = LODData.RenderSections[SectionIndex].MaterialIndex;
					if ( Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()) )
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe, SectionIndex, MaterialIndex);
					}
					else
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, Component->HitProxyPriority, SectionIndex, MaterialIndex);
					}

					// Set the hitproxy.
					check(LODSection.SectionElements[SectionIndex].HitProxy == NULL);
					LODSection.SectionElements[SectionIndex].HitProxy = ActorHitProxy;
					OutHitProxies.Add(ActorHitProxy);
				}
			}
		}
		else
		{
			return FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
		}
	}

	return NULL;
}
#endif

void FSkeletalMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	if (!MeshObject)
	{
		return;
	}

	if (!HasViewDependentDPG())
	{
		ESceneDepthPriorityGroup PrimitiveDPG = GetStaticDepthPriorityGroup();
		bool bUseSelectedMaterial = false;

		int32 NumLODs = SkeletalMeshRenderData->LODRenderData.Num();
		int32 ClampedMinLOD = 0; // TODO: MinLOD, Bias?

		for (int32 LODIndex = ClampedMinLOD; LODIndex < NumLODs; ++LODIndex)
		{
			const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

			if (LODSections.Num() > 0 && LODData.GetNumVertices() > 0)
			{
				float ScreenSize = MeshObject->GetScreenSize(LODIndex);
				const FLODSectionElements& LODSection = LODSections[LODIndex];
				check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());

				for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection); Iter; ++Iter)
				{
					const FSkelMeshRenderSection& Section = Iter.GetSection();
					const int32 SectionIndex = Iter.GetSectionElementIndex();
					const FVertexFactory* VertexFactory = MeshObject->GetStaticSkinVertexFactory(LODIndex, SectionIndex, ESkinVertexFactoryMode::Default);

					if (!VertexFactory)
					{
						// hide this part
						continue;
					}

					const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

					// If hidden skip the draw
					if (MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex) || Section.bDisabled)
					{
						continue;
					}

				#if WITH_EDITOR
					if (GIsEditor)
					{
						bUseSelectedMaterial = (MeshObject->SelectedEditorSection == SectionIndex);
						PDI->SetHitProxy(SectionElementInfo.HitProxy);
					}
				#endif // WITH_EDITOR

					FMeshBatch MeshElement;
					FMeshBatchElement& BatchElement = MeshElement.Elements[0];
					MeshElement.DepthPriorityGroup = PrimitiveDPG;
					MeshElement.VertexFactory = VertexFactory;
					MeshElement.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy();
					MeshElement.ReverseCulling = IsLocalToWorldDeterminantNegative();
					MeshElement.CastShadow = SectionElementInfo.bEnableShadowCasting;
				#if RHI_RAYTRACING
					MeshElement.CastRayTracedShadow = MeshElement.CastShadow && bCastDynamicShadow;
				#endif
					MeshElement.Type = PT_TriangleList;
					MeshElement.LODIndex = LODIndex;
					MeshElement.SegmentIndex = SectionIndex;
					MeshElement.MeshIdInPrimitive = SectionIndex;

					BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
					BatchElement.FirstIndex = Section.BaseIndex;
					BatchElement.MinVertexIndex = Section.BaseVertexIndex;
					BatchElement.MaxVertexIndex = LODData.GetNumVertices() - 1;
					BatchElement.NumPrimitives = Section.NumTriangles;
					BatchElement.IndexBuffer = LODData.MultiSizeIndexContainer.GetIndexBuffer();

					PDI->DrawMesh(MeshElement, ScreenSize);

					UMaterialInterface* SectionOverlayMaterial = (SectionElementInfo.PerSectionOverlayMaterial != nullptr) ? SectionElementInfo.PerSectionOverlayMaterial : OverlayMaterial;
					if (SectionOverlayMaterial != nullptr)
					{
						FMeshBatch OverlayMeshBatch(MeshElement);
						OverlayMeshBatch.bOverlayMaterial = true;
						OverlayMeshBatch.CastShadow = false;
						OverlayMeshBatch.bSelectable = false;
						OverlayMeshBatch.MaterialRenderProxy = SectionOverlayMaterial->GetRenderProxy();
						// make sure overlay is always rendered on top of base mesh
						OverlayMeshBatch.MeshIdInPrimitive += LODData.RenderSections.Num();
						// Reuse mesh ScreenSize as cull distance for an overlay. Overlay does not need to compute LOD so we can avoid adding new members into MeshBatch or MeshRelevance
						float OverlayMeshScreenSize = OverlayMaterialMaxDrawDistance;
						PDI->DrawMesh(OverlayMeshBatch, OverlayMeshScreenSize);
					}
				}
			}
		}
	}
}

void FSkeletalMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshSceneProxy_GetMeshElements);
	GetMeshElementsConditionallySelectable(Views, ViewFamily, true, VisibilityMap, Collector);
}

void FSkeletalMeshSceneProxy::GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if (!MeshObject)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SkeletalMesh);

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

	const int32 FirstLODIdx = SkeletalMeshRenderData->GetFirstValidLODIdx(SkeletalMeshRenderData->CurrentFirstLODIdx);
	if (FirstLODIdx == INDEX_NONE)
	{
#if DO_CHECK
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Skeletal mesh %s has no valid LODs for rendering."), *GetResourceName().ToString());
#endif
	}
	else
	{
		const int32 LODIndex = MeshObject->GetLOD();
		check(LODIndex < SkeletalMeshRenderData->LODRenderData.Num());
		const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

		if (LODSections.Num() > 0 && LODIndex >= FirstLODIdx)
		{
			check(SkeletalMeshRenderData->LODRenderData[LODIndex].GetNumVertices() > 0);

			const FLODSectionElements& LODSection = LODSections[LODIndex];

			check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());

			for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection); Iter; ++Iter)
			{
				const FSkelMeshRenderSection& Section = Iter.GetSection();
				const int32 SectionIndex = Iter.GetSectionElementIndex();
				const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

				bool bSectionSelected = false;

#if WITH_EDITORONLY_DATA
				// TODO: This is not threadsafe! A render command should be used to propagate SelectedEditorSection to the scene proxy.
				if (MeshObject->SelectedEditorMaterial != INDEX_NONE)
				{
					bSectionSelected = (MeshObject->SelectedEditorMaterial == SectionElementInfo.UseMaterialIndex);
				}
				else
				{
					bSectionSelected = (MeshObject->SelectedEditorSection == SectionIndex);
				}
			
#endif
				// If hidden skip the draw
				if (MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex) || Section.bDisabled)
				{
					continue;
				}

				GetDynamicElementsSection(Views, ViewFamily, VisibilityMap, LODData, LODIndex, SectionIndex, bSectionSelected, SectionElementInfo, bInSelectable, Collector);
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if( PhysicsAssetForDebug )
			{
				DebugDrawPhysicsAsset(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}

			if (EngineShowFlags.MassProperties && DebugMassData.Num() > 0)
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				if (MeshObject->GetComponentSpaceTransforms())
				{
					const TArray<FTransform>& ComponentSpaceTransforms = *MeshObject->GetComponentSpaceTransforms();

					for (const FDebugMassData& DebugMass : DebugMassData)
					{
						if (ComponentSpaceTransforms.IsValidIndex(DebugMass.BoneIndex))
						{
							const FTransform BoneToWorld = ComponentSpaceTransforms[DebugMass.BoneIndex] * FTransform(GetLocalToWorld());
							DebugMass.DrawDebugMass(PDI, BoneToWorld);
						}
					}
				}
			}

			if (ViewFamily.EngineShowFlags.SkeletalMeshes)
			{
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}

			if (ViewFamily.EngineShowFlags.Bones || bDrawDebugSkeleton)
			{
				DebugDrawSkeleton(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}

#if WITH_EDITOR
			DebugDrawPoseWatchSkeletons(ViewIndex, Collector, ViewFamily.EngineShowFlags);
#endif
		}
	}
#endif
}

void FSkeletalMeshSceneProxy::UpdateLumenCardsFromBounds()
{
	if (CardRepresentationData)
	{
		delete CardRepresentationData;
		CardRepresentationData = nullptr;
	}

	if (!bVisibleInLumenScene || !AllowLumenCardGenerationForSkeletalMeshes(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		return;
	}

	CardRepresentationData = new FCardRepresentationData;
	FMeshCardsBuildData& CardData = CardRepresentationData->MeshCardsBuildData;

	CardData.Bounds = PreSkinnedLocalBounds.GetBox();
	// Skeletal meshes usually doesn't match their surface cache very well due to animation.
	// Mark as two-sided so a high sampling bias is used and hits are accepted even if they don't match well
	CardData.bMostlyTwoSided = true;

	MeshCardRepresentation::SetCardsFromBounds(CardData);
}

const FCardRepresentationData* FSkeletalMeshSceneProxy::GetMeshCardRepresentation() const
{
	return CardRepresentationData;
}

void FSkeletalMeshSceneProxy::CreateBaseMeshBatch(const FSceneView* View, const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, const FSectionElementInfo& SectionElementInfo, FMeshBatch& Mesh, ESkinVertexFactoryMode VFMode) const
{
	const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

	if (Section.bDisabled && VFMode == ESkinVertexFactoryMode::RayTracing)
	{
		// Use static skin vertex factory when section is disabled since the StaticVertexBuffers.PositionVertexBuffer is used to build BLAS 
		// (see FSkeletalMeshObjectGPUSkin::UpdateRayTracingGeometry_Internal)
		// TODO: Should also do this for ESkinVertexFactoryMode::Default?
		Mesh.VertexFactory = MeshObject->GetStaticSkinVertexFactory(LODIndex, SectionIndex, VFMode);
	}
	else
	{
		Mesh.VertexFactory = MeshObject->GetSkinVertexFactory(View, LODIndex, SectionIndex, VFMode);
	}
	Mesh.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy();
#if RHI_RAYTRACING
	Mesh.SegmentIndex = SectionIndex;
	Mesh.CastRayTracedShadow = SectionElementInfo.bEnableShadowCasting && bCastDynamicShadow;
#endif

	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.FirstIndex = Section.BaseIndex;
	BatchElement.IndexBuffer = LODData.MultiSizeIndexContainer.GetIndexBuffer();
	BatchElement.MinVertexIndex = Section.GetVertexBufferIndex();
	BatchElement.MaxVertexIndex = Section.GetVertexBufferIndex() + Section.GetNumVertices() - 1;

	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
	BatchElement.NumPrimitives = Section.NumTriangles;
}

uint8 FSkeletalMeshSceneProxy::GetCurrentFirstLODIdx_Internal() const
{
	return SkeletalMeshRenderData->CurrentFirstLODIdx;
}

FDesiredLODLevel FSkeletalMeshSceneProxy::GetDesiredLODLevel_RenderThread(const FSceneView* View) const
{
	return FDesiredLODLevel::CreateFixed(MeshObject->GetLOD());
}

bool FSkeletalMeshSceneProxy::GetCachedGeometry(FRDGBuilder& GraphBuilder, FCachedGeometry& OutCachedGeometry) const 
{
	return MeshObject != nullptr && MeshObject->GetCachedGeometry(GraphBuilder, OutCachedGeometry); 
}

void FSkeletalMeshSceneProxy::GetDynamicElementsSection(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, 
	const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, bool bSectionSelected,
	const FSectionElementInfo& SectionElementInfo, bool bInSelectable, FMeshElementCollector& Collector ) const
{
	const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

#if !WITH_EDITOR
	const bool bIsSelected = false;
#else // #if !WITH_EDITOR
	bool bIsSelected = IsSelected();

	// if the mesh isn't selected but the mesh section is selected in the AnimSetViewer, find the mesh component and make sure that it can be highlighted (ie. are we rendering for the AnimSetViewer or not?)
	if( !bIsSelected && bSectionSelected && bCanHighlightSelectedSections )
	{
		bIsSelected = true;
	}
	if (WantsEditorEffects())
	{
		bIsSelected = true;
	}
#endif // #if WITH_EDITOR

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			FMeshBatch& Mesh = Collector.AllocateMesh();

			CreateBaseMeshBatch(View, LODData, LODIndex, SectionIndex, SectionElementInfo, Mesh);
			//For dynamic mesh elements, Mesh.MeshIdInPrimitive is setup in Collector.AddMesh.
			
			if(!Mesh.VertexFactory)
			{
				// hide this part
				continue;
			}

			Mesh.bWireframe |= bForceWireframe;
			Mesh.Type = PT_TriangleList;
			Mesh.bSelectable = bInSelectable;

			FMeshBatchElement& BatchElement = Mesh.Elements[0];

		#if WITH_EDITOR
			Mesh.BatchHitProxyId = SectionElementInfo.HitProxy ? SectionElementInfo.HitProxy->Id : FHitProxyId();

			if (bSectionSelected && bCanHighlightSelectedSections)
			{
				Mesh.bUseSelectionOutline = true;
			}
			else
			{
				Mesh.bUseSelectionOutline = !bCanHighlightSelectedSections && bIsSelected;
			}
		#endif

#if WITH_EDITORONLY_DATA
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bIsSelected && ViewFamily.EngineShowFlags.VertexColors && AllowDebugViewmodes())
			{
				// Note: static mesh renderer does something more complicated involving per-section selection, but whole component selection seems ok for now.
				if (FMaterialRenderProxy* VertexColorVisualizationMaterialInstance = MeshPaintVisualize::GetMaterialRenderProxy(bIsSelected, IsHovered()))
				{
					Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
					Mesh.MaterialRenderProxy = VertexColorVisualizationMaterialInstance;
				}
			}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif // WITH_EDITORONLY_DATA

			BatchElement.MinVertexIndex = Section.BaseVertexIndex;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.CastShadow = SectionElementInfo.bEnableShadowCasting;
			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseWireframeSelectionColoring = bIsSelected;

		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			BatchElement.SkinCacheDebugColor = MeshObject->GetSkinCacheVisualizationDebugColor(View->CurrentGPUSkinCacheVisualizationMode, SectionIndex);
			BatchElement.VisualizeElementIndex = SectionIndex;
			Mesh.VisualizeLODIndex = LODIndex;
		#endif

			if (ensureMsgf(Mesh.MaterialRenderProxy, TEXT("GetDynamicElementsSection with invalid MaterialRenderProxy. Owner:%s LODIndex:%d UseMaterialIndex:%d"), *GetOwnerName().ToString(), LODIndex, SectionElementInfo.UseMaterialIndex))
			{
				Collector.AddMesh(ViewIndex, Mesh);
			}

			const int32 NumVertices = Section.GetNumVertices();
			INC_DWORD_STAT_BY(STAT_GPUSkinVertices,(uint32)(bIsCPUSkinned ? 0 : NumVertices)); // TODO: Nanite-Skinning
			INC_DWORD_STAT_BY(STAT_SkelMeshTriangles,Mesh.GetNumPrimitives());
			INC_DWORD_STAT(STAT_SkelMeshDrawCalls);

			UMaterialInterface* ActiveOverlayMaterial = SectionElementInfo.PerSectionOverlayMaterial ? SectionElementInfo.PerSectionOverlayMaterial : OverlayMaterial;
			// negative cull distance disables overlay rendering
			if (ActiveOverlayMaterial != nullptr && OverlayMaterialMaxDrawDistance >= 0.f)
			{
				const bool bHasOverlayCullDistance = 
					OverlayMaterialMaxDrawDistance > 0.f && 
					OverlayMaterialMaxDrawDistance != FLT_MAX && 
					!ViewFamily.EngineShowFlags.DistanceCulledPrimitives;
				
				bool bAddOverlay = true;
				if (bHasOverlayCullDistance)
				{
					// this is already combined with ViewDistanceScale
					float MaxDrawDistanceScale = GetCachedScalabilityCVars().SkeletalMeshOverlayDistanceScale;
					MaxDrawDistanceScale *= GetCachedScalabilityCVars().CalculateFieldOfViewDistanceScale(View->DesiredFOV);
					float DistanceSquared = (GetBounds().Origin - View->ViewMatrices.GetViewOrigin()).SizeSquared();
					if (DistanceSquared > FMath::Square(OverlayMaterialMaxDrawDistance * MaxDrawDistanceScale))
					{
						// distance culled
						bAddOverlay = false;
					}
				}
				
				if (bAddOverlay)
				{
					FMeshBatch& OverlayMeshBatch = Collector.AllocateMesh();
					OverlayMeshBatch = Mesh;
					OverlayMeshBatch.bOverlayMaterial = true;
					OverlayMeshBatch.CastShadow = false;
					OverlayMeshBatch.bSelectable = false;
					OverlayMeshBatch.MaterialRenderProxy = ActiveOverlayMaterial->GetRenderProxy();
					// make sure overlay is always rendered on top of base mesh
					OverlayMeshBatch.MeshIdInPrimitive += LODData.RenderSections.Num();
					Collector.AddMesh(ViewIndex, OverlayMeshBatch);
				
					INC_DWORD_STAT_BY(STAT_SkelMeshTriangles, OverlayMeshBatch.GetNumPrimitives());
					INC_DWORD_STAT(STAT_SkelMeshDrawCalls);
				}
			}
		}
	}
}

void FSkeletalMeshSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
#if RHI_RAYTRACING
	if (IsRayTracingAllowed())
	{
		// copy RayTracingGeometryGroupHandle from FSkeletalMeshRenderData since USkeletalMesh can be released before the proxy is destroyed
		RayTracingGeometryGroupHandle = SkeletalMeshRenderData->RayTracingGeometryGroupHandle;
	}
#endif
}

#if RHI_RAYTRACING
bool FSkeletalMeshSceneProxy::HasRayTracingRepresentation() const
{
	return bRenderStatic || (CVarRayTracingSupportSkeletalMeshes.GetValueOnAnyThread() != 0 && CVarRayTracingSkeletalMeshes.GetValueOnAnyThread() != 0);
}

RayTracing::FGeometryGroupHandle FSkeletalMeshSceneProxy::GetRayTracingGeometryGroupHandle() const
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());
	return RayTracingGeometryGroupHandle;
}

TArray<FRayTracingGeometry*> FSkeletalMeshSceneProxy::GetStaticRayTracingGeometries() const
{
	if (IsRayTracingEnabled() && bRenderStatic)
	{
		TArray<FRayTracingGeometry*> RayTracingGeometries;
		RayTracingGeometries.AddDefaulted(SkeletalMeshRenderData->LODRenderData.Num());
		for (int32 LODIndex = 0; LODIndex < SkeletalMeshRenderData->LODRenderData.Num(); LODIndex++)
		{
			FSkeletalMeshLODRenderData& LODRenderData = SkeletalMeshRenderData->LODRenderData[LODIndex];

			// Skip LODs that have their render data stripped
			if (LODRenderData.GetNumVertices() > 0)
			{
				ensure(LODRenderData.NumReferencingStaticSkeletalMeshObjects > 0);
				RayTracingGeometries[LODIndex] = &LODRenderData.StaticRayTracingGeometry;
			}
		}

		return MoveTemp(RayTracingGeometries);
	}

	return {};
}

void FSkeletalMeshSceneProxy::GetDynamicRayTracingInstances(FRayTracingInstanceCollector& Collector)
{
	if (!CVarRayTracingSkeletalMeshes.GetValueOnRenderThread()
		|| !CVarRayTracingSupportSkeletalMeshes.GetValueOnRenderThread())
	{
		return;
	}
	
	// According to GetMeshElementsConditionallySelectable(), non-resident LODs should just be skipped
	if (MeshObject->GetRayTracingLOD() < SkeletalMeshRenderData->CurrentFirstLODIdx)
	{
		return;
	}

	FRayTracingGeometry* RayTracingGeometry = MeshObject->GetRayTracingGeometry();
	if (RayTracingGeometry == nullptr)
	{
		return;
	}

	check(RayTracingGeometry->Initializer.IndexBuffer.IsValid());

	// Update BLAS if build is required, RT geometry is not valid or evicted
	bool bRequiresUpdate = RayTracingGeometry->GetRequiresUpdate() || !RayTracingGeometry->IsValid() || RayTracingGeometry->IsEvicted();
				
	FRayTracingInstance RayTracingInstance;
	RayTracingInstance.Geometry = RayTracingGeometry;

	// Setup materials for each segment
	const int32 LODIndex = MeshObject->GetRayTracingLOD();
	check(LODIndex < SkeletalMeshRenderData->LODRenderData.Num());
	const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

	ensure(LODSections.Num() > 0);
	const FLODSectionElements& LODSection = LODSections[LODIndex];
	check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());
				
	//#dxr_todo (UE-113617): verify why this condition is not fulfilled sometimes
	if (!ensure(LODSection.SectionElements.Num() == RayTracingGeometry->Initializer.Segments.Num()))
	{
		return;
	}
			
	const bool bIgnorePreviewFilter = true;

	bool bVFsSupportRayTracingDynamicGeometry = true;

	for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection, bIgnorePreviewFilter); Iter; ++Iter)
	{
		const FSkelMeshRenderSection& Section = Iter.GetSection();
		const int32 SectionIndex = Iter.GetSectionElementIndex();
		const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

		FMeshBatch MeshBatch;
		CreateBaseMeshBatch(Collector.GetReferenceView(), LODData, LODIndex, SectionIndex, SectionElementInfo, MeshBatch, ESkinVertexFactoryMode::RayTracing);

		RayTracingInstance.Materials.Add(MeshBatch);

		if (bAnySegmentUsesWorldPositionOffset && !Section.bDisabled)
		{
			const FVertexFactoryType* VertexFactoryType = MeshBatch.VertexFactory->GetType();

			if (!ensureMsgf(VertexFactoryType->SupportsRayTracingDynamicGeometry(),
				TEXT("Mesh uses world position offset, but the vertex factory does not support ray tracing dynamic geometry. ")
				TEXT("MeshObject: %s, VertexFactory: %s."), *MeshObject->GetDebugName().ToString(), VertexFactoryType->GetName()))
			{
				bVFsSupportRayTracingDynamicGeometry = false;
			}
		}
	}

	RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());

	const uint32 VertexBufferNumVertices = LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
	const uint32 VertexBufferStride = LODData.StaticVertexBuffers.PositionVertexBuffer.GetStride();

	if (bAnySegmentUsesWorldPositionOffset && bVFsSupportRayTracingDynamicGeometry)
	{
		TArray<FRayTracingGeometrySegment> GeometrySections;
		GeometrySections.Reserve(LODData.RenderSections.Num());

		for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection, bIgnorePreviewFilter); Iter; ++Iter)
		{
			const FSkelMeshRenderSection& Section = Iter.GetSection();
			const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

			FRayTracingGeometrySegment Segment;
			Segment.VertexBufferStride = VertexBufferStride;
			Segment.MaxVertices = VertexBufferNumVertices;
			Segment.FirstPrimitive = Section.BaseIndex / 3;
			Segment.NumPrimitives = Section.NumTriangles;
			Segment.bEnabled = !MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex) && !Section.bDisabled && Section.bVisibleInRayTracing;
			GeometrySections.Add(Segment);
		}

		RayTracingGeometry->Initializer.Segments = GeometrySections;

		Collector.AddRayTracingGeometryUpdate(
			FRayTracingDynamicGeometryUpdateParams
			{
				RayTracingInstance.Materials,
				false,
				LODData.GetNumVertices(),
				LODData.GetNumVertices() * (uint32)sizeof(FVector3f),
				RayTracingGeometry->Initializer.TotalPrimitiveCount,
				RayTracingGeometry,
				MeshObject->GetRayTracingDynamicVertexBuffer(),
				true
			}
		);
	}
	else if (bRequiresUpdate)
	{
		// No compute shader update required - just a BLAS build/update
		FRayTracingDynamicGeometryUpdateParams UpdateParams;
		UpdateParams.Geometry = RayTracingGeometry;
		Collector.AddRayTracingGeometryUpdate(MoveTemp(UpdateParams));
	}

	Collector.AddRayTracingInstance(MoveTemp(RayTracingInstance));
}
#endif // RHI_RAYTRACING

SIZE_T FSkeletalMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

bool FSkeletalMeshSceneProxy::HasDynamicIndirectShadowCasterRepresentation() const
{
	return CastsDynamicShadow() && CastsDynamicIndirectShadow();
}

void FSkeletalMeshSceneProxy::GetShadowShapes(FVector PreViewTranslation, TArray<FCapsuleShape3f>& OutCapsuleShapes) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetShadowShapes);

	const TArray<FMatrix44f>& ReferenceToLocalMatrices = MeshObject->GetReferenceToLocalMatrices();
	const FMatrix& ProxyLocalToWorld = GetLocalToWorld();

	int32 CapsuleIndex = OutCapsuleShapes.Num();
	OutCapsuleShapes.SetNum(OutCapsuleShapes.Num() + ShadowCapsuleData.Num(), EAllowShrinking::No);

	for(const TPair<int32, FCapsuleShape>& CapsuleData : ShadowCapsuleData)
	{
		FMatrix ReferenceToWorld = ProxyLocalToWorld;
		if (ReferenceToLocalMatrices.IsValidIndex(CapsuleData.Key))
		{
			ReferenceToWorld = FMatrix(ReferenceToLocalMatrices[CapsuleData.Key]) * ProxyLocalToWorld;
		}
		const float MaxScale = ReferenceToWorld.GetScaleVector().GetMax();

		FCapsuleShape3f& NewCapsule = OutCapsuleShapes[CapsuleIndex++];

		NewCapsule.Center = (FVector4f)(ReferenceToWorld.TransformPosition(CapsuleData.Value.Center) + PreViewTranslation);
		NewCapsule.Radius = CapsuleData.Value.Radius * MaxScale;
		NewCapsule.Orientation = (FVector4f)ReferenceToWorld.TransformVector(CapsuleData.Value.Orientation).GetSafeNormal();
		NewCapsule.Length = CapsuleData.Value.Length * MaxScale;
	}
}

/**
 * Returns the world transform to use for drawing.
 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
 */
bool FSkeletalMeshSceneProxy::GetWorldMatrices( FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal ) const
{
	OutLocalToWorld = GetLocalToWorld();
	if (OutLocalToWorld.GetScaledAxis(EAxis::X).IsNearlyZero(UE_SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Y).IsNearlyZero(UE_SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Z).IsNearlyZero(UE_SMALL_NUMBER))
	{
		return false;
	}
	OutWorldToLocal = GetLocalToWorld().InverseFast();
	return true;
}

/**
 * Relevance is always dynamic for skel meshes unless they are disabled
 */
FPrimitiveViewRelevance FSkeletalMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	// View relevance is updated once per frame per view across all views in the frame (including shadows) so we update the LOD level for next frame here.
	MeshObject->UpdateMinDesiredLODLevel(View, GetBounds());

	const auto& EngineShowFlags = View->Family->EngineShowFlags;

	const auto IsDynamic = [&]
	{
#if !(UE_BUILD_SHIPPING) || WITH_EDITOR
		return IsRichView(*View->Family)
			|| EngineShowFlags.Bounds
			|| EngineShowFlags.Bones
			|| EngineShowFlags.Collision
			|| EngineShowFlags.VisualizeGPUSkinCache
			|| (IsSelected() && EngineShowFlags.VertexColors)
			|| bForceWireframe
#if WITH_EDITORONLY_DATA
			|| MeshObject->SelectedEditorMaterial != -1
			|| MeshObject->SelectedEditorSection != -1
			|| (PoseWatchDynamicData && !PoseWatchDynamicData->PoseWatches.IsEmpty())
#endif
			;
#else
		return false;
#endif
	};

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && !!EngineShowFlags.SkeletalMeshes;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bStaticRelevance = (bRenderStatic || GSkeletalMeshUseCachedMDCs)
		&& MeshObject->SupportsStaticRelevance()
		// Switch to dynamic if the mesh object is not ready. GetDynamicMeshElements won't generate any mesh batch in this case.
		// Consequently, this mesh won't be drawn this frame but render time will be updated which triggers an update to the mesh object. 
		&& MeshObject->GetLOD() >= GetCurrentFirstLODIdx_Internal()
		&& !IsDynamic();
	Result.bDynamicRelevance = ~Result.bStaticRelevance;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bRenderInDepthPass = ShouldRenderInDepthPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	
	MaterialRelevance.SetPrimitiveViewRelevance(Result);

#if !UE_BUILD_SHIPPING
	Result.bSeparateTranslucency |= EngineShowFlags.Constraints;
#endif

#if WITH_EDITOR
	//only check these in the editor
	if (Result.bStaticRelevance)
	{
		Result.bEditorStaticSelectionRelevance = (IsSelected() || IsHovered() || WantsEditorEffects());

		Result.bEditorVisualizeLevelInstanceRelevance = IsEditingLevelInstanceChild();
	}
#endif

	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;

	return Result;
}

bool FSkeletalMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest && !MaterialRelevance.bPostMotionBlurTranslucency && !ShouldRenderCustomDepth();
}

bool FSkeletalMeshSceneProxy::IsUsingDistanceCullFade() const
{
	return MaterialRelevance.bUsesDistanceCullFade;
}

/** Util for getting LOD index currently used by this SceneProxy. */
int32 FSkeletalMeshSceneProxy::GetCurrentLODIndex()
{
	if(MeshObject)
	{
		return MeshObject->GetLOD();
	}
	else
	{
		return 0;
	}
}


/** 
 * Render physics asset for debug display
 */
void FSkeletalMeshSceneProxy::DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
	const FMatrix& ProxyLocalToWorld = GetLocalToWorld();

	FMatrix ScalingMatrix = ProxyLocalToWorld;
	FVector TotalScale = ScalingMatrix.ExtractScaling();

	// Only if valid
	if( !TotalScale.IsNearlyZero() )
	{
		FTransform LocalToWorldTransform(ProxyLocalToWorld);

		TArray<FTransform>* BoneSpaceBases = MeshObject->GetComponentSpaceTransforms();
		if(BoneSpaceBases)
		{
			//TODO: These data structures are not double buffered. This is not thread safe!
			check(PhysicsAssetForDebug);
			if (EngineShowFlags.Collision && IsCollisionEnabled())
			{
				PhysicsAssetForDebug->GetCollisionMesh(ViewIndex, Collector, SkeletalMeshForDebug->GetRefSkeleton(), *BoneSpaceBases, LocalToWorldTransform, TotalScale);
			}
			if (EngineShowFlags.Constraints)
			{
				PhysicsAssetForDebug->DrawConstraints(ViewIndex, Collector, SkeletalMeshForDebug->GetRefSkeleton(), *BoneSpaceBases, LocalToWorldTransform, TotalScale.X);
			}
		}
	}
}

#if WITH_EDITOR
void FSkeletalMeshSceneProxy::DebugDrawPoseWatchSkeletons(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

	if (PoseWatchDynamicData)
	{
		for (const FAnimNodePoseWatch& PoseWatch : PoseWatchDynamicData->PoseWatches)
		{
			SkeletalDebugRendering::DrawBonesFromPoseWatch(PDI, PoseWatch, true);
		}
	}
}
#endif

void FSkeletalMeshSceneProxy::DebugDrawSkeleton(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const FMatrix& ProxyLocalToWorld = GetLocalToWorld();

	// Can't draw this, don't have ComponentSpaceTransforms. This happens with sk meshes rendered with FSkeletalMeshObjectStatic.
	if (!MeshObject->GetComponentSpaceTransforms())
	{
		return;
	}

	FTransform LocalToWorldTransform(ProxyLocalToWorld);

	auto MakeRandomColorForSkeleton = [](uint32 InUID)
	{
		FRandomStream Stream((int32)InUID);
		const uint8 Hue = (uint8)(Stream.FRand()*255.f);
		return FLinearColor::MakeFromHSV8(Hue, 255, 255);
	};

	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
	TArray<FTransform>& ComponentSpaceTransforms = *MeshObject->GetComponentSpaceTransforms();

	for (int32 Index = 0; Index < ComponentSpaceTransforms.Num(); ++Index)
	{
		const int32 ParentIndex = SkeletalMeshForDebug->GetRefSkeleton().GetParentIndex(Index);
		FVector Start, End;
		
		FLinearColor LineColor = DebugDrawColor.Get(MakeRandomColorForSkeleton(GetPrimitiveComponentId().PrimIDValue));
		const FTransform Transform = ComponentSpaceTransforms[Index] * LocalToWorldTransform;

		if (ParentIndex >= 0)
		{
			Start = (ComponentSpaceTransforms[ParentIndex] * LocalToWorldTransform).GetLocation();
			End = Transform.GetLocation();
		}
		else
		{
			Start = LocalToWorldTransform.GetLocation();
			End = Transform.GetLocation();
		}

		if(EngineShowFlags.Bones || bDrawDebugSkeleton)
		{
			if(CVarDebugDrawSimpleBones.GetValueOnRenderThread() != 0)
			{
				PDI->DrawLine(Start, End, LineColor, SDPG_Foreground, 0.0f, 1.0f);
			}
			else
			{
				SkeletalDebugRendering::DrawWireBone(PDI, Start, End, LineColor, SDPG_Foreground);
			}

			if(CVarDebugDrawBoneAxes.GetValueOnRenderThread() != 0)
			{
				SkeletalDebugRendering::DrawAxes(PDI, Transform, SDPG_Foreground);
			}
		}
	}
#endif
}

/**
* Updates morph material usage for materials referenced by each LOD entry
*
* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
*/
void FSkeletalMeshSceneProxy::UpdateMorphMaterialUsage_GameThread(TArray<UMaterialInterface*>& MaterialUsingMorphTarget)
{
	bool bNeedsMorphUsage = MaterialUsingMorphTarget.Num() > 0;
	if( bNeedsMorphUsage != bMaterialsNeedMorphUsage_GameThread )
	{
		// keep track of current morph material usage for the proxy
		bMaterialsNeedMorphUsage_GameThread = bNeedsMorphUsage;

		TSet<UMaterialInterface*> MaterialsToSwap;
		for (auto It = MaterialsInUse_GameThread.CreateConstIterator(); It; ++It)
		{
			UMaterialInterface* Material = *It;
			if (Material)
			{
				const bool bCheckSkelUsage = Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
				if (!bCheckSkelUsage)
				{
					MaterialsToSwap.Add(Material);
				}
				else if(MaterialUsingMorphTarget.Contains(Material))
				{
					const bool bCheckMorphUsage = !bMaterialsNeedMorphUsage_GameThread || (bMaterialsNeedMorphUsage_GameThread && Material->CheckMaterialUsage_Concurrent(MATUSAGE_MorphTargets));
					// make sure morph material usage and default skeletal usage are both valid
					if (!bCheckMorphUsage)
					{
						MaterialsToSwap.Add(Material);
					}
				}
			}
		}

		// update the new LODSections on the render thread proxy
		if (MaterialsToSwap.Num())
		{
			TSet<UMaterialInterface*> InMaterialsToSwap = MaterialsToSwap;
			UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			ERHIFeatureLevel::Type InFeatureLevel = GetScene().GetFeatureLevel();
			FSkeletalMeshSceneProxy* SkelMeshSceneProxy = this;
			FMaterialRelevance DefaultRelevance = DefaultMaterial->GetRelevance(InFeatureLevel);
			ENQUEUE_RENDER_COMMAND(UpdateSkelProxyLODSectionElementsCmd)(UE::RenderCommandPipe::SkeletalMesh,
				[InMaterialsToSwap, DefaultMaterial, DefaultRelevance, InFeatureLevel, SkelMeshSceneProxy]
				{
					for( int32 LodIdx=0; LodIdx < SkelMeshSceneProxy->LODSections.Num(); LodIdx++ )
					{
						FLODSectionElements& LODSection = SkelMeshSceneProxy->LODSections[LodIdx];
						for( int32 SectIdx=0; SectIdx < LODSection.SectionElements.Num(); SectIdx++ )
						{
							FSectionElementInfo& SectionElement = LODSection.SectionElements[SectIdx];
							if( InMaterialsToSwap.Contains(SectionElement.Material) )
							{
								// fallback to default material if needed
								SectionElement.Material = DefaultMaterial;
							}
						}
					}
					SkelMeshSceneProxy->MaterialRelevance |= DefaultRelevance;
				});
		}
	}
}

#if WITH_EDITORONLY_DATA

bool FSkeletalMeshSceneProxy::GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const
{

	if (FPrimitiveSceneProxy::GetPrimitiveDistance(LODIndex, SectionIndex, ViewOrigin, PrimitiveDistance))
	{
		const float OneOverDistanceMultiplier = 1.f / FMath::Max<float>(UE_SMALL_NUMBER, StreamingDistanceMultiplier);
		PrimitiveDistance *= OneOverDistanceMultiplier;
		return true;
	}
	return false;
}

bool FSkeletalMeshSceneProxy::GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const
{
	if (LODSections.IsValidIndex(LODIndex) && LODSections[LODIndex].SectionElements.IsValidIndex(SectionIndex))
	{
		// The LOD-section data is stored per material index as it is only used for texture streaming currently.
		const int32 MaterialIndex = LODSections[LODIndex].SectionElements[SectionIndex].UseMaterialIndex;
		if (SkeletalMeshRenderData && SkeletalMeshRenderData->UVChannelDataPerMaterial.IsValidIndex(MaterialIndex))
		{
			const float TransformScale = GetLocalToWorld().GetMaximumAxisScale();
			const float* LocalUVDensities = SkeletalMeshRenderData->UVChannelDataPerMaterial[MaterialIndex].LocalUVDensities;

			WorldUVDensities.Set(
				LocalUVDensities[0] * TransformScale,
				LocalUVDensities[1] * TransformScale,
				LocalUVDensities[2] * TransformScale,
				LocalUVDensities[3] * TransformScale);
			
			return true;
		}
	}
	return FPrimitiveSceneProxy::GetMeshUVDensities(LODIndex, SectionIndex, WorldUVDensities);
}

bool FSkeletalMeshSceneProxy::GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4f* OneOverScales, FIntVector4* UVChannelIndices) const
	{
	if (LODSections.IsValidIndex(LODIndex) && LODSections[LODIndex].SectionElements.IsValidIndex(SectionIndex))
	{
		const UMaterialInterface* Material = LODSections[LODIndex].SectionElements[SectionIndex].Material;
		if (Material)
		{
			// This is thread safe because material texture data is only updated while the renderthread is idle.
			for (const FMaterialTextureInfo& TextureData : Material->GetTextureStreamingData())
			{
				const int32 TextureIndex = TextureData.TextureIndex;
				if (TextureData.IsValid(true))
				{
					OneOverScales[TextureIndex / 4][TextureIndex % 4] = 1.f / TextureData.SamplingScale;
					UVChannelIndices[TextureIndex / 4][TextureIndex % 4] = TextureData.UVChannelIndex;
				}
			}
			for (const FMaterialTextureInfo& TextureData : Material->TextureStreamingDataMissingEntries)
			{
				const int32 TextureIndex = TextureData.TextureIndex;
				if (TextureIndex >= 0 && TextureIndex < TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL)
				{
					OneOverScales[TextureIndex / 4][TextureIndex % 4] = 1.f;
					UVChannelIndices[TextureIndex / 4][TextureIndex % 4] = 0;
				}
			}
			return true;
		}
	}
	return false;
}
#endif

void FSkeletalMeshSceneProxy::OnTransformChanged(FRHICommandListBase& RHICmdList)
{
	// OnTransformChanged is called on the following frame after FSkeletalMeshObject::Update(), thus omit '+ 1' to frame number.
	MeshObject->SetTransform(GetLocalToWorld(), GetScene().GetFrameNumber());
	MeshObject->RefreshClothingTransforms(GetLocalToWorld(), GetScene().GetFrameNumber());
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeNaniteComponent.h"
#include "DerivedDataCacheInterface.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "NaniteSceneProxy.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "NaniteDefinitions.h"
#include "UObject/Package.h"
#include "RenderUtils.h"
#include "Serialization/MemoryReader.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeNaniteComponent)

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "StaticMeshOperations.h"
#include "MeshUtilitiesCommon.h"
#include "OverlappingCorners.h"
#include "MeshBuild.h"
#include "StaticMeshBuilder.h"
#include "NaniteBuilder.h"
#include "Rendering/NaniteResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "LandscapePrivate.h"
#include "LandscapeDataAccess.h"
#include "LandscapeSubsystem.h"
#include "MeshDescriptionHelper.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "EditorFramework/AssetImportData.h"
#include "Interfaces/ITargetPlatform.h"
#endif

extern float LandscapeNaniteAsyncDebugWait;
extern float LandscapeNaniteStallDetectionTimeout;

namespace UE::Landscape
{
	extern int32 NaniteExportCacheMaxQuadCount;
	
#if WITH_EDITOR
	bool Nanite::FAsyncBuildData::CheckForStallAndWarn()
	{
		if (bIsComplete.load())
		{
			return false;
		}

		// check if it's taking a long time
		// TODO [chris.tchou] Checking start/finish timestamps is not a great way to detect stalls, as it is prone to false positives.
		// Especially because of the way we queue up tasks for the entire landscape all at once, it can take a while to chew through the backlog.
		// (this is worse on larger landscapes and slower machines).
		// Better would be to have a manager that only kicked off tasks based on available resources,
		// and track timestamps on individual task/step completion.
		double Now = FPlatformTime::Seconds();
		bool bStalled =
			((TimeStamp_Requested > 0.0) && (Now - TimeStamp_Requested > LandscapeNaniteStallDetectionTimeout)) ||
			((TimeStamp_StaticMeshBatchBuildStart > 0.0) && (TimeStamp_StaticMeshBatchBuildPostMeshBuildCall < 0.0) && (Now - TimeStamp_StaticMeshBatchBuildStart > LandscapeNaniteStallDetectionTimeout * 0.1));

		if (bStalled)
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				// assume when a debugger is attached, any stalls are caused by breakpoints
				return false;
			}

			if (!bWarnedStall)
			{
				FString LandscapeName = LandscapeWeakRef.IsValid() ? LandscapeWeakRef->GetName() : FString("INVALID");

				UE_LOG(LogLandscape, Warning, TEXT("Nanite Build Task for '%s' is taking a long time: Req:%f Exp:%f-%f MB:%f-%f BB:%f PMB:%f LU:%f-%f Complete:%f Cancelled:%f Now:%f bResult:%d bCancel:%d bNeedsPMB:%d  Changing landscape.Nanite.StallDetectionTimeout controls how long until this message appears."),
					*LandscapeName,
					TimeStamp_Requested,
					TimeStamp_ExportMeshStart,
					TimeStamp_ExportMeshEnd,
					TimeStamp_StaticMeshBuildStart,
					TimeStamp_StaticMeshBuildEnd,
					TimeStamp_StaticMeshBatchBuildStart,
					TimeStamp_StaticMeshBatchBuildPostMeshBuildCall,
					TimeStamp_LandscapeUpdateStart,
					TimeStamp_LandscapeUpdateEnd,
					TimeStamp_Complete,
					TimeStamp_Cancelled,
					Now,
					bExportResult.load(),
					bCancelled.load(),
					bStaticMeshNeedsToCallPostMeshBuild.load()
				);
				bWarnedStall = true;
			}
		}
		return bStalled;
	}
#endif // WITH_EDITOR
}

ULandscapeNaniteComponent::ULandscapeNaniteComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnabled(true)
{
	// We don't want Nanite representation in ray tracing
	bVisibleInRayTracing = false;

	// We don't want WPO evaluation enabled on landscape meshes
	bEvaluateWorldPositionOffset = false;
}

void ULandscapeNaniteComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (UStaticMesh* NaniteStaticMesh = GetStaticMesh())
	{
		UPackage* CurrentPackage = GetPackage();
		check(CurrentPackage);
		// At one point, the Nanite mesh was outered to the component, which leads the mesh to be duplicated when entering PIE. If we outer the mesh to the package instead, 
		//  PIE duplication will simply reference that mesh, preventing the expensive copy to occur when entering PIE: 
		if (!(CurrentPackage->GetPackageFlags() & PKG_PlayInEditor)  // No need to do it on PIE, since the outer should already have been changed in the original object 
			&& (NaniteStaticMesh->GetOuter() != CurrentPackage))
		{
			// Change the outer : 
			NaniteStaticMesh->Rename(nullptr, CurrentPackage);
		}
	}
#endif // WITH_EDITOR

	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (ensure(LandscapeProxy))
	{
		// Ensure that the component lighting and shadow settings matches the actor
		UpdatedSharedPropertiesFromActor();
	}

	// Override settings that may have been serialized previously with the wrong values
	{
		// We don't want Nanite representation in ray tracing
		bVisibleInRayTracing = false;

		// We don't want WPO evaluation enabled on landscape meshes
		bEvaluateWorldPositionOffset = false;
	}
}

void ULandscapeNaniteComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	Super::CollectPSOPrecacheData(BasePrecachePSOParams, OutParams);
	
	// Mark high priority
	for (FMaterialInterfacePSOPrecacheParams& Params : OutParams)
	{
		Params.Priority = EPSOPrecachePriority::High;
	}
}

ALandscapeProxy* ULandscapeNaniteComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

ALandscape* ULandscapeNaniteComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = GetLandscapeProxy();
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return nullptr;
}

void ULandscapeNaniteComponent::UpdatedSharedPropertiesFromActor()
{
	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();

	CastShadow = LandscapeProxy->CastShadow;
	bCastDynamicShadow = LandscapeProxy->bCastDynamicShadow;
	bCastStaticShadow = LandscapeProxy->bCastStaticShadow;
	bCastContactShadow = LandscapeProxy->bCastContactShadow;
	bCastFarShadow = LandscapeProxy->bCastFarShadow;
	bCastHiddenShadow = LandscapeProxy->bCastHiddenShadow;
	bCastShadowAsTwoSided = LandscapeProxy->bCastShadowAsTwoSided;
	bAffectDistanceFieldLighting = LandscapeProxy->bAffectDistanceFieldLighting;
	bAffectDynamicIndirectLighting = LandscapeProxy->bAffectDynamicIndirectLighting;
	bAffectIndirectLightingWhileHidden = LandscapeProxy->bAffectIndirectLightingWhileHidden;
	bRenderCustomDepth = LandscapeProxy->bRenderCustomDepth;
	CustomDepthStencilWriteMask = LandscapeProxy->CustomDepthStencilWriteMask;
	CustomDepthStencilValue = LandscapeProxy->CustomDepthStencilValue;
	SetCullDistance(LandscapeProxy->LDMaxDrawDistance);
	LightingChannels = LandscapeProxy->LightingChannels;
	bHoldout = LandscapeProxy->bHoldout;
	ShadowCacheInvalidationBehavior = LandscapeProxy->ShadowCacheInvalidationBehavior;
}

void ULandscapeNaniteComponent::SetEnabled(bool bValue)
{
	if (bValue != bEnabled)
	{
		bEnabled = bValue;
		MarkRenderStateDirty();
	}
}

bool ULandscapeNaniteComponent::NeedsLoadForTargetPlatform(const class ITargetPlatform* TargetPlatform) const
{
	// The ULandscapeNaniteComponent will never contain collision data, so if the platform cannot support rendering nanite, it does not need to be exported
	return DoesTargetPlatformSupportNanite(TargetPlatform);
}

bool ULandscapeNaniteComponent::IsHLODRelevant() const
{
	// This component doesn't need to be included in HLOD, as we're already including the non-nanite LS components
	return false;
}

#if WITH_EDITOR

FGraphEventRef ULandscapeNaniteComponent::InitializeForLandscapeAsync(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId, const TArray<ULandscapeComponent*>& InComponentsToExport, int32 InNaniteComponentIndex)
{
	UE_LOG(LogLandscape, VeryVerbose, TEXT("InitializeForLandscapeAsync actor: '%s' package:'%s'"), *Landscape->GetActorNameOrLabel(), *Landscape->GetPackage()->GetName());

	check(bVisibleInRayTracing == false);

	UWorld* World = Landscape->GetWorld();
	
	ULandscapeSubsystem* LandscapeSubSystem = World->GetSubsystem<ULandscapeSubsystem>();
	check(LandscapeSubSystem);
	LandscapeSubSystem->IncNaniteBuild();

	TSharedRef<UE::Landscape::Nanite::FAsyncBuildData> AsyncBuildData = 
		LandscapeSubSystem->CreateTrackedNaniteBuildState(Landscape, GetLandscapeActor()->GetNaniteLODIndex(), InComponentsToExport);
	check(AsyncBuildData->NaniteStaticMesh);

	FGraphEventRef StaticMeshBuildCompleteEvent = AsyncBuildData->BuildCompleteEvent;
	
	FGraphEventRef ExportMeshEvent = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[AsyncBuildData, ProxyContentId = NewProxyContentId, Name = Landscape->GetActorNameOrLabel()]()
		{			
			TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeNaniteComponent::ExportLandscapeAsync-ExportMeshTask);

			UE_LOG(LogLandscape,  VeryVerbose, TEXT("Exporting actor '%s' package:'%s'"), *Name, *AsyncBuildData->LandscapeWeakRef->GetPackage()->GetName());
			AsyncBuildData->TimeStamp_ExportMeshStart = FPlatformTime::Seconds();

			if (!AsyncBuildData->LandscapeWeakRef.IsValid() || AsyncBuildData->bCancelled)
			{
				if (AsyncBuildData->TimeStamp_Cancelled < 0.0)
				{
					AsyncBuildData->TimeStamp_Cancelled = FPlatformTime::Seconds();
				}
				AsyncBuildData->bCancelled = true;
				return;
			}

			UWorld* World = AsyncBuildData->LandscapeWeakRef->GetWorld();
			ULandscapeSubsystem* LandscapeSubSystem = World->GetSubsystem<ULandscapeSubsystem>();
			check(LandscapeSubSystem);

			// LandscapeSubSystem->WaitLaunchNaniteBuild();		// TODO [chris.tchou]: this can deadlock, any waits should be done outside of async tasks
		
			AsyncBuildData->SourceModel = &AsyncBuildData->NaniteStaticMesh->AddSourceModel();
			AsyncBuildData->NaniteMeshDescription = AsyncBuildData->NaniteStaticMesh->CreateMeshDescription(0);

			// ExportToRawMeshDataCopy places Lightmap UVs in coord 2
			const int32 LightmapUVCoordIndex = 2;
			AsyncBuildData->NaniteStaticMesh->SetLightMapCoordinateIndex(LightmapUVCoordIndex);

			// create a hash key for the DDC cache of the landscape static mesh export
			FString ExportDDCKey;
			{
				// Mesh Export Version, expressed as a GUID string.  Change this if any of the mesh building code here changes.
				// NOTE: this does not invalidate the outer cache where we check if nanite meshes need to be rebuilt on load/cook.
				// it only invalidates the MeshExport DDC cache here.
				static const char* MeshExportVersion = "070c6830-8d06-42a3-f43e-0709bc41a5a9";

				FSHA1 Hasher;
				check(PLATFORM_LITTLE_ENDIAN); // not sure if NewProxyContentId byte order is platform agnostic or not
				Hasher.Update(reinterpret_cast<const uint8*>(&ProxyContentId), sizeof(FGuid));
				Hasher.Update(reinterpret_cast<const uint8*>(MeshExportVersion), strlen(MeshExportVersion));

				// since we can break proxies into multiple nanite meshes, the hash needs to include which piece(s) we are building here
				for (ULandscapeComponent* Component : AsyncBuildData->InputComponents)
				{
					FIntPoint ComponentBase = Component->GetSectionBase();
					Hasher.Update(reinterpret_cast<const uint8*>(&ComponentBase), sizeof(FIntPoint));
				}

				ExportDDCKey = Hasher.Finalize().ToString();
			}

			// Don't allow the engine to recalculate normals
			AsyncBuildData->SourceModel->BuildSettings.bRecomputeNormals = false;
			AsyncBuildData->SourceModel->BuildSettings.bRecomputeTangents = false;
			AsyncBuildData->SourceModel->BuildSettings.bRemoveDegenerates = false;
			AsyncBuildData->SourceModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
			AsyncBuildData->SourceModel->BuildSettings.bUseFullPrecisionUVs = false;			
			AsyncBuildData->SourceModel->BuildSettings.bGenerateLightmapUVs = false; // we generate our own Lightmap UVs; don't stomp on them!

			FMeshNaniteSettings& NaniteSettings = AsyncBuildData->NaniteStaticMesh->NaniteSettings;
			NaniteSettings.bEnabled = true;
			NaniteSettings.FallbackPercentTriangles = 0.01f; // Keep effectively no fallback mesh triangles
			NaniteSettings.FallbackRelativeError = 1.0f;

			const FVector3d Scale = AsyncBuildData->LandscapeWeakRef->GetTransform().GetScale3D();
			NaniteSettings.PositionPrecision = FMath::Log2(Scale.GetAbsMax() ) + AsyncBuildData->LandscapeWeakRef->GetNanitePositionPrecision();
			NaniteSettings.MaxEdgeLengthFactor = AsyncBuildData->LandscapeWeakRef->GetNaniteMaxEdgeLengthFactor();

			int32 LOD = AsyncBuildData->LOD;
			
			ALandscapeProxy::FRawMeshExportParams ExportParams;
			ExportParams.ComponentsToExport = MakeArrayView(AsyncBuildData->InputComponents.GetData(), AsyncBuildData->InputComponents.Num());
			ExportParams.ComponentsMaterialSlotName = MakeArrayView(AsyncBuildData->InputMaterialSlotNames.GetData(), AsyncBuildData->InputMaterialSlotNames.Num());
			if (AsyncBuildData->LandscapeWeakRef->IsNaniteSkirtEnabled())
			{
				ExportParams.SkirtDepth = AsyncBuildData->LandscapeWeakRef->GetNaniteSkirtDepth();
			}
			
			ExportParams.ExportLOD = LOD;
			ExportParams.ExportCoordinatesType = ALandscapeProxy::FRawMeshExportParams::EExportCoordinatesType::RelativeToProxy;
			ExportParams.UVConfiguration.ExportUVMappingTypes.SetNumZeroed(4);
			ExportParams.UVConfiguration.ExportUVMappingTypes[0] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::TerrainCoordMapping_XY; // In LandscapeVertexFactory, Texcoords0 = ETerrainCoordMappingType::TCMT_XY (or ELandscapeCustomizedCoordType::LCCT_CustomUV0)
			ExportParams.UVConfiguration.ExportUVMappingTypes[1] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::TerrainCoordMapping_XZ; // In LandscapeVertexFactory, Texcoords1 = ETerrainCoordMappingType::TCMT_XZ (or ELandscapeCustomizedCoordType::LCCT_CustomUV1)
			ExportParams.UVConfiguration.ExportUVMappingTypes[2] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::LightmapUV;			  // Note that this does not match LandscapeVertexFactory's usage, but we work around it in the material graph node to remap TCMT_YZ
			ExportParams.UVConfiguration.ExportUVMappingTypes[3] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::WeightmapUV;			  // In LandscapeVertexFactory, Texcoords3 = ELandscapeCustomizedCoordType::LCCT_WeightMapUV

			// in case we do generate lightmap UVs, use the "XY" mapping as the source chart UV, and store them to UV channel 2
			AsyncBuildData->SourceModel->BuildSettings.SrcLightmapIndex = 0;
			AsyncBuildData->SourceModel->BuildSettings.DstLightmapIndex = LightmapUVCoordIndex;

			// COMMENT [jonathan.bard] ATM Nanite meshes only support up to 4 UV sets so we cannot support those 2 : 
			//ExportParams.UVConfiguration.ExportUVMappingTypes[4] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::LightmapUV; // In LandscapeVertexFactory, Texcoords4 = lightmap UV
			//ExportParams.UVConfiguration.ExportUVMappingTypes[5] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::HeightmapUV; // // In LandscapeVertexFactory, Texcoords5 = heightmap UV

			// calculate the lightmap resolution for the proxy, and the number of quads
			int32 ProxyLightmapRes = 64;
			int32 ProxyQuadCount = 0;
			{
				const int32 ComponentSizeQuads = AsyncBuildData->LandscapeWeakRef->ComponentSizeQuads;
				const float LightMapRes = AsyncBuildData->LandscapeWeakRef->StaticLightingResolution;
			
				// min/max section bases of all exported components
				FIntPoint MinSectionBase(INT_MAX, INT_MAX);
				FIntPoint MaxSectionBase(-INT_MAX, -INT_MAX);
				for (ULandscapeComponent* Component : AsyncBuildData->InputComponents)
				{
					FIntPoint SectionBase{ Component->SectionBaseX, Component->SectionBaseY };
					MinSectionBase = MinSectionBase.ComponentMin(SectionBase);
					MaxSectionBase = MaxSectionBase.ComponentMax(SectionBase);
					ProxyQuadCount += ComponentSizeQuads;
				}
				int ProxyQuadsX = (MaxSectionBase.X + ComponentSizeQuads + 1 - MinSectionBase.X);
				int ProxyQuadsY = (MaxSectionBase.Y + ComponentSizeQuads + 1 - MinSectionBase.Y);

				// as the lightmap is just mapped as a square, it uses the square bounds to determine the resolution
				ProxyLightmapRes = (ProxyQuadsX > ProxyQuadsY ? ProxyQuadsX : ProxyQuadsY) * LightMapRes;
			}

			AsyncBuildData->NaniteStaticMesh->SetLightMapResolution(ProxyLightmapRes);

			const bool bUseNaniteExportCache = (UE::Landscape::NaniteExportCacheMaxQuadCount < 0) || (ProxyQuadCount <= UE::Landscape::NaniteExportCacheMaxQuadCount);

			bool bSuccess = false;
			int64 DDCReadBytes = 0;
			int64 DDCWriteBytes = 0;
			
			if (TArray64<uint8> MeshDescriptionData; 
				bUseNaniteExportCache && GetDerivedDataCacheRef().GetSynchronous(*ExportDDCKey, MeshDescriptionData, *AsyncBuildData->LandscapeWeakRef->GetFullName()))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeNaniteComponent::ExportLandscapeAsync - ReadExportedMeshFromDDC);

				FMemoryReaderView Reader(MakeMemoryView(MeshDescriptionData));
				AsyncBuildData->NaniteMeshDescription->Serialize(Reader);

				bSuccess = true;
				DDCReadBytes += MeshDescriptionData.Num();
			}
			else
			{
				// build the nanite mesh description
				bSuccess = AsyncBuildData->LandscapeWeakRef->ExportToRawMeshDataCopy(ExportParams, *AsyncBuildData->NaniteMeshDescription, AsyncBuildData.Get());

				// Apply the mesh description cleanup/optimization here instead of during DDC build (avoids expensive large mesh copies)
				FMeshDescriptionHelper MeshDescriptionHelper(&AsyncBuildData->SourceModel->BuildSettings);
				MeshDescriptionHelper.SetupRenderMeshDescription(AsyncBuildData->NaniteStaticMesh.Get(), *AsyncBuildData->NaniteMeshDescription, true /* Is Nanite */, false /* bNeedTangents */);

				// cache mesh description, only if we succeeded (failure may be non-deterministic)
				if (bUseNaniteExportCache && bSuccess)
				{
					// serialize the nanite mesh description and submit it to DDC 
					TArray64<uint8> MeshDescriptionData64;
					FMemoryWriter64 Writer(MeshDescriptionData64);
					AsyncBuildData->NaniteMeshDescription->Serialize(Writer);

					GetDerivedDataCacheRef().Put(*ExportDDCKey, MeshDescriptionData64, *AsyncBuildData->LandscapeWeakRef->GetFullName());
					DDCWriteBytes += MeshDescriptionData64.Num();
				}
			}

			const double ExportSeconds = FPlatformTime::Seconds() - AsyncBuildData->TimeStamp_ExportMeshStart;
			if (!bSuccess)
			{
				UE_LOG(LogLandscape, Log, TEXT("Failed export of raw static mesh for Nanite landscape (%i components) for actor %s : (DDC: %d, DDC read: %lld bytes, DDC write: %lld bytes, key: %s, export: %f seconds)"), AsyncBuildData->InputComponents.Num(), *Name, bUseNaniteExportCache, DDCReadBytes, DDCWriteBytes, *ExportDDCKey, ExportSeconds);
				if (AsyncBuildData->TimeStamp_Cancelled < 0.0)
				{
					AsyncBuildData->TimeStamp_Cancelled = FPlatformTime::Seconds();
				}
				AsyncBuildData->bCancelled = true;
				return;
			}

			// check we have one polygon group per component
			const FPolygonGroupArray& PolygonGroups = AsyncBuildData->NaniteMeshDescription->PolygonGroups();
			checkf(bSuccess && (PolygonGroups.Num() == AsyncBuildData->InputComponents.Num()), TEXT("Invalid landscape static mesh raw mesh export for actor %s (%i components)"), *Name, AsyncBuildData->InputComponents.Num());
			check(AsyncBuildData->InputMaterials.Num() == AsyncBuildData->InputComponents.Num());
			AsyncBuildData->MeshAttributes = MakeShared<FStaticMeshAttributes>(*AsyncBuildData->NaniteMeshDescription);

			TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeNaniteComponent::ExportLandscapeAsync - CommitMeshDescription);

			// commit the mesh description to build the static mesh for realz
			UStaticMesh::FCommitMeshDescriptionParams CommitParams;
			CommitParams.bMarkPackageDirty = false;
			CommitParams.bUseHashAsGuid = true;

			AsyncBuildData->NaniteStaticMesh->CommitMeshDescription(0u, CommitParams);
			AsyncBuildData->bExportResult = true;

			AsyncBuildData->TimeStamp_ExportMeshEnd = FPlatformTime::Seconds();
			const double DurationSeconds = AsyncBuildData->TimeStamp_ExportMeshEnd - AsyncBuildData->TimeStamp_ExportMeshStart;
			UE_LOG(LogLandscape, Log, TEXT("Successful export of raw static mesh for Nanite landscape (%i components) for actor %s : (DDC: %d, DDC read: %lld bytes, DDC write: %lld bytes, key: %s, export: %f seconds, commit: %f seconds)"), AsyncBuildData->InputComponents.Num(), *Name, bUseNaniteExportCache, DDCReadBytes, DDCWriteBytes, *ExportDDCKey, ExportSeconds, DurationSeconds - ExportSeconds);

			if (const double ExtraWait = FMath::Max(LandscapeNaniteAsyncDebugWait - DurationSeconds, 0.0); ExtraWait > 0.0)
			{
				FPlatformProcess::Sleep(ExtraWait);
			}

		}, TStatId(), nullptr, 
			ENamedThreads::AnyBackgroundHiPriTask);

	FGraphEventArray CommitDependencies{ ExportMeshEvent };

	FGraphEventRef BatchBuildEvent = FFunctionGraphTask::CreateAndDispatchWhenReady([AsyncBuildData, Component = this, NewProxyContentId, Name = Landscape->GetActorNameOrLabel(), StaticMeshBuildCompleteEvent, InNaniteComponentIndex]()
		{
			auto OnFinishTask = [StaticMeshBuildCompleteEvent, AsyncBuildData]()
			{
				if (AsyncBuildData->LandscapeSubSystemWeakRef.IsValid())
				{
					AsyncBuildData->LandscapeSubSystemWeakRef->DecNaniteBuild();
				}
				StaticMeshBuildCompleteEvent->DispatchSubsequents();
			};

			AsyncBuildData->TimeStamp_StaticMeshBuildStart = FPlatformTime::Seconds();

			if (AsyncBuildData->bCancelled || !AsyncBuildData->LandscapeWeakRef.IsValid())
			{
				AsyncBuildData->TimeStamp_StaticMeshBuildEnd = AsyncBuildData->TimeStamp_StaticMeshBuildStart;
				UE_LOG(LogLandscape, Verbose, TEXT("CANCELLED Build Static Mesh '%s'"), *Name);
				OnFinishTask();
				if (AsyncBuildData->TimeStamp_Cancelled < 0.0)
				{
					AsyncBuildData->TimeStamp_Cancelled = FPlatformTime::Seconds();
				}
				return;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeNaniteComponent::ExportLandscapeAsync-BatchBuildTask);
			AsyncBuildData->NaniteStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

			UE_LOG(LogLandscape, VeryVerbose, TEXT("Build Static Mesh '%s' package:'%s'"), *Name, *AsyncBuildData->LandscapeWeakRef->GetPackage()->GetName());
			auto CompleteStaticMesh = [AsyncBuildData, Component, NewProxyContentId, Name, InNaniteComponentIndex, OnFinishTask](UStaticMesh* InStaticMesh)
			{
				check(IsInGameThread());
				check(AsyncBuildData->NaniteStaticMesh.Get() == InStaticMesh);

				// ensure we always remove our PostMeshBuild delegate before returning
				ON_SCOPE_EXIT
				{
					// we need to do this at the very end (otherwise we end up deleting the lambda we are in and making captured data inaccessible...)
					// even at the end, this is a little bit suspect...
					if (AsyncBuildData->PostMeshBuildDelegateHandle.IsValid())
					{
						FDelegateHandle DelegateHandleToRemove = AsyncBuildData->PostMeshBuildDelegateHandle;
						AsyncBuildData->PostMeshBuildDelegateHandle.Reset();	// have to reset before calling Remove, as Remove will bork AsyncBuildData..
						InStaticMesh->OnPostMeshBuild().Remove(DelegateHandleToRemove);
					}
				};

				if (AsyncBuildData->bStaticMeshNeedsToCallPostMeshBuild)
				{
					AsyncBuildData->TimeStamp_StaticMeshBatchBuildPostMeshBuildCall = FPlatformTime::Seconds();
					UE_LOG(LogLandscape, Verbose, TEXT("Called CompleteStaticMesh from PostMeshBuild for %s"), *Name);
					AsyncBuildData->bStaticMeshNeedsToCallPostMeshBuild = false;
					check(AsyncBuildData->PostMeshBuildDelegateHandle.IsValid());	// will be removed on scope exit
				}

				AsyncBuildData->TimeStamp_LandscapeUpdateStart = FPlatformTime::Seconds();

				// this is as horror as we have to mark all the objects created in the background thread as not async 
				AsyncBuildData->NaniteStaticMesh->ClearInternalFlags(EInternalObjectFlags::Async);
				AsyncBuildData->NaniteStaticMesh->AssetImportData->ClearInternalFlags(EInternalObjectFlags::Async);

				AsyncBuildData->NaniteStaticMesh->GetHiResSourceModel().StaticMeshDescriptionBulkData->ClearInternalFlags(EInternalObjectFlags::Async);
				AsyncBuildData->NaniteStaticMesh->GetHiResSourceModel().StaticMeshDescriptionBulkData->CreateMeshDescription()->ClearInternalFlags(EInternalObjectFlags::Async);

				AsyncBuildData->NaniteStaticMesh->GetSourceModel(0).StaticMeshDescriptionBulkData->ClearInternalFlags(EInternalObjectFlags::Async);
				AsyncBuildData->NaniteStaticMesh->GetSourceModel(0).StaticMeshDescriptionBulkData->GetMeshDescription()->ClearInternalFlags(EInternalObjectFlags::Async);

				if (AsyncBuildData->bCancelled || !AsyncBuildData->LandscapeWeakRef.IsValid())
				{
					if (AsyncBuildData->TimeStamp_Cancelled < 0.0)
					{
						AsyncBuildData->TimeStamp_Cancelled = FPlatformTime::Seconds();
					}
					OnFinishTask();
					AsyncBuildData->TimeStamp_LandscapeUpdateEnd = FPlatformTime::Seconds();
					return;
				}

				check(AsyncBuildData->NaniteStaticMesh.Get() == InStaticMesh);

				// Proxy has been updated since and this nanite calculation is out of date.
				if (AsyncBuildData->LandscapeWeakRef->GetNaniteContentId() != NewProxyContentId)
				{
					AsyncBuildData->bIsComplete = true;
					AsyncBuildData->TimeStamp_Complete = FPlatformTime::Seconds();
					OnFinishTask();
					AsyncBuildData->TimeStamp_LandscapeUpdateEnd = FPlatformTime::Seconds();
					return;
				}				
				
				AsyncBuildData->NaniteStaticMesh->MarkPackageDirty();

				TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeNaniteComponent::ExportLandscapeAsync - FinalizeOnComponent);

				InStaticMesh->CreateBodySetup();
				if (UBodySetup* BodySetup = InStaticMesh->GetBodySetup())
				{
					BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
					BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
					// We won't ever enable collisions (since collisions are handled by ULandscapeHeightfieldCollisionComponent), ensure we don't even cook or load any collision data on this mesh: 
					BodySetup->bNeverNeedsCookedCollisionData = true;
				}

				Component->SetStaticMesh(InStaticMesh);
				AsyncBuildData->NaniteStaticMesh.Reset();  // Release the strong pointer.  The component owns it now.
				Component->SetProxyContentId(NewProxyContentId);
				Component->SetEnabled(!Component->IsEnabled());

				// Nanite Component should remember which ULandscapeComponents it was generated from if we need to update materials.
				Component->SourceLandscapeComponents = AsyncBuildData->InputComponents;
				
				AsyncBuildData->LandscapeWeakRef->UpdateRenderingMethod();
				AsyncBuildData->LandscapeWeakRef->NaniteComponents[InNaniteComponentIndex]->MarkRenderStateDirty();
				AsyncBuildData->LandscapeWeakRef->NaniteComponents[InNaniteComponentIndex] = Component;
				AsyncBuildData->bIsComplete = true;
				AsyncBuildData->TimeStamp_Complete = FPlatformTime::Seconds();

				UE_LOG(LogLandscape, VeryVerbose, TEXT("Complete Static Mesh '%s' package:'%s'"), *Name, *AsyncBuildData->LandscapeWeakRef->GetPackage()->GetName());
				AsyncBuildData->TimeStamp_LandscapeUpdateEnd = FPlatformTime::Seconds();

				OnFinishTask();
			};

			// when static mesh build is complete, call CompleteStaticMesh
			AsyncBuildData->bStaticMeshNeedsToCallPostMeshBuild = true;
			UE_LOG(LogLandscape, VeryVerbose, TEXT("Attaching to PostMeshBuild for %s"), *Name);

			AsyncBuildData->PostMeshBuildDelegateHandle =
				AsyncBuildData->NaniteStaticMesh->OnPostMeshBuild().AddLambda(CompleteStaticMesh);

			TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames = AsyncBuildData->MeshAttributes->GetPolygonGroupMaterialSlotNames();
			int32 ComponentIndex = 0;
			for (UMaterialInterface* Material : AsyncBuildData->InputMaterials)
			{
				check(Material != nullptr);
				const FName MaterialSlotName = AsyncBuildData->InputMaterialSlotNames[ComponentIndex];
				check(PolygonGroupMaterialSlotNames.GetRawArray().Contains(MaterialSlotName));
				AsyncBuildData->NaniteStaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, MaterialSlotName));
				++ComponentIndex;
			}
			
			AsyncBuildData->NaniteStaticMesh->MarkAsNotHavingNavigationData();

			AsyncBuildData->TimeStamp_StaticMeshBatchBuildStart = FPlatformTime::Seconds();
			UStaticMesh::FBuildParameters BuildParameters;
			BuildParameters.bInSilent = true;		
			UStaticMesh::BatchBuild({ AsyncBuildData->NaniteStaticMesh.Get() }, BuildParameters);

			AsyncBuildData->TimeStamp_StaticMeshBuildEnd = FPlatformTime::Seconds();
		},
	TStatId(),
	& CommitDependencies,
	ENamedThreads::GameThread);

	return StaticMeshBuildCompleteEvent;
}

void ULandscapeNaniteComponent::UpdateMaterials()
{
	if ( !GetLandscapeActor() || !GetLandscapeActor()->IsNaniteEnabled() || !GetStaticMesh())
	{
		return;
	}

	if (GetStaticMesh()->GetStaticMaterials().Num() < SourceLandscapeComponents.Num())
	{
		return;
	}

	bool bApplyResults = false;
	
	// Re-use existing static materials
	TArray<FStaticMaterial> StaticMaterials = GetStaticMesh()->GetStaticMaterials();
	bool bMaterialsRequiredUpdate = false;
	TArray<TObjectPtr<ULandscapeComponent>>& LandscapeComponents = GetLandscapeProxy()->LandscapeComponents;
	for (int32 SourceComponentIndex = 0; SourceComponentIndex < SourceLandscapeComponents.Num(); ++SourceComponentIndex)
	{
		TObjectPtr<ULandscapeComponent>* SourceLandscapeComponent = Algo::Find(LandscapeComponents, SourceLandscapeComponents[SourceComponentIndex]);
		if (SourceLandscapeComponent && (*SourceLandscapeComponent)->GetMaterialInstanceCount() > 0)
		{
			StaticMaterials[SourceComponentIndex].MaterialInterface = (*SourceLandscapeComponent)->GetMaterialInstance(0);
			bApplyResults = true;
		}
	}

	if (bApplyResults)
	{
		GetStaticMesh()->SetStaticMaterials(StaticMaterials);

#if WITH_EDITOR
		bool bRequiresPostEdit = !GetStaticMesh()->HasAnyFlags(RF_NeedPostLoad) && !HasAnyFlags(RF_NeedPostLoad) && !HasAnyFlags(RF_NeedPostLoadSubobjects);
		if (bRequiresPostEdit)
		{
			if (GetStaticMesh()->IsCompiling())
			{
				FStaticMeshCompilingManager::Get().FinishCompilation({ GetStaticMesh() });
			}
			GetStaticMesh()->PostEditChange();
		}
#endif
	}
}

void ULandscapeNaniteComponent::SetSourceLandscapeComponents(const TArray<ULandscapeComponent*>& InSourceLandscapeComponents)
{
	SourceLandscapeComponents.Reset();
	Algo::Transform(InSourceLandscapeComponents, SourceLandscapeComponents, [](ULandscapeComponent* Component){return TObjectPtr<ULandscapeComponent>(Component);});
	UpdateMaterials();
}

bool ULandscapeNaniteComponent::InitializeForLandscape(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId, const TArray<ULandscapeComponent*>& InComponentsToExport, int32 InNaniteComponentIndex)
{
	FGraphEventRef GraphEvent = InitializeForLandscapeAsync(Landscape, NewProxyContentId, InComponentsToExport, InNaniteComponentIndex);

	UWorld* World = Landscape->GetWorld();
	ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>();
	check(LandscapeSubsystem);
	const bool bAllNaniteBuildsDone = LandscapeSubsystem->FinishAllNaniteBuildsInFlightNow(ULandscapeSubsystem::EFinishAllNaniteBuildsInFlightFlags::Default);
	// Not passing ULandscapeSubsystem::EFinishAllNaniteBuildsInFlightFlags::AllowCancel, so there should be no way that FinishAllNaniteBuildsInFlightNow returns false :
	check(bAllNaniteBuildsDone && GraphEvent->IsComplete());
	
	return true;
}

bool ULandscapeNaniteComponent::InitializePlatformForLandscape(ALandscapeProxy* Landscape, const ITargetPlatform* TargetPlatform)
{
	
	UE_LOG(LogLandscape, Verbose, TEXT("InitializePlatformForLandscape '%s' package:'%s'"), *Landscape->GetActorNameOrLabel(), *Landscape->GetPackage()->GetName());

	// This is a workaround. IsCachedCookedPlatformDataLoaded needs to return true to ensure that StreamablePages are loaded from DDC
	if (TargetPlatform)
	{
		UE_LOG(LogLandscape, Verbose, TEXT("InitializePlatformForLandscape '%s' platform:'%s'"), *Landscape->GetActorNameOrLabel(), *TargetPlatform->DisplayName().ToString());
		if (UStaticMesh* NaniteStaticMesh = GetStaticMesh())
		{
			UE_LOG(LogLandscape, Verbose, TEXT("InitializePlatformForLandscape '%s' mesh:'%p'"), *Landscape->GetActorNameOrLabel(), NaniteStaticMesh);
			NaniteStaticMesh->BeginCacheForCookedPlatformData(TargetPlatform);
			FStaticMeshCompilingManager::Get().FinishCompilation({ NaniteStaticMesh });

			const double StartTime = FPlatformTime::Seconds();

			while (!NaniteStaticMesh->IsCachedCookedPlatformDataLoaded(TargetPlatform))
			{
				FAssetCompilingManager::Get().ProcessAsyncTasks(true);
				FPlatformProcess::Sleep(0.01);

				constexpr double MaxWaitSeconds = 240.0;
				if (FPlatformTime::Seconds() - StartTime > MaxWaitSeconds)
				{
					UE_LOG(LogLandscape, Error, TEXT("ULandscapeNaniteComponent::InitializePlatformForLandscape waited more than %f seconds for IsCachedCookedPlatformDataLoaded to return true"), MaxWaitSeconds);
					return false;
				}
			}
			UE_LOG(LogLandscape, Verbose, TEXT("InitializePlatformForLandscape '%s' Finished in %f"), *Landscape->GetActorNameOrLabel(), FPlatformTime::Seconds() - StartTime);
		}	
	}

	return true;
}

#endif

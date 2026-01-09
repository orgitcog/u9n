// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoPrimitiveComponent.h"
#include "FastGeoContainer.h"
#include "FastGeoComponentCluster.h"
#include "FastGeoInstancedStaticMeshComponent.h"
#include "FastGeoWorldSubsystem.h"
#include "FastGeoComponentCluster.h"
#include "FastGeoWeakElement.h"
#include "FastGeoLog.h"
#include "AI/NavigationModifier.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "PrimitiveSceneDesc.h"
#include "SceneInterface.h"
#include "WorldPartition/WorldPartition.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PrimitiveComponentHelper.h"
#include "PSOPrecache.h"
#include "PSOPrecacheMaterial.h"
#include "PrimitiveComponentHelper.h"
#include "UnrealEngine.h"

#if !UE_BUILD_SHIPPING
namespace FastGeo
{
	static int32 GShowFastGeo = 1;
	FAutoConsoleCommand GShowFastGeoCommand(
		TEXT("FastGeo.Show"),
		TEXT("Turn on/off rendering of FastGeo."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			GShowFastGeo = (Args.Num() != 1) || (Args[0] != TEXT("0"));
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World && World->IsGameWorld())
				{
					for (ULevel* Level : World->GetLevels())
					{
						if (UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>())
						{
							FastGeo->ForEachComponentCluster([](FFastGeoComponentCluster& ComponentCluster)
							{
								ComponentCluster.UpdateVisibility();
							});
						}
					}
				}
			}
		})
	);
}
#endif

const FFastGeoElementType FFastGeoPrimitiveComponent::Type(&FFastGeoComponent::Type);

FFastGeoPrimitiveComponent::FFastGeoPrimitiveComponent(int32 InComponentIndex, FFastGeoElementType InType)
	: Super(InComponentIndex, InType)
	, LocalBounds(ForceInit)
	, WorldBounds(ForceInit)
{
}

void FFastGeoPrimitiveComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize persistent data from FFastGeoPrimitiveComponent
	Ar << LocalTransform;
	Ar << WorldTransform;
	Ar << LocalBounds;
	Ar << WorldBounds;
	FArchive_Serialize_BitfieldBool(Ar, bIsVisible);
	FArchive_Serialize_BitfieldBool(Ar, bStaticWhenNotMoveable);
	FArchive_Serialize_BitfieldBool(Ar, bFillCollisionUnderneathForNavmesh);
	FArchive_Serialize_BitfieldBool(Ar, bRasterizeAsFilledConvexVolume);
	FArchive_Serialize_BitfieldBool(Ar, bCanEverAffectNavigation);
	Ar << CustomPrimitiveData.Data;
	Ar << DetailMode;
	Ar << bHasCustomNavigableGeometry;
	Ar << RuntimeVirtualTextures;
	FBodyInstance::StaticStruct()->SerializeItem(Ar, &BodyInstance, nullptr);

	// Serialize persistent data from FPrimitiveSceneProxyDesc
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.CastShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bReceivesDecals);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bOnlyOwnerSee);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bOwnerNoSee);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bUseViewOwnerDepthPriorityGroup);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bVisibleInReflectionCaptures);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bVisibleInRealTimeSkyCaptures);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bVisibleInRayTracing);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bRenderInDepthPass);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bRenderInMainPass);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bTreatAsBackgroundForOcclusion);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastDynamicShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastStaticShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bEmissiveLightSource);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bAffectDynamicIndirectLighting);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bAffectIndirectLightingWhileHidden);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bAffectDistanceFieldLighting);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastVolumetricTranslucentShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastContactShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastHiddenShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastShadowAsTwoSided);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bSelfShadowOnly);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastInsetShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastCinematicShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCastFarShadow);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bLightAttachmentsAsGroup);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bSingleSampleShadowFromStationaryLights);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bUseAsOccluder);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bHasPerInstanceHitProxies);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bReceiveMobileCSMShadows);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bRenderCustomDepth);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bVisibleInSceneCaptureOnly);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bHiddenInSceneCapture);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bForceMipStreaming);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bRayTracingFarField);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bHoldout);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bIsFirstPerson);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bIsFirstPersonWorldSpaceRepresentation);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bCollisionEnabled);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bIsHidden);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bSupportsWorldPositionOffsetVelocity);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bIsInstancedStaticMesh);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bHasStaticLighting);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bHasValidSettingsForStaticLighting);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bIsPrecomputedLightingValid);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bShadowIndirectOnly);
	Ar << SceneProxyDesc.Mobility;
	Ar << SceneProxyDesc.TranslucencySortPriority;
	Ar << SceneProxyDesc.TranslucencySortDistanceOffset;
	Ar << SceneProxyDesc.LightmapType;
	Ar << SceneProxyDesc.ViewOwnerDepthPriorityGroup;
	Ar << SceneProxyDesc.CustomDepthStencilValue;
	Ar << SceneProxyDesc.CustomDepthStencilWriteMask;
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.LightingChannels.bChannel0);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.LightingChannels.bChannel1);
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.LightingChannels.bChannel2);
	Ar << SceneProxyDesc.RayTracingGroupCullingPriority;
	Ar << SceneProxyDesc.IndirectLightingCacheQuality;
	Ar << SceneProxyDesc.ShadowCacheInvalidationBehavior;
	Ar << SceneProxyDesc.DepthPriorityGroup;
	Ar << SceneProxyDesc.VirtualTextureLodBias;
	Ar << SceneProxyDesc.VirtualTextureCullMips;
	Ar << SceneProxyDesc.VirtualTextureMinCoverage;
	Ar << SceneProxyDesc.VisibilityId;
	Ar << SceneProxyDesc.CachedMaxDrawDistance;
	Ar << SceneProxyDesc.MinDrawDistance;
	Ar << SceneProxyDesc.BoundsScale;
	Ar << SceneProxyDesc.RayTracingGroupId;
	Ar << SceneProxyDesc.VirtualTextureRenderPassType;
	Ar << SceneProxyDesc.VirtualTextureMainPassMaxDrawDistance;
}

void FFastGeoPrimitiveComponent::ApplyWorldTransform(const FTransform& InTransform)
{
	check(!GetOwnerContainer()->IsRegistered());
	WorldTransform = LocalTransform * InTransform;
}

#if WITH_EDITOR

void FFastGeoPrimitiveComponent::InitializeFromComponent(UActorComponent* Component)
{
	Super::InitializeFromComponent(Component);

	// Initialize properties not handled by InitializeFromPrimitiveComponent
	UPrimitiveComponent* PrimitiveComponent = CastChecked<UPrimitiveComponent>(Component);
	PrimitiveComponent->UpdateComponentToWorld();
	LocalTransform = PrimitiveComponent->GetComponentToWorld();
	WorldTransform = LocalTransform;
	bIsVisible = PrimitiveComponent->IsVisible();
	bStaticWhenNotMoveable = PrimitiveComponent->GetStaticWhenNotMoveable();
	bFillCollisionUnderneathForNavmesh = PrimitiveComponent->bFillCollisionUnderneathForNavmesh;
	bRasterizeAsFilledConvexVolume = PrimitiveComponent->bRasterizeAsFilledConvexVolume;
	bCanEverAffectNavigation = PrimitiveComponent->CanEverAffectNavigation();
	CustomPrimitiveData = PrimitiveComponent->GetCustomPrimitiveData();
	DetailMode = PrimitiveComponent->DetailMode;
	bHasCustomNavigableGeometry = PrimitiveComponent->bHasCustomNavigableGeometry;
	BodyInstance.CopyBodyInstancePropertiesFrom(&PrimitiveComponent->BodyInstance);
	RuntimeVirtualTextures = PrimitiveComponent->GetRuntimeVirtualTextures();

	// Initialize SceneProxyDesc from component
	InitializeSceneProxyDescFromComponent(Component);

	// Reset some values that are not used in FastGeo
	ResetSceneProxyDescUnsupportedProperties();
}

UClass* FFastGeoPrimitiveComponent::GetEditorProxyClass() const
{
	return UFastGeoPrimitiveComponentEditorProxy::StaticClass();
}

void FFastGeoPrimitiveComponent::ResetSceneProxyDescUnsupportedProperties()
{
	// Unsupported properties
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneProxyDesc.bLevelInstanceEditingState = false;
	SceneProxyDesc.bSelectable = false;
	SceneProxyDesc.bUseEditorCompositing = false;
	SceneProxyDesc.bIsBeingMovedByEditor = false;
	SceneProxyDesc.bSelected = false;
	SceneProxyDesc.bIndividuallySelected = false;
	SceneProxyDesc.bShouldRenderSelected = false;
	SceneProxyDesc.bWantsEditorEffects = false;
	SceneProxyDesc.bIsHiddenEd = false;
	SceneProxyDesc.bIsOwnerEditorOnly = false;
	SceneProxyDesc.bIsOwnedByFoliage = false;
	SceneProxyDesc.HiddenEditorViews = 0;
	SceneProxyDesc.OverlayColor = FColor(EForceInit::ForceInitToZero);
	SceneProxyDesc.Component = nullptr;

	// Properties that will be initialized by InitializeSceneProxyDescDynamicProperties
	SceneProxyDesc.ComponentId = FPrimitiveComponentId();
	SceneProxyDesc.StatId = TStatId();
	SceneProxyDesc.Owner = nullptr;
	SceneProxyDesc.World = nullptr;
	SceneProxyDesc.CustomPrimitiveData = nullptr;
	SceneProxyDesc.Scene = nullptr;
	SceneProxyDesc.PrimitiveComponentInterface = nullptr;
	SceneProxyDesc.FeatureLevel = ERHIFeatureLevel::Num;
	SceneProxyDesc.RuntimeVirtualTextures = TArrayView<URuntimeVirtualTexture*>();
	SceneProxyDesc.bIsVisible = false;
	SceneProxyDesc.bShouldRenderProxyFallbackToDefaultMaterial = false;
#if MESH_DRAW_COMMAND_STATS
	SceneProxyDesc.MeshDrawCommandStatsCategory = NAME_None;
#endif
}
#endif

bool FFastGeoPrimitiveComponent::IsDrawnInGame() const
{
	// Drawn in game must consider both the component bIsVisible flag AND the bIsHidden flag (which actually originates from the actor bHiddenInGame property)
	// This logic mimics what is done to initialize FPrimitiveSceneProxy::DrawInGame
	return GetSceneProxyDesc().bIsVisible && !GetSceneProxyDesc().bIsHidden;
}

EComponentMobility::Type FFastGeoPrimitiveComponent::GetMobility() const
{
	return GetSceneProxyDesc().Mobility;
}

void FFastGeoPrimitiveComponent::UpdateVisibility()
{
	// Update SceneProxyDesc.bIsVisible as it's dependant on component and component cluster visibility
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneProxyDesc.bIsVisible = bIsVisible && GetOwnerComponentCluster()->IsVisible();
#if !UE_BUILD_SHIPPING
	SceneProxyDesc.bIsVisible = SceneProxyDesc.bIsVisible && FastGeo::GShowFastGeo;
#endif
}

void FFastGeoPrimitiveComponent::InitializeSceneProxyDescDynamicProperties()
{
	check(GetWorld());
	check(GetScene());

#if WITH_EDITOR
	ResetSceneProxyDescUnsupportedProperties();
#endif

	// Initialize non-serialized properties
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneProxyDesc.ComponentId = GetPrimitiveSceneId();
	UObject const* AdditionalStatObjectPtr = AdditionalStatObject();
	SceneProxyDesc.StatId = AdditionalStatObjectPtr ? AdditionalStatObjectPtr->GetStatID(true) : TStatId();
	SceneProxyDesc.Owner = GetOwnerContainer();
#if !WITH_STATE_STREAM
	SceneProxyDesc.World = GetWorld();
#endif
	SceneProxyDesc.CustomPrimitiveData = &CustomPrimitiveData;
	SceneProxyDesc.Scene = GetScene();
#if WITH_EDITOR
	SceneProxyDesc.PrimitiveComponentInterface = GetEditorProxy<UFastGeoPrimitiveComponentEditorProxy>()->GetPrimitiveComponentInterface();
#endif
	SceneProxyDesc.FeatureLevel = SceneProxyDesc.Scene->GetFeatureLevel();
	TArray<URuntimeVirtualTexture*> const& VirtualTextures = GetRuntimeVirtualTextures();
	SceneProxyDesc.RuntimeVirtualTextures = MakeArrayView(const_cast<URuntimeVirtualTexture**>(VirtualTextures.GetData()), VirtualTextures.Num());
	SceneProxyDesc.bShouldRenderProxyFallbackToDefaultMaterial = ShouldRenderProxyFallbackToDefaultMaterial();
#if MESH_DRAW_COMMAND_STATS
	static FName NAME_FastGeoPrimitiveComponent(TEXT("FastGeoPrimitiveComponent"));
	SceneProxyDesc.MeshDrawCommandStatsCategory = NAME_FastGeoPrimitiveComponent;
#endif
	check(SceneProxyDesc.bIsInstancedStaticMesh == this->IsA<FFastGeoInstancedStaticMeshComponent>());
	UpdateVisibility();
}

FPrimitiveSceneDesc FFastGeoPrimitiveComponent::BuildSceneDesc()
{
	check(GetSceneProxy());

	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	FPrimitiveSceneDesc SceneDesc;
	SceneDesc.SceneProxy = GetSceneProxy();
	SceneDesc.ProxyDesc = &SceneProxyDesc;
	SceneDesc.PrimitiveSceneData = &PrimitiveSceneData;
	SceneDesc.RenderMatrix = GetRenderMatrix();
	SceneDesc.AttachmentRootPosition = GetTransform().GetTranslation();
	SceneDesc.LocalBounds = LocalBounds;
	SceneDesc.Bounds = GetBounds();
	SceneDesc.Mobility = SceneProxyDesc.Mobility;

	return SceneDesc;
}

void FFastGeoPrimitiveComponent::CreateRenderState(FRegisterComponentContext* Context)
{
	FWriteScopeLock WriteLock(Lock.Get());
	ProxyState = EProxyCreationState::Creating;
	bRenderStateDirty = false;

#if WITH_EDITOR
	GetEditorProxy<EditorProxyType>()->NotifyRenderStateChanged();
#endif
	
	FSceneInterface* Scene = GetScene();
	check(Scene);

	ESceneProxyCreationError Error;
	if (FPrimitiveSceneProxy* SceneProxy = CreateSceneProxy(&Error))
	{
		SceneProxy->SetPrimitiveColor(GetDebugColor());
		check(GetSceneProxy());
		FPrimitiveSceneDesc Desc = BuildSceneDesc();
		Scene->AddPrimitive(&Desc);

		ProxyState = EProxyCreationState::Created;
	}
	else if (Error == ESceneProxyCreationError::WaitingPSOs)
	{
		ProxyState = EProxyCreationState::Delayed;
	}
	else
	{
		ProxyState = EProxyCreationState::None;
	}
}

FFastGeoPrimitiveComponent::FFastGeoDestroyRenderStateContext::FFastGeoDestroyRenderStateContext(FSceneInterface* InScene)
	: Scene(InScene)
{
}

FFastGeoPrimitiveComponent::FFastGeoDestroyRenderStateContext::~FFastGeoDestroyRenderStateContext()
{
	if (HasPendingWork())
	{
		Scene->BatchRemovePrimitives(MoveTemp(PrimitiveSceneProxies));
	}
}

bool FFastGeoPrimitiveComponent::FFastGeoDestroyRenderStateContext::HasPendingWork() const
{
	return !PrimitiveSceneProxies.IsEmpty();
}

void FFastGeoPrimitiveComponent::FFastGeoDestroyRenderStateContext::DestroyProxy(FFastGeoDestroyRenderStateContext* InContext, FPrimitiveSceneProxy* InPrimitiveSceneProxy)
{
	if (InContext)
	{
		InContext->PrimitiveSceneProxies.Add(InPrimitiveSceneProxy);
	}
	else
	{
		InPrimitiveSceneProxy->GetScene().BatchRemovePrimitives({ InPrimitiveSceneProxy });
	}
}

void FFastGeoPrimitiveComponent::DestroyRenderState(FFastGeoDestroyRenderStateContext* Context)
{
	FWriteScopeLock WriteLock(Lock.Get());
	if (GetSceneProxy())
	{
		check(ProxyState == EProxyCreationState::Created);

		FFastGeoDestroyRenderStateContext::DestroyProxy(Context, GetSceneProxy());

		PrimitiveSceneData.SceneProxy = nullptr;

		ProxyState = EProxyCreationState::Pending;

		bRenderStateDirty = false;

#if WITH_EDITOR
		GetEditorProxy<EditorProxyType>()->NotifyRenderStateChanged();
#endif
	}
}

void FFastGeoPrimitiveComponent::OnAsyncCreatePhysicsState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoPrimitiveComponent::OnAsyncCreatePhysicsState);

	Super::OnAsyncCreatePhysicsState();

	// if we have a scene, we don't want to disable all physics and we have no bodyinstance already
	if (!BodyInstance.IsValidBodyInstance())
	{
		if (UBodySetup* BodySetup = GetBodySetup())
		{
			// Create new BodyInstance at given location.
			FTransform BodyTransform = WorldTransform;

			// Here we make sure we don't have zero scale. This still results in a body being made and placed in
			// world (very small) but is consistent with a body scaled to zero.
			const FVector BodyScale = BodyTransform.GetScale3D();
			if (BodyScale.IsNearlyZero())
			{
				BodyTransform.SetScale3D(FVector(UE_KINDA_SMALL_NUMBER));
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if ((BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision) && (FMath::IsNearlyZero(BodyScale.X) || FMath::IsNearlyZero(BodyScale.Y) || FMath::IsNearlyZero(BodyScale.Z)))
			{
				UE_LOG(LogFastGeoStreaming, Warning, TEXT("Scale for FastGeoPrimitiveComponent has a component set to zero, which will result in a bad body instance. Scale:%s"), *BodyScale.ToString());

				// User warning has been output - fix up the scale to be valid for physics
				BodyTransform.SetScale3D(FVector(
					FMath::IsNearlyZero(BodyScale.X) ? UE_KINDA_SMALL_NUMBER : BodyScale.X,
					FMath::IsNearlyZero(BodyScale.Y) ? UE_KINDA_SMALL_NUMBER : BodyScale.Y,
					FMath::IsNearlyZero(BodyScale.Z) ? UE_KINDA_SMALL_NUMBER : BodyScale.Z
				));
			}
#endif

			// Initialize BodyInstanceOwner
			BodyInstanceOwner.Initialize(this);

			// Initialize the body instance
			BodyInstance.InitBody(BodySetup, BodyTransform, nullptr, GetWorld()->GetPhysicsScene(), FInitBodySpawnParams(IsStaticPhysics(), /*bPhysicsTypeDeterminesSimulation*/false), &BodyInstanceOwner);

			// Assign BodyInstanceOwner
			if (Chaos::FSingleParticlePhysicsProxy* ProxyHandle = BodyInstance.GetPhysicsActor())
			{
				if (Chaos::FPhysicsObject* PhysicsObject = BodyInstance.IsValidBodyInstance() ? BodyInstance.GetPhysicsActor()->GetPhysicsObject() : nullptr)
				{
					TArrayView<Chaos::FPhysicsObject*> PhysicsObjects(&PhysicsObject, 1);
					FPhysicsObjectExternalInterface::LockWrite(PhysicsObjects)->SetUserDefinedEntity(PhysicsObjects, &BodyInstanceOwner);
				}
			}
		}
	}
}

void FFastGeoPrimitiveComponent::OnAsyncDestroyPhysicsStateBegin_GameThread()
{
	check(!AsyncTermBodyPayload.IsSet());
	AsyncTermBodyPayload = BodyInstance.StartAsyncTermBody_GameThread();
	check(!BodyInstance.IsValidBodyInstance());

	Super::OnAsyncDestroyPhysicsStateBegin_GameThread();
}

void FFastGeoPrimitiveComponent::OnAsyncDestroyPhysicsStateEnd_GameThread()
{
	Super::OnAsyncDestroyPhysicsStateEnd_GameThread();

	// Reset BodyInstanceOwner
	BodyInstanceOwner.Uninitialize();
}

void FFastGeoPrimitiveComponent::OnAsyncDestroyPhysicsState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoPrimitiveComponent::OnAsyncDestroyPhysicsState);

	// We tell the BodyInstance to shut down the physics-engine data.
	if (ensure(AsyncTermBodyPayload.IsSet()))
	{
		// Remove all user defined entities
		if (Chaos::FPhysicsObject* PhysicsObject = AsyncTermBodyPayload->GetPhysicsActor() ? AsyncTermBodyPayload->GetPhysicsActor()->GetPhysicsObject() : nullptr)
		{
			TArrayView<Chaos::FPhysicsObject*> PhysicsObjects(&PhysicsObject, 1);
			FPhysicsObjectExternalInterface::LockWrite(PhysicsObjects)->SetUserDefinedEntity(PhysicsObjects, nullptr);
		}

		FBodyInstance::AsyncTermBody(AsyncTermBodyPayload.GetValue());
		AsyncTermBodyPayload.Reset();
	}

	Super::OnAsyncDestroyPhysicsState();
}

bool FFastGeoPrimitiveComponent::IsRenderStateCreated() const
{
	return ProxyState == EProxyCreationState::Created;
}

bool FFastGeoPrimitiveComponent::IsRenderStateDelayed() const
{
	return ProxyState == EProxyCreationState::Delayed;
}

bool FFastGeoPrimitiveComponent::IsRenderStateDirty() const
{
	return bRenderStateDirty;
}

bool FFastGeoPrimitiveComponent::ShouldCreateRenderState() const
{
	if (!FApp::CanEverRender())
	{
		return false;
	}

	// If the detail mode setting allows it, add it to the scene.
	const bool bDetailModeAllowsRendering = DetailMode <= GetCachedScalabilityCVars().DetailMode;
	if (!bDetailModeAllowsRendering)
	{
		return false;
	}

	const FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	const bool bShouldCreateRenderState = (bIsVisible && !SceneProxyDesc.bIsHidden) || SceneProxyDesc.bCastHiddenShadow || SceneProxyDesc.bAffectIndirectLightingWhileHidden || SceneProxyDesc.bRayTracingFarField;
	return bShouldCreateRenderState;
}

bool FFastGeoPrimitiveComponent::IsFirstPersonRelevant() const
{
	return GetSceneProxyDesc().IsFirstPersonRelevant();
}

bool FFastGeoPrimitiveComponent::IsCollisionEnabled() const
{
	return GetSceneProxyDesc().bCollisionEnabled;
}

FSceneInterface* FFastGeoPrimitiveComponent::GetScene() const
{
	if (UWorld* World = GetWorld())
	{
		return World->Scene;
	}
	return nullptr;
}

FPrimitiveSceneProxy* FFastGeoPrimitiveComponent::GetSceneProxy() const
{
	return PrimitiveSceneData.SceneProxy;
}

void FFastGeoPrimitiveComponent::MarkRenderStateDirty()
{
	if ((IsRenderStateCreated() || IsRenderStateDelayed()) && !IsRenderStateDirty())
	{
		bRenderStateDirty = true;

		UFastGeoWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>();
		if (ensure(WorldSubsystem))
		{
			WorldSubsystem->AddToComponentsPendingRecreate(this);
		}
	}
}

const FTransform& FFastGeoPrimitiveComponent::GetTransform() const
{
	return WorldTransform;
}

const FBoxSphereBounds& FFastGeoPrimitiveComponent::GetBounds() const
{
	return WorldBounds;
}

FMatrix FFastGeoPrimitiveComponent::GetRenderMatrix() const
{
	return GetTransform().ToMatrixWithScale();
}

float FFastGeoPrimitiveComponent::GetLastRenderTimeOnScreen() const
{
	return PrimitiveSceneData.LastRenderTimeOnScreen;
}

void FFastGeoPrimitiveComponent::SetCollisionEnabled(bool bInCollisionEnabled)
{
	FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	SceneProxyDesc.bCollisionEnabled = bInCollisionEnabled;
}

void FFastGeoPrimitiveComponent::InitializeDynamicProperties()
{
	Super::InitializeDynamicProperties();

#if !WITH_EDITOR
	BodyInstance.FixupData(GetOwnerContainer());
#endif
}

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
struct FFastGeoPSOPrecacheFinishedTask
{
	explicit FFastGeoPSOPrecacheFinishedTask(FFastGeoPrimitiveComponent* InPrimitiveComponent, int32 InJobSetThatJustCompleted)
		: WeakPrimitiveComponent(InPrimitiveComponent)
		, JobSetThatJustCompleted(InJobSetThatJustCompleted)
	{
	}

	static TStatId GetStatId() { return TStatId(); }
	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (FFastGeoPrimitiveComponent* PrimitiveComponent = WeakPrimitiveComponent.Get<FFastGeoPrimitiveComponent>())
		{
			// Validate that the component is still part of a streamed-in level
			if (PrimitiveComponent->GetWorld())
			{
				PrimitiveComponent->OnPrecacheFinished(JobSetThatJustCompleted);
			}
		}
	}

	FWeakFastGeoComponent WeakPrimitiveComponent;
	int32 JobSetThatJustCompleted;
};

void FFastGeoPrimitiveComponent::OnPrecacheFinished(int32 JobSetThatJustCompleted)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PSOPrecacheFinishedTask);
	int32 CurrJobSetCompleted = LatestPSOPrecacheJobSetCompleted.load();
	while (CurrJobSetCompleted < JobSetThatJustCompleted && !LatestPSOPrecacheJobSetCompleted.compare_exchange_weak(CurrJobSetCompleted, JobSetThatJustCompleted)) {}
	MarkRenderStateDirty();
}

void FFastGeoPrimitiveComponent::RequestRecreateRenderStateWhenPSOPrecacheFinished(const FGraphEventArray& PSOPrecacheCompileEvents)
{
	// If the proxy creation strategy relies on knowing when the precached PSO has been compiled,
	// schedule a task to mark the render state dirty when all PSOs are compiled so the proxy gets recreated.
	if (GetPSOPrecacheProxyCreationStrategy() != EPSOPrecacheProxyCreationStrategy::AlwaysCreate)
	{
		LatestPSOPrecacheJobSet++;
		// Even if PSOPrecacheCompileEvents is empty, still push the completion task as it needs to run on the Game Thread and call MarkRenderStateDirty
		TGraphTask<FFastGeoPSOPrecacheFinishedTask>::CreateTask(&PSOPrecacheCompileEvents).ConstructAndDispatchWhenReady(this, LatestPSOPrecacheJobSet);
	}
	bPSOPrecacheCalled = true;
}

void FFastGeoPrimitiveComponent::SetupPrecachePSOParams(FPSOPrecacheParams& Params)
{
	const FPrimitiveSceneProxyDesc& SceneProxyDesc = GetSceneProxyDesc();
	auto IsPrecomputedLightingValid = []() { return false; };
	Params.bRenderInMainPass = SceneProxyDesc.bRenderInMainPass;
	Params.bRenderInDepthPass = SceneProxyDesc.bRenderInDepthPass;
	Params.bStaticLighting = SceneProxyDesc.bHasStaticLighting;
	Params.bUsesIndirectLightingCache = Params.bStaticLighting && SceneProxyDesc.IndirectLightingCacheQuality != ILCQ_Off && (!IsPrecomputedLightingValid() || SceneProxyDesc.LightmapType == ELightmapType::ForceVolumetric);
	Params.bAffectDynamicIndirectLighting = SceneProxyDesc.bAffectDynamicIndirectLighting;
	Params.bCastShadow = SceneProxyDesc.CastShadow;
	// Custom depth can be toggled at runtime with PSO precache call so assume it might be needed when depth pass is needed
	// Ideally precache those with lower priority and don't wait on these (UE-174426)
	Params.bRenderCustomDepth = SceneProxyDesc.bRenderInDepthPass;
	Params.bCastShadowAsTwoSided = SceneProxyDesc.bCastShadowAsTwoSided;
	Params.SetMobility(SceneProxyDesc.Mobility);
	Params.SetStencilWriteMask(FRendererStencilMaskEvaluation::ToStencilMask(SceneProxyDesc.CustomDepthStencilWriteMask));

	TArray<UMaterialInterface*> UsedMaterials;
	GetUsedMaterials(UsedMaterials);
	for (const UMaterialInterface* MaterialInterface : UsedMaterials)
	{
		if (MaterialInterface && MaterialInterface->IsUsingWorldPositionOffset_Concurrent(GMaxRHIFeatureLevel))
		{
			Params.bAnyMaterialHasWorldPositionOffset = true;
			break;
		}
	}
}

#endif

bool FFastGeoPrimitiveComponent::IsPSOPrecaching() const
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	// Consider as precaching when marked as required to do PSOs precaching (even if task has not been launched yet) 
	return bPSOPrecacheRequired || (LatestPSOPrecacheJobSetCompleted != LatestPSOPrecacheJobSet);
#else
	return false;
#endif
}

bool FFastGeoPrimitiveComponent::ShouldRenderProxyFallbackToDefaultMaterial() const
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	return IsPSOPrecaching() && GetPSOPrecacheProxyCreationStrategy() == EPSOPrecacheProxyCreationStrategy::UseDefaultMaterialUntilPSOPrecached;
#else
	return false;
#endif
}

bool FFastGeoPrimitiveComponent::CheckPSOPrecachingAndBoostPriority(EPSOPrecachePriority NewPSOPrecachePriority)
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	bool bPrecacheStillRunning = IsPSOPrecaching();

	ensure(!IsComponentPSOPrecachingEnabled() || bPSOPrecacheCalled || bPSOPrecacheRequired);
	check(NewPSOPrecachePriority == EPSOPrecachePriority::High || NewPSOPrecachePriority == EPSOPrecachePriority::Highest);

	if (bPrecacheStillRunning && PSOPrecacheRequestPriority < NewPSOPrecachePriority)
	{
		// Only boost PSO priority if PSO task was started
		if (LatestPSOPrecacheJobSetCompleted != LatestPSOPrecacheJobSet)
		{
			BoostPSOPriority(NewPSOPrecachePriority, MaterialPSOPrecacheRequestIDs);
		}
		PSOPrecacheRequestPriority = NewPSOPrecachePriority;
	}
	return bPrecacheStillRunning;
#else
	return false;
#endif
}


void FFastGeoPrimitiveComponent::MarkPrecachePSOsRequired()
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	if (bPSOPrecacheCalled || !FApp::CanEverRender() || !IsComponentPSOPrecachingEnabled())
	{
		return;
	}
	bPSOPrecacheRequired = true;
	PSOPrecacheRequestPriority = EPSOPrecachePriority::Medium;
#endif
}

void FFastGeoPrimitiveComponent::PrecachePSOs()
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoPrimitiveComponent::PrecachePSOs);
	check(bPSOPrecacheRequired);

	// Clear the current request data
	MaterialPSOPrecacheRequestIDs.Empty();

	// Collect the data from the derived classes
	FPSOPrecacheParams PSOPrecacheParams;
	SetupPrecachePSOParams(PSOPrecacheParams);
	FMaterialInterfacePSOPrecacheParamsList PSOPrecacheDataArray;
	CollectPSOPrecacheData(PSOPrecacheParams, PSOPrecacheDataArray);
	// Set priority
	for (FMaterialInterfacePSOPrecacheParams& Params : PSOPrecacheDataArray)
	{
		Params.Priority = PSOPrecacheRequestPriority;
	}

	FGraphEventArray GraphEvents;
	PrecacheMaterialPSOs(PSOPrecacheDataArray, MaterialPSOPrecacheRequestIDs, GraphEvents);

	RequestRecreateRenderStateWhenPSOPrecacheFinished(GraphEvents);
	bPSOPrecacheRequired = false;
#endif
}

bool FFastGeoPrimitiveComponent::IsNavigationRelevant() const
{
	if (!bCanEverAffectNavigation)
	{
		return false;
	}

	if (HasCustomNavigableGeometry() >= EHasCustomNavigableGeometry::EvenIfNotCollidable)
	{
		return true;
	}

	auto GetCollisionEnabled = [this]() -> ECollisionEnabled::Type
	{
		if (!IsCollisionEnabled())
		{
			return ECollisionEnabled::NoCollision;
		}
		return BodyInstance.GetCollisionEnabled(false);
	};

	auto IsQueryCollisionEnabled = [&GetCollisionEnabled]()
	{
		return CollisionEnabledHasQuery(GetCollisionEnabled());
	};

	auto GetCollisionResponseToChannels = [this]()
	{
		return BodyInstance.GetResponseToChannels();
	};

	const FCollisionResponseContainer& ResponseToChannels = GetCollisionResponseToChannels();
	return IsQueryCollisionEnabled() &&
		(ResponseToChannels.GetResponse(ECC_Pawn) == ECR_Block || ResponseToChannels.GetResponse(ECC_Vehicle) == ECR_Block);
}

FBox FFastGeoPrimitiveComponent::GetNavigationBounds() const
{
	return GetBounds().GetBox();
}

void FFastGeoPrimitiveComponent::GetNavigationData(FNavigationRelevantData& OutData) const
{
	FPrimitiveComponentHelper::GetNavigationData(*this, OutData);
}

EHasCustomNavigableGeometry::Type FFastGeoPrimitiveComponent::HasCustomNavigableGeometry() const
{
	return bHasCustomNavigableGeometry;
}

bool FFastGeoPrimitiveComponent::IsStaticPhysics() const
{
	return GetSceneProxyDesc().Mobility != EComponentMobility::Movable && bStaticWhenNotMoveable;
}

UObject* FFastGeoPrimitiveComponent::GetSourceObject() const
{
	return GetOwnerContainer();
}

ECollisionResponse FFastGeoPrimitiveComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	return BodyInstance.GetResponseToChannel(Channel);
}

FPrimitiveMaterialPropertyDescriptor FFastGeoPrimitiveComponent::GetUsedMaterialPropertyDesc(ERHIFeatureLevel::Type FeatureLevel) const
{
	return FPrimitiveComponentHelper::GetUsedMaterialPropertyDesc(*this, FeatureLevel);
}

const FName FFastGeoPhysicsBodyInstanceOwner::NAME_FastGeoPhysicsBodyInstanceOwner = TEXT("FastGeoPhysicsBodyInstanceOwner");

FFastGeoPhysicsBodyInstanceOwner::FFastGeoPhysicsBodyInstanceOwner()
	: FChaosUserDefinedEntity(FFastGeoPhysicsBodyInstanceOwner::NAME_FastGeoPhysicsBodyInstanceOwner)
	, OwnerComponent(nullptr)
{}

void FFastGeoPhysicsBodyInstanceOwner::Uninitialize()
{
	Initialize(nullptr);
}

void FFastGeoPhysicsBodyInstanceOwner::Initialize(FFastGeoPrimitiveComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
	UFastGeoContainer* NewContainer = OwnerComponent ? OwnerComponent->GetOwnerContainer() : nullptr;
	check(!OwnerComponent || NewContainer);
	check(OwnerContainer.IsExplicitlyNull() || !NewContainer || OwnerContainer == NewContainer);
	OwnerContainer = NewContainer;
}

TWeakObjectPtr<UObject> FFastGeoPhysicsBodyInstanceOwner::GetOwnerObject()
{
	return OwnerContainer;
}

IPhysicsBodyInstanceOwner* FFastGeoPhysicsBodyInstanceOwner::GetPhysicsBodyInstanceOwner(FChaosUserDefinedEntity* InUserDefinedEntity)
{
	if (InUserDefinedEntity && InUserDefinedEntity->GetEntityTypeName() == NAME_FastGeoPhysicsBodyInstanceOwner)
	{
		FFastGeoPhysicsBodyInstanceOwner* FastGeoPhysicsBodyInstanceOwner = static_cast<FFastGeoPhysicsBodyInstanceOwner*>(InUserDefinedEntity);
		check(FastGeoPhysicsBodyInstanceOwner->GetOwnerObject().IsValid());
		return FastGeoPhysicsBodyInstanceOwner;
	}
	return nullptr;
}

bool FFastGeoPhysicsBodyInstanceOwner::IsStaticPhysics() const
{
	check(OwnerContainer.IsValid());
	return OwnerComponent->IsStaticPhysics();
}

UObject* FFastGeoPhysicsBodyInstanceOwner::GetSourceObject() const
{
	check(OwnerContainer.IsValid());
	return OwnerComponent->GetSourceObject();
}

ECollisionResponse FFastGeoPhysicsBodyInstanceOwner::GetCollisionResponseToChannel(ECollisionChannel Channel) const 
{
	check(OwnerContainer.IsValid());
	return OwnerComponent->GetCollisionResponseToChannel(Channel);
}

UPhysicalMaterial* FFastGeoPhysicsBodyInstanceOwner::GetPhysicalMaterial() const
{
	check(OwnerContainer.IsValid());
	return OwnerComponent->GetPhysicalMaterial();
}

void FFastGeoPhysicsBodyInstanceOwner::GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& OutPhysMaterials, TArray<FPhysicalMaterialMaskParams>* OutPhysMaterialMasks) const
{
	check(OwnerContainer.IsValid());
	return OwnerComponent->GetComplexPhysicalMaterials(OutPhysMaterials, OutPhysMaterialMasks);
}

#if WITH_EDITOR
void UFastGeoPrimitiveComponentEditorProxy::NotifyRenderStateChanged()
{
	FObjectCacheEventSink::NotifyRenderStateChanged_Concurrent(this);
}

IPrimitiveComponent* UFastGeoPrimitiveComponentEditorProxy::GetPrimitiveComponentInterface()
{
	return this;
}

void UFastGeoPrimitiveComponentEditorProxy::BeginDestroy()
{
	Super::BeginDestroy();

	if (!IsTemplate())
	{
		NotifyRenderStateChanged();
	}
}

//~ Begin IPrimitiveComponent interface
bool UFastGeoPrimitiveComponentEditorProxy::IsRenderStateCreated() const
{
	return GetComponent<ComponentType>().IsRenderStateCreated();
}

bool UFastGeoPrimitiveComponentEditorProxy::IsRenderStateDirty() const
{
	return GetComponent<ComponentType>().IsRenderStateDirty();
}

bool UFastGeoPrimitiveComponentEditorProxy::ShouldCreateRenderState() const
{
	return GetComponent<ComponentType>().ShouldCreateRenderState();
}

bool UFastGeoPrimitiveComponentEditorProxy::IsRegistered() const
{
	return GetComponent<ComponentType>().IsRegistered();
}

bool UFastGeoPrimitiveComponentEditorProxy::IsUnreachable() const
{
	return Super::IsUnreachable();
}

UWorld* UFastGeoPrimitiveComponentEditorProxy::GetWorld() const
{
	return GetComponent<ComponentType>().GetWorld();
}

FSceneInterface* UFastGeoPrimitiveComponentEditorProxy::GetScene() const
{
	return GetComponent<ComponentType>().GetScene();
}

FPrimitiveSceneProxy* UFastGeoPrimitiveComponentEditorProxy::GetSceneProxy() const
{
	return GetComponent<ComponentType>().GetSceneProxy();
}

void UFastGeoPrimitiveComponentEditorProxy::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	GetComponent<ComponentType>().GetUsedMaterials(OutMaterials, bGetDebugMaterials);
}

void UFastGeoPrimitiveComponentEditorProxy::MarkRenderStateDirty()
{
	GetComponent<ComponentType>().MarkRenderStateDirty();
}

void UFastGeoPrimitiveComponentEditorProxy::DestroyRenderState()
{
	GetComponent<ComponentType>().DestroyRenderState(nullptr);
}

void UFastGeoPrimitiveComponentEditorProxy::CreateRenderState(FRegisterComponentContext* Context)
{
	GetComponent<ComponentType>().CreateRenderState(Context);
}

FString UFastGeoPrimitiveComponentEditorProxy::GetName() const
{
	return GetUObject()->GetName();
}

FString UFastGeoPrimitiveComponentEditorProxy::GetFullName() const
{
	return GetUObject()->GetFullName();
}

FTransform UFastGeoPrimitiveComponentEditorProxy::GetTransform() const
{
	return GetComponent<ComponentType>().GetTransform();
}

FBoxSphereBounds UFastGeoPrimitiveComponentEditorProxy::GetBounds() const
{
	return GetComponent<ComponentType>().GetBounds();
}

float UFastGeoPrimitiveComponentEditorProxy::GetLastRenderTimeOnScreen() const
{
	return GetComponent<ComponentType>().GetLastRenderTimeOnScreen();
}

void UFastGeoPrimitiveComponentEditorProxy::GetPrimitiveStats(FPrimitiveStats& PrimitiveStats) const
{
}

UObject* UFastGeoPrimitiveComponentEditorProxy::GetUObject()
{
	return this;
}

const UObject* UFastGeoPrimitiveComponentEditorProxy::GetUObject() const
{
	return this;
}

void UFastGeoPrimitiveComponentEditorProxy::PrecachePSOs()
{
	GetComponent<ComponentType>().PrecachePSOs();
}

UObject* UFastGeoPrimitiveComponentEditorProxy::GetOwner() const
{
	return GetOuter();
}

FString UFastGeoPrimitiveComponentEditorProxy::GetOwnerName() const
{
	return GetOwner()->GetName();
}

FPrimitiveSceneProxy* UFastGeoPrimitiveComponentEditorProxy::CreateSceneProxy()
{
	return GetComponent<ComponentType>().CreateSceneProxy();
}

HHitProxy* UFastGeoPrimitiveComponentEditorProxy::CreateMeshHitProxy(int32 SectionIndex, int32 MaterialIndex)
{
	return nullptr;
}

HHitProxy* UFastGeoPrimitiveComponentEditorProxy::CreatePrimitiveHitProxies(TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
	return nullptr;
}
//~ End IPrimitiveComponent interface
#endif

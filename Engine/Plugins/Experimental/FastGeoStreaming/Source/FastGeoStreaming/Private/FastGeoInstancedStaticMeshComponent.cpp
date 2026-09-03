// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoInstancedStaticMeshComponent.h"
#include "FastGeoComponentCluster.h"
#include "FastGeoContainer.h"
#include "FastGeoHLOD.h"
#include "FastGeoLog.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "Engine/InstancedStaticMesh.h"
#include "InstancedStaticMeshComponentHelper.h"
#include "InstancedStaticMesh/ISMInstanceDataSceneProxy.h"
#include "NaniteVertexFactory.h"
#include "NaniteSceneProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "SceneInterface.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

const FFastGeoElementType FFastGeoInstancedStaticMeshComponent::Type(&FFastGeoStaticMeshComponentBase::Type);

FFastGeoInstancedStaticMeshComponent::FFastGeoInstancedStaticMeshComponent(int32 InComponentIndex, FFastGeoElementType InType)
	: Super(InComponentIndex, InType)
{
}

void FFastGeoInstancedStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize persistent data from FFastGeoInstancedStaticMeshComponent
	PerInstanceSMData.BulkSerialize(Ar);
	Ar << InstancingRandomSeed;
	PerInstanceSMCustomData.BulkSerialize(Ar);
	Ar << AdditionalRandomSeeds;
	Ar << NavigationBounds;

	// Serialize persistent data from FInstancedStaticMeshSceneProxyDesc
	Ar << SceneProxyDesc.InstanceLODDistanceScale;
	Ar << SceneProxyDesc.InstanceMinDrawDistance;
	Ar << SceneProxyDesc.InstanceStartCullDistance;
	Ar << SceneProxyDesc.InstanceEndCullDistance;
	FArchive_Serialize_BitfieldBool(Ar, SceneProxyDesc.bUseGpuLodSelection);
}

#if WITH_EDITOR
void FFastGeoInstancedStaticMeshComponent::InitializeSceneProxyDescFromComponent(UActorComponent* Component)
{
	UInstancedStaticMeshComponent* InstancedStaticMeshComponent = CastChecked<UInstancedStaticMeshComponent>(Component);
	SceneProxyDesc.InitializeFromInstancedStaticMeshComponent(InstancedStaticMeshComponent);
}

void FFastGeoInstancedStaticMeshComponent::InitializeFromComponent(UActorComponent* Component)
{
	Super::InitializeFromComponent(Component);

	UInstancedStaticMeshComponent* ISMComponent = CastChecked<UInstancedStaticMeshComponent>(Component);
	SceneProxyDesc.bCollisionEnabled = !!SceneProxyDesc.bCollisionEnabled && !ISMComponent->bDisableCollision;
	AdditionalRandomSeeds = ISMComponent->AdditionalRandomSeeds;
	PerInstanceSMData = ISMComponent->PerInstanceSMData;
	PerInstanceSMCustomData = ISMComponent->PerInstanceSMCustomData;
	InstancingRandomSeed = ISMComponent->InstancingRandomSeed;

	// ISMC with no instances should never be transformed to FastGeo
	check(!PerInstanceSMData.IsEmpty());

	LocalBounds = CalculateBounds(EBoundsType::LocalBounds);
	WorldBounds = CalculateBounds(EBoundsType::WorldBounds);
	NavigationBounds = CalculateBounds(EBoundsType::NavigationBounds).GetBox();
}

UClass* FFastGeoInstancedStaticMeshComponent::GetEditorProxyClass() const
{
	return UFastGeoInstancedStaticMeshComponentEditorProxy::StaticClass();
}

void FFastGeoInstancedStaticMeshComponent::ResetSceneProxyDescUnsupportedProperties()
{
	Super::ResetSceneProxyDescUnsupportedProperties();

	SceneProxyDesc.InstanceDataSceneProxy = nullptr;
	SceneProxyDesc.bHasSelectedInstances = false;
}
#endif

void FFastGeoInstancedStaticMeshComponent::ApplyWorldTransform(const FTransform& InTransform)
{
	Super::ApplyWorldTransform(InTransform);

	WorldBounds = CalculateBounds(EBoundsType::WorldBounds);
	NavigationBounds = CalculateBounds(EBoundsType::NavigationBounds).GetBox();
}

TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe>& FFastGeoInstancedStaticMeshComponent::BuildInstanceData()
{
	FInstanceSceneDataBuffers InstanceSceneDataBuffers{};
	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	auto View = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);
	
	// PrimitiveLocalToWorld
	InstanceSceneDataBuffers.SetPrimitiveLocalToWorld(GetRenderMatrix(), AccessTag);
	
	// InstanceLocalBounds
	const FPrimitiveMaterialPropertyDescriptor PrimitiveMaterialDesc = GetUsedMaterialPropertyDesc(GetScene()->GetFeatureLevel());
	const float LocalAbsMaxDisplacement = FMath::Max(-PrimitiveMaterialDesc.MinMaxMaterialDisplacement.X, PrimitiveMaterialDesc.MinMaxMaterialDisplacement.Y) + PrimitiveMaterialDesc.MaxWorldPositionOffsetDisplacement;
	const FVector3f PadExtent = FISMCInstanceDataSceneProxy::GetLocalBoundsPadExtent(View.PrimitiveToRelativeWorld, LocalAbsMaxDisplacement);
	FRenderBounds InstanceLocalBounds = GetStaticMesh()->GetBounds();
	InstanceLocalBounds.Min -= PadExtent;
	InstanceLocalBounds.Max += PadExtent;
	check(!View.Flags.bHasPerInstanceLocalBounds);
	View.InstanceLocalBounds.Add(InstanceLocalBounds);

	// LocalToPrimitiveRelativeWorld
	View.InstanceToPrimitiveRelative.Reserve(PerInstanceSMData.Num());
	for (const auto& SM : PerInstanceSMData)
	{
		FRenderTransform InstanceToPrimitive = SM.Transform;
		FRenderTransform LocalToPrimitiveRelativeWorld = InstanceToPrimitive * View.PrimitiveToRelativeWorld;
		LocalToPrimitiveRelativeWorld.Orthogonalize();
		View.InstanceToPrimitiveRelative.Add(LocalToPrimitiveRelativeWorld);
	}

	// InstanceCustomData
	View.InstanceCustomData = PerInstanceSMCustomData;
	View.NumCustomDataFloats = PerInstanceSMCustomData.Num() && PerInstanceSMData.Num() ? PerInstanceSMCustomData.Num() / PerInstanceSMData.Num() : 0;
	View.Flags.bHasPerInstanceCustomData = PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceCustomData && View.NumCustomDataFloats != 0;
	if (!View.Flags.bHasPerInstanceCustomData)
	{
		View.NumCustomDataFloats = 0;
		View.InstanceCustomData.Reset();
	}

	// InstanceRandomIDs
	View.Flags.bHasPerInstanceRandom = PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceRandom && (PerInstanceSMData.Num() > 0);
	if (View.Flags.bHasPerInstanceRandom && InstanceRandomIDs.IsEmpty())
	{
		InstanceRandomIDs.SetNumZeroed(PerInstanceSMData.Num());
		FRandomStream RandomStream = FRandomStream(InstancingRandomSeed);
		auto AdditionalRandomSeedsIt = AdditionalRandomSeeds.CreateIterator();
		int32 SeedResetIndex = AdditionalRandomSeedsIt ? AdditionalRandomSeedsIt->StartInstanceIndex : INDEX_NONE;
		for (int32 Index = 0; Index < InstanceRandomIDs.Num(); ++Index)
		{
			// Reset the random stream if necessary
			if (Index == SeedResetIndex)
			{
				RandomStream = FRandomStream(AdditionalRandomSeedsIt->RandomSeed);
				AdditionalRandomSeedsIt++;
				SeedResetIndex = AdditionalRandomSeedsIt ? AdditionalRandomSeedsIt->StartInstanceIndex : INDEX_NONE;
			}
			InstanceRandomIDs[Index] = RandomStream.GetFraction();
		}
	}
	View.InstanceRandomIDs = InstanceRandomIDs;

	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);
	InstanceSceneDataBuffers.ValidateData();

	DataProxy = MakeShared<FInstanceDataSceneProxy, ESPMode::ThreadSafe>(MoveTemp(InstanceSceneDataBuffers));
	return DataProxy;
}

FPrimitiveSceneProxy* FFastGeoInstancedStaticMeshComponent::CreateStaticMeshSceneProxy(const Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite)
{
	check(GetWorld());
	check(SceneProxyDesc.Scene);
	check(PerInstanceSMData.Num() > 0);

	SceneProxyDesc.InstanceDataSceneProxy = BuildInstanceData();
	if (bCreateNanite)
	{
		PrimitiveSceneData.SceneProxy = ::new Nanite::FSceneProxy(NaniteMaterials, SceneProxyDesc);
	}
	else
	{
		PrimitiveSceneData.SceneProxy = ::new FInstancedStaticMeshSceneProxy(SceneProxyDesc, SceneProxyDesc.FeatureLevel);
	}
	return PrimitiveSceneData.SceneProxy;
}

void FFastGeoInstancedStaticMeshComponent::OnAsyncCreatePhysicsState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoInstancedStaticMeshComponent::OnAsyncCreatePhysicsState);

	check(InstanceBodies.Num() == 0);

	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
	if (!PhysScene)
	{
		return;
	}

	// Create all the bodies.
	CreateAllInstanceBodies();

	FFastGeoComponent::OnAsyncCreatePhysicsState();
}

void FFastGeoInstancedStaticMeshComponent::OnAsyncDestroyPhysicsStateBegin_GameThread()
{
	FFastGeoComponent::OnAsyncDestroyPhysicsStateBegin_GameThread();

	// Move InstanceBodies in AsyncDestroyPhysicsStatePayload
	check(AsyncDestroyPhysicsStatePayload.IsEmpty());
	AsyncDestroyPhysicsStatePayload = MoveTemp(InstanceBodies);
}

void FFastGeoInstancedStaticMeshComponent::OnAsyncDestroyPhysicsStateEnd_GameThread()
{
	FFastGeoComponent::OnAsyncDestroyPhysicsStateEnd_GameThread();

	// Reset BodyInstanceOwner
	BodyInstanceOwner.Uninitialize();
}

void FFastGeoInstancedStaticMeshComponent::OnAsyncDestroyPhysicsState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoInstancedStaticMeshComponent::OnAsyncDestroyPhysicsState);

	FFastGeoComponent::OnAsyncDestroyPhysicsState();

	// Remove all user defined entities
	TArray<Chaos::FPhysicsObject*> PhysicsObjects = GetAllPhysicsObjects(AsyncDestroyPhysicsStatePayload);
	FPhysicsObjectExternalInterface::LockWrite(PhysicsObjects)->SetUserDefinedEntity(PhysicsObjects, nullptr);

	check(InstanceBodies.IsEmpty());
	for (FBodyInstance*& Instance : AsyncDestroyPhysicsStatePayload)
	{
		if (Instance)
		{
			Instance->TermBody();
			delete Instance;
		}
	}
	AsyncDestroyPhysicsStatePayload.Empty();
}

TArray<Chaos::FPhysicsObject*> FFastGeoInstancedStaticMeshComponent::GetAllPhysicsObjects(TArray<FBodyInstance*> InInstanceBodies)
{
	TArray<Chaos::FPhysicsObject*> Objects;
	Objects.Reserve(InInstanceBodies.Num());
	for (FBodyInstance* InstancedBody : InInstanceBodies)
	{
		if (Chaos::FPhysicsObject* PhysicsObject = InstancedBody && InstancedBody->GetPhysicsActor() ? InstancedBody->GetPhysicsActor()->GetPhysicsObject() : nullptr)
		{
			Objects.Add(PhysicsObject);
		}
	}
	return Objects;
}

void FFastGeoInstancedStaticMeshComponent::CreateAllInstanceBodies()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoInstancedStaticMeshComponent::CreateAllInstanceBodies);
	
	const int32 NumBodies = PerInstanceSMData.Num();
	check(InstanceBodies.Num() == 0);
	check(SceneProxyDesc.Mobility != EComponentMobility::Movable);

	if (UBodySetup* BodySetup = GetBodySetup())
	{
		FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

		if (!BodyInstance.GetOverrideWalkableSlopeOnInstance())
		{
			BodyInstance.SetWalkableSlopeOverride(BodySetup->WalkableSlopeOverride, false);
		}

		InstanceBodies.SetNumUninitialized(NumBodies);

		// Sanitized array does not contain any nulls
		TArray<FBodyInstance*> InstanceBodiesSanitized;
		InstanceBodiesSanitized.Reserve(NumBodies);

		TArray<FTransform> Transforms;
		Transforms.Reserve(NumBodies);
		for (int32 i = 0; i < NumBodies; ++i)
		{
			const FTransform InstanceTM = FTransform(PerInstanceSMData[i].Transform) * WorldTransform;
			if (InstanceTM.GetScale3D().IsNearlyZero())
			{
				InstanceBodies[i] = nullptr;
			}
			else
			{
				FBodyInstance* Instance = new FBodyInstance;

				InstanceBodiesSanitized.Add(Instance);
				InstanceBodies[i] = Instance;
				Instance->CopyBodyInstancePropertiesFrom(&BodyInstance);
				Instance->InstanceBodyIndex = i;
				Instance->bAutoWeld = false;
				Instance->bSimulatePhysics = false;
				Transforms.Add(InstanceTM);
			}
		}

		if (InstanceBodiesSanitized.Num() > 0)
		{
			// Initialize BodyInstanceOwner
			BodyInstanceOwner.Initialize(this);

			// Initialize body instances
			FBodyInstance::InitStaticBodies(MoveTemp(InstanceBodiesSanitized), MoveTemp(Transforms), BodySetup, nullptr, GetWorld()->GetPhysicsScene(), &BodyInstanceOwner);

			// Assign BodyInstanceOwner
			TArray<Chaos::FPhysicsObject*> PhysicsObjects = GetAllPhysicsObjects(InstanceBodies);
			FPhysicsObjectExternalInterface::LockWrite(PhysicsObjects)->SetUserDefinedEntity(PhysicsObjects, &BodyInstanceOwner);
		}
	}
	else
	{
		// In case we get into some bad state where the BodySetup is invalid but bPhysicsStateCreated is true,
		// issue a warning and add nullptrs to InstanceBodies.
		UE_LOG(LogFastGeoStreaming, Warning, TEXT("Instance Static Mesh Component unable to create InstanceBodies!"));
		InstanceBodies.AddZeroed(NumBodies);
	}
}

FBoxSphereBounds FFastGeoInstancedStaticMeshComponent::CalculateBounds(EBoundsType BoundsType)
{
	if (GetStaticMesh() && PerInstanceSMData.Num() > 0)
	{
		const bool bWorldSpace = (BoundsType != EBoundsType::LocalBounds);
		const FBox InstanceBounds = (BoundsType == EBoundsType::NavigationBounds) ? FInstancedStaticMeshComponentHelper::GetInstanceNavigationBounds(*this) : GetStaticMesh()->GetBounds().GetBox();
		const FMatrix ComponentTransformMatrix = WorldTransform.ToMatrixWithScale();
		if (InstanceBounds.IsValid)
		{
			FBoxSphereBounds::Builder BoundsBuilder;
			for (int32 InstanceIndex = 0; InstanceIndex < PerInstanceSMData.Num(); InstanceIndex++)
			{
				if (bWorldSpace)
				{
					BoundsBuilder += InstanceBounds.TransformBy(PerInstanceSMData[InstanceIndex].Transform * ComponentTransformMatrix);
				}
				else
				{
					BoundsBuilder += InstanceBounds.TransformBy(PerInstanceSMData[InstanceIndex].Transform);
				}
			}
			return BoundsBuilder;
		}
	}
	return FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);
};

void FFastGeoInstancedStaticMeshComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	FInstancedStaticMeshComponentHelper::GetNavigationData(*this, Data, FNavDataPerInstanceTransformDelegate::CreateWeakLambda(GetOwnerContainer(), [this](const FBox& AreaBox, TArray<FTransform>& InstanceData)
	{
		FInstancedStaticMeshComponentHelper::GetNavigationPerInstanceTransforms(*this, AreaBox, InstanceData);
	}));
}

FBox FFastGeoInstancedStaticMeshComponent::GetNavigationBounds() const
{
	return NavigationBounds;
}

bool FFastGeoInstancedStaticMeshComponent::IsNavigationRelevant() const
{
	return PerInstanceSMData.Num() > 0 && Super::IsNavigationRelevant();
}

bool FFastGeoInstancedStaticMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	return FInstancedStaticMeshComponentHelper::DoCustomNavigableGeometryExport(*this, GeomExport, FNavDataPerInstanceTransformDelegate::CreateWeakLambda(GetOwnerContainer(), [this](const FBox& AreaBox, TArray<FTransform>& InstanceData)
	{
		FInstancedStaticMeshComponentHelper::GetNavigationPerInstanceTransforms(*this, AreaBox, InstanceData);
	}));
}

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
void FFastGeoInstancedStaticMeshComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	return FInstancedStaticMeshComponentHelper::CollectPSOPrecacheData(*this, BasePrecachePSOParams, OutParams);
}
#endif
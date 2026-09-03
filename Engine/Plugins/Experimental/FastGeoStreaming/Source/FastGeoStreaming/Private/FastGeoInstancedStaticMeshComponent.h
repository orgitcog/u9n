// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FastGeoStaticMeshComponent.h"
#include "InstancedStaticMeshSceneProxyDesc.h"
#include "FastGeoInstancedStaticMeshComponent.generated.h"

class FASTGEOSTREAMING_API FFastGeoInstancedStaticMeshComponent : public FFastGeoStaticMeshComponentBase
{
public:
	typedef FFastGeoStaticMeshComponentBase Super;

	/** Static type identifier for this element class */
	static const FFastGeoElementType Type;

	FFastGeoInstancedStaticMeshComponent(int32 InComponentIndex = INDEX_NONE, FFastGeoElementType InType = Type);
	virtual ~FFastGeoInstancedStaticMeshComponent() = default;

	//~ Begin FFastGeoComponent Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void OnAsyncCreatePhysicsState() override;
	virtual void OnAsyncDestroyPhysicsState() override;
	virtual void OnAsyncDestroyPhysicsStateBegin_GameThread() override;
	virtual void OnAsyncDestroyPhysicsStateEnd_GameThread() override;
#if WITH_EDITOR
	virtual void InitializeFromComponent(UActorComponent* Component) override;
	virtual UClass* GetEditorProxyClass() const override;
#endif
	//~ End FFastGeoComponent Interface

	//~ Begin FFastGeoPrimitiveComponent interface
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override;
	virtual bool IsNavigationRelevant() const override;
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	virtual bool ShouldSkipNavigationDirtyAreaOnAddOrRemove() override { return true; }
	//~ End FFastGeoPrimitiveComponent interface

protected:
	//~ Begin FFastGeoPrimitiveComponent interface
	virtual FPrimitiveSceneProxyDesc& GetSceneProxyDesc() override { return SceneProxyDesc; }
	virtual const FPrimitiveSceneProxyDesc& GetSceneProxyDesc() const override { return SceneProxyDesc; }
#if WITH_EDITOR
	virtual void InitializeSceneProxyDescFromComponent(UActorComponent* Component) override;
	virtual void ResetSceneProxyDescUnsupportedProperties() override;
#endif
	virtual void ApplyWorldTransform(const FTransform& InTransform) override;
	//~ End FFastGeoPrimitiveComponent interface

	//~ Begin FFastGeoStaticMeshComponentBase interface
	virtual FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() override { return SceneProxyDesc; }
	virtual const FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() const override { return SceneProxyDesc; }
	virtual FPrimitiveSceneProxy* CreateStaticMeshSceneProxy(const Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite) override;
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams) override;
#endif
	//~ End FFastGeoStaticMeshComponentBase interface

	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe>& BuildInstanceData();

private:
	enum class EBoundsType
	{
		LocalBounds,
		WorldBounds,
		NavigationBounds
	};
	FBoxSphereBounds CalculateBounds(EBoundsType BoundsType);
	void CreateAllInstanceBodies();
	static TArray<Chaos::FPhysicsObject*> GetAllPhysicsObjects(TArray<FBodyInstance*> InInstanceBodies);

private:
	// Persistent Data
	TArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;
	int32 InstancingRandomSeed = 0;
	TArray<float> PerInstanceSMCustomData;
	TArray<FInstancedStaticMeshRandomSeed> AdditionalRandomSeeds;
	FBox NavigationBounds;
	float InstanceLODDistanceScale = 1.f;
	int32 InstanceStartCullDistance = 0;
	int32 InstanceEndCullDistance = 0;
	uint8 bUseGpuLodSelection : 1 = false;
	FInstancedStaticMeshSceneProxyDesc SceneProxyDesc{};

	// Transient data
	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> DataProxy{};
	TArray<float> InstanceRandomIDs;
	
	/** Physics representation of the instance bodies. */
	TArray<FBodyInstance*> InstanceBodies;

	/** Payload used by asynchronous destruction of physics state (see OnAsyncDestroyPhysicsState). */
	TArray<FBodyInstance*> AsyncDestroyPhysicsStatePayload;

	friend class FInstancedStaticMeshComponentHelper;
};

UCLASS()
class UFastGeoInstancedStaticMeshComponentEditorProxy : public UFastGeoStaticMeshComponentEditorProxy
{
	GENERATED_BODY()

#if WITH_EDITOR
	typedef FFastGeoInstancedStaticMeshComponent ComponentType;
#endif
};

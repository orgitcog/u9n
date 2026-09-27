// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FastGeoComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ComponentInterfaces.h"
#include "Engine/EngineTypes.h"
#include "PrimitiveSceneInfoData.h"
#include "PrimitiveSceneProxy.h"
#include "RenderCommandFence.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsBodyInstanceOwnerInterface.h"
#include "Chaos/ChaosUserEntity.h"
#include "PSOPrecacheFwd.h"
#include "Templates/DontCopy.h"
#include "FastGeoPrimitiveComponent.generated.h"

class FPrimitiveSceneProxy;
class UPrimitiveComponent;
class URuntimeVirtualTexture;
class FFastGeoPrimitiveComponent;
struct FPrimitiveSceneDesc;
struct FPrimitiveSceneProxyDesc;
struct FPSOPrecacheParams;
enum class EPSOPrecachePriority : uint8;

enum class ESceneProxyCreationError
{
	None,
	WaitingPSOs,
	InvalidMesh
};

class FFastGeoPhysicsBodyInstanceOwner : public FChaosUserDefinedEntity, public IPhysicsBodyInstanceOwner
{
public:
	FFastGeoPhysicsBodyInstanceOwner();
	virtual ~FFastGeoPhysicsBodyInstanceOwner() = default;

	/** Returns the IPhysicsBodyInstanceOwner based on the provided FChaosUserDefinedEntity */
	static IPhysicsBodyInstanceOwner* GetPhysicsBodyInstanceOwner(FChaosUserDefinedEntity* InUserDefinedEntity);

	//~ Begin FChaosUserDefinedEntity interface
	TWeakObjectPtr<UObject> GetOwnerObject() override;
	//~ End FChaosUserDefinedEntity interface

	//~ Begin IPhysicsBodyInstanceOwner interface
	virtual bool IsStaticPhysics() const override;
	virtual UObject* GetSourceObject() const override;
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const override;
	virtual UPhysicalMaterial* GetPhysicalMaterial() const override;
	virtual void GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& OutPhysMaterials, TArray<FPhysicalMaterialMaskParams>* OutPhysMaterialMasks) const override;
	//~ End IPhysicsBodyInstanceOwner interface

private:
	void Uninitialize();
	void Initialize(FFastGeoPrimitiveComponent* InOwner);

	FFastGeoPrimitiveComponent* OwnerComponent;
	TWeakObjectPtr<UFastGeoContainer> OwnerContainer;
	static const FName NAME_FastGeoPhysicsBodyInstanceOwner;

	friend class FFastGeoPrimitiveComponent;
	friend class FFastGeoInstancedStaticMeshComponent;
};

class FASTGEOSTREAMING_API FFastGeoPrimitiveComponent : public FFastGeoComponent
{
public:
	typedef FFastGeoComponent Super;
	typedef UFastGeoPrimitiveComponentEditorProxy EditorProxyType;

	/** Static type identifier for this element class */
	static const FFastGeoElementType Type;

	FFastGeoPrimitiveComponent(int32 InComponentIndex = INDEX_NONE, FFastGeoElementType InType = Type);
	virtual ~FFastGeoPrimitiveComponent() = default;

	TArray<URuntimeVirtualTexture*> const& GetRuntimeVirtualTextures() const { return RuntimeVirtualTextures; }
	bool IsFirstPersonRelevant() const;

	FPrimitiveComponentId GetPrimitiveSceneId() const { return PrimitiveSceneData.PrimitiveSceneId; }
	virtual UObject const* AdditionalStatObject() const { return nullptr; }
		
	FSceneInterface* GetScene() const;
	FPrimitiveSceneProxy* GetSceneProxy() const;
	const FTransform& GetTransform() const;
	const FBoxSphereBounds& GetBounds() const;
	FMatrix GetRenderMatrix() const;
	float GetLastRenderTimeOnScreen() const;

	// Used by FStaticMeshComponentHelper/FInstancedStaticMeshComponentHelper
	const FTransform& GetComponentTransform() const { return GetTransform(); }

	void CreateRenderState(FRegisterComponentContext* Context);

	struct FFastGeoDestroyRenderStateContext
	{
		FFastGeoDestroyRenderStateContext(FSceneInterface* InScene);
		~FFastGeoDestroyRenderStateContext();
		bool HasPendingWork() const;

		static void DestroyProxy(FFastGeoDestroyRenderStateContext* InContext, FPrimitiveSceneProxy* InPrimitiveSceneProxy);

	private:
		FSceneInterface* Scene;
		TArray<FPrimitiveSceneProxy*> PrimitiveSceneProxies;
	};

	virtual void DestroyRenderState(FFastGeoDestroyRenderStateContext* Context);
	bool IsRenderStateCreated() const;
	bool IsRenderStateDelayed() const;
	bool IsRenderStateDirty() const;
	bool IsDrawnInGame() const;
	bool ShouldCreateRenderState() const;
	void MarkRenderStateDirty();
	void MarkPrecachePSOsRequired();
	void PrecachePSOs();
	bool CheckPSOPrecachingAndBoostPriority(EPSOPrecachePriority NewPSOPrecachePriority = EPSOPrecachePriority::High);
	bool IsPSOPrecaching() const;
	void SetCollisionEnabled(bool bInCollisionEnabled);
	void UpdateVisibility();
	EComponentMobility::Type GetMobility() const;
	
	//~ Begin FFastGeoComponent interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void InitializeDynamicProperties() override;
	virtual void OnAsyncCreatePhysicsState() override;
	virtual void OnAsyncDestroyPhysicsStateBegin_GameThread() override;
	virtual void OnAsyncDestroyPhysicsStateEnd_GameThread() override;
	virtual void OnAsyncDestroyPhysicsState() override;
	virtual bool IsCollisionEnabled() const override;
#if WITH_EDITOR
	virtual void InitializeFromComponent(UActorComponent* Component) override;
	virtual UClass* GetEditorProxyClass() const override;
#endif
	//~End FFastGeoComponent interface

	//~ Begin Materials
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const = 0;
	virtual int32 GetNumMaterials() const = 0;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const = 0;
	FPrimitiveMaterialPropertyDescriptor GetUsedMaterialPropertyDesc(ERHIFeatureLevel::Type FeatureLevel) const;
	//~ End Materials

	//~ Begin Navigation
	virtual bool IsNavigationRelevant() const;
	virtual bool ShouldSkipNavigationDirtyAreaOnAddOrRemove() { return false; }
	virtual FBox GetNavigationBounds() const;
	virtual void GetNavigationData(FNavigationRelevantData& OutData) const;
	virtual EHasCustomNavigableGeometry::Type HasCustomNavigableGeometry() const;
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const { return true; }
	//~ End Navigation

	
protected:
	virtual FPrimitiveSceneProxyDesc& GetSceneProxyDesc() = 0;
	virtual const FPrimitiveSceneProxyDesc& GetSceneProxyDesc() const = 0;
#if WITH_EDITOR
	virtual void InitializeSceneProxyDescFromComponent(UActorComponent* Component) = 0;
	virtual void ResetSceneProxyDescUnsupportedProperties();
#endif
	virtual void InitializeSceneProxyDescDynamicProperties();
	virtual void ApplyWorldTransform(const FTransform& InTransform);
	virtual FPrimitiveSceneProxy* CreateSceneProxy(ESceneProxyCreationError* OutError = nullptr) = 0;
	FPrimitiveSceneDesc BuildSceneDesc();

	//~ Begin Physics
	virtual bool IsStaticPhysics() const;
	virtual UObject* GetSourceObject() const;
	virtual UPhysicalMaterial* GetPhysicalMaterial() const { return nullptr; }
	virtual void GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& OutPhysMaterials, TArray<FPhysicalMaterialMaskParams>* OutPhysMaterialMasks) const {}
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const;
	//~ End Navigation

protected:
	// Persistent data
	FTransform LocalTransform;
	FTransform WorldTransform;
	FBoxSphereBounds LocalBounds;
	FBoxSphereBounds WorldBounds;
	uint8 bIsVisible : 1 = true;
	uint8 bStaticWhenNotMoveable : 1 = true;
	uint8 bFillCollisionUnderneathForNavmesh : 1 = false;
	uint8 bRasterizeAsFilledConvexVolume : 1 = false;
	uint8 bCanEverAffectNavigation : 1 = false;
	FCustomPrimitiveData CustomPrimitiveData;
	TEnumAsByte<enum EDetailMode> DetailMode;
	TEnumAsByte<EHasCustomNavigableGeometry::Type> bHasCustomNavigableGeometry;
	FBodyInstance BodyInstance;
	TArray<URuntimeVirtualTexture*> RuntimeVirtualTextures;

	// Runtime Data (transient)
	FFastGeoPhysicsBodyInstanceOwner BodyInstanceOwner;
	FPrimitiveSceneInfoData PrimitiveSceneData{};
	/** Payload used to release BodyInstance resources in asynchronous mode (see OnAsyncDestroyPhysicsState). */
	TOptional<FBodyInstance::FAsyncTermBodyPayload> AsyncTermBodyPayload;

	enum class EProxyCreationState : uint8
	{
		None, // Constructed/Initialized
		Pending, // AddedToWorld & proxy creation is pending
		Creating, // Actively creating the proxy
		Created, // Proxy is now created
		Delayed // Proxy creation delayed (used when PSO precaching is not ready when creating proxy)
	};

	EProxyCreationState ProxyState = EProxyCreationState::None;
	bool bRenderStateDirty = false;

private:
	bool ShouldRenderProxyFallbackToDefaultMaterial() const;

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
public:
	void OnPrecacheFinished(int32 JobSetThatJustCompleted);

protected:
	void RequestRecreateRenderStateWhenPSOPrecacheFinished(const FGraphEventArray& PSOPrecacheCompileEvents);
	void SetupPrecachePSOParams(FPSOPrecacheParams& Params);
	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams) {}

	// Cached array of material PSO requests which can be used to boost the priority
	TArray<FMaterialPSOPrecacheRequestID> MaterialPSOPrecacheRequestIDs;
	// Atomic int used to track the last PSO precache events
	std::atomic<int> LatestPSOPrecacheJobSetCompleted = 0;
	int32 LatestPSOPrecacheJobSet = 0;
	// Helper flag to check if PSOs have been precached already
	std::atomic<bool> bPSOPrecacheCalled = false;
	std::atomic<bool> bPSOPrecacheRequired = false;
	// PSOs requested priority
	std::atomic<EPSOPrecachePriority> PSOPrecacheRequestPriority = EPSOPrecachePriority::Medium;
	static_assert((int)EPSOPrecachePriority::Highest < 1 << 2);
#endif

	TDontCopy<FRWLock> Lock;

	friend class FFastGeoComponentCluster;
	friend class FPrimitiveComponentHelper;
	friend class FFastGeoPhysicsBodyInstanceOwner;
	friend class UFastGeoContainer;
	friend class UFastGeoPrimitiveComponentEditorProxy;
};

// Dummy type to use as base class in non editor builds
template <typename TInterface>
class IFastGeoDummyInterface {};

// Implement TInterface, only in editor.
// Would have been easier with a #ifdef in the class declaration, but UHT doesn't allow it.
template <typename TInterface>
class IFastGeoEditorInterface : public std::conditional<WITH_EDITOR, TInterface, IFastGeoDummyInterface<TInterface>>::type {};

UCLASS()
class UFastGeoPrimitiveComponentEditorProxy : public UFastGeoComponentEditorProxy, public IFastGeoEditorInterface<IPrimitiveComponent>
{
	GENERATED_BODY()

#if WITH_EDITOR
	typedef FFastGeoPrimitiveComponent ComponentType;

public:
	virtual void NotifyRenderStateChanged();
	virtual IPrimitiveComponent* GetPrimitiveComponentInterface();

protected:
	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

	//~ Begin IPrimitiveComponent interface
	virtual bool IsRenderStateCreated() const override;
	virtual bool IsRenderStateDirty() const override;
	virtual bool ShouldCreateRenderState() const override;
	virtual bool IsRegistered() const override;
	virtual bool IsUnreachable() const override;
	virtual UWorld* GetWorld() const override;
	virtual FSceneInterface* GetScene() const override;
	virtual FPrimitiveSceneProxy* GetSceneProxy() const override;
	virtual void GetStreamableRenderAssetInfo(TArray<FStreamingRenderAssetPrimitiveInfo>& ) const override {} 
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void MarkRenderStateDirty() override;
	virtual void DestroyRenderState() override;
	virtual void CreateRenderState(FRegisterComponentContext* Context) override;
	virtual FString GetName() const override;
	virtual FString GetFullName() const override;
	virtual FTransform GetTransform() const override;
	virtual FBoxSphereBounds GetBounds() const override;
	virtual float GetLastRenderTimeOnScreen() const override;
	virtual void GetPrimitiveStats(FPrimitiveStats& PrimitiveStats) const override;
	virtual UObject* GetUObject() override;
	virtual const UObject* GetUObject() const override;
	virtual void PrecachePSOs() override;
	virtual UObject* GetOwner() const override;
	virtual FString GetOwnerName() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual HHitProxy* CreateMeshHitProxy(int32 SectionIndex, int32 MaterialIndex) override;
	virtual HHitProxy* CreatePrimitiveHitProxies(TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
	//~ End IPrimitiveComponent interface
#endif
};

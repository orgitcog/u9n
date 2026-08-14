// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/NaniteInterface.h"
#include "Animation/AnimBank.h"
#include "InstanceDataSceneProxy.h"
#include "InstanceData/InstanceDataManager.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "InstancedSkinnedMeshComponent.generated.h"

class FPrimitiveSceneProxy;
class USkinnedMeshComponent;
class UMaterialInterface;
class UTexture;
class UAnimBank;
struct FAnimBankRecordHandle;
struct FPrimitiveSceneDesc;

USTRUCT()
struct FSkinnedMeshInstanceData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Instances)
	FTransform3f Transform;

	UPROPERTY(EditAnywhere, Category = Animation)
	uint32 BankIndex;

	FSkinnedMeshInstanceData()
	: Transform(FTransform3f::Identity)
	, BankIndex(0)
	{
	}

	FSkinnedMeshInstanceData(const FTransform3f& InTransform, uint32 InBankIndex)
	: Transform(InTransform)
	, BankIndex(InBankIndex)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FSkinnedMeshInstanceData& InstanceData)
	{
		Ar << InstanceData.Transform;
		Ar << InstanceData.BankIndex;
		return Ar;
	}
};

UCLASS(ClassGroup=Rendering, hidecategories=(Object,Activation,Collision,"Components|Activation",Physics), editinlinenew, meta=(BlueprintSpawnableComponent), Blueprintable, MinimalAPI)
class UInstancedSkinnedMeshComponent : public USkinnedMeshComponent
{
	GENERATED_UCLASS_BODY()
	
protected:
	/** Array of instances, bulk serialized. */
	UPROPERTY(EditAnywhere, SkipSerialization, DisplayName="Instances", Category=Instances, meta=(MakeEditWidget=true, EditFixedOrder))
	TArray<FSkinnedMeshInstanceData> InstanceData;

	/** Defines the number of floats that will be available per instance for custom data */
	UPROPERTY(EditAnywhere, Category=Instances, AdvancedDisplay)
	int32 NumCustomDataFloats = 0;

	/** Array of custom data for instances. This will contains NumCustomDataFloats*InstanceCount entries. The entries are represented sequantially, in instance order. Can be read in a material and manipulated through Blueprints.
	*	Example: If NumCustomDataFloats is 1, then each entry will belong to an instance. Custom data 0 will belong to Instance 0. Custom data 1 will belong to Instance 1 etc.
	*	Example: If NumCustomDataFloats is 2, then each pair of sequential entries belong to an instance. Custom data 0 and 1 will belong to Instance 0. Custom data 2 and 3 will belong to Instance 2 etc.
	*/
	UPROPERTY(EditAnywhere, EditFixedSize, SkipSerialization, DisplayName="Custom Data", Category=Instances, AdvancedDisplay, meta=(EditFixedOrder))
	TArray<float> InstanceCustomData;

	/** Screen space footprint (in primary view) cutoff which dictates the far distance the instance will play back animation. 
	 *  Using 0.0 (the default) falls back to a global threshold, a negative value disables the cutoff.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Animation)
	float AnimationMinScreenSize = 0.0f;

	/** Distance from camera at which each instance begins to draw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Culling)
	int32 InstanceMinDrawDistance;

	/** Distance from camera at which each instance begins to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Culling)
	int32 InstanceStartCullDistance;

	/** Distance from camera at which each instance completely fades out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Culling)
	int32 InstanceEndCullDistance;

	/** If true, this component will avoid serializing its per instance data / those properties will also not be editable */
	UPROPERTY()
	uint8 bInheritPerInstanceData : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Animation, meta = (ShowOnlyInnerProperties))
	TArray<struct FAnimBankItem> AnimBankItems;

	UPROPERTY()
	FBox PrimitiveBoundsOverride;

	UPROPERTY()
	bool bIsInstanceDataGPUOnly = false;

	UPROPERTY()
	int32 NumInstancesGPUOnly = 0;

public:

	ENGINE_API UInstancedSkinnedMeshComponent(FVTableHelper& Helper);
	virtual ~UInstancedSkinnedMeshComponent();

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) override;
#endif
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void SendRenderInstanceData_Concurrent() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool IsHLODRelevant() const override;
#if WITH_EDITOR
	virtual void ComputeHLODHash(class FHLODHashBuilder& HashBuilder) const override;
#endif
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState_Concurrent() override;

	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	//~ End USceneComponent Interface

	//~ Begin USkinnedMeshComponent Interface
	virtual const Nanite::FResources* GetNaniteResources() const;
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = nullptr) override;
#if WITH_EDITOR
	/**
	 * Called by the asset compilers (FAnimBankCompilingManager / FSkinnedAssetCompilingManager) when done to allow render state to be created.
	 */
	virtual void PostAssetCompilation();

#endif
	//~ End USkinnedMeshComponent Interface

	ENGINE_API void BuildSceneDesc(FPrimitiveSceneProxyDesc* InSceneProxyDesc, FPrimitiveSceneDesc& OutPrimitiveSceneDesc);

public:
	void SetNumGPUInstances(int32 InCount);

	bool UsesGPUOnlyInstances() const { return bIsInstanceDataGPUOnly; }

	FBox GetPrimitiveBoundsOverride() const { return PrimitiveBoundsOverride; }
	void SetPrimitiveBoundsOverride(const FBox& InBounds) { PrimitiveBoundsOverride = InBounds; }

	/** Add an instance to this component. Transform is given in local space of this component unless bWorldSpace is set. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API FPrimitiveInstanceId AddInstance(const FTransform& InstanceTransform, int32 BankIndex, bool bWorldSpace = false);

	/** Add multiple instances to this component. Transform is given in local space of this component unless bWorldSpace is set. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API TArray<FPrimitiveInstanceId> AddInstances(const TArray<FTransform>& Transforms, const TArray<int32>& BankIndices, bool bShouldReturnIds, bool bWorldSpace = false);

	/** Update custom data for specific instance */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API bool SetCustomDataValue(FPrimitiveInstanceId InstanceId, int32 CustomDataIndex, float CustomDataValue);

	/** Update all custom data values for specific instance, the size of the array view must match the NumCustomDataFloats. Returns True on success. */
	ENGINE_API bool SetCustomData(FPrimitiveInstanceId InstanceId, TArrayView<const float> CustomDataFloats);

	/** Get all custom data values for specific instance, the size of the array view must match the NumCustomDataFloats. Returns True on success. */
	ENGINE_API bool GetCustomData(FPrimitiveInstanceId InstanceId, TArrayView<float> CustomDataFloats) const;

	/** Update number of custom data entries per instance. This applies to all instances and will reallocate the full custom data buffer and reset all values to 0 */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API void SetNumCustomDataFloats(int32 InNumCustomDataFloats);

	/** Get the transform for the instance specified. Instance is returned in local space of this component unless bWorldSpace is set. Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API bool GetInstanceTransform(FPrimitiveInstanceId InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace = false) const;

	/** Get the bank index for the instance specified. Returns True on success. */
	ENGINE_API bool GetInstanceBankIndex(FPrimitiveInstanceId InstanceId, int32& OutBankIndex) const;

	/** Remove the instance specified. Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API bool RemoveInstance(FPrimitiveInstanceId InstanceId);

	/** Remove the instances specified. Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API void RemoveInstances(const TArray<FPrimitiveInstanceId>& InstancesToRemove);

	/** Clear all instances being rendered by this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API void ClearInstances();

	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API int32 AddAnimBankItem(const FAnimBankItem& BankItem);

	/**
	 * Optimize the instance data by performing sorting according to spatial hash on the _source_ data.
	 * Note that this reorders the instances and thus any indexing will change. By default resets the ID mapping to identity.
	 *  @param bShouldRetainIdMap	If true, the id mapping is updated instead of reset to identity, this retains the validity of the IDs but adds some memory and storage cost (for the ID mapping).
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedSkinnedMesh")
	ENGINE_API void OptimizeInstanceData(bool bShouldRetainIdMap = false);

	ENGINE_API bool IsEnabled() const;
	ENGINE_API int32 GetInstanceCount() const;

	ENGINE_API TArrayView<const struct FAnimBankItem> GetAnimBankItems() const;

	ENGINE_API void SetAnimBankItems(TArrayView<const struct FAnimBankItem> InAnimBankItems);

	int32 GetNumCustomDataFloats() const { return NumCustomDataFloats; }
	const TArray<float>& GetInstanceCustomData() const { return InstanceCustomData; }
	const TArray<FSkinnedMeshInstanceData>& GetInstanceData() const { return InstanceData; }

	int32 GetInstanceCountGPUOnly() const { return NumInstancesGPUOnly; }

	int32 GetMinDrawDistance() const { return InstanceMinDrawDistance; }
	void GetCullDistances(int32& OutStartCullDistance, int32& OutEndCullDistance) const { OutStartCullDistance = InstanceStartCullDistance; OutEndCullDistance = InstanceEndCullDistance; }
	ENGINE_API void SetCullDistances(int32 StartCullDistance, int32 EndCullDistance);
protected:

	/** Handle changes that must happen before the proxy is recreated. */
	void PreApplyComponentInstanceData(struct FInstancedSkinnedMeshComponentInstanceData* ComponentInstanceData);

	/** Applies the cached component instance data to a newly blueprint constructed component. */
	void ApplyComponentInstanceData(struct FInstancedSkinnedMeshComponentInstanceData* ComponentInstanceData);

	FInstanceDataManagerSourceDataDesc GetComponentDesc(ERHIFeatureLevel::Type FeatureLevel);

#if WITH_EDITOR
	/** One bit per instance if the instance is selected. */
	TBitArray<> SelectedInstances;
#endif

	bool bIsInstanceDataApplyCompleted = true;

	void CreateHitProxyData(TArray<TRefCountPtr<HHitProxy>>& HitProxies);

	/** 
	 */
	FPrimitiveInstanceId AddInstanceInternal(int32 InstanceIndex, const FTransform& InstanceTransform, int32 BankIndex, bool bWorldSpace);

	/** 
	 */
	bool RemoveInstanceInternal(int32 InstanceIndex, bool InstanceAlreadyRemoved);

private:
	void ApplyInheritedPerInstanceData(const UInstancedSkinnedMeshComponent* InArchetype);
	bool ShouldInheritPerInstanceData(const UInstancedSkinnedMeshComponent* InArchetype) const;
	bool ShouldInheritPerInstanceData() const;

	void SetInstanceDataGPUOnly(bool bInInstancesGPUOnly);

	/** Sets up new instance data to sensible defaults, creates physics counterparts if possible. */
	void SetupNewInstanceData(FSkinnedMeshInstanceData& InOutNewInstanceData, int32 InInstanceIndex, const FTransform3f& InInstanceTransform, int32 BankIndex);

	static ENGINE_API bool ShouldForceRefPose();
	static ENGINE_API bool ShouldUseSampledBounds();

	static ENGINE_API FSkeletalMeshObject* CreateMeshObject(const FInstancedSkinnedMeshSceneProxyDesc& InMeshDesc, TConstArrayView<FAnimBankItem> InAnimBankItems, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel);
	static ENGINE_API FPrimitiveSceneProxy* CreateSceneProxy(const FInstancedSkinnedMeshSceneProxyDesc& Desc, bool bHideSkin, bool bShouldNaniteSkin, bool bIsEnabled, int32 MinLODIndex);

	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> GetOrCreateInstanceDataSceneProxy();

	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> GetInstanceDataSceneProxy() const;

	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> CreateInstanceDataProxyGPUOnly() const;

	FInstanceDataManager InstanceDataManager;

	friend class FInstancedSkinnedMeshSceneProxy;
	friend class FInstancedSkinnedMeshObject;
	friend class FInstancedSkinnedMeshComponentHelper;
	friend struct FInstancedSkinnedMeshComponentInstanceData;
	friend struct FInstancedSkinnedMeshSceneProxyDesc;
	friend struct FSkinnedMeshComponentDescriptorBase;

private:
	void SetSkinnedAssetCallback();
};

/** Helper class used to preserve state across blueprint re-instancing */
USTRUCT()
struct FInstancedSkinnedMeshComponentInstanceData : public FSceneComponentInstanceData
{
	GENERATED_BODY()

public:
	FInstancedSkinnedMeshComponentInstanceData() = default;
	
	FInstancedSkinnedMeshComponentInstanceData(const UInstancedSkinnedMeshComponent* InComponent)
		: FSceneComponentInstanceData(InComponent)
		, SkinnedAsset(InComponent->GetSkinnedAsset())
		, PrimitiveBoundsOverride(InComponent->GetPrimitiveBoundsOverride())
		, bIsInstanceDataGPUOnly(InComponent->UsesGPUOnlyInstances())
		, NumInstancesGPUOnly(InComponent->GetInstanceCountGPUOnly())
	{
	}

	virtual ~FInstancedSkinnedMeshComponentInstanceData() = default;

	virtual bool ContainsData() const override
	{
		return true;
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		// The Super::ApplyToComponent will cause the scene proxy to be recreated, so we must do what we can to make sure the state is ok before that.
		CastChecked<UInstancedSkinnedMeshComponent>(Component)->PreApplyComponentInstanceData(this);
		Super::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UInstancedSkinnedMeshComponent>(Component)->ApplyComponentInstanceData(this);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Super::AddReferencedObjects(Collector);
		Collector.AddReferencedObject(SkinnedAsset);
	}

public:
	UPROPERTY()
	TObjectPtr<USkinnedAsset> SkinnedAsset = nullptr;

	UPROPERTY()
	TArray<FSkinnedMeshInstanceData> InstanceData;

	TBitArray<> SelectedInstances;

	UPROPERTY()
	bool bHasPerInstanceHitProxies = false;

	UPROPERTY()
	FBox PrimitiveBoundsOverride;

	UPROPERTY()
	bool bIsInstanceDataGPUOnly = false;

	UPROPERTY()
	int32 NumInstancesGPUOnly = 0;
};

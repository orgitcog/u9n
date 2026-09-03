// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "SmartObjectTypes.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectDefinitionReference.h"
#include "SmartObjectComponent.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

namespace EEndPlayReason { enum Type : int; }

class UAbilitySystemComponent;
struct FSmartObjectRuntime;
struct FSmartObjectComponentInstanceData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSmartObjectComponentEventSignature, const FSmartObjectEventData&, EventData, const AActor*, Interactor);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSmartObjectComponentEventNativeSignature, const FSmartObjectEventData& EventData, const AActor* Interactor);

enum class ESmartObjectRegistrationType : uint8
{
	/** Not registered yet */
	NotRegistered,

	/**
	 * Registered and bound to a SmartObject already created from a persistent collection entry or from method CreateSmartObject.
	 * Lifetime of the SmartObject is not bound to the component unregistration but by method UnregisterCollection in the case of 
	 * a collection entry or by method DestroySmartObject when CreateSmartObject was used.
	 */
	BindToExistingInstance,

	/**
	 * Component is registered and bound to a newly created SmartObject.
	 * The lifetime of the SmartObject is bound to the component unregistration will be unbound/destroyed by UnregisterSmartObject/RemoveSmartObject.
	 */
	Dynamic,
	
	None UE_DEPRECATED(5.4, "Use NotRegistered enumeration value instead.") = NotRegistered,
	WithCollection UE_DEPRECATED(5.4, "Use NotRegistered enumeration value instead.") = BindToExistingInstance,
};

enum class ESmartObjectUnregistrationType : uint8
{
	/**
	 * Component registered by a collection (WithCollection) will be unbound from the simulation but its associated runtime data will persist.
	 * Otherwise (Dynamic), runtime data will also be destroyed.
	 */
	RegularProcess,
	/** Component will be unbound from the simulation and its runtime data will be destroyed regardless of the registration type */
	ForceRemove
};

UCLASS(MinimalAPI, Blueprintable, ClassGroup = Gameplay, meta = (BlueprintSpawnableComponent), config = Game, HideCategories = (Activation, AssetUserData, Collision, Cooking, HLOD, Lighting, LOD, Mobile, Mobility, Navigation, Physics, RayTracing, Rendering, Tags, TextureStreaming))
class USmartObjectComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UE_DEPRECATED(5.6, "Use the delegate taking a pointer to the component instead.")
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSmartObjectChanged, const USmartObjectComponent& /*Instance*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSmartObjectComponentChanged, TNotNull<const USmartObjectComponent*> /*Instance*/);

	UE_API explicit USmartObjectComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UE_API FBox GetSmartObjectBounds() const;

	/** @return Smart Object Definition with parameters applied. */
	UFUNCTION(BlueprintGetter)
	UE_API const USmartObjectDefinition* GetDefinition() const;

	/** @return Smart Object Definition without applied parameters. */
	UE_API const USmartObjectDefinition* GetBaseDefinition() const;

	/** Sets the Smart Object Definition. */
	UFUNCTION(BlueprintSetter)
	UE_API void SetDefinition(USmartObjectDefinition* DefinitionAsset);

	bool GetCanBePartOfCollection() const
	{
		return bCanBePartOfCollection;
	}

	ESmartObjectRegistrationType GetRegistrationType() const
	{
		return RegistrationType;
	}

	FSmartObjectHandle GetRegisteredHandle() const
	{
		return RegisteredHandle;
	}

	UE_API void SetRegisteredHandle(const FSmartObjectHandle Value, const ESmartObjectRegistrationType InRegistrationType);
	UE_API void InvalidateRegisteredHandle();

	UE_API void OnRuntimeInstanceBound(FSmartObjectRuntime& RuntimeInstance);
	UE_API void OnRuntimeInstanceUnbound(FSmartObjectRuntime& RuntimeInstance);

	/**
	 * Enables or disables the smart object using the default reason (i.e. Gameplay).
	 * @return false if it was not possible to change the enabled state (ie. if it's not registered or there is no smart object subsystem).
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Set SmartObject Enabled (default reason: Gameplay)", ReturnDisplayName="Status changed"))
	UE_API bool SetSmartObjectEnabled(const bool bEnable) const;

	/**
	 * Enables or disables the smart object for the specified reason.
	 * @param ReasonTag Valid Tag to specify the reason for changing the enabled state of the object. Method will ensure if not valid (i.e. None).
	 * @param bEnabled If true enables the smart object, disables otherwise.
	 * @return false if it was not possible to change the enabled state (ie. if it's not registered or there is no smart object subsystem).
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Set SmartObject Enabled (specific reason)", ReturnDisplayName="Status changed"))
	UE_API bool SetSmartObjectEnabledForReason(FGameplayTag ReasonTag, const bool bEnabled) const;

	/**
	 * Returns the enabled state of the smart object regardless of the disabled reason.
	 * @return True when associated smart object is set to be enabled. False otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Is SmartObject Enabled (for any reason)", ReturnDisplayName="Enabled"))
	UE_API bool IsSmartObjectEnabled() const;

	/**
	 * Returns the enabled state of the smart object based on a specific reason.
	 * @param ReasonTag Valid Tag to test if enabled for a specific reason. Method will ensure if not valid (i.e. None).
	 * @return True when associated smart object is set to be enabled. False otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Is SmartObject Enabled (for specific reason)", ReturnDisplayName="Enabled"))
	UE_API bool IsSmartObjectEnabledForReason(FGameplayTag ReasonTag) const;

	FSmartObjectComponentEventNativeSignature& GetOnSmartObjectEventNative()
	{
		return OnSmartObjectEventNative;
	}

	/** Returns true if the Smart Object component is registered to the Smart Object subsystem. Depending on the update order, sometimes it is possible that the subsystem gets enabled after the component. */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool IsBoundToSimulation() const
	{
		return EventDelegateHandle.IsValid();
	}

#if WITH_EDITORONLY_DATA
	/** Conditionally updates the GUID if it was never set. Used for collection deprecation only. */
	void ValidateGUIDForDeprecation()
	{
		ValidateGUID();
	}

	static FOnSmartObjectComponentChanged& GetOnSmartObjectComponentChanged()
	{
		return OnSmartObjectComponentChanged;
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use OnSmartObjectComponentChanged instead.")
	static FOnSmartObjectChanged& GetOnSmartObjectChanged()
	{
		return OnSmartObjectChanged;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif // WITH_EDITORONLY_DATA

	const FSmartObjectDefinitionReference& GetDefinitionReference() const
	{
		return DefinitionRef;
	}

#if WITH_EDITOR
	FSmartObjectDefinitionReference& GetMutableDefinitionReference()
	{
		return DefinitionRef;
	}
#endif // WITH_EDITOR

	/** Returns this component Guid */
	[[nodiscard]] FGuid GetComponentGuid() const
	{
		return ComponentGuid;
	}

protected:
	friend FSmartObjectComponentInstanceData;
	UE_API virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UE_API void OnRuntimeEventReceived(const FSmartObjectEventData& Event);
	
	UPROPERTY(BlueprintAssignable, Category = SmartObject, meta=(DisplayName = "OnSmartObjectEvent"))
	FSmartObjectComponentEventSignature OnSmartObjectEvent;

	/** Native version of OnSmartObjectEvent. */
	FSmartObjectComponentEventNativeSignature OnSmartObjectEventNative;

	UFUNCTION(BlueprintImplementableEvent, Category = SmartObject, meta=(DisplayName = "OnSmartObjectEventReceived"))
	UE_API void ReceiveOnEvent(const FSmartObjectEventData& EventData, const AActor* Interactor);

	UE_API virtual void PostInitProperties() override;
	UE_API virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	UE_API virtual void Serialize(FArchive& Ar) override;

	UE_API virtual void OnRegister() override;

	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	UE_API virtual void OnUnregister() override;

	UE_API virtual void PostEditImport() override;
	UE_API virtual void PostEditUndo() override;
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	UE_API virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif // WITH_EDITOR

	UE_API void RegisterToSubsystem();
	UE_API void UnregisterFromSubsystem(const ESmartObjectUnregistrationType UnregistrationType);

	/** Unique ID used, along with the owner's ActorGuid to generate a SmartObjectHandle */
	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FGuid ComponentGuid;

	/** Reference to Smart Object Definition Asset with parameters. */
	UPROPERTY(EditAnywhere, Category = SmartObject, Replicated, meta = (DisplayName="Definition"))
	FSmartObjectDefinitionReference DefinitionRef;

	/** RegisteredHandle != FSmartObjectHandle::Invalid when registered into a collection by SmartObjectSubsystem */
	UPROPERTY(Transient, VisibleAnywhere, Category = SmartObject, BlueprintReadOnly, Replicated)
	FSmartObjectHandle RegisteredHandle;

	FDelegateHandle EventDelegateHandle;

	ESmartObjectRegistrationType RegistrationType = ESmartObjectRegistrationType::NotRegistered;

	/** 
	 * Controls whether a given SmartObject can be aggregated in SmartObjectPersistentCollections. SOs in collections
	 * can be queried and reasoned about even while the actual Actor and its components are not streamed in.
	 * By default SmartObjects are not placed in collections and are active only as long as the owner-actor remains
	 * loaded and active (i.e. not streamed out).
	 */
	UPROPERTY(config, EditAnywhere, Category = SmartObject, AdvancedDisplay)
	bool bCanBePartOfCollection = false;

#if WITH_EDITORONLY_DATA
	static UE_API FOnSmartObjectComponentChanged OnSmartObjectComponentChanged;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use OnSmartObjectComponentChanged instead.")
	static UE_API FOnSmartObjectChanged OnSmartObjectChanged;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

private:
	/** Conditionally updates the GUID if it was never set */
	void ValidateGUID();

	/** Assigns a new GUID to the component*/
	UE_API void UpdateGUID();

	//~ Do not use directly from native code, use GetDefinition() / SetDefinition() instead.
	//~ Also Keeping blueprint accessors for convenience and deprecation purposes.
	UPROPERTY(Transient, Category = SmartObject, BlueprintSetter = SetDefinition, BlueprintGetter = GetDefinition, meta = (DisplayName="Definition Asset"))
	mutable TObjectPtr<USmartObjectDefinition> CachedDefinitionAssetVariation = nullptr;

#if WITH_EDITORONLY_DATA
	FDelegateHandle OnSavingDefinitionDelegateHandle;

	/** return true if applied or false if already applied */
	UE_API bool ApplyDeprecation();

	/** return true if applied or  false if already applied */
	UE_API bool ApplyParentDeprecation();

	/** flag to keep track of the deprecation status of the object */
	UPROPERTY()
	bool bDeprecationApplied = false;

	UPROPERTY(Category = SmartObject, BlueprintSetter = SetDefinition, BlueprintGetter = GetDefinition, meta = (DeprecatedProperty, DisplayName="Deprecated Definition Asset", BlueprintPrivate))
	TObjectPtr<USmartObjectDefinition> DefinitionAsset_DEPRECATED;
#endif//WITH_EDITOR
};


/** Used to store SmartObjectComponent data during RerunConstructionScripts */
USTRUCT()
struct FSmartObjectComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()

public:
	FSmartObjectComponentInstanceData() = default;

	UE_DEPRECATED(5.6, "Use the constructor taking only the component pointer instead.")
	explicit FSmartObjectComponentInstanceData(const USmartObjectComponent* SourceComponent, const FSmartObjectDefinitionReference& Ref)
		: FActorComponentInstanceData(SourceComponent)
		, SmartObjectDefinitionRef(Ref)
		, OriginalGuid(SourceComponent->ComponentGuid)
	{
	}

	explicit FSmartObjectComponentInstanceData(const TNotNull<const USmartObjectComponent*> SourceComponent)
		: FActorComponentInstanceData(SourceComponent)
		, SmartObjectDefinitionRef(SourceComponent->DefinitionRef)
		, OriginalGuid(SourceComponent->ComponentGuid)
	{
	}

	const FSmartObjectDefinitionReference& GetSmartObjectDefinitionReference() const
	{
		return SmartObjectDefinitionRef;
	}

protected:
	virtual bool ContainsData() const override;
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;

	UPROPERTY()
	FSmartObjectDefinitionReference SmartObjectDefinitionRef;

	UPROPERTY()
	FGuid OriginalGuid;
};

#undef UE_API

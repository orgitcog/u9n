// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectDefinitionReference.h"
#include "SmartObjectPersistentCollection.h"
#include "SmartObjectRuntime.h"
#include "WorldConditionContext.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "Misc/MTAccessDetector.h"
#include "MassExternalSubsystemTraits.h"

#ifndef WITH_SMARTOBJECT_MT_INSTANCE_LOCK
#define WITH_SMARTOBJECT_MT_INSTANCE_LOCK 0
#endif

#if WITH_SMARTOBJECT_MT_INSTANCE_LOCK
#include "HAL/CriticalSection.h"
#endif

#include "SmartObjectSubsystem.generated.h"

struct FSmartObjectRequest;
struct FSmartObjectRequestResult;
struct FSmartObjectRequestFilter;
class UCanvas;
class USmartObjectBehaviorDefinition;

class USmartObjectComponent;
class UWorldPartitionSmartObjectCollectionBuilder;
struct FMassEntityManager;
class ASmartObjectSubsystemRenderingActor;
class FDebugRenderSceneProxy;
class ADEPRECATED_SmartObjectCollection;
class UNavigationQueryFilter;
class ANavigationData;
struct FSmartObjectValidationContext;
struct FTargetingRequestHandle;

#if WITH_EDITOR
/** Called when an event related to the main collection occured. */
DECLARE_MULTICAST_DELEGATE(FOnMainCollectionEvent);
#endif

/**
 * Defines method for selecting slot entry from multiple candidates.
 */
UENUM()
enum class FSmartObjectSlotEntrySelectionMethod : uint8
{
	/** Return first entry location (in order defined in the slot definition). */
	First,
	
	/** Return nearest entry to specified search location. */
	NearestToSearchLocation,
};

/**
 * Handle describing a specific entrance on a smart object slot.
 */
USTRUCT(BlueprintType)
struct FSmartObjectSlotEntranceHandle
{
	GENERATED_BODY()

	FSmartObjectSlotEntranceHandle() = default;

	FSmartObjectSlotHandle GetSlotHandle() const
	{
		return SlotHandle;
	}
	
	bool IsValid() const
	{
		return Type != EType::Invalid;
	}

	bool operator==(const FSmartObjectSlotEntranceHandle& Other) const
	{
		return SlotHandle == Other.SlotHandle && Type == Other.Type && Index == Other.Index;
	}

	bool operator!=(const FSmartObjectSlotEntranceHandle& Other) const
	{
		return !(*this == Other);
	}

private:

	enum class EType : uint8
	{
		Invalid,	// Handle is invalid
		Entrance,	// The handle points to a specific entrance, index is slot data index.
		Slot,		// The handle points to the slot itself.
	};
	
	explicit FSmartObjectSlotEntranceHandle(const FSmartObjectSlotHandle InSlotHandle, const EType InType, const int32 InIndex = 0)
		: SlotHandle(InSlotHandle)
		, Type(InType)
	{
		using IndexType = decltype(Index);
		check(InIndex >= std::numeric_limits<IndexType>::min() && InIndex <= std::numeric_limits<IndexType>::max());
		Index = static_cast<IndexType>(InIndex);
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject", meta = (AllowPrivateAccess = "true"))
	FSmartObjectSlotHandle SlotHandle;
	
	EType Type = EType::Invalid;
	uint8 Index = 0;
	
	friend class USmartObjectSubsystem;
};

/**
 * Struct used to request slot entry or exit location.
 *
 * When used with actor, it is generally enough to set the UserActor. In that case NavigationData, ValidationFilter,
 * and UserCapsule are queried via the INavAgentInterface and USmartObjectUserComponent on the actor if they are _not_ set.
 * 
 * If the user actor is not available (e.g. with Mass), then ValidationFilter and UserCapsule must be defined, and if bProjectNavigationLocation is set NavigationData must be valid. 
 * 
 * The location validation is done in following order:
 *  - navigation projection
 *  - trace ground location (uses altered location from navigation test if applicable)
 *  - check transition trajectory (test between unmodified navigation location and slow location)
 */
USTRUCT(BlueprintType)
struct FSmartObjectSlotEntranceLocationRequest
{
	GENERATED_BODY()

	// Macro needed to avoid deprecation errors with "UserCapsule" being copied or created in the default methods
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSmartObjectSlotEntranceLocationRequest() = default;
	FSmartObjectSlotEntranceLocationRequest(const FSmartObjectSlotEntranceLocationRequest&) = default;
	FSmartObjectSlotEntranceLocationRequest(FSmartObjectSlotEntranceLocationRequest&&) = default;
	FSmartObjectSlotEntranceLocationRequest& operator=(const FSmartObjectSlotEntranceLocationRequest&) = default;
	FSmartObjectSlotEntranceLocationRequest& operator=(FSmartObjectSlotEntranceLocationRequest&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Actor that is using the smart object slot. (Optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<const AActor> UserActor = nullptr;

	/** Filter to use for the validation. If not set and UserActor is valid, the filter is queried via USmartObjectUserComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	TSubclassOf<USmartObjectSlotValidationFilter> ValidationFilter = nullptr;

	/** Navigation data to use for the navigation queries. If not set and UserActor is valid, the navigation data is queried via INavAgentInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<const ANavigationData> NavigationData = nullptr;
	
	/** Size of the user of the smart object. If not set and UserActor is valid, the dimensions are queried via INavAgentInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectUserCapsuleParams UserCapsuleParams = FSmartObjectUserCapsuleParams::Invalid;

	/** Search location that may be used to select an entry from multiple candidates. (e.g. user actor location). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	FVector SearchLocation = FVector::ZeroVector;

	/** How to select an entry when a slot has multiple entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectSlotEntrySelectionMethod SelectMethod = FSmartObjectSlotEntrySelectionMethod::First;

	/** Enum indicating if we're looking for a location to enter or exit the smart object slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	ESmartObjectSlotNavigationLocationType LocationType = ESmartObjectSlotNavigationLocationType::Entry;

	/** If true, try to project the location on navigable area. If projection fails, an entry is discarded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool bProjectNavigationLocation = true;

	/** If true, try to trace the location on ground. If trace fails, an entry is discarded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool bTraceGroundLocation = true;

	/** If true, check collisions between navigation location and slot location. If collisions are found, an entry is discarded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool bCheckTransitionTrajectory = true;

	/** If true, check user capsule collisions at the entrance location. Uses capsule dimensions set in the validation filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool bCheckEntranceLocationOverlap = true;

	/** If true, check user capsule collisions at the slot location. Uses capsule dimensions set in an annotation on the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool bCheckSlotLocationOverlap = true;

	/** If true, include slot location as a candidate if no navigation annotation is present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool bUseSlotLocationAsFallback = false;

	/** If true, the result rotation will only contain rotation around the UP axis (i.e., Yaw only; Pitch and Roll set to 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool bUseUpAxisLockedRotation = false;

	UE_DEPRECATED(5.4, "Use UserCapsuleParams instead.")
	TOptional<FSmartObjectUserCapsuleParams> UserCapsule;
};

/**
 * Validated result from FindEntranceLocationForSlot().
 */
USTRUCT(BlueprintType)
struct FSmartObjectSlotEntranceLocationResult
{
	GENERATED_BODY()

	// Macro needed to avoid deprecation errors with "Tag" being copied or created in the default methods
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSmartObjectSlotEntranceLocationResult() = default;
	FSmartObjectSlotEntranceLocationResult(const FSmartObjectSlotEntranceLocationResult&) = default;
	FSmartObjectSlotEntranceLocationResult(FSmartObjectSlotEntranceLocationResult&&) = default;
	FSmartObjectSlotEntranceLocationResult& operator=(const FSmartObjectSlotEntranceLocationResult&) = default;
	FSmartObjectSlotEntranceLocationResult& operator=(FSmartObjectSlotEntranceLocationResult&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** @returns entry as nav location. */
	FNavLocation GetNavLocation() const
	{
		return FNavLocation(Location, NodeRef);
	}

	/** @returns true if the result contains valid navigation node reference. */
	bool HasNodeRef() const
	{
		return NodeRef != INVALID_NAVNODEREF;
	}

	/** The location to enter the slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FVector Location = FVector::ZeroVector;

	/** The expected direction to enter the slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FRotator Rotation = FRotator::ZeroRotator;
	
	/** Node reference in navigation data (if requested with bMustBeNavigable). */
	NavNodeRef NodeRef = INVALID_NAVNODEREF;

	/** Gameplay tag associated with the entrance. */
	UE_DEPRECATED(5.3, "Use Tags instead.")
	UPROPERTY()
	FGameplayTag Tag;

	/** Gameplay tags associated with the entrance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FGameplayTagContainer Tags;

	/** Handle identifying the entrance that was found. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FSmartObjectSlotEntranceHandle EntranceHandle;

	/** True if the result has passed validation tests. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	bool bIsValid = true;
};

using FSmartObjectSlotNavigationLocationResult = FSmartObjectSlotEntranceLocationResult; 

/**
 * Result code indicating if the Collection was successfully registered or why it was not.
 */
UENUM()
enum class ESmartObjectCollectionRegistrationResult : uint8
{
	Failed_InvalidCollection,
	Failed_AlreadyRegistered,
	Failed_NotFromPersistentLevel,
	Succeeded
};

/**
 * Subsystem that holds all registered smart object instances and offers the API for spatial queries and reservations.
 * 
 * [Notes regarding thread safety]
 * The subsystem is not thread-safe, but a first pass has been made to make it possible to perform a set
 * of operations from multiple threads.
 * To use this mode the following compiler switch is required: #define WITH_SMARTOBJECT_MT_INSTANCE_LOCK 1
 *
 * Not safe:
 *	- runtime instance lifetime controlled from Registration/Unregistration
 *		(i.e., CreateSmartObject, RegisterCollection, UnregisterCollection, DestroySmartObject, etc.)
 *	- queries: to prevent locking for a long time it is still required to send queries from a single thread
 *		(e.g., async request pattern like MassSmartObject)
 *
 * Safe operation on a smart object instance or slot from an object or slot handle:
 *	- query and set Enable state 
 *	- query and set Transform/Location
 *	- query and set Tags 
 *	- update slot state (e.g., MarkSlotAsClaimed, MarkSlotAsReleased, etc.)
 *	- use a slot view using ReadSlotData/MutateSlotData
 */
UCLASS(MinimalAPI, config = SmartObjects, defaultconfig, Transient)
class USmartObjectSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	SMARTOBJECTSMODULE_API USmartObjectSubsystem();

	static SMARTOBJECTSMODULE_API USmartObjectSubsystem* GetCurrent(const UWorld* World);

	SMARTOBJECTSMODULE_API ESmartObjectCollectionRegistrationResult RegisterCollection(ASmartObjectPersistentCollection& InCollection);
	SMARTOBJECTSMODULE_API void UnregisterCollection(ASmartObjectPersistentCollection& InCollection);

	const FSmartObjectContainer& GetSmartObjectContainer() const
	{
		return SmartObjectContainer;
	}

	FSmartObjectContainer& GetMutableSmartObjectContainer()
	{
		return SmartObjectContainer;
	}

	/**
	 * Enables or disables the entire smart object represented by the provided handle using the default reason (i.e. Gameplay)..
	 * Delegate 'OnEvent' is broadcasted with ESmartObjectChangeReason::OnEnabled/ESmartObjectChangeReason::OnDisabled if state changed.
	 * @param Handle Handle to the smart object.
	 * @param bEnabled If true enables the smart object, disables otherwise.
	 * @return True when associated smart object is found and set (or already set) to desired state; false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Set SmartObject Enabled (default reason: Gameplay)", ReturnDisplayName="Status changed"))
	SMARTOBJECTSMODULE_API bool SetEnabled(const FSmartObjectHandle Handle, const bool bEnabled);

	/**
	 * Enables or disables the entire smart object represented by the provided handle using the specified reason.
	 * Delegate 'OnEvent' is broadcasted with ESmartObjectChangeReason::OnEnabled/ESmartObjectChangeReason::OnDisabled if state changed.
	 * @param Handle Handle to the smart object.
	 * @param ReasonTag Valid Tag to specify the reason for changing the enabled state of the object. Method will ensure if not valid (i.e. None).
	 * @param bEnabled If true enables the smart object, disables otherwise.
	 * @return True when associated smart object is found and set (or already set) to desired state; false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Set SmartObject Enabled (specific reason)", ReturnDisplayName="Status changed"))
	SMARTOBJECTSMODULE_API bool SetEnabledForReason(const FSmartObjectHandle Handle, FGameplayTag ReasonTag, const bool bEnabled);

	/**
	 * Returns the enabled state of the smart object represented by the provided handle regardless of the disabled reason.
	 * @param Handle Handle to the smart object.
	 * @return True when associated smart object is found and set to be enabled. False otherwise.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject", meta=(DisplayName="Is SmartObject Enabled (for any reason)", ReturnDisplayName="Enabled"))
	SMARTOBJECTSMODULE_API bool IsEnabled(const FSmartObjectHandle Handle) const;

	/**
	 * Returns the enabled state of the smart object represented by the provided handle based on a specific reason.
	 * @param Handle Handle to the smart object.
	 * @param ReasonTag Valid Tag to test if enabled for a specific reason. Method will ensure if not valid (i.e. None).
	 * @return True when associated smart object is found and set to be enabled. False otherwise.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject", meta=(DisplayName="Is SmartObject Enabled (for specific reason)", ReturnDisplayName="Enabled"))
	SMARTOBJECTSMODULE_API bool IsEnabledForReason(const FSmartObjectHandle Handle, FGameplayTag ReasonTag) const;

	/**
	 * Enables or disables all smart objects associated to the provided actor (multiple components).
	 * @param SmartObjectActor Smart object(s) parent actor.
	 * @param bEnabled If true enables, disables otherwise.
	 * @return True if all (at least one) smart object components are found with their associated
	 * runtime and values set (or already set) to desired state; false otherwise.
	 */
	SMARTOBJECTSMODULE_API bool SetSmartObjectActorEnabled(const AActor& SmartObjectActor, bool bEnabled);

	/**
	 * Registers to the runtime simulation all smart object components for a given actor.
	 * @param SmartObjectActor Actor owning the components to register
	 * @return true when components are found and all successfully registered, false otherwise
	 */
	SMARTOBJECTSMODULE_API bool RegisterSmartObjectActor(const AActor& SmartObjectActor);

	/**
	 * Unregisters from the simulation all smart object components for a given actor.
	 * @param SmartObjectActor Actor owning the components to unregister
	 * @return true when components are found and all successfully unregistered, false otherwise
	 */
	SMARTOBJECTSMODULE_API bool UnregisterSmartObjectActor(const AActor& SmartObjectActor);

	/**
	 * Removes all data associated to smart object components of a given actor from the simulation.
	 * @param SmartObjectActor Actor owning the components to delete
	 * @return whether components are found and all successfully deleted
	 */
	SMARTOBJECTSMODULE_API bool RemoveSmartObjectActor(const AActor& SmartObjectActor);

	/**
	 * Registers a smart object components to the runtime simulation.
	 * @param SmartObjectComponent Smart object component to register
	 * @return true when component is successfully registered, false otherwise
	 */
	SMARTOBJECTSMODULE_API bool RegisterSmartObject(TNotNull<USmartObjectComponent*> SmartObjectComponent);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	bool RegisterSmartObject(USmartObjectComponent& SmartObjectComponent)
	{
		return RegisterSmartObject(&SmartObjectComponent);
	}

	/**
	 * Creates a new smart object runtime instance from an external system.
	 * @param Definition Smart object definition that the entry will use
	 * @param Transform World position where the entry will be initially located
	 * @param OwnerData Payload stored in the created runtime instance to identify the owner of the smart object
	 * @return Handle to the newly created smart object if the operation was successful.
	 */
	SMARTOBJECTSMODULE_API FSmartObjectHandle CreateSmartObject(const USmartObjectDefinition& Definition, const FTransform& Transform, const FConstStructView OwnerData);

	/**
	 * Unregisters the component from the subsystem, unbinds it from the runtime simulation and handles the associated runtime data
	 * according to the component registration type (i.e. runtime data associated to components from persistent collections
	 * will remain in the simulation).
	 * @param SmartObjectComponent Smart object component to unregister
	 * @return true when component is successfully unregistered, false otherwise
	 */
	SMARTOBJECTSMODULE_API bool UnregisterSmartObject(TNotNull<USmartObjectComponent*> SmartObjectComponent);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	bool UnregisterSmartObject(USmartObjectComponent& SmartObjectComponent)
	{
		return UnregisterSmartObject(&SmartObjectComponent);
	}

	/**
	 * Unregisters the component from the subsystem, unbinds it from the runtime simulation and removes its runtime data.
	 * @param SmartObjectComponent Smart object component to remove
	 * @return whether smart object data has been successfully found and removed
	 */
	SMARTOBJECTSMODULE_API bool RemoveSmartObject(TNotNull<USmartObjectComponent*> SmartObjectComponent);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	bool RemoveSmartObject(USmartObjectComponent& SmartObjectComponent)
	{
		return RemoveSmartObject(&SmartObjectComponent);
	}

	/**
	 * Removes the smart object runtime data from the simulation, destroys it and unbinds and unregister associated component if any.
	 * @param Handle Handle to the smart object to destroy
	 * @return True if the smart object data has been successfully found and destroyed, false otherwise.
	 */
	SMARTOBJECTSMODULE_API bool DestroySmartObject(FSmartObjectHandle Handle);

	/**
	 * Binds a smart object component to an existing instance in the simulation. If a given SmartObjectComponent has not 
	 * been registered yet an ensure will trigger.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime instance must exist
	*/
	UE_DEPRECATED(5.6, "Use RegisterSmartObject instead.")
	SMARTOBJECTSMODULE_API void BindComponentToSimulation(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Unbinds a smart object component from an existing instance in the simulation.
	 * @param SmartObjectComponent The component to remove from the simulation
	 */
	UE_DEPRECATED(5.6, "Use UnregisterSmartObject instead.")
	SMARTOBJECTSMODULE_API void UnbindComponentFromSimulation(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Updates the smart object transform.
	 * @param Handle Handle to the smart object to update.
	 * @param NewTransform New transform of the runtime smart object
	 * @return is transform was updated.
	 */
	SMARTOBJECTSMODULE_API bool UpdateSmartObjectTransform(const FSmartObjectHandle Handle, const FTransform& NewTransform);
	
	/**
	 * Returns the component associated to the claim handle if still
	 * accessible. In some scenarios the component may no longer exist
	 * but its smart object data could (e.g. streaming)
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param TrySpawnActorIfDehydrated Indicates if the subsystem should try to spawn the actor/component
	 *        associated to the smart object if it is currently owned by an instanced actor.
	 * @return A pointer to the USmartObjectComponent* associated to the handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API USmartObjectComponent* GetSmartObjectComponent(const FSmartObjectClaimHandle& ClaimHandle,
		ETrySpawnActorIfDehydrated TrySpawnActorIfDehydrated = ETrySpawnActorIfDehydrated::No) const;

	/**
	 * Returns the component associated to the  given request result
	 * In some scenarios the component may no longer exist
	 * but its smart object data could (e.g. streaming)
	 * @param Result A request result returned by any of the Find methods .
	 * @param TrySpawnActorIfDehydrated Indicates if the subsystem should try to spawn the actor/component
	 *        associated to the smart object if it is currently owned by an instanced actor.
	 * @return A pointer to the USmartObjectComponent* associated to the handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API USmartObjectComponent* GetSmartObjectComponentByRequestResult(const FSmartObjectRequestResult& Result,
		ETrySpawnActorIfDehydrated TrySpawnActorIfDehydrated = ETrySpawnActorIfDehydrated::No) const;

	/**
	 * Spatial lookup for first slot in range respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return First valid smart object in range. Not the closest one, just the one
	 *		that happens to be retrieved first from space partition
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API FSmartObjectRequestResult FindSmartObject(const FSmartObjectRequest& Request, const FConstStructView UserData) const;

	/**
	 * Spatial lookup for slot candidates respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return True if at least one candidate was found.
	 */
	SMARTOBJECTSMODULE_API bool FindSmartObjects(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults, const FConstStructView UserData) const;

	/**
	 * Search list of specific actors (often from a physics query) for slot candidates respecting request criteria and selection conditions.
	 * 
	 * @param Filter Parameters defining the search area and criteria
	 * @param ActorList Ordered list of actors to search
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return True if at least one candidate was found.
	 */
	SMARTOBJECTSMODULE_API bool FindSmartObjectsInList(const FSmartObjectRequestFilter& Filter, const TConstArrayView<AActor*> ActorList, TArray<FSmartObjectRequestResult>& OutResults, const FConstStructView UserData) const;

	/**
	 * Search the results of the given targeting request handle for smart objects that match the request criteria
	 *
	 * @param Filter Parameters defining the search area and criteria
	 * @param TargetingHandle The targeting handle of the request that will have its resulted searched for smart objects
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 *
	 * @return True if at least one candidate was found.
	 */
	SMARTOBJECTSMODULE_API bool FindSmartObjectsInTargetingRequest(const FSmartObjectRequestFilter& Filter, const FTargetingRequestHandle TargetingHandle, TArray<FSmartObjectRequestResult>& OutResults, const FConstStructView UserData) const;
	
	/**
	 * Spatial lookup for first slot in range respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param UserActor Actor claiming the smart object used to evaluate selection conditions
	 * @return First valid smart object in range. Not the closest one, just the one
	 *		that happens to be retrieved first from space partition
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API FSmartObjectRequestResult FindSmartObject(const FSmartObjectRequest& Request, const AActor* UserActor = nullptr) const;

	/**
	 * Blueprint function for spatial lookup for slot candidates respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param OutResults List of smart object slot candidates
	 * @param UserActor Actor claiming the smart object
	 * @return All valid smart objects in range.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SmartObject", meta=(DisplayName="Find Smart Objects"))
	bool FindSmartObjects_BP(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr) const
	{
		return FindSmartObjects(Request, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}

	/**
	 * Returns slots of a given smart object matching the filter.
	 * @param Handle Handle to the smart object.
	 * @param Filter Filter to apply on object and slots.
	 * @param OutSlots Available slots found that match the filter
	 * @param UserData Optional additional data that could be provided to bind values in the conditions evaluation context
	 */
	SMARTOBJECTSMODULE_API void FindSlots(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutSlots, const FConstStructView UserData = {}) const;

	/**
	 * Return all slots of a given smart object.
	 * @param Handle Handle to the smart object.
	 * @param OutSlots All slots of the smart object
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API void GetAllSlots(const FSmartObjectHandle Handle, TArray<FSmartObjectSlotHandle>& OutSlots) const;

	/**
	 * Evaluates conditions of each slot and add to the result array on success.
	 * Optional user data can be provided to bind parameters in evaluation context based
	 * on the schema used by the object definition.
	 * @param SlotsToFilter List of slot handles to apply selection conditions on
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return List of slot handles that pass all their selection conditions
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API TArray<FSmartObjectSlotHandle> FilterSlotsBySelectionConditions(
		const TConstArrayView<FSmartObjectSlotHandle>& SlotsToFilter,
		const FConstStructView UserData
		) const;
	
	/**
	 * Evaluates conditions of the slot specified by each request result and add to the result array on success.
	 * Optional user data can be provided to bind parameters in evaluation context based
	 * on the schema used by the object definition.
	 * @param ResultsToFilter List of request results to apply selection conditions on
	 * @param UserData The additional data that could be bound in the conditions evaluation context
	 * @return List of request results that pass all their selection conditions
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API TArray<FSmartObjectRequestResult> FilterResultsBySelectionConditions(
		const TConstArrayView<FSmartObjectRequestResult>& ResultsToFilter,
		const FConstStructView UserData
		) const;

	/**
	 * Evaluates conditions of the specified slot and its parent smart object.
	 * Optional user data can be provided to bind parameters in evaluation context based
	 * on the schema used by the object definition.
	 * @param SlotHandle Handle to the smart object slot
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return True if all conditions are met; false otherwise
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API bool EvaluateSelectionConditions(const FSmartObjectSlotHandle& SlotHandle, const FConstStructView UserData) const;

	/**
	 * Finds entrance location for a specific slot. Each slot can be annotated with multiple entrance locations, and the request can be configured to also consider the slot location as one entry.
	 * Additionally, the entrance locations can be checked to be on navigable surface (does not check that the point is reachable, though), traced on ground, and without of collisions.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param Request Request describing how to select the entry.
	 * @param Result Entry location result (in world space).
	 * @return True if valid entry found.
	 */
	SMARTOBJECTSMODULE_API bool FindEntranceLocationForSlot(const FSmartObjectSlotHandle& SlotHandle, const FSmartObjectSlotEntranceLocationRequest& Request, FSmartObjectSlotEntranceLocationResult& Result) const;
	
	/**
	 * Runs the same logic as FindEntranceLocationForSlot() but for a specific entrance location. The entrance handle can be get from entrance location result.
	 * @param EntranceHandle Handle to a specific smart object slot entrance.
	 * @param Request Request describing how to select the entry.
	 * @param Result Entry location result (in world space).
	 * @return True if result is valid.
	 */
	SMARTOBJECTSMODULE_API bool UpdateEntranceLocation(const FSmartObjectSlotEntranceHandle& EntranceHandle, const FSmartObjectSlotEntranceLocationRequest& Request, FSmartObjectSlotEntranceLocationResult& Result) const;

	/**
	 * Runs the entrance validation logic for all the slots in the smart object definition and returns all validated locations.
	 * This method can be used to a smart object definition before it is added to the simulation, for example to show some UI visualization while placing an actor with smart object.
	 * The method is static so it can be used even if the smart object subsystem is not present. 
	 * @param World World to use for validation tracing.
	 * @param SmartObjectDefinition Smart object definition to validate.
	 * @param SmartObjectTransform World transform of the smart object definition (e.g. smart object Component transform). 
	 * @param SkipActor An actor to skip during validation (this could be an actor representing the smart object during placement).
	 * @param Request Request describing how to validate the entries.
	 * @param Results All entrance locations, FSmartObjectSlotEntranceLocationResult::bIsValid can be used to check if a specific result is valid.
	 * @return True if any entrances were found.
	 */
	static SMARTOBJECTSMODULE_API bool QueryAllValidatedEntranceLocations(
		const UWorld* World,
		const USmartObjectDefinition& SmartObjectDefinition,
		const FTransform& SmartObjectTransform,
		const AActor* SkipActor,
		const FSmartObjectSlotEntranceLocationRequest& Request,
		TArray<FSmartObjectSlotEntranceLocationResult>& Results
	);
	
	/**
	 * Checks whether given slot is free and can be claimed (i.e. slot and its parent are both enabled)
	 * @note This methods doesn't evaluate the selection conditions. EvaluateSelectionConditions must be called separately.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param ClaimPriority Claim priority, a slot claimed at lower priority can be claimed by higher priority (unless already in use).
	 * @return true if the indicated slot can be claimed, false otherwise
	 * @see EvaluateSelectionConditions
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API bool CanBeClaimed(const FSmartObjectSlotHandle& SlotHandle, ESmartObjectClaimPriority ClaimPriority = ESmartObjectClaimPriority::Normal) const;

	/**
	 * Marks a smart object slot as claimed.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param ClaimPriority Claim priority, a slot claimed at lower priority can be claimed by higher priority (unless already in use).
	 * @param UserData Instanced struct that represents the interacting agent.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API FSmartObjectClaimHandle MarkSlotAsClaimed(const FSmartObjectSlotHandle& SlotHandle, ESmartObjectClaimPriority ClaimPriority, const FConstStructView UserData = {});

	/**
	 * Indicates if the object referred to by the given handle is still accessible in the simulation.
	 * This should only be required when a handle is stored and used later.
	 * @param Handle Handle to the smart object.
	 * @return True if the handle is valid and its associated object is accessible; false otherwise.
	 */
	SMARTOBJECTSMODULE_API bool IsSmartObjectValid(const FSmartObjectHandle Handle) const;

	/**
	 * Indicates if the object/slot referred to by the given handle are still accessible in the simulation.
	 * This should only be required when a handle is stored and later needed to access slot or object information (e.g. SlotView)
	 * Otherwise a valid ClaimHandle can be use directly after calling 'Claim'.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return True if the claim handle is valid and its associated object is accessible; false otherwise.
	 */
	SMARTOBJECTSMODULE_API bool IsClaimedSmartObjectValid(const FSmartObjectClaimHandle& ClaimHandle) const;

	/**
	 * Indicates if the slot referred to by the given handle is still accessible in the simulation.
	 * This should only be required when a handle is stored and later needed to access slot information (e.g. SlotView)
	 * Otherwise a valid SlotHandle can be use directly after calling any of the 'Find' or 'Claim' methods.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return True if the handle is valid and its associated slot is accessible; false otherwise.
	 */
	SMARTOBJECTSMODULE_API bool IsSmartObjectSlotValid(const FSmartObjectSlotHandle& SlotHandle) const;

	/**
	 * Marks a previously claimed smart object slot as occupied.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition the user wants to use.
	 * @return The base class pointer of the requested behavior definition class associated to the slot
	 */
	SMARTOBJECTSMODULE_API const USmartObjectBehaviorDefinition* MarkSlotAsOccupied(const FSmartObjectClaimHandle& ClaimHandle, TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass);

	/**
	 * Marks a previously claimed smart object slot as occupied.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return The requested behavior definition class pointer associated to the slot
	 */
	template <typename DefinitionType>
	const DefinitionType* MarkSlotAsOccupied(const FSmartObjectClaimHandle& ClaimHandle)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(MarkSlotAsOccupied(ClaimHandle, DefinitionType::StaticClass()));
	}

	/**
	 * Marks a claimed or occupied smart object as free.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Whether the claim was successfully released or not
	 */
	SMARTOBJECTSMODULE_API bool MarkSlotAsFree(const FSmartObjectClaimHandle& ClaimHandle);

	/**
	 * Return the behavior definition of a given type from a claimed object.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition.
	 * @return The base class pointer of the requested behavior definition class associated to the slotClaim handle
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DeterminesOutputType = "DefinitionClass"))
	SMARTOBJECTSMODULE_API const USmartObjectBehaviorDefinition* GetBehaviorDefinition(
		const FSmartObjectClaimHandle& ClaimHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);

	/**
	 * Return the behavior definition of a given type from a claimed object.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return The requested behavior definition class pointer associated to the Claim handle
	 */
	template <typename DefinitionType>
	const DefinitionType* GetBehaviorDefinition(const FSmartObjectClaimHandle& ClaimHandle)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(GetBehaviorDefinition(ClaimHandle, DefinitionType::StaticClass()));
	}

	/**
	 * Return the behavior definition of a given type from a request result.
	 * @param RequestResult A request result returned by any of the Find methods.
	 * @param DefinitionClass The type of behavior definition.
	 * @return The base class pointer of the requested behavior definition class associated to the request result
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DeterminesOutputType = "DefinitionClass"))
	SMARTOBJECTSMODULE_API const USmartObjectBehaviorDefinition* GetBehaviorDefinitionByRequestResult(
		const FSmartObjectRequestResult& RequestResult,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);

	/**
	 * Return the behavior definition of a given type from a claimed object.
	 * @param RequestResult A request result returned by any of the Find methods.
	 * @return The requested behavior definition class pointer associated to the request result
	 */
	template <typename DefinitionType>
	const DefinitionType* GetBehaviorDefinition(const FSmartObjectRequestResult& RequestResult)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(GetBehaviorDefinitionByRequestResult(RequestResult, DefinitionType::StaticClass()));
	}

	/**
	* Returns the state of the given smart object Slot handle.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject")
	SMARTOBJECTSMODULE_API ESmartObjectSlotState GetSlotState(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Adds state data to a slot instance. Data must be a struct that inherits
	 * from FSmartObjectSlotStateData and passed as a struct view (e.g. FConstStructView::Make(FSomeStruct))
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param InData A view on the struct to add.
	 */
	SMARTOBJECTSMODULE_API void AddSlotData(const FSmartObjectClaimHandle& ClaimHandle, FConstStructView InData);

	UE_DEPRECATED(5.6, "Use ReadSlotData or MutateSlotData instead.")
	SMARTOBJECTSMODULE_API FSmartObjectSlotView GetSlotView(const FSmartObjectSlotHandle& SlotHandle) const;

	/**
	 * Executes the provided function if a valid const view can be created for the provided slot handle.
	 * In a multithreaded scenario, the method will prevent the slot from being modified
	 * during the execution of the function.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param Function Function to execute if a valid view can be created
	 * @return Whether the slot was found, was valid, and the function was called
	 * @see MutateSlotData
	 */
	SMARTOBJECTSMODULE_API bool ReadSlotData(const FSmartObjectSlotHandle& SlotHandle, TFunctionRef<void(FConstSmartObjectSlotView)> Function) const;

	/**
	 * Executes the provided function if a valid mutable view can be created for the provided slot handle.
	 * In a multithreaded scenario, the method will prevent the slot from being modified
	 * during the execution of the function.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param Function Function to execute if a valid view can be created
	 * @return Whether the slot was found, was valid, and the function was called
	 * @see ReadSlotData
	 */
	SMARTOBJECTSMODULE_API bool MutateSlotData(const FSmartObjectSlotHandle& SlotHandle, TFunctionRef<void(const FSmartObjectSlotView&)> Function) const;

	/**
	 * Returns the position (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Position (in world space) of the slot associated to ClaimHandle.
	 */
	TOptional<FVector> GetSlotLocation(const FSmartObjectClaimHandle& ClaimHandle) const
	{
		return GetSlotLocation(ClaimHandle.SlotHandle);
	}
	
	/**
	 * Returns the position (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param OutSlotLocation Position (in world space) of the slot associated to ClaimHandle.
	 * @return Whether the location was found and assigned to 'OutSlotLocation'
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API bool GetSlotLocation(const FSmartObjectClaimHandle& ClaimHandle, FVector& OutSlotLocation) const;

	/**
	 * Returns the position (in world space) of the slot associated to the given request result.
	 * @param Result A request result returned by any of the Find methods.
	 * @return Position (in world space) of the slot associated to Result.
	 */
	SMARTOBJECTSMODULE_API TOptional<FVector> GetSlotLocation(const FSmartObjectRequestResult& Result) const;

	/**
	 * Returns the position (in world space) of the slot represented by the provided slot handle.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return Position (in world space) of the slot associated to SlotHandle.
	 */
	SMARTOBJECTSMODULE_API TOptional<FVector> GetSlotLocation(const FSmartObjectSlotHandle& SlotHandle) const;

	/**
	 * Returns the transform (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Transform (in world space) of the slot associated to ClaimHandle.
	 */
	TOptional<FTransform> GetSlotTransform(const FSmartObjectClaimHandle& ClaimHandle) const
	{
		return GetSlotTransform(ClaimHandle.SlotHandle);
	}

	/**
	 * Returns the transform (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param OutSlotTransform Transform (in world space) of the slot associated to ClaimHandle.
	 * @return Whether the transform was found and assigned to 'OutSlotTransform'
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API bool GetSlotTransform(const FSmartObjectClaimHandle& ClaimHandle, FTransform& OutSlotTransform) const;

	/**
	 * Returns the transform (in world space) of the slot associated to the given request result.
	 * @param Result A request result returned by any of the Find methods.
	 * @return Transform (in world space) of the slot associated to Result.
	 */
	SMARTOBJECTSMODULE_API TOptional<FTransform> GetSlotTransform(const FSmartObjectRequestResult& Result) const;

	/**
	 * Returns the transform (in world space) of the slot associated to the given RequestResult.
	 * @param RequestResult Result returned by any of the FindSmartObject(s) methods.
	 * @param OutSlotTransform Transform (in world space) of the slot associated to the RequestResult.
	 * @return Whether the transform was found and assigned to 'OutSlotTransform'
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API bool GetSlotTransformFromRequestResult(const FSmartObjectRequestResult& RequestResult, FTransform& OutSlotTransform) const;

	/**
	 * Returns the transform (in world space) of the slot represented by the provided slot handle.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return Transform (in world space) of the slot associated to SlotHandle.
	 */
	SMARTOBJECTSMODULE_API TOptional<FTransform> GetSlotTransform(const FSmartObjectSlotHandle& SlotHandle) const;

	/**
	 * Similarly to GetSlotTransform fetches the transform (in world space) of the indicated slot, but assumes the slot 
	 * handle is valid and that the EntityManager is known. The burden of ensuring that's the case is on the caller. 
	 * @param SlotHandle Handle to a smart object slot.
	 * @return Transform (in world space) of the slot associated to SlotHandle.
	 */
	SMARTOBJECTSMODULE_API FTransform GetSlotTransformChecked(const FSmartObjectSlotHandle& SlotHandle) const;

	/**
	 * Returns a view on the owner data for the smart object instance represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @return Unvalidated view of the smart object instance owner data.
	 * @note The returned view points to data that is only valid as long as the object is registered
	 * so it should be read immediately after calling this method or stored in an instances struct.
	 */
	SMARTOBJECTSMODULE_API FConstStructView GetOwnerData(const FSmartObjectHandle Handle) const;

	/**
	 * Returns the list of tags associated to the smart object instance represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @return Container of tags associated to the smart object instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API const FGameplayTagContainer& GetInstanceTags(const FSmartObjectHandle Handle) const;

	/**
	 * Adds a single tag to the smart object instance represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @param Tag Tag to add to the smart object instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API void AddTagToInstance(const FSmartObjectHandle Handle, const FGameplayTag& Tag);

	/**
	 * Removes a single tag from the smart object instance represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @param Tag Tag to remove from the smart object instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API void RemoveTagFromInstance(const FSmartObjectHandle Handle, const FGameplayTag& Tag);

	/**
	 * Returns pointer to the smart object instance event delegate.
	 * @param SmartObjectHandle Handle to the smart object.
	 * @return Pointer to object's delegate, or nullptr if instance doesn't exists.
	 * @note The delegate can be broadcast from any thread so it is the responsibility of the caller
	 * to make sure that the operations executed are safe. 
	 */
	SMARTOBJECTSMODULE_API FOnSmartObjectEvent* GetEventDelegate(const FSmartObjectHandle SmartObjectHandle);
	
	/**
	 * Returns the list of tags associated to the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @return Container of tags associated to the smart object instance, or empty container if slot was not valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API const FGameplayTagContainer& GetSlotTags(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Adds a single tag to the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param Tag Tag to add to the smart object slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API void AddTagToSlot(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag& Tag);

	/**
	 * Removes a single tag from the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param Tag Tag to remove from the smart object slot.
	 * @return True if the tag was removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API bool RemoveTagFromSlot(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag& Tag);

	/**
	 * Enables or disables the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param bEnabled If true enables the slot, if false, disables the slot.
	 * @return Previous enabled state. 
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	SMARTOBJECTSMODULE_API bool SetSlotEnabled(const FSmartObjectSlotHandle SlotHandle, const bool bEnabled);

	/**
	 * Sends event to a smart object slot.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param EventTag Gameplay Tag describing the event type.
	 * @param Payload Struct payload for the event.
	 * @return True if the event was successfully sent. 
	 */
	SMARTOBJECTSMODULE_API bool SendSlotEvent(const FSmartObjectSlotHandle& SlotHandle, const FGameplayTag EventTag, const FConstStructView Payload = FConstStructView());

	/**
	 * Returns pointer to the smart object changed delegate associated to the provided handle.
	 * The delegate is shared for all slots so listeners must filter using 'Event.SlotHandle'.
	 * @param SlotHandle Handle to the smart object slot.
	 * @return Pointer to slot's delegate, or nullptr if slot does not exists.
	 * @note The delegate can be broadcast from any thread so it is the responsibility of the caller
	 * to make sure that the operations executed are safe.
	 */
	SMARTOBJECTSMODULE_API FOnSmartObjectEvent* GetSlotEventDelegate(const FSmartObjectSlotHandle& SlotHandle);
	
	/**
	 * Register a callback to be notified if the claimed slot is no longer available and user need to perform cleanup.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param Callback Delegate that will be called to notify that a slot gets invalidated and can no longer be used.
	 */
	SMARTOBJECTSMODULE_API void RegisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle, const FOnSlotInvalidated& Callback);

	/**
	 * Unregisters a callback to be notified if the claimed slot is no longer available and user need to perform cleanup.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 */
	SMARTOBJECTSMODULE_API void UnregisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle);

#if UE_ENABLE_DEBUG_DRAWING
	SMARTOBJECTSMODULE_API void DebugDraw(FDebugRenderSceneProxy* DebugProxy) const;
	void DebugDrawCanvas(UCanvas* Canvas, APlayerController* PlayerController) const
	{
		// Placeholder in case of future needs
	}
#endif

#if WITH_EDITORONLY_DATA
	/** 
	 * Special-purpose function used to set up an instance of ASmartObjectPersistentCollection with data from a given
	 * instance of ADEPRECATED_SmartObjectCollection 
	 */
	static SMARTOBJECTSMODULE_API void CreatePersistentCollectionFromDeprecatedData(UWorld& World, const ADEPRECATED_SmartObjectCollection& DeprecatedCollection);

	TConstArrayView<TWeakObjectPtr<ASmartObjectPersistentCollection>> GetRegisteredCollections() const
	{
		return RegisteredCollections;
	}

	TArrayView<TWeakObjectPtr<ASmartObjectPersistentCollection>> GetMutableRegisteredCollections()
	{
		return RegisteredCollections;
	}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	mutable FOnMainCollectionEvent OnMainCollectionChanged;
	mutable FOnMainCollectionEvent OnMainCollectionDirtied;
#endif

protected:
	friend UWorldPartitionSmartObjectCollectionBuilder;

	SMARTOBJECTSMODULE_API bool UnregisterSmartObjectInternal(TNotNull<USmartObjectComponent*> SmartObjectComponent, const bool bDestroyRuntimeState);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	bool UnregisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent, const bool bDestroyRuntimeState)
	{
		return UnregisterSmartObjectInternal(&SmartObjectComponent, bDestroyRuntimeState);
	}

	/**
	 * Callback overridden to gather loaded collections, spawn missing one and set the main collection.
	 * @note we use this method instead of `Initialize` or `PostInitialize` so active level is set and actors registered.
	 */
	SMARTOBJECTSMODULE_API virtual void OnWorldComponentsUpdated(UWorld& World) override;

	/**
	 * BeginPlay will push all objects stored in the collection to the runtime simulation
	 * and initialize octree using collection bounds.
	 */
	SMARTOBJECTSMODULE_API virtual void OnWorldBeginPlay(UWorld& World) override;

	// USubsystem BEGIN
	SMARTOBJECTSMODULE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	SMARTOBJECTSMODULE_API virtual void Deinitialize() override;
	SMARTOBJECTSMODULE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// USubsystem END

	/** Creates all runtime data using main collection */
	SMARTOBJECTSMODULE_API void InitializeRuntime();

	/** Removes all runtime data */
	SMARTOBJECTSMODULE_API virtual void CleanupRuntime();

	/**
	 * Returns the runtime instance associated to the provided handle
	 */
	FSmartObjectRuntime* GetRuntimeInstanceInternal(const FSmartObjectHandle SmartObjectHandle)
	{
		return RuntimeSmartObjects.Find(SmartObjectHandle);
	}

	UE_DEPRECATED(5.6, "This method will no longer be exposed, use ExecuteOnValidatedRuntime instead.")
	FSmartObjectRuntime* GetRuntimeInstance(const FSmartObjectHandle SmartObjectHandle)
	{
		return GetRuntimeInstanceInternal(SmartObjectHandle);
	}

	/**
	 * Returns the const runtime instance associated to the provided handle
	 */
	const FSmartObjectRuntime* GetRuntimeInstanceInternal(const FSmartObjectHandle SmartObjectHandle) const
	{
		return RuntimeSmartObjects.Find(SmartObjectHandle);
	}

	UE_DEPRECATED(5.6, "This method will no longer be exposed, use ExecuteOnValidatedRuntime instead.")
	const FSmartObjectRuntime* GetRuntimeInstance(const FSmartObjectHandle SmartObjectHandle) const
	{
		return GetRuntimeInstanceInternal(SmartObjectHandle);
	}

	/**
	 * Indicates if the handle is set and the slot referred to is still accessible in the simulation.
	 * Log is produced for any failing condition using provided LogContext.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param CallingFunctionName String describing the context in which the method is called (e.g. caller function name).
	 * @return True if the handle is valid and its associated slot is accessible; false otherwise.
	 */
	SMARTOBJECTSMODULE_API bool IsSlotValidVerbose(const FSmartObjectSlotHandle& SlotHandle, const ANSICHAR* CallingFunctionName) const;

	/**
	 * Returns the const runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param OutSmartObjectRuntime Runtime instance matching the slot handle.
	 * @param OutSlot Slot runtime matching the slot handle.
	 * @param CallingFunctionName String describing the context in which the method is called (e.g. caller function name).
	 * @return true if a valid matching runtime instance and slot were found.
	 * @see ExecuteOnValidatedRuntimeAndSlot
	 */
	SMARTOBJECTSMODULE_API bool GetValidatedRuntimeAndSlotInternal(const FSmartObjectSlotHandle& SlotHandle, const FSmartObjectRuntime*& OutSmartObjectRuntime, const FSmartObjectRuntimeSlot*& OutSlot, const ANSICHAR* CallingFunctionName) const;
	UE_DEPRECATED(5.6, "This method will no longer be exposed, use ExecuteOnValidatedRuntimeAndSlot instead.")
	bool GetValidatedRuntimeAndSlot(const FSmartObjectSlotHandle SlotHandle, const FSmartObjectRuntime*& OutSmartObjectRuntime, const FSmartObjectRuntimeSlot*& OutSlot, const ANSICHAR* CallingFunctionName) const
	{
		return GetValidatedRuntimeAndSlotInternal(SlotHandle, OutSmartObjectRuntime, OutSlot, CallingFunctionName);
	}

	/**
	 * Executes the provided function using the runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * Method provides a thread safe way to read information from a runtime instance or from its slots.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param Function Function to execute if a valid view can be created.
	 * @param CallingFunctionName String describing the context in which the method is called (e.g. caller function name).
	 * @return Whether the runtime instance and slot were found, were valid, and the function was called
	 */
	SMARTOBJECTSMODULE_API bool ExecuteOnValidatedRuntimeAndSlot(const FSmartObjectSlotHandle& SlotHandle, TFunctionRef<void(const FSmartObjectRuntime&, const FSmartObjectRuntimeSlot&)> Function, const ANSICHAR* CallingFunctionName) const;

	/**
	 * Returns the mutable runtime instance associated to the provided handle
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * @see ExecuteOnValidatedMutableRuntimeAndSlot
	 */
	SMARTOBJECTSMODULE_API bool GetValidatedMutableRuntimeAndSlotInternal(const FSmartObjectSlotHandle& SlotHandle, FSmartObjectRuntime*& OutSmartObjectRuntime, FSmartObjectRuntimeSlot*& OutSlot, const ANSICHAR* CallingFunctionName) const;
	UE_DEPRECATED(5.6, "This method will no longer be exposed, use ExecuteOnValidatedMutableRuntimeAndSlot instead.")
	bool GetValidatedMutableRuntimeAndSlot(const FSmartObjectSlotHandle SlotHandle, FSmartObjectRuntime*& OutSmartObjectRuntime, FSmartObjectRuntimeSlot*& OutSlot, const ANSICHAR* CallingFunctionName) const
	{
		return GetValidatedMutableRuntimeAndSlotInternal(SlotHandle, OutSmartObjectRuntime, OutSlot, CallingFunctionName);
	}

	/**
	 * Executes the provided function using the runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * Method provides a thread safe way to modify a runtime instance or its slots.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param Function Function to execute if a valid view can be created.
	 * @param CallingFunctionName String describing the context in which the method is called (e.g. caller function name).
	 * @return Whether the runtime instance and slot were found, were valid, and the function was called
	 */
	SMARTOBJECTSMODULE_API bool ExecuteOnValidatedMutableRuntimeAndSlot(const FSmartObjectSlotHandle& SlotHandle, TFunctionRef<void(FSmartObjectRuntime&, FSmartObjectRuntimeSlot&)> Function, const ANSICHAR* CallingFunctionName) const;

	/**
	 * Returns the const runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * @see ExecuteOnValidatedRuntime
	 */
	SMARTOBJECTSMODULE_API const FSmartObjectRuntime* GetValidatedRuntimeInternal(const FSmartObjectHandle Handle, const ANSICHAR* CallingFunctionName) const;
	UE_DEPRECATED(5.6, "This method will no longer be exposed, use ExecuteOnValidatedRuntime instead.")
	const FSmartObjectRuntime* GetValidatedRuntime(const FSmartObjectHandle Handle, const ANSICHAR* CallingFunctionName) const
	{
		return GetValidatedRuntimeInternal(Handle, CallingFunctionName);
	}

	/**
	 * Executes the provided function using the runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * Method provides a thread safe way to read information from a runtime instance.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param Function Function to execute if a valid view can be created.
	 * @param CallingFunctionName String describing the context in which the method is called (e.g. caller function name).
	 * @return Whether the runtime instance was found, was valid, and the function was called
	 */
	SMARTOBJECTSMODULE_API bool ExecuteOnValidatedRuntime(const FSmartObjectHandle SlotHandle, TFunctionRef<void(const FSmartObjectRuntime&)> Function, const ANSICHAR* CallingFunctionName) const;

	/**
	 * Returns the mutable runtime instance associated to the provided handle
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * @see ExecuteOnValidatedMutableRuntime
	 */
	SMARTOBJECTSMODULE_API FSmartObjectRuntime* GetValidatedMutableRuntimeInternal(const FSmartObjectHandle Handle, const ANSICHAR* CallingFunctionName) const;
	UE_DEPRECATED(5.6, "This method will no longer be exposed, use ExecuteOnValidatedRuntime instead.")
	FSmartObjectRuntime* GetValidatedMutableRuntime(const FSmartObjectHandle Handle, const ANSICHAR* CallingFunctionName) const
	{
		return GetValidatedMutableRuntimeInternal(Handle, CallingFunctionName);
	}

	/**
	 * Executes the provided function using the runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 * Method provides a thread safe way to modify a runtime instance.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param Function Function to execute if a valid view can be created.
	 * @param CallingFunctionName String describing the context in which the method is called (e.g. caller function name).
	 * @return Whether the runtime instance was found, was valid, and the function was called
	 */
	SMARTOBJECTSMODULE_API bool ExecuteOnValidatedMutableRuntime(const FSmartObjectHandle SlotHandle, TFunctionRef<void(FSmartObjectRuntime&)> Function, const ANSICHAR* CallingFunctionName) const;

	static SMARTOBJECTSMODULE_API void AddTagToInstanceInternal(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	static void AddTagToInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag)
	{
		AddTagToInstanceInternal(SmartObjectRuntime, Tag);
	}

	static SMARTOBJECTSMODULE_API void RemoveTagFromInstanceInternal(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	static void RemoveTagFromInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag)
	{
		RemoveTagFromInstanceInternal(SmartObjectRuntime, Tag);
	}

	static SMARTOBJECTSMODULE_API void OnSlotChangedInternal(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectRuntimeSlot& Slot,
		const FSmartObjectSlotHandle& SlotHandle,
		const ESmartObjectChangeReason Reason,
		const FConstStructView Payload = {},
		const FGameplayTag ChangedTag = FGameplayTag()
		);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	static void OnSlotChanged(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectRuntimeSlot& Slot,
		const FSmartObjectSlotHandle SlotHandle,
		const ESmartObjectChangeReason Reason,
		const FConstStructView Payload = {},
		const FGameplayTag ChangedTag = FGameplayTag()
		)
	{
		OnSlotChangedInternal(SmartObjectRuntime, Slot, SlotHandle, Reason, Payload, ChangedTag);
	}

	/** Goes through all defined slots of smart object represented by SmartObjectRuntime and finds the ones matching the filter. */
	SMARTOBJECTSMODULE_API void FindSlotsInternal(
		const FSmartObjectHandle Handle,
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectRequestFilter& Filter,
		 TArray<FSmartObjectSlotHandle>& OutResults,
		 const FConstStructView UserData) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	void FindSlots(
		const FSmartObjectHandle Handle,
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectRequestFilter& Filter,
		 TArray<FSmartObjectSlotHandle>& OutResults,
		 const FConstStructView UserData) const
	{
		FindSlotsInternal(Handle, SmartObjectRuntime, Filter, OutResults, UserData);
	}

	/** Applies filter on provided definition and fills OutValidIndices with indices of all valid slots. */
	static SMARTOBJECTSMODULE_API void FindMatchingSlotDefinitionIndicesInternal(const USmartObjectDefinition& Definition, const FSmartObjectRequestFilter& Filter, TArray<int32>& OutValidIndices);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	static void FindMatchingSlotDefinitionIndices(const USmartObjectDefinition& Definition, const FSmartObjectRequestFilter& Filter, TArray<int32>& OutValidIndices)
	{
		FindMatchingSlotDefinitionIndicesInternal(Definition, Filter, OutValidIndices);
	}

	SMARTOBJECTSMODULE_API void ExecuteOnSlotFilteredBySelectionConditions(
		const TConstStridedView<FSmartObjectSlotHandle> SlotsToFilter,
		const FConstStructView UserData,
		TFunctionRef<void(const int32 /*SlotIndex*/)> Function
		) const;

	static SMARTOBJECTSMODULE_API const USmartObjectBehaviorDefinition* GetBehaviorDefinitionInternal(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle& SlotHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	static const USmartObjectBehaviorDefinition* GetBehaviorDefinition(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle SlotHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		)
	{
		return GetBehaviorDefinitionInternal(SmartObjectRuntime, SlotHandle, DefinitionClass);
	}

	SMARTOBJECTSMODULE_API const USmartObjectBehaviorDefinition* MarkSlotAsOccupiedInternal(
		FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectClaimHandle& ClaimHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	const USmartObjectBehaviorDefinition* MarkSlotAsOccupied(
		FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectClaimHandle& ClaimHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		)
	{
		return MarkSlotAsOccupiedInternal(SmartObjectRuntime, ClaimHandle, DefinitionClass);
	}

	SMARTOBJECTSMODULE_API void AbortAllInternal(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	void AbortAll(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime) const
	{
		AbortAllInternal(Handle, SmartObjectRuntime);
	}

	/** Make sure that all SmartObjectCollection actors from our associated world are registered. */
	SMARTOBJECTSMODULE_API void RegisterCollectionInstances();

	SMARTOBJECTSMODULE_API void AddContainerToSimulation(const FSmartObjectContainer& InSmartObjectContainer);

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 */
	SMARTOBJECTSMODULE_API FSmartObjectRuntime* AddCollectionEntryToSimulationInternal(
		const FSmartObjectCollectionEntry& Entry,
		const USmartObjectDefinition& Definition,
		USmartObjectComponent* OwnerComponent
		);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	FSmartObjectRuntime* AddCollectionEntryToSimulation(
		const FSmartObjectCollectionEntry& Entry,
		const USmartObjectDefinition& Definition,
		USmartObjectComponent* OwnerComponent
		)
	{
		return AddCollectionEntryToSimulationInternal(Entry, Definition, OwnerComponent);
	}

	/*
	 * Initializes preconditions, adds to the space partition structure using the specified bounds and broadcasts event.
	 * @param SmartObjectRuntime The runtime instance of the smart object to initialize
	 * @param Bounds Bounds used to register the object in the space partition structure
	 * @return Pointer to the created runtime or nullptr if an error occurs.
	 */
	SMARTOBJECTSMODULE_API FSmartObjectRuntime* CreateRuntimeInstance(const FSmartObjectHandle Handle, const USmartObjectDefinition& Definition, const FBox& Bounds, USmartObjectComponent* OwnerComponent = nullptr);

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime entry might be created or an existing one found
	 * @param CollectionEntry The associated collection entry that got created to add the component to the simulation.
	 */
	SMARTOBJECTSMODULE_API FSmartObjectRuntime* AddComponentToSimulationInternal(TNotNull<USmartObjectComponent*> SmartObjectComponent, const FSmartObjectCollectionEntry& CollectionEntry);

	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	FSmartObjectRuntime* AddComponentToSimulation(USmartObjectComponent& SmartObjectComponent, const FSmartObjectCollectionEntry& CollectionEntry)
	{
		return AddComponentToSimulationInternal(&SmartObjectComponent, CollectionEntry);
	}

	/**
	 * Binds a smart object component to an existing instance in the simulation and notifies that it has been bound.
	 * If a given SmartObjectComponent has not been registered yet an ensure will trigger.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime instance must exist
	 * @param SmartObjectRuntime Associated runtime structure
	*/
	SMARTOBJECTSMODULE_API void BindComponentToSimulationInternal(TNotNull<USmartObjectComponent*> SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime) const;

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	void BindComponentToSimulationInternal(USmartObjectComponent& SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime) const
	{
		return BindComponentToSimulationInternal(&SmartObjectComponent, SmartObjectRuntime);
	}

	/**
	 * Unbinds a smart object component from the given FSmartObjectRuntime instance.
	 * Note that the component is still registered to the subsystem.
	 * @param SmartObjectComponent The component to remove from the simulation
	 * @param SmartObjectRuntime runtime data representing the component being removed
	 */
	SMARTOBJECTSMODULE_API void UnbindComponentFromSimulationInternal(TNotNull<USmartObjectComponent*> SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime) const;

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	void UnbindComponentFromSimulationInternal(USmartObjectComponent& SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime) const
	{
		return UnbindComponentFromSimulationInternal(&SmartObjectComponent, SmartObjectRuntime);
	}

	/**
	 * Removes a runtime instance from the simulation.
	 * Note that the component is still registered to the subsystem.
	 * @return whether the removal was successful
	 */
	SMARTOBJECTSMODULE_API bool RemoveRuntimeInstanceFromSimulationInternal(FSmartObjectRuntime& SmartObjectRuntime, USmartObjectComponent* SmartObjectComponent = nullptr);
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	bool RemoveRuntimeInstanceFromSimulation(FSmartObjectRuntime& SmartObjectRuntime, USmartObjectComponent* SmartObjectComponent = nullptr)
	{
		return RemoveRuntimeInstanceFromSimulationInternal(SmartObjectRuntime, SmartObjectComponent);
	}

	/**
	 * Finds the runtime instance associated to the collection entry and removes it from the simulation.
	 * Note that if there is an associated component it is still registered to the subsystem.
	 * @return whether the removal was successful
	 */
	SMARTOBJECTSMODULE_API bool RemoveCollectionEntryFromSimulation(const FSmartObjectCollectionEntry& Entry);

	/**
	 * Finds the runtime instance associated to the component and removes it from the simulation.
	 * Note that the component is still registered to the subsystem.
	 */
	SMARTOBJECTSMODULE_API void RemoveComponentFromSimulation(TNotNull<USmartObjectComponent*> SmartObjectComponent);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	void RemoveComponentFromSimulation(USmartObjectComponent& SmartObjectComponent)
	{
		return RemoveComponentFromSimulation(&SmartObjectComponent);
	}

	/** Destroy SmartObjectRuntime contents as Handle's representation. */
	SMARTOBJECTSMODULE_API void DestroyRuntimeInstanceInternal(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime);

	/**
	 * Activate preconditions on the main object.
	 * @param ContextData The context data to use for conditions evaluation
	 * @param SmartObjectRuntime Runtime struct associated to the smart object
	 * @return True if conditions are successfully activated; false otherwise
	 */
	SMARTOBJECTSMODULE_API bool ActivateObjectPreconditionsInternal(const FWorldConditionContextData& ContextData, const FSmartObjectRuntime& SmartObjectRuntime) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	bool ActivateObjectPreconditions(const FWorldConditionContextData& ContextData, const FSmartObjectRuntime& SmartObjectRuntime) const
	{
		return ActivateObjectPreconditionsInternal(ContextData, SmartObjectRuntime);
	}

	/**
	 * Activate preconditions on the specified slot.
	 * @param ContextData The context data to fill and use for conditions evaluation
	 * @param Slot Runtime struct associated to the smart object slot
	 * @param SlotHandle Handle to the smart object slot
	 * @return True if all conditions are successfully activated; false otherwise
	 */
	SMARTOBJECTSMODULE_API bool ActivateSlotPreconditionsInternal(FWorldConditionContextData& ContextData, const FSmartObjectRuntimeSlot& Slot, const FSmartObjectSlotHandle& SlotHandle) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	bool ActivateSlotPreconditions(FWorldConditionContextData& ContextData, const FSmartObjectRuntimeSlot& Slot, FSmartObjectSlotHandle SlotHandle) const
	{
		return ActivateSlotPreconditionsInternal(ContextData, Slot, SlotHandle);
	}

	/**
	 * Activate preconditions on the main object and all its slots.
	 * Currently the conditions require an actor so this method will try to fetch it if it is currently dehydrated.
	 * @param SmartObjectRuntime Runtime struct associated to the smart object
	 * @return True if all conditions are successfully activated; false otherwise
	 */
	SMARTOBJECTSMODULE_API bool TryActivatePreconditionsInternal(const FSmartObjectRuntime& SmartObjectRuntime) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	bool TryActivatePreconditions(const FSmartObjectRuntime& SmartObjectRuntime) const
	{
		return TryActivatePreconditionsInternal(SmartObjectRuntime);
	}

	/**
	 * Fills the provided context data with the smart object actor and handle associated to 'SmartObjectRuntime' and the subsystem. 
	 * @param ContextData The context data to fill
	 * @param SmartObjectRuntime The runtime instance of the smart object for which the context must be filled 
	 */
	SMARTOBJECTSMODULE_API void SetupConditionContextCommonDataInternal(FWorldConditionContextData& ContextData, const FSmartObjectRuntime& SmartObjectRuntime) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	void SetupConditionContextCommonData(FWorldConditionContextData& ContextData, const FSmartObjectRuntime& SmartObjectRuntime) const
	{
		SetupConditionContextCommonDataInternal(ContextData, SmartObjectRuntime);
	}

	/**
	 * Binds properties of the context data to property values of the user data struct when they match type and name.
	 * @param ContextData The context data to fill
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 */
	SMARTOBJECTSMODULE_API void BindPropertiesFromStructInternal(FWorldConditionContextData& ContextData, const FConstStructView& UserData) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	void BindPropertiesFromStruct(FWorldConditionContextData& ContextData, const FConstStructView& UserData) const
	{
		BindPropertiesFromStructInternal(ContextData, UserData);
	}

	/**
	 * Use the provided context data that is expected to be already filled by calling 'SetupConditionContextCommonData'
	 * and adds the slot related part. It then evaluates all conditions associated to the specified slot.  
	 * @param ConditionContextData The context data to fill and use for conditions evaluation
	 * @param SmartObjectRuntime Runtime struct associated to the smart object slot
	 * @param SlotHandle Handle to the smart object slot
	 * @return True if all conditions are met; false otherwise
	 * @see SetupDefaultConditionsContext
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API bool EvaluateSlotConditionsInternal(
		FWorldConditionContextData& ConditionContextData,
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle& SlotHandle
		) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	[[nodiscard]] bool EvaluateSlotConditions(
		FWorldConditionContextData& ConditionContextData,
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle SlotHandle
		) const
	{
		return EvaluateSlotConditionsInternal(ConditionContextData, SmartObjectRuntime, SlotHandle);
	}

	/**
	 * Use the provided context data that is expected to be already filled by calling 'SetupConditionContextCommonData'
	 * and evaluates all conditions associated to the specified object.
	 * @param ConditionContextData The context data to use for conditions evaluation
	 * @param SmartObjectRuntime Runtime data representing the smart object for which the conditions must be evaluated
	 * @return True if all conditions are met; false otherwise
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API bool EvaluateObjectConditionsInternal(const FWorldConditionContextData& ConditionContextData, const FSmartObjectRuntime& SmartObjectRuntime) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	[[nodiscard]] bool EvaluateObjectConditions(const FWorldConditionContextData& ConditionContextData, const FSmartObjectRuntime& SmartObjectRuntime) const
	{
		return EvaluateObjectConditionsInternal(ConditionContextData, SmartObjectRuntime);
	}

	/**
	 * Internal helper for filter methods to build the list of accepted slots
	 * by reusing context data and schema as much as possible.
	 */
	[[nodiscard]] SMARTOBJECTSMODULE_API bool EvaluateConditionsForFilteringInternal(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle& SlotHandle,
		FWorldConditionContextData& ContextData,
		const FConstStructView UserData,
		TPair<const FSmartObjectRuntime*, bool>& LastEvaluatedRuntime
		) const;
	UE_DEPRECATED(5.6, "Use the version with the 'Internal' suffix instead and make sure that the required access detectors are used.")
	[[nodiscard]] bool EvaluateConditionsForFiltering(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle SlotHandle,
		FWorldConditionContextData& ContextData,
		const FConstStructView UserData,
		TPair<const FSmartObjectRuntime*, bool>& LastEvaluatedRuntime
		) const
	{
		return EvaluateConditionsForFilteringInternal(SmartObjectRuntime, SlotHandle, ContextData, UserData, LastEvaluatedRuntime);
	}

	/**
	 * Finds entrance location for a specific slot. Each slot can be annotated with multiple entrance locations, and the request can be configured to also consider the slot location as one entry.
	 * Additionally the entrance locations can be checked to be on navigable surface (does not check that the point is reachable, though), traced on ground, and without of collisions.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param SlotEntranceHandle Handle to specific entrance if just one entrance should be checked. (Optional)
	 * @param Request Request describing how to select the entry.
	 * @param OutResult Entry location result (in world space).
	 * @return True if valid entry found.
	 */
	SMARTOBJECTSMODULE_API bool FindEntranceLocationInternal(
		const FSmartObjectSlotHandle& SlotHandle,
		const FSmartObjectSlotEntranceHandle& SlotEntranceHandle,
		const FSmartObjectSlotEntranceLocationRequest& Request,
		FSmartObjectSlotEntranceLocationResult& OutResult
		) const;

	/**
	 * Validates entrance locations for a specific slot. Each slot can be annotated with multiple entrance locations, and the request can be configured to also consider the slot location as one entry.
	 * Additionally the entrance locations can be checked to be on navigable surface (does not check that the point is reachable, though), traced on ground, and without of collisions.
	 * @param World World to use for validation tracing. 
	 * @param ValidationContext Valid validation context.
	 * @param Request Request describing how to validate the entries.
	 * @param SlotHandle Handle to the smart object slot (will be passed into the result).
	 * @param SlotDefinition Smart object slot definition to use for validation.
	 * @param SlotTransform Transform of the slot.
	 * @param SlotEntranceHandle Handle to specific entrance if just one entrance should be checked. (Optional)
	 * @param ResultFunc Callback called on each result
	 */
	static SMARTOBJECTSMODULE_API void QueryValidatedSlotEntranceLocationsInternal(
		const UWorld* World,
		FSmartObjectValidationContext& ValidationContext,
		const FSmartObjectSlotEntranceLocationRequest& Request,
		const FSmartObjectSlotHandle& SlotHandle,
		const FSmartObjectSlotDefinition& SlotDefinition,
		const FTransform& SlotTransform,
		const FSmartObjectSlotEntranceHandle& SlotEntranceHandle,
		TFunctionRef<bool(const FSmartObjectSlotEntranceLocationResult&)> ResultFunc
		);

	/**
	 * Name of the Space partition class to use.
	 * Usage:
	 *		[/Script/SmartObjectsModule.SmartObjectSubsystem]
	 *		SpacePartitionClassName=/Script/SmartObjectsModule.<SpacePartitionClassName>
	 */
	UPROPERTY(config, meta=(MetaClass="/Script/SmartObjectsModule.SmartObjectSpacePartition", DisplayName="Spatial Representation Structure Class"))
	FSoftClassPath SpacePartitionClassName;

	UPROPERTY()
	TSubclassOf<USmartObjectSpacePartition> SpacePartitionClass;

	UPROPERTY()
	TObjectPtr<USmartObjectSpacePartition> SpacePartition;

	UPROPERTY()
	TObjectPtr<ASmartObjectSubsystemRenderingActor> RenderingActor;

	UPROPERTY(Transient)
	FSmartObjectContainer SmartObjectContainer;

	TArray<TWeakObjectPtr<ASmartObjectPersistentCollection>> RegisteredCollections;

	/**
	 * A map of registered smart object handles to their associated runtime data.
	 * Client side smart object Subsystem's will only have runtime data
	 * for smart object Components who enable replication, but server subsystems will have all smart object
	 * data.
	 */
	UPROPERTY(Transient)
	TMap<FSmartObjectHandle, FSmartObjectRuntime> RuntimeSmartObjects;

	/** List of registered components. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USmartObjectComponent>> RegisteredSOComponents;

	/** smart objects that attempted to register while no collection was being present */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USmartObjectComponent>> PendingSmartObjectRegistration;

	/** Multithreading access detector to validate accesses to the list of runtime smart object instances */
	UE_MT_DECLARE_RW_FULLY_RECURSIVE_ACCESS_DETECTOR(RuntimeInstanceListAccessDetector);

	/** Multithreading access detector to validate accesses to single smart object instance */
	UE_MT_DECLARE_RW_FULLY_RECURSIVE_ACCESS_DETECTOR(SingleRuntimeInstanceAccessDetector);

#if WITH_SMARTOBJECT_MT_INSTANCE_LOCK
	/** Critical section used to protect read/write operations on a smart object instance and its slots */
	mutable FCriticalSection RuntimeInstanceLock;
#endif // WITH_SMARTOBJECT_MT_INSTANCE_LOCK

	uint32 NextFreeUserID = 1;

	bool bRuntimeInitialized = false;
	
	/** Returns true if this subsystem is running on the server. */
	SMARTOBJECTSMODULE_API bool IsRunningOnServer() const;

#if WITH_EDITOR
	bool bAutoInitializeEditorInstances = true;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Set in OnWorldComponentsUpdated and used to control special logic required to build collections in Editor mode */
	bool bIsPartitionedWorld = false;

	friend class ASmartObjectPersistentCollection;

	SMARTOBJECTSMODULE_API void PopulateCollection(ASmartObjectPersistentCollection& InCollection) const;

	/** Iteratively adds items to registered collections. Expected to be called in World Partitined worlds. */
	SMARTOBJECTSMODULE_API void IterativelyBuildCollections();

	SMARTOBJECTSMODULE_API int32 GetRegisteredSmartObjectsCompatibleWithCollection(const ASmartObjectPersistentCollection& InCollection, TArray<USmartObjectComponent*>& OutRelevantComponents) const;

	/**
	 * Compute bounds from the given world 
	 * @param World World from which the bounds must be computed
	 */
	SMARTOBJECTSMODULE_API FBox ComputeBounds(const UWorld& World) const;
#endif // WITH_EDITORONLY_DATA

#if WITH_SMARTOBJECT_DEBUG
public:
	uint32 DebugGetNumRuntimeObjects() const
	{
		return RuntimeSmartObjects.Num();
	}

	const TMap<FSmartObjectHandle, FSmartObjectRuntime>& DebugGetRuntimeObjects() const
	{
		return RuntimeSmartObjects;
	}

	uint32 DebugGetNumRegisteredComponents() const
	{
		return RegisteredSOComponents.Num();
	}

	/** Debugging helper to remove all registered smart objects from the simulation */
	SMARTOBJECTSMODULE_API void DebugUnregisterAllSmartObjects();

	/** Debugging helpers to add all registered smart objects to the simulation */
	SMARTOBJECTSMODULE_API void DebugRegisterAllSmartObjects();

	/** Debugging helper to emulate the start of the simulation to create all runtime data */
	SMARTOBJECTSMODULE_API void DebugInitializeRuntime();

	/** Debugging helper to emulate the stop of the simulation to destroy all runtime data */
	SMARTOBJECTSMODULE_API void DebugCleanupRuntime();
#endif // WITH_SMARTOBJECT_DEBUG

	/** DEPRECATED BLOCK BEGIN */
public:

	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Find Smart Objects (Pure)", DeprecatedFunction, DeprecationMessage="The pure version is deprecated, place a new Find Smart Objects node and connect the exec pin"))
	bool FindSmartObjects(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr) const
	{
		return FindSmartObjects(Request, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}

	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DeprecatedFunction, DeprecationMessage = "Use MarkSmartObjectSlotAsFree instead."))
	bool Release(const FSmartObjectClaimHandle& ClaimHandle)
	{
		return MarkSlotAsFree(ClaimHandle);
	}

	/** DEPRECATED BLOCK END */
};

template<>
struct TMassExternalSubsystemTraits<USmartObjectSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = true
	};
};

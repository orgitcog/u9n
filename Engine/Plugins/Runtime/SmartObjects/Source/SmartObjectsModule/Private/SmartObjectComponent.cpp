// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectComponent.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"
#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif
#include "UObject/UObjectThreadContext.h"
#include "WorldPartition/ActorInstanceGuids.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectComponent)

#if WITH_EDITORONLY_DATA
USmartObjectComponent::FOnSmartObjectComponentChanged USmartObjectComponent::OnSmartObjectComponentChanged;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
USmartObjectComponent::FOnSmartObjectChanged USmartObjectComponent::OnSmartObjectChanged;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

USmartObjectComponent::USmartObjectComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void USmartObjectComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// Required to allow for sub classes to replicate the state of this smart object.
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DISABLE_REPLICATED_PROPERTY(USmartObjectComponent, DefinitionRef);
	DISABLE_REPLICATED_PROPERTY(USmartObjectComponent, RegisteredHandle);
}

void USmartObjectComponent::ValidateGUID()
{
	if (!ComponentGuid.IsValid())
	{
		UpdateGUID();
	}
	else
	{
		UE_SUPPRESS(LogSmartObject, Verbose,
		{
			if (const AActor* Owner = GetOwner())
			{
				const FGuid OwnerGuid = FActorInstanceGuid::GetActorInstanceGuid(*Owner);
				UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Valid Guid:    A:%s + C:%s = %s (%s)")
					, *OwnerGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
					, *ComponentGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
					, *FGuid::Combine(ComponentGuid, OwnerGuid).ToString(EGuidFormats::DigitsWithHyphensInBraces)
					, *FPackageName::ObjectPathToSubObjectPath(GetPathName())
				);
			}
		})
	}
}

void USmartObjectComponent::UpdateGUID()
{
#if WITH_EDITOR
	// This case covers old components that were never saved with a Guid
	// and is required for deterministic cooking
	if (IsRunningCookCommandlet())
	{
		ComponentGuid = FGuid::NewDeterministicGuid(GetFullName());
	}
	else
#endif
	{
		ComponentGuid = FGuid::NewGuid();
	}

	UE_SUPPRESS(LogSmartObject, Verbose,
		{
			if (const AActor * Owner = GetOwner())
			{
				const FGuid OwnerGuid = FActorInstanceGuid::GetActorInstanceGuid(*GetOwner());
				UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Updating Guid: A:%s + C:%s = %s (%s)")
					, *OwnerGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
					, *ComponentGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
					, *FGuid::Combine(ComponentGuid, OwnerGuid).ToString(EGuidFormats::DigitsWithHyphensInBraces)
					, *FPackageName::ObjectPathToSubObjectPath(GetPathName()));
			}
		})
}

void USmartObjectComponent::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		AActor* Actor = GetOwner();
		if (Actor != nullptr && Actor->HasAnyFlags(RF_ClassDefaultObject) == false)
		{
			// tagging owner actors since the tags get included in FWorldPartitionActorDesc 
			// and that's the only way we can tell a given actor has a SmartObjectComponent 
			// until it's fully loaded
			if (Actor->Tags.Contains(UE::SmartObject::WithSmartObjectTag) == false)
			{
				Actor->Tags.AddUnique(UE::SmartObject::WithSmartObjectTag);
				Actor->MarkPackageDirty();
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
bool USmartObjectComponent::ApplyDeprecation()
{
	if (bDeprecationApplied)
	{
		return false;
	}

	// Older versions of the SmartObject Component used to have property `DefinitionAsset`.
	// which referenced the SmartObject Definition asset. The data is now stored in DefinitionRef.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (DefinitionAsset_DEPRECATED)
	{
		DefinitionRef.SetSmartObjectDefinition(DefinitionAsset_DEPRECATED);
	}
	CachedDefinitionAssetVariation = nullptr;
	DefinitionAsset_DEPRECATED = nullptr;
	bDeprecationApplied = true;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	return true;
}

bool USmartObjectComponent::ApplyParentDeprecation()
{
	if (bDeprecationApplied)
	{
		return false;
	}

	if (USmartObjectComponent* Archetype = Cast<USmartObjectComponent>(GetArchetype()))
	{
		// If our archetype was already deprecated it indicates that the current instance
		// was created from an up to date archetype so no need to deprecate those values
		// and we consider the deprecation applied
		if (const bool bArchetypeAlreadyDeprecated = !Archetype->ApplyParentDeprecation())
		{
			bDeprecationApplied = true;
			return false;
		}
	}

	return ApplyDeprecation();
}
#endif

void USmartObjectComponent::Serialize(FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading())
	{
		// Keep track of deprecated definition before serialization in case we had a different asset
		// then we'll need to deprecate it
		const TObjectPtr<USmartObjectDefinition> AssetBeforeSerialization = DefinitionAsset_DEPRECATED;

		// CDOs don't run serialize, apply deprecation if needed
		ApplyParentDeprecation();

		Super::Serialize(Ar);

		// Object had its own asset, deprecate it
		if (DefinitionAsset_DEPRECATED != AssetBeforeSerialization)
		{
			// Reset deprecation that might have been set before serializing
			bDeprecationApplied = false;
			ApplyDeprecation();
		}
	}
	else
#endif
	{
		Super::Serialize(Ar);
	}
}

void USmartObjectComponent::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		UpdateGUID();
	}
}

void USmartObjectComponent::OnRegister()
{
	Super::OnRegister();

	ValidateGUID();

#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		// Component gets registered on BeginPlay for game worlds
		RegisterToSubsystem();

		// For non-game world in Editor we monitor saved definition,
		// so we can clear our cached variation when the based definition is saved.
		// This way we don't stick with the old base definition.
		OnSavingDefinitionDelegateHandle = UE::SmartObject::Delegates::OnSavingDefinition.AddLambda([this](const USmartObjectDefinition& Definition)
		{
			if (CachedDefinitionAssetVariation
				&& GetBaseDefinition() == &Definition)
			{
				CachedDefinitionAssetVariation = nullptr;
			}
		});
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void USmartObjectComponent::OnUnregister()
{
	if (OnSavingDefinitionDelegateHandle.IsValid())
	{
		UE::SmartObject::Delegates::OnSavingDefinition.Remove(OnSavingDefinitionDelegateHandle);
	}

	// Component gets unregistered on EndPlay for game worlds
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld() == false)
	{
		UnregisterFromSubsystem(ESmartObjectUnregistrationType::RegularProcess);
	}

	Super::OnUnregister();
}

void USmartObjectComponent::PostEditImport()
{
	Super::PostEditImport();

	UpdateGUID();
}
#endif // WITH_EDITOR

void USmartObjectComponent::RegisterToSubsystem()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

#if WITH_EDITOR
	// Do not process any component registered to preview world
	if (World->WorldType == EWorldType::EditorPreview)
	{
		return;
	}
#endif // WITH_EDITOR

	if (GetOwnerRole() == ROLE_Authority)
	{
		// Note: we don't report error or ensure on missing subsystem since it might happen
		// in various scenarios (e.g. inactive world)
		if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
		{
			Subsystem->RegisterSmartObject(this);
		}
	}
}

void USmartObjectComponent::UnregisterFromSubsystem(const ESmartObjectUnregistrationType UnregistrationType)
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

#if WITH_EDITOR
	// Do not process any component registered to preview world
	if (World->WorldType == EWorldType::EditorPreview)
	{
		return;
	}
#endif // WITH_EDITOR

	// Only attempt to unregister if we are the authoritative role
	if (GetRegisteredHandle().IsValid() && GetOwnerRole() == ENetRole::ROLE_Authority)
	{
		if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
		{
			if (UnregistrationType == ESmartObjectUnregistrationType::ForceRemove
				|| (!World->IsGameWorld() && (IsBeingDestroyed() || (GetOwner() && GetOwner()->IsActorBeingDestroyed()))))
			{
				// note that this case is really only expected in the editor when the component is being unregistered 
				// as part of DestroyComponent (or from its owner destruction).
				Subsystem->RemoveSmartObject(this);
			}
			else
			{
				Subsystem->UnregisterSmartObject(this);
			}
		}
	}
}

void USmartObjectComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register only for game worlds only since component is registered by OnRegister for the other scenarios.
	// Can't enforce a check here in case BeginPlay is manually dispatched on worlds of other type (e.g. Editor, Preview).
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld())
	{
		RegisterToSubsystem();
	}
}

void USmartObjectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister only for game worlds (see details in BeginPlay)
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld())
	{
		// When the object gets destroyed or streamed out we unregister the component according to its registration type
		// to preserve runtime data for components bounds to existing objects.
		if (EndPlayReason == EEndPlayReason::RemovedFromWorld
			|| EndPlayReason == EEndPlayReason::Destroyed)
		{
			UnregisterFromSubsystem(ESmartObjectUnregistrationType::RegularProcess);
		}
		// In all other scenarios (e.g. LevelTransition, EndPIE, Quit, etc.) we always remove the runtime data
		else
		{
			UnregisterFromSubsystem(ESmartObjectUnregistrationType::ForceRemove);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FBox USmartObjectComponent::GetSmartObjectBounds() const
{
	FBox BoundingBox(ForceInitToZero);

	if (const AActor* Owner = GetOwner())
	{
		if (const USmartObjectDefinition* Definition = GetDefinition())
		{
			BoundingBox = Definition->GetBounds().TransformBy(Owner->GetTransform());
		}
	}

	return BoundingBox;
}

const USmartObjectDefinition* USmartObjectComponent::GetDefinition() const
{
	if (!CachedDefinitionAssetVariation)
	{
		ensureMsgf(!FUObjectThreadContext::Get().IsRoutingPostLoad
			, TEXT("%hs can't be called from PostLoad since the required level's owning world is not set yet."
			" Consider moving the function call to OnRegister or BeginPlay."), __FUNCTION__);
		CachedDefinitionAssetVariation = DefinitionRef.GetAssetVariation(GetWorld());
	}
	
	return CachedDefinitionAssetVariation;
}

const USmartObjectDefinition* USmartObjectComponent::GetBaseDefinition() const
{
	return DefinitionRef.GetSmartObjectDefinition();
}

void USmartObjectComponent::SetDefinition(USmartObjectDefinition* Definition)
{
	if (IsBoundToSimulation())
	{
		UE_LOG(LogSmartObject, Warning,
			TEXT("Changing Definition is not supported when the component is registered to the simulation."
				" Call UnregisterSmartObject before, set the definition, then register again to update the runtime instance with the new definition."));
		return;
	}

	DefinitionRef.SetSmartObjectDefinition(Definition);

	// Reset cache so it will get updated next time GetDefinition() gets called.
	CachedDefinitionAssetVariation = nullptr;
}

void USmartObjectComponent::SetRegisteredHandle(const FSmartObjectHandle Value, const ESmartObjectRegistrationType InRegistrationType)
{
	ensure(Value.IsValid());
	ensure(RegisteredHandle.IsValid() == false || RegisteredHandle == Value);
	RegisteredHandle = Value;
	ensure(RegistrationType == ESmartObjectRegistrationType::NotRegistered && InRegistrationType != ESmartObjectRegistrationType::NotRegistered);
	RegistrationType = InRegistrationType;
}

void USmartObjectComponent::InvalidateRegisteredHandle()
{
	RegisteredHandle = FSmartObjectHandle::Invalid;
	RegistrationType = ESmartObjectRegistrationType::NotRegistered;
}

void USmartObjectComponent::OnRuntimeInstanceBound(FSmartObjectRuntime& RuntimeInstance)
{
	checkf(!RuntimeInstance.GetMutableEventDelegate().IsBoundToObject(this), TEXT("Component and runtime instance should be bound only once."));
	EventDelegateHandle = RuntimeInstance.GetMutableEventDelegate().AddUObject(this, &USmartObjectComponent::OnRuntimeEventReceived);
}

void USmartObjectComponent::OnRuntimeInstanceUnbound(FSmartObjectRuntime& RuntimeInstance)
{
	if (EventDelegateHandle.IsValid())
	{
		RuntimeInstance.GetMutableEventDelegate().Remove(EventDelegateHandle);
		EventDelegateHandle.Reset();
	}
}

bool USmartObjectComponent::SetSmartObjectEnabled(const bool bEnable) const
{
	return SetSmartObjectEnabledForReason(UE::SmartObject::EnabledReason::Gameplay, bEnable);
}

bool USmartObjectComponent::SetSmartObjectEnabledForReason(const FGameplayTag ReasonTag, const bool bEnabled) const
{
	if (GetRegisteredHandle().IsValid())
	{
		if (USmartObjectSubsystem* const Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			Subsystem->SetEnabledForReason(GetRegisteredHandle(), ReasonTag, bEnabled);

			return true;
		}
	}
	
	return false;
}

bool USmartObjectComponent::IsSmartObjectEnabled() const
{
	if (GetRegisteredHandle().IsValid())
	{
		if (const USmartObjectSubsystem* const Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			return Subsystem->IsEnabled(GetRegisteredHandle());
		}
	}

	return false;
}

bool USmartObjectComponent::IsSmartObjectEnabledForReason(const FGameplayTag ReasonTag) const
{
	if (GetRegisteredHandle().IsValid())
	{
		if (const USmartObjectSubsystem* const Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			return Subsystem->IsEnabledForReason(GetRegisteredHandle(), ReasonTag);
		}
	}

	return false;
}

TStructOnScope<FActorComponentInstanceData> USmartObjectComponent::GetComponentInstanceData() const
{
	return MakeStructOnScope<FActorComponentInstanceData, FSmartObjectComponentInstanceData>(this);
}

#if WITH_EDITOR
void USmartObjectComponent::PostEditUndo()
{
	Super::PostEditUndo();
	CachedDefinitionAssetVariation = nullptr;

	OnSmartObjectComponentChanged.Broadcast(this);
}

void USmartObjectComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CachedDefinitionAssetVariation = nullptr;

	OnSmartObjectComponentChanged.Broadcast(this);
}

void USmartObjectComponent::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	if (!IsTemplate())
	{
		// Make sure all saved components have a valid Guid.
		ValidateGUID();

		// In cooked build the ActorGuid is not available after component registration
		// so we combine them to store the final one that will be used directly by CreateHandleForComponent.
		if (SaveContext.IsCooking() && GetCanBePartOfCollection())
		{
			ComponentGuid = FSmartObjectHandleFactory::CreateHandleGuidFromComponent(this);
		}
	}
}
#endif // WITH_EDITOR

//-----------------------------------------------------------------------------
// FSmartObjectComponentInstanceData
//-----------------------------------------------------------------------------
bool FSmartObjectComponentInstanceData::ContainsData() const
{
	return true;
}

void FSmartObjectComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	// Apply data first since we might need to register to the subsystem
	// before the component gets re-registered by the base class.
	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
	{
		USmartObjectComponent* SmartObjectComponent = CastChecked<USmartObjectComponent>(Component);

		// Clear cache to make sure we get an updated variation in case BP modified some parameters.
		SmartObjectComponent->CachedDefinitionAssetVariation = nullptr;

		// We are about to change our Guid so we need to unregister from the subsystem first
		if (SmartObjectComponent->IsRegistered())
		{
			SmartObjectComponent->UnregisterFromSubsystem(ESmartObjectUnregistrationType::ForceRemove);
		}

		SmartObjectComponent->ComponentGuid = OriginalGuid;
		SmartObjectComponent->DefinitionRef = SmartObjectDefinitionRef;

		// Registering to the subsystem should only be attempted on registered component
		// otherwise the OnRegister callback will take care of it.
		if (SmartObjectComponent->IsRegistered())
		{
			SmartObjectComponent->RegisterToSubsystem();
		}
	}

	Super::ApplyToComponent(Component, CacheApplyPhase);
}

void USmartObjectComponent::OnRuntimeEventReceived(const FSmartObjectEventData& Event)
{
	const AActor* Interactor = nullptr;
	if (const FSmartObjectActorUserData* ActorUser = Event.EventPayload.GetPtr<const FSmartObjectActorUserData>())
	{
		Interactor = ActorUser->UserActor.Get();
	}
					
	UE_CVLOG_LOCATION(Interactor != nullptr, USmartObjectSubsystem::GetCurrent(GetWorld()), LogSmartObject, Display,
		Interactor->GetActorLocation(), /*Radius*/25, FColor::Green, TEXT("%s: %s. Interactor: %s"),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Event.Reason), *GetNameSafe(Interactor));

	ReceiveOnEvent(Event, Interactor);
	OnSmartObjectEvent.Broadcast(Event, Interactor);
	OnSmartObjectEventNative.Broadcast(Event, Interactor);
}
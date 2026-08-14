// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectTypes.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "GameplayTagsManager.h"
#include "LevelUtils.h"
#include "NavigationSystem.h"
#include "SmartObjectComponent.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "GameFramework/Actor.h"
#include "VisualLogger/VisualLogger.h"
#include "WorldPartition/ActorInstanceGuids.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectTypes)

DEFINE_LOG_CATEGORY(LogSmartObject);

const FSmartObjectUserHandle FSmartObjectUserHandle::Invalid;
const FSmartObjectHandle FSmartObjectHandle::Invalid;

namespace UE::SmartObject::EnabledReason
{

FGameplayTag Gameplay;

struct FNativeGameplayTags : FGameplayTagNativeAdder
{
	virtual ~FNativeGameplayTags() {}

	virtual void AddTags() override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		Gameplay = Manager.AddNativeGameplayTag(TEXT("SmartObject.EnabledReason.Gameplay"));
	}

	static const FNativeGameplayTags& Get()
	{
		return StaticInstance;
	}
	static FNativeGameplayTags StaticInstance;
};
FNativeGameplayTags FNativeGameplayTags::StaticInstance;

} // UE::SmartObject::EnabledReason

//----------------------------------------------------------------------//
// FSmartObjectUserCapsuleParams
//----------------------------------------------------------------------//

const FSmartObjectUserCapsuleParams FSmartObjectUserCapsuleParams::Invalid(0.f, 0.f, 0.f);

FSmartObjectAnnotationCollider FSmartObjectUserCapsuleParams::GetAsCollider(const FVector& Location, const FQuat& Rotation) const
{
	const float ConstrainedRadius = Radius;
	const float ConstrainedHeight = FMath::Max(ConstrainedRadius * 2.0f, Height);
	const float ConstrainedStepHeight = FMath::Min(StepHeight, ConstrainedHeight - (ConstrainedRadius * 2.0f));
	const float ConstrainedHalfHeight = (ConstrainedHeight - ConstrainedStepHeight) * 0.5f;
	const float ConstrainedCenter = ConstrainedStepHeight + ConstrainedHalfHeight;

	FSmartObjectAnnotationCollider Collider;
	Collider.Location = Location + Rotation.GetAxisZ() * ConstrainedCenter;
	Collider.Rotation = Rotation;
	Collider.CollisionShape = FCollisionShape::MakeCapsule(ConstrainedRadius, ConstrainedHalfHeight);

	return Collider;
}

//----------------------------------------------------------------------//
// FSmartObjectSlotValidationParams
//----------------------------------------------------------------------//
const FSmartObjectUserCapsuleParams& FSmartObjectSlotValidationParams::GetUserCapsule(const FSmartObjectUserCapsuleParams& NavigationCapsule) const
{
	if (bUseNavigationCapsuleSize)
	{
		return NavigationCapsule;
	}
	return UserCapsule;
}

bool FSmartObjectSlotValidationParams::GetUserCapsuleForActor(const AActor& UserActor, FSmartObjectUserCapsuleParams& OutCapsule) const
{
	if (bUseNavigationCapsuleSize)
	{
		const INavAgentInterface* NavAgent = Cast<INavAgentInterface>(&UserActor);
		if (!NavAgent)
		{
			return false;
		}
		
		const FNavAgentProperties& NavAgentProps = NavAgent->GetNavAgentPropertiesRef();
		if (NavAgentProps.AgentRadius < 0.0f
			|| NavAgentProps.AgentHeight < 0.0f)
		{
			return false;
		}
		
		OutCapsule.Radius = NavAgentProps.AgentRadius;
		OutCapsule.Height = NavAgentProps.AgentHeight;
		if (NavAgentProps.HasStepHeightOverride())
		{
			OutCapsule.StepHeight = NavAgentProps.AgentStepHeight;
		}
		else
		{
			// Get the default step height value from nav data.
			const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(UserActor.GetWorld());
			const ANavigationData* NavData = NavSys ? NavSys->GetNavDataForProps(NavAgentProps, NavAgent->GetNavAgentLocation()) : nullptr;
			if (!NavData)
			{
				return false;
			}
			const FNavDataConfig& Config = NavData->GetConfig();
			OutCapsule.StepHeight = FMath::Max(0.0f, Config.AgentStepHeight);
		}
	}
	else
	{
		OutCapsule = UserCapsule;
	}

	return true;
}

bool FSmartObjectSlotValidationParams::GetPreviewUserCapsule(const UWorld& World, FSmartObjectUserCapsuleParams& OutCapsule) const
{
	if (bUseNavigationCapsuleSize)
	{
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		if (!NavSys)
		{
			return false;
		}

		const TArray<FNavDataConfig>& SupportedAgents = NavSys->GetSupportedAgents();
		if (SupportedAgents.IsEmpty())
		{
			return false;
		}
		
		const FNavDataConfig& Config = SupportedAgents[0];
		OutCapsule.Radius = Config.AgentRadius;
		OutCapsule.Height = Config.AgentHeight;
		OutCapsule.StepHeight = Config.AgentStepHeight;
	}
	else
	{
		OutCapsule = UserCapsule;
	}

	return true;
}

//----------------------------------------------------------------------//
// FSmartObjectActorUserData
//----------------------------------------------------------------------//
FSmartObjectActorUserData::FSmartObjectActorUserData(const AActor* InUserActor)
	: UserActor(InUserActor)
{
}

//----------------------------------------------------------------------//
// FSmartObjectHandleFactory
//----------------------------------------------------------------------//
FGuid FSmartObjectHandleFactory::CreateHandleGuidFromComponent(TNotNull<const USmartObjectComponent*> Component)
{
	// When a component can't be part of a collection it indicates that we'll never need
	// to bind persistent data to this component at runtime. In this case we simply assign
	// a new Id used to bind it to its runtime entry during the component lifetime and
	// to unregister from the subsystem when it gets removed (e.g. streaming out, destroyed, etc.).
	if (Component->GetCanBePartOfCollection() == false)
	{
		return FGuid::NewGuid();
	}

	const FGuid ComponentGuid = Component->GetComponentGuid();
	ensureMsgf(ComponentGuid.IsValid(), TEXT("Component always need an ID at this point, please verify the current code path."));

#if WITH_EDITOR
	const AActor* Owner = Component->GetOwner();
	// Allow users to store Components in other types than actors.
	// In such case we consider that this particular component will never need a Guid.
	if (Owner == nullptr)
	{
		return FGuid();
	}

	const FGuid OwnerGuid = FActorInstanceGuid::GetActorInstanceGuid(*Owner);
	ensureMsgf(OwnerGuid.IsValid(), TEXT("Actor unique ID is expected to be accessible in Editor."));
	const FGuid HandleGuid = FGuid::Combine(ComponentGuid, OwnerGuid);

	UE_VLOG_UELOG(Component, LogSmartObject, Verbose, TEXT("Create Handle: A:%s + C:%s = %s (%s)")
		, *OwnerGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
		, *ComponentGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
		, *HandleGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
		, *FPackageName::ObjectPathToSubObjectPath(Component->GetPathName()));
#else
	// In non-editor builds the component Guid is already combined with the actor and ready to be used
	const FGuid HandleGuid = ComponentGuid;
	UE_VLOG_UELOG(Component, LogSmartObject, Verbose, TEXT("Create Handle: A + C = %s (%s)")
		, *HandleGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces)
		, *FPackageName::ObjectPathToSubObjectPath(Component->GetPathName()));
#endif // WITH_EDITOR

	return HandleGuid;
}

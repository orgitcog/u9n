// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameFramework/Actor.h"

#include "CustomizableInstanceLODManagement.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class UCustomizableObjectInstanceUsage;
typedef TMap<const class UCustomizableObjectInstance*, class FMutableUpdateCandidate> FMutableInstanceUpdateMap;

// This is an abstract base class, override it to create a MakeShared<FInstance> LOD management system and register with UCustomizableObjectSystem::SetInstanceLODManagement
UCLASS(MinimalAPI, Abstract, Blueprintable, BlueprintType)
class UCustomizableInstanceLODManagementBase : public UObject
{
	GENERATED_BODY()
public:
	UCustomizableInstanceLODManagementBase() : UObject() {};
	virtual ~UCustomizableInstanceLODManagementBase() {};

	// WARNING! The following methods must be overriden in derived classes
	UE_API virtual void UpdateInstanceDistsAndLODs(FMutableInstanceUpdateMap& InOutRequestedUpdates) PURE_VIRTUAL(UCustomizableInstanceLODManagementBase::UpdateInstanceDistsAndLODs, return;);
	UE_API virtual int32 GetNumGeneratedInstancesLimitFullLODs() const PURE_VIRTUAL(UCustomizableInstanceLODManagementBase::GetNumGeneratedInstancesLimitFullLODs, return 0;);
	UE_API virtual int32 GetNumGeneratedInstancesLimitLOD1() const PURE_VIRTUAL(UCustomizableInstanceLODManagementBase::GetNumGeneratedInstancesLimitLOD1, return 0;);
	UE_API virtual int32 GetNumGeneratedInstancesLimitLOD2() const PURE_VIRTUAL(UCustomizableInstanceLODManagementBase::GetNumGeneratedInstancesLimitLOD2, return 0;);
	UE_API virtual float GetOnlyUpdateCloseCustomizableObjectsDist() const PURE_VIRTUAL(UCustomizableInstanceLODManagementBase::GetOnlyUpdateCloseCustomizableObjectsDist, return 0.f;);
	UE_API virtual bool IsOnlyUpdateCloseCustomizableObjectsEnabled() const PURE_VIRTUAL(UCustomizableInstanceLODManagementBase::IsOnlyUpdateCloseCustomizableObjectsEnabled, return false;);
	UE_API virtual bool IsOnlyGenerateRequestedLODLevelsEnabled() const PURE_VIRTUAL(UCustomizableInstanceLODManagementBase::IsOnlyGenerateRequestedLODLevelsEnabled, return false;);

#if WITH_EDITOR
	UE_API void EditorUpdateComponent(UCustomizableObjectInstanceUsage* InstanceUsage);
#endif
};


UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UCustomizableInstanceLODManagement : public UCustomizableInstanceLODManagementBase
{
	GENERATED_BODY()
public:
	UE_API UCustomizableInstanceLODManagement();
	UE_API virtual ~UCustomizableInstanceLODManagement();

	UE_API virtual void UpdateInstanceDistsAndLODs(FMutableInstanceUpdateMap& InOutRequestedUpdates) override;

	UE_API virtual int32 GetNumGeneratedInstancesLimitFullLODs() const override;
	UE_API virtual int32 GetNumGeneratedInstancesLimitLOD1() const override;
	UE_API virtual int32 GetNumGeneratedInstancesLimitLOD2() const override;

	// Used to define the actors that will be considered the centers of LOD distance calculations. If left empty it will revert to the default, the first player controller actor
	void AddViewCenter(const AActor* const InCentralActor) { ViewCenters.Add(TWeakObjectPtr<const AActor>(InCentralActor)); };
	void RemoveViewCenter(const AActor* const InCentralActor) { ViewCenters.Remove(TWeakObjectPtr<const AActor>(InCentralActor)); };
	void ClearViewCenters() { ViewCenters.Empty(); }

	// Sets how many of the nearest instances to the player will have updates with priority over LOD changes, only works if LODs are enabled with b.NumGeneratedInstancesLimitLODs
	UE_API void SetNumberOfPriorityUpdateInstances(int32 NumPriorityUpdateInstances);
	UE_API int32 GetNumberOfPriorityUpdateInstances() const;

	UE_API virtual void SetCustomizableObjectsUpdateDistance(float Distance);
	UE_API virtual float GetOnlyUpdateCloseCustomizableObjectsDist() const override;

	UE_API virtual bool IsOnlyUpdateCloseCustomizableObjectsEnabled() const override;
	virtual bool IsOnlyGenerateRequestedLODLevelsEnabled() const override { return true; };

private:
	// The list of actors that define a view radius (CloseDist) around them used for LOD management
	TSet<TWeakObjectPtr<const AActor>> ViewCenters;
	
	// Sets how many of the nearest instances to the player will have updates with priority over LOD changes
	int32 NumPriorityUpdateInstances = 1;

	float CloseCustomizableObjectsDist;	
};

#undef UE_API

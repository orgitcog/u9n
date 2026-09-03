// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableInstanceLODManagement.h"

#include "MuCO/CustomizableObjectInstanceUsagePrivate.h"
#include "Camera/CameraActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectPrivate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableInstanceLODManagement)

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

static TAutoConsoleVariable<int32> CVarNumGeneratedInstancesLimit(
	TEXT("b.NumGeneratedInstancesLimit"),
	0,
	TEXT("If different than 0, limit the number of mutable instances with full LODs to have at a given time."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarNumGeneratedInstancesLimitLOD1(
	TEXT("b.NumGeneratedInstancesLimitLOD1"),
	0,
	TEXT("If different than 0, limit the number of mutable instances with LOD 1 to have at a given time."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarNumGeneratedInstancesLimitLOD2(
	TEXT("b.NumGeneratedInstancesLimitLOD2"),
	0,
	TEXT("If different than 0, limit the number of mutable instances with LOD 2 to have at a given time."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarDistanceForFixedLOD2(
	TEXT("b.DistanceForFixedLOD2"),
	50000,
	TEXT("If NumGeneratedInstancesLimit is different than 0, sets the distance at which the system will fix the LOD of an instance to the lowest res one (LOD2) to prevent unnecessary LOD changes and memory consumption"),
	ECVF_Scalability);

static TAutoConsoleVariable<bool> CVarOnlyUpdateCloseCustomizableObjects(
	TEXT("b.OnlyUpdateCloseCustomizableObjects"),
	false,
	TEXT("If true, only CustomizableObjects within a predefined distance to the view centers will be generated"),
	ECVF_Scalability);


#if WITH_EDITOR
void UCustomizableInstanceLODManagementBase::EditorUpdateComponent(UCustomizableObjectInstanceUsage* InstanceUsage)
{
}
#endif


UCustomizableInstanceLODManagement::UCustomizableInstanceLODManagement() : UCustomizableInstanceLODManagementBase()
{
	CloseCustomizableObjectsDist = 2000.f;
}


UCustomizableInstanceLODManagement::~UCustomizableInstanceLODManagement()
{

}

// Used to manually update distances used in "OnlyUpdateCloseCustomizableObjects" system. If OnlyForInstance is null, all instances have their distance updated
// ViewCenter is the origin where the distances will be measured from.
void UpdatePawnToInstancesDistances(const class UCustomizableObjectInstance* OnlyForInstance, const TWeakObjectPtr<const AActor> ViewCenter)
{
	for (TObjectIterator<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage; CustomizableObjectInstanceUsage; ++CustomizableObjectInstanceUsage)
	{
#if WITH_EDITOR
		if (IsValid(*CustomizableObjectInstanceUsage) && CustomizableObjectInstanceUsage->GetPrivate()->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}
#endif

		if (IsValid(*CustomizableObjectInstanceUsage) && (OnlyForInstance == nullptr || CustomizableObjectInstanceUsage->GetCustomizableObjectInstance() == OnlyForInstance))
		{
			CustomizableObjectInstanceUsage->GetPrivate()->UpdateDistFromComponentToPlayer(ViewCenter.IsValid() ? ViewCenter.Get() : nullptr, OnlyForInstance != nullptr);
		}
	}
}

#if WITH_EDITOR
// Used to manually update instances in the level editor
void UpdateCameraToInstancesDistance(const FVector CameraPosition)
{
	for (TObjectIterator<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage; CustomizableObjectInstanceUsage; ++CustomizableObjectInstanceUsage)
	{
#if WITH_EDITOR
		if (IsValid(*CustomizableObjectInstanceUsage) && CustomizableObjectInstanceUsage->GetPrivate()->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}
#endif
		if (IsValid(*CustomizableObjectInstanceUsage) && !CustomizableObjectInstanceUsage->IsTemplate())
		{
			CustomizableObjectInstanceUsage->GetPrivate()->UpdateDistFromComponentToLevelEditorCamera(CameraPosition);
		}
	}
}
#endif


void UCustomizableInstanceLODManagement::UpdateInstanceDistsAndLODs(FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	int32 NumGeneratedInstancesLimitLODs = GetNumGeneratedInstancesLimitFullLODs();
	if (NumGeneratedInstancesLimitLODs > 0 || (IsOnlyUpdateCloseCustomizableObjectsEnabled() && IsOnlyGenerateRequestedLODLevelsEnabled()))
	{
		UCustomizableObjectSystem* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance();

		if (ViewCenters.Num() == 0) // Just use the first pawn
		{
			UCustomizableObjectInstanceUsage* FirstCustomizableObjectInstanceUsage = nullptr;

	#if WITH_EDITOR
			bool bLevelEditorInstancesUpdated = false;
	#endif

			for (TObjectIterator<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage; CustomizableObjectInstanceUsage; ++CustomizableObjectInstanceUsage)
			{
#if WITH_EDITOR
				if (IsValid(*CustomizableObjectInstanceUsage) && CustomizableObjectInstanceUsage->GetPrivate()->IsNetMode(NM_DedicatedServer))
				{
					continue;
				}
#endif
				if (!IsValid(*CustomizableObjectInstanceUsage))
				{
					continue;
				}

				USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(CustomizableObjectInstanceUsage->GetAttachParent());

				if (!CustomizableObjectInstanceUsage->IsTemplate())
				{
					UWorld* LocalWorld = Parent ? Parent->GetWorld() : nullptr;
					APlayerController* Controller = LocalWorld ? LocalWorld->GetFirstPlayerController() : nullptr;
					TWeakObjectPtr<const AActor> ViewCenter = Controller ? TWeakObjectPtr<const AActor>(Controller->GetPawn()) : nullptr;

					if (ViewCenter.IsValid())
					{
						UpdatePawnToInstancesDistances(nullptr, ViewCenter);
						break;
					}

	#if WITH_EDITOR
					else if (Parent && Parent->GetWorld())
					{
						EWorldType::Type WorldType = Parent->GetWorld()->WorldType;
					
						// Level Editor Instances (non PIE)
						if (!bLevelEditorInstancesUpdated && WorldType == EWorldType::Editor)
						{
							for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
							{
								if (LevelVC && LevelVC->IsPerspective())
								{
									UpdateCameraToInstancesDistance(LevelVC->GetViewLocation());
									bLevelEditorInstancesUpdated = true;
									break;
								}
							}
						}
					}
	#endif // WITH_EDITOR

				}
			}
		}
		else
		{
			for (const TWeakObjectPtr<const AActor> ViewCenter : ViewCenters)
			{
				if (ViewCenter.IsValid())
				{
					UpdatePawnToInstancesDistances(nullptr, ViewCenter);
				}
			}
		}
	}

	if (NumGeneratedInstancesLimitLODs > 0)
	{
		int32 NumGeneratedInstancesLimitLOD1 = GetNumGeneratedInstancesLimitLOD1();
		int32 NumGeneratedInstancesLimitLOD2 = GetNumGeneratedInstancesLimitLOD2();

		TArray<UCustomizableObjectInstance*> SortedInstances;

		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValid(*CustomizableObjectInstance) &&
				CustomizableObjectInstance->GetPrivate() &&
				CustomizableObjectInstance->GetIsBeingUsedByComponentInPlay())
			{
				if (const UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
					CustomizableObject &&
					!CustomizableObject->GetPrivate()->IsLocked())
				{
					UCustomizableObjectInstance* Ptr = *CustomizableObjectInstance;
					Ptr->SetIsDiscardedBecauseOfTooManyInstances(false);
					SortedInstances.Add(Ptr);
				}
			}
		}

		for (int32 i = 0; i < GetNumberOfPriorityUpdateInstances() && i < SortedInstances.Num(); ++i)
		{
			SortedInstances[i]->SetIsPlayerOrNearIt(true);
		}

		TMap<FName, uint8> MinLODs;
		TMap<FName, uint8> RequestedLODs;
		for (int32 i = NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1 + NumGeneratedInstancesLimitLOD2; i < SortedInstances.Num(); ++i)
		{
			SortedInstances[i]->SetIsDiscardedBecauseOfTooManyInstances(true);
		}

		if (SortedInstances.Num() > NumGeneratedInstancesLimitLODs)
		{
			SortedInstances.Sort([](UCustomizableObjectInstance& A, UCustomizableObjectInstance& B)
			{
				return A.GetMinSquareDistToPlayer() < B.GetMinSquareDistToPlayer();
			});

			bool bAlreadyReachedFixedLOD = false;
			const float DistanceForFixedLOD = float(CVarDistanceForFixedLOD2.GetValueOnGameThread());
			const float DistanceForFixedLODSquared = FMath::Square(DistanceForFixedLOD);


			for (int32 i = 0; i < NumGeneratedInstancesLimitLODs && i < SortedInstances.Num(); ++i)
			{
				const UCustomizableObject* CO = SortedInstances[i]->GetCustomizableObject();
				
				if (!bAlreadyReachedFixedLOD && SortedInstances[i]->GetMinSquareDistToPlayer() < DistanceForFixedLODSquared)
				{
					for (int32 ComponentIndex = 0; ComponentIndex < CO->GetComponentCount(); ++ComponentIndex)
					{
						MinLODs.Add(CO->GetComponentName(ComponentIndex), 0);
						RequestedLODs.Add(CO->GetComponentName(ComponentIndex), 0);
					}

					SortedInstances[i]->SetRequestedLODs(MinLODs, RequestedLODs, InOutRequestedUpdates);
				}
				else
				{
					for (int32 ComponentIndex = 0; ComponentIndex < CO->GetComponentCount(); ++ComponentIndex)
					{
						MinLODs.Add(CO->GetComponentName(ComponentIndex), UINT8_MAX);
						RequestedLODs.Add(CO->GetComponentName(ComponentIndex), 0);
					}

					SortedInstances[i]->SetRequestedLODs(MinLODs, RequestedLODs, InOutRequestedUpdates);
					bAlreadyReachedFixedLOD = true;
				}
			}

			for (int32 i = NumGeneratedInstancesLimitLODs; i < NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1 && i < SortedInstances.Num(); ++i)
			{
				const UCustomizableObject* CO = SortedInstances[i]->GetCustomizableObject();

				if (!bAlreadyReachedFixedLOD && SortedInstances[i]->GetMinSquareDistToPlayer() < DistanceForFixedLODSquared)
				{
					for (int32 ComponentIndex = 0; ComponentIndex < CO->GetComponentCount(); ++ComponentIndex)
					{
						MinLODs.Add(CO->GetComponentName(ComponentIndex), 1);
						RequestedLODs.Add(CO->GetComponentName(ComponentIndex), 0);
					}
					
					SortedInstances[i]->SetRequestedLODs(MinLODs, RequestedLODs, InOutRequestedUpdates);
				}
				else
				{
					for (int32 ComponentIndex = 0; ComponentIndex < CO->GetComponentCount(); ++ComponentIndex)
					{
						MinLODs.Add(CO->GetComponentName(ComponentIndex), UINT8_MAX);
						RequestedLODs.Add(CO->GetComponentName(ComponentIndex), 0);
					}
					
					SortedInstances[i]->SetRequestedLODs(MinLODs, RequestedLODs, InOutRequestedUpdates);
					bAlreadyReachedFixedLOD = true;
				}
			}

			for (int32 i = NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1; i < NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1 + NumGeneratedInstancesLimitLOD2 && i < SortedInstances.Num(); ++i)
			{
				const UCustomizableObject* CO = SortedInstances[i]->GetCustomizableObject();
				for (int32 ComponentIndex = 0; ComponentIndex < CO->GetComponentCount(); ++ComponentIndex)
				{
					MinLODs.Add(CO->GetComponentName(ComponentIndex), 2);
					RequestedLODs.Add(CO->GetComponentName(ComponentIndex), 0);
				}

				SortedInstances[i]->SetRequestedLODs(MinLODs, RequestedLODs, InOutRequestedUpdates);
			}

			for (int32 i = NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1 + NumGeneratedInstancesLimitLOD2; i < SortedInstances.Num(); ++i)
			{
				SortedInstances[i]->SetIsDiscardedBecauseOfTooManyInstances(true);
			}
		}
		else
		{
			// No limit surpassed, set all instances to have all LODs, there will be an UpdateSkeletalMesh only if there's a change in LOD state
			for (int32 i = 0; i < SortedInstances.Num(); ++i)
			{
				const UCustomizableObject* CO = SortedInstances[i]->GetCustomizableObject();
				for (int32 ComponentIndex = 0; ComponentIndex < CO->GetComponentCount(); ++ComponentIndex)
				{
					MinLODs.Add(CO->GetComponentName(ComponentIndex), 2);
					RequestedLODs.Add(CO->GetComponentName(ComponentIndex), 0);
				}
				
				SortedInstances[i]->SetRequestedLODs(MinLODs, RequestedLODs, InOutRequestedUpdates);
			}
		}
	}
	else if (IsOnlyGenerateRequestedLODLevelsEnabled())
	{
		struct FLODTracker
		{
			TMap<FName, uint8> MinLOD;

			bool bInitialized = false;
			TMap<FName, uint8> RequestedLODPerComponent;
		};

		TMap<TObjectPtr<UCustomizableObjectInstance>, FLODTracker> InstancesMinLOD;
		InstancesMinLOD.Reserve(100);

		for (TObjectIterator<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage; CustomizableObjectInstanceUsage; ++CustomizableObjectInstanceUsage)
		{
#if WITH_EDITOR
			if (IsValid(*CustomizableObjectInstanceUsage) && CustomizableObjectInstanceUsage->GetPrivate()->IsNetMode(NM_DedicatedServer))
			{
				continue;
			}
#endif

			if (IsValid(*CustomizableObjectInstanceUsage) && !CustomizableObjectInstanceUsage->IsTemplate())
			{
				UCustomizableObjectInstance* COI = CustomizableObjectInstanceUsage->GetCustomizableObjectInstance();
				if (!COI || !COI->GetCustomizableObject())
				{
					continue;
				}

				USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(CustomizableObjectInstanceUsage->GetAttachParent());

#if WITH_EDITOR

				EWorldType::Type WorldType = EWorldType::Type::None;

				UWorld* World = Parent ? Parent->GetWorld() : nullptr;
				if (World)
				{
					WorldType = World->WorldType;
				}

				// Blueprint instances and CO Editors
				
				const USceneComponent* const AttachParentComponent = CustomizableObjectInstanceUsage->GetAttachParent();
				bool bAttachParentActor = AttachParentComponent ? AttachParentComponent->GetOwner()!=nullptr : false;

				if (WorldType == EWorldType::EditorPreview || (!World && !bAttachParentActor))
				{
					continue;
				}

				// Skip if world type is EditorPreview, GamePreview, etc...
				if (WorldType == EWorldType::GamePreview || WorldType == EWorldType::GameRPC || WorldType == EWorldType::Inactive)
				{
					continue;
				}
#endif // WITH_EDITOR

				FLODTracker& LODTracker = InstancesMinLOD.FindOrAdd(COI);

				if (!LODTracker.bInitialized)
				{
					const UCustomizableObject* CO = COI->GetCustomizableObject();

					for (int32 ComponentIndex = 0; ComponentIndex < CO->GetComponentCount(); ++ComponentIndex)
					{
						LODTracker.RequestedLODPerComponent.Add(CO->GetComponentName(ComponentIndex), MAX_uint8);
					}

					LODTracker.bInitialized = true;
				}

				if (Parent)
				{
					COI->SetIsBeingUsedByComponentInPlay(true);

					const FName& ComponentName = CustomizableObjectInstanceUsage->GetComponentName();

#if WITH_EDITOR
					// If the instance is generated but the component doesn't have a mesh, set it.
					// Can happen when duplicating instances in the editor.
					const USkeletalMesh* SkeletalMesh = COI->GetComponentMeshSkeletalMesh(ComponentName);
					if (SkeletalMesh && !Parent->GetSkeletalMeshAsset())
					{
						// As the instance is already generated, this will be very fast and just set the mesh and call the delegates
						COI->UpdateSkeletalMeshAsync();
					}
#endif

					// If it's the local player set max priority
					const USceneComponent* const AttachParentParentComponent = Parent->GetAttachParent();
					AActor* ParentParentActor = AttachParentParentComponent ? AttachParentParentComponent->GetOwner() : nullptr;

					const APawn* Pawn = Cast<APawn>(ParentParentActor);
					if (Pawn && Pawn->IsPlayerControlled())
					{
						COI->SetMinSquareDistToPlayer(-1.f);
					}

					// Use the component minLOD to set the minimum LOD to generate
					uint8& MinLOD = LODTracker.MinLOD.FindOrAdd(ComponentName, UINT8_MAX);
					MinLOD = FMath::Min(MinLOD, static_cast<uint8>(Parent->bOverrideMinLod ? Parent->MinLodModel : 0));

					// If the parent component have a SkeletalMesh use the RequestedLODLevel of the component as reference to know which LODs mutable should generate.
					if (UE_MUTABLE_GETSKELETALMESHASSET(Parent) && LODTracker.RequestedLODPerComponent.Contains(ComponentName))
					{
						uint8& RequestedLOD = LODTracker.RequestedLODPerComponent[ComponentName];
						RequestedLOD = FMath::Min((int32)RequestedLOD, Parent->GetPredictedLODLevel());
					}
				}
			}
		}

		for (TPair<TObjectPtr<UCustomizableObjectInstance>, FLODTracker>& It : InstancesMinLOD)
		{
			if (IsValidChecked(It.Key) && It.Key->GetPrivate())
			{
				const UCustomizableObject* CustomizableObject = It.Key->GetCustomizableObject();
				if (!CustomizableObject || CustomizableObject->GetPrivate()->IsLocked())
				{
					continue;
				}

				if (IsOnlyUpdateCloseCustomizableObjectsEnabled() && (It.Key->GetMinSquareDistToPlayer() > FMath::Square(CloseCustomizableObjectsDist) || !It.Key->GetPrivate()->NearestToActor.IsValid()))
				{
					continue;
				}

				It.Key->SetRequestedLODs(It.Value.MinLOD, It.Value.RequestedLODPerComponent, InOutRequestedUpdates);
			}
		}
	}
}


int32 UCustomizableInstanceLODManagement::GetNumGeneratedInstancesLimitFullLODs() const
{
	return CVarNumGeneratedInstancesLimit.GetValueOnGameThread();
}


int32 UCustomizableInstanceLODManagement::GetNumGeneratedInstancesLimitLOD1() const
{
	return CVarNumGeneratedInstancesLimitLOD1.GetValueOnGameThread();
}


int32 UCustomizableInstanceLODManagement::GetNumGeneratedInstancesLimitLOD2() const
{
	return CVarNumGeneratedInstancesLimitLOD2.GetValueOnGameThread();
}


void UCustomizableInstanceLODManagement::SetNumberOfPriorityUpdateInstances(int32 InNumPriorityUpdateInstances)
{
	NumPriorityUpdateInstances = InNumPriorityUpdateInstances;
}


int32 UCustomizableInstanceLODManagement::GetNumberOfPriorityUpdateInstances() const
{
	return NumPriorityUpdateInstances;
}


void UCustomizableInstanceLODManagement::SetCustomizableObjectsUpdateDistance(float Distance)
{
	CloseCustomizableObjectsDist = Distance;
}


float UCustomizableInstanceLODManagement::GetOnlyUpdateCloseCustomizableObjectsDist() const
{
	return CloseCustomizableObjectsDist;
}


bool UCustomizableInstanceLODManagement::IsOnlyUpdateCloseCustomizableObjectsEnabled() const
{
	return CVarOnlyUpdateCloseCustomizableObjects.GetValueOnGameThread();
}

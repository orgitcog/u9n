// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "MuCO/CustomizableObjectInstanceUsagePrivate.h"

#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCO/CustomizableSkeletalComponentPrivate.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/ObjectSaveContext.h"
#include "Stats/Stats.h"


void UCustomizableObjectInstanceUsagePrivate::Callbacks()
{
	for (const UCustomizableObjectExtension* Extension : ICustomizableObjectModule::Get().GetRegisteredExtensions())
	{
		Extension->OnCustomizableObjectInstanceUsageUpdated(*GetPublic());
	}
	
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetCustomizableSkeletalComponent())
	{
		CustomizableSkeletalComponent->UpdatedDelegate.ExecuteIfBound();

		if (GetPublic()->UpdatedDelegate.IsBound() && CustomizableSkeletalComponent->UpdatedDelegate.IsBound())
		{
			UE_LOG(LogMutable, Error, TEXT("The UpdatedDelegate is bound both in the UCustomizableObjectInstanceUsage and in its parent CustomizableSkeletalComponent. Only one should be bound."));
			ensure(false);
		}
	}
	
	GetPublic()->UpdatedDelegate.ExecuteIfBound();
}


UCustomizableObjectInstanceUsage::UCustomizableObjectInstanceUsage()
{
	Private = CreateDefaultSubobject<UCustomizableObjectInstanceUsagePrivate>(FName("Private"));
}


void UCustomizableObjectInstanceUsage::SetCustomizableObjectInstance(UCustomizableObjectInstance* CustomizableObjectInstance)
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		CustomizableSkeletalComponent->CustomizableObjectInstance = CustomizableObjectInstance;
	}
	else
	{
		UsedCustomizableObjectInstance = CustomizableObjectInstance;
	}
}


UCustomizableObjectInstance* UCustomizableObjectInstanceUsage::GetCustomizableObjectInstance() const
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		return CustomizableSkeletalComponent->CustomizableObjectInstance;
	}
	else
	{
		return UsedCustomizableObjectInstance;
	}
}


void UCustomizableObjectInstanceUsage::SetComponentIndex(int32 ComponentIndex)
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		CustomizableSkeletalComponent->ComponentIndex = ComponentIndex;
	}
	else
	{
		UsedComponentIndex = ComponentIndex;
	}
}


int32 UCustomizableObjectInstanceUsage::GetComponentIndex() const
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		return CustomizableSkeletalComponent->ComponentIndex;
	}
	else
	{
		return UsedComponentIndex;
	}
}


void UCustomizableObjectInstanceUsage::SetComponentName(const FName& Name)
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		CustomizableSkeletalComponent->SetComponentName(Name);
	}
	else
	{
		UsedComponentIndex = INDEX_NONE;
		UsedComponentName = Name;
	}
}


FName UCustomizableObjectInstanceUsage::GetComponentName() const
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		return CustomizableSkeletalComponent->GetComponentName();
	}
	else
	{
		if (UsedComponentIndex == INDEX_NONE)
		{
			return UsedComponentName;	
		}
		else
		{
			return FName(FString::FromInt(UsedComponentIndex));
		}
	}
}


UCustomizableSkeletalComponent* UCustomizableObjectInstanceUsagePrivate::GetCustomizableSkeletalComponent() const
{
	UObject* Outer = GetPublic()->GetOuter(); // UCustomizableSkeletalComponentPrivate
	if (!Outer)
	{
		return nullptr;
	}

	return Cast<UCustomizableSkeletalComponent>(Outer->GetOuter());
}

UCustomizableObjectInstanceUsage* UCustomizableObjectInstanceUsagePrivate::GetPublic()
{
	UCustomizableObjectInstanceUsage* Public = StaticCast<UCustomizableObjectInstanceUsage*>(GetOuter());
	check(Public);

	return Public;
}


const UCustomizableObjectInstanceUsage* UCustomizableObjectInstanceUsagePrivate::GetPublic() const
{
	UCustomizableObjectInstanceUsage* Public = StaticCast<UCustomizableObjectInstanceUsage*>(GetOuter());
	check(Public);

	return Public;
}


void UCustomizableObjectInstanceUsage::SetSkipSetReferenceSkeletalMesh(bool bSkip)
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		CustomizableSkeletalComponent->SetSkipSetReferenceSkeletalMesh(bSkip);
	}
	else
	{
		bUsedSkipSetReferenceSkeletalMesh = bSkip;
	}
}


bool UCustomizableObjectInstanceUsage::GetSkipSetReferenceSkeletalMesh() const
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		return CustomizableSkeletalComponent->GetSkipSetReferenceSkeletalMesh();
	}
	else
	{
		return bUsedSkipSetReferenceSkeletalMesh;
	}
}


void UCustomizableObjectInstanceUsage::SetSkipSetSkeletalMeshOnAttach(bool bSkip)
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		CustomizableSkeletalComponent->SetSkipSetSkeletalMeshOnAttach(bSkip);
	}
	else
	{
		bUsedSkipSetSkeletalMeshOnAttach = bSkip;
	}
}


bool UCustomizableObjectInstanceUsage::GetSkipSetSkeletalMeshOnAttach() const
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		return CustomizableSkeletalComponent->GetSkipSetReferenceSkeletalMesh();
	}
	else
	{
		return bUsedSkipSetSkeletalMeshOnAttach;
	}
}


void UCustomizableObjectInstanceUsage::AttachTo(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (IsValid(SkeletalMeshComponent))
	{
		UsedSkeletalMeshComponent = SkeletalMeshComponent;
	}
	else
	{
		UsedSkeletalMeshComponent = nullptr;
	}

	if (!GetSkipSetSkeletalMeshOnAttach())
	{
		GetPrivate()->bPendingSetSkeletalMesh = true;
	}
}


USkeletalMeshComponent* UCustomizableObjectInstanceUsage::GetAttachParent() const
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetPrivate()->GetCustomizableSkeletalComponent())
	{
		return Cast<USkeletalMeshComponent>(CustomizableSkeletalComponent->GetAttachParent());
	}
	else if(UsedSkeletalMeshComponent.IsValid())
	{
		return UsedSkeletalMeshComponent.Get();
	}

	return nullptr;
}


USkeletalMesh* UCustomizableObjectInstanceUsagePrivate::GetSkeletalMesh() const
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetPublic()->GetCustomizableObjectInstance();

	return CustomizableObjectInstance ? CustomizableObjectInstance->GetComponentMeshSkeletalMesh(GetPublic()->GetComponentName()) : nullptr;
}


USkeletalMesh* UCustomizableObjectInstanceUsagePrivate::GetAttachedSkeletalMesh() const
{
	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetPublic()->GetAttachParent());

	if (Parent)
	{
		return UE_MUTABLE_GETSKELETALMESHASSET(Parent);
	}

	return nullptr;
}


void UCustomizableObjectInstanceUsage::UpdateSkeletalMeshAsync(bool bNeverSkipUpdate)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsync(false, false);
	}
}


void UCustomizableObjectInstanceUsage::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsyncResult(Callback, false, false);
	}
}


#if WITH_EDITOR
void UCustomizableObjectInstanceUsagePrivate::UpdateDistFromComponentToLevelEditorCamera(const FVector& CameraPosition)
{
	// We want instances in the editor to be generated
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::Editor)
	{
		return;
	}

	UCustomizableObjectInstance* CustomizableObjectInstance = GetPublic()->GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = GetPublic()->GetAttachParent();
		AActor* ParentActor = SkeletalMeshComponent ? SkeletalMeshComponent->GetAttachmentRootActor() : nullptr;
		if (ParentActor && ParentActor->IsValidLowLevel())
		{
			// update distance to camera and set the instance as being used by a component
			CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(UsedByComponent);

			float SquareDist = FVector::DistSquared(CameraPosition, ParentActor->GetActorLocation());
			CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = 
				FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
		}
	}
}
#endif


UCustomizableObjectInstanceUsagePrivate* UCustomizableObjectInstanceUsage::GetPrivate()
{
	return Private;
}


const UCustomizableObjectInstanceUsagePrivate* UCustomizableObjectInstanceUsage::GetPrivate() const
{
	return Private;
}


void UCustomizableObjectInstanceUsagePrivate::UpdateDistFromComponentToPlayer(const AActor* ViewCenter, const bool bForceEvenIfNotBegunPlay)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetPublic()->GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = GetPublic()->GetAttachParent();
		AActor* ParentActor = SkeletalMeshComponent ? SkeletalMeshComponent->GetAttachmentRootActor() : nullptr;
		
		CustomizableObjectInstance->SetIsPlayerOrNearIt(false);

		if (ParentActor && ParentActor->IsValidLowLevel())
		{
			if (ParentActor->HasActorBegunPlay() || bForceEvenIfNotBegunPlay)
			{
				float SquareDist = FLT_MAX;

				if (ViewCenter && ViewCenter->IsValidLowLevel())
				{
					APawn* Pawn = Cast<APawn>(ParentActor);
					bool bIsPlayer = Pawn ? Pawn->IsPlayerControlled() : false;
					CustomizableObjectInstance->SetIsPlayerOrNearIt(bIsPlayer);

					if (bIsPlayer)
					{
						SquareDist = -0.01f; // Negative value to give the player character more priority than any other character
					}
					else
					{
						SquareDist = FVector::DistSquared(ViewCenter->GetActorLocation(), ParentActor->GetActorLocation());
					}
				}
				else if (bForceEvenIfNotBegunPlay)
				{
					SquareDist = -0.01f; // This is a manual update before begin play and the creation of the pawn, so it should probably be high priority
					CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer = FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
				}
				else
				{
					SquareDist = 0.f; // This a mutable tick before begin play and the creation of the pawn, so it should have a definite and high priority but less than a manual update
					CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer = FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
				}

				CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
				CustomizableObjectInstance->SetIsBeingUsedByComponentInPlay(true);

				if (CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer == SquareDist)
				{
					CustomizableObjectInstance->GetPrivate()->NearestToActor = GetPublic();
					CustomizableObjectInstance->GetPrivate()->NearestToViewCenter = ViewCenter;
				}
			}
		}
	}
}


bool UCustomizableObjectInstanceUsagePrivate::IsNetMode(ENetMode InNetMode) const
{
	if (UCustomizableSkeletalComponent* CustomizableSkeletalComponent = GetCustomizableSkeletalComponent())
	{
		return CustomizableSkeletalComponent->IsNetMode(InNetMode);
	}
	else if (GetPublic()->UsedSkeletalMeshComponent.IsValid())
	{
		return GetPublic()->UsedSkeletalMeshComponent->IsNetMode(InNetMode);
	}

	return false;
}


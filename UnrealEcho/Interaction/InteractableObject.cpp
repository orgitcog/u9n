// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Interactable Object Implementation

#include "InteractableObject.h"
#include "Components/StaticMeshComponent.h"
#include "Personality/PersonalityTraitSystem.h"

//////////////////////////////////////////////////////////////////////////
// AInteractableObject

AInteractableObject::AInteractableObject()
	: bCanInteractWhileBusy(false)
	, MaxSimultaneousInteractors(1)
	, InteractionProgress(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootSceneComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(RootSceneComponent);

	// Add interactable tag
	Tags.Add(TEXT("Interactable"));
}

void AInteractableObject::BeginPlay()
{
	Super::BeginPlay();
}

void AInteractableObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ActiveInteractions.Num() > 0)
	{
		UpdateInteraction(DeltaTime);
	}
}

bool AInteractableObject::IsInteractionAvailable(AActor* Interactor, EInteractionType Type) const
{
	if (!Interactor || !InteractableData)
	{
		return false;
	}

	// Check if already at max interactors
	if (!bCanInteractWhileBusy && CurrentInteractors.Num() >= MaxSimultaneousInteractors)
	{
		return false;
	}

	// Find the interaction data
	const FInteractionData* Data = nullptr;
	for (const FInteractionData& InteractionData : InteractableData->AvailableInteractions)
	{
		if (InteractionData.Type == Type)
		{
			Data = &InteractionData;
			break;
		}
	}

	if (!Data)
	{
		return false;
	}

	return CheckInteractionRequirements(Interactor, *Data);
}

TArray<FInteractionData> AInteractableObject::GetAvailableInteractions(AActor* Interactor) const
{
	TArray<FInteractionData> Available;

	if (!Interactor || !InteractableData)
	{
		return Available;
	}

	for (const FInteractionData& Data : InteractableData->AvailableInteractions)
	{
		if (CheckInteractionRequirements(Interactor, Data))
		{
			Available.Add(Data);
		}
	}

	return Available;
}

bool AInteractableObject::BeginInteraction(AActor* Interactor, EInteractionType Type)
{
	if (!IsInteractionAvailable(Interactor, Type))
	{
		return false;
	}

	// Add to current interactors
	CurrentInteractors.Add(Interactor);
	ActiveInteractions.Add(Interactor, Type);

	// Notify
	OnInteractionStarted(Interactor, Type);
	OnInteractionBegin.Broadcast(Interactor, Type);

	return true;
}

void AInteractableObject::EndInteraction(AActor* Interactor, bool bSuccess)
{
	if (!Interactor)
	{
		return;
	}

	EInteractionType* TypePtr = ActiveInteractions.Find(Interactor);
	if (!TypePtr)
	{
		return;
	}

	EInteractionType Type = *TypePtr;

	// Remove from tracking
	CurrentInteractors.Remove(Interactor);
	ActiveInteractions.Remove(Interactor);

	// Notify
	OnInteractionCompleted(Interactor, Type, bSuccess);
	OnInteractionEnd.Broadcast(Interactor, Type, bSuccess);
}

void AInteractableObject::CancelInteraction(AActor* Interactor)
{
	EndInteraction(Interactor, false);
}

FInteractionData AInteractableObject::GetInteractionData(EInteractionType Type) const
{
	if (InteractableData)
	{
		for (const FInteractionData& Data : InteractableData->AvailableInteractions)
		{
			if (Data.Type == Type)
			{
				return Data;
			}
		}
	}

	return FInteractionData();
}

float AInteractableObject::GetPersonalityAffinity(const FPersonalityState& Personality) const
{
	if (!InteractableData)
	{
		return 0.5f;
	}

	float Affinity = 0.0f;

	// Calculate weighted affinity based on personality traits
	Affinity += Personality.Extraversion * InteractableData->ExtraversionAffinity;
	Affinity += Personality.Openness * InteractableData->OpennessAffinity;
	Affinity += Personality.Conscientiousness * InteractableData->ConscientiousnessAffinity;
	Affinity += Personality.Agreeableness * InteractableData->AgreeablenessAffinity;
	Affinity += (1.0f - Personality.Neuroticism) * InteractableData->NeuroticismAffinity; // Inverted

	// Normalize to 0-1 range
	Affinity /= 5.0f;

	return FMath::Clamp(Affinity, 0.0f, 1.0f);
}

void AInteractableObject::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	// Override in subclasses
	InteractionProgress = 0.0f;
}

void AInteractableObject::OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess)
{
	// Override in subclasses
	InteractionProgress = 0.0f;
}

void AInteractableObject::UpdateInteraction(float DeltaTime)
{
	// Update progress for timed interactions
	for (auto& Pair : ActiveInteractions)
	{
		AActor* Interactor = Pair.Key.Get();
		EInteractionType Type = Pair.Value;

		if (!Interactor)
		{
			continue;
		}

		FInteractionData Data = GetInteractionData(Type);
		if (Data.Duration > 0.0f)
		{
			InteractionProgress += DeltaTime / Data.Duration;

			if (InteractionProgress >= 1.0f)
			{
				EndInteraction(Interactor, true);
			}
		}
	}
}

bool AInteractableObject::CheckInteractionRequirements(AActor* Interactor, const FInteractionData& Data) const
{
	if (!Interactor)
	{
		return false;
	}

	// Check distance
	float Distance = FVector::Dist(GetActorLocation(), Interactor->GetActorLocation());
	if (Distance > Data.MaxInteractionDistance)
	{
		return false;
	}

	// Check line of sight if required
	if (Data.bRequiresLineOfSight)
	{
		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(Interactor);

		bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			Interactor->GetActorLocation(),
			GetActorLocation(),
			ECC_Visibility,
			QueryParams
		);

		if (bHit)
		{
			return false; // Something is blocking line of sight
		}
	}

	// Check required tags (would need to get tags from interactor)
	// For now, return true if basic requirements are met

	return true;
}

//////////////////////////////////////////////////////////////////////////
// AInteractableContainer

AInteractableContainer::AInteractableContainer()
	: bIsOpen(false)
{
}

void AInteractableContainer::Open(AActor* Opener)
{
	if (!bIsOpen)
	{
		bIsOpen = true;

		// Spawn contained items
		for (TSubclassOf<AActor> ItemClass : ContainedItemClasses)
		{
			if (ItemClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = this;
				AActor* SpawnedItem = GetWorld()->SpawnActor<AActor>(ItemClass, GetActorLocation(), GetActorRotation(), SpawnParams);
				if (SpawnedItem)
				{
					SpawnedContents.Add(SpawnedItem);
				}
			}
		}
	}
}

void AInteractableContainer::Close(AActor* Closer)
{
	if (bIsOpen)
	{
		bIsOpen = false;

		// Destroy spawned contents
		for (AActor* Item : SpawnedContents)
		{
			if (Item)
			{
				Item->Destroy();
			}
		}
		SpawnedContents.Empty();
	}
}

TArray<AActor*> AInteractableContainer::GetContents() const
{
	TArray<AActor*> Contents;
	for (AActor* Item : SpawnedContents)
	{
		if (Item)
		{
			Contents.Add(Item);
		}
	}
	return Contents;
}

void AInteractableContainer::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	Super::OnInteractionStarted(Interactor, Type);

	if (Type == EInteractionType::Open)
	{
		if (bIsOpen)
		{
			Close(Interactor);
		}
		else
		{
			Open(Interactor);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// AInteractableFurniture

AInteractableFurniture::AInteractableFurniture()
	: SitSocketName(TEXT("SitSocket"))
{
}

bool AInteractableFurniture::Occupy(AActor* Occupant)
{
	if (IsOccupied() || !Occupant)
	{
		return false;
	}

	CurrentOccupant = Occupant;
	return true;
}

void AInteractableFurniture::Vacate(AActor* Occupant)
{
	if (CurrentOccupant == Occupant)
	{
		CurrentOccupant = nullptr;
	}
}

FTransform AInteractableFurniture::GetSitTransform() const
{
	FTransform SitTransform = GetActorTransform();

	// Apply offset
	SitTransform = SitOffset * SitTransform;

	return SitTransform;
}

void AInteractableFurniture::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	Super::OnInteractionStarted(Interactor, Type);

	if (Type == EInteractionType::Sit)
	{
		Occupy(Interactor);
	}
}

void AInteractableFurniture::OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess)
{
	Super::OnInteractionCompleted(Interactor, Type, bSuccess);

	if (Type == EInteractionType::Sit)
	{
		Vacate(Interactor);
	}
}

//////////////////////////////////////////////////////////////////////////
// AInteractableNPC

AInteractableNPC::AInteractableNPC()
	: RelationshipLevel(0.5f)
	, bInConversation(false)
{
}

void AInteractableNPC::StartConversation(AActor* Initiator)
{
	if (!bInConversation && Initiator)
	{
		bInConversation = true;
		ConversationPartner = Initiator;

		// Select a greeting based on relationship level
		if (GreetingLines.Num() > 0)
		{
			int32 GreetingIndex = FMath::Clamp(
				FMath::FloorToInt(RelationshipLevel * GreetingLines.Num()),
				0,
				GreetingLines.Num() - 1
			);

			// Would trigger dialogue system here
			UE_LOG(LogTemp, Log, TEXT("NPC %s says: %s"), *NPCName.ToString(), *GreetingLines[GreetingIndex].ToString());
		}
	}
}

void AInteractableNPC::EndConversation()
{
	bInConversation = false;
	ConversationPartner = nullptr;
}

void AInteractableNPC::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	Super::OnInteractionStarted(Interactor, Type);

	if (Type == EInteractionType::Talk)
	{
		StartConversation(Interactor);
	}
}

void AInteractableNPC::OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess)
{
	Super::OnInteractionCompleted(Interactor, Type, bSuccess);

	if (Type == EInteractionType::Talk)
	{
		EndConversation();

		// Update relationship based on success
		if (bSuccess)
		{
			RelationshipLevel = FMath::Clamp(RelationshipLevel + 0.05f, 0.0f, 1.0f);
		}
	}
}

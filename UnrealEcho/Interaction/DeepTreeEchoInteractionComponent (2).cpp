// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Cognitive-Aware Interaction Implementation

#include "DeepTreeEchoInteractionComponent.h"
#include "Character/DeepTreeEchoCharacter.h"
#include "Cognitive/CognitiveStateComponent.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UDeepTreeEchoInteractionComponent::UDeepTreeEchoInteractionComponent()
	: ScanRadius(500.0f)
	, ScanInterval(0.25f)
	, MaxInteractionMemory(100)
	, bRequireLineOfSight(true)
	, AttentionBonusMultiplier(1.5f)
	, bIsInteracting(false)
	, TimeSinceLastScan(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // Tick at 10Hz for performance

	// Default interactable object types
	InteractableObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	InteractableObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
}

void UDeepTreeEchoInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Try to find owning character if not set
	if (!OwningCharacter)
	{
		OwningCharacter = Cast<ADeepTreeEchoCharacter>(GetOwner());
	}
}

void UDeepTreeEchoInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Periodic environment scan
	TimeSinceLastScan += DeltaTime;
	if (TimeSinceLastScan >= ScanInterval)
	{
		PerformEnvironmentScan();
		TimeSinceLastScan = 0.0f;
	}

	// Update priorities based on current cognitive/emotional state
	UpdateInteractablePriorities();

	// Cleanup old memories periodically
	if (InteractionHistory.Num() > MaxInteractionMemory)
	{
		CleanupOldMemories();
	}
}

void UDeepTreeEchoInteractionComponent::Initialize(ADeepTreeEchoCharacter* InOwner)
{
	OwningCharacter = InOwner;

	if (OwningCharacter)
	{
		CognitiveState = OwningCharacter->GetCognitiveState();
		PersonalitySystem = OwningCharacter->GetPersonalitySystem();
	}
}

TArray<AActor*> UDeepTreeEchoInteractionComponent::FindInteractableObjects(float Radius) const
{
	TArray<AActor*> Results;

	if (!GetOwner())
	{
		return Results;
	}

	FVector Origin = GetOwner()->GetActorLocation();

	// Perform sphere overlap
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),
		FCollisionShape::MakeSphere(Radius),
		QueryParams
	);

	// Filter and collect interactable actors
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && IsInteractable(Actor))
		{
			Results.AddUnique(Actor);
		}
	}

	return Results;
}

TArray<FInteractableInfo> UDeepTreeEchoInteractionComponent::GetNearbyInteractables() const
{
	return CachedInteractables;
}

FInteractableInfo UDeepTreeEchoInteractionComponent::GetBestInteractable() const
{
	if (CachedInteractables.Num() > 0)
	{
		return CachedInteractables[0]; // Already sorted by priority
	}

	return FInteractableInfo();
}

FInteractionResult UDeepTreeEchoInteractionComponent::InteractWith(AActor* Target)
{
	FInteractionResult Result;

	if (!Target)
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("No target specified"));
		return Result;
	}

	if (!IsInteractable(Target))
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("Target is not interactable"));
		return Result;
	}

	if (bIsInteracting)
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("Already interacting with another object"));
		return Result;
	}

	// Check line of sight if required
	if (bRequireLineOfSight && !CheckLineOfSight(Target))
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("No line of sight to target"));
		return Result;
	}

	// Start interaction
	bIsInteracting = true;
	CurrentInteractionTarget = Target;

	// Broadcast start event
	OnInteractionStarted.Broadcast(Target, NAME_None);

	// Execute the interaction
	Result = ExecuteInteraction(Target);

	// Record in memory
	RecordInteraction(Target, NAME_None, Result);

	// End interaction
	bIsInteracting = false;
	CurrentInteractionTarget = nullptr;

	// Broadcast completion event
	OnInteractionCompleted.Broadcast(Target, Result);

	return Result;
}

FInteractionResult UDeepTreeEchoInteractionComponent::InteractWithBest()
{
	FInteractableInfo Best = GetBestInteractable();
	if (Best.Actor.IsValid())
	{
		return InteractWith(Best.Actor.Get());
	}

	FInteractionResult Result;
	Result.bSuccess = false;
	Result.ResultMessage = FText::FromString(TEXT("No interactable objects nearby"));
	return Result;
}

bool UDeepTreeEchoInteractionComponent::IsInteractable(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Check if actor implements interaction interface
	// For now, check if actor has specific tags or components
	if (Actor->Tags.Contains(TEXT("Interactable")))
	{
		return true;
	}

	// Check for interaction component
	UActorComponent* InteractionComp = Actor->GetComponentByClass(UDeepTreeEchoInteractionComponent::StaticClass());
	if (InteractionComp)
	{
		return true;
	}

	// Check gameplay tags if required
	if (RequiredInteractionTags.Num() > 0)
	{
		// Would check actor's gameplay tags here
		// For now, return true if no specific tags required
	}

	return false;
}

void UDeepTreeEchoInteractionComponent::CancelInteraction()
{
	if (bIsInteracting && CurrentInteractionTarget.IsValid())
	{
		FInteractionResult CancelResult;
		CancelResult.bSuccess = false;
		CancelResult.ResultMessage = FText::FromString(TEXT("Interaction cancelled"));
		CancelResult.EmotionalImpact = -0.1f; // Slight negative impact

		OnInteractionCompleted.Broadcast(CurrentInteractionTarget.Get(), CancelResult);

		bIsInteracting = false;
		CurrentInteractionTarget = nullptr;
	}
}

int32 UDeepTreeEchoInteractionComponent::GetInteractionCount(AActor* Actor) const
{
	int32 Count = 0;
	for (const FInteractionMemory& Memory : InteractionHistory)
	{
		if (Memory.InteractedActor.Get() == Actor)
		{
			Count++;
		}
	}
	return Count;
}

float UDeepTreeEchoInteractionComponent::GetEmotionalAssociation(AActor* Actor) const
{
	float TotalValence = 0.0f;
	int32 Count = 0;

	for (const FInteractionMemory& Memory : InteractionHistory)
	{
		if (Memory.InteractedActor.Get() == Actor)
		{
			TotalValence += Memory.EmotionalValence;
			Count++;
		}
	}

	if (Count > 0)
	{
		return TotalValence / Count;
	}

	return 0.5f; // Neutral if no history
}

void UDeepTreeEchoInteractionComponent::SetGazeTarget(AActor* Target)
{
	CurrentGazeTarget = Target;
}

void UDeepTreeEchoInteractionComponent::PerformEnvironmentScan()
{
	TArray<FInteractableInfo> OldInteractables = CachedInteractables;
	CachedInteractables.Empty();

	if (!GetOwner())
	{
		return;
	}

	FVector Origin = GetOwner()->GetActorLocation();

	// Find all interactable objects
	TArray<AActor*> InteractableActors = FindInteractableObjects(ScanRadius);

	// Build info for each interactable
	for (AActor* Actor : InteractableActors)
	{
		FInteractableInfo Info;
		Info.Actor = Actor;
		Info.Distance = FVector::Dist(Origin, Actor->GetActorLocation());
		Info.InteractionType = NAME_None; // Would be determined by actor's interaction interface
		Info.DisplayName = FText::FromString(Actor->GetName());
		Info.bRequiresLineOfSight = bRequireLineOfSight;
		Info.bIsCurrentlyVisible = CheckLineOfSight(Actor);

		// Calculate priority
		Info.Priority = CalculateInteractionPriority(Info);

		CachedInteractables.Add(Info);
	}

	// Sort by priority (highest first)
	CachedInteractables.Sort([](const FInteractableInfo& A, const FInteractableInfo& B)
	{
		return A.Priority > B.Priority;
	});

	// Notify of changes
	NotifyInteractableChanges(OldInteractables);
}

void UDeepTreeEchoInteractionComponent::UpdateInteractablePriorities()
{
	for (FInteractableInfo& Info : CachedInteractables)
	{
		if (Info.Actor.IsValid())
		{
			Info.Priority = CalculateInteractionPriority(Info);
			Info.bIsCurrentlyVisible = CheckLineOfSight(Info.Actor.Get());
		}
	}

	// Re-sort
	CachedInteractables.Sort([](const FInteractableInfo& A, const FInteractableInfo& B)
	{
		return A.Priority > B.Priority;
	});
}

float UDeepTreeEchoInteractionComponent::CalculateInteractionPriority(const FInteractableInfo& Info) const
{
	float Priority = 0.0f;

	if (!Info.Actor.IsValid())
	{
		return Priority;
	}

	// Base priority from distance (closer = higher priority)
	float DistanceFactor = 1.0f - FMath::Clamp(Info.Distance / ScanRadius, 0.0f, 1.0f);
	Priority += DistanceFactor * 30.0f;

	// Visibility bonus
	if (Info.bIsCurrentlyVisible)
	{
		Priority += 20.0f;
	}

	// Attention bonus (if this is the gaze target)
	if (CurrentGazeTarget.IsValid() && CurrentGazeTarget.Get() == Info.Actor.Get())
	{
		Priority *= AttentionBonusMultiplier;
	}

	// Personality modifier
	Priority += GetPersonalityInteractionModifier(Info);

	// Cognitive modifier
	Priority += GetCognitiveInteractionModifier(Info);

	// Memory modifier (familiarity)
	Priority += GetMemoryInteractionModifier(Info.Actor.Get());

	return Priority;
}

bool UDeepTreeEchoInteractionComponent::CheckLineOfSight(AActor* Target) const
{
	if (!Target || !GetOwner())
	{
		return false;
	}

	FVector Start = GetOwner()->GetActorLocation();
	FVector End = Target->GetActorLocation();

	// Add eye height offset
	if (OwningCharacter)
	{
		Start.Z += 60.0f; // Approximate eye height
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(Target);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	// If we didn't hit anything, we have line of sight
	return !bHit;
}

void UDeepTreeEchoInteractionComponent::RecordInteraction(AActor* Target, FName InteractionType, const FInteractionResult& Result)
{
	FInteractionMemory Memory;
	Memory.InteractedActor = Target;
	Memory.InteractionType = InteractionType;
	Memory.Timestamp = GetWorld()->GetTimeSeconds();
	Memory.EmotionalValence = 0.5f + (Result.EmotionalImpact * 0.5f); // Convert -1,1 to 0,1
	Memory.CognitiveLoad = Result.CognitiveImpact;
	Memory.bWasSuccessful = Result.bSuccess;

	InteractionHistory.Add(Memory);

	// Notify cognitive system
	if (CognitiveState)
	{
		CognitiveState->RecordInteraction(Target);
	}

	// Notify personality system
	if (PersonalitySystem)
	{
		PersonalitySystem->ProcessInteractionFeedback(Target);
	}
}

void UDeepTreeEchoInteractionComponent::CleanupOldMemories()
{
	// Keep only the most recent memories
	if (InteractionHistory.Num() > MaxInteractionMemory)
	{
		int32 ToRemove = InteractionHistory.Num() - MaxInteractionMemory;
		InteractionHistory.RemoveAt(0, ToRemove);
	}
}

FInteractionResult UDeepTreeEchoInteractionComponent::ExecuteInteraction(AActor* Target)
{
	FInteractionResult Result;
	Result.bSuccess = true;
	Result.ResultMessage = FText::FromString(FString::Printf(TEXT("Interacted with %s"), *Target->GetName()));

	// Calculate emotional impact based on personality and past experiences
	float EmotionalAssociation = GetEmotionalAssociation(Target);
	
	// Positive association = positive impact, negative = negative
	Result.EmotionalImpact = (EmotionalAssociation - 0.5f) * 0.4f;

	// Add some randomness for emergent behavior
	Result.EmotionalImpact += FMath::RandRange(-0.1f, 0.1f);

	// Cognitive impact based on novelty
	int32 PastInteractions = GetInteractionCount(Target);
	if (PastInteractions == 0)
	{
		// First interaction - higher cognitive load (novelty)
		Result.CognitiveImpact = 0.3f;
	}
	else
	{
		// Familiar interaction - lower cognitive load
		Result.CognitiveImpact = 0.1f / FMath::Min(PastInteractions, 10);
	}

	// Apply cognitive impact to cognitive state
	if (CognitiveState)
	{
		CognitiveState->AddCognitiveLoad(Result.CognitiveImpact);
	}

	// Apply emotional impact to personality system
	if (PersonalitySystem)
	{
		FEmotionalState CurrentEmotion = PersonalitySystem->GetCurrentEmotionalState();
		CurrentEmotion.Valence = FMath::Clamp(CurrentEmotion.Valence + Result.EmotionalImpact, 0.0f, 1.0f);
		PersonalitySystem->SetEmotionalState(CurrentEmotion);
	}

	return Result;
}

void UDeepTreeEchoInteractionComponent::NotifyInteractableChanges(const TArray<FInteractableInfo>& OldInteractables)
{
	// Find newly discovered interactables
	for (const FInteractableInfo& NewInfo : CachedInteractables)
	{
		bool bWasKnown = false;
		for (const FInteractableInfo& OldInfo : OldInteractables)
		{
			if (OldInfo.Actor.Get() == NewInfo.Actor.Get())
			{
				bWasKnown = true;
				break;
			}
		}

		if (!bWasKnown)
		{
			OnInteractableDiscovered.Broadcast(NewInfo);
		}
	}

	// Find lost interactables
	for (const FInteractableInfo& OldInfo : OldInteractables)
	{
		bool bStillKnown = false;
		for (const FInteractableInfo& NewInfo : CachedInteractables)
		{
			if (NewInfo.Actor.Get() == OldInfo.Actor.Get())
			{
				bStillKnown = true;
				break;
			}
		}

		if (!bStillKnown && OldInfo.Actor.IsValid())
		{
			OnInteractableLost.Broadcast(OldInfo.Actor.Get());
		}
	}
}

float UDeepTreeEchoInteractionComponent::GetPersonalityInteractionModifier(const FInteractableInfo& Info) const
{
	float Modifier = 0.0f;

	if (!PersonalitySystem)
	{
		return Modifier;
	}

	// Extraversion: Prefer social interactions
	float Extraversion = PersonalitySystem->GetTraitValue(TEXT("Extraversion"));
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Social"))))
	{
		Modifier += (Extraversion - 0.5f) * 20.0f;
	}

	// Openness: Prefer novel/creative interactions
	float Openness = PersonalitySystem->GetTraitValue(TEXT("Openness"));
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Creative"))))
	{
		Modifier += (Openness - 0.5f) * 20.0f;
	}

	// Conscientiousness: Prefer task-oriented interactions
	float Conscientiousness = PersonalitySystem->GetTraitValue(TEXT("Conscientiousness"));
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Task"))))
	{
		Modifier += (Conscientiousness - 0.5f) * 20.0f;
	}

	// Agreeableness: Prefer helpful interactions
	float Agreeableness = PersonalitySystem->GetTraitValue(TEXT("Agreeableness"));
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Helpful"))))
	{
		Modifier += (Agreeableness - 0.5f) * 20.0f;
	}

	return Modifier;
}

float UDeepTreeEchoInteractionComponent::GetCognitiveInteractionModifier(const FInteractableInfo& Info) const
{
	float Modifier = 0.0f;

	if (!CognitiveState)
	{
		return Modifier;
	}

	// High cognitive load: Prefer simpler interactions
	float CogLoad = CognitiveState->GetCurrentCognitiveLoad();
	
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Complex"))))
	{
		// Complex interactions are less preferred when cognitive load is high
		Modifier -= CogLoad * 30.0f;
	}
	else if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Simple"))))
	{
		// Simple interactions are more preferred when cognitive load is high
		Modifier += CogLoad * 15.0f;
	}

	// Attention level affects all priorities
	float Attention = CognitiveState->GetAttentionLevel();
	Modifier *= Attention; // Scale all modifiers by attention

	return Modifier;
}

float UDeepTreeEchoInteractionComponent::GetMemoryInteractionModifier(AActor* Actor) const
{
	float Modifier = 0.0f;

	if (!Actor)
	{
		return Modifier;
	}

	// Familiarity bonus (more interactions = more familiar)
	int32 InteractionCount = GetInteractionCount(Actor);
	if (InteractionCount > 0)
	{
		// Logarithmic familiarity bonus (diminishing returns)
		Modifier += FMath::Loge(1.0f + InteractionCount) * 5.0f;
	}

	// Emotional association modifier
	float EmotionalAssociation = GetEmotionalAssociation(Actor);
	// Positive associations increase priority, negative decrease
	Modifier += (EmotionalAssociation - 0.5f) * 20.0f;

	return Modifier;
}

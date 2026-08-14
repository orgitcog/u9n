// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Advanced Character Implementation

#include "DeepTreeEchoCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"

// Deep Tree Echo components
#include "Personality/PersonalityTraitSystem.h"
#include "Cognitive/CognitiveStateComponent.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Avatar/Avatar3DComponent.h"
#include "Audio/AudioManagerComponent.h"
#include "Interaction/DeepTreeEchoInteractionComponent.h"
#include "Cosmetics/DeepTreeEchoCosmeticsComponent.h"

//////////////////////////////////////////////////////////////////////////
// FDeepTreeEchoReplicatedState

bool FDeepTreeEchoReplicatedState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	Ar << Valence;
	Ar << Arousal;
	Ar << Dominance;
	Ar << CognitiveLoad;
	Ar << Openness;
	Ar << Conscientiousness;
	Ar << Extraversion;
	Ar << Agreeableness;
	Ar << Neuroticism;
	Ar << BehaviorFlags;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// ADeepTreeEchoCharacter

ADeepTreeEchoCharacter::ADeepTreeEchoCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDeepTreeEchoMovementComponent>(ACharacter::CharacterMovementComponentName))
	, CachedCognitiveLoad(0.0f)
	, CachedEmotionalState(FVector::ZeroVector)
	, ReplicationUpdateTimer(0.0f)
	, VisualizationUpdateTimer(0.0f)
{
	// Set this character to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Configure character mesh
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	GetMesh()->SetGenerateOverlapEvents(true);
	GetMesh()->SetNotifyRigidBodyCollision(true);

	// Create personality system
	PersonalitySystem = CreateDefaultSubobject<UPersonalityTraitSystem>(TEXT("PersonalitySystem"));

	// Create cognitive state component
	CognitiveState = CreateDefaultSubobject<UCognitiveStateComponent>(TEXT("CognitiveState"));

	// Create neurochemical simulation
	NeurochemicalSystem = CreateDefaultSubobject<UNeurochemicalSimulationComponent>(TEXT("NeurochemicalSystem"));

	// Create avatar 3D component
	AvatarComponent = CreateDefaultSubobject<UAvatar3DComponent>(TEXT("AvatarComponent"));

	// Create interaction component
	InteractionComponent = CreateDefaultSubobject<UDeepTreeEchoInteractionComponent>(TEXT("InteractionComponent"));

	// Create cosmetics component
	CosmeticsComponent = CreateDefaultSubobject<UDeepTreeEchoCosmeticsComponent>(TEXT("CosmeticsComponent"));

	// Create audio manager
	AudioManager = CreateDefaultSubobject<UAudioManagerComponent>(TEXT("AudioManager"));

	// Create ability system component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Configure replication
	bReplicates = true;
	SetReplicatingMovement(true);
}

void ADeepTreeEchoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Input will be configured via Enhanced Input System in blueprints
	// or through a separate input configuration component
}

void ADeepTreeEchoCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Initialize ability system for server
	InitializeAbilitySystem();

	// Bind to personality events
	if (PersonalitySystem)
	{
		PersonalitySystem->OnPersonalityShift.AddDynamic(this, &ADeepTreeEchoCharacter::OnPersonalityShift);
	}

	// Bind to cognitive events
	if (CognitiveState)
	{
		CognitiveState->OnCognitiveLoadChanged.AddDynamic(this, &ADeepTreeEchoCharacter::OnCognitiveLoadChanged);
	}
}

void ADeepTreeEchoCharacter::UnPossessed()
{
	// Unbind events
	if (PersonalitySystem)
	{
		PersonalitySystem->OnPersonalityShift.RemoveDynamic(this, &ADeepTreeEchoCharacter::OnPersonalityShift);
	}

	if (CognitiveState)
	{
		CognitiveState->OnCognitiveLoadChanged.RemoveDynamic(this, &ADeepTreeEchoCharacter::OnCognitiveLoadChanged);
	}

	Super::UnPossessed();
}

void ADeepTreeEchoCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Initialize all components
	InitializeComponents();

	// Start with default emotional state
	if (AvatarComponent)
	{
		AvatarComponent->SetEmotionalState(FEmotionalState(0.5f, 0.5f, 0.5f));
	}
}

void ADeepTreeEchoCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ADeepTreeEchoCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update component synchronization
	UpdateComponentSynchronization();

	// Update emotional effects
	UpdateEmotionalEffects();

	// Update cognitive visualization
	UpdateCognitiveVisualization();

	// Update replicated state (server only, throttled)
	if (HasAuthority())
	{
		ReplicationUpdateTimer += DeltaTime;
		if (ReplicationUpdateTimer >= 0.1f) // Update 10 times per second
		{
			UpdateReplicatedState();
			ReplicationUpdateTimer = 0.0f;
		}
	}
}

UAbilitySystemComponent* ADeepTreeEchoCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

//////////////////////////////////////////////////////////////////////////
// Emotional State Accessors

float ADeepTreeEchoCharacter::GetEmotionalValence() const
{
	if (PersonalitySystem)
	{
		return PersonalitySystem->GetCurrentEmotionalState().Valence;
	}
	return 0.5f;
}

float ADeepTreeEchoCharacter::GetEmotionalArousal() const
{
	if (PersonalitySystem)
	{
		return PersonalitySystem->GetCurrentEmotionalState().Arousal;
	}
	return 0.5f;
}

float ADeepTreeEchoCharacter::GetEmotionalDominance() const
{
	if (PersonalitySystem)
	{
		return PersonalitySystem->GetCurrentEmotionalState().Dominance;
	}
	return 0.5f;
}

//////////////////////////////////////////////////////////////////////////
// Cognitive State Accessors

float ADeepTreeEchoCharacter::GetCognitiveLoad() const
{
	if (CognitiveState)
	{
		return CognitiveState->GetCurrentCognitiveLoad();
	}
	return 0.0f;
}

float ADeepTreeEchoCharacter::GetAttentionLevel() const
{
	if (CognitiveState)
	{
		return CognitiveState->GetAttentionLevel();
	}
	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////
// Personality Accessors

float ADeepTreeEchoCharacter::GetPersonalityTrait(FName TraitName) const
{
	if (PersonalitySystem)
	{
		return PersonalitySystem->GetTraitValue(TraitName);
	}
	return 0.5f;
}

//////////////////////////////////////////////////////////////////////////
// Behavioral Control

void ADeepTreeEchoCharacter::TriggerEmergentBehavior(FName BehaviorName)
{
	if (PersonalitySystem)
	{
		PersonalitySystem->TriggerEmergentBehavior(BehaviorName);
	}

	// Trigger corresponding animation or effect
	if (AvatarComponent)
	{
		AvatarComponent->PlayGesture(BehaviorName);
	}

	// Play audio feedback
	if (AudioManager)
	{
		AudioManager->PlayGestureSound(BehaviorName);
	}
}

void ADeepTreeEchoCharacter::InterruptCurrentBehavior()
{
	if (PersonalitySystem)
	{
		PersonalitySystem->InterruptCurrentBehavior();
	}

	// Stop current animations
	if (AvatarComponent)
	{
		AvatarComponent->StopAllMontages();
	}
}

//////////////////////////////////////////////////////////////////////////
// Environmental Interaction

void ADeepTreeEchoCharacter::InteractWithObject(AActor* TargetObject)
{
	if (!TargetObject || !InteractionComponent)
	{
		return;
	}

	// Delegate to interaction component
	InteractionComponent->InteractWith(TargetObject);

	// Record interaction in memory
	if (CognitiveState)
	{
		CognitiveState->RecordInteraction(TargetObject);
	}

	// Update personality based on interaction
	if (PersonalitySystem)
	{
		PersonalitySystem->ProcessInteractionFeedback(TargetObject);
	}
}

TArray<AActor*> ADeepTreeEchoCharacter::GetNearbyInteractableObjects(float Radius) const
{
	TArray<AActor*> InteractableObjects;

	if (!InteractionComponent)
	{
		return InteractableObjects;
	}

	// Query interaction component for nearby objects
	InteractableObjects = InteractionComponent->FindInteractableObjects(Radius);

	// Filter based on cognitive state (attention, priorities)
	if (CognitiveState)
	{
		InteractableObjects = CognitiveState->FilterByAttention(InteractableObjects);
	}

	return InteractableObjects;
}

//////////////////////////////////////////////////////////////////////////
// Visual Effects Control

void ADeepTreeEchoCharacter::SetEmotionalAuraColor(FLinearColor Color, float Intensity)
{
	if (AvatarComponent)
	{
		AvatarComponent->SetEmotionalAuraColor(Color);
		AvatarComponent->SetEmotionalAuraIntensity(Intensity);
	}
}

void ADeepTreeEchoCharacter::TriggerCognitiveVisualization(FName VisualizationType)
{
	if (AvatarComponent)
	{
		AvatarComponent->TriggerCognitiveEffect(VisualizationType);
	}
}

//////////////////////////////////////////////////////////////////////////
// Internal Update Functions

void ADeepTreeEchoCharacter::UpdateReplicatedState()
{
	if (!HasAuthority())
	{
		return;
	}

	// Update emotional state
	FEmotionalState EmotionalState = PersonalitySystem ? PersonalitySystem->GetCurrentEmotionalState() : FEmotionalState();
	ReplicatedState.Valence = FMath::Clamp(FMath::RoundToInt(EmotionalState.Valence * 255.0f), 0, 255);
	ReplicatedState.Arousal = FMath::Clamp(FMath::RoundToInt(EmotionalState.Arousal * 255.0f), 0, 255);
	ReplicatedState.Dominance = FMath::Clamp(FMath::RoundToInt(EmotionalState.Dominance * 255.0f), 0, 255);

	// Update cognitive load
	float CogLoad = CognitiveState ? CognitiveState->GetCurrentCognitiveLoad() : 0.0f;
	ReplicatedState.CognitiveLoad = FMath::Clamp(FMath::RoundToInt(CogLoad * 255.0f), 0, 255);

	// Update personality traits
	if (PersonalitySystem)
	{
		ReplicatedState.Openness = FMath::Clamp(FMath::RoundToInt(PersonalitySystem->GetTraitValue(TEXT("Openness")) * 255.0f), 0, 255);
		ReplicatedState.Conscientiousness = FMath::Clamp(FMath::RoundToInt(PersonalitySystem->GetTraitValue(TEXT("Conscientiousness")) * 255.0f), 0, 255);
		ReplicatedState.Extraversion = FMath::Clamp(FMath::RoundToInt(PersonalitySystem->GetTraitValue(TEXT("Extraversion")) * 255.0f), 0, 255);
		ReplicatedState.Agreeableness = FMath::Clamp(FMath::RoundToInt(PersonalitySystem->GetTraitValue(TEXT("Agreeableness")) * 255.0f), 0, 255);
		ReplicatedState.Neuroticism = FMath::Clamp(FMath::RoundToInt(PersonalitySystem->GetTraitValue(TEXT("Neuroticism")) * 255.0f), 0, 255);
	}
}

void ADeepTreeEchoCharacter::UpdateComponentSynchronization()
{
	// Synchronize neurochemical effects with personality
	if (NeurochemicalSystem && PersonalitySystem)
	{
		FNeurochemicalState Neurochem = NeurochemicalSystem->GetCurrentState();
		PersonalitySystem->ApplyNeurochemicalInfluence(Neurochem);
	}

	// Synchronize cognitive state with avatar visuals
	if (CognitiveState && AvatarComponent)
	{
		float CogLoad = CognitiveState->GetCurrentCognitiveLoad();
		AvatarComponent->SetCognitiveLoadVisualization(CogLoad);
	}

	// Synchronize personality with movement
	if (PersonalitySystem && GetCharacterMovement())
	{
		UDeepTreeEchoMovementComponent* DTEMovement = Cast<UDeepTreeEchoMovementComponent>(GetCharacterMovement());
		if (DTEMovement)
		{
			FEmotionalState EmotionalState = PersonalitySystem->GetCurrentEmotionalState();
			DTEMovement->SetEmotionalState(EmotionalState);
		}
	}
}

void ADeepTreeEchoCharacter::UpdateEmotionalEffects()
{
	if (!AvatarComponent || !PersonalitySystem)
	{
		return;
	}

	// Get current emotional state
	FEmotionalState EmotionalState = PersonalitySystem->GetCurrentEmotionalState();

	// Update avatar visuals based on emotion
	AvatarComponent->SetEmotionalState(EmotionalState);

	// Update audio based on emotion
	if (AudioManager)
	{
		AudioManager->SetEmotionalState(EmotionalState);
	}

	// Update cosmetics based on emotion
	if (CosmeticsComponent)
	{
		CosmeticsComponent->ApplyEmotionalEffects(EmotionalState);
	}
}

void ADeepTreeEchoCharacter::UpdateCognitiveVisualization()
{
	VisualizationUpdateTimer += GetWorld()->GetDeltaSeconds();
	
	if (VisualizationUpdateTimer < 0.5f) // Update twice per second
	{
		return;
	}

	VisualizationUpdateTimer = 0.0f;

	if (!AvatarComponent || !CognitiveState)
	{
		return;
	}

	// Update cognitive visualization effects
	float CogLoad = CognitiveState->GetCurrentCognitiveLoad();
	AvatarComponent->UpdateCognitiveVisualization(CogLoad);
}

//////////////////////////////////////////////////////////////////////////
// Network Replication

void ADeepTreeEchoCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADeepTreeEchoCharacter, ReplicatedState);
}

//////////////////////////////////////////////////////////////////////////
// Component Initialization

void ADeepTreeEchoCharacter::InitializeComponents()
{
	// Initialize personality system
	if (PersonalitySystem)
	{
		PersonalitySystem->Initialize();
	}

	// Initialize cognitive state
	if (CognitiveState)
	{
		CognitiveState->Initialize();
	}

	// Initialize neurochemical simulation
	if (NeurochemicalSystem)
	{
		NeurochemicalSystem->Initialize();
	}

	// Initialize avatar component
	if (AvatarComponent)
	{
		AvatarComponent->Initialize(GetMesh());
	}

	// Initialize interaction component
	if (InteractionComponent)
	{
		InteractionComponent->Initialize(this);
	}

	// Initialize cosmetics component
	if (CosmeticsComponent)
	{
		CosmeticsComponent->Initialize(GetMesh());
	}

	// Initialize audio manager
	if (AudioManager)
	{
		AudioManager->Initialize();
	}
}

void ADeepTreeEchoCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Initialize ability system component
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// Grant default abilities (personality traits as abilities)
	if (PersonalitySystem)
	{
		PersonalitySystem->GrantAbilities(AbilitySystemComponent);
	}
}

//////////////////////////////////////////////////////////////////////////
// Event Handlers

void ADeepTreeEchoCharacter::OnPersonalityShift(const FPersonalityState& OldState, const FPersonalityState& NewState)
{
	// Update visuals based on personality shift
	if (AvatarComponent)
	{
		AvatarComponent->OnPersonalityChanged(NewState);
	}

	// Update cosmetics
	if (CosmeticsComponent)
	{
		CosmeticsComponent->ApplyPersonalityOutfit(NewState);
	}

	// Play audio feedback
	if (AudioManager)
	{
		AudioManager->OnPersonalityShift(OldState, NewState);
	}

	// Broadcast to blueprints
	OnPersonalityShiftEvent.Broadcast(OldState, NewState);
}

void ADeepTreeEchoCharacter::OnEmotionalStateChanged(const FEmotionalState& NewState)
{
	// Update avatar visuals
	if (AvatarComponent)
	{
		AvatarComponent->SetEmotionalState(NewState);
	}

	// Update audio
	if (AudioManager)
	{
		AudioManager->SetEmotionalState(NewState);
	}
}

void ADeepTreeEchoCharacter::OnCognitiveLoadChanged(float NewLoad)
{
	CachedCognitiveLoad = NewLoad;

	// Update visualization
	if (AvatarComponent)
	{
		AvatarComponent->SetCognitiveLoadVisualization(NewLoad);
	}

	// Affect movement speed
	if (GetCharacterMovement())
	{
		UDeepTreeEchoMovementComponent* DTEMovement = Cast<UDeepTreeEchoMovementComponent>(GetCharacterMovement());
		if (DTEMovement)
		{
			DTEMovement->SetCognitiveLoad(NewLoad);
		}
	}
}

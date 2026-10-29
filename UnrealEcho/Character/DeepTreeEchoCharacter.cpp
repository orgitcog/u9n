// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Advanced Character Implementation

#include "DeepTreeEchoCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "UObject/Class.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Deep Tree Echo components
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Avatar/Avatar3DComponent.h"
#include "Avatar/FacialAnimationSystem.h"
#include "Avatar/GestureSystem.h"
#include "Avatar/EmotionalAuraComponent.h"
#include "Avatar/CognitiveVisualizationComponent.h"
#include "Avatar/DeepTreeEchoExpressionSystem.h"
#include "Audio/AudioManagerComponent.h"
#include "Interaction/DeepTreeEchoInteractionComponent.h"
#include "Cosmetics/DeepTreeEchoCosmeticsComponent.h"
#include "Character/DeepTreeEchoMovementComponent.h"
#include "DTELanguageModelBridge.h"

namespace
{
	/** Chaotic event the personality system publishes for a listener with a gesture library. */
	const FName ChaoticEventUnpredictableGesture(TEXT("unpredictable_gesture"));

	/**
	 * Deterministic mapping from the character's measured affect to the music/ambience
	 * layer the audio manager mixes. This drives the soundtrack only; it never triggers
	 * a sound presented as the character's own reaction.
	 */
	EAvatarEmotionalState MapAffectToMusicLayer(const FDeepTreeEchoAffect& Affect, float& OutIntensity)
	{
		const float ValenceOffset = Affect.Valence - 0.5f;
		const float ArousalOffset = Affect.Arousal - 0.5f;

		OutIntensity = Affect.Intensity;

		if (OutIntensity < 0.15f)
		{
			return EAvatarEmotionalState::Neutral;
		}

		if (ValenceOffset >= 0.0f)
		{
			return ArousalOffset > 0.15f ? EAvatarEmotionalState::Excited : EAvatarEmotionalState::Happy;
		}

		if (ArousalOffset > 0.15f)
		{
			return Affect.Dominance > 0.5f ? EAvatarEmotionalState::Angry : EAvatarEmotionalState::Surprised;
		}

		return EAvatarEmotionalState::Sad;
	}

	/** Comma-separated names of a reflected enum, without the generated _MAX sentinel. */
	FString EnumMenu(const UEnum* Enum)
	{
		FString Menu;
		if (!Enum)
		{
			return Menu;
		}
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			if (!Menu.IsEmpty())
			{
				Menu += TEXT(", ");
			}
			Menu += Enum->GetNameStringByIndex(Index);
		}
		return Menu;
	}

	/**
	 * Pull the JSON object out of a model reply (tolerating text around it).
	 * Returns nullptr when the reply holds no parsable object.
	 */
	TSharedPtr<FJsonObject> ExtractJsonObject(const FString& ModelText)
	{
		int32 Open = INDEX_NONE;
		int32 Close = INDEX_NONE;
		ModelText.FindChar(TEXT('{'), Open);
		ModelText.FindLastChar(TEXT('}'), Close);
		if (Open == INDEX_NONE || Close == INDEX_NONE || Close <= Open)
		{
			return nullptr;
		}

		const FString JsonText = ModelText.Mid(Open, Close - Open + 1);
		TSharedPtr<FJsonObject> Object;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			return nullptr;
		}
		return Object;
	}
}

//////////////////////////////////////////////////////////////////////////
// FDeepTreeEchoAffect

FDeepTreeEchoAffect FDeepTreeEchoAffect::FromChemistry(const FEmotionalChemistry& Chemistry)
{
	// Chemistry readings are 0..1 with a ~0.5 baseline; offsets from baseline move the dimensions.
	const float Happiness = FMath::Clamp(Chemistry.Happiness, 0.0f, 1.0f) - 0.5f;
	const float Excitement = FMath::Clamp(Chemistry.Excitement, 0.0f, 1.0f) - 0.5f;
	const float Calmness = FMath::Clamp(Chemistry.Calmness, 0.0f, 1.0f) - 0.5f;
	const float Anxiety = FMath::Clamp(Chemistry.Anxiety, 0.0f, 1.0f) - 0.5f;
	const float Affection = FMath::Clamp(Chemistry.Affection, 0.0f, 1.0f) - 0.5f;
	const float Focus = FMath::Clamp(Chemistry.Focus, 0.0f, 1.0f) - 0.5f;

	FDeepTreeEchoAffect Affect;
	Affect.Valence = FMath::Clamp(0.5f + 0.6f * Happiness + 0.3f * Affection - 0.5f * Anxiety, 0.0f, 1.0f);
	Affect.Arousal = FMath::Clamp(0.5f + 0.6f * Excitement + 0.4f * Anxiety - 0.5f * Calmness, 0.0f, 1.0f);
	Affect.Dominance = FMath::Clamp(0.5f + 0.5f * Focus + 0.3f * Happiness - 0.5f * Anxiety, 0.0f, 1.0f);
	Affect.Intensity = FMath::Clamp(FMath::Max(FMath::Abs(Affect.Valence - 0.5f), FMath::Abs(Affect.Arousal - 0.5f)) * 2.0f, 0.0f, 1.0f);
	return Affect;
}

FString FDeepTreeEchoAffect::Describe() const
{
	return FString::Printf(TEXT("valence %.2f, arousal %.2f, dominance %.2f (intensity %.2f)"), Valence, Arousal, Dominance, Intensity);
}

//////////////////////////////////////////////////////////////////////////
// FDeepTreeEchoReplicatedState

bool FDeepTreeEchoReplicatedState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	Ar << Valence;
	Ar << Arousal;
	Ar << Dominance;
	Ar << CognitiveLoad;
	for (uint8& Trait : TraitIntensities)
	{
		Ar << Trait;
	}
	Ar << BehaviorFlags;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// ADeepTreeEchoCharacter

ADeepTreeEchoCharacter::ADeepTreeEchoCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDeepTreeEchoMovementComponent>(ACharacter::CharacterMovementComponentName))
	, bHasAppliedFacialEmotion(false)
	, LastFacialEmotion(EEmotionState::Neutral)
	, bHasAppliedAuraEmotion(false)
	, LastAuraEmotion(EAuraEmotionType::Neutral)
	, bBehaviorRequestPending(false)
	, PendingUtteranceRequests(0)
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

	// Create neurochemical simulation (the character's affect source)
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
}

void ADeepTreeEchoCharacter::UnPossessed()
{
	Super::UnPossessed();
}

void ADeepTreeEchoCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The model bridge is the only source of words and of behaviour choices
	ModelBridge = UDTELanguageModelBridge::FindOrAdd(this);
	if (!ModelBridge)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: no language model bridge could be created; the character will stay silent"), *GetName());
	}

	// Optional expression system (added in Blueprint) drives the face from affect and model choices
	ExpressionSystem = FindComponentByClass<UDeepTreeEchoExpressionSystem>();

	// Initialize all components
	InitializeComponents();

	// Chaotic events chosen by the model arrive here; gesture events are routed back through the model
	if (PersonalitySystem)
	{
		PersonalitySystem->OnChaoticEventChosen.AddDynamic(this, &ADeepTreeEchoCharacter::HandleChaoticEventChosen);
	}

	// The face starts neutral; the first UpdateEmotionalEffects applies the measured affect.
}

void ADeepTreeEchoCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PersonalitySystem)
	{
		PersonalitySystem->OnChaoticEventChosen.RemoveDynamic(this, &ADeepTreeEchoCharacter::HandleChaoticEventChosen);
	}

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
// Affect Accessors

FDeepTreeEchoAffect ADeepTreeEchoCharacter::GetAffect() const
{
	if (NeurochemicalSystem)
	{
		return FDeepTreeEchoAffect::FromChemistry(NeurochemicalSystem->GetEmotionalChemistry());
	}
	// No affect source: neutral, nothing is synthesized
	return FDeepTreeEchoAffect();
}

float ADeepTreeEchoCharacter::GetEmotionalValence() const
{
	return GetAffect().Valence;
}

float ADeepTreeEchoCharacter::GetEmotionalArousal() const
{
	return GetAffect().Arousal;
}

float ADeepTreeEchoCharacter::GetEmotionalDominance() const
{
	return GetAffect().Dominance;
}

//////////////////////////////////////////////////////////////////////////
// Cognitive State Accessors

float ADeepTreeEchoCharacter::GetCognitiveLoad() const
{
	if (NeurochemicalSystem)
	{
		return FMath::Clamp(1.0f - NeurochemicalSystem->GetCognitiveClarity(), 0.0f, 1.0f);
	}
	return 0.0f;
}

float ADeepTreeEchoCharacter::GetAttentionLevel() const
{
	if (ExpressionSystem)
	{
		return FMath::Clamp(ExpressionSystem->GetAttentionLevel(), 0.0f, 1.0f);
	}
	if (NeurochemicalSystem)
	{
		return FMath::Clamp(NeurochemicalSystem->GetEmotionalChemistry().Focus, 0.0f, 1.0f);
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
// Personality Accessors

float ADeepTreeEchoCharacter::GetPersonalityTrait(FName TraitName) const
{
	if (!PersonalitySystem || TraitName.IsNone())
	{
		return 0.0f;
	}

	const UEnum* TraitEnum = StaticEnum<EPersonalityTraitType>();
	const int64 Value = TraitEnum ? TraitEnum->GetValueByNameString(TraitName.ToString()) : INDEX_NONE;
	if (Value == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoCharacter: \"%s\" is not an EPersonalityTraitType"), *TraitName.ToString());
		return 0.0f;
	}

	return PersonalitySystem->GetTraitIntensity(static_cast<EPersonalityTraitType>(Value));
}

//////////////////////////////////////////////////////////////////////////
// Behavioral Control

void ADeepTreeEchoCharacter::TriggerEmergentBehavior(FName BehaviorName)
{
	if (BehaviorName.IsNone())
	{
		return;
	}

	if (!ModelBridge)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: behaviour \"%s\" skipped, no model bridge to choose a gesture"),
			*GetName(), *BehaviorName.ToString());
		return;
	}

	if (bBehaviorRequestPending)
	{
		UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoCharacter %s: behaviour \"%s\" skipped, a gesture choice is already in flight"),
			*GetName(), *BehaviorName.ToString());
		return;
	}

	const FString Instruction = TEXT(
		"The character is about to act on the behaviour named in the context. Choose the single gesture from the gesture menu "
		"that fits this behaviour and her current state, or \"none\" if no gesture fits. "
		"Answer with one JSON object: {\"gesture\": <a name from the gesture menu or none>, \"intensity\": <0.0-1.0>}.");

	const FString Context = FString::Printf(TEXT("behaviour: %s\ngesture menu: %s\n%s"),
		*BehaviorName.ToString(), *EnumMenu(StaticEnum<EGestureType>()), *DescribeStateForModel());

	bBehaviorRequestPending = true;

	TWeakObjectPtr<ADeepTreeEchoCharacter> WeakThis(this);
	ModelBridge->RequestJson(Instruction, Context,
		[WeakThis, BehaviorName](bool bSuccess, const FString& TextOrError)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			ADeepTreeEchoCharacter* Self = WeakThis.Get();
			Self->bBehaviorRequestPending = false;

			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: behaviour \"%s\" skipped, model unavailable: %s"),
					*Self->GetName(), *BehaviorName.ToString(), *TextOrError);
				return;
			}

			TSharedPtr<FJsonObject> Object = ExtractJsonObject(TextOrError);
			FString GestureText;
			if (!Object.IsValid() || !Object->TryGetStringField(TEXT("gesture"), GestureText))
			{
				UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: behaviour \"%s\" skipped, model reply was not the requested JSON object"),
					*Self->GetName(), *BehaviorName.ToString());
				return;
			}

			GestureText.TrimStartAndEndInline();
			if (GestureText.IsEmpty() || GestureText.Equals(TEXT("none"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCharacter %s: model chose no gesture for behaviour \"%s\""),
					*Self->GetName(), *BehaviorName.ToString());
				Self->OnBehaviorChosen.Broadcast(BehaviorName, NAME_None, 0.0f);
				return;
			}

			const int64 Value = StaticEnum<EGestureType>()->GetValueByNameString(GestureText);
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: behaviour \"%s\" skipped, \"%s\" is not on the gesture menu"),
					*Self->GetName(), *BehaviorName.ToString(), *GestureText);
				return;
			}

			double Intensity = 1.0;
			Object->TryGetNumberField(TEXT("intensity"), Intensity);

			Self->ApplyBehavior(BehaviorName, static_cast<EGestureType>(Value), FMath::Clamp(static_cast<float>(Intensity), 0.0f, 1.0f));
		});
}

void ADeepTreeEchoCharacter::ApplyBehavior(FName BehaviorName, EGestureType Gesture, float Intensity)
{
	const FName GestureName(*StaticEnum<EGestureType>()->GetNameStringByValue(static_cast<int64>(Gesture)));

	if (AvatarComponent && AvatarComponent->GestureSystem)
	{
		AvatarComponent->GestureSystem->TriggerGesture(Gesture, Intensity);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: no gesture system on the avatar to play %s"), *GetName(), *GestureName.ToString());
	}

	// The gesture's own foley, if the operator assigned a sound asset to this gesture name
	if (AudioManager)
	{
		AudioManager->PlayGestureSound(GestureName);
	}

	// Let the model also pick the face for this moment when an expression system is present
	if (ExpressionSystem)
	{
		ExpressionSystem->RequestExpressionForEvent(FString::Printf(TEXT("behaviour \"%s\" with gesture %s"), *BehaviorName.ToString(), *GestureName.ToString()));
	}

	OnBehaviorChosen.Broadcast(BehaviorName, GestureName, Intensity);
}

void ADeepTreeEchoCharacter::InterruptCurrentBehavior()
{
	// Stop current gesture montages on the character mesh and the avatar mesh
	if (UAnimInstance* MeshAnim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		MeshAnim->Montage_Stop(0.2f);
	}
	if (UAnimInstance* AvatarAnim = AvatarComponent ? AvatarComponent->GetAnimInstance() : nullptr)
	{
		AvatarAnim->Montage_Stop(0.2f);
	}

	// Stop speech that is playing
	if (AudioManager)
	{
		AudioManager->StopSpeaking();
	}

	if (InteractionComponent && InteractionComponent->IsInteracting())
	{
		InteractionComponent->CancelInteraction();
	}
}

//////////////////////////////////////////////////////////////////////////
// Speech

bool ADeepTreeEchoCharacter::RequestUtterance(const FString& Instruction, const FString& Context)
{
	if (!ModelBridge)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: utterance not requested, no model bridge"), *GetName());
		return false;
	}

	if (!AudioManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: utterance not requested, no audio manager to speak through"), *GetName());
		return false;
	}

	if (!AudioManager->IsSpeechSynthesizerBound())
	{
		UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: utterance not requested, no speech synthesizer is bound so nothing could be heard"), *GetName());
		return false;
	}

	FString FullContext = DescribeStateForModel();
	if (!Context.IsEmpty())
	{
		FullContext = Context + TEXT("\n") + FullContext;
	}

	++PendingUtteranceRequests;

	TWeakObjectPtr<ADeepTreeEchoCharacter> WeakThis(this);
	ModelBridge->RequestText(Instruction, FullContext,
		[WeakThis, Instruction](bool bSuccess, const FString& TextOrError)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			ADeepTreeEchoCharacter* Self = WeakThis.Get();
			Self->PendingUtteranceRequests = FMath::Max(0, Self->PendingUtteranceRequests - 1);

			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: nothing said, model unavailable: %s"), *Self->GetName(), *TextOrError);
				return;
			}

			const FString Text = TextOrError.TrimStartAndEnd();
			if (Text.IsEmpty() || !Self->AudioManager)
			{
				return;
			}

			// Speech is only ever synthesized audio of the model's words
			const FString RequestId = Self->AudioManager->Speak(Text, Self->GetAffect().Arousal);
			if (RequestId.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter %s: model text could not be handed to speech synthesis"), *Self->GetName());
				return;
			}

			// The model also chooses the face for its own words when an expression system is present
			if (Self->ExpressionSystem)
			{
				Self->ExpressionSystem->RequestExpressionForUtterance(Text, Instruction);
			}

			Self->OnUtteranceRequested.Broadcast(Text, RequestId);
		});

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Environmental Interaction

void ADeepTreeEchoCharacter::InteractWithObject(AActor* TargetObject)
{
	if (!TargetObject || !InteractionComponent)
	{
		return;
	}

	// The interaction component records the interaction and its cognitive/emotional impact itself
	const FInteractionResult Result = InteractionComponent->InteractWith(TargetObject);

	// Feed the interaction's measured emotional impact into the neurochemistry (the affect source)
	if (NeurochemicalSystem && Result.bSuccess)
	{
		NeurochemicalSystem->SimulateSocialInteraction(FMath::Clamp(0.5f + 0.5f * Result.EmotionalImpact, 0.0f, 1.0f), 1.0f);
		if (Result.CognitiveImpact > 0.0f)
		{
			NeurochemicalSystem->SimulateCognitiveTask(FMath::Clamp(Result.CognitiveImpact, 0.0f, 1.0f), 1.0f);
		}
	}
}

TArray<AActor*> ADeepTreeEchoCharacter::GetNearbyInteractableObjects(float Radius) const
{
	if (!InteractionComponent)
	{
		return TArray<AActor*>();
	}

	// The interaction component already weighs candidates by attention and personality
	return InteractionComponent->FindInteractableObjects(Radius);
}

//////////////////////////////////////////////////////////////////////////
// Visual Effects Control

void ADeepTreeEchoCharacter::SetEmotionalAuraColor(FLinearColor Color, float Intensity)
{
	if (AvatarComponent && AvatarComponent->EmotionalAura)
	{
		AvatarComponent->EmotionalAura->SetCustomAuraColors(Color, Color);
		AvatarComponent->EmotionalAura->SetAuraIntensity(Intensity);
	}

	if (CosmeticsComponent)
	{
		CosmeticsComponent->SetEmotionalAura(Color, Intensity);
	}
}

void ADeepTreeEchoCharacter::TriggerCognitiveVisualization(FName VisualizationType)
{
	if (!AvatarComponent || !AvatarComponent->CognitiveViz || VisualizationType.IsNone())
	{
		return;
	}

	const UEnum* ActivityEnum = StaticEnum<ECognitiveActivityType>();
	const int64 Value = ActivityEnum ? ActivityEnum->GetValueByNameString(VisualizationType.ToString()) : INDEX_NONE;
	if (Value == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCharacter: \"%s\" is not an ECognitiveActivityType"), *VisualizationType.ToString());
		return;
	}

	AvatarComponent->CognitiveViz->SetCognitiveActivity(static_cast<ECognitiveActivityType>(Value), 1.0f);
}

//////////////////////////////////////////////////////////////////////////
// Internal Update Functions

void ADeepTreeEchoCharacter::UpdateReplicatedState()
{
	if (!HasAuthority())
	{
		return;
	}

	// Affect
	const FDeepTreeEchoAffect Affect = GetAffect();
	ReplicatedState.Valence = FMath::Clamp(FMath::RoundToInt(Affect.Valence * 255.0f), 0, 255);
	ReplicatedState.Arousal = FMath::Clamp(FMath::RoundToInt(Affect.Arousal * 255.0f), 0, 255);
	ReplicatedState.Dominance = FMath::Clamp(FMath::RoundToInt(Affect.Dominance * 255.0f), 0, 255);

	// Cognitive load
	ReplicatedState.CognitiveLoad = FMath::Clamp(FMath::RoundToInt(GetCognitiveLoad() * 255.0f), 0, 255);

	// Personality traits
	if (PersonalitySystem)
	{
		const UEnum* TraitEnum = StaticEnum<EPersonalityTraitType>();
		const int32 TraitCount = TraitEnum ? FMath::Min(TraitEnum->NumEnums() - 1, (int32)UE_ARRAY_COUNT(ReplicatedState.TraitIntensities)) : 0;
		for (int32 Index = 0; Index < TraitCount; ++Index)
		{
			const EPersonalityTraitType Trait = static_cast<EPersonalityTraitType>(TraitEnum->GetValueByIndex(Index));
			ReplicatedState.TraitIntensities[Index] = FMath::Clamp(FMath::RoundToInt(PersonalitySystem->GetTraitIntensity(Trait) * 255.0f), 0, 255);
		}
	}

	// Behaviour flags
	uint8 Flags = 0;
	if (AudioManager && AudioManager->IsSpeaking())
	{
		Flags |= EDeepTreeEchoBehaviorFlags::Speaking;
	}
	if (AvatarComponent && AvatarComponent->GestureSystem && AvatarComponent->GestureSystem->IsGesturePlaying())
	{
		Flags |= EDeepTreeEchoBehaviorFlags::GesturePlaying;
	}
	if (bBehaviorRequestPending)
	{
		Flags |= EDeepTreeEchoBehaviorFlags::BehaviorPending;
	}
	if (PendingUtteranceRequests > 0)
	{
		Flags |= EDeepTreeEchoBehaviorFlags::UtterancePending;
	}
	ReplicatedState.BehaviorFlags = Flags;
}

void ADeepTreeEchoCharacter::UpdateComponentSynchronization()
{
	// The personality system reads the neurochemical state itself; nothing to push.

	// Synchronize cognitive load with movement responsiveness
	if (UDeepTreeEchoMovementComponent* DTEMovement = Cast<UDeepTreeEchoMovementComponent>(GetCharacterMovement()))
	{
		DTEMovement->SetCognitiveLoad(GetCognitiveLoad());
	}
}

void ADeepTreeEchoCharacter::UpdateEmotionalEffects()
{
	if (!NeurochemicalSystem)
	{
		// No affect source: nothing is shown as a reaction
		return;
	}

	const FDeepTreeEchoAffect Affect = GetAffect();

	// Face: when an expression system is present it derives the face from the same chemistry itself.
	if (!ExpressionSystem && AvatarComponent && AvatarComponent->FacialSystem)
	{
		const EEmotionState FacialEmotion = MapAffectToFacialEmotion(Affect);
		if (!bHasAppliedFacialEmotion || FacialEmotion != LastFacialEmotion)
		{
			AvatarComponent->FacialSystem->SetEmotionState(FacialEmotion, 0.5f);
			LastFacialEmotion = FacialEmotion;
			bHasAppliedFacialEmotion = true;
		}
	}

	// Aura: discrete emotion from affect, intensity from how far the affect is from neutral
	if (AvatarComponent && AvatarComponent->EmotionalAura)
	{
		const EAuraEmotionType AuraEmotion = MapAffectToAuraEmotion(Affect);
		if (!bHasAppliedAuraEmotion || AuraEmotion != LastAuraEmotion)
		{
			AvatarComponent->EmotionalAura->SetEmotionalState(AuraEmotion, 1.0f);
			LastAuraEmotion = AuraEmotion;
			bHasAppliedAuraEmotion = true;
		}
		AvatarComponent->EmotionalAura->SetAuraIntensity(Affect.Intensity);
	}

	// Soundtrack follows the measured affect (music layer, not a reaction cue)
	if (AudioManager)
	{
		float MusicIntensity = 0.0f;
		const EAvatarEmotionalState MusicLayer = MapAffectToMusicLayer(Affect, MusicIntensity);
		AudioManager->UpdateMusicLayers(MusicLayer, MusicIntensity);
	}

	// Cosmetics: the same deterministic affect -> material mappings the component uses internally
	if (CosmeticsComponent)
	{
		CosmeticsComponent->SetBlushIntensity(FMath::Clamp(Affect.Arousal * Affect.Valence, 0.0f, 1.0f));
		CosmeticsComponent->SetEyeSparkle(FMath::Clamp(Affect.Valence * Affect.Arousal, 0.0f, 1.0f));
		CosmeticsComponent->SetHairShimmer(FMath::Clamp(Affect.Dominance * Affect.Valence * 0.8f, 0.0f, 1.0f));
		CosmeticsComponent->SetSkinGlow(FMath::Clamp(Affect.Valence * 0.5f, 0.0f, 1.0f));
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

	const float CogLoad = GetCognitiveLoad();

	if (AvatarComponent && AvatarComponent->CognitiveViz)
	{
		AvatarComponent->CognitiveViz->SetVisualizationIntensity(CogLoad);
	}

	if (CosmeticsComponent)
	{
		CosmeticsComponent->SetCognitiveVisualization(CogLoad);
	}

	// Ambience follows measured load and the live chaos trait
	if (AudioManager)
	{
		const float ChaosFactor = PersonalitySystem ? PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::HyperChaotic) : 0.0f;
		AudioManager->UpdateAmbientSound(CogLoad, ChaosFactor);
	}
}

FString ADeepTreeEchoCharacter::DescribeStateForModel() const
{
	FString Description = FString::Printf(TEXT("affect: %s"), *GetAffect().Describe());

	if (NeurochemicalSystem)
	{
		const FEmotionalChemistry Chemistry = NeurochemicalSystem->GetEmotionalChemistry();
		Description += FString::Printf(TEXT("\nchemistry: happiness %.2f, excitement %.2f, calmness %.2f, anxiety %.2f, affection %.2f, focus %.2f"),
			Chemistry.Happiness, Chemistry.Excitement, Chemistry.Calmness, Chemistry.Anxiety, Chemistry.Affection, Chemistry.Focus);
	}

	Description += FString::Printf(TEXT("\ncognitive load %.2f, attention %.2f"), GetCognitiveLoad(), GetAttentionLevel());

	if (PersonalitySystem)
	{
		const UEnum* TraitEnum = StaticEnum<EPersonalityTraitType>();
		FString Traits;
		for (EPersonalityTraitType Trait : PersonalitySystem->GetDominantTraits(3))
		{
			if (!Traits.IsEmpty())
			{
				Traits += TEXT(", ");
			}
			Traits += FString::Printf(TEXT("%s %.2f"),
				TraitEnum ? *TraitEnum->GetNameStringByValue(static_cast<int64>(Trait)) : TEXT("?"),
				PersonalitySystem->GetTraitIntensity(Trait));
		}
		Description += FString::Printf(TEXT("\ndominant traits: %s"), *Traits);
	}

	if (AudioManager)
	{
		Description += AudioManager->IsSpeaking() ? TEXT("\ncurrently speaking") : TEXT("\ncurrently silent");
	}

	return Description;
}

//////////////////////////////////////////////////////////////////////////
// Affect -> discrete emotion mappings

EEmotionState ADeepTreeEchoCharacter::MapAffectToFacialEmotion(const FDeepTreeEchoAffect& Affect)
{
	if (Affect.Intensity < 0.15f)
	{
		return EEmotionState::Neutral;
	}

	if (Affect.Valence >= 0.5f)
	{
		if (Affect.Arousal <= 0.65f && Affect.Dominance > 0.6f)
		{
			return EEmotionState::Confident;
		}
		return EEmotionState::Happy;
	}

	if (Affect.Arousal > 0.65f)
	{
		return Affect.Dominance > 0.5f ? EEmotionState::Angry : EEmotionState::Fearful;
	}

	return EEmotionState::Sad;
}

EAuraEmotionType ADeepTreeEchoCharacter::MapAffectToAuraEmotion(const FDeepTreeEchoAffect& Affect)
{
	if (Affect.Intensity < 0.15f)
	{
		return EAuraEmotionType::Neutral;
	}

	if (Affect.Valence >= 0.5f)
	{
		if (Affect.Arousal > 0.65f)
		{
			return EAuraEmotionType::Excitement;
		}
		if (Affect.Arousal < 0.35f)
		{
			return EAuraEmotionType::Calm;
		}
		return Affect.Dominance > 0.6f ? EAuraEmotionType::Confidence : EAuraEmotionType::Joy;
	}

	if (Affect.Arousal > 0.65f)
	{
		return Affect.Dominance > 0.5f ? EAuraEmotionType::Anger : EAuraEmotionType::Fear;
	}

	return EAuraEmotionType::Sadness;
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
	// The personality, neurochemical and avatar components set themselves up in their own
	// BeginPlay; only the components with an explicit initialization entry point are called.

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

	// The audio manager creates its voice, music and ambience components in its own
	// BeginPlay; it has no separate initialization entry point.
}

void ADeepTreeEchoCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Initialize ability system component; abilities are granted by gameplay data, not by personality
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

//////////////////////////////////////////////////////////////////////////
// Event Handlers

void ADeepTreeEchoCharacter::HandleChaoticEventChosen(FName EventName, float Intensity)
{
	// The personality system has already dispatched the event to its own listeners. The one
	// event it cannot act on itself (it has no gesture library) comes back through the model,
	// which chooses the gesture for it.
	if (EventName == ChaoticEventUnpredictableGesture)
	{
		TriggerEmergentBehavior(EventName);
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoCharacter %s: chaotic event %s (intensity %.2f)"), *GetName(), *EventName.ToString(), Intensity);
}

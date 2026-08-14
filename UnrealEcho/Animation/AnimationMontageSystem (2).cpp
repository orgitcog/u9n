// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Animation Montage System Implementation

#include "AnimationMontageSystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Cognitive/EmotionalState.h"

//////////////////////////////////////////////////////////////////////////
// UMontageLibraryDataAsset

FMontageEntry UMontageLibraryDataAsset::GetMontageById(FName MontageId) const
{
	for (const FMontageEntry& Entry : Montages)
	{
		if (Entry.MontageId == MontageId)
		{
			return Entry;
		}
	}
	return FMontageEntry();
}

TArray<FMontageEntry> UMontageLibraryDataAsset::GetMontagesByCategory(EMontageCategory Category) const
{
	TArray<FMontageEntry> Result;
	for (const FMontageEntry& Entry : Montages)
	{
		if (Entry.Category == Category)
		{
			Result.Add(Entry);
		}
	}
	return Result;
}

TArray<FMontageEntry> UMontageLibraryDataAsset::GetMontagesByTag(FGameplayTag Tag) const
{
	TArray<FMontageEntry> Result;
	for (const FMontageEntry& Entry : Montages)
	{
		if (Entry.MontageTags.HasTag(Tag))
		{
			Result.Add(Entry);
		}
	}
	return Result;
}

//////////////////////////////////////////////////////////////////////////
// UAnimationMontageSystem

UAnimationMontageSystem::UAnimationMontageSystem()
	: DefaultBlendInTime(0.25f)
	, DefaultBlendOutTime(0.25f)
	, CurrentMontagePriority(-1)
	, PersonalityExtraversion(0.5f)
	, PersonalityOpenness(0.5f)
	, PersonalityConscientiousness(0.5f)
	, PersonalityAgreeableness(0.5f)
	, PersonalityNeuroticism(0.5f)
	, EmotionalValence(0.0f)
	, EmotionalArousal(0.5f)
	, EmotionalDominance(0.5f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz for cooldown updates
}

void UAnimationMontageSystem::BeginPlay()
{
	Super::BeginPlay();

	// Use default library if set
	if (DefaultMontageLibrary)
	{
		CurrentLibrary = DefaultMontageLibrary;
	}
}

void UAnimationMontageSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCooldowns(DeltaTime);
}

void UAnimationMontageSystem::Initialize(UAnimInstance* InAnimInstance)
{
	AnimInstance = InAnimInstance;

	if (AnimInstance)
	{
		// Bind to montage ended event
		MontageEndedHandle = AnimInstance->OnMontageEnded.AddUObject(this, &UAnimationMontageSystem::OnMontageEndedInternal);
	}
}

void UAnimationMontageSystem::SetMontageLibrary(UMontageLibraryDataAsset* Library)
{
	CurrentLibrary = Library;
}

bool UAnimationMontageSystem::PlayMontage(FName MontageId, const FMontagePlayRequest& Request)
{
	if (!CurrentLibrary || !AnimInstance)
	{
		return false;
	}

	FMontageEntry Entry = CurrentLibrary->GetMontageById(MontageId);
	if (Entry.MontageId.IsNone() || Entry.Variants.Num() == 0)
	{
		return false;
	}

	// Check if we can play
	if (!CanPlayMontage(Entry, Request.bForcePlay))
	{
		return false;
	}

	// Select best variant based on personality/emotion
	FMontageVariant BestVariant = SelectBestVariant(Entry);

	// Load montage if needed
	UAnimMontage* Montage = BestVariant.Montage.LoadSynchronous();
	if (!Montage)
	{
		return false;
	}

	// Calculate final play rate
	float FinalPlayRate = BestVariant.PlayRate * Request.PlayRate;

	// Play the montage
	float Duration = AnimInstance->Montage_Play(Montage, FinalPlayRate, EMontagePlayReturnType::MontageLength, Request.StartPosition);

	if (Duration > 0.0f)
	{
		// Jump to section if specified
		if (!Request.StartSection.IsNone())
		{
			AnimInstance->Montage_JumpToSection(Request.StartSection, Montage);
		}

		// Update state
		CurrentMontageId = MontageId;
		CurrentMontage = Montage;
		CurrentMontagePriority = Entry.Priority;

		// Set cooldown
		if (Entry.Cooldown > 0.0f)
		{
			MontageCooldowns.Add(MontageId, Entry.Cooldown);
		}

		// Broadcast event
		OnMontageStarted.Broadcast(MontageId, Montage);

		return true;
	}

	return false;
}

bool UAnimationMontageSystem::PlayMontageDirectly(UAnimMontage* Montage, float PlayRate, FName StartSection)
{
	if (!Montage || !AnimInstance)
	{
		return false;
	}

	float Duration = AnimInstance->Montage_Play(Montage, PlayRate);

	if (Duration > 0.0f)
	{
		if (!StartSection.IsNone())
		{
			AnimInstance->Montage_JumpToSection(StartSection, Montage);
		}

		CurrentMontageId = Montage->GetFName();
		CurrentMontage = Montage;
		CurrentMontagePriority = 0;

		OnMontageStarted.Broadcast(CurrentMontageId, Montage);

		return true;
	}

	return false;
}

bool UAnimationMontageSystem::PlayRandomFromCategory(EMontageCategory Category)
{
	if (!CurrentLibrary)
	{
		return false;
	}

	TArray<FMontageEntry> CategoryMontages = CurrentLibrary->GetMontagesByCategory(Category);

	// Filter out montages on cooldown
	TArray<FMontageEntry> AvailableMontages;
	for (const FMontageEntry& Entry : CategoryMontages)
	{
		if (CanPlayMontage(Entry, false))
		{
			AvailableMontages.Add(Entry);
		}
	}

	if (AvailableMontages.Num() == 0)
	{
		return false;
	}

	// Select random montage
	int32 RandomIndex = FMath::RandRange(0, AvailableMontages.Num() - 1);
	return PlayMontage(AvailableMontages[RandomIndex].MontageId);
}

void UAnimationMontageSystem::StopCurrentMontage(float BlendOutTime)
{
	if (AnimInstance && CurrentMontage)
	{
		AnimInstance->Montage_Stop(BlendOutTime, CurrentMontage);
	}
}

bool UAnimationMontageSystem::IsMontagePlayingById(FName MontageId) const
{
	return CurrentMontageId == MontageId && AnimInstance && AnimInstance->Montage_IsPlaying(CurrentMontage);
}

bool UAnimationMontageSystem::IsAnyMontagePlayingFromCategory(EMontageCategory Category) const
{
	if (!CurrentLibrary || CurrentMontageId.IsNone())
	{
		return false;
	}

	FMontageEntry Entry = CurrentLibrary->GetMontageById(CurrentMontageId);
	return Entry.Category == Category && AnimInstance && AnimInstance->Montage_IsPlaying(CurrentMontage);
}

void UAnimationMontageSystem::SetPersonalityState(const FPersonalityState& State)
{
	PersonalityExtraversion = State.Extraversion;
	PersonalityOpenness = State.Openness;
	PersonalityConscientiousness = State.Conscientiousness;
	PersonalityAgreeableness = State.Agreeableness;
	PersonalityNeuroticism = State.Neuroticism;
}

void UAnimationMontageSystem::SetEmotionalState(const FEmotionalState& State)
{
	EmotionalValence = State.Valence;
	EmotionalArousal = State.Arousal;
	EmotionalDominance = State.Dominance;
}

FMontageVariant UAnimationMontageSystem::SelectBestVariant(const FMontageEntry& Entry) const
{
	if (Entry.Variants.Num() == 0)
	{
		return FMontageVariant();
	}

	if (Entry.Variants.Num() == 1)
	{
		return Entry.Variants[0];
	}

	// Score each variant and select the best
	float BestScore = -FLT_MAX;
	int32 BestIndex = 0;

	for (int32 i = 0; i < Entry.Variants.Num(); ++i)
	{
		float Score = CalculateVariantScore(Entry.Variants[i]);

		// Add some randomness to prevent always selecting the same variant
		Score += FMath::FRandRange(-0.1f, 0.1f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = i;
		}
	}

	return Entry.Variants[BestIndex];
}

float UAnimationMontageSystem::CalculateVariantScore(const FMontageVariant& Variant) const
{
	float Score = 0.0f;

	// Personality matching (higher weight for traits that match)
	Score += (1.0f - FMath::Abs(PersonalityExtraversion - Variant.ExtraversionWeight)) * 0.2f;
	Score += (1.0f - FMath::Abs(PersonalityOpenness - Variant.OpennessWeight)) * 0.2f;
	Score += (1.0f - FMath::Abs(PersonalityConscientiousness - Variant.ConscientiousnessWeight)) * 0.2f;
	Score += (1.0f - FMath::Abs(PersonalityAgreeableness - Variant.AgreeablenessWeight)) * 0.2f;
	Score += (1.0f - FMath::Abs(PersonalityNeuroticism - Variant.NeuroticismWeight)) * 0.2f;

	// Emotion matching
	// Valence is -1 to 1, so we need to normalize
	float NormalizedValence = (EmotionalValence + 1.0f) / 2.0f;
	float NormalizedVariantValence = (Variant.ValenceWeight + 1.0f) / 2.0f;
	Score += (1.0f - FMath::Abs(NormalizedValence - NormalizedVariantValence)) * 0.3f;

	Score += (1.0f - FMath::Abs(EmotionalArousal - Variant.ArousalWeight)) * 0.2f;
	Score += (1.0f - FMath::Abs(EmotionalDominance - Variant.DominanceWeight)) * 0.1f;

	return Score;
}

bool UAnimationMontageSystem::CanPlayMontage(const FMontageEntry& Entry, bool bForce) const
{
	if (bForce)
	{
		return true;
	}

	// Check cooldown
	if (const float* Cooldown = MontageCooldowns.Find(Entry.MontageId))
	{
		if (*Cooldown > 0.0f)
		{
			return false;
		}
	}

	// Check if current montage can be interrupted
	if (CurrentMontage && AnimInstance && AnimInstance->Montage_IsPlaying(CurrentMontage))
	{
		// Check priority
		if (Entry.Priority <= CurrentMontagePriority)
		{
			// Check if current can be interrupted
			if (!Entry.bCanInterrupt)
			{
				return false;
			}
		}
	}

	return true;
}

void UAnimationMontageSystem::UpdateCooldowns(float DeltaTime)
{
	TArray<FName> CompletedCooldowns;

	for (auto& Pair : MontageCooldowns)
	{
		Pair.Value -= DeltaTime;
		if (Pair.Value <= 0.0f)
		{
			CompletedCooldowns.Add(Pair.Key);
		}
	}

	for (FName Id : CompletedCooldowns)
	{
		MontageCooldowns.Remove(Id);
	}
}

void UAnimationMontageSystem::OnMontageEndedInternal(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == CurrentMontage)
	{
		FName EndedMontageId = CurrentMontageId;

		CurrentMontageId = NAME_None;
		CurrentMontage = nullptr;
		CurrentMontagePriority = -1;

		OnMontageEnded.Broadcast(EndedMontageId, Montage, bInterrupted);
	}
}

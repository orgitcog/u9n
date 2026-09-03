// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Dynamic Cosmetics Implementation

#include "DeepTreeEchoCosmeticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "Personality/PersonalityTraitSystem.h"

UDeepTreeEchoCosmeticsComponent::UDeepTreeEchoCosmeticsComponent()
	: EmotionalEffectInterpSpeed(3.0f)
	, BlushMaxIntensity(0.8f)
	, AuraMaxIntensity(1.0f)
	, bGlitchActive(false)
	, GlitchSeverity(0.0f)
	, GlitchTimeRemaining(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.033f; // 30 FPS for cosmetics updates
}

void UDeepTreeEchoCosmeticsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize default emotional effect
	CurrentEmotionalEffect = FEmotionalVisualEffect();
	TargetEmotionalEffect = FEmotionalVisualEffect();
}

void UDeepTreeEchoCosmeticsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Interpolate emotional effects
	InterpolateEmotionalEffects(DeltaTime);

	// Update material parameters
	UpdateMaterialParameters(DeltaTime);

	// Update glitch effect
	if (bGlitchActive)
	{
		UpdateGlitchEffect(DeltaTime);
	}
}

void UDeepTreeEchoCosmeticsComponent::Initialize(USkeletalMeshComponent* InMeshComponent)
{
	MeshComponent = InMeshComponent;

	if (MeshComponent)
	{
		CreateDynamicMaterials();
	}
}

void UDeepTreeEchoCosmeticsComponent::SetOutfit(FName OutfitName)
{
	if (AvailableOutfits.Contains(OutfitName))
	{
		ApplyOutfitConfiguration(AvailableOutfits[OutfitName]);
	}
}

void UDeepTreeEchoCosmeticsComponent::ApplyOutfitConfiguration(const FOutfitConfiguration& Config)
{
	FName OldOutfitName = CurrentOutfitName;
	CurrentOutfitName = Config.OutfitName;
	CurrentOutfitConfig = Config;

	// Apply mesh if specified
	if (MeshComponent && Config.BaseMesh.IsValid())
	{
		USkeletalMesh* LoadedMesh = Config.BaseMesh.LoadSynchronous();
		if (LoadedMesh)
		{
			MeshComponent->SetSkeletalMesh(LoadedMesh);
			CreateDynamicMaterials(); // Recreate materials for new mesh
		}
	}

	// Apply colors
	SetPrimaryColor(Config.PrimaryColor);
	SetSecondaryColor(Config.SecondaryColor);
	SetAccentColor(Config.AccentColor);

	// Broadcast event
	OnOutfitChanged.Broadcast(OldOutfitName, CurrentOutfitName);
}

void UDeepTreeEchoCosmeticsComponent::ApplyPersonalityOutfit(const FPersonalityState& PersonalityState)
{
	FOutfitConfiguration SelectedOutfit = SelectOutfitForPersonality(PersonalityState);
	ApplyOutfitConfiguration(SelectedOutfit);
}

void UDeepTreeEchoCosmeticsComponent::EquipAccessory(FName SlotName, const FAccessorySlot& Accessory)
{
	// Remove existing accessory in slot
	if (EquippedAccessories.Contains(SlotName))
	{
		DestroyAccessoryComponent(SlotName);
	}

	// Add new accessory
	EquippedAccessories.Add(SlotName, Accessory);
	SpawnAccessoryComponent(Accessory);

	OnAccessoryChanged.Broadcast(SlotName, true);
}

void UDeepTreeEchoCosmeticsComponent::UnequipAccessory(FName SlotName)
{
	if (EquippedAccessories.Contains(SlotName))
	{
		DestroyAccessoryComponent(SlotName);
		EquippedAccessories.Remove(SlotName);

		OnAccessoryChanged.Broadcast(SlotName, false);
	}
}

void UDeepTreeEchoCosmeticsComponent::SetAccessoryVisibility(FName SlotName, bool bVisible)
{
	if (AccessoryComponents.Contains(SlotName))
	{
		UStaticMeshComponent* AccessoryComp = AccessoryComponents[SlotName];
		if (AccessoryComp)
		{
			AccessoryComp->SetVisibility(bVisible);
		}
	}

	if (EquippedAccessories.Contains(SlotName))
	{
		EquippedAccessories[SlotName].bIsVisible = bVisible;
	}
}

bool UDeepTreeEchoCosmeticsComponent::IsAccessoryEquipped(FName SlotName) const
{
	return EquippedAccessories.Contains(SlotName);
}

void UDeepTreeEchoCosmeticsComponent::ApplyEmotionalEffects(const FEmotionalState& EmotionalState)
{
	TargetEmotionalEffect = CalculateEmotionalEffect(EmotionalState);
}

void UDeepTreeEchoCosmeticsComponent::SetBlushIntensity(float Intensity)
{
	TargetEmotionalEffect.BlushIntensity = FMath::Clamp(Intensity, 0.0f, BlushMaxIntensity);
}

void UDeepTreeEchoCosmeticsComponent::SetEmotionalAura(FLinearColor Color, float Intensity)
{
	TargetEmotionalEffect.AuraColor = Color;
	TargetEmotionalEffect.AuraIntensity = FMath::Clamp(Intensity, 0.0f, AuraMaxIntensity);

	// Update particle system if available
	if (AuraParticleSystem)
	{
		AuraParticleSystem->SetColorParameter(TEXT("AuraColor"), Color);
		AuraParticleSystem->SetFloatParameter(TEXT("AuraIntensity"), Intensity);
	}
}

void UDeepTreeEchoCosmeticsComponent::SetEyeSparkle(float Intensity)
{
	TargetEmotionalEffect.EyeSparkleIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UDeepTreeEchoCosmeticsComponent::SetHairShimmer(float Intensity)
{
	TargetEmotionalEffect.HairShimmerIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UDeepTreeEchoCosmeticsComponent::SetSkinGlow(float Intensity)
{
	TargetEmotionalEffect.SkinGlowIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UDeepTreeEchoCosmeticsComponent::SetCognitiveVisualization(float CognitiveLoad)
{
	// Update cognitive particle system
	if (CognitiveParticleSystem)
	{
		CognitiveParticleSystem->SetFloatParameter(TEXT("CognitiveLoad"), CognitiveLoad);
		
		// Color shifts from blue (low load) to red (high load)
		FLinearColor CognitiveColor = FLinearColor::LerpUsingHSV(
			FLinearColor(0.2f, 0.4f, 1.0f), // Blue
			FLinearColor(1.0f, 0.2f, 0.2f), // Red
			CognitiveLoad
		);
		CognitiveParticleSystem->SetColorParameter(TEXT("CognitiveColor"), CognitiveColor);
	}

	// Apply to materials
	for (auto& Pair : DynamicMaterials)
	{
		if (Pair.Value)
		{
			Pair.Value->SetScalarParameterValue(TEXT("CognitiveLoad"), CognitiveLoad);
		}
	}
}

void UDeepTreeEchoCosmeticsComponent::TriggerGlitchEffect(float Severity, float Duration)
{
	bGlitchActive = true;
	GlitchSeverity = FMath::Clamp(Severity, 0.0f, 1.0f);
	GlitchTimeRemaining = Duration;

	// Apply initial glitch to materials
	for (auto& Pair : DynamicMaterials)
	{
		if (Pair.Value)
		{
			Pair.Value->SetScalarParameterValue(TEXT("GlitchIntensity"), GlitchSeverity);
		}
	}
}

void UDeepTreeEchoCosmeticsComponent::ShowMemoryConstellation(const TArray<FVector>& MemoryNodes)
{
	// Spawn particle effects at memory node positions
	if (CognitiveParticleSystem)
	{
		for (const FVector& Node : MemoryNodes)
		{
			CognitiveParticleSystem->SetVectorParameter(TEXT("MemoryNode"), Node);
		}
	}
}

void UDeepTreeEchoCosmeticsComponent::SetPrimaryColor(FLinearColor Color)
{
	CurrentOutfitConfig.PrimaryColor = Color;
	ApplyMaterialParameter(TEXT("Skin"), TEXT("PrimaryColor"), Color);
	ApplyMaterialParameter(TEXT("Outfit"), TEXT("PrimaryColor"), Color);
}

void UDeepTreeEchoCosmeticsComponent::SetSecondaryColor(FLinearColor Color)
{
	CurrentOutfitConfig.SecondaryColor = Color;
	ApplyMaterialParameter(TEXT("Outfit"), TEXT("SecondaryColor"), Color);
}

void UDeepTreeEchoCosmeticsComponent::SetAccentColor(FLinearColor Color)
{
	CurrentOutfitConfig.AccentColor = Color;
	ApplyMaterialParameter(TEXT("Outfit"), TEXT("AccentColor"), Color);
	ApplyMaterialParameter(TEXT("Accessory"), TEXT("AccentColor"), Color);
}

void UDeepTreeEchoCosmeticsComponent::SetHairColor(FLinearColor Color)
{
	ApplyMaterialParameter(TEXT("Hair"), TEXT("HairColor"), Color);
	ApplyMaterialParameter(TEXT("Hair"), TEXT("HairTipColor"), Color * 1.2f); // Slightly lighter tips
}

void UDeepTreeEchoCosmeticsComponent::SetEyeColor(FLinearColor Color)
{
	ApplyMaterialParameter(TEXT("Eye"), TEXT("IrisColor"), Color);
	ApplyMaterialParameter(TEXT("Eye"), TEXT("PupilColor"), Color * 0.3f); // Darker pupil
}

void UDeepTreeEchoCosmeticsComponent::SetSkinTone(FLinearColor Color)
{
	ApplyMaterialParameter(TEXT("Skin"), TEXT("SkinBaseColor"), Color);
	ApplyMaterialParameter(TEXT("Skin"), TEXT("SubsurfaceColor"), Color * FLinearColor(1.0f, 0.8f, 0.7f)); // Warmer subsurface
}

void UDeepTreeEchoCosmeticsComponent::CreateDynamicMaterials()
{
	if (!MeshComponent)
	{
		return;
	}

	DynamicMaterials.Empty();

	int32 NumMaterials = MeshComponent->GetNumMaterials();
	for (int32 i = 0; i < NumMaterials; i++)
	{
		UMaterialInterface* Material = MeshComponent->GetMaterial(i);
		if (Material)
		{
			UMaterialInstanceDynamic* DynamicMat = UMaterialInstanceDynamic::Create(Material, this);
			if (DynamicMat)
			{
				MeshComponent->SetMaterial(i, DynamicMat);

				// Determine material slot name based on index or material name
				FName SlotName = FName(*FString::Printf(TEXT("Material_%d"), i));
				
				// Try to get a more descriptive name from the material
				FString MaterialName = Material->GetName().ToLower();
				if (MaterialName.Contains(TEXT("skin")))
				{
					SlotName = TEXT("Skin");
				}
				else if (MaterialName.Contains(TEXT("hair")))
				{
					SlotName = TEXT("Hair");
				}
				else if (MaterialName.Contains(TEXT("eye")))
				{
					SlotName = TEXT("Eye");
				}
				else if (MaterialName.Contains(TEXT("outfit")) || MaterialName.Contains(TEXT("cloth")))
				{
					SlotName = TEXT("Outfit");
				}

				DynamicMaterials.Add(SlotName, DynamicMat);
			}
		}
	}
}

void UDeepTreeEchoCosmeticsComponent::UpdateMaterialParameters(float DeltaTime)
{
	// Apply current emotional effect to all materials
	for (auto& Pair : DynamicMaterials)
	{
		UMaterialInstanceDynamic* DynamicMat = Pair.Value;
		if (!DynamicMat)
		{
			continue;
		}

		FName SlotName = Pair.Key;

		// Blush (skin only)
		if (SlotName == TEXT("Skin"))
		{
			DynamicMat->SetScalarParameterValue(TEXT("BlushIntensity"), CurrentEmotionalEffect.BlushIntensity);
			DynamicMat->SetScalarParameterValue(TEXT("SkinGlowIntensity"), CurrentEmotionalEffect.SkinGlowIntensity);
		}

		// Eye sparkle (eyes only)
		if (SlotName == TEXT("Eye"))
		{
			DynamicMat->SetScalarParameterValue(TEXT("SparkleIntensity"), CurrentEmotionalEffect.EyeSparkleIntensity);
		}

		// Hair shimmer (hair only)
		if (SlotName == TEXT("Hair"))
		{
			DynamicMat->SetScalarParameterValue(TEXT("ShimmerIntensity"), CurrentEmotionalEffect.HairShimmerIntensity);
		}

		// Aura (all materials)
		DynamicMat->SetVectorParameterValue(TEXT("AuraColor"), CurrentEmotionalEffect.AuraColor);
		DynamicMat->SetScalarParameterValue(TEXT("AuraIntensity"), CurrentEmotionalEffect.AuraIntensity);
	}
}

void UDeepTreeEchoCosmeticsComponent::InterpolateEmotionalEffects(float DeltaTime)
{
	float InterpAlpha = FMath::Clamp(EmotionalEffectInterpSpeed * DeltaTime, 0.0f, 1.0f);

	// Interpolate all effect values
	CurrentEmotionalEffect.BlushIntensity = FMath::Lerp(
		CurrentEmotionalEffect.BlushIntensity,
		TargetEmotionalEffect.BlushIntensity,
		InterpAlpha
	);

	CurrentEmotionalEffect.AuraColor = FLinearColor::LerpUsingHSV(
		CurrentEmotionalEffect.AuraColor,
		TargetEmotionalEffect.AuraColor,
		InterpAlpha
	);

	CurrentEmotionalEffect.AuraIntensity = FMath::Lerp(
		CurrentEmotionalEffect.AuraIntensity,
		TargetEmotionalEffect.AuraIntensity,
		InterpAlpha
	);

	CurrentEmotionalEffect.EyeSparkleIntensity = FMath::Lerp(
		CurrentEmotionalEffect.EyeSparkleIntensity,
		TargetEmotionalEffect.EyeSparkleIntensity,
		InterpAlpha
	);

	CurrentEmotionalEffect.HairShimmerIntensity = FMath::Lerp(
		CurrentEmotionalEffect.HairShimmerIntensity,
		TargetEmotionalEffect.HairShimmerIntensity,
		InterpAlpha
	);

	CurrentEmotionalEffect.SkinGlowIntensity = FMath::Lerp(
		CurrentEmotionalEffect.SkinGlowIntensity,
		TargetEmotionalEffect.SkinGlowIntensity,
		InterpAlpha
	);
}

void UDeepTreeEchoCosmeticsComponent::UpdateGlitchEffect(float DeltaTime)
{
	GlitchTimeRemaining -= DeltaTime;

	if (GlitchTimeRemaining <= 0.0f)
	{
		bGlitchActive = false;
		GlitchSeverity = 0.0f;

		// Reset glitch on all materials
		for (auto& Pair : DynamicMaterials)
		{
			if (Pair.Value)
			{
				Pair.Value->SetScalarParameterValue(TEXT("GlitchIntensity"), 0.0f);
			}
		}
	}
	else
	{
		// Pulsing glitch effect
		float GlitchPulse = GlitchSeverity * (0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 20.0f));
		
		// Add random spikes
		if (FMath::RandRange(0.0f, 1.0f) < 0.1f)
		{
			GlitchPulse = GlitchSeverity;
		}

		for (auto& Pair : DynamicMaterials)
		{
			if (Pair.Value)
			{
				Pair.Value->SetScalarParameterValue(TEXT("GlitchIntensity"), GlitchPulse);
				
				// Random UV offset for glitch
				float UVOffset = FMath::RandRange(-0.1f, 0.1f) * GlitchPulse;
				Pair.Value->SetScalarParameterValue(TEXT("GlitchUVOffset"), UVOffset);
			}
		}
	}
}

void UDeepTreeEchoCosmeticsComponent::ApplyMaterialParameter(FName MaterialSlot, FName ParameterName, float Value)
{
	if (DynamicMaterials.Contains(MaterialSlot))
	{
		UMaterialInstanceDynamic* DynamicMat = DynamicMaterials[MaterialSlot];
		if (DynamicMat)
		{
			DynamicMat->SetScalarParameterValue(ParameterName, Value);
		}
	}
}

void UDeepTreeEchoCosmeticsComponent::ApplyMaterialParameter(FName MaterialSlot, FName ParameterName, FLinearColor Value)
{
	if (DynamicMaterials.Contains(MaterialSlot))
	{
		UMaterialInstanceDynamic* DynamicMat = DynamicMaterials[MaterialSlot];
		if (DynamicMat)
		{
			DynamicMat->SetVectorParameterValue(ParameterName, Value);
		}
	}
}

FEmotionalVisualEffect UDeepTreeEchoCosmeticsComponent::CalculateEmotionalEffect(const FEmotionalState& State) const
{
	FEmotionalVisualEffect Effect;

	// Blush: High arousal + positive valence = blush
	// Also triggered by embarrassment (high arousal + negative valence + low dominance)
	float BlushFromHappiness = State.Arousal * State.Valence;
	float BlushFromEmbarrassment = State.Arousal * (1.0f - State.Valence) * (1.0f - State.Dominance);
	Effect.BlushIntensity = FMath::Max(BlushFromHappiness, BlushFromEmbarrassment) * BlushMaxIntensity;

	// Aura color: Based on emotional state
	// Positive valence = warm colors (yellow/orange/pink)
	// Negative valence = cool colors (blue/purple)
	// High arousal = more saturated
	// Low arousal = more muted
	if (State.Valence > 0.5f)
	{
		// Positive emotions
		if (State.Arousal > 0.5f)
		{
			// Excited/Happy = bright yellow/orange
			Effect.AuraColor = FLinearColor::LerpUsingHSV(
				FLinearColor(1.0f, 0.8f, 0.2f), // Yellow
				FLinearColor(1.0f, 0.4f, 0.6f), // Pink
				State.Dominance
			);
		}
		else
		{
			// Calm/Content = soft green/blue
			Effect.AuraColor = FLinearColor::LerpUsingHSV(
				FLinearColor(0.4f, 0.8f, 0.6f), // Soft green
				FLinearColor(0.6f, 0.8f, 1.0f), // Soft blue
				State.Dominance
			);
		}
	}
	else
	{
		// Negative emotions
		if (State.Arousal > 0.5f)
		{
			// Angry/Anxious = red/orange
			Effect.AuraColor = FLinearColor::LerpUsingHSV(
				FLinearColor(1.0f, 0.2f, 0.2f), // Red
				FLinearColor(1.0f, 0.5f, 0.0f), // Orange
				State.Dominance
			);
		}
		else
		{
			// Sad/Depressed = blue/purple
			Effect.AuraColor = FLinearColor::LerpUsingHSV(
				FLinearColor(0.3f, 0.3f, 0.8f), // Blue
				FLinearColor(0.5f, 0.2f, 0.6f), // Purple
				State.Dominance
			);
		}
	}

	// Aura intensity: Based on arousal (more intense when more aroused)
	Effect.AuraIntensity = State.Arousal * AuraMaxIntensity * 0.5f;

	// Eye sparkle: High valence + high arousal = sparkly eyes
	Effect.EyeSparkleIntensity = State.Valence * State.Arousal;

	// Hair shimmer: High dominance + positive valence = confident shimmer
	Effect.HairShimmerIntensity = State.Dominance * State.Valence * 0.8f;

	// Skin glow: High valence = healthy glow
	Effect.SkinGlowIntensity = State.Valence * 0.5f;

	return Effect;
}

FOutfitConfiguration UDeepTreeEchoCosmeticsComponent::SelectOutfitForPersonality(const FPersonalityState& State) const
{
	FOutfitConfiguration SelectedOutfit;

	// Find the best matching outfit based on personality
	float BestMatch = -1.0f;
	FName BestOutfitName = NAME_None;

	for (const auto& Pair : AvailableOutfits)
	{
		const FOutfitConfiguration& Outfit = Pair.Value;

		// Calculate match score
		float MatchScore = 0.0f;

		// Extraversion affects expressiveness preference
		float ExpressivenessMatch = 1.0f - FMath::Abs(State.Extraversion - Outfit.Expressiveness);
		MatchScore += ExpressivenessMatch * 30.0f;

		// Conscientiousness affects formality preference
		float FormalityMatch = 1.0f - FMath::Abs(State.Conscientiousness - Outfit.Formality);
		MatchScore += FormalityMatch * 30.0f;

		// Openness affects color boldness preference
		float ColorBoldness = (Outfit.PrimaryColor.GetLuminance() + Outfit.AccentColor.GetLuminance()) / 2.0f;
		float BoldnessMatch = 1.0f - FMath::Abs(State.Openness - ColorBoldness);
		MatchScore += BoldnessMatch * 20.0f;

		if (MatchScore > BestMatch)
		{
			BestMatch = MatchScore;
			BestOutfitName = Pair.Key;
			SelectedOutfit = Outfit;
		}
	}

	// If no outfit found, return current or default
	if (BestOutfitName == NAME_None)
	{
		return CurrentOutfitConfig;
	}

	return SelectedOutfit;
}

void UDeepTreeEchoCosmeticsComponent::SpawnAccessoryComponent(const FAccessorySlot& Slot)
{
	if (!MeshComponent || !Slot.AccessoryMesh.IsValid())
	{
		return;
	}

	UStaticMesh* LoadedMesh = Slot.AccessoryMesh.LoadSynchronous();
	if (!LoadedMesh)
	{
		return;
	}

	// Create static mesh component for accessory
	UStaticMeshComponent* AccessoryComp = NewObject<UStaticMeshComponent>(GetOwner());
	if (AccessoryComp)
	{
		AccessoryComp->SetStaticMesh(LoadedMesh);
		AccessoryComp->SetRelativeTransform(Slot.RelativeTransform);
		AccessoryComp->SetVisibility(Slot.bIsVisible);

		// Attach to socket
		AccessoryComp->AttachToComponent(
			MeshComponent,
			FAttachmentTransformRules::KeepRelativeTransform,
			Slot.AttachSocketName
		);

		AccessoryComp->RegisterComponent();

		AccessoryComponents.Add(Slot.SlotName, AccessoryComp);
	}
}

void UDeepTreeEchoCosmeticsComponent::DestroyAccessoryComponent(FName SlotName)
{
	if (AccessoryComponents.Contains(SlotName))
	{
		UStaticMeshComponent* AccessoryComp = AccessoryComponents[SlotName];
		if (AccessoryComp)
		{
			AccessoryComp->DestroyComponent();
		}
		AccessoryComponents.Remove(SlotName);
	}
}

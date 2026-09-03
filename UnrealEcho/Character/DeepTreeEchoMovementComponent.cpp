// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Personality-Driven Movement Implementation

#include "DeepTreeEchoMovementComponent.h"
#include "GameFramework/Character.h"
#include "Personality/PersonalityTraitSystem.h"

UDeepTreeEchoMovementComponent::UDeepTreeEchoMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BaseWalkSpeed(600.0f)
	, BaseRunSpeed(1200.0f)
	, BaseAcceleration(2048.0f)
	, EmotionalSpeedModifier(1.0f)
	, EmotionalAccelerationModifier(1.0f)
	, PersonalitySpeedModifier(1.0f)
	, PersonalityTurningModifier(1.0f)
	, CognitiveLoadModifier(1.0f)
	, CurrentCognitiveLoad(0.0f)
{
	// Configure default movement parameters
	MaxWalkSpeed = BaseWalkSpeed;
	MaxAcceleration = BaseAcceleration;
	BrakingDecelerationWalking = 2048.0f;
	GroundFriction = 8.0f;
	MaxStepHeight = 45.0f;
	JumpZVelocity = 700.0f;
	AirControl = 0.35f;
	AirControlBoostMultiplier = 2.0f;
	AirControlBoostVelocityThreshold = 25.0f;
	FallingLateralFriction = 0.0f;
	bCanWalkOffLedgesWhenCrouching = true;
	bNetworkSmoothingComplete = true;
}

float UDeepTreeEchoMovementComponent::GetMaxSpeed() const
{
	float BaseSpeed = Super::GetMaxSpeed();
	
	// Apply all modifiers
	float FinalSpeed = BaseSpeed * EmotionalSpeedModifier * PersonalitySpeedModifier * CognitiveLoadModifier;
	
	return FMath::Max(FinalSpeed, 0.0f);
}

float UDeepTreeEchoMovementComponent::GetMaxAcceleration() const
{
	float BaseAccel = Super::GetMaxAcceleration();
	
	// Apply emotional and cognitive modifiers to acceleration
	float FinalAccel = BaseAccel * EmotionalAccelerationModifier * CognitiveLoadModifier;
	
	return FMath::Max(FinalAccel, 0.0f);
}

float UDeepTreeEchoMovementComponent::GetMaxBrakingDeceleration() const
{
	float BaseDecel = Super::GetMaxBrakingDeceleration();
	
	// Cognitive load affects braking (higher load = slower reactions)
	float FinalDecel = BaseDecel * CognitiveLoadModifier;
	
	return FMath::Max(FinalDecel, 0.0f);
}

void UDeepTreeEchoMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Update all modifiers
	UpdateEmotionalModifiers();
	UpdatePersonalityModifiers();
	UpdateCognitiveModifiers();
	ApplyMovementModifiers();

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UDeepTreeEchoMovementComponent::SetEmotionalState(const FEmotionalState& NewState)
{
	CurrentEmotionalState = NewState;
	UpdateEmotionalModifiers();
}

void UDeepTreeEchoMovementComponent::SetPersonalityState(const FPersonalityState& NewState)
{
	CurrentPersonalityState = NewState;
	UpdatePersonalityModifiers();
}

void UDeepTreeEchoMovementComponent::SetCognitiveLoad(float NewLoad)
{
	CurrentCognitiveLoad = FMath::Clamp(NewLoad, 0.0f, 1.0f);
	UpdateCognitiveModifiers();
}

void UDeepTreeEchoMovementComponent::UpdateEmotionalModifiers()
{
	// Arousal affects speed (high arousal = faster movement)
	// Valence affects acceleration (positive = quicker acceleration)
	// Dominance affects turning (high dominance = sharper turns)

	float TargetSpeedModifier = 1.0f;
	float TargetAccelModifier = 1.0f;

	// Arousal: 0.0 (calm) to 1.0 (excited)
	// Map to speed modifier: 0.7 (calm) to 1.3 (excited)
	float Arousal = CurrentEmotionalState.Arousal;
	TargetSpeedModifier = FMath::Lerp(0.7f, 1.3f, Arousal);

	// Valence: 0.0 (negative) to 1.0 (positive)
	// Map to acceleration modifier: 0.8 (negative) to 1.2 (positive)
	float Valence = CurrentEmotionalState.Valence;
	TargetAccelModifier = FMath::Lerp(0.8f, 1.2f, Valence);

	// Smooth interpolation
	float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	EmotionalSpeedModifier = InterpolateModifier(EmotionalSpeedModifier, TargetSpeedModifier, DeltaTime);
	EmotionalAccelerationModifier = InterpolateModifier(EmotionalAccelerationModifier, TargetAccelModifier, DeltaTime);
}

void UDeepTreeEchoMovementComponent::UpdatePersonalityModifiers()
{
	// Extraversion affects base speed (extraverts move faster)
	// Conscientiousness affects precision (more deliberate movement)
	// Openness affects exploration (more varied movement patterns)

	float TargetSpeedModifier = 1.0f;
	float TargetTurningModifier = 1.0f;

	// Extraversion: 0.0 (introvert) to 1.0 (extravert)
	// Map to speed modifier: 0.9 (introvert) to 1.1 (extravert)
	float Extraversion = CurrentPersonalityState.Extraversion;
	TargetSpeedModifier = FMath::Lerp(0.9f, 1.1f, Extraversion);

	// Conscientiousness: 0.0 (spontaneous) to 1.0 (deliberate)
	// Map to turning modifier: 1.2 (spontaneous, quick turns) to 0.9 (deliberate, smooth turns)
	float Conscientiousness = CurrentPersonalityState.Conscientiousness;
	TargetTurningModifier = FMath::Lerp(1.2f, 0.9f, Conscientiousness);

	// Smooth interpolation
	float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	PersonalitySpeedModifier = InterpolateModifier(PersonalitySpeedModifier, TargetSpeedModifier, DeltaTime, 1.0f);
	PersonalityTurningModifier = InterpolateModifier(PersonalityTurningModifier, TargetTurningModifier, DeltaTime, 1.0f);
}

void UDeepTreeEchoMovementComponent::UpdateCognitiveModifiers()
{
	// Cognitive load affects all movement (higher load = slower, less precise)
	// Map cognitive load (0.0 to 1.0) to modifier (1.0 to 0.6)
	float TargetModifier = FMath::Lerp(1.0f, 0.6f, CurrentCognitiveLoad);

	// Smooth interpolation
	float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	CognitiveLoadModifier = InterpolateModifier(CognitiveLoadModifier, TargetModifier, DeltaTime, 3.0f);
}

void UDeepTreeEchoMovementComponent::ApplyMovementModifiers()
{
	// Update max walk speed
	MaxWalkSpeed = BaseWalkSpeed * EmotionalSpeedModifier * PersonalitySpeedModifier * CognitiveLoadModifier;

	// Update acceleration
	MaxAcceleration = BaseAcceleration * EmotionalAccelerationModifier * CognitiveLoadModifier;

	// Update turning rate (affects rotation rate)
	RotationRate.Yaw = 360.0f * PersonalityTurningModifier * CognitiveLoadModifier;
}

float UDeepTreeEchoMovementComponent::InterpolateModifier(float Current, float Target, float DeltaTime, float InterpSpeed)
{
	return FMath::FInterpTo(Current, Target, DeltaTime, InterpSpeed);
}

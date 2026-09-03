// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Personality-Driven Movement Component

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DeepTreeEchoMovementComponent.generated.h"

struct FEmotionalState;
struct FPersonalityState;

/**
 * UDeepTreeEchoMovementComponent
 *
 * Enhanced character movement component that modulates movement based on:
 * - Emotional state (speed, acceleration, turning)
 * - Personality traits (movement style, responsiveness)
 * - Cognitive load (reaction time, precision)
 * - Neurochemical state (energy levels, coordination)
 */
UCLASS()
class UNREALENGINE_API UDeepTreeEchoMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDeepTreeEchoMovementComponent(const FObjectInitializer& ObjectInitializer);

	//~UCharacterMovementComponent interface
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UCharacterMovementComponent interface

	// Set emotional state for movement modulation
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Movement")
	void SetEmotionalState(const FEmotionalState& NewState);

	// Set personality state for movement style
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Movement")
	void SetPersonalityState(const FPersonalityState& NewState);

	// Set cognitive load for responsiveness
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Movement")
	void SetCognitiveLoad(float NewLoad);

	// Get current movement modifiers
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Movement")
	float GetEmotionalSpeedModifier() const { return EmotionalSpeedModifier; }

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Movement")
	float GetPersonalitySpeedModifier() const { return PersonalitySpeedModifier; }

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Movement")
	float GetCognitiveLoadModifier() const { return CognitiveLoadModifier; }

protected:
	// Base movement parameters (configured in blueprints)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Movement")
	float BaseWalkSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Movement")
	float BaseRunSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Movement")
	float BaseAcceleration;

	// Emotional state modifiers
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Movement")
	float EmotionalSpeedModifier;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Movement")
	float EmotionalAccelerationModifier;

	// Personality modifiers
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Movement")
	float PersonalitySpeedModifier;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Movement")
	float PersonalityTurningModifier;

	// Cognitive load modifiers
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Movement")
	float CognitiveLoadModifier;

	// Current state
	FEmotionalState CurrentEmotionalState;
	FPersonalityState CurrentPersonalityState;
	float CurrentCognitiveLoad;

	// Internal calculation functions
	void UpdateEmotionalModifiers();
	void UpdatePersonalityModifiers();
	void UpdateCognitiveModifiers();
	void ApplyMovementModifiers();

	// Smooth interpolation for modifier changes
	float InterpolateModifier(float Current, float Target, float DeltaTime, float InterpSpeed = 2.0f);
};

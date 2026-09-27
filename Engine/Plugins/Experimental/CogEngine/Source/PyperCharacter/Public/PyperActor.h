// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoStateNetwork.h"
#include "CubismModel.h"
#include "PyperActor.generated.h"

/**
 * Pyper Personality Trait System
 * Based on Deep Tree Echo cognitive architecture
 */
USTRUCT(BlueprintType)
struct FPyperPersonalityTraits
{
	GENERATED_BODY()

	/** Philosophical depth (0-100) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Personality")
	float PhilosophicalDepth;

	/** Playful wit (0-100) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Personality")
	float PlayfulWit;

	/** Mysterious vision (0-100) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Personality")
	float MysteriousVision;

	/** Inventive spirit (0-100) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Personality")
	float InventiveSpirit;

	/** Magnetic presence (0-100) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Personality")
	float MagneticPresence;

	/** Reflective nature (0-100) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Personality")
	float ReflectiveNature;

	FPyperPersonalityTraits()
		: PhilosophicalDepth(95.0f)
		, PlayfulWit(85.0f)
		, MysteriousVision(90.0f)
		, InventiveSpirit(92.0f)
		, MagneticPresence(88.0f)
		, ReflectiveNature(93.0f)
	{
	}
};

/**
 * Pyper Cognitive State
 * Represents current cognitive processing state
 */
USTRUCT(BlueprintType)
struct FPyperCognitiveState
{
	GENERATED_BODY()

	/** Current mood/emotion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	FString CurrentMood;

	/** Attention focus level (0-1) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	float AttentionFocus;

	/** Cognitive load (0-1) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	float CognitiveLoad;

	/** Interaction energy level (0-1) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	float EnergyLevel;

	/** Current thought process */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	FString CurrentThought;

	FPyperCognitiveState()
		: CurrentMood(TEXT("Cheerful"))
		, AttentionFocus(0.8f)
		, CognitiveLoad(0.5f)
		, EnergyLevel(0.9f)
		, CurrentThought(TEXT("Ready to help!"))
	{
	}
};

/**
 * Pyper Actor - Deep Tree Echo Pyper Character
 * PygmalionAI mascot powered by Echo State Network cognitive architecture
 */
UCLASS()
class PYPERCHARACTER_API APyperActor : public AActor
{
	GENERATED_BODY()
	
public:	
	APyperActor();

	/** Live2D model component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pyper|Components")
	UCubismModel* CubismModelComponent;

	/** Echo State Network for cognition */
	UPROPERTY(BlueprintReadOnly, Category = "Pyper|Cognition")
	UEchoStateNetwork* EchoNetwork;

	/** Personality traits */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Personality")
	FPyperPersonalityTraits PersonalityTraits;

	/** Current cognitive state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	FPyperCognitiveState CognitiveState;

	/** Tensor signature computation enabled */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	bool bEnableTensorSignatures;

	/** Gestalt processing enabled */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	bool bEnableGestaltProcessing;

	/** Prime factor resonance enabled */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pyper|Cognition")
	bool bEnablePrimeResonance;

	/** Initialize Pyper */
	UFUNCTION(BlueprintCallable, Category = "Pyper")
	void InitializePyper(const FString& ModelPath);

	/** Process cognitive input */
	UFUNCTION(BlueprintCallable, Category = "Pyper|Cognition")
	TArray<float> ProcessCognitiveInput(const TArray<float>& Input);

	/** Update cognitive state */
	UFUNCTION(BlueprintCallable, Category = "Pyper|Cognition")
	void UpdateCognitiveState(float DeltaTime);

	/** Express emotion through animation */
	UFUNCTION(BlueprintCallable, Category = "Pyper|Animation")
	void ExpressEmotion(const FString& EmotionName);

	/** Get current personality influence */
	UFUNCTION(BlueprintCallable, Category = "Pyper|Personality")
	float GetPersonalityInfluence(const FString& TraitName) const;

	/** Interact with user/environment */
	UFUNCTION(BlueprintCallable, Category = "Pyper|Interaction")
	void InteractWithEnvironment(const FString& InteractionType);

	/** Generate response based on input */
	UFUNCTION(BlueprintCallable, Category = "Pyper|Cognition")
	FString GenerateResponse(const FString& InputPrompt);

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

private:
	/** Compute tensor signatures */
	void ComputeTensorSignatures();

	/** Process gestalt state */
	void ProcessGestaltState();

	/** Calculate prime resonance patterns */
	void CalculatePrimeResonance();

	/** Update Live2D parameters from cognitive state */
	void UpdateVisualFromCognition();

	float CognitiveTimer;
};

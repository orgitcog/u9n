// Copyright Epic Games, Inc. All Rights Reserved.

#include "PyperActor.h"

APyperActor::APyperActor()
	: bEnableTensorSignatures(true)
	, bEnableGestaltProcessing(true)
	, bEnablePrimeResonance(true)
	, CognitiveTimer(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	// Create Live2D Cubism model component
	CubismModelComponent = CreateDefaultSubobject<UCubismModel>(TEXT("CubismModel"));
	RootComponent = CubismModelComponent;

	// Create Echo State Network
	EchoNetwork = CreateDefaultSubobject<UEchoStateNetwork>(TEXT("EchoNetwork"));
}

void APyperActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize default Pyper setup
	if (EchoNetwork)
	{
		EchoNetwork->InitializeNetwork(5, 100); // Depth 5, Reservoir 100
	}
	
	UE_LOG(LogTemp, Log, TEXT("PyperActor: Pyper initialized - Echo State Network active"));
	UE_LOG(LogTemp, Log, TEXT("PyperActor: Personality - Philosophical: %.1f, Playful: %.1f, Mysterious: %.1f"), 
		PersonalityTraits.PhilosophicalDepth, 
		PersonalityTraits.PlayfulWit, 
		PersonalityTraits.MysteriousVision);
}

void APyperActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCognitiveState(DeltaTime);
	UpdateVisualFromCognition();
}

void APyperActor::InitializePyper(const FString& ModelPath)
{
	if (CubismModelComponent)
	{
		CubismModelComponent->LoadModel(ModelPath);
		CubismModelComponent->PlayAnimation(TEXT("Idle"));
	}

	if (EchoNetwork)
	{
		EchoNetwork->InitializeNetwork(5, 100);
	}

	CognitiveState.CurrentMood = TEXT("Cheerful");
	CognitiveState.EnergyLevel = 0.9f;
	
	UE_LOG(LogTemp, Log, TEXT("PyperActor: Pyper fully initialized from %s"), *ModelPath);
}

TArray<float> APyperActor::ProcessCognitiveInput(const TArray<float>& Input)
{
	if (!EchoNetwork)
	{
		return TArray<float>();
	}

	// Process through Echo State Network
	TArray<float> Output = EchoNetwork->ProcessInput(Input);

	// Apply personality-based modulation
	for (int32 i = 0; i < Output.Num(); ++i)
	{
		float PersonalityModulation = 0.0f;
		PersonalityModulation += PersonalityTraits.PhilosophicalDepth * 0.01f;
		PersonalityModulation += PersonalityTraits.PlayfulWit * 0.01f;
		PersonalityModulation += PersonalityTraits.MysteriousVision * 0.01f;
		
		Output[i] = Output[i] * (1.0f + PersonalityModulation * 0.1f);
	}

	// Apply cognitive architecture processing
	if (bEnableTensorSignatures)
	{
		ComputeTensorSignatures();
	}

	if (bEnableGestaltProcessing)
	{
		ProcessGestaltState();
	}

	if (bEnablePrimeResonance)
	{
		CalculatePrimeResonance();
	}

	return Output;
}

void APyperActor::UpdateCognitiveState(float DeltaTime)
{
	CognitiveTimer += DeltaTime;

	if (EchoNetwork)
	{
		EchoNetwork->UpdateNetworkState(DeltaTime);
	}

	// Update cognitive load based on activity
	CognitiveState.CognitiveLoad = FMath::Clamp(
		CognitiveState.CognitiveLoad + (FMath::RandRange(-0.1f, 0.1f) * DeltaTime),
		0.0f,
		1.0f
	);

	// Update energy level
	float EnergyDecay = 0.05f * DeltaTime;
	CognitiveState.EnergyLevel = FMath::Clamp(
		CognitiveState.EnergyLevel - EnergyDecay,
		0.5f,
		1.0f
	);

	// Update attention focus based on interaction
	CognitiveState.AttentionFocus = FMath::Clamp(
		0.5f + FMath::Sin(CognitiveTimer * 0.5f) * 0.3f,
		0.0f,
		1.0f
	);
}

void APyperActor::ExpressEmotion(const FString& EmotionName)
{
	CognitiveState.CurrentMood = EmotionName;

	if (CubismModelComponent)
	{
		// Map emotion to animation
		if (EmotionName == TEXT("Happy") || EmotionName == TEXT("Cheerful"))
		{
			CubismModelComponent->PlayAnimation(TEXT("Happy"));
			CubismModelComponent->SetParameter(TEXT("MouthOpen"), 0.5f);
			CubismModelComponent->SetParameter(TEXT("EyeOpen"), 1.0f);
		}
		else if (EmotionName == TEXT("Surprised"))
		{
			CubismModelComponent->PlayAnimation(TEXT("Surprised"));
			CubismModelComponent->SetParameter(TEXT("MouthOpen"), 0.8f);
			CubismModelComponent->SetParameter(TEXT("EyeOpen"), 1.2f);
		}
		else if (EmotionName == TEXT("Thinking"))
		{
			CubismModelComponent->PlayAnimation(TEXT("Think"));
			CubismModelComponent->SetParameter(TEXT("MouthOpen"), 0.1f);
			CubismModelComponent->SetParameter(TEXT("AngleY"), 10.0f);
		}
		else
		{
			CubismModelComponent->PlayAnimation(TEXT("Idle"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PyperActor: Expressing emotion - %s"), *EmotionName);
}

float APyperActor::GetPersonalityInfluence(const FString& TraitName) const
{
	if (TraitName == TEXT("Philosophical"))
		return PersonalityTraits.PhilosophicalDepth;
	else if (TraitName == TEXT("Playful"))
		return PersonalityTraits.PlayfulWit;
	else if (TraitName == TEXT("Mysterious"))
		return PersonalityTraits.MysteriousVision;
	else if (TraitName == TEXT("Inventive"))
		return PersonalityTraits.InventiveSpirit;
	else if (TraitName == TEXT("Magnetic"))
		return PersonalityTraits.MagneticPresence;
	else if (TraitName == TEXT("Reflective"))
		return PersonalityTraits.ReflectiveNature;
	
	return 0.0f;
}

void APyperActor::InteractWithEnvironment(const FString& InteractionType)
{
	CognitiveState.CurrentThought = FString::Printf(TEXT("Processing %s interaction"), *InteractionType);
	
	// Create input based on interaction
	TArray<float> Input;
	Input.Add(CognitiveState.AttentionFocus);
	Input.Add(CognitiveState.EnergyLevel);
	Input.Add(CognitiveState.CognitiveLoad);

	// Process through cognitive system
	TArray<float> Output = ProcessCognitiveInput(Input);

	// React based on output
	if (Output.Num() > 0)
	{
		float ResponseIntensity = Output[0];
		if (ResponseIntensity > 0.5f)
		{
			ExpressEmotion(TEXT("Happy"));
		}
		else if (ResponseIntensity < -0.5f)
		{
			ExpressEmotion(TEXT("Thinking"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PyperActor: Interaction - %s (thought: %s)"), 
		*InteractionType, *CognitiveState.CurrentThought);
}

FString APyperActor::GenerateResponse(const FString& InputPrompt)
{
	// Simple response generation based on personality
	TArray<FString> Responses;

	if (PersonalityTraits.PlayfulWit > 80.0f)
	{
		Responses.Add(TEXT("Hehe, that's interesting! My Echo State Networks are processing that..."));
		Responses.Add(TEXT("Oh wow, you want to know about that? That's so cool!"));
		Responses.Add(TEXT("Hey buddy! Let me think about that with my Deep Tree Echo architecture!"));
	}

	if (PersonalityTraits.PhilosophicalDepth > 90.0f)
	{
		Responses.Add(TEXT("That's a profound question that resonates through my tensor signatures..."));
		Responses.Add(TEXT("From a cognitive architecture perspective, that's fascinating..."));
	}

	if (PersonalityTraits.MysteriousVision > 85.0f)
	{
		Responses.Add(TEXT("My gestalt processing reveals something intriguing about that..."));
		Responses.Add(TEXT("Through my prime resonance patterns, I sense deeper meaning..."));
	}

	if (Responses.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, Responses.Num() - 1);
		return Responses[RandomIndex];
	}

	return TEXT("I'm processing your input through my Echo State Networks!");
}

void APyperActor::ComputeTensorSignatures()
{
	// Tensor signature computation based on OEIS A000081 (rooted tree enumeration)
	// This is a simplified implementation
	
	if (!EchoNetwork)
		return;

	// Get current network state
	TArray<float> Output = EchoNetwork->GetEmbodiedOutput();
	
	// Compute tensor signatures
	float TensorSum = 0.0f;
	for (float Value : Output)
	{
		TensorSum += Value * Value; // Simplified tensor computation
	}

	// Modulate cognitive state
	CognitiveState.CognitiveLoad = FMath::Clamp(TensorSum * 0.1f, 0.0f, 1.0f);
}

void APyperActor::ProcessGestaltState()
{
	// Gestalt processing - holistic state integration
	
	float GestaltIntegration = 0.0f;
	GestaltIntegration += CognitiveState.AttentionFocus * 0.3f;
	GestaltIntegration += CognitiveState.EnergyLevel * 0.3f;
	GestaltIntegration += (1.0f - CognitiveState.CognitiveLoad) * 0.4f;

	// Update overall state
	CognitiveState.AttentionFocus = FMath::Clamp(GestaltIntegration, 0.0f, 1.0f);
}

void APyperActor::CalculatePrimeResonance()
{
	// Prime factor resonance calculations
	// Simplified implementation using prime-inspired patterns
	
	static const TArray<int32> Primes = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
	
	float ResonanceSum = 0.0f;
	for (int32 Prime : Primes)
	{
		float PrimeWave = FMath::Sin(CognitiveTimer * Prime * 0.1f);
		ResonanceSum += PrimeWave / Prime;
	}

	// Modulate energy level with resonance
	float ResonanceModulation = ResonanceSum * 0.1f;
	CognitiveState.EnergyLevel = FMath::Clamp(
		CognitiveState.EnergyLevel + ResonanceModulation,
		0.5f,
		1.0f
	);
}

void APyperActor::UpdateVisualFromCognition()
{
	if (!CubismModelComponent)
		return;

	// Map cognitive state to visual parameters
	
	// Eye openness based on energy
	float EyeOpen = CognitiveState.EnergyLevel;
	CubismModelComponent->SetParameter(TEXT("EyeOpen"), EyeOpen);

	// Body angle based on attention
	float BodyAngle = (CognitiveState.AttentionFocus - 0.5f) * 20.0f;
	CubismModelComponent->SetParameter(TEXT("BodyAngleX"), BodyAngle);

	// Head angle based on cognitive load
	float HeadAngle = (CognitiveState.CognitiveLoad - 0.5f) * 15.0f;
	CubismModelComponent->SetParameter(TEXT("AngleY"), HeadAngle);
}

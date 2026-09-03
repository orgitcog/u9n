// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Enhanced Rendering Implementation

#include "DeepTreeEchoRenderingSubsystem.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Character/DeepTreeEchoCharacter.h"

void UDeepTreeEchoRenderingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Initialize default quality settings
	CurrentQualitySettings = FRenderQualitySettings();

	// Create post-process component
	UWorld* World = GetWorld();
	if (World)
	{
		PostProcessComponent = NewObject<UPostProcessComponent>(World->GetWorldSettings());
		if (PostProcessComponent)
		{
			PostProcessComponent->bEnabled = true;
			PostProcessComponent->bUnbound = true;
			PostProcessComponent->Priority = 1.0f;
			PostProcessComponent->RegisterComponent();
		}
	}

	// Apply initial quality settings
	ApplyQualitySettings();

	// Initialize performance tracking
	FrameTimeAccumulator = 0.0f;
	FrameCount = 0;
	AverageFrameRate = 60.0f;
}

void UDeepTreeEchoRenderingSubsystem::Deinitialize()
{
	// Cleanup post-process component
	if (PostProcessComponent)
	{
		PostProcessComponent->DestroyComponent();
		PostProcessComponent = nullptr;
	}

	// Clear registered characters
	RegisteredCharacters.Empty();

	Super::Deinitialize();
}

UDeepTreeEchoRenderingSubsystem* UDeepTreeEchoRenderingSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UDeepTreeEchoRenderingSubsystem>();
}

void UDeepTreeEchoRenderingSubsystem::SetRenderQuality(const FRenderQualitySettings& Settings)
{
	CurrentQualitySettings = Settings;
	ApplyQualitySettings();
}

void UDeepTreeEchoRenderingSubsystem::SetRayTracingEnabled(bool bEnabled)
{
	CurrentQualitySettings.bEnableRayTracing = bEnabled;
	
	// Apply ray tracing settings
	static IConsoleVariable* CVarRayTracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
	if (CVarRayTracing)
	{
		CVarRayTracing->Set(bEnabled ? 1 : 0);
	}
}

void UDeepTreeEchoRenderingSubsystem::SetLumenEnabled(bool bEnabled)
{
	CurrentQualitySettings.bEnableLumen = bEnabled;
	
	// Apply Lumen settings
	static IConsoleVariable* CVarLumenGI = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"));
	if (CVarLumenGI)
	{
		CVarLumenGI->Set(bEnabled ? 1 : 0);
	}
	
	static IConsoleVariable* CVarLumenReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.Allow"));
	if (CVarLumenReflections)
	{
		CVarLumenReflections->Set(bEnabled ? 1 : 0);
	}
}

void UDeepTreeEchoRenderingSubsystem::SetNaniteEnabled(bool bEnabled)
{
	CurrentQualitySettings.bEnableNanite = bEnabled;
	
	// Apply Nanite settings
	static IConsoleVariable* CVarNanite = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
	if (CVarNanite)
	{
		CVarNanite->Set(bEnabled ? 1 : 0);
	}
}

void UDeepTreeEchoRenderingSubsystem::SetGlobalEmotionalAura(FLinearColor Color, float Intensity)
{
	if (!GlobalMaterialParameters)
	{
		// Try to load the global material parameter collection
		GlobalMaterialParameters = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/DeepTreeEcho/Materials/MPC_GlobalParameters"));
	}

	if (GlobalMaterialParameters)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(GlobalMaterialParameters);
			if (Instance)
			{
				Instance->SetVectorParameterValue(TEXT("GlobalEmotionalAuraColor"), Color);
				Instance->SetScalarParameterValue(TEXT("GlobalEmotionalAuraIntensity"), Intensity);
			}
		}
	}
}

void UDeepTreeEchoRenderingSubsystem::SetGlobalCognitiveVisualization(float Intensity)
{
	if (!GlobalMaterialParameters)
	{
		GlobalMaterialParameters = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/DeepTreeEcho/Materials/MPC_GlobalParameters"));
	}

	if (GlobalMaterialParameters)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(GlobalMaterialParameters);
			if (Instance)
			{
				Instance->SetScalarParameterValue(TEXT("GlobalCognitiveIntensity"), Intensity);
			}
		}
	}
}

void UDeepTreeEchoRenderingSubsystem::EnableCinematicMode(bool bEnabled, float TransitionTime)
{
	if (!PostProcessComponent)
	{
		return;
	}

	// Configure cinematic post-process settings
	FPostProcessSettings& Settings = PostProcessComponent->Settings;

	if (bEnabled)
	{
		// Cinematic depth of field
		Settings.bOverride_DepthOfFieldFocalDistance = true;
		Settings.DepthOfFieldFocalDistance = 200.0f;
		Settings.bOverride_DepthOfFieldFstop = true;
		Settings.DepthOfFieldFstop = 2.8f;

		// Cinematic motion blur
		Settings.bOverride_MotionBlurAmount = true;
		Settings.MotionBlurAmount = 0.5f;

		// Film grain
		Settings.bOverride_FilmGrainIntensity = true;
		Settings.FilmGrainIntensity = 0.1f;

		// Vignette
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.4f;
	}
	else
	{
		// Reset to default
		Settings.bOverride_DepthOfFieldFocalDistance = false;
		Settings.bOverride_DepthOfFieldFstop = false;
		Settings.bOverride_MotionBlurAmount = false;
		Settings.bOverride_FilmGrainIntensity = false;
		Settings.bOverride_VignetteIntensity = false;
	}

	PostProcessComponent->MarkRenderStateDirty();
}

void UDeepTreeEchoRenderingSubsystem::SetDepthOfFieldSettings(float FocalDistance, float Aperture)
{
	if (!PostProcessComponent)
	{
		return;
	}

	FPostProcessSettings& Settings = PostProcessComponent->Settings;
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = FocalDistance;
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = Aperture;

	PostProcessComponent->MarkRenderStateDirty();
}

void UDeepTreeEchoRenderingSubsystem::SetMotionBlurAmount(float Amount)
{
	if (!PostProcessComponent)
	{
		return;
	}

	FPostProcessSettings& Settings = PostProcessComponent->Settings;
	Settings.bOverride_MotionBlurAmount = true;
	Settings.MotionBlurAmount = FMath::Clamp(Amount, 0.0f, 1.0f);

	PostProcessComponent->MarkRenderStateDirty();
}

void UDeepTreeEchoRenderingSubsystem::RegisterCharacter(ADeepTreeEchoCharacter* Character)
{
	if (Character && !RegisteredCharacters.Contains(Character))
	{
		RegisteredCharacters.Add(Character);
		OptimizeLODs();
	}
}

void UDeepTreeEchoRenderingSubsystem::UnregisterCharacter(ADeepTreeEchoCharacter* Character)
{
	if (Character)
	{
		RegisteredCharacters.Remove(Character);
	}
}

float UDeepTreeEchoRenderingSubsystem::GetCurrentFrameRate() const
{
	return AverageFrameRate;
}

float UDeepTreeEchoRenderingSubsystem::GetCurrentGPUTime() const
{
	// Get GPU frame time from stats
	static IConsoleVariable* CVarGPUTime = IConsoleManager::Get().FindConsoleVariable(TEXT("stat.GPU"));
	if (CVarGPUTime)
	{
		return CVarGPUTime->GetFloat();
	}
	return 16.67f; // Default to 60 FPS equivalent
}

void UDeepTreeEchoRenderingSubsystem::OptimizeForPerformance()
{
	// Check current frame rate
	float CurrentFPS = GetCurrentFrameRate();
	int32 TargetFPS = CurrentQualitySettings.TargetFrameRate;

	// If below target, reduce quality
	if (CurrentFPS < TargetFPS * 0.9f)
	{
		// Reduce shadow quality
		if (CurrentQualitySettings.ShadowQuality > 2)
		{
			CurrentQualitySettings.ShadowQuality--;
			ApplyQualitySettings();
		}
		// Reduce post-process quality
		else if (CurrentQualitySettings.PostProcessQuality > 2)
		{
			CurrentQualitySettings.PostProcessQuality--;
			ApplyQualitySettings();
		}
		// Disable ray tracing if still struggling
		else if (CurrentQualitySettings.bEnableRayTracing)
		{
			SetRayTracingEnabled(false);
		}
	}
	// If well above target, increase quality
	else if (CurrentFPS > TargetFPS * 1.2f)
	{
		// Increase shadow quality
		if (CurrentQualitySettings.ShadowQuality < 4)
		{
			CurrentQualitySettings.ShadowQuality++;
			ApplyQualitySettings();
		}
		// Increase post-process quality
		else if (CurrentQualitySettings.PostProcessQuality < 4)
		{
			CurrentQualitySettings.PostProcessQuality++;
			ApplyQualitySettings();
		}
		// Enable ray tracing if hardware can handle it
		else if (!CurrentQualitySettings.bEnableRayTracing)
		{
			SetRayTracingEnabled(true);
		}
	}

	// Optimize LODs for registered characters
	OptimizeLODs();

	// Update dynamic resolution
	UpdateDynamicResolution();
}

void UDeepTreeEchoRenderingSubsystem::ApplyQualitySettings()
{
	// Apply ray tracing
	SetRayTracingEnabled(CurrentQualitySettings.bEnableRayTracing);

	// Apply Lumen
	SetLumenEnabled(CurrentQualitySettings.bEnableLumen);

	// Apply Nanite
	SetNaniteEnabled(CurrentQualitySettings.bEnableNanite);

	// Apply virtual shadow maps
	static IConsoleVariable* CVarVSM = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"));
	if (CVarVSM)
	{
		CVarVSM->Set(CurrentQualitySettings.bEnableVirtualShadowMaps ? 1 : 0);
	}

	// Apply texture quality
	static IConsoleVariable* CVarTextureQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize"));
	if (CVarTextureQuality)
	{
		int32 PoolSize = 1000 + (CurrentQualitySettings.TextureQuality * 1000);
		CVarTextureQuality->Set(PoolSize);
	}

	// Apply shadow quality
	static IConsoleVariable* CVarShadowQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MaxResolution"));
	if (CVarShadowQuality)
	{
		int32 ShadowRes = 512 << CurrentQualitySettings.ShadowQuality; // 512, 1024, 2048, 4096, 8192
		CVarShadowQuality->Set(ShadowRes);
	}

	// Apply post-process quality
	static IConsoleVariable* CVarPPQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessAAQuality"));
	if (CVarPPQuality)
	{
		CVarPPQuality->Set(CurrentQualitySettings.PostProcessQuality);
	}
}

void UDeepTreeEchoRenderingSubsystem::UpdateGlobalMaterialParameters()
{
	// Update global material parameters based on registered characters
	// This allows for synchronized visual effects across all characters
}

void UDeepTreeEchoRenderingSubsystem::UpdatePostProcessEffects()
{
	// Update post-process effects based on scene state
}

void UDeepTreeEchoRenderingSubsystem::OptimizeLODs()
{
	// Optimize LODs for registered characters based on distance and importance
	for (ADeepTreeEchoCharacter* Character : RegisteredCharacters)
	{
		if (!Character)
		{
			continue;
		}

		// Get distance to camera
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (!PC || !PC->PlayerCameraManager)
		{
			continue;
		}

		FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
		FVector CharacterLocation = Character->GetActorLocation();
		float Distance = FVector::Dist(CameraLocation, CharacterLocation);

		// Set LOD based on distance
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (Mesh)
		{
			if (Distance < 500.0f)
			{
				Mesh->SetForcedLOD(1); // Highest quality
			}
			else if (Distance < 1500.0f)
			{
				Mesh->SetForcedLOD(2); // Medium quality
			}
			else
			{
				Mesh->SetForcedLOD(3); // Lower quality
			}
		}
	}
}

void UDeepTreeEchoRenderingSubsystem::UpdateDynamicResolution()
{
	// Enable dynamic resolution scaling based on performance
	float CurrentFPS = GetCurrentFrameRate();
	int32 TargetFPS = CurrentQualitySettings.TargetFrameRate;

	static IConsoleVariable* CVarDynRes = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode"));
	if (CVarDynRes)
	{
		if (CurrentFPS < TargetFPS * 0.85f)
		{
			CVarDynRes->Set(2); // Enable dynamic resolution
		}
		else if (CurrentFPS > TargetFPS * 1.1f)
		{
			CVarDynRes->Set(0); // Disable dynamic resolution
		}
	}
}

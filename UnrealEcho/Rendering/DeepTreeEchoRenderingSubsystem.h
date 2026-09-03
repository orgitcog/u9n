// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Enhanced Rendering Subsystem

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepTreeEchoRenderingSubsystem.generated.h"

class UMaterialParameterCollection;
class UPostProcessComponent;
class ADeepTreeEchoCharacter;

/**
 * FRenderQualitySettings
 * 
 * Configuration for rendering quality levels to match AI Angel standards.
 */
USTRUCT(BlueprintType)
struct FRenderQualitySettings
{
	GENERATED_BODY()

	// Enable ray tracing for realistic lighting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	bool bEnableRayTracing = true;

	// Enable Lumen global illumination
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	bool bEnableLumen = true;

	// Enable Nanite virtualized geometry
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	bool bEnableNanite = true;

	// Enable virtual shadow maps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	bool bEnableVirtualShadowMaps = true;

	// Texture quality (0-4, 4 = highest)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality", meta = (ClampMin = "0", ClampMax = "4"))
	int32 TextureQuality = 4;

	// Shadow quality (0-4, 4 = highest)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality", meta = (ClampMin = "0", ClampMax = "4"))
	int32 ShadowQuality = 4;

	// Post-process quality (0-4, 4 = highest)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality", meta = (ClampMin = "0", ClampMax = "4"))
	int32 PostProcessQuality = 4;

	// Target frame rate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	int32 TargetFrameRate = 60;
};

/**
 * UDeepTreeEchoRenderingSubsystem
 *
 * World subsystem that manages enhanced rendering features for the Deep Tree Echo avatar.
 * Implements AI Angel-level visual quality with:
 * - Ray tracing and path tracing
 * - Lumen global illumination
 * - Nanite virtualized geometry
 * - Advanced post-processing effects
 * - Dynamic material parameter control
 * - Performance optimization
 */
UCLASS()
class UNREALENGINE_API UDeepTreeEchoRenderingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of USubsystem interface

	// Get the singleton instance
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Rendering", meta = (WorldContext = "WorldContextObject"))
	static UDeepTreeEchoRenderingSubsystem* Get(const UObject* WorldContextObject);

	// Configure rendering quality
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetRenderQuality(const FRenderQualitySettings& Settings);

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Rendering")
	FRenderQualitySettings GetRenderQuality() const { return CurrentQualitySettings; }

	// Enable/disable specific features
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetRayTracingEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetLumenEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetNaniteEnabled(bool bEnabled);

	// Global material parameter control
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetGlobalEmotionalAura(FLinearColor Color, float Intensity);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetGlobalCognitiveVisualization(float Intensity);

	// Post-process effects
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void EnableCinematicMode(bool bEnabled, float TransitionTime = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetDepthOfFieldSettings(float FocalDistance, float Aperture);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void SetMotionBlurAmount(float Amount);

	// Character-specific rendering
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void RegisterCharacter(ADeepTreeEchoCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void UnregisterCharacter(ADeepTreeEchoCharacter* Character);

	// Performance monitoring
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Rendering")
	float GetCurrentFrameRate() const;

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Rendering")
	float GetCurrentGPUTime() const;

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Rendering")
	void OptimizeForPerformance();

protected:
	// Current quality settings
	UPROPERTY()
	FRenderQualitySettings CurrentQualitySettings;

	// Material parameter collection for global control
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> GlobalMaterialParameters;

	// Post-process component for effects
	UPROPERTY()
	TObjectPtr<UPostProcessComponent> PostProcessComponent;

	// Registered characters for rendering optimization
	UPROPERTY()
	TArray<TObjectPtr<ADeepTreeEchoCharacter>> RegisteredCharacters;

	// Internal functions
	void ApplyQualitySettings();
	void UpdateGlobalMaterialParameters();
	void UpdatePostProcessEffects();
	void OptimizeLODs();
	void UpdateDynamicResolution();

	// Performance tracking
	float FrameTimeAccumulator;
	int32 FrameCount;
	float AverageFrameRate;
};

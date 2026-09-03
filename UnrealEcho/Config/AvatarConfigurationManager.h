//=============================================================================
// AvatarConfigurationManager.h
// 
// Configuration management system for Deep Tree Echo avatar.
// Handles loading, saving, and managing all avatar configuration settings.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AvatarConfigurationManager.generated.h"

/**
 * Avatar visual configuration
 */
USTRUCT(BlueprintType)
struct FAvatarVisualConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ModelPath;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor HairColor = FLinearColor(0.59f, 0.86f, 0.90f); // Pastel cyan
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor EyeColor = FLinearColor(0.4f, 0.2f, 0.1f); // Brown/amber
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EyeSparkleIntensity = 0.7f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HairShimmerIntensity = 0.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BlushIntensity = 0.3f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePhysics = true;
};

/**
 * Personality configuration
 */
USTRUCT(BlueprintType)
struct FPersonalityConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ConfidenceBaseline = 0.8f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CharmBaseline = 0.7f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PlayfulnessBaseline = 0.6f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UnpredictabilityBaseline = 0.7f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VolatilityBaseline = 0.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ImpulsivityBaseline = 0.6f;
};

/**
 * Neurochemical configuration
 */
USTRUCT(BlueprintType)
struct FNeurochemicalConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DopamineBaseline = 0.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SerotoninBaseline = 0.6f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NorepinephrineBaseline = 0.4f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CortisolBaseline = 0.2f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DecayRate = 0.1f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HomeostasisStrength = 0.5f;
};

/**
 * Narrative loop configuration
 */
USTRUCT(BlueprintType)
struct FNarrativeConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DiaryUpdateInterval = 60.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InsightGenerationInterval = 300.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BlogPublishInterval = 900.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InsightThreshold = 0.7f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoPublish = true;
};

/**
 * Performance configuration
 */
USTRUCT(BlueprintType)
struct FPerformanceConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TargetFPS = 60.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxTriangles = 100000;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxDrawCalls = 1000;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxMemoryMB = 512.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePerformanceMonitoring = true;
};

/**
 * Complete avatar configuration
 */
USTRUCT(BlueprintType)
struct FAvatarConfiguration
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConfigurationName = TEXT("Default");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConfigurationVersion = TEXT("1.0");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAvatarVisualConfig VisualConfig;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPersonalityConfig PersonalityConfig;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNeurochemicalConfig NeurochemicalConfig;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNarrativeConfig NarrativeConfig;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPerformanceConfig PerformanceConfig;
};

/**
 * Configuration manager class
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UAvatarConfigurationManager : public UObject
{
    GENERATED_BODY()

public:
    UAvatarConfigurationManager();
    
    // Singleton access
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    static UAvatarConfigurationManager* GetInstance();
    
    // Configuration management
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool LoadConfiguration(const FString& ConfigName);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool SaveConfiguration(const FString& ConfigName);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool LoadConfigurationFromFile(const FString& FilePath);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool SaveConfigurationToFile(const FString& FilePath);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void ResetToDefaults();
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    TArray<FString> GetAvailableConfigurations() const;
    
    // Configuration access
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    FAvatarConfiguration GetCurrentConfiguration() const { return CurrentConfiguration; }
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void SetCurrentConfiguration(const FAvatarConfiguration& NewConfig);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    FAvatarVisualConfig GetVisualConfig() const { return CurrentConfiguration.VisualConfig; }
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    FPersonalityConfig GetPersonalityConfig() const { return CurrentConfiguration.PersonalityConfig; }
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    FNeurochemicalConfig GetNeurochemicalConfig() const { return CurrentConfiguration.NeurochemicalConfig; }
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    FNarrativeConfig GetNarrativeConfig() const { return CurrentConfiguration.NarrativeConfig; }
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    FPerformanceConfig GetPerformanceConfig() const { return CurrentConfiguration.PerformanceConfig; }
    
    // Configuration modification
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void SetVisualConfig(const FAvatarVisualConfig& NewConfig);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void SetPersonalityConfig(const FPersonalityConfig& NewConfig);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void SetNeurochemicalConfig(const FNeurochemicalConfig& NewConfig);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void SetNarrativeConfig(const FNarrativeConfig& NewConfig);
    
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    void SetPerformanceConfig(const FPerformanceConfig& NewConfig);
    
    // Validation
    UFUNCTION(BlueprintCallable, Category = "Configuration")
    bool ValidateConfiguration(const FAvatarConfiguration& Config, FString& OutErrorMessage) const;

private:
    static UAvatarConfigurationManager* Instance;
    
    UPROPERTY()
    FAvatarConfiguration CurrentConfiguration;
    
    UPROPERTY()
    FAvatarConfiguration DefaultConfiguration;
    
    void InitializeDefaults();
    FString GetConfigDirectory() const;
    FString ConfigToJson(const FAvatarConfiguration& Config) const;
    bool JsonToConfig(const FString& JsonString, FAvatarConfiguration& OutConfig) const;
};

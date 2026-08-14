#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "VirtualEnvironmentManager.generated.h"

// Forward declarations
class UDynamicLightingSystem;
class UParticleEffectManager;

/**
 * Virtual Environment Manager
 * 
 * Manages dynamic environmental systems including:
 * - Day/night cycle with realistic sun positioning
 * - Dynamic weather simulation
 * - Atmospheric effects (fog, density, sky brightness)
 * - Wind simulation with directional variation
 * - Dynamic lighting based on time and weather
 * 
 * This component creates an immersive virtual environment for AGI avatar interactions.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UVirtualEnvironmentManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UVirtualEnvironmentManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Sub-component systems
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    UDynamicLightingSystem* LightingSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    UParticleEffectManager* ParticleManager;

    // Day/Night Cycle
    UFUNCTION(BlueprintCallable, Category = "Environment|Time")
    void SetTimeOfDay(float Hours);
    
    UFUNCTION(BlueprintPure, Category = "Environment|Time")
    float GetTimeOfDay() const;
    
    UFUNCTION(BlueprintCallable, Category = "Environment|Time")
    void EnableDayNightCycle(bool bEnable);

    // Weather System
    UFUNCTION(BlueprintCallable, Category = "Environment|Weather")
    void SetWeatherIntensity(float Intensity);
    
    UFUNCTION(BlueprintPure, Category = "Environment|Weather")
    float GetWeatherIntensity() const;
    
    UFUNCTION(BlueprintCallable, Category = "Environment|Weather")
    void EnableDynamicWeather(bool bEnable);

    // Wind System
    UFUNCTION(BlueprintCallable, Category = "Environment|Wind")
    void SetWindStrength(float Strength);
    
    UFUNCTION(BlueprintCallable, Category = "Environment|Wind")
    void SetWindDirection(FVector Direction);
    
    UFUNCTION(BlueprintPure, Category = "Environment|Wind")
    float GetWindStrength() const;
    
    UFUNCTION(BlueprintPure, Category = "Environment|Wind")
    FVector GetWindDirection() const;

protected:
    // Initialization methods
    void InitializeSunLight();
    void InitializeSkyLight();
    void InitializeFog();
    void InitializePostProcess();

    // Update methods
    void UpdateDayNightCycle(float DeltaTime);
    void UpdateWeatherSystem(float DeltaTime);
    void UpdateAtmosphere(float DeltaTime);
    void UpdateWindSimulation(float DeltaTime);
    void UpdateLighting(float DeltaTime);

private:
    // Environment actors
    UPROPERTY()
    ADirectionalLight* SunLight;
    
    UPROPERTY()
    ASkyLight* SkyLight;
    
    UPROPERTY()
    AExponentialHeightFog* HeightFog;
    
    UPROPERTY()
    APostProcessVolume* PostProcessVolume;

    // Time system
    UPROPERTY(EditAnywhere, Category = "Environment|Time")
    float CurrentTimeOfDay; // 0-24 hours
    
    UPROPERTY(EditAnywhere, Category = "Environment|Time")
    float TimeScale; // Speed multiplier for day/night cycle
    
    UPROPERTY(EditAnywhere, Category = "Environment|Time")
    bool bEnableDayNightCycle;

    // Weather system
    UPROPERTY(EditAnywhere, Category = "Environment|Weather")
    float WeatherIntensity; // 0-1, affects fog, lighting, wind
    
    UPROPERTY(EditAnywhere, Category = "Environment|Weather")
    bool bEnableDynamicWeather;

    // Atmospheric properties
    UPROPERTY(EditAnywhere, Category = "Environment|Atmosphere")
    float AtmosphericDensity;
    
    UPROPERTY(EditAnywhere, Category = "Environment|Atmosphere")
    float FogDensity;
    
    UPROPERTY(EditAnywhere, Category = "Environment|Atmosphere")
    float SkyBrightness;
    
    UPROPERTY(EditAnywhere, Category = "Environment|Atmosphere")
    float AmbientTemperature;

    // Wind system
    UPROPERTY(EditAnywhere, Category = "Environment|Wind")
    float WindStrength;
    
    UPROPERTY(EditAnywhere, Category = "Environment|Wind")
    FVector WindDirection;
};

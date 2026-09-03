#include "VirtualEnvironmentManager.h"
#include "DynamicLightingSystem.h"
#include "ParticleEffectManager.h"
#include "Engine/World.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/PostProcessVolume.h"

UVirtualEnvironmentManager::UVirtualEnvironmentManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    
    // Initialize default environment settings
    CurrentTimeOfDay = 12.0f; // Noon
    TimeScale = 1.0f;
    WeatherIntensity = 0.0f;
    AmbientTemperature = 20.0f;
    WindStrength = 0.0f;
    WindDirection = FVector(1.0f, 0.0f, 0.0f);
    
    // Initialize atmosphere settings
    AtmosphericDensity = 1.0f;
    FogDensity = 0.02f;
    SkyBrightness = 1.0f;
    
    bEnableDynamicWeather = true;
    bEnableDayNightCycle = true;
}

void UVirtualEnvironmentManager::BeginPlay()
{
    Super::BeginPlay();

    // Initialize sub-components
    if (LightingSystem)
    {
        LightingSystem->RegisterComponent();
    }

    if (ParticleManager)
    {
        ParticleManager->RegisterComponent();
    }
    
    // Find or create directional light for sun
    InitializeSunLight();
    
    // Initialize sky light
    InitializeSkyLight();
    
    // Initialize fog
    InitializeFog();
    
    // Initialize post-process volume
    InitializePostProcess();
    
    UE_LOG(LogTemp, Log, TEXT("Virtual Environment Manager initialized successfully"));
}

void UVirtualEnvironmentManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update day-night cycle
    if (bEnableDayNightCycle)
    {
        UpdateDayNightCycle(DeltaTime);
    }
    
    // Update dynamic weather
    if (bEnableDynamicWeather)
    {
        UpdateWeatherSystem(DeltaTime);
    }
    
    // Update atmospheric effects
    UpdateAtmosphere(DeltaTime);
    
    // Update wind simulation
    UpdateWindSimulation(DeltaTime);
    
    // Update lighting based on time and weather
    UpdateLighting(DeltaTime);
}

void UVirtualEnvironmentManager::InitializeSunLight()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // Try to find existing directional light
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        SunLight = Cast<ADirectionalLight>(FoundActors[0]);
    }
    else
    {
        // Create new directional light for sun
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        SunLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        
        if (SunLight)
        {
            SunLight->SetMobility(EComponentMobility::Movable);
            UE_LOG(LogTemp, Log, TEXT("Created new directional light for sun"));
        }
    }
}

void UVirtualEnvironmentManager::InitializeSkyLight()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // Try to find existing sky light
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, ASkyLight::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        SkyLight = Cast<ASkyLight>(FoundActors[0]);
    }
    else
    {
        // Create new sky light
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        SkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        
        if (SkyLight)
        {
            SkyLight->SetMobility(EComponentMobility::Movable);
            UE_LOG(LogTemp, Log, TEXT("Created new sky light"));
        }
    }
}

void UVirtualEnvironmentManager::InitializeFog()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // Try to find existing exponential height fog
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, AExponentialHeightFog::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        HeightFog = Cast<AExponentialHeightFog>(FoundActors[0]);
    }
    else
    {
        // Create new exponential height fog
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        HeightFog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        
        if (HeightFog)
        {
            UE_LOG(LogTemp, Log, TEXT("Created new exponential height fog"));
        }
    }
}

void UVirtualEnvironmentManager::InitializePostProcess()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // Try to find existing post-process volume
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, APostProcessVolume::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        PostProcessVolume = Cast<APostProcessVolume>(FoundActors[0]);
    }
    else
    {
        // Create new post-process volume
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        PostProcessVolume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        
        if (PostProcessVolume)
        {
            PostProcessVolume->bUnbound = true;
            UE_LOG(LogTemp, Log, TEXT("Created new post-process volume"));
        }
    }
}

void UVirtualEnvironmentManager::UpdateDayNightCycle(float DeltaTime)
{
    // Update time of day (24-hour cycle)
    CurrentTimeOfDay += (DeltaTime * TimeScale) / 3600.0f; // Convert seconds to hours
    
    if (CurrentTimeOfDay >= 24.0f)
    {
        CurrentTimeOfDay -= 24.0f;
    }
    
    // Calculate sun angle based on time of day
    // 0:00 = -90° (midnight), 6:00 = 0° (sunrise), 12:00 = 90° (noon), 18:00 = 180° (sunset)
    float SunAngle = (CurrentTimeOfDay - 6.0f) * 15.0f; // 15 degrees per hour
    
    if (SunLight)
    {
        FRotator SunRotation = FRotator(-SunAngle, 0.0f, 0.0f);
        SunLight->SetActorRotation(SunRotation);
        
        // Adjust sun intensity based on angle
        float SunHeight = FMath::Sin(FMath::DegreesToRadians(SunAngle));
        float SunIntensity = FMath::Clamp(SunHeight, 0.0f, 1.0f) * 10.0f;
        
        if (UDirectionalLightComponent* LightComp = SunLight->GetComponent())
        {
            LightComp->SetIntensity(SunIntensity);
        }
    }
}

void UVirtualEnvironmentManager::UpdateWeatherSystem(float DeltaTime)
{
    // Simulate weather transitions
    static float WeatherTimer = 0.0f;
    WeatherTimer += DeltaTime;
    
    // Change weather every 5 minutes (300 seconds)
    if (WeatherTimer >= 300.0f)
    {
        WeatherTimer = 0.0f;
        
        // Randomly change weather intensity
        float TargetWeatherIntensity = FMath::RandRange(0.0f, 1.0f);
        WeatherIntensity = FMath::Lerp(WeatherIntensity, TargetWeatherIntensity, 0.1f);
    }
    
    // Smoothly interpolate weather intensity
    WeatherIntensity = FMath::Lerp(WeatherIntensity, WeatherIntensity, DeltaTime * 0.5f);
    
    // Update fog density based on weather
    FogDensity = FMath::Lerp(0.02f, 0.1f, WeatherIntensity);
    
    if (HeightFog)
    {
        if (UExponentialHeightFogComponent* FogComp = HeightFog->GetComponent())
        {
            FogComp->SetFogDensity(FogDensity);
        }
    }
}

void UVirtualEnvironmentManager::UpdateAtmosphere(float DeltaTime)
{
    // Update atmospheric density based on altitude and weather
    AtmosphericDensity = FMath::Lerp(1.0f, 0.5f, WeatherIntensity);
    
    // Update sky brightness based on time of day
    float TimeNormalized = CurrentTimeOfDay / 24.0f;
    float DayBrightness = FMath::Sin(TimeNormalized * PI * 2.0f);
    SkyBrightness = FMath::Clamp(DayBrightness, 0.1f, 1.0f);
    
    if (SkyLight)
    {
        if (USkyLightComponent* SkyComp = SkyLight->GetComponent())
        {
            SkyComp->SetIntensity(SkyBrightness);
        }
    }
}

void UVirtualEnvironmentManager::UpdateWindSimulation(float DeltaTime)
{
    // Simulate wind with Perlin-like noise
    static float WindTimer = 0.0f;
    WindTimer += DeltaTime;
    
    // Update wind strength with smooth variation
    float WindNoise = FMath::Sin(WindTimer * 0.5f) * 0.5f + 0.5f;
    WindStrength = FMath::Lerp(0.0f, 10.0f, WindNoise * WeatherIntensity);
    
    // Update wind direction with smooth rotation
    float WindAngle = FMath::Sin(WindTimer * 0.2f) * 180.0f;
    WindDirection = FVector(FMath::Cos(FMath::DegreesToRadians(WindAngle)), FMath::Sin(FMath::DegreesToRadians(WindAngle)), 0.0f);
    WindDirection.Normalize();
}

void UVirtualEnvironmentManager::UpdateLighting(float DeltaTime)
{
    // Update lighting based on time of day and weather
    if (SunLight)
    {
        // Adjust sun color based on time of day
        float TimeNormalized = CurrentTimeOfDay / 24.0f;
        FLinearColor SunColor;
        
        if (CurrentTimeOfDay >= 5.0f && CurrentTimeOfDay < 7.0f)
        {
            // Sunrise - orange/red tint
            float SunriseProgress = (CurrentTimeOfDay - 5.0f) / 2.0f;
            SunColor = FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.3f, 0.1f), FLinearColor(1.0f, 0.9f, 0.7f), SunriseProgress);
        }
        else if (CurrentTimeOfDay >= 7.0f && CurrentTimeOfDay < 17.0f)
        {
            // Daytime - white/yellow
            SunColor = FLinearColor(1.0f, 0.95f, 0.8f);
        }
        else if (CurrentTimeOfDay >= 17.0f && CurrentTimeOfDay < 19.0f)
        {
            // Sunset - orange/red tint
            float SunsetProgress = (CurrentTimeOfDay - 17.0f) / 2.0f;
            SunColor = FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.9f, 0.7f), FLinearColor(1.0f, 0.3f, 0.1f), SunsetProgress);
        }
        else
        {
            // Night - blue tint
            SunColor = FLinearColor(0.1f, 0.15f, 0.3f);
        }
        
        // Apply weather influence
        SunColor = FLinearColor::LerpUsingHSV(SunColor, FLinearColor(0.5f, 0.5f, 0.5f), WeatherIntensity * 0.5f);
        
        if (UDirectionalLightComponent* LightComp = SunLight->GetComponent())
        {
            LightComp->SetLightColor(SunColor);
        }
    }
}

void UVirtualEnvironmentManager::SetTimeOfDay(float Hours)
{
    CurrentTimeOfDay = FMath::Clamp(Hours, 0.0f, 24.0f);
}

void UVirtualEnvironmentManager::SetWeatherIntensity(float Intensity)
{
    WeatherIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UVirtualEnvironmentManager::SetWindStrength(float Strength)
{
    WindStrength = FMath::Max(0.0f, Strength);
}

void UVirtualEnvironmentManager::SetWindDirection(FVector Direction)
{
    WindDirection = Direction;
    WindDirection.Normalize();
}

float UVirtualEnvironmentManager::GetTimeOfDay() const
{
    return CurrentTimeOfDay;
}

float UVirtualEnvironmentManager::GetWeatherIntensity() const
{
    return WeatherIntensity;
}

float UVirtualEnvironmentManager::GetWindStrength() const
{
    return WindStrength;
}

FVector UVirtualEnvironmentManager::GetWindDirection() const
{
    return WindDirection;
}

void UVirtualEnvironmentManager::EnableDayNightCycle(bool bEnable)
{
    bEnableDayNightCycle = bEnable;
}

void UVirtualEnvironmentManager::EnableDynamicWeather(bool bEnable)
{
    bEnableDynamicWeather = bEnable;
}

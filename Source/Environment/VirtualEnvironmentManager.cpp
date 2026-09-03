#include "VirtualEnvironmentManager.h"
#include "DynamicLightingSystem.h"
#include "ParticleEffectManager.h"

UVirtualEnvironmentManager::UVirtualEnvironmentManager()
{
    PrimaryComponentTick.bCanEverTick = true;
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
}

void UVirtualEnvironmentManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Tick sub-components
}

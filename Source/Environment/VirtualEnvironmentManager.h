#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualEnvironmentManager.generated.h"

// Forward declarations
class UDynamicLightingSystem;
class UParticleEffectManager;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    class UDynamicLightingSystem* LightingSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    class UParticleEffectManager* ParticleManager;
};

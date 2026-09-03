// Perception/AIPerceptionComponent.h - Stub for standalone compilation testing
#pragma once
#include "CoreMinimal.h"

struct FActorPerceptionBlueprintInfo {
    AActor* Target = nullptr;
    TArray<bool> LastSensedStimuli;
    bool bIsHostile = false;
};

class UAIPerceptionComponent : public UActorComponent {
public:
    void RequestStimuliListenerUpdate() {}
    void SetSenseEnabled(int SenseClass, bool bEnabled) {}
    void GetCurrentlyPerceivedActors(UClass* SenseToUse, TArray<AActor*>& OutActors) const {}
    void GetKnownPerceivedActors(UClass* SenseToUse, TArray<AActor*>& OutActors) const {}
    bool GetActorsPerception(AActor* Actor, FActorPerceptionBlueprintInfo& Info) const { return false; }
};

class UAISenseConfig : public UObject {
public:
    float MaxAge = 5.0f;
};

class UAISenseConfig_Sight : public UAISenseConfig {
public:
    float SightRadius = 3000.0f;
    float LoseSightRadius = 3500.0f;
    float PeripheralVisionAngleDegrees = 90.0f;
};

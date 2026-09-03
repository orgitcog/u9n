// Kismet/GameplayStatics.h - Stub for standalone compilation testing
#pragma once
#include "CoreMinimal.h"

class UGameplayStatics {
public:
    static AActor* GetPlayerPawn(const UObject* WorldContextObject, int32 PlayerIndex) { return nullptr; }
    static APlayerController* GetPlayerController(const UObject* WorldContextObject, int32 PlayerIndex) { return nullptr; }
    static UWorld* GetWorld(const UObject* WorldContextObject) { return nullptr; }
    static float GetTimeSeconds(const UObject* WorldContextObject) { return 0.0f; }
    static float GetRealTimeSeconds(const UObject* WorldContextObject) { return 0.0f; }
    static void SetGamePaused(const UObject* WorldContextObject, bool bPaused) {}
    static bool IsGamePaused(const UObject* WorldContextObject) { return false; }
    
    static AActor* SpawnActor(UClass* Class, const FTransform& Transform) { return nullptr; }
    
    template<typename T>
    static T* SpawnActorDeferred(UClass* Class, const FTransform& Transform) { return nullptr; }
    
    static void PlaySound2D(const UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier = 1.0f) {}
    static void PlaySoundAtLocation(const UObject* WorldContextObject, class USoundBase* Sound, FVector Location) {}
    
    static TArray<AActor*> GetAllActorsOfClass(const UObject* WorldContextObject, UClass* ActorClass) { return TArray<AActor*>(); }
    
    template<typename T>
    static void GetAllActorsOfClass(const UObject* WorldContextObject, TArray<T*>& OutActors) {}
};

class USoundBase : public UObject {};

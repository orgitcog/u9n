#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
class UActorComponent;

enum class EAvatarEmotionalState : uint8;

class FDeepTreeEchoTestUtilities
{
public:
    // Spawns a complete avatar with all necessary components for testing
    static AActor* SpawnCompleteAvatar(UWorld* World);

    // Sets the avatar to a specific emotional state
    static void SetEmotionalState(AActor* Avatar, EAvatarEmotionalState Emotion, float Intensity);

    // Sets the avatar's cognitive load
    static void SetCognitiveLoad(AActor* Avatar, float Load);

    // Captures a screenshot for visual validation
    static void CaptureScreenshot(const FString& Name);

    // Compares two images to check for visual regressions
    static bool CompareImages(const FString& ImageA, const FString& ImageB, float Tolerance = 0.95f);

    // Gets performance statistics for a specific component
    static FString GetComponentStats(UActorComponent* Component);
};

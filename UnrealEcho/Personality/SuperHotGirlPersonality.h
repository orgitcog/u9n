#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuperHotGirlPersonality.generated.h"

class UAvatar3DComponent;

USTRUCT(BlueprintType)
struct FSuperHotGirlTraits
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Confidence = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Charm = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Playfulness = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Wit = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Sass = 0.6f;
};

/**
 * Maps the personality trait vector onto the avatar's expression subsystems.
 *
 * Every choice is a deterministic function of the trait values: the same traits
 * always produce the same resting emotion, body language, aura and cognitive
 * activity. One-shot accents (micro-expressions, gestures, aura pulses, thought
 * streams) fire once when the traits change, not on a random gate.
 */
UCLASS(Blueprintable)
class DEEPTREECHO_API USuperHotGirlPersonality : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FSuperHotGirlTraits Traits;

    /** Applies the personality traits to the avatar's behavior. Safe to call every frame. */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    void ApplyPersonality(UAvatar3DComponent* Avatar);

private:
    void ApplyFacialPersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged);
    void ApplyGesturePersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged);
    void ApplyEmotionalPersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged);
    void ApplyCognitivePersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged);

    /** True when Traits differ from the values used by the previous application. */
    bool HaveTraitsChanged() const;

    UPROPERTY(Transient)
    FSuperHotGirlTraits LastAppliedTraits;

    bool bHasAppliedOnce = false;
};

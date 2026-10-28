#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HyperChaoticBehavior.generated.h"

class UAvatar3DComponent;
class UDTELanguageModelBridge;

USTRUCT(BlueprintType)
struct FHyperChaoticProperties
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Randomness = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Unpredictability = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float EmotionalVolatility = 0.6f;
};

/**
 * Affect readings taken from the neurochemical simulation on the avatar's owner.
 * Plain data (not reflected); every value is in [0, 1].
 */
struct FHyperChaoticAffect
{
    float Happiness = 0.0f;
    float Excitement = 0.0f;
    float Calmness = 0.0f;
    float Anxiety = 0.0f;
    float Affection = 0.0f;
    float Focus = 0.0f;
    float Energy = 0.0f;
    float SocialDesire = 0.0f;
    float Clarity = 0.0f;
    float Stability = 0.0f;

    /** One-line summary used as model context and in logs. */
    FString Describe() const;
};

/**
 * Hyper-chaotic behaviour modulation.
 *
 * The chaos parameters shape HOW the avatar expresses itself: how quickly it
 * shifts, how strongly, how often it re-evaluates. They never decide WHAT it
 * expresses. The expression shown at any moment is either chosen by the
 * language model from the rig's menus with the live affect state as context,
 * or derived deterministically from the neurochemical affect state found on
 * the avatar's owner. With neither available the avatar is left in its current
 * state and nothing is synthesised.
 */
UCLASS(Blueprintable)
class DEEPTREECHO_API UHyperChaoticBehavior : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FHyperChaoticProperties Properties;

    /** Seconds between expression re-evaluations when Unpredictability is 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "0.5"))
    float SlowestCadenceSeconds = 8.0f;

    /** Seconds between expression re-evaluations when Unpredictability is 1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "0.5"))
    float FastestCadenceSeconds = 2.0f;

    /**
     * Applies the hyper-chaotic properties to the avatar's behavior. Safe to
     * call every frame; the expression is re-evaluated on the chaos cadence and
     * the numeric chaos dynamics advance every call.
     */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    void ApplyBehavior(UAvatar3DComponent* Avatar);

    /** True while the model is being asked which expression to show. */
    UFUNCTION(BlueprintPure, Category = "Personality")
    bool IsModelChoicePending() const { return bModelChoicePending; }

private:
    /** Reads the affect state from the owner's neurochemical simulation. False when there is none. */
    bool ReadAffect(const UAvatar3DComponent* Avatar, FHyperChaoticAffect& OutAffect) const;

    /** Asks the model to choose the next expression from the subsystem menus. */
    void RequestModelExpression(UAvatar3DComponent* Avatar, const FHyperChaoticAffect& Affect);

    /** Deterministic affect -> expression mapping used when no model answers. */
    void ApplyAffectMapping(UAvatar3DComponent* Avatar, const FHyperChaoticAffect& Affect);

    /** Numeric chaos dynamics: parameter drift, echo memory, gestalt coherence. Not an expression. */
    void UpdateChaosDynamics(UAvatar3DComponent* Avatar, float Now);

    /** Next sample of the deterministic chaotic driver in [0, 1]. */
    float SampleChaos();

    UPROPERTY(Transient)
    UDTELanguageModelBridge* ModelBridge = nullptr;

    bool bModelChoicePending = false;
    float LastEvaluationTime = -1.0f;
    float LastAutonomyTime = -1.0f;
    float EchoMemory = 0.0f;
    float ChaosState = 0.37f;
};

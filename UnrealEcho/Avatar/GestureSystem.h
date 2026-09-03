#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimMontage.h"
#include "GestureSystem.generated.h"

UENUM(BlueprintType)
enum class EGestureType : uint8
{
    Wave UMETA(DisplayName = "Wave"),
    Point UMETA(DisplayName = "Point"),
    ThumbsUp UMETA(DisplayName = "Thumbs Up"),
    ThumbsDown UMETA(DisplayName = "Thumbs Down"),
    Peace UMETA(DisplayName = "Peace Sign"),
    Heart UMETA(DisplayName = "Heart"),
    Clap UMETA(DisplayName = "Clap"),
    Shrug UMETA(DisplayName = "Shrug"),
    Nod UMETA(DisplayName = "Nod"),
    Shake UMETA(DisplayName = "Shake Head"),
    Bow UMETA(DisplayName = "Bow"),
    Salute UMETA(DisplayName = "Salute"),
    Thinking UMETA(DisplayName = "Thinking"),
    Excited UMETA(DisplayName = "Excited"),
    Confident UMETA(DisplayName = "Confident Pose"),
    Flirty UMETA(DisplayName = "Flirty Gesture"),
    Playful UMETA(DisplayName = "Playful Gesture"),
    Elegant UMETA(DisplayName = "Elegant Pose")
};

UENUM(BlueprintType)
enum class EBodyLanguageState : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Open UMETA(DisplayName = "Open"),
    Closed UMETA(DisplayName = "Closed"),
    Confident UMETA(DisplayName = "Confident"),
    Shy UMETA(DisplayName = "Shy"),
    Aggressive UMETA(DisplayName = "Aggressive"),
    Relaxed UMETA(DisplayName = "Relaxed"),
    Tense UMETA(DisplayName = "Tense"),
    Playful UMETA(DisplayName = "Playful"),
    Seductive UMETA(DisplayName = "Seductive")
};

USTRUCT(BlueprintType)
struct FGestureData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gesture")
    EGestureType Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gesture")
    float Duration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gesture")
    float BlendInTime = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gesture")
    float BlendOutTime = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gesture")
    bool bLooping = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gesture")
    UAnimMontage* AnimationMontage = nullptr;
};

USTRUCT(BlueprintType)
struct FBodyLanguageData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Language")
    FVector SpineRotation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Language")
    FVector ShoulderOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Language")
    FVector HipRotation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Language")
    float ChestExpansion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body Language")
    float ArmOpenness = 0.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UGestureSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UGestureSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Trigger a gesture */
    UFUNCTION(BlueprintCallable, Category = "Gesture")
    void TriggerGesture(EGestureType Gesture, float IntensityMultiplier = 1.0f);

    /** Set body language state */
    UFUNCTION(BlueprintCallable, Category = "Gesture")
    void SetBodyLanguageState(EBodyLanguageState NewState, float TransitionTime = 0.5f);

    /** Get current body language state */
    UFUNCTION(BlueprintPure, Category = "Gesture")
    EBodyLanguageState GetBodyLanguageState() const { return CurrentBodyLanguage; }

    /** Check if gesture is currently playing */
    UFUNCTION(BlueprintPure, Category = "Gesture")
    bool IsGesturePlaying() const { return bGesturePlaying; }

    /** Chain gestures together */
    UFUNCTION(BlueprintCallable, Category = "Gesture")
    void ChainGestures(const TArray<EGestureType>& GestureChain);

    /** Apply procedural animation to skeletal mesh */
    UFUNCTION(BlueprintCallable, Category = "Gesture")
    void ApplyProceduralAnimation(class USkeletalMeshComponent* TargetMesh);

    /** Set gesture context for automatic selection */
    UFUNCTION(BlueprintCallable, Category = "Gesture")
    void SetGestureContext(const FString& Context);

    /** Select appropriate gesture based on context */
    UFUNCTION(BlueprintCallable, Category = "Gesture")
    EGestureType SelectContextualGesture();

private:
    /** Initialize gesture library */
    void InitializeGestureLibrary();

    /** Initialize body language presets */
    void InitializeBodyLanguagePresets();

    /** Update current gesture */
    void UpdateGesture(float DeltaTime);

    /** Update body language transition */
    void UpdateBodyLanguageTransition(float DeltaTime);

    /** Update gesture chain */
    void UpdateGestureChain(float DeltaTime);

    /** Interpolate body language data */
    FBodyLanguageData InterpolateBodyLanguage(const FBodyLanguageData& From, const FBodyLanguageData& To, float Alpha);

    /** Gesture library */
    UPROPERTY()
    TMap<EGestureType, FGestureData> GestureLibrary;

    /** Body language presets */
    UPROPERTY()
    TMap<EBodyLanguageState, FBodyLanguageData> BodyLanguagePresets;

    /** Current body language state */
    UPROPERTY()
    EBodyLanguageState CurrentBodyLanguage;

    /** Target body language state */
    UPROPERTY()
    EBodyLanguageState TargetBodyLanguage;

    /** Body language transition progress */
    float BodyLanguageTransitionProgress;

    /** Body language transition duration */
    float BodyLanguageTransitionDuration;

    /** Current body language data */
    UPROPERTY()
    FBodyLanguageData CurrentBodyLanguageData;

    /** Target body language data */
    UPROPERTY()
    FBodyLanguageData TargetBodyLanguageData;

    /** Is gesture currently playing */
    bool bGesturePlaying;

    /** Current gesture type */
    EGestureType CurrentGesture;

    /** Gesture timer */
    float GestureTimer;

    /** Gesture duration */
    float GestureDuration;

    /** Gesture intensity */
    float GestureIntensity;

    /** Gesture chain queue */
    TArray<EGestureType> GestureChainQueue;

    /** Current gesture context */
    FString CurrentContext;

    /** Context-gesture mappings */
    TMap<FString, TArray<EGestureType>> ContextGestureMappings;
};

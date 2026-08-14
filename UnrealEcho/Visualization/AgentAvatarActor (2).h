#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AgentVisualizationManager.h"
#include "AgentAvatarActor.generated.h"

// Forward declarations
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UWidgetComponent;
class UPostProcessComponent;
class USpotLightComponent;

/**
 * Expression Morph Targets for facial animation
 */
USTRUCT(BlueprintType)
struct FExpressionMorphState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Happiness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Sadness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Anger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Fear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Surprise;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Disgust;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Contempt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Curiosity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Excitement;

    FExpressionMorphState()
        : Happiness(0.0f)
        , Sadness(0.0f)
        , Anger(0.0f)
        , Fear(0.0f)
        , Surprise(0.0f)
        , Disgust(0.0f)
        , Contempt(0.0f)
        , Curiosity(0.0f)
        , Excitement(0.0f)
    {}
};

/**
 * Aura Visual Settings
 */
USTRUCT(BlueprintType)
struct FAuraSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BaseColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PulseFrequency;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PulseAmplitude;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePulse;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bReactToEmotion;

    FAuraSettings()
        : BaseColor(FLinearColor(0.3f, 0.5f, 1.0f, 0.5f))
        , Intensity(1.0f)
        , PulseFrequency(1.0f)
        , PulseAmplitude(0.2f)
        , Radius(100.0f)
        , bEnablePulse(true)
        , bReactToEmotion(true)
    {}
};

/**
 * Agent Label Display Settings
 */
USTRUCT(BlueprintType)
struct FAgentLabelSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowLabel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowStatus;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowCapabilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LabelHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor LabelColor;

    FAgentLabelSettings()
        : DisplayName(TEXT("Agent"))
        , AgentType(TEXT("generic"))
        , bShowLabel(true)
        , bShowStatus(true)
        , bShowCapabilities(false)
        , LabelHeight(200.0f)
        , LabelColor(FLinearColor::White)
    {}
};

/**
 * Delegates for avatar events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvatarStateChanged, const FString&, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAvatarInteraction, const FString&, InteractionType, AActor*, InteractingActor);

/**
 * Agent Avatar Actor
 *
 * Visual representation of an AGI-OS agent in the Unreal Engine world.
 *
 * Features:
 * - Skeletal mesh avatar with facial expressions
 * - Aura effect that reflects emotional state
 * - Label widget showing agent info
 * - LOD system for performance
 * - 9P state synchronization
 *
 * This actor can represent any AGI-OS agent, from Deep Tree Echo to Eliza agents.
 */
UCLASS()
class UNREALENGINE_API AAgentAvatarActor : public AActor
{
    GENERATED_BODY()

public:
    AAgentAvatarActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // ===== Agent Identity =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Identity")
    void SetAgentPath(const FString& Path);

    UFUNCTION(BlueprintPure, Category = "AgentAvatar|Identity")
    FString GetAgentPath() const { return AgentPath; }

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Identity")
    void SetAgentName(const FString& Name);

    UFUNCTION(BlueprintPure, Category = "AgentAvatar|Identity")
    FString GetAgentName() const { return AgentName; }

    // ===== Visual Style =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Style")
    void SetVisualStyle(const FAgentVisualStyle& Style);

    UFUNCTION(BlueprintPure, Category = "AgentAvatar|Style")
    FAgentVisualStyle GetVisualStyle() const { return CurrentStyle; }

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Style")
    void SetAvatarMesh(USkeletalMesh* Mesh);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Style")
    void SetAnimationBlueprint(UAnimBlueprint* AnimBP);

    // ===== Aura Effects =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Aura")
    void SetAuraColor(FLinearColor Color);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Aura")
    void SetAuraIntensity(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Aura")
    void SetAuraSettings(const FAuraSettings& Settings);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Aura")
    void PulseAura(float Intensity, float Duration);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Aura")
    void SetAuraVisible(bool bVisible);

    // ===== Emotional Expression =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Expression")
    void UpdateEmotionalExpression(const TMap<FString, float>& EmotionalState);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Expression")
    void SetExpression(const FExpressionMorphState& Expression);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Expression")
    void BlendToExpression(const FExpressionMorphState& TargetExpression, float BlendTime);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Expression")
    FExpressionMorphState GetCurrentExpression() const { return CurrentExpression; }

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Expression")
    void TriggerMicroExpression(const FString& ExpressionType, float Intensity, float Duration);

    // ===== Activity State =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|State")
    void SetAttentionLevel(float Level);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|State")
    void SetActivityLevel(float Level);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|State")
    void SetOnlineState(bool bOnline);

    UFUNCTION(BlueprintPure, Category = "AgentAvatar|State")
    float GetAttentionLevel() const { return AttentionLevel; }

    UFUNCTION(BlueprintPure, Category = "AgentAvatar|State")
    float GetActivityLevel() const { return ActivityLevel; }

    UFUNCTION(BlueprintPure, Category = "AgentAvatar|State")
    bool IsOnline() const { return bIsOnline; }

    // ===== Label/UI =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Label")
    void SetLabelSettings(const FAgentLabelSettings& Settings);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Label")
    void SetLabelVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Label")
    void SetStatusText(const FString& Status);

    // ===== LOD =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|LOD")
    void SetLODLevel(int32 Level);

    UFUNCTION(BlueprintPure, Category = "AgentAvatar|LOD")
    int32 GetLODLevel() const { return CurrentLODLevel; }

    // ===== Animation =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Animation")
    void PlayAnimation(UAnimSequence* Animation, bool bLoop = false);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Animation")
    void SetIdleVariation(int32 Variation);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Animation")
    void TriggerGesture(const FString& GestureType);

    // ===== Interaction =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Interaction")
    void LookAt(FVector Target);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Interaction")
    void LookAtActor(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Interaction")
    void StartConversation(AActor* OtherActor);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Interaction")
    void EndConversation();

    // ===== Visual Effects =====

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Effects")
    void TriggerThinkingEffect(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Effects")
    void TriggerSpeakingEffect(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "AgentAvatar|Effects")
    void TriggerHighlight(float Duration, FLinearColor Color);

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "AgentAvatar|Events")
    FOnAvatarStateChanged OnAvatarStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "AgentAvatar|Events")
    FOnAvatarInteraction OnAvatarInteraction;

protected:
    // ===== Components =====

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* AvatarMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* AuraMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* AuraParticles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UWidgetComponent* LabelWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USpotLightComponent* AuraLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* ThinkingParticles;

    // ===== Materials =====

    UPROPERTY()
    UMaterialInstanceDynamic* AuraMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* AvatarMaterial;

    // ===== Internal Methods =====

    void UpdateAuraVisuals(float DeltaTime);
    void UpdateExpressionBlend(float DeltaTime);
    void ApplyExpressionToMesh();
    void UpdateLabelWidget();
    void ApplyLODSettings();
    FLinearColor CalculateEmotionColor(const TMap<FString, float>& EmotionalState);

private:
    // ===== Identity =====

    UPROPERTY()
    FString AgentPath;

    UPROPERTY()
    FString AgentName;

    // ===== Style =====

    UPROPERTY()
    FAgentVisualStyle CurrentStyle;

    // ===== Aura State =====

    UPROPERTY()
    FAuraSettings AuraSettings;

    float AuraPulseTimer;
    float AuraPulseTarget;
    float AuraPulseDuration;

    // ===== Expression State =====

    UPROPERTY()
    FExpressionMorphState CurrentExpression;

    UPROPERTY()
    FExpressionMorphState TargetExpression;

    float ExpressionBlendAlpha;
    float ExpressionBlendTime;
    bool bIsBlendingExpression;

    // ===== Activity State =====

    float AttentionLevel;
    float ActivityLevel;
    bool bIsOnline;

    // ===== Label =====

    UPROPERTY()
    FAgentLabelSettings LabelSettings;

    FString CurrentStatusText;

    // ===== LOD =====

    int32 CurrentLODLevel;

    // ===== Interaction =====

    UPROPERTY()
    AActor* LookAtTarget;

    UPROPERTY()
    AActor* ConversationPartner;

    // ===== Effects State =====

    bool bIsThinking;
    bool bIsSpeaking;

    // ===== Timers =====

    float MicroExpressionTimer;
    float HighlightTimer;
};

// DeepTreeEchoBlueprintLibrary.h
// Blueprint Function Library for Deep Tree Echo Cognitive Framework
// Provides easy access to all cognitive systems from Blueprints

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "../../Core/CognitiveCycleManager.h"
#include "../../Wisdom/RelevanceRealizationEnnead.h"
#include "../../Wisdom/WisdomCultivation.h"
#include "../../Embodied/SensorimotorIntegration.h"
#include "../../Introspection/AutognosisSystem.h"
#include "DeepTreeEchoBlueprintLibrary.generated.h"

/**
 * Deep Tree Echo Blueprint Function Library
 * Provides static functions for accessing cognitive systems from Blueprints
 */
UCLASS()
class DEEPTREEECHO_API UDeepTreeEchoBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================
    // COMPONENT GETTERS
    // ========================================

    /** Get the Cognitive Cycle Manager from an actor */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Components", 
              meta = (DefaultToSelf = "Actor"))
    static UCognitiveCycleManager* GetCognitiveCycleManager(AActor* Actor);

    /** Get the Relevance Realization Ennead from an actor */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Components", 
              meta = (DefaultToSelf = "Actor"))
    static URelevanceRealizationEnnead* GetRelevanceRealizationEnnead(AActor* Actor);

    /** Get the Wisdom Cultivation component from an actor */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Components", 
              meta = (DefaultToSelf = "Actor"))
    static UWisdomCultivation* GetWisdomCultivation(AActor* Actor);

    /** Get the Sensorimotor Integration component from an actor */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Components", 
              meta = (DefaultToSelf = "Actor"))
    static USensorimotorIntegration* GetSensorimotorIntegration(AActor* Actor);

    /** Get the Autognosis System from an actor */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Components", 
              meta = (DefaultToSelf = "Actor"))
    static UAutognosisSystem* GetAutognosisSystem(AActor* Actor);

    // ========================================
    // COGNITIVE CYCLE SHORTCUTS
    // ========================================

    /** Get the current cognitive step (1-12) */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cognitive Cycle", 
              meta = (DefaultToSelf = "Actor"))
    static int32 GetCurrentCognitiveStep(AActor* Actor);

    /** Get the current cognitive mode (Expressive/Reflective) */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cognitive Cycle", 
              meta = (DefaultToSelf = "Actor"))
    static ECognitiveModeType GetCurrentCognitiveMode(AActor* Actor);

    /** Check if current step is pivotal */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cognitive Cycle", 
              meta = (DefaultToSelf = "Actor"))
    static bool IsCurrentStepPivotal(AActor* Actor);

    /** Get the relevance realization level */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cognitive Cycle", 
              meta = (DefaultToSelf = "Actor"))
    static float GetRelevanceRealizationLevel(AActor* Actor);

    /** Advance to the next cognitive step */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cognitive Cycle", 
              meta = (DefaultToSelf = "Actor"))
    static void AdvanceCognitiveStep(AActor* Actor);

    // ========================================
    // WISDOM SHORTCUTS
    // ========================================

    /** Get the overall wisdom level */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Wisdom", 
              meta = (DefaultToSelf = "Actor"))
    static float GetWisdomLevel(AActor* Actor);

    /** Get the meaning level */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Wisdom", 
              meta = (DefaultToSelf = "Actor"))
    static float GetMeaningLevel(AActor* Actor);

    /** Activate a way of knowing */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Wisdom", 
              meta = (DefaultToSelf = "Actor"))
    static void ActivateWayOfKnowing(AActor* Actor, EWayOfKnowing Way, float Intensity);

    /** Activate a practice of wisdom */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Wisdom", 
              meta = (DefaultToSelf = "Actor"))
    static void ActivatePracticeOfWisdom(AActor* Actor, EPracticeOfWisdom Practice, float Intensity);

    /** Get the current gnostic stage */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Wisdom", 
              meta = (DefaultToSelf = "Actor"))
    static EGnosticStage GetCurrentGnosticStage(AActor* Actor);

    // ========================================
    // EMBODIMENT SHORTCUTS
    // ========================================

    /** Get the overall 4E embodiment level */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Embodiment", 
              meta = (DefaultToSelf = "Actor"))
    static float GetOverallEmbodiment(AActor* Actor);

    /** Get embodiment level for a specific dimension */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Embodiment", 
              meta = (DefaultToSelf = "Actor"))
    static float GetEmbodimentDimension(AActor* Actor, EEmbodimentDimension Dimension);

    /** Register sensory input */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Embodiment", 
              meta = (DefaultToSelf = "Actor"))
    static void RegisterSensoryInput(AActor* Actor, ESensoryModality Modality, 
                                      float Intensity, float Salience, FVector Location);

    /** Issue a motor command */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Embodiment", 
              meta = (DefaultToSelf = "Actor"))
    static void IssueMotorCommand(AActor* Actor, EMotorActionType ActionType, 
                                   FVector TargetLocation, float Intensity);

    /** Detect affordances in the environment */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Embodiment", 
              meta = (DefaultToSelf = "Actor"))
    static TArray<FDetectedAffordance> DetectAffordances(AActor* Actor);

    /** Get the best available affordance */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Embodiment", 
              meta = (DefaultToSelf = "Actor"))
    static FDetectedAffordance GetBestAffordance(AActor* Actor);

    // ========================================
    // SELF-AWARENESS SHORTCUTS
    // ========================================

    /** Get the overall self-awareness score */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Self-Awareness", 
              meta = (DefaultToSelf = "Actor"))
    static float GetSelfAwarenessScore(AActor* Actor);

    /** Get the self-awareness assessment */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Self-Awareness", 
              meta = (DefaultToSelf = "Actor"))
    static FSelfAwarenessAssessment GetSelfAwarenessAssessment(AActor* Actor);

    /** Start the autognosis system */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Self-Awareness", 
              meta = (DefaultToSelf = "Actor"))
    static void StartAutognosis(AActor* Actor);

    /** Stop the autognosis system */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Self-Awareness", 
              meta = (DefaultToSelf = "Actor"))
    static void StopAutognosis(AActor* Actor);

    /** Get recent meta-cognitive insights */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Self-Awareness", 
              meta = (DefaultToSelf = "Actor"))
    static TArray<FMetaCognitiveInsight> GetRecentInsights(AActor* Actor, int32 Count);

    // ========================================
    // UTILITY FUNCTIONS
    // ========================================

    /** Check if an actor has Deep Tree Echo components */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Utility")
    static bool HasDeepTreeEchoComponents(AActor* Actor);

    /** Get a summary of the cognitive state */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Utility", 
              meta = (DefaultToSelf = "Actor"))
    static FString GetCognitiveStateSummary(AActor* Actor);

    /** Convert cognitive step to string */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Utility")
    static FString CognitiveStepToString(ECognitiveStepType Step);

    /** Convert way of knowing to string */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Utility")
    static FString WayOfKnowingToString(EWayOfKnowing Way);

    /** Convert practice of wisdom to string */
    UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Utility")
    static FString PracticeOfWisdomToString(EPracticeOfWisdom Practice);

    // ========================================
    // QUICK SETUP
    // ========================================

    /** Add all Deep Tree Echo components to an actor */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Setup", 
              meta = (DefaultToSelf = "Actor"))
    static void AddDeepTreeEchoComponents(AActor* Actor);

    /** Initialize all Deep Tree Echo systems on an actor */
    UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Setup", 
              meta = (DefaultToSelf = "Actor"))
    static void InitializeDeepTreeEcho(AActor* Actor);
};

// Enhanced4ECognition.h
// Deep Tree Echo - Enhanced 4E Embodied Cognition System
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enhanced4ECognition.generated.h"

// Forward declarations
struct FCognitiveState;
struct FEmotionalState;
struct FNeurochemicalState;

/**
 * Body Schema State
 * Represents the avatar's internal model of its body
 */
USTRUCT(BlueprintType)
struct FBodySchemaState
{
    GENERATED_BODY()

    // Proprioceptive state (6 DOF per major joint)
    UPROPERTY(BlueprintReadWrite, Category = "Body Schema")
    TMap<FName, FTransform> JointTransforms;

    // Body boundaries and extent
    UPROPERTY(BlueprintReadWrite, Category = "Body Schema")
    FVector BodyExtent;

    UPROPERTY(BlueprintReadWrite, Category = "Body Schema")
    FVector CenterOfMass;

    // Reach space
    UPROPERTY(BlueprintReadWrite, Category = "Body Schema")
    float MaxReachDistance;

    // Body schema confidence
    UPROPERTY(BlueprintReadWrite, Category = "Body Schema")
    float SchemaConfidence;

    FBodySchemaState()
        : BodyExtent(FVector(100.0f, 50.0f, 180.0f))
        , CenterOfMass(FVector::ZeroVector)
        , MaxReachDistance(100.0f)
        , SchemaConfidence(1.0f)
    {}
};

/**
 * Sensorimotor Contingency
 * Learned relationship between motor commands and sensory feedback
 */
USTRUCT(BlueprintType)
struct FSensorimotorContingency
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    FString MotorCommand;

    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    FString ExpectedSensoryOutcome;

    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    float Confidence;

    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    int32 ExperienceCount;

    FSensorimotorContingency()
        : Confidence(0.5f)
        , ExperienceCount(0)
    {}
};

/**
 * Somatic Marker
 * Association between body state and emotional state
 */
USTRUCT(BlueprintType)
struct FSomaticMarker
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Somatic")
    FString BodyStateSignature;

    UPROPERTY(BlueprintReadWrite, Category = "Somatic")
    FEmotionalState AssociatedEmotion;

    UPROPERTY(BlueprintReadWrite, Category = "Somatic")
    float Strength;

    UPROPERTY(BlueprintReadWrite, Category = "Somatic")
    int32 ActivationCount;

    FSomaticMarker()
        : Strength(0.5f)
        , ActivationCount(0)
    {}
};

/**
 * Interoceptive State
 * Internal body state awareness
 */
USTRUCT(BlueprintType)
struct FInteroceptiveState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float Hunger;

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float Thirst;

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float Fatigue;

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float Arousal;

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float Pain;

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float Temperature;

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float HeartRate;

    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float BreathingRate;

    FInteroceptiveState()
        : Hunger(0.0f)
        , Thirst(0.0f)
        , Fatigue(0.0f)
        , Arousal(0.5f)
        , Pain(0.0f)
        , Temperature(0.5f)
        , HeartRate(70.0f)
        , BreathingRate(15.0f)
    {}
};

/**
 * Affordance
 * Action possibility offered by environment
 */
USTRUCT(BlueprintType)
struct FAffordance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    AActor* TargetObject;

    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    FString AffordanceType; // "Graspable", "Sittable", "Climbable", etc.

    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    FVector InteractionLocation;

    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    float Salience;

    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    float Feasibility;

    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    bool bCurrentlyAvailable;

    FAffordance()
        : TargetObject(nullptr)
        , InteractionLocation(FVector::ZeroVector)
        , Salience(0.5f)
        , Feasibility(1.0f)
        , bCurrentlyAvailable(true)
    {}
};

/**
 * Environmental Niche
 * Adapted environmental context
 */
USTRUCT(BlueprintType)
struct FEnvironmentalNiche
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Niche")
    FString NicheType;

    UPROPERTY(BlueprintReadWrite, Category = "Niche")
    TArray<AActor*> RelevantObjects;

    UPROPERTY(BlueprintReadWrite, Category = "Niche")
    TArray<FAffordance> AvailableAffordances;

    UPROPERTY(BlueprintReadWrite, Category = "Niche")
    float Familiarity;

    UPROPERTY(BlueprintReadWrite, Category = "Niche")
    float CognitiveSupport;

    FEnvironmentalNiche()
        : Familiarity(0.0f)
        , CognitiveSupport(0.5f)
    {}
};

/**
 * Cognitive Tool
 * External artifact used to extend cognition
 */
USTRUCT(BlueprintType)
struct FCognitiveTool
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Tool")
    AActor* ToolObject;

    UPROPERTY(BlueprintReadWrite, Category = "Tool")
    FString ToolType;

    UPROPERTY(BlueprintReadWrite, Category = "Tool")
    FString CognitiveFunction; // "Memory", "Calculation", "Communication", etc.

    UPROPERTY(BlueprintReadWrite, Category = "Tool")
    float Proficiency;

    UPROPERTY(BlueprintReadWrite, Category = "Tool")
    bool bCurrentlyUsing;

    FCognitiveTool()
        : ToolObject(nullptr)
        , Proficiency(0.0f)
        , bCurrentlyUsing(false)
    {}
};

/**
 * Social Agent
 * Other agent in social network
 */
USTRUCT(BlueprintType)
struct FSocialAgent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Social")
    AActor* AgentActor;

    UPROPERTY(BlueprintReadWrite, Category = "Social")
    FString AgentName;

    UPROPERTY(BlueprintReadWrite, Category = "Social")
    float SocialBondStrength;

    UPROPERTY(BlueprintReadWrite, Category = "Social")
    float Trust;

    UPROPERTY(BlueprintReadWrite, Category = "Social")
    TArray<FString> SharedKnowledge;

    FSocialAgent()
        : AgentActor(nullptr)
        , SocialBondStrength(0.0f)
        , Trust(0.5f)
    {}
};

/**
 * UEnhanced4ECognitionComponent
 * 
 * Enhanced implementation of 4E (Embodied, Embedded, Enacted, Extended) cognition.
 * Provides deep integration of sensorimotor contingencies, environmental coupling,
 * active world-making, and cognitive extension.
 * 
 * Key Features:
 * 
 * **Embodied:**
 * - Body schema awareness and adaptation
 * - Sensorimotor contingency learning
 * - Somatic marker integration
 * - Interoceptive state tracking
 * - Motor prediction and validation
 * 
 * **Embedded:**
 * - Affordance detection and utilization
 * - Environmental scaffolding recognition
 * - Niche construction and adaptation
 * - Context-sensitive processing
 * - Salience landscape navigation
 * 
 * **Enacted:**
 * - Active sampling and exploration
 * - Sensorimotor prediction validation
 * - World model enactment
 * - Participatory meaning-making
 * - Transformative experience integration
 * 
 * **Extended:**
 * - Tool use and cognitive extension
 * - External memory utilization
 * - Social cognition and collective intelligence
 * - Cultural knowledge inheritance
 * - Distributed cognitive processing
 * 
 * Integration Points:
 * - ControlRigIntegration: Embodied animation
 * - AffordanceDetectionSystem: Environmental interaction
 * - SocialCognitionFramework: Social intelligence
 * - CognitiveExtensionSystem: Tool use and extension
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API UEnhanced4ECognitionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnhanced4ECognitionComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // EMBODIED: Body Schema & Sensorimotor
    // ========================================

    /**
     * Update body schema from current skeleton state
     * @param BoneTransforms - Current bone transforms
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void UpdateBodySchema(const TMap<FName, FTransform>& BoneTransforms);

    /**
     * Get current body schema
     * @return Current body schema state
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "4E|Embodied")
    FBodySchemaState GetBodySchema() const { return CurrentBodySchema; }

    /**
     * Learn sensorimotor contingency from experience
     * @param MotorCommand - Motor command executed
     * @param SensoryFeedback - Resulting sensory feedback
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void LearnSensorimotorContingency(const FString& MotorCommand, const FString& SensoryFeedback);

    /**
     * Predict sensory outcome from motor command
     * @param MotorCommand - Planned motor command
     * @return Predicted sensory outcome
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    FString PredictSensoryOutcome(const FString& MotorCommand);

    /**
     * Calculate prediction error
     * @param Predicted - Predicted sensory outcome
     * @param Actual - Actual sensory outcome
     * @return Prediction error magnitude (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    float CalculatePredictionError(const FString& Predicted, const FString& Actual);

    /**
     * Associate somatic marker with emotion
     * @param BodyState - Current body state signature
     * @param Emotion - Associated emotional state
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void AssociateSomaticMarker(const FString& BodyState, const FEmotionalState& Emotion);

    /**
     * Recall emotion from body state
     * @param BodyState - Current body state signature
     * @param OutEmotion - Output recalled emotion
     * @return True if emotion recalled
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    bool RecallEmotionFromBodyState(const FString& BodyState, FEmotionalState& OutEmotion);

    /**
     * Get interoceptive state
     * @return Current interoceptive state
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "4E|Embodied")
    FInteroceptiveState GetInteroceptiveState() const { return CurrentInteroceptiveState; }

    /**
     * Update interoceptive state
     * @param DeltaTime - Time delta
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void UpdateInteroceptiveState(float DeltaTime);

    // ========================================
    // EMBEDDED: Affordances & Environment
    // ========================================

    /**
     * Detect affordances in environment
     * @param NearbyObjects - Objects to check for affordances
     * @return Array of detected affordances
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    TArray<FAffordance> DetectAffordances(const TArray<AActor*>& NearbyObjects);

    /**
     * Filter affordances by cognitive state
     * @param Affordances - All detected affordances
     * @param State - Current cognitive state
     * @return Filtered affordances based on relevance
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    TArray<FAffordance> FilterAffordancesByCognitiveState(const TArray<FAffordance>& Affordances, const FCognitiveState& State);

    /**
     * Get current environmental niche
     * @return Current niche
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "4E|Embedded")
    FEnvironmentalNiche GetCurrentNiche() const { return CurrentNiche; }

    /**
     * Modify environment for cognitive support (niche construction)
     * @param ModificationType - Type of modification
     * @param TargetObject - Object to modify
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    bool ModifyEnvironmentForCognitiveSupport(const FString& ModificationType, AActor* TargetObject);

    /**
     * Detect environmental scaffolding
     * @return Array of scaffolding resources
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    TArray<AActor*> DetectEnvironmentalScaffolding();

    /**
     * Utilize scaffolding resource
     * @param Scaffolding - Scaffolding resource to use
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    bool UtilizeScaffolding(AActor* Scaffolding);

    // ========================================
    // ENACTED: Active Sampling & World-Making
    // ========================================

    /**
     * Initiate exploratory sampling
     * @param UncertaintyLevel - Current uncertainty level
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void InitiateExploratorySampling(float UncertaintyLevel);

    /**
     * Generate sampling action
     * @return Motor command for sampling
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    FString GenerateSamplingAction();

    /**
     * Validate sensorimotor prediction
     * @param Prediction - Predicted sensory outcome
     * @param Actual - Actual sensory outcome
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void ValidateSensorimotorPrediction(const FString& Prediction, const FString& Actual);

    /**
     * Update world model from validation
     * @param PredictionError - Magnitude of prediction error
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void UpdateWorldModelFromValidation(float PredictionError);

    /**
     * Enact world model through sensorimotor loop
     * @return Enacted world model description
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    FString EnactWorldModel();

    /**
     * Co-create meaning with environment
     * @param InteractionContext - Current interaction context
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void CoCreateMeaningWithEnvironment(const FString& InteractionContext);

    /**
     * Engage participatory knowing
     * @param Phenomenon - Phenomenon to engage with
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void EngageParticipatoryKnowing(const FString& Phenomenon);

    // ========================================
    // EXTENDED: Tools & Social Cognition
    // ========================================

    /**
     * Register cognitive tool
     * @param Tool - Tool to register
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void RegisterCognitiveTool(const FCognitiveTool& Tool);

    /**
     * Extend cognition through tool
     * @param Tool - Tool to use
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    bool ExtendCognitionThroughTool(const FCognitiveTool& Tool);

    /**
     * Get available cognitive tools
     * @return Array of available tools
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "4E|Extended")
    TArray<FCognitiveTool> GetAvailableTools() const { return RegisteredTools; }

    /**
     * Register social agent
     * @param Agent - Agent to register
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void RegisterSocialAgent(const FSocialAgent& Agent);

    /**
     * Engage collective intelligence
     * @param Agents - Agents to engage with
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void EngageCollectiveIntelligence(const TArray<FSocialAgent>& Agents);

    /**
     * Access shared knowledge from social network
     * @return Array of shared knowledge items
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    TArray<FString> AccessSharedKnowledge();

    /**
     * Inherit cultural knowledge
     * @param CulturalContext - Cultural context to inherit from
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void InheritCulturalKnowledge(const FString& CulturalContext);

    /**
     * Contribute to cultural knowledge
     * @param Contribution - Knowledge to contribute
     */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void ContributeToCulturalKnowledge(const FString& Contribution);

protected:
    // ========================================
    // EMBODIED State
    // ========================================

    UPROPERTY()
    FBodySchemaState CurrentBodySchema;

    UPROPERTY()
    TArray<FSensorimotorContingency> LearnedContingencies;

    UPROPERTY()
    TArray<FSomaticMarker> SomaticMarkerMemory;

    UPROPERTY()
    FInteroceptiveState CurrentInteroceptiveState;

    // ========================================
    // EMBEDDED State
    // ========================================

    UPROPERTY()
    TArray<FAffordance> DetectedAffordances;

    UPROPERTY()
    FEnvironmentalNiche CurrentNiche;

    UPROPERTY()
    TArray<AActor*> EnvironmentalScaffolding;

    // ========================================
    // ENACTED State
    // ========================================

    UPROPERTY()
    TArray<FString> PendingPredictions;

    UPROPERTY()
    float ExplorationDrive;

    UPROPERTY()
    FString EnactedWorldModelDescription;

    // ========================================
    // EXTENDED State
    // ========================================

    UPROPERTY()
    TArray<FCognitiveTool> RegisteredTools;

    UPROPERTY()
    TArray<FSocialAgent> SocialNetwork;

    UPROPERTY()
    TArray<FString> CulturalKnowledgeBase;

    UPROPERTY()
    TArray<FString> SharedKnowledgePool;

    // Helper functions
    void UpdateBodySchemaConfidence(float DeltaTime);
    float CalculateAffordanceSalience(const FAffordance& Affordance, const FCognitiveState& State);
    bool CheckAffordanceFeasibility(const FAffordance& Affordance);
    void UpdateEnvironmentalNiche(float DeltaTime);
    void UpdateExplorationDrive(float DeltaTime);
    FString GenerateBodyStateSignature();
    void LearnFromPredictionError(float Error);
};

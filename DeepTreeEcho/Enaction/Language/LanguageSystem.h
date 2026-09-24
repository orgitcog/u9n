// LanguageSystem.h
// Natural Language Processing and Generation for Deep Tree Echo
// Implements semantic parsing, dialogue management, and narrative generation

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LanguageSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UMemorySystems;
class UEmotionalSystem;

/**
 * Speech act type
 */
UENUM(BlueprintType)
enum class ESpeechAct : uint8
{
    Assertion       UMETA(DisplayName = "Assertion"),
    Question        UMETA(DisplayName = "Question"),
    Command         UMETA(DisplayName = "Command"),
    Request         UMETA(DisplayName = "Request"),
    Promise         UMETA(DisplayName = "Promise"),
    Greeting        UMETA(DisplayName = "Greeting"),
    Farewell        UMETA(DisplayName = "Farewell"),
    Acknowledgment  UMETA(DisplayName = "Acknowledgment"),
    Expression      UMETA(DisplayName = "Expression"),
    Declaration     UMETA(DisplayName = "Declaration")
};

/**
 * Dialogue state
 */
UENUM(BlueprintType)
enum class EDialogueState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Listening       UMETA(DisplayName = "Listening"),
    Processing      UMETA(DisplayName = "Processing"),
    Responding      UMETA(DisplayName = "Responding"),
    Waiting         UMETA(DisplayName = "Waiting"),
    Clarifying      UMETA(DisplayName = "Clarifying")
};

/**
 * Semantic role
 */
UENUM(BlueprintType)
enum class ESemanticRole : uint8
{
    Agent           UMETA(DisplayName = "Agent"),
    Patient         UMETA(DisplayName = "Patient"),
    Theme           UMETA(DisplayName = "Theme"),
    Experiencer     UMETA(DisplayName = "Experiencer"),
    Beneficiary     UMETA(DisplayName = "Beneficiary"),
    Instrument      UMETA(DisplayName = "Instrument"),
    Location        UMETA(DisplayName = "Location"),
    Source          UMETA(DisplayName = "Source"),
    Goal            UMETA(DisplayName = "Goal"),
    Time            UMETA(DisplayName = "Time")
};

/**
 * Parsed utterance
 */
USTRUCT(BlueprintType)
struct FParsedUtterance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString UtteranceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RawText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeechAct SpeechAct = ESpeechAct::Assertion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MainVerb;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<ESemanticRole, FString> SemanticRoles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Entities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Keywords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Sentiment = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Dialogue turn
 */
USTRUCT(BlueprintType)
struct FDialogueTurn
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TurnID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Speaker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Utterance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FParsedUtterance ParsedContent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsAgentTurn = false;
};

/**
 * Dialogue context
 */
USTRUCT(BlueprintType)
struct FDialogueContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConversationID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDialogueTurn> TurnHistory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CurrentTopic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> MentionedEntities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> SharedKnowledge;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ConversationStartTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDialogueState State = EDialogueState::Idle;
};

/**
 * Response template
 */
USTRUCT(BlueprintType)
struct FResponseTemplate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TemplateID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeechAct ResponseType = ESpeechAct::Assertion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TemplateText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RequiredSlots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalTone = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Formality = 0.5f;
};

/**
 * Generated response
 */
USTRUCT(BlueprintType)
struct FGeneratedResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ResponseID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeechAct SpeechAct = ESpeechAct::Assertion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalTone = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> AlternativeResponses;
};

/**
 * Narrative element
 */
USTRUCT(BlueprintType)
struct FNarrativeElement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ElementID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NarrativeType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Characters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Setting;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArc = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUtteranceParsed, const FParsedUtterance&, Parsed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResponseGenerated, const FGeneratedResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueStateChanged, EDialogueState, OldState, EDialogueState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTopicChanged, const FString&, NewTopic);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeGenerated, const FNarrativeElement&, Narrative);

/**
 * Language System Component
 * Implements natural language processing and generation
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API ULanguageSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    ULanguageSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxTurnHistory = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float DefaultFormality = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ResponseTimeout = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    FString AgentName = TEXT("Echo");

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUtteranceParsed OnUtteranceParsed;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnResponseGenerated OnResponseGenerated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDialogueStateChanged OnDialogueStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTopicChanged OnTopicChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnNarrativeGenerated OnNarrativeGenerated;

    // ========================================
    // PARSING
    // ========================================

    /** Parse an utterance */
    UFUNCTION(BlueprintCallable, Category = "Parsing")
    FParsedUtterance ParseUtterance(const FString& Text);

    /** Identify speech act */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    ESpeechAct IdentifySpeechAct(const FString& Text) const;

    /** Extract entities from text */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    TArray<FString> ExtractEntities(const FString& Text) const;

    /** Extract keywords from text */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    TArray<FString> ExtractKeywords(const FString& Text) const;

    /** Analyze sentiment */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    float AnalyzeSentiment(const FString& Text) const;

    // ========================================
    // DIALOGUE MANAGEMENT
    // ========================================

    /** Start a new conversation */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void StartConversation(const FString& InitialTopic);

    /** Process incoming utterance */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    FGeneratedResponse ProcessUtterance(const FString& Speaker, const FString& Text);

    /** Get dialogue context */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    FDialogueContext GetDialogueContext() const;

    /** Get dialogue state */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    EDialogueState GetDialogueState() const;

    /** Set dialogue state */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SetDialogueState(EDialogueState NewState);

    /** Get current topic */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    FString GetCurrentTopic() const;

    /** Set current topic */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SetCurrentTopic(const FString& Topic);

    /** End conversation */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void EndConversation();

    /** Get turn history */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    TArray<FDialogueTurn> GetTurnHistory() const;

    // ========================================
    // RESPONSE GENERATION
    // ========================================

    /** Generate response to parsed utterance */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateResponse(const FParsedUtterance& Input);

    /** Generate response with emotional tone */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone);

    /** Generate clarification question */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateClarification(const FString& UnclearAspect);

    /** Generate acknowledgment */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateAcknowledgment();

    /** Generate greeting */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateGreeting(const FString& RecipientName);

    /** Generate farewell */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateFarewell();

    // ========================================
    // NARRATIVE GENERATION
    // ========================================

    /** Generate narrative description */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FNarrativeElement GenerateNarrative(const FString& Subject, const TArray<FString>& Events);

    /** Generate self-description */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FNarrativeElement GenerateSelfNarrative();

    /** Generate experience summary */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FNarrativeElement GenerateExperienceSummary(float TimeSpan);

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UMemorySystems* MemoryComponent;

    UPROPERTY()
    UEmotionalSystem* EmotionalComponent;

    // Internal state
    FDialogueContext DialogueContext;
    TArray<FResponseTemplate> ResponseTemplates;

    int32 UtteranceIDCounter = 0;
    int32 TurnIDCounter = 0;
    int32 ResponseIDCounter = 0;
    int32 NarrativeIDCounter = 0;
    int32 ConversationIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeLanguageSystem();
    void InitializeResponseTemplates();

    void UpdateDialogueState(float DeltaTime);
    void AddTurnToHistory(const FDialogueTurn& Turn);

    FString SelectResponseTemplate(ESpeechAct InputAct, float EmotionalTone) const;
    FString FillTemplateSlots(const FString& Template, const FParsedUtterance& Input) const;
    FString ApplyEmotionalModulation(const FString& Text, float EmotionalTone) const;

    bool IsQuestion(const FString& Text) const;
    bool IsCommand(const FString& Text) const;
    bool IsGreeting(const FString& Text) const;

    FString GenerateUtteranceID();
    FString GenerateTurnID();
    FString GenerateResponseID();
    FString GenerateNarrativeID();
    FString GenerateConversationID();
};

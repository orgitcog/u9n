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
class UDTELanguageModelBridge;
struct FDTEChatMessage;

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
 * Generated response.
 *
 * Text is always produced by the language model. A struct returned
 * synchronously from the Generate* functions is a pending placeholder
 * (bIsPending = true, empty Text); the completed response with the same
 * ResponseID arrives through ULanguageSystem::OnResponseGenerated.
 */
USTRUCT(BlueprintType)
struct FGeneratedResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ResponseID;

    /** Model-generated utterance. Empty while pending or when the model did not answer. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Text;

    /** True when the model request is still in flight and Text has not been produced yet. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPending = false;

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
 * Narrative element.
 *
 * Content is always produced by the language model. A struct returned
 * synchronously from the Generate* functions is a pending placeholder
 * (bIsPending = true, empty Content); the completed narrative with the same
 * ElementID arrives through ULanguageSystem::OnNarrativeGenerated.
 */
USTRUCT(BlueprintType)
struct FNarrativeElement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ElementID;

    /** Model-generated narrative text. Empty while pending or when the model did not answer. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    /** True when the model request is still in flight and Content has not been produced yet. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPending = false;

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
 * Implements natural language processing and generation.
 *
 * Parsing (speech-act detection, entity/keyword extraction, sentiment) is
 * local analysis. Every utterance the agent produces is requested from the
 * language model through UDTELanguageModelBridge: the kind of utterance
 * wanted (greeting, answer, acknowledgment, narrative, ...) becomes the
 * instruction and the dialogue context (recent turns, topic, recipient,
 * emotional tone, formality) becomes the prompt context. Generated text is
 * delivered asynchronously through OnResponseGenerated / OnNarrativeGenerated.
 * When the model does not answer, nothing is said and an operator-facing
 * warning is logged.
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

    /** How many recent dialogue turns are sent to the model as conversation context */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "0"))
    int32 RecentTurnsInPrompt = 8;

    /** Number of model requests currently in flight for this component */
    UPROPERTY(BlueprintReadOnly, Category = "Configuration")
    int32 PendingRequestCount = 0;

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

    /**
     * Process incoming utterance: parse it, record the speaker's turn and ask
     * the model for the agent's reply. The returned struct is a pending
     * placeholder (bIsPending = true, empty Text). When the model answers, the
     * agent's turn is added to the history and OnResponseGenerated fires with
     * the same ResponseID. When the model does not answer, no agent turn is
     * recorded and nothing is broadcast.
     */
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
    //
    // Request* functions ask the model and return the ResponseID that the
    // eventual OnResponseGenerated broadcast will carry. The Generate*
    // functions are kept for API compatibility: they dispatch the same request
    // and return a pending placeholder (bIsPending = true, empty Text). No
    // path fills Text without the model.

    /** Ask the model to respond to a parsed utterance using the current emotional state. Returns the ResponseID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestResponse(const FParsedUtterance& Input);

    /** Ask the model to respond to a parsed utterance with an explicit emotional tone (-1..1). Returns the ResponseID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone);

    /** Ask the model for a clarification question about an unclear aspect. Returns the ResponseID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestClarification(const FString& UnclearAspect);

    /** Ask the model for a brief acknowledgment of the last thing said. Returns the ResponseID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestAcknowledgment();

    /** Ask the model to greet the recipient (empty name: whoever last spoke). Returns the ResponseID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestGreeting(const FString& RecipientName);

    /** Ask the model to close the conversation. Returns the ResponseID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestFarewell();

    /** Compatibility wrapper for RequestResponse. Returns a pending placeholder; text arrives via OnResponseGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateResponse(const FParsedUtterance& Input);

    /** Compatibility wrapper for RequestEmotionalResponse. Returns a pending placeholder; text arrives via OnResponseGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone);

    /** Compatibility wrapper for RequestClarification. Returns a pending placeholder; text arrives via OnResponseGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateClarification(const FString& UnclearAspect);

    /** Compatibility wrapper for RequestAcknowledgment. Returns a pending placeholder; text arrives via OnResponseGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateAcknowledgment();

    /** Compatibility wrapper for RequestGreeting. Returns a pending placeholder; text arrives via OnResponseGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateGreeting(const FString& RecipientName);

    /** Compatibility wrapper for RequestFarewell. Returns a pending placeholder; text arrives via OnResponseGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FGeneratedResponse GenerateFarewell();

    // ========================================
    // NARRATIVE GENERATION
    // ========================================
    //
    // Request* functions ask the model and return the ElementID that the
    // eventual OnNarrativeGenerated broadcast will carry. The Generate*
    // functions dispatch the same request and return a pending placeholder.

    /** Ask the model to narrate what Subject experienced across Events. Returns the ElementID. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString RequestNarrative(const FString& Subject, const TArray<FString>& Events);

    /** Ask the model for the agent's first-person self-description. Returns the ElementID. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString RequestSelfNarrative();

    /** Ask the model for a first-person summary of recent experience. Returns the ElementID. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString RequestExperienceSummary(float TimeSpan);

    /** Compatibility wrapper for RequestNarrative. Returns a pending placeholder; text arrives via OnNarrativeGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FNarrativeElement GenerateNarrative(const FString& Subject, const TArray<FString>& Events);

    /** Compatibility wrapper for RequestSelfNarrative. Returns a pending placeholder; text arrives via OnNarrativeGenerated. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FNarrativeElement GenerateSelfNarrative();

    /** Compatibility wrapper for RequestExperienceSummary. Returns a pending placeholder; text arrives via OnNarrativeGenerated. */
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

    /** The model bridge every utterance is requested from */
    UPROPERTY()
    UDTELanguageModelBridge* ModelBridge;

    // Internal state
    FDialogueContext DialogueContext;

    int32 UtteranceIDCounter = 0;
    int32 TurnIDCounter = 0;
    int32 ResponseIDCounter = 0;
    int32 NarrativeIDCounter = 0;
    int32 ConversationIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeLanguageSystem();

    /** The cached bridge, resolving it on the owner when not yet cached */
    UDTELanguageModelBridge* GetModelBridge();

    void UpdateDialogueState(float DeltaTime);
    void AddTurnToHistory(const FDialogueTurn& Turn);

    /** Current emotional valence from the emotional component (0 when absent) */
    float GetCurrentEmotionalTone() const;

    /** Name of the most recent non-agent speaker, or empty */
    FString GetLastSpeaker() const;

    /**
     * Compose the instruction for a reply to Input and dispatch the model
     * request. bRecordAgentTurn adds the reply to the turn history on success.
     */
    FString DispatchReplyRequest(const FParsedUtterance& Input, float EmotionalTone, bool bRecordAgentTurn);

    /**
     * Ask the model for one utterance. Instruction says what kind of utterance
     * is wanted; Situation adds facts beyond the shared dialogue context.
     * On success an FGeneratedResponse is built and OnResponseGenerated fires;
     * on failure a warning is logged and nothing is broadcast.
     */
    FString DispatchSpeechRequest(ESpeechAct ResponseAct, const FString& Instruction, const FString& Situation,
                                  float EmotionalTone, float Confidence, bool bRecordAgentTurn,
                                  EDialogueState StateOnSuccess);

    /**
     * Ask the model for a narrative passage. On success an FNarrativeElement is
     * built and OnNarrativeGenerated fires; on failure nothing is broadcast.
     */
    FString DispatchNarrativeRequest(const FString& NarrativeType, const FString& Instruction, const FString& Situation,
                                     const TArray<FString>& Characters, const FString& Setting, float EmotionalArc);

    /** Shared situational prompt: identity, topic, recipient, emotional tone, formality */
    FString BuildSituationPrompt(float EmotionalTone, const FString& Recipient) const;

    /** Recent dialogue turns as chat messages (user/assistant) */
    void AppendRecentTurns(TArray<FDTEChatMessage>& Messages) const;

    /** Model-facing descriptions of scalar state (prompt context, not character text) */
    FString DescribeEmotionalTone(float EmotionalTone) const;
    FString DescribeFormality(float Formality) const;
    FString DescribeSpeechAct(ESpeechAct Act) const;

    bool IsQuestion(const FString& Text) const;
    bool IsCommand(const FString& Text) const;
    bool IsGreeting(const FString& Text) const;

    FString GenerateUtteranceID();
    FString GenerateTurnID();
    FString GenerateResponseID();
    FString GenerateNarrativeID();
    FString GenerateConversationID();
};

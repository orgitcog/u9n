#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DTELanguageModelBridge.h"
#include "AvatarCommunicationManager.generated.h"

// Forward declarations
class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;
class UDiaryInsightBlogLoop;
class UAvatar9PServer;
class UAvatarAtomSpaceClient;

/**
 * Message Source Types
 */
UENUM(BlueprintType)
enum class EMessageSource : uint8
{
    DeltaChat UMETA(DisplayName = "DeltaChat"),
    Dovecot UMETA(DisplayName = "Dovecot/Email"),
    WebSocket UMETA(DisplayName = "WebSocket"),
    P9 UMETA(DisplayName = "9P Protocol"),
    Internal UMETA(DisplayName = "Internal System"),
    Agent UMETA(DisplayName = "AGI-OS Agent")
};

/**
 * Message Priority Levels
 */
UENUM(BlueprintType)
enum class EMessagePriority : uint8
{
    Low UMETA(DisplayName = "Low"),
    Normal UMETA(DisplayName = "Normal"),
    High UMETA(DisplayName = "High"),
    Urgent UMETA(DisplayName = "Urgent"),
    System UMETA(DisplayName = "System Critical")
};

/**
 * Conversation State
 */
UENUM(BlueprintType)
enum class EConversationState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Active UMETA(DisplayName = "Active"),
    Waiting UMETA(DisplayName = "Waiting for Response"),
    Processing UMETA(DisplayName = "Processing"),
    Ended UMETA(DisplayName = "Ended")
};

/**
 * DeltaChat Message structure
 */
USTRUCT(BlueprintType)
struct FDeltaChatMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MessageId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ChatId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SenderId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SenderName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsGroupChat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Attachments;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ReplyToMessageId;

    FDeltaChatMessage()
        : Timestamp(FDateTime::Now())
        , bIsGroupChat(false)
    {}
};

/**
 * Email Message structure (Dovecot)
 */
USTRUCT(BlueprintType)
struct FDovecotEmail
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EmailId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString From;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> To;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Cc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Subject;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BodyText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BodyHtml;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime ReceivedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Attachments;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InReplyTo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ThreadId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMessagePriority Priority;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsRead;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsStarred;

    FDovecotEmail()
        : ReceivedAt(FDateTime::Now())
        , Priority(EMessagePriority::Normal)
        , bIsRead(false)
        , bIsStarred(false)
    {}
};

/**
 * Generated Response structure
 *
 * Content is always written by the language model. A response whose text has
 * not arrived yet has empty Content and bIsPending = true; nothing is ever
 * sent, logged or broadcast for it until the model answers.
 */
USTRUCT(BlueprintType)
struct FGeneratedResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ResponseId;

    /** Reply text as written by the model. Empty while bIsPending. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Tone; // friendly, professional, playful, etc.

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArousal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SuggestedActions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresConfirmation;

    /** True while the model request that will fill Content is still in flight. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPending;

    FGeneratedResponse()
        : Confidence(0.8f)
        , EmotionalValence(0.5f)
        , EmotionalArousal(0.5f)
        , bRequiresConfirmation(false)
        , bIsPending(false)
    {}
};

/**
 * Conversation Context
 */
USTRUCT(BlueprintType)
struct FConversationContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConversationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMessageSource Source;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParticipantId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParticipantName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EConversationState State;

    /** Messages received from the participant, oldest first. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> MessageHistory;

    /** Replies actually sent to the participant, oldest first. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ReplyHistory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime StartedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime LastActivityAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Topic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SentimentScore;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MessageCount;

    FConversationContext()
        : Source(EMessageSource::Internal)
        , State(EConversationState::Idle)
        , StartedAt(FDateTime::Now())
        , LastActivityAt(FDateTime::Now())
        , SentimentScore(0.5f)
        , MessageCount(0)
    {}
};

/**
 * Delegates for communication events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeltaChatMessageReceived, const FDeltaChatMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmailReceived, const FDovecotEmail&, Email);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResponseGenerated, const FString&, MessageId, const FGeneratedResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConversationStateChanged, const FString&, ConversationId, EConversationState, NewState);

/**
 * Completion callback for the Request... functions. bSuccess is false when the
 * model did not answer; Response.Content is then empty and must not be sent.
 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnCommunicationResponseReady, bool, bSuccess, const FGeneratedResponse&, Response);

/** C++ counterpart of FOnCommunicationResponseReady. */
using FCommunicationResponseCallback = TFunction<void(bool bSuccess, const FGeneratedResponse& Response)>;

/**
 * Avatar Communication Manager Component
 * Handles messaging integration with DeltaChat and Dovecot for AGI-OS
 *
 * Message Flow:
 * DeltaChat/Dovecot -> 9P Interface -> Avatar Communication Manager
 *     |
 *     v
 * Personality System (determine response style)
 *     |
 *     v
 * Narrative System (add to diary, generate insight)
 *     |
 *     v
 * Language Model Bridge (writes the reply; personality, tone, history and
 *                        detected intent are supplied as prompt context)
 *     |
 *     v
 * DeltaChat/Dovecot <- 9P Interface <- Avatar Communication Manager
 *
 * Every reply is written by the model. When the model does not answer, the
 * incoming message is left unanswered and recorded in UnansweredMessageIds so
 * an operator can see it; no substitute text is ever sent.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAvatarCommunicationManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarCommunicationManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Connection Management =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    void ConnectDeltaChat(const FString& AccountEmail, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    void ConnectDovecot(const FString& ImapHost, int32 ImapPort, const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    void DisconnectAll();

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    bool IsDeltaChatConnected() const { return bDeltaChatConnected; }

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    bool IsDovecotConnected() const { return bDovecotConnected; }

    // ===== Message Reception =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Messages")
    void OnDeltaChatMessageReceived(const FDeltaChatMessage& Message);

    UFUNCTION(BlueprintCallable, Category = "Communication|Messages")
    void OnEmailReceived(const FDovecotEmail& Email);

    UFUNCTION(BlueprintCallable, Category = "Communication|Messages")
    void ProcessPendingMessages();

    // ===== Response Generation =====
    //
    // All reply text comes from the language model and arrives asynchronously.

    /**
     * Ask the model to reply to Input within Context. OnDone fires once with the
     * finished response, or with bSuccess = false and empty Content when the
     * model did not answer.
     */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    void RequestResponse(const FString& Input, const FConversationContext& Context, FOnCommunicationResponseReady OnDone);

    /** Ask the model to reply to a DeltaChat message. Broadcasts OnResponseGenerated(MessageId) on success, then OnDone. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    void RequestDeltaChatResponse(const FDeltaChatMessage& Message, FOnCommunicationResponseReady OnDone);

    /** Ask the model to reply to an email. Broadcasts OnResponseGenerated(EmailId) on success, then OnDone. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    void RequestEmailResponse(const FDovecotEmail& Email, FOnCommunicationResponseReady OnDone);

    /**
     * Compatibility wrapper around RequestResponse. Dispatches the model request
     * and returns an empty response with bIsPending = true; the written reply
     * arrives through OnResponseGenerated, keyed by the returned ResponseId.
     */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response", meta = (DeprecatedFunction, DeprecationMessage = "Use RequestResponse; replies arrive asynchronously."))
    FGeneratedResponse GenerateResponse(const FString& Input, const FConversationContext& Context);

    /** Compatibility wrapper around RequestDeltaChatResponse; see GenerateResponse. Keyed by Message.MessageId. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response", meta = (DeprecatedFunction, DeprecationMessage = "Use RequestDeltaChatResponse; replies arrive asynchronously."))
    FGeneratedResponse GenerateDeltaChatResponse(const FDeltaChatMessage& Message);

    /** Compatibility wrapper around RequestEmailResponse; see GenerateResponse. Keyed by Email.EmailId. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response", meta = (DeprecatedFunction, DeprecationMessage = "Use RequestEmailResponse; replies arrive asynchronously."))
    FGeneratedResponse GenerateEmailResponse(const FDovecotEmail& Email);

    /** Messages the model did not answer (dropped from the queue, nothing sent). Operator-facing. */
    UFUNCTION(BlueprintPure, Category = "Communication|Response")
    TArray<FString> GetUnansweredMessageIds() const { return UnansweredMessageIds; }

    /** Number of model requests currently in flight for this component. */
    UFUNCTION(BlueprintPure, Category = "Communication|Response")
    int32 GetInFlightRequestCount() const { return InFlightRequests; }

    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    void SendDeltaChatResponse(const FString& ChatId, const FGeneratedResponse& Response);

    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    void SendEmailResponse(const FString& EmailId, const FGeneratedResponse& Response);

    // ===== Conversation Management =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    FConversationContext GetOrCreateConversation(const FString& ParticipantId, EMessageSource Source);

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    void UpdateConversationState(const FString& ConversationId, EConversationState NewState);

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    FConversationContext GetConversation(const FString& ConversationId);

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    TArray<FConversationContext> GetActiveConversations();

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    void EndConversation(const FString& ConversationId);

    // ===== Personality-Driven Response Style =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Style")
    FString DetermineResponseTone(const FConversationContext& Context);

    UFUNCTION(BlueprintCallable, Category = "Communication|Style")
    float CalculateResponseDelay(const FConversationContext& Context);

    UFUNCTION(BlueprintCallable, Category = "Communication|Style")
    bool ShouldAddEmoji(const FConversationContext& Context);

    UFUNCTION(BlueprintCallable, Category = "Communication|Style")
    int32 CalculateResponseLength(const FConversationContext& Context);

    // ===== Narrative Integration =====

    /** Records the exchange (incoming text and the reply the model wrote). Only called once a reply exists. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Narrative")
    void LogToNarrativeSystem(const FDeltaChatMessage& Message, const FGeneratedResponse& Response);

    /** Records the exchange (incoming email and the reply the model wrote). Only called once a reply exists. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Narrative")
    void LogEmailToNarrativeSystem(const FDovecotEmail& Email, const FGeneratedResponse& Response);

    /** Asks the model to reflect on the conversation; the insight is stored only when it answers. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Narrative")
    void ExtractInsightsFromConversation(const FConversationContext& Context);

    // ===== Sentiment Analysis =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Analysis")
    float AnalyzeSentiment(const FString& Text);

    UFUNCTION(BlueprintCallable, Category = "Communication|Analysis")
    TArray<FString> ExtractKeywords(const FString& Text);

    UFUNCTION(BlueprintCallable, Category = "Communication|Analysis")
    FString DetectIntent(const FString& Text);

    // ===== 9P Integration =====

    UFUNCTION(BlueprintCallable, Category = "Communication|9P")
    void Register9PNamespace();

    UFUNCTION(BlueprintCallable, Category = "Communication|9P")
    FString Handle9PRead(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "Communication|9P")
    bool Handle9PWrite(const FString& Path, const FString& Data);

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnDeltaChatMessageReceived OnDeltaChatMessage;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnEmailReceived OnEmail;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnResponseGenerated OnResponseGenerated;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnConversationStateChanged OnConversationStateChanged;

protected:
    // ===== Internal Methods =====

    void ProcessDeltaChatQueue();
    void ProcessEmailQueue();

    /** C++ core of RequestResponse. ResponseId is assigned to the finished response. */
    void RequestResponseInternal(const FString& Input, const FConversationContext& Context,
                                 const FString& ResponseId, FCommunicationResponseCallback OnDone);
    void RequestDeltaChatResponseInternal(const FDeltaChatMessage& Message, const FString& ResponseId,
                                          FCommunicationResponseCallback OnDone);
    void RequestEmailResponseInternal(const FDovecotEmail& Email, const FString& ResponseId,
                                      FCommunicationResponseCallback OnDone);

    /** Builds the system/style message for a reply request from context, tone, intent and personality. */
    FString BuildReplyInstruction(const FConversationContext& Context, const FString& Intent,
                                  const FString& Tone, int32 TargetLength, bool bUseEmoji) const;
    /** Builds the transcript (history plus the new message) the model replies to. */
    TArray<FDTEChatMessage> BuildReplyTranscript(const FString& Input, const FConversationContext& Context,
                                                 const FString& Instruction) const;
    /** Personality traits and current emotional chemistry as prompt context. */
    FString DescribePersonalityForPrompt() const;

    /** Response metadata only (tone, valence, arousal, confidence). Never touches Content. */
    void ApplyPersonalityToResponse(FGeneratedResponse& Response);
    void FillEmotionalMetadata(FGeneratedResponse& Response, const FConversationContext& Context) const;
    FGeneratedResponse MakePendingResponse(const FConversationContext& Context) const;

    void RecordReply(const FString& ParticipantId, const FString& Content);
    void MarkUnanswered(const FString& MessageId, const FString& Source, const FString& Reason);
    void UpdateEmotionalStateFromInteraction(const FString& MessageContent, bool bIsIncoming);

private:
    // ===== Component References =====

    /** Writes every reply. Resolved in BeginPlay via UDTELanguageModelBridge::FindOrAdd(GetOwner()). */
    UPROPERTY()
    UDTELanguageModelBridge* ModelBridge;

    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem;

    UPROPERTY()
    UDiaryInsightBlogLoop* NarrativeSystem;

    UPROPERTY()
    UAvatar9PServer* P9Server;

    UPROPERTY()
    UAvatarAtomSpaceClient* AtomSpaceClient;

    // ===== Connection State =====

    UPROPERTY()
    bool bDeltaChatConnected;

    UPROPERTY()
    bool bDovecotConnected;

    UPROPERTY()
    FString DeltaChatAccountEmail;

    UPROPERTY()
    FString DovecotImapHost;

    UPROPERTY()
    int32 DovecotImapPort;

    // ===== Message Queues =====

    UPROPERTY()
    TArray<FDeltaChatMessage> PendingDeltaChatMessages;

    UPROPERTY()
    TArray<FDovecotEmail> PendingEmails;

    UPROPERTY()
    TArray<FGeneratedResponse> PendingResponses;

    /** Ids of messages/emails the model did not answer. Readable at /mnt/avatar/communication/unanswered. */
    UPROPERTY(VisibleAnywhere, Category = "Communication|Status")
    TArray<FString> UnansweredMessageIds;

    UPROPERTY(VisibleAnywhere, Category = "Communication|Status")
    int32 InFlightRequests;

    // ===== Conversation State =====

    UPROPERTY()
    TMap<FString, FConversationContext> ActiveConversations;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    float MessageProcessingInterval;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    int32 MaxPendingMessages;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    float MinResponseDelay;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    float MaxResponseDelay;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    bool bAutoRespond;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    bool bVerboseLogging;

    // ===== Timers =====

    float ProcessingTimer;
};

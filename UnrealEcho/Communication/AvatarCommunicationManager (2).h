#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
 */
USTRUCT(BlueprintType)
struct FGeneratedResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ResponseId;

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

    FGeneratedResponse()
        : Confidence(0.8f)
        , EmotionalValence(0.5f)
        , EmotionalArousal(0.5f)
        , bRequiresConfirmation(false)
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> MessageHistory;

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
 * Response Generation (craft reply)
 *     |
 *     v
 * DeltaChat/Dovecot <- 9P Interface <- Avatar Communication Manager
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

    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    FGeneratedResponse GenerateResponse(const FString& Input, const FConversationContext& Context);

    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    FGeneratedResponse GenerateDeltaChatResponse(const FDeltaChatMessage& Message);

    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    FGeneratedResponse GenerateEmailResponse(const FDovecotEmail& Email);

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

    UFUNCTION(BlueprintCallable, Category = "Communication|Narrative")
    void LogToNarrativeSystem(const FDeltaChatMessage& Message, const FGeneratedResponse& Response);

    UFUNCTION(BlueprintCallable, Category = "Communication|Narrative")
    void LogEmailToNarrativeSystem(const FDovecotEmail& Email, const FGeneratedResponse& Response);

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
    FString GenerateResponseText(const FString& Input, const FConversationContext& Context);
    void ApplyPersonalityToResponse(FGeneratedResponse& Response);
    void UpdateEmotionalStateFromInteraction(const FString& MessageContent, bool bIsIncoming);

private:
    // ===== Component References =====

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

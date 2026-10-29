#include "AvatarCommunicationManager.h"
#include "../Personality/PersonalityTraitSystem.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "../Narrative/DiaryInsightBlogLoop.h"
#include "../9P/Avatar9PServer.h"
#include "../AtomSpace/AvatarAtomSpaceClient.h"
#include "DTELanguageModelBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommunication, Log, All);

UAvatarCommunicationManager::UAvatarCommunicationManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz

    bDeltaChatConnected = false;
    bDovecotConnected = false;
    DovecotImapPort = 993;
    MessageProcessingInterval = 0.5f;
    MaxPendingMessages = 100;
    MinResponseDelay = 0.5f;
    MaxResponseDelay = 3.0f;
    bAutoRespond = true;
    bVerboseLogging = false;
    ProcessingTimer = 0.0f;
    ModelBridge = nullptr;
    InFlightRequests = 0;
}

void UAvatarCommunicationManager::BeginPlay()
{
    Super::BeginPlay();

    // Find sibling components
    if (AActor* Owner = GetOwner())
    {
        PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
        NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
        NarrativeSystem = Owner->FindComponentByClass<UDiaryInsightBlogLoop>();
        P9Server = Owner->FindComponentByClass<UAvatar9PServer>();
        AtomSpaceClient = Owner->FindComponentByClass<UAvatarAtomSpaceClient>();

        // The model writes every reply; without it nothing is ever sent.
        ModelBridge = UDTELanguageModelBridge::FindOrAdd(Owner);
    }

    if (!ModelBridge)
    {
        UE_LOG(LogCommunication, Warning, TEXT("No language model bridge could be attached; incoming messages will stay unanswered"));
    }

    // Register 9P namespace entries for communication
    Register9PNamespace();

    UE_LOG(LogCommunication, Log, TEXT("Avatar Communication Manager initialized"));
}

void UAvatarCommunicationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisconnectAll();
    Super::EndPlay(EndPlayReason);
}

void UAvatarCommunicationManager::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ProcessingTimer += DeltaTime;
    if (ProcessingTimer >= MessageProcessingInterval)
    {
        ProcessPendingMessages();
        ProcessingTimer = 0.0f;
    }
}

// ===== Connection Management =====

void UAvatarCommunicationManager::ConnectDeltaChat(const FString& AccountEmail, const FString& Password)
{
    DeltaChatAccountEmail = AccountEmail;
    // In real implementation, would initialize DeltaChat core library
    bDeltaChatConnected = true;

    UE_LOG(LogCommunication, Log, TEXT("Connected to DeltaChat with account: %s"), *AccountEmail);
}

void UAvatarCommunicationManager::ConnectDovecot(const FString& ImapHost, int32 ImapPort,
                                                   const FString& Username, const FString& Password)
{
    DovecotImapHost = ImapHost;
    DovecotImapPort = ImapPort;
    // In real implementation, would establish IMAP connection
    bDovecotConnected = true;

    UE_LOG(LogCommunication, Log, TEXT("Connected to Dovecot IMAP at %s:%d"), *ImapHost, ImapPort);
}

void UAvatarCommunicationManager::DisconnectAll()
{
    bDeltaChatConnected = false;
    bDovecotConnected = false;
    PendingDeltaChatMessages.Empty();
    PendingEmails.Empty();

    UE_LOG(LogCommunication, Log, TEXT("Disconnected from all communication services"));
}

// ===== Message Reception =====

void UAvatarCommunicationManager::OnDeltaChatMessageReceived(const FDeltaChatMessage& Message)
{
    if (PendingDeltaChatMessages.Num() < MaxPendingMessages)
    {
        PendingDeltaChatMessages.Add(Message);

        // Get or create conversation context
        FConversationContext Context = GetOrCreateConversation(Message.SenderId, EMessageSource::DeltaChat);
        Context.MessageHistory.Add(Message.Content);
        Context.LastActivityAt = FDateTime::Now();
        Context.MessageCount++;
        Context.SentimentScore = AnalyzeSentiment(Message.Content);
        ActiveConversations.Add(Message.SenderId, Context);

        // Update emotional state based on incoming message
        UpdateEmotionalStateFromInteraction(Message.Content, true);

        // Broadcast event
        OnDeltaChatMessage.Broadcast(Message);

        if (bVerboseLogging)
        {
            UE_LOG(LogCommunication, Verbose, TEXT("Received DeltaChat message from %s: %s"),
                *Message.SenderName, *Message.Content);
        }
    }
}

void UAvatarCommunicationManager::OnEmailReceived(const FDovecotEmail& Email)
{
    if (PendingEmails.Num() < MaxPendingMessages)
    {
        PendingEmails.Add(Email);

        // Get or create conversation context
        FConversationContext Context = GetOrCreateConversation(Email.From, EMessageSource::Dovecot);
        Context.MessageHistory.Add(Email.BodyText);
        Context.LastActivityAt = FDateTime::Now();
        Context.MessageCount++;
        Context.Topic = Email.Subject;
        Context.SentimentScore = AnalyzeSentiment(Email.BodyText);
        ActiveConversations.Add(Email.From, Context);

        // Update emotional state
        UpdateEmotionalStateFromInteraction(Email.BodyText, true);

        // Broadcast event
        OnEmail.Broadcast(Email);

        if (bVerboseLogging)
        {
            UE_LOG(LogCommunication, Verbose, TEXT("Received email from %s: %s"), *Email.From, *Email.Subject);
        }
    }
}

void UAvatarCommunicationManager::ProcessPendingMessages()
{
    ProcessDeltaChatQueue();
    ProcessEmailQueue();
}

void UAvatarCommunicationManager::ProcessDeltaChatQueue()
{
    if (PendingDeltaChatMessages.Num() == 0 || !bAutoRespond)
    {
        return;
    }

    // Dispatch one message per processing interval. The message leaves the queue
    // now; if the model does not answer it is recorded as unanswered, not retried.
    FDeltaChatMessage Message = PendingDeltaChatMessages[0];
    PendingDeltaChatMessages.RemoveAt(0);

    TWeakObjectPtr<UAvatarCommunicationManager> WeakThis(this);
    RequestDeltaChatResponseInternal(Message, FGuid::NewGuid().ToString(),
        [WeakThis, Message](bool bSuccess, const FGeneratedResponse& Response)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            if (!bSuccess)
            {
                WeakThis->MarkUnanswered(Message.MessageId, TEXT("DeltaChat"), Response.Tone);
                return;
            }

            // Side effects only once the model has written the reply
            WeakThis->LogToNarrativeSystem(Message, Response);
            WeakThis->SendDeltaChatResponse(Message.ChatId, Response);
            WeakThis->RecordReply(Message.SenderId, Response.Content);
        });
}

void UAvatarCommunicationManager::ProcessEmailQueue()
{
    if (PendingEmails.Num() == 0 || !bAutoRespond)
    {
        return;
    }

    // Dispatch one email per processing interval; see ProcessDeltaChatQueue.
    FDovecotEmail Email = PendingEmails[0];
    PendingEmails.RemoveAt(0);

    TWeakObjectPtr<UAvatarCommunicationManager> WeakThis(this);
    RequestEmailResponseInternal(Email, FGuid::NewGuid().ToString(),
        [WeakThis, Email](bool bSuccess, const FGeneratedResponse& Response)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            if (!bSuccess)
            {
                WeakThis->MarkUnanswered(Email.EmailId, TEXT("Email"), Response.Tone);
                return;
            }

            WeakThis->LogEmailToNarrativeSystem(Email, Response);
            WeakThis->SendEmailResponse(Email.EmailId, Response);
            WeakThis->RecordReply(Email.From, Response.Content);
        });
}

// ===== Response Generation =====

void UAvatarCommunicationManager::RequestResponse(const FString& Input, const FConversationContext& Context, FOnCommunicationResponseReady OnDone)
{
    RequestResponseInternal(Input, Context, FGuid::NewGuid().ToString(),
        [OnDone](bool bSuccess, const FGeneratedResponse& Response)
        {
            OnDone.ExecuteIfBound(bSuccess, Response);
        });
}

void UAvatarCommunicationManager::RequestDeltaChatResponse(const FDeltaChatMessage& Message, FOnCommunicationResponseReady OnDone)
{
    RequestDeltaChatResponseInternal(Message, FGuid::NewGuid().ToString(),
        [OnDone](bool bSuccess, const FGeneratedResponse& Response)
        {
            OnDone.ExecuteIfBound(bSuccess, Response);
        });
}

void UAvatarCommunicationManager::RequestEmailResponse(const FDovecotEmail& Email, FOnCommunicationResponseReady OnDone)
{
    RequestEmailResponseInternal(Email, FGuid::NewGuid().ToString(),
        [OnDone](bool bSuccess, const FGeneratedResponse& Response)
        {
            OnDone.ExecuteIfBound(bSuccess, Response);
        });
}

FGeneratedResponse UAvatarCommunicationManager::GenerateResponse(const FString& Input, const FConversationContext& Context)
{
    // Compatibility wrapper: the reply is written asynchronously by the model and
    // delivered through OnResponseGenerated keyed by Pending.ResponseId.
    FGeneratedResponse Pending = MakePendingResponse(Context);
    const FString ResponseId = Pending.ResponseId;

    TWeakObjectPtr<UAvatarCommunicationManager> WeakThis(this);
    RequestResponseInternal(Input, Context, ResponseId,
        [WeakThis, ResponseId](bool bSuccess, const FGeneratedResponse& Response)
        {
            if (bSuccess && WeakThis.IsValid())
            {
                WeakThis->OnResponseGenerated.Broadcast(ResponseId, Response);
            }
        });

    return Pending;
}

FGeneratedResponse UAvatarCommunicationManager::GenerateDeltaChatResponse(const FDeltaChatMessage& Message)
{
    // Compatibility wrapper; the written reply arrives via OnResponseGenerated(Message.MessageId).
    FConversationContext Context = GetOrCreateConversation(Message.SenderId, EMessageSource::DeltaChat);
    FGeneratedResponse Pending = MakePendingResponse(Context);
    RequestDeltaChatResponseInternal(Message, Pending.ResponseId, FCommunicationResponseCallback());
    return Pending;
}

FGeneratedResponse UAvatarCommunicationManager::GenerateEmailResponse(const FDovecotEmail& Email)
{
    // Compatibility wrapper; the written reply arrives via OnResponseGenerated(Email.EmailId).
    FConversationContext Context = GetOrCreateConversation(Email.From, EMessageSource::Dovecot);
    FGeneratedResponse Pending = MakePendingResponse(Context);
    Pending.Tone = TEXT("professional");
    RequestEmailResponseInternal(Email, Pending.ResponseId, FCommunicationResponseCallback());
    return Pending;
}

void UAvatarCommunicationManager::RequestResponseInternal(const FString& Input, const FConversationContext& Context,
                                                          const FString& ResponseId, FCommunicationResponseCallback OnDone)
{
    FGeneratedResponse Result = MakePendingResponse(Context);
    Result.ResponseId = ResponseId;

    if (!ModelBridge)
    {
        UE_LOG(LogCommunication, Warning, TEXT("Reply to %s not written: no language model bridge on %s"),
            *Context.ParticipantId, *GetNameSafe(GetOwner()));
        Result.bIsPending = false;
        Result.Tone = TEXT("no language model bridge");
        if (OnDone)
        {
            OnDone(false, Result);
        }
        return;
    }

    // Analysis of the incoming text (not character speech) becomes prompt context.
    const FString Intent = DetectIntent(Input);
    const FString Tone = Context.Source == EMessageSource::Dovecot ? TEXT("professional") : DetermineResponseTone(Context);
    const int32 TargetLength = CalculateResponseLength(Context);
    const bool bUseEmoji = ShouldAddEmoji(Context);

    const FString Instruction = BuildReplyInstruction(Context, Intent, Tone, TargetLength, bUseEmoji);
    const TArray<FDTEChatMessage> Transcript = BuildReplyTranscript(Input, Context, Instruction);

    // Generous token budget for the target character count; the prompt states the limit.
    const int32 MaxTokens = FMath::Clamp(TargetLength, 96, 1024);

    InFlightRequests++;

    TWeakObjectPtr<UAvatarCommunicationManager> WeakThis(this);
    ModelBridge->RequestChat(Transcript,
        [WeakThis, Result, Context, Tone, OnDone](bool bSuccess, const FString& TextOrError) mutable
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            WeakThis->InFlightRequests = FMath::Max(0, WeakThis->InFlightRequests - 1);
            Result.bIsPending = false;

            if (!bSuccess || TextOrError.TrimStartAndEnd().IsEmpty())
            {
                // Honest failure: no text, nothing said. Tone carries the operator-facing reason.
                UE_LOG(LogCommunication, Warning, TEXT("Model did not write a reply for %s (%s)"),
                    *Context.ParticipantId, *TextOrError);
                Result.Content.Empty();
                Result.Tone = TextOrError;
                if (OnDone)
                {
                    OnDone(false, Result);
                }
                return;
            }

            Result.Content = TextOrError.TrimStartAndEnd();
            Result.Tone = Tone;
            WeakThis->ApplyPersonalityToResponse(Result);
            WeakThis->FillEmotionalMetadata(Result, Context);

            if (OnDone)
            {
                OnDone(true, Result);
            }
        },
        ModelBridge->Temperature, MaxTokens);
}

void UAvatarCommunicationManager::RequestDeltaChatResponseInternal(const FDeltaChatMessage& Message, const FString& ResponseId,
                                                                   FCommunicationResponseCallback OnDone)
{
    FConversationContext Context = GetOrCreateConversation(Message.SenderId, EMessageSource::DeltaChat);
    Context.ParticipantName = Message.SenderName;
    if (FConversationContext* Stored = ActiveConversations.Find(Message.SenderId))
    {
        Stored->ParticipantName = Message.SenderName;
    }

    const FString MessageId = Message.MessageId;
    TWeakObjectPtr<UAvatarCommunicationManager> WeakThis(this);
    RequestResponseInternal(Message.Content, Context, ResponseId,
        [WeakThis, MessageId, OnDone](bool bSuccess, const FGeneratedResponse& Response)
        {
            if (bSuccess && WeakThis.IsValid())
            {
                WeakThis->OnResponseGenerated.Broadcast(MessageId, Response);
            }
            if (OnDone)
            {
                OnDone(bSuccess, Response);
            }
        });
}

void UAvatarCommunicationManager::RequestEmailResponseInternal(const FDovecotEmail& Email, const FString& ResponseId,
                                                               FCommunicationResponseCallback OnDone)
{
    FConversationContext Context = GetOrCreateConversation(Email.From, EMessageSource::Dovecot);
    Context.Topic = Email.Subject;
    if (Context.ParticipantName.IsEmpty())
    {
        Context.ParticipantName = Email.From;
    }

    // Subject and body are both part of what the model replies to.
    const FString Input = Email.Subject.IsEmpty()
        ? Email.BodyText
        : FString::Printf(TEXT("Subject: %s\n\n%s"), *Email.Subject, *Email.BodyText);

    const FString EmailId = Email.EmailId;
    TWeakObjectPtr<UAvatarCommunicationManager> WeakThis(this);
    RequestResponseInternal(Input, Context, ResponseId,
        [WeakThis, EmailId, OnDone](bool bSuccess, const FGeneratedResponse& Response)
        {
            if (bSuccess && WeakThis.IsValid())
            {
                WeakThis->OnResponseGenerated.Broadcast(EmailId, Response);
            }
            if (OnDone)
            {
                OnDone(bSuccess, Response);
            }
        });
}

void UAvatarCommunicationManager::SendDeltaChatResponse(const FString& ChatId, const FGeneratedResponse& Response)
{
    if (Response.bIsPending || Response.Content.IsEmpty())
    {
        UE_LOG(LogCommunication, Warning, TEXT("Refusing to send an empty or pending response to chat %s"), *ChatId);
        return;
    }

    // In real implementation, would send via DeltaChat API
    UE_LOG(LogCommunication, Log, TEXT("Sending DeltaChat response to chat %s: %s"), *ChatId, *Response.Content.Left(50));

    // Update emotional state based on outgoing message
    UpdateEmotionalStateFromInteraction(Response.Content, false);
}

void UAvatarCommunicationManager::SendEmailResponse(const FString& EmailId, const FGeneratedResponse& Response)
{
    if (Response.bIsPending || Response.Content.IsEmpty())
    {
        UE_LOG(LogCommunication, Warning, TEXT("Refusing to send an empty or pending response for email %s"), *EmailId);
        return;
    }

    // In real implementation, would send via SMTP
    UE_LOG(LogCommunication, Log, TEXT("Sending email response for %s: %s"), *EmailId, *Response.Content.Left(50));

    // Update emotional state
    UpdateEmotionalStateFromInteraction(Response.Content, false);
}

// ===== Conversation Management =====

FConversationContext UAvatarCommunicationManager::GetOrCreateConversation(const FString& ParticipantId, EMessageSource Source)
{
    if (FConversationContext* Existing = ActiveConversations.Find(ParticipantId))
    {
        return *Existing;
    }

    FConversationContext NewContext;
    NewContext.ConversationId = FGuid::NewGuid().ToString();
    NewContext.ParticipantId = ParticipantId;
    NewContext.Source = Source;
    NewContext.State = EConversationState::Active;
    NewContext.StartedAt = FDateTime::Now();
    NewContext.LastActivityAt = FDateTime::Now();

    ActiveConversations.Add(ParticipantId, NewContext);
    return NewContext;
}

void UAvatarCommunicationManager::UpdateConversationState(const FString& ConversationId, EConversationState NewState)
{
    for (auto& Pair : ActiveConversations)
    {
        if (Pair.Value.ConversationId == ConversationId)
        {
            Pair.Value.State = NewState;
            OnConversationStateChanged.Broadcast(ConversationId, NewState);
            return;
        }
    }
}

FConversationContext UAvatarCommunicationManager::GetConversation(const FString& ConversationId)
{
    for (auto& Pair : ActiveConversations)
    {
        if (Pair.Value.ConversationId == ConversationId)
        {
            return Pair.Value;
        }
    }
    return FConversationContext();
}

TArray<FConversationContext> UAvatarCommunicationManager::GetActiveConversations()
{
    TArray<FConversationContext> Contexts;
    for (auto& Pair : ActiveConversations)
    {
        if (Pair.Value.State == EConversationState::Active)
        {
            Contexts.Add(Pair.Value);
        }
    }
    return Contexts;
}

void UAvatarCommunicationManager::EndConversation(const FString& ConversationId)
{
    UpdateConversationState(ConversationId, EConversationState::Ended);

    // Extract insights from conversation before ending
    FConversationContext Context = GetConversation(ConversationId);
    if (Context.MessageCount > 3)
    {
        ExtractInsightsFromConversation(Context);
    }
}

// ===== Personality-Driven Response Style =====

FString UAvatarCommunicationManager::DetermineResponseTone(const FConversationContext& Context)
{
    if (!PersonalitySystem)
    {
        return TEXT("neutral");
    }

    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();

    // High playfulness + high confidence = flirty
    if (SHGTrait.Playfulness > 0.7f && SHGTrait.Confidence > 0.7f)
    {
        return TEXT("flirty");
    }

    // High confidence + low playfulness = assertive
    if (SHGTrait.Confidence > 0.8f && SHGTrait.Playfulness < 0.4f)
    {
        return TEXT("assertive");
    }

    // High unpredictability = chaotic/quirky
    if (HCTrait.UnpredictabilityFactor > 0.6f)
    {
        return TEXT("quirky");
    }

    // High charm = warm
    if (SHGTrait.Charm > 0.7f)
    {
        return TEXT("warm");
    }

    // Negative sentiment context = empathetic
    if (Context.SentimentScore < 0.3f)
    {
        return TEXT("empathetic");
    }

    return TEXT("friendly");
}

float UAvatarCommunicationManager::CalculateResponseDelay(const FConversationContext& Context)
{
    if (!PersonalitySystem)
    {
        return (MinResponseDelay + MaxResponseDelay) / 2.0f;
    }

    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();
    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();

    // Higher impulsivity = faster response
    float ImpulsivityFactor = 1.0f - HCTrait.ImpulsivityLevel;

    // Higher confidence = faster response
    float ConfidenceFactor = 1.0f - (SHGTrait.Confidence * 0.3f);

    // Long conversation = faster response (more engaged)
    float EngagementFactor = FMath::Clamp(1.0f - (Context.MessageCount * 0.05f), 0.3f, 1.0f);

    float Delay = MinResponseDelay + (MaxResponseDelay - MinResponseDelay) * ImpulsivityFactor * ConfidenceFactor * EngagementFactor;

    return FMath::Clamp(Delay, MinResponseDelay, MaxResponseDelay);
}

bool UAvatarCommunicationManager::ShouldAddEmoji(const FConversationContext& Context)
{
    if (!PersonalitySystem)
    {
        return false;
    }

    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();

    // Higher expressiveness = more likely to use emoji
    float EmojiProbability = SHGTrait.Expressiveness * 0.8f + SHGTrait.Playfulness * 0.2f;

    // DeltaChat messages more likely to have emoji
    if (Context.Source == EMessageSource::DeltaChat)
    {
        EmojiProbability += 0.2f;
    }

    // Email less likely
    if (Context.Source == EMessageSource::Dovecot)
    {
        EmojiProbability -= 0.3f;
    }

    return FMath::FRand() < EmojiProbability;
}

int32 UAvatarCommunicationManager::CalculateResponseLength(const FConversationContext& Context)
{
    // Base length
    int32 BaseLength = 100;

    // Email = longer
    if (Context.Source == EMessageSource::Dovecot)
    {
        BaseLength = 300;
    }

    // Longer conversation = shorter responses (more back-and-forth)
    if (Context.MessageCount > 10)
    {
        BaseLength = FMath::Max(50, BaseLength - 50);
    }

    // Negative sentiment = longer (more supportive)
    if (Context.SentimentScore < 0.3f)
    {
        BaseLength += 50;
    }

    return BaseLength;
}

// ===== Narrative Integration =====

void UAvatarCommunicationManager::LogToNarrativeSystem(const FDeltaChatMessage& Message, const FGeneratedResponse& Response)
{
    if (Response.bIsPending || Response.Content.IsEmpty())
    {
        return; // Nothing was said; nothing to record.
    }

    if (AtomSpaceClient)
    {
        // Transcript record of the exchange: the incoming text and the reply the model wrote.
        FDiaryEntry Entry;
        Entry.EntryId = FGuid::NewGuid().ToString();
        Entry.Content = FString::Printf(TEXT("[DeltaChat] From %s: %s\n\n[Reply sent] %s"),
            *Message.SenderName, *Message.Content, *Response.Content);
        Entry.Timestamp = FDateTime::Now();
        Entry.EmotionalValence = Response.EmotionalValence;
        Entry.EmotionalArousal = Response.EmotionalArousal;
        Entry.Tags.Add(TEXT("deltachat"));
        Entry.Tags.Add(TEXT("conversation"));

        AtomSpaceClient->StoreDiaryEntry(Entry);
    }
}

void UAvatarCommunicationManager::LogEmailToNarrativeSystem(const FDovecotEmail& Email, const FGeneratedResponse& Response)
{
    if (Response.bIsPending || Response.Content.IsEmpty())
    {
        return; // Nothing was sent; nothing to record.
    }

    if (AtomSpaceClient)
    {
        // Transcript record of the exchange: the incoming email and the reply the model wrote.
        FDiaryEntry Entry;
        Entry.EntryId = FGuid::NewGuid().ToString();
        Entry.Content = FString::Printf(TEXT("[Email] From %s\nSubject: %s\n\n%s\n\n[Reply sent] %s"),
            *Email.From, *Email.Subject, *Email.BodyText.Left(500), *Response.Content);
        Entry.Timestamp = FDateTime::Now();
        Entry.EmotionalValence = Response.EmotionalValence;
        Entry.EmotionalArousal = Response.EmotionalArousal;
        Entry.Tags.Add(TEXT("email"));
        Entry.Tags.Add(TEXT("correspondence"));

        AtomSpaceClient->StoreDiaryEntry(Entry);
    }
}

void UAvatarCommunicationManager::ExtractInsightsFromConversation(const FConversationContext& Context)
{
    if (!AtomSpaceClient)
    {
        return;
    }

    if (!ModelBridge)
    {
        UE_LOG(LogCommunication, Warning, TEXT("No language model bridge; no insight recorded for conversation %s"), *Context.ConversationId);
        return;
    }

    // The reflection is written by the model; the numbers below are only context for it.
    const FString Instruction = TEXT(
        "Reflect, in one or two sentences and in your own voice, on a conversation you just finished. "
        "Note what the exchange was like for you and anything you want to remember about how to talk "
        "with this person next time. Write only the reflection.");

    FString PromptContext;
    PromptContext += FString::Printf(TEXT("Participant: %s\n"), *(Context.ParticipantName.IsEmpty() ? Context.ParticipantId : Context.ParticipantName));
    PromptContext += FString::Printf(TEXT("Channel: %s\n"), *UEnum::GetDisplayValueAsText(Context.Source).ToString());
    if (!Context.Topic.IsEmpty())
    {
        PromptContext += FString::Printf(TEXT("Topic: %s\n"), *Context.Topic);
    }
    PromptContext += FString::Printf(TEXT("Messages exchanged: %d\n"), Context.MessageCount);
    PromptContext += FString::Printf(TEXT("Overall sentiment of their messages (0 negative .. 1 positive): %.2f\n"), Context.SentimentScore);
    if (Context.MessageHistory.Num() > 0)
    {
        PromptContext += TEXT("Their recent messages:\n");
        const int32 First = FMath::Max(0, Context.MessageHistory.Num() - 5);
        for (int32 Index = First; Index < Context.MessageHistory.Num(); ++Index)
        {
            PromptContext += FString::Printf(TEXT("- %s\n"), *Context.MessageHistory[Index].Left(300));
        }
    }
    if (Context.ReplyHistory.Num() > 0)
    {
        PromptContext += TEXT("Your recent replies:\n");
        const int32 First = FMath::Max(0, Context.ReplyHistory.Num() - 5);
        for (int32 Index = First; Index < Context.ReplyHistory.Num(); ++Index)
        {
            PromptContext += FString::Printf(TEXT("- %s\n"), *Context.ReplyHistory[Index].Left(300));
        }
    }
    PromptContext += DescribePersonalityForPrompt();

    const float InsightConfidence = FMath::Clamp(Context.MessageCount * 0.1f, 0.3f, 0.9f);
    const FString ConversationId = Context.ConversationId;

    TWeakObjectPtr<UAvatarCommunicationManager> WeakThis(this);
    ModelBridge->RequestText(Instruction, PromptContext,
        [WeakThis, ConversationId, InsightConfidence](bool bSuccess, const FString& TextOrError)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            if (!bSuccess || TextOrError.TrimStartAndEnd().IsEmpty())
            {
                UE_LOG(LogCommunication, Warning, TEXT("Model did not write an insight for conversation %s (%s)"),
                    *ConversationId, *TextOrError);
                return;
            }

            if (!WeakThis->AtomSpaceClient)
            {
                return;
            }

            FInsight Insight;
            Insight.InsightId = FGuid::NewGuid().ToString();
            Insight.Category = TEXT("communication_pattern");
            Insight.Content = TextOrError.TrimStartAndEnd();
            Insight.Confidence = InsightConfidence;
            WeakThis->AtomSpaceClient->StoreInsight(Insight);
        });
}

// ===== Sentiment Analysis =====

float UAvatarCommunicationManager::AnalyzeSentiment(const FString& Text)
{
    // Simple keyword-based sentiment analysis
    float Sentiment = 0.5f;

    // Positive keywords
    TArray<FString> PositiveWords = {
        TEXT("love"), TEXT("great"), TEXT("amazing"), TEXT("wonderful"), TEXT("excellent"),
        TEXT("happy"), TEXT("joy"), TEXT("thanks"), TEXT("thank"), TEXT("awesome"),
        TEXT("beautiful"), TEXT("perfect"), TEXT("fantastic"), TEXT("good"), TEXT("nice")
    };

    // Negative keywords
    TArray<FString> NegativeWords = {
        TEXT("hate"), TEXT("bad"), TEXT("terrible"), TEXT("awful"), TEXT("horrible"),
        TEXT("sad"), TEXT("angry"), TEXT("upset"), TEXT("disappointed"), TEXT("annoyed"),
        TEXT("frustrated"), TEXT("problem"), TEXT("issue"), TEXT("wrong"), TEXT("sorry")
    };

    FString LowerText = Text.ToLower();

    for (const FString& Word : PositiveWords)
    {
        if (LowerText.Contains(Word))
        {
            Sentiment += 0.1f;
        }
    }

    for (const FString& Word : NegativeWords)
    {
        if (LowerText.Contains(Word))
        {
            Sentiment -= 0.1f;
        }
    }

    return FMath::Clamp(Sentiment, 0.0f, 1.0f);
}

TArray<FString> UAvatarCommunicationManager::ExtractKeywords(const FString& Text)
{
    TArray<FString> Keywords;

    // Simple word extraction (would use NLP in full implementation)
    TArray<FString> Words;
    Text.ParseIntoArray(Words, TEXT(" "));

    // Filter short words and common words
    TArray<FString> StopWords = {
        TEXT("the"), TEXT("a"), TEXT("an"), TEXT("is"), TEXT("are"), TEXT("was"),
        TEXT("were"), TEXT("be"), TEXT("been"), TEXT("being"), TEXT("have"), TEXT("has"),
        TEXT("had"), TEXT("do"), TEXT("does"), TEXT("did"), TEXT("will"), TEXT("would"),
        TEXT("could"), TEXT("should"), TEXT("may"), TEXT("might"), TEXT("must"),
        TEXT("i"), TEXT("you"), TEXT("he"), TEXT("she"), TEXT("it"), TEXT("we"), TEXT("they")
    };

    for (const FString& Word : Words)
    {
        FString CleanWord = Word.ToLower().TrimStartAndEnd();
        if (CleanWord.Len() > 3 && !StopWords.Contains(CleanWord))
        {
            Keywords.AddUnique(CleanWord);
        }
    }

    return Keywords;
}

FString UAvatarCommunicationManager::DetectIntent(const FString& Text)
{
    FString LowerText = Text.ToLower();

    // Question detection
    if (LowerText.Contains(TEXT("?")) || LowerText.Contains(TEXT("how")) ||
        LowerText.Contains(TEXT("what")) || LowerText.Contains(TEXT("why")) ||
        LowerText.Contains(TEXT("when")) || LowerText.Contains(TEXT("where")))
    {
        return TEXT("question");
    }

    // Request detection
    if (LowerText.Contains(TEXT("please")) || LowerText.Contains(TEXT("could you")) ||
        LowerText.Contains(TEXT("can you")) || LowerText.Contains(TEXT("help")))
    {
        return TEXT("request");
    }

    // Greeting detection
    if (LowerText.Contains(TEXT("hello")) || LowerText.Contains(TEXT("hi")) ||
        LowerText.Contains(TEXT("hey")) || LowerText.Contains(TEXT("good morning")))
    {
        return TEXT("greeting");
    }

    // Farewell detection
    if (LowerText.Contains(TEXT("bye")) || LowerText.Contains(TEXT("goodbye")) ||
        LowerText.Contains(TEXT("see you")) || LowerText.Contains(TEXT("take care")))
    {
        return TEXT("farewell");
    }

    // Compliment detection
    if (LowerText.Contains(TEXT("love")) || LowerText.Contains(TEXT("amazing")) ||
        LowerText.Contains(TEXT("beautiful")) || LowerText.Contains(TEXT("wonderful")))
    {
        return TEXT("compliment");
    }

    return TEXT("statement");
}

// ===== 9P Integration =====

void UAvatarCommunicationManager::Register9PNamespace()
{
    if (!P9Server)
    {
        return;
    }

    // Register communication namespace entries
    FAvatarNamespaceEntry CommDir;
    CommDir.Name = TEXT("communication");
    CommDir.FullPath = TEXT("/mnt/avatar/communication");
    CommDir.bIsDirectory = true;
    CommDir.bReadable = true;
    CommDir.Description = TEXT("Communication state and message handling");
    P9Server->RegisterNamespaceEntry(CommDir);

    // Messages inbox
    FAvatarNamespaceEntry InboxEntry;
    InboxEntry.Name = TEXT("inbox");
    InboxEntry.FullPath = TEXT("/mnt/avatar/communication/inbox");
    InboxEntry.bIsDirectory = false;
    InboxEntry.bReadable = true;
    InboxEntry.Description = TEXT("Pending messages");
    P9Server->RegisterNamespaceEntry(InboxEntry);

    // Send message
    FAvatarNamespaceEntry SendEntry;
    SendEntry.Name = TEXT("send");
    SendEntry.FullPath = TEXT("/mnt/avatar/communication/send");
    SendEntry.bIsDirectory = false;
    SendEntry.bReadable = false;
    SendEntry.bWritable = true;
    SendEntry.Description = TEXT("Send message (write JSON payload)");
    P9Server->RegisterNamespaceEntry(SendEntry);

    // Messages the model left unanswered (operator-facing)
    FAvatarNamespaceEntry UnansweredEntry;
    UnansweredEntry.Name = TEXT("unanswered");
    UnansweredEntry.FullPath = TEXT("/mnt/avatar/communication/unanswered");
    UnansweredEntry.bIsDirectory = false;
    UnansweredEntry.bReadable = true;
    UnansweredEntry.Description = TEXT("Ids of messages the model did not answer (nothing was sent)");
    P9Server->RegisterNamespaceEntry(UnansweredEntry);

    // Conversation list
    FAvatarNamespaceEntry ConversationsEntry;
    ConversationsEntry.Name = TEXT("conversations");
    ConversationsEntry.FullPath = TEXT("/mnt/avatar/communication/conversations");
    ConversationsEntry.bIsDirectory = false;
    ConversationsEntry.bReadable = true;
    ConversationsEntry.Description = TEXT("Active conversations");
    P9Server->RegisterNamespaceEntry(ConversationsEntry);
}

FString UAvatarCommunicationManager::Handle9PRead(const FString& Path)
{
    if (Path.Contains(TEXT("inbox")))
    {
        // Return pending messages as JSON
        TArray<FString> MessageStrings;
        for (const FDeltaChatMessage& Msg : PendingDeltaChatMessages)
        {
            MessageStrings.Add(FString::Printf(TEXT("{\"id\":\"%s\",\"from\":\"%s\",\"content\":\"%s\"}"),
                *Msg.MessageId, *Msg.SenderName, *Msg.Content.Left(100)));
        }
        return FString::Printf(TEXT("[%s]"), *FString::Join(MessageStrings, TEXT(",")));
    }
    else if (Path.Contains(TEXT("conversations")))
    {
        // Return active conversations
        TArray<FString> ConvStrings;
        for (const auto& Pair : ActiveConversations)
        {
            const FConversationContext& Ctx = Pair.Value;
            ConvStrings.Add(FString::Printf(TEXT("{\"id\":\"%s\",\"participant\":\"%s\",\"messages\":%d}"),
                *Ctx.ConversationId, *Ctx.ParticipantName, Ctx.MessageCount));
        }
        return FString::Printf(TEXT("[%s]"), *FString::Join(ConvStrings, TEXT(",")));
    }
    else if (Path.Contains(TEXT("unanswered")))
    {
        TArray<FString> Ids;
        for (const FString& Id : UnansweredMessageIds)
        {
            Ids.Add(FString::Printf(TEXT("\"%s\""), *Id));
        }
        return FString::Printf(TEXT("{\"in_flight\":%d,\"unanswered\":[%s]}"), InFlightRequests, *FString::Join(Ids, TEXT(",")));
    }

    return TEXT("{}");
}

bool UAvatarCommunicationManager::Handle9PWrite(const FString& Path, const FString& Data)
{
    if (Path.Contains(TEXT("send")))
    {
        // Parse JSON and send message
        // In full implementation, would parse and route to appropriate service
        UE_LOG(LogCommunication, Log, TEXT("9P send request: %s"), *Data.Left(100));
        return true;
    }

    return false;
}

// ===== Internal Methods =====

FString UAvatarCommunicationManager::BuildReplyInstruction(const FConversationContext& Context, const FString& Intent,
                                                           const FString& Tone, int32 TargetLength, bool bUseEmoji) const
{
    FString Instruction;

    if (ModelBridge)
    {
        Instruction += ModelBridge->GetSystemPrompt();
        Instruction += TEXT("\n\n");
    }

    const FString Who = Context.ParticipantName.IsEmpty() ? Context.ParticipantId : Context.ParticipantName;

    if (Context.Source == EMessageSource::Dovecot)
    {
        Instruction += FString::Printf(TEXT(
            "You are answering an email from %s. Write the complete reply as you would send it: "
            "a greeting if it fits, the body, and your own sign-off. Do not include a subject line "
            "or any commentary outside the email itself.\n"), *Who);
    }
    else
    {
        Instruction += FString::Printf(TEXT(
            "You are replying to %s in a chat conversation%s. Write only the message you send; "
            "no quoting, no labels, no commentary.\n"),
            *Who,
            Context.Source == EMessageSource::DeltaChat ? TEXT(" over DeltaChat") : TEXT(""));
    }

    // The detected intent of their latest message tells the model what kind of reply is called for.
    if (Intent == TEXT("greeting"))
    {
        Instruction += TEXT("Their latest message is a greeting: greet them back and open the conversation.\n");
    }
    else if (Intent == TEXT("farewell"))
    {
        Instruction += TEXT("Their latest message is a farewell: say goodbye in a way that fits how the conversation went.\n");
    }
    else if (Intent == TEXT("compliment"))
    {
        Instruction += TEXT("Their latest message contains a compliment: acknowledge it genuinely.\n");
    }
    else if (Intent == TEXT("question"))
    {
        Instruction += TEXT("Their latest message asks a question: answer it directly and honestly; if you do not know, say so.\n");
    }
    else if (Intent == TEXT("request"))
    {
        Instruction += TEXT("Their latest message asks for something: respond to the request concretely, saying what you will or cannot do.\n");
    }
    else
    {
        Instruction += TEXT("Their latest message is a statement: respond to what they actually said and keep the conversation going.\n");
    }

    Instruction += FString::Printf(TEXT("Tone: %s.\n"), *Tone);
    Instruction += FString::Printf(TEXT("Length: about %d characters%s.\n"),
        TargetLength,
        Context.Source == EMessageSource::DeltaChat ? TEXT(", never more than 500") : TEXT(""));
    Instruction += bUseEmoji
        ? TEXT("An emoji or two is welcome if it feels natural.\n")
        : TEXT("Do not use emoji.\n");

    if (!Context.Topic.IsEmpty())
    {
        Instruction += FString::Printf(TEXT("Topic of the conversation: %s\n"), *Context.Topic);
    }
    Instruction += FString::Printf(TEXT("Sentiment of their messages so far (0 negative .. 1 positive): %.2f\n"), Context.SentimentScore);
    Instruction += FString::Printf(TEXT("Messages exchanged so far: %d\n"), Context.MessageCount);

    Instruction += DescribePersonalityForPrompt();

    return Instruction;
}

TArray<FDTEChatMessage> UAvatarCommunicationManager::BuildReplyTranscript(const FString& Input, const FConversationContext& Context,
                                                                          const FString& Instruction) const
{
    TArray<FDTEChatMessage> Transcript;
    Transcript.Add(FDTEChatMessage(TEXT("system"), Instruction));

    // Earlier incoming messages and the replies that were actually sent. MessageHistory
    // already contains the latest incoming message, so it is excluded here and appended last.
    const int32 MaxHistory = 10;
    const int32 PriorIncoming = FMath::Max(0, Context.MessageHistory.Num() - 1);
    const int32 FirstIncoming = FMath::Max(0, PriorIncoming - MaxHistory);
    const int32 FirstReply = FMath::Max(0, Context.ReplyHistory.Num() - MaxHistory);

    // Replies are interleaved after the incoming message they followed where counts line up.
    int32 ReplyIndex = FirstReply;
    for (int32 Index = FirstIncoming; Index < PriorIncoming; ++Index)
    {
        Transcript.Add(FDTEChatMessage(TEXT("user"), Context.MessageHistory[Index]));
        if (ReplyIndex < Context.ReplyHistory.Num())
        {
            Transcript.Add(FDTEChatMessage(TEXT("assistant"), Context.ReplyHistory[ReplyIndex]));
            ++ReplyIndex;
        }
    }
    for (; ReplyIndex < Context.ReplyHistory.Num(); ++ReplyIndex)
    {
        Transcript.Add(FDTEChatMessage(TEXT("assistant"), Context.ReplyHistory[ReplyIndex]));
    }

    Transcript.Add(FDTEChatMessage(TEXT("user"), Input));
    return Transcript;
}

FString UAvatarCommunicationManager::DescribePersonalityForPrompt() const
{
    FString Description;

    if (PersonalitySystem)
    {
        const FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
        const FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();

        Description += TEXT("Your personality right now (0 low .. 1 high): ");
        Description += FString::Printf(TEXT("confidence %.2f, charm %.2f, playfulness %.2f, elegance %.2f, expressiveness %.2f, "),
            SHGTrait.Confidence, SHGTrait.Charm, SHGTrait.Playfulness, SHGTrait.Elegance, SHGTrait.Expressiveness);
        Description += FString::Printf(TEXT("unpredictability %.2f, emotional volatility %.2f, impulsivity %.2f. "),
            HCTrait.UnpredictabilityFactor, HCTrait.EmotionalVolatility, HCTrait.ImpulsivityLevel);
        Description += TEXT("Let these shape how you write, not what you claim about yourself.\n");
    }

    if (NeurochemicalSystem)
    {
        const FEmotionalChemistry Emotions = NeurochemicalSystem->GetEmotionalChemistry();
        Description += FString::Printf(TEXT("Your current mood (0 .. 1): happiness %.2f, excitement %.2f, anxiety %.2f.\n"),
            Emotions.Happiness, Emotions.Excitement, Emotions.Anxiety);
    }

    return Description;
}

void UAvatarCommunicationManager::ApplyPersonalityToResponse(FGeneratedResponse& Response)
{
    // Personality reaches the reply through the prompt (DescribePersonalityForPrompt).
    // Here only metadata is adjusted; Content is never modified.
    if (!PersonalitySystem)
    {
        return;
    }

    const FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
    if (SHGTrait.Charm > 0.8f && Response.Tone != TEXT("professional"))
    {
        Response.Tone = TEXT("warm");
    }
}

void UAvatarCommunicationManager::FillEmotionalMetadata(FGeneratedResponse& Response, const FConversationContext& Context) const
{
    if (NeurochemicalSystem)
    {
        const FEmotionalChemistry Emotions = NeurochemicalSystem->GetEmotionalChemistry();
        Response.EmotionalValence = (Emotions.Happiness - Emotions.Anxiety) * 0.5f + 0.5f;
        Response.EmotionalArousal = Emotions.Excitement;
    }

    Response.Confidence = FMath::Clamp(0.5f + Context.SentimentScore * 0.3f + (Context.MessageCount * 0.02f), 0.5f, 0.95f);
}

FGeneratedResponse UAvatarCommunicationManager::MakePendingResponse(const FConversationContext& Context) const
{
    FGeneratedResponse Pending;
    Pending.ResponseId = FGuid::NewGuid().ToString();
    Pending.bIsPending = true;
    Pending.Tone = Context.Source == EMessageSource::Dovecot ? TEXT("professional") : TEXT("");
    return Pending;
}

void UAvatarCommunicationManager::RecordReply(const FString& ParticipantId, const FString& Content)
{
    if (FConversationContext* Stored = ActiveConversations.Find(ParticipantId))
    {
        Stored->ReplyHistory.Add(Content);
        Stored->LastActivityAt = FDateTime::Now();
    }
}

void UAvatarCommunicationManager::MarkUnanswered(const FString& MessageId, const FString& Source, const FString& Reason)
{
    UnansweredMessageIds.AddUnique(MessageId);
    UE_LOG(LogCommunication, Warning, TEXT("%s message %s left unanswered: the model did not write a reply (%s). Nothing was sent."),
        *Source, *MessageId, *Reason);
}

void UAvatarCommunicationManager::UpdateEmotionalStateFromInteraction(const FString& MessageContent, bool bIsIncoming)
{
    if (!NeurochemicalSystem)
    {
        return;
    }

    float Sentiment = AnalyzeSentiment(MessageContent);

    if (bIsIncoming)
    {
        // Receiving positive messages boosts oxytocin and dopamine
        if (Sentiment > 0.6f)
        {
            NeurochemicalSystem->TriggerSocialBonding(0.3f);
            NeurochemicalSystem->TriggerRewardResponse(0.2f);
        }
        // Negative messages increase cortisol
        else if (Sentiment < 0.4f)
        {
            NeurochemicalSystem->TriggerStressResponse(0.2f);
        }
    }
    else
    {
        // Sending responses gives small dopamine boost
        NeurochemicalSystem->TriggerRewardResponse(0.1f);
    }
}

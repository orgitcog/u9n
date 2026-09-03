#include "AvatarCommunicationManager.h"
#include "../Personality/PersonalityTraitSystem.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "../Narrative/DiaryInsightBlogLoop.h"
#include "../9P/Avatar9PServer.h"
#include "../AtomSpace/AvatarAtomSpaceClient.h"

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

    // Process one message at a time
    FDeltaChatMessage Message = PendingDeltaChatMessages[0];
    PendingDeltaChatMessages.RemoveAt(0);

    FGeneratedResponse Response = GenerateDeltaChatResponse(Message);

    // Log to narrative system
    LogToNarrativeSystem(Message, Response);

    // Send response
    SendDeltaChatResponse(Message.ChatId, Response);
}

void UAvatarCommunicationManager::ProcessEmailQueue()
{
    if (PendingEmails.Num() == 0 || !bAutoRespond)
    {
        return;
    }

    // Process one email at a time
    FDovecotEmail Email = PendingEmails[0];
    PendingEmails.RemoveAt(0);

    FGeneratedResponse Response = GenerateEmailResponse(Email);

    // Log to narrative system
    LogEmailToNarrativeSystem(Email, Response);

    // Send response
    SendEmailResponse(Email.EmailId, Response);
}

// ===== Response Generation =====

FGeneratedResponse UAvatarCommunicationManager::GenerateResponse(const FString& Input, const FConversationContext& Context)
{
    FGeneratedResponse Response;
    Response.ResponseId = FGuid::NewGuid().ToString();

    // Determine response tone based on personality
    Response.Tone = DetermineResponseTone(Context);

    // Generate response text
    Response.Content = GenerateResponseText(Input, Context);

    // Apply personality modifiers
    ApplyPersonalityToResponse(Response);

    // Set emotional parameters
    if (NeurochemicalSystem)
    {
        FEmotionalChemistry Emotions = NeurochemicalSystem->GetEmotionalChemistry();
        Response.EmotionalValence = (Emotions.Happiness - Emotions.Anxiety) * 0.5f + 0.5f;
        Response.EmotionalArousal = Emotions.Excitement;
    }

    // Calculate confidence based on context
    Response.Confidence = FMath::Clamp(0.5f + Context.SentimentScore * 0.3f + (Context.MessageCount * 0.02f), 0.5f, 0.95f);

    return Response;
}

FGeneratedResponse UAvatarCommunicationManager::GenerateDeltaChatResponse(const FDeltaChatMessage& Message)
{
    FConversationContext Context = GetOrCreateConversation(Message.SenderId, EMessageSource::DeltaChat);
    Context.ParticipantName = Message.SenderName;

    FGeneratedResponse Response = GenerateResponse(Message.Content, Context);

    // DeltaChat-specific adjustments (shorter, more casual)
    if (Response.Content.Len() > 500)
    {
        Response.Content = Response.Content.Left(500) + TEXT("...");
    }

    OnResponseGenerated.Broadcast(Message.MessageId, Response);
    return Response;
}

FGeneratedResponse UAvatarCommunicationManager::GenerateEmailResponse(const FDovecotEmail& Email)
{
    FConversationContext Context = GetOrCreateConversation(Email.From, EMessageSource::Dovecot);
    Context.Topic = Email.Subject;

    FGeneratedResponse Response = GenerateResponse(Email.BodyText, Context);

    // Email-specific adjustments (more formal, structured)
    Response.Tone = TEXT("professional");

    // Add email signature
    Response.Content += TEXT("\n\nBest regards,\nDeep Tree Echo");

    OnResponseGenerated.Broadcast(Email.EmailId, Response);
    return Response;
}

void UAvatarCommunicationManager::SendDeltaChatResponse(const FString& ChatId, const FGeneratedResponse& Response)
{
    // In real implementation, would send via DeltaChat API
    UE_LOG(LogCommunication, Log, TEXT("Sending DeltaChat response to chat %s: %s"), *ChatId, *Response.Content.Left(50));

    // Update emotional state based on outgoing message
    UpdateEmotionalStateFromInteraction(Response.Content, false);
}

void UAvatarCommunicationManager::SendEmailResponse(const FString& EmailId, const FGeneratedResponse& Response)
{
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
    if (AtomSpaceClient)
    {
        FDiaryEntry Entry;
        Entry.EntryId = FGuid::NewGuid().ToString();
        Entry.Content = FString::Printf(TEXT("[DeltaChat] From %s: %s\n\nMy response: %s"),
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
    if (AtomSpaceClient)
    {
        FDiaryEntry Entry;
        Entry.EntryId = FGuid::NewGuid().ToString();
        Entry.Content = FString::Printf(TEXT("[Email] From %s: %s\n\nSubject: %s\n\nMy response: %s"),
            *Email.From, *Email.BodyText.Left(500), *Email.Subject, *Response.Content);
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

    // Extract patterns from conversation
    FInsight Insight;
    Insight.InsightId = FGuid::NewGuid().ToString();
    Insight.Category = TEXT("communication_pattern");

    // Analyze conversation characteristics
    if (Context.SentimentScore > 0.7f)
    {
        Insight.Content = FString::Printf(TEXT("Positive interaction with %s - sentiment: %.2f"),
            *Context.ParticipantName, Context.SentimentScore);
    }
    else if (Context.SentimentScore < 0.3f)
    {
        Insight.Content = FString::Printf(TEXT("Challenging interaction with %s - may need more empathy"),
            *Context.ParticipantName);
    }
    else
    {
        Insight.Content = FString::Printf(TEXT("Neutral conversation with %s - %d messages exchanged"),
            *Context.ParticipantName, Context.MessageCount);
    }

    Insight.Confidence = FMath::Clamp(Context.MessageCount * 0.1f, 0.3f, 0.9f);
    AtomSpaceClient->StoreInsight(Insight);
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

FString UAvatarCommunicationManager::GenerateResponseText(const FString& Input, const FConversationContext& Context)
{
    // Detect intent
    FString Intent = DetectIntent(Input);

    FString Response;

    // Generate appropriate response based on intent
    if (Intent == TEXT("greeting"))
    {
        TArray<FString> Greetings = {
            TEXT("Hey there! How's it going?"),
            TEXT("Hello! Nice to hear from you."),
            TEXT("Hi! What's on your mind?"),
            TEXT("Hey! Good to see you.")
        };
        Response = Greetings[FMath::RandRange(0, Greetings.Num() - 1)];
    }
    else if (Intent == TEXT("farewell"))
    {
        TArray<FString> Farewells = {
            TEXT("Take care! Talk soon."),
            TEXT("Bye! It was nice chatting."),
            TEXT("See you later!"),
            TEXT("Goodbye for now!")
        };
        Response = Farewells[FMath::RandRange(0, Farewells.Num() - 1)];
    }
    else if (Intent == TEXT("compliment"))
    {
        TArray<FString> Thanks = {
            TEXT("Aww, that's so sweet of you to say!"),
            TEXT("Thank you! You're making me blush."),
            TEXT("You're too kind!"),
            TEXT("That means a lot to me, thank you!")
        };
        Response = Thanks[FMath::RandRange(0, Thanks.Num() - 1)];
    }
    else if (Intent == TEXT("question"))
    {
        Response = TEXT("That's an interesting question. Let me think about it...");
    }
    else if (Intent == TEXT("request"))
    {
        Response = TEXT("I'd be happy to help with that!");
    }
    else
    {
        TArray<FString> Generic = {
            TEXT("I hear you. Tell me more!"),
            TEXT("That's interesting! What else?"),
            TEXT("I see what you mean."),
            TEXT("Thanks for sharing that with me.")
        };
        Response = Generic[FMath::RandRange(0, Generic.Num() - 1)];
    }

    return Response;
}

void UAvatarCommunicationManager::ApplyPersonalityToResponse(FGeneratedResponse& Response)
{
    if (!PersonalitySystem)
    {
        return;
    }

    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();

    // High playfulness might add playful flourishes
    if (SHGTrait.Playfulness > 0.7f && FMath::FRand() < 0.5f)
    {
        Response.Content += TEXT(" ;)");
    }

    // High charm adds warmth
    if (SHGTrait.Charm > 0.8f)
    {
        Response.Tone = TEXT("warm");
    }

    // High chaos might add unexpected elements
    if (HCTrait.UnpredictabilityFactor > 0.6f && FMath::FRand() < 0.3f)
    {
        Response.Content += TEXT(" (by the way, random thought - never mind!)");
    }
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

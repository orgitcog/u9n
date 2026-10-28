// LanguageSystem.cpp
// Implementation of Natural Language Processing and Generation

#include "LanguageSystem.h"
#include "Core/CognitiveCycleManager.h"
#include "Memory/MemorySystems.h"
#include "Emotion/EmotionalSystem.h"
#include "DTELanguageModelBridge.h"
#include "Math/UnrealMathUtility.h"

ULanguageSystem::ULanguageSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void ULanguageSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeLanguageSystem();
}

void ULanguageSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateDialogueState(DeltaTime);
}

void ULanguageSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        MemoryComponent = Owner->FindComponentByClass<UMemorySystems>();
        EmotionalComponent = Owner->FindComponentByClass<UEmotionalSystem>();
        ModelBridge = UDTELanguageModelBridge::FindOrAdd(Owner);
    }
}

void ULanguageSystem::InitializeLanguageSystem()
{
    DialogueContext.ConversationID = TEXT("");
    DialogueContext.State = EDialogueState::Idle;
    DialogueContext.TurnHistory.Empty();
}

UDTELanguageModelBridge* ULanguageSystem::GetModelBridge()
{
    if (!ModelBridge)
    {
        ModelBridge = UDTELanguageModelBridge::FindOrAdd(GetOwner());
    }
    return ModelBridge;
}

// ========================================
// PARSING
// ========================================

FParsedUtterance ULanguageSystem::ParseUtterance(const FString& Text)
{
    FParsedUtterance Parsed;
    Parsed.UtteranceID = GenerateUtteranceID();
    Parsed.RawText = Text;
    Parsed.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Identify speech act
    Parsed.SpeechAct = IdentifySpeechAct(Text);

    // Extract entities
    Parsed.Entities = ExtractEntities(Text);

    // Extract keywords
    Parsed.Keywords = ExtractKeywords(Text);

    // Analyze sentiment
    Parsed.Sentiment = AnalyzeSentiment(Text);

    // Set confidence based on parsing success
    Parsed.Confidence = 0.7f;
    if (Parsed.Keywords.Num() > 0) Parsed.Confidence += 0.1f;
    if (Parsed.Entities.Num() > 0) Parsed.Confidence += 0.1f;
    if (Parsed.SpeechAct != ESpeechAct::Assertion) Parsed.Confidence += 0.1f;

    OnUtteranceParsed.Broadcast(Parsed);

    return Parsed;
}

ESpeechAct ULanguageSystem::IdentifySpeechAct(const FString& Text) const
{
    FString LowerText = Text.ToLower();

    if (IsGreeting(LowerText))
    {
        return ESpeechAct::Greeting;
    }

    if (LowerText.Contains(TEXT("goodbye")) || LowerText.Contains(TEXT("bye")) || 
        LowerText.Contains(TEXT("see you")))
    {
        return ESpeechAct::Farewell;
    }

    if (IsQuestion(Text))
    {
        return ESpeechAct::Question;
    }

    if (IsCommand(LowerText))
    {
        return ESpeechAct::Command;
    }

    if (LowerText.Contains(TEXT("please")) || LowerText.Contains(TEXT("could you")) ||
        LowerText.Contains(TEXT("would you")))
    {
        return ESpeechAct::Request;
    }

    if (LowerText.Contains(TEXT("i promise")) || LowerText.Contains(TEXT("i will")))
    {
        return ESpeechAct::Promise;
    }

    if (LowerText.Contains(TEXT("thank")) || LowerText.Contains(TEXT("okay")) ||
        LowerText.Contains(TEXT("yes")) || LowerText.Contains(TEXT("no")))
    {
        return ESpeechAct::Acknowledgment;
    }

    return ESpeechAct::Assertion;
}

TArray<FString> ULanguageSystem::ExtractEntities(const FString& Text) const
{
    TArray<FString> Entities;

    // Simple entity extraction - words starting with capital letters
    TArray<FString> Words;
    Text.ParseIntoArray(Words, TEXT(" "), true);

    for (const FString& Word : Words)
    {
        if (Word.Len() > 1)
        {
            FString CleanWord = Word;
            CleanWord.RemoveFromEnd(TEXT("."));
            CleanWord.RemoveFromEnd(TEXT(","));
            CleanWord.RemoveFromEnd(TEXT("!"));
            CleanWord.RemoveFromEnd(TEXT("?"));

            if (CleanWord.Len() > 0 && FChar::IsUpper(CleanWord[0]))
            {
                // Check if it's not a sentence start
                if (!Entities.Contains(CleanWord))
                {
                    Entities.Add(CleanWord);
                }
            }
        }
    }

    return Entities;
}

TArray<FString> ULanguageSystem::ExtractKeywords(const FString& Text) const
{
    TArray<FString> Keywords;

    // Common stop words to filter
    TArray<FString> StopWords = {
        TEXT("the"), TEXT("a"), TEXT("an"), TEXT("is"), TEXT("are"), TEXT("was"), TEXT("were"),
        TEXT("be"), TEXT("been"), TEXT("being"), TEXT("have"), TEXT("has"), TEXT("had"),
        TEXT("do"), TEXT("does"), TEXT("did"), TEXT("will"), TEXT("would"), TEXT("could"),
        TEXT("should"), TEXT("may"), TEXT("might"), TEXT("must"), TEXT("shall"),
        TEXT("i"), TEXT("you"), TEXT("he"), TEXT("she"), TEXT("it"), TEXT("we"), TEXT("they"),
        TEXT("this"), TEXT("that"), TEXT("these"), TEXT("those"),
        TEXT("and"), TEXT("or"), TEXT("but"), TEXT("if"), TEXT("then"), TEXT("else"),
        TEXT("to"), TEXT("of"), TEXT("in"), TEXT("on"), TEXT("at"), TEXT("by"), TEXT("for"),
        TEXT("with"), TEXT("about"), TEXT("into"), TEXT("through"), TEXT("during"),
        TEXT("what"), TEXT("which"), TEXT("who"), TEXT("whom"), TEXT("whose"),
        TEXT("where"), TEXT("when"), TEXT("why"), TEXT("how")
    };

    TArray<FString> Words;
    Text.ToLower().ParseIntoArray(Words, TEXT(" "), true);

    for (FString Word : Words)
    {
        Word.RemoveFromEnd(TEXT("."));
        Word.RemoveFromEnd(TEXT(","));
        Word.RemoveFromEnd(TEXT("!"));
        Word.RemoveFromEnd(TEXT("?"));

        if (Word.Len() > 2 && !StopWords.Contains(Word) && !Keywords.Contains(Word))
        {
            Keywords.Add(Word);
        }
    }

    return Keywords;
}

float ULanguageSystem::AnalyzeSentiment(const FString& Text) const
{
    FString LowerText = Text.ToLower();
    float Sentiment = 0.0f;

    // Positive words
    TArray<FString> PositiveWords = {
        TEXT("good"), TEXT("great"), TEXT("excellent"), TEXT("wonderful"), TEXT("amazing"),
        TEXT("happy"), TEXT("joy"), TEXT("love"), TEXT("like"), TEXT("best"),
        TEXT("beautiful"), TEXT("nice"), TEXT("awesome"), TEXT("fantastic"), TEXT("perfect"),
        TEXT("thank"), TEXT("thanks"), TEXT("please"), TEXT("appreciate"), TEXT("enjoy")
    };

    // Negative words
    TArray<FString> NegativeWords = {
        TEXT("bad"), TEXT("terrible"), TEXT("awful"), TEXT("horrible"), TEXT("worst"),
        TEXT("sad"), TEXT("angry"), TEXT("hate"), TEXT("dislike"), TEXT("ugly"),
        TEXT("stupid"), TEXT("dumb"), TEXT("wrong"), TEXT("fail"), TEXT("failed"),
        TEXT("never"), TEXT("nothing"), TEXT("nobody"), TEXT("nowhere"), TEXT("problem")
    };

    for (const FString& Word : PositiveWords)
    {
        if (LowerText.Contains(Word))
        {
            Sentiment += 0.2f;
        }
    }

    for (const FString& Word : NegativeWords)
    {
        if (LowerText.Contains(Word))
        {
            Sentiment -= 0.2f;
        }
    }

    // Check for negation
    if (LowerText.Contains(TEXT("not ")) || LowerText.Contains(TEXT("n't")))
    {
        Sentiment *= -0.5f;
    }

    return FMath::Clamp(Sentiment, -1.0f, 1.0f);
}

// ========================================
// DIALOGUE MANAGEMENT
// ========================================

void ULanguageSystem::StartConversation(const FString& InitialTopic)
{
    DialogueContext.ConversationID = GenerateConversationID();
    DialogueContext.TurnHistory.Empty();
    DialogueContext.CurrentTopic = InitialTopic;
    DialogueContext.MentionedEntities.Empty();
    DialogueContext.SharedKnowledge.Empty();
    DialogueContext.ConversationStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    SetDialogueState(EDialogueState::Listening);
}

FGeneratedResponse ULanguageSystem::ProcessUtterance(const FString& Speaker, const FString& Text)
{
    SetDialogueState(EDialogueState::Processing);

    // Parse the utterance (local analysis)
    FParsedUtterance Parsed = ParseUtterance(Text);

    // Record the speaker's turn
    FDialogueTurn Turn;
    Turn.TurnID = GenerateTurnID();
    Turn.Speaker = Speaker;
    Turn.Utterance = Text;
    Turn.ParsedContent = Parsed;
    Turn.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Turn.bIsAgentTurn = false;

    AddTurnToHistory(Turn);

    // Update mentioned entities
    for (const FString& Entity : Parsed.Entities)
    {
        DialogueContext.MentionedEntities.AddUnique(Entity);
    }

    // Ask the model for the reply. The agent's turn is recorded and
    // OnResponseGenerated fires only once the model has answered.
    FGeneratedResponse Pending;
    Pending.ResponseID = DispatchReplyRequest(Parsed, GetCurrentEmotionalTone(), /*bRecordAgentTurn*/ true);
    Pending.SpeechAct = Parsed.SpeechAct;
    Pending.EmotionalTone = GetCurrentEmotionalTone();
    Pending.bIsPending = true;

    return Pending;
}

FDialogueContext ULanguageSystem::GetDialogueContext() const
{
    return DialogueContext;
}

EDialogueState ULanguageSystem::GetDialogueState() const
{
    return DialogueContext.State;
}

void ULanguageSystem::SetDialogueState(EDialogueState NewState)
{
    if (DialogueContext.State != NewState)
    {
        EDialogueState OldState = DialogueContext.State;
        DialogueContext.State = NewState;
        OnDialogueStateChanged.Broadcast(OldState, NewState);
    }
}

FString ULanguageSystem::GetCurrentTopic() const
{
    return DialogueContext.CurrentTopic;
}

void ULanguageSystem::SetCurrentTopic(const FString& Topic)
{
    if (DialogueContext.CurrentTopic != Topic)
    {
        DialogueContext.CurrentTopic = Topic;
        OnTopicChanged.Broadcast(Topic);
    }
}

void ULanguageSystem::EndConversation()
{
    SetDialogueState(EDialogueState::Idle);
    DialogueContext.ConversationID = TEXT("");
}

TArray<FDialogueTurn> ULanguageSystem::GetTurnHistory() const
{
    return DialogueContext.TurnHistory;
}

// ========================================
// RESPONSE GENERATION
// ========================================

namespace
{
    FGeneratedResponse MakePendingResponse(const FString& ResponseID, ESpeechAct Act, float EmotionalTone)
    {
        FGeneratedResponse Pending;
        Pending.ResponseID = ResponseID;
        Pending.SpeechAct = Act;
        Pending.EmotionalTone = EmotionalTone;
        Pending.bIsPending = true;
        return Pending;
    }

    FNarrativeElement MakePendingNarrative(const FString& ElementID, const FString& NarrativeType)
    {
        FNarrativeElement Pending;
        Pending.ElementID = ElementID;
        Pending.NarrativeType = NarrativeType;
        Pending.bIsPending = true;
        return Pending;
    }

    FString GreetingInstruction(const FString& Recipient)
    {
        return Recipient.IsEmpty()
            ? FString(TEXT("Greet the person who has just arrived and open the conversation in your own voice."))
            : FString::Printf(TEXT("Greet %s by name and open the conversation in your own voice."), *Recipient);
    }

    FString FarewellInstruction()
    {
        return TEXT("Say goodbye and bring the conversation to a close in your own voice, reflecting what was discussed.");
    }

    FString AcknowledgmentInstruction()
    {
        return TEXT("Briefly acknowledge what was just said, showing that you followed it, without adding a new topic.");
    }
}

FString ULanguageSystem::RequestResponse(const FParsedUtterance& Input)
{
    return DispatchReplyRequest(Input, GetCurrentEmotionalTone(), /*bRecordAgentTurn*/ false);
}

FString ULanguageSystem::RequestEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone)
{
    return DispatchReplyRequest(Input, EmotionalTone, /*bRecordAgentTurn*/ false);
}

FString ULanguageSystem::RequestClarification(const FString& UnclearAspect)
{
    SetDialogueState(EDialogueState::Clarifying);

    const FString Instruction = FString::Printf(
        TEXT("Ask one clarifying question about what the speaker meant by '%s'."), *UnclearAspect);
    const FString Situation = FString::Printf(TEXT("Unclear aspect of the last utterance: %s"), *UnclearAspect);

    return DispatchSpeechRequest(ESpeechAct::Question, Instruction, Situation,
                                 GetCurrentEmotionalTone(), 0.8f, /*bRecordAgentTurn*/ false,
                                 EDialogueState::Clarifying);
}

FString ULanguageSystem::RequestAcknowledgment()
{
    return DispatchSpeechRequest(ESpeechAct::Acknowledgment, AcknowledgmentInstruction(), FString(),
                                 GetCurrentEmotionalTone(), 0.9f, /*bRecordAgentTurn*/ false,
                                 EDialogueState::Waiting);
}

FString ULanguageSystem::RequestGreeting(const FString& RecipientName)
{
    const FString Recipient = RecipientName.IsEmpty() ? GetLastSpeaker() : RecipientName;
    return DispatchSpeechRequest(ESpeechAct::Greeting, GreetingInstruction(Recipient), FString(),
                                 GetCurrentEmotionalTone(), 0.95f, /*bRecordAgentTurn*/ false,
                                 EDialogueState::Waiting);
}

FString ULanguageSystem::RequestFarewell()
{
    return DispatchSpeechRequest(ESpeechAct::Farewell, FarewellInstruction(), FString(),
                                 GetCurrentEmotionalTone(), 0.95f, /*bRecordAgentTurn*/ false,
                                 EDialogueState::Waiting);
}

FGeneratedResponse ULanguageSystem::GenerateResponse(const FParsedUtterance& Input)
{
    const float Tone = GetCurrentEmotionalTone();
    return MakePendingResponse(RequestEmotionalResponse(Input, Tone), Input.SpeechAct, Tone);
}

FGeneratedResponse ULanguageSystem::GenerateEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone)
{
    return MakePendingResponse(RequestEmotionalResponse(Input, EmotionalTone), Input.SpeechAct, EmotionalTone);
}

FGeneratedResponse ULanguageSystem::GenerateClarification(const FString& UnclearAspect)
{
    return MakePendingResponse(RequestClarification(UnclearAspect), ESpeechAct::Question, GetCurrentEmotionalTone());
}

FGeneratedResponse ULanguageSystem::GenerateAcknowledgment()
{
    return MakePendingResponse(RequestAcknowledgment(), ESpeechAct::Acknowledgment, GetCurrentEmotionalTone());
}

FGeneratedResponse ULanguageSystem::GenerateGreeting(const FString& RecipientName)
{
    return MakePendingResponse(RequestGreeting(RecipientName), ESpeechAct::Greeting, GetCurrentEmotionalTone());
}

FGeneratedResponse ULanguageSystem::GenerateFarewell()
{
    return MakePendingResponse(RequestFarewell(), ESpeechAct::Farewell, GetCurrentEmotionalTone());
}

// ========================================
// NARRATIVE GENERATION
// ========================================

FString ULanguageSystem::RequestNarrative(const FString& Subject, const TArray<FString>& Events)
{
    FString EventList;
    for (int32 i = 0; i < Events.Num(); ++i)
    {
        EventList += FString::Printf(TEXT("%d. %s\n"), i + 1, *Events[i]);
    }
    if (EventList.IsEmpty())
    {
        EventList = TEXT("(no events recorded)\n");
    }

    const FString Instruction = FString::Printf(
        TEXT("Narrate, as a short descriptive passage, what %s experienced through the events listed, in the order given."),
        *Subject);
    const FString Situation = FString::Printf(TEXT("Subject: %s\nEvents in order:\n%s"), *Subject, *EventList);

    TArray<FString> Characters;
    Characters.Add(Subject);

    return DispatchNarrativeRequest(TEXT("Description"), Instruction, Situation, Characters, FString(), 0.0f);
}

FString ULanguageSystem::RequestSelfNarrative()
{
    const float Valence = GetCurrentEmotionalTone();
    const float Arousal = EmotionalComponent ? EmotionalComponent->GetArousal() : 0.0f;

    const FString Instruction =
        TEXT("Describe yourself in the first person: who you are, how you are feeling right now and what you are here for.");
    const FString Situation = FString::Printf(
        TEXT("Your name: %s\nCurrent emotional valence: %.2f (%s)\nCurrent arousal: %.2f\nConversation turns so far: %d"),
        *AgentName, Valence, *DescribeEmotionalTone(Valence), Arousal, DialogueContext.TurnHistory.Num());

    TArray<FString> Characters;
    Characters.Add(AgentName);

    return DispatchNarrativeRequest(TEXT("Self-Description"), Instruction, Situation, Characters, FString(), Valence);
}

FString ULanguageSystem::RequestExperienceSummary(float TimeSpan)
{
    const int32 TurnCount = DialogueContext.TurnHistory.Num();

    FString Entities;
    const int32 EntityCount = FMath::Min(5, DialogueContext.MentionedEntities.Num());
    for (int32 i = 0; i < EntityCount; ++i)
    {
        Entities += DialogueContext.MentionedEntities[i];
        if (i < EntityCount - 1)
        {
            Entities += TEXT(", ");
        }
    }

    FString RecentExchanges;
    const int32 FirstTurn = FMath::Max(0, TurnCount - RecentTurnsInPrompt);
    for (int32 i = FirstTurn; i < TurnCount; ++i)
    {
        const FDialogueTurn& Turn = DialogueContext.TurnHistory[i];
        RecentExchanges += FString::Printf(TEXT("- %s: %s\n"), *Turn.Speaker, *Turn.Utterance);
    }

    const FString Instruction =
        TEXT("Summarise, in the first person, what you have experienced over the recent period: ")
        TEXT("the conversations you took part in, the topics and people that came up, and what stood out to you.");
    const FString Situation = FString::Printf(
        TEXT("Your name: %s\nPeriod covered: the last %.0f seconds\nConversational exchanges in that period: %d\n")
        TEXT("Current topic: %s\nEntities mentioned: %s\nRecent exchanges:\n%s"),
        *AgentName, TimeSpan, TurnCount,
        DialogueContext.CurrentTopic.IsEmpty() ? TEXT("(none)") : *DialogueContext.CurrentTopic,
        Entities.IsEmpty() ? TEXT("(none)") : *Entities,
        RecentExchanges.IsEmpty() ? TEXT("(none)\n") : *RecentExchanges);

    TArray<FString> Characters;
    Characters.Add(AgentName);

    return DispatchNarrativeRequest(TEXT("Experience Summary"), Instruction, Situation, Characters, FString(),
                                    GetCurrentEmotionalTone());
}

FNarrativeElement ULanguageSystem::GenerateNarrative(const FString& Subject, const TArray<FString>& Events)
{
    FNarrativeElement Pending = MakePendingNarrative(RequestNarrative(Subject, Events), TEXT("Description"));
    Pending.Characters.Add(Subject);
    return Pending;
}

FNarrativeElement ULanguageSystem::GenerateSelfNarrative()
{
    FNarrativeElement Pending = MakePendingNarrative(RequestSelfNarrative(), TEXT("Self-Description"));
    Pending.Characters.Add(AgentName);
    return Pending;
}

FNarrativeElement ULanguageSystem::GenerateExperienceSummary(float TimeSpan)
{
    FNarrativeElement Pending = MakePendingNarrative(RequestExperienceSummary(TimeSpan), TEXT("Experience Summary"));
    Pending.Characters.Add(AgentName);
    return Pending;
}

// ========================================
// INTERNAL METHODS
// ========================================

void ULanguageSystem::UpdateDialogueState(float DeltaTime)
{
    // Timeout handling
    if (DialogueContext.State == EDialogueState::Waiting)
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (DialogueContext.TurnHistory.Num() > 0)
        {
            float LastTurnTime = DialogueContext.TurnHistory.Last().Timestamp;
            if (CurrentTime - LastTurnTime > ResponseTimeout)
            {
                SetDialogueState(EDialogueState::Listening);
            }
        }
    }
}

void ULanguageSystem::AddTurnToHistory(const FDialogueTurn& Turn)
{
    DialogueContext.TurnHistory.Add(Turn);

    // Limit history
    while (DialogueContext.TurnHistory.Num() > MaxTurnHistory)
    {
        DialogueContext.TurnHistory.RemoveAt(0);
    }
}

float ULanguageSystem::GetCurrentEmotionalTone() const
{
    return EmotionalComponent ? EmotionalComponent->GetValence() : 0.0f;
}

FString ULanguageSystem::GetLastSpeaker() const
{
    for (int32 i = DialogueContext.TurnHistory.Num() - 1; i >= 0; --i)
    {
        const FDialogueTurn& Turn = DialogueContext.TurnHistory[i];
        if (!Turn.bIsAgentTurn && !Turn.Speaker.IsEmpty())
        {
            return Turn.Speaker;
        }
    }
    return FString();
}

FString ULanguageSystem::DispatchReplyRequest(const FParsedUtterance& Input, float EmotionalTone, bool bRecordAgentTurn)
{
    const FString Recipient = GetLastSpeaker();

    ESpeechAct ResponseAct = ESpeechAct::Assertion;
    FString Instruction;
    float Confidence = 0.5f;

    switch (Input.SpeechAct)
    {
        case ESpeechAct::Greeting:
            ResponseAct = ESpeechAct::Greeting;
            Instruction = GreetingInstruction(Recipient);
            Confidence = 0.95f;
            break;

        case ESpeechAct::Farewell:
            ResponseAct = ESpeechAct::Farewell;
            Instruction = FarewellInstruction();
            Confidence = 0.95f;
            break;

        case ESpeechAct::Question:
            ResponseAct = ESpeechAct::Assertion;
            Instruction = TEXT("Answer the speaker's question directly. If you do not know, say what you do know and what you would need to find out.");
            Confidence = 0.6f;
            break;

        case ESpeechAct::Command:
        case ESpeechAct::Request:
            ResponseAct = ESpeechAct::Acknowledgment;
            Instruction = TEXT("Respond to the speaker's request: say what you will do about it, or what you cannot do and why.");
            Confidence = 0.7f;
            break;

        case ESpeechAct::Acknowledgment:
            ResponseAct = ESpeechAct::Acknowledgment;
            Instruction = AcknowledgmentInstruction();
            Confidence = 0.9f;
            break;

        default:
            ResponseAct = ESpeechAct::Assertion;
            Instruction = TEXT("Respond to what the speaker just said, engaging with its content and inviting them to continue if that fits.");
            Confidence = 0.5f;
            break;
    }

    FString Keywords;
    for (int32 i = 0; i < FMath::Min(6, Input.Keywords.Num()); ++i)
    {
        if (i > 0) { Keywords += TEXT(", "); }
        Keywords += Input.Keywords[i];
    }
    FString Entities;
    for (int32 i = 0; i < FMath::Min(6, Input.Entities.Num()); ++i)
    {
        if (i > 0) { Entities += TEXT(", "); }
        Entities += Input.Entities[i];
    }

    const FString Situation = FString::Printf(
        TEXT("Last utterance: %s\nIts speech act: %s\nIts sentiment: %.2f (%s)\nKeywords: %s\nEntities: %s"),
        *Input.RawText,
        *DescribeSpeechAct(Input.SpeechAct),
        Input.Sentiment, *DescribeEmotionalTone(Input.Sentiment),
        Keywords.IsEmpty() ? TEXT("(none)") : *Keywords,
        Entities.IsEmpty() ? TEXT("(none)") : *Entities);

    return DispatchSpeechRequest(ResponseAct, Instruction, Situation, EmotionalTone, Confidence,
                                 bRecordAgentTurn, EDialogueState::Waiting);
}

FString ULanguageSystem::DispatchSpeechRequest(ESpeechAct ResponseAct, const FString& Instruction, const FString& Situation,
                                               float EmotionalTone, float Confidence, bool bRecordAgentTurn,
                                               EDialogueState StateOnSuccess)
{
    const FString ResponseID = GenerateResponseID();

    UDTELanguageModelBridge* Bridge = GetModelBridge();
    if (!Bridge)
    {
        UE_LOG(LogDTEModel, Warning, TEXT("LanguageSystem %s: no language model bridge on owner; response %s not generated"),
               *AgentName, *ResponseID);
        if (DialogueContext.State == EDialogueState::Processing || DialogueContext.State == EDialogueState::Responding)
        {
            SetDialogueState(EDialogueState::Listening);
        }
        return ResponseID;
    }

    TArray<FDTEChatMessage> Messages;
    FString SystemPrompt = Bridge->GetSystemPrompt();
    SystemPrompt += TEXT("\n\n");
    SystemPrompt += BuildSituationPrompt(EmotionalTone, GetLastSpeaker());
    if (!Situation.IsEmpty())
    {
        SystemPrompt += TEXT("\n");
        SystemPrompt += Situation;
    }
    Messages.Add(FDTEChatMessage(TEXT("system"), SystemPrompt));

    AppendRecentTurns(Messages);

    Messages.Add(FDTEChatMessage(TEXT("user"), FString::Printf(
        TEXT("[Direction to %s] %s Reply with only the words you say, as a %s."),
        *AgentName, *Instruction, *DescribeSpeechAct(ResponseAct))));

    SetDialogueState(EDialogueState::Responding);
    ++PendingRequestCount;

    TWeakObjectPtr<ULanguageSystem> WeakThis(this);
    Bridge->RequestChat(Messages,
        [WeakThis, ResponseID, ResponseAct, EmotionalTone, Confidence, bRecordAgentTurn, StateOnSuccess]
        (bool bSuccess, const FString& TextOrError)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            ULanguageSystem* Self = WeakThis.Get();
            Self->PendingRequestCount = FMath::Max(0, Self->PendingRequestCount - 1);

            if (!bSuccess)
            {
                UE_LOG(LogDTEModel, Warning, TEXT("LanguageSystem %s: model did not answer for response %s (%s); nothing said"),
                       *Self->AgentName, *ResponseID, *TextOrError);
                if (Self->PendingRequestCount == 0 && Self->DialogueContext.State == EDialogueState::Responding)
                {
                    Self->SetDialogueState(EDialogueState::Listening);
                }
                return;
            }

            FGeneratedResponse Response;
            Response.ResponseID = ResponseID;
            Response.Text = TextOrError.TrimStartAndEnd();
            Response.SpeechAct = ResponseAct;
            Response.Confidence = Confidence;
            Response.EmotionalTone = EmotionalTone;
            Response.bIsPending = false;

            if (bRecordAgentTurn)
            {
                FDialogueTurn AgentTurn;
                AgentTurn.TurnID = Self->GenerateTurnID();
                AgentTurn.Speaker = Self->AgentName;
                AgentTurn.Utterance = Response.Text;
                AgentTurn.ParsedContent.RawText = Response.Text;
                AgentTurn.ParsedContent.SpeechAct = ResponseAct;
                AgentTurn.Timestamp = Self->GetWorld() ? Self->GetWorld()->GetTimeSeconds() : 0.0f;
                AgentTurn.bIsAgentTurn = true;
                Self->AddTurnToHistory(AgentTurn);
            }

            if (Self->PendingRequestCount == 0)
            {
                Self->SetDialogueState(StateOnSuccess);
            }

            Self->OnResponseGenerated.Broadcast(Response);
        });

    return ResponseID;
}

FString ULanguageSystem::DispatchNarrativeRequest(const FString& NarrativeType, const FString& Instruction, const FString& Situation,
                                                  const TArray<FString>& Characters, const FString& Setting, float EmotionalArc)
{
    const FString ElementID = GenerateNarrativeID();

    UDTELanguageModelBridge* Bridge = GetModelBridge();
    if (!Bridge)
    {
        UE_LOG(LogDTEModel, Warning, TEXT("LanguageSystem %s: no language model bridge on owner; narrative %s not generated"),
               *AgentName, *ElementID);
        return ElementID;
    }

    FString Context = BuildSituationPrompt(GetCurrentEmotionalTone(), FString());
    Context += TEXT("\nNarrative type: ");
    Context += NarrativeType;
    if (!Setting.IsEmpty())
    {
        Context += TEXT("\nSetting: ");
        Context += Setting;
    }
    if (!Situation.IsEmpty())
    {
        Context += TEXT("\n");
        Context += Situation;
    }

    ++PendingRequestCount;

    TWeakObjectPtr<ULanguageSystem> WeakThis(this);
    Bridge->RequestText(Instruction, Context,
        [WeakThis, ElementID, NarrativeType, Characters, Setting, EmotionalArc](bool bSuccess, const FString& TextOrError)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            ULanguageSystem* Self = WeakThis.Get();
            Self->PendingRequestCount = FMath::Max(0, Self->PendingRequestCount - 1);

            if (!bSuccess)
            {
                UE_LOG(LogDTEModel, Warning, TEXT("LanguageSystem %s: model did not answer for narrative %s (%s); nothing written"),
                       *Self->AgentName, *ElementID, *TextOrError);
                return;
            }

            FNarrativeElement Narrative;
            Narrative.ElementID = ElementID;
            Narrative.Content = TextOrError.TrimStartAndEnd();
            Narrative.NarrativeType = NarrativeType;
            Narrative.Characters = Characters;
            Narrative.Setting = Setting;
            Narrative.EmotionalArc = EmotionalArc;
            Narrative.bIsPending = false;

            Self->OnNarrativeGenerated.Broadcast(Narrative);
        });

    return ElementID;
}

FString ULanguageSystem::BuildSituationPrompt(float EmotionalTone, const FString& Recipient) const
{
    FString Prompt = FString::Printf(TEXT("You are speaking as %s."), *AgentName);

    if (!Recipient.IsEmpty())
    {
        Prompt += FString::Printf(TEXT("\nYou are talking with %s."), *Recipient);
    }
    if (!DialogueContext.CurrentTopic.IsEmpty())
    {
        Prompt += FString::Printf(TEXT("\nCurrent topic: %s"), *DialogueContext.CurrentTopic);
    }
    if (DialogueContext.MentionedEntities.Num() > 0)
    {
        FString Entities;
        const int32 Count = FMath::Min(8, DialogueContext.MentionedEntities.Num());
        for (int32 i = 0; i < Count; ++i)
        {
            if (i > 0) { Entities += TEXT(", "); }
            Entities += DialogueContext.MentionedEntities[i];
        }
        Prompt += FString::Printf(TEXT("\nPeople and things mentioned so far: %s"), *Entities);
    }

    Prompt += FString::Printf(TEXT("\nYour emotional tone right now: %s (valence %.2f). Let it colour how you speak, not what you say."),
                              *DescribeEmotionalTone(EmotionalTone), EmotionalTone);
    Prompt += FString::Printf(TEXT("\nRegister: %s."), *DescribeFormality(DefaultFormality));
    Prompt += TEXT("\nSpeak in the first person as yourself, in one to three sentences unless the direction asks for more.");

    return Prompt;
}

void ULanguageSystem::AppendRecentTurns(TArray<FDTEChatMessage>& Messages) const
{
    const int32 TurnCount = DialogueContext.TurnHistory.Num();
    const int32 FirstTurn = FMath::Max(0, TurnCount - RecentTurnsInPrompt);

    for (int32 i = FirstTurn; i < TurnCount; ++i)
    {
        const FDialogueTurn& Turn = DialogueContext.TurnHistory[i];
        if (Turn.Utterance.IsEmpty())
        {
            continue;
        }
        if (Turn.bIsAgentTurn)
        {
            Messages.Add(FDTEChatMessage(TEXT("assistant"), Turn.Utterance));
        }
        else
        {
            const FString Content = Turn.Speaker.IsEmpty()
                ? Turn.Utterance
                : FString::Printf(TEXT("%s: %s"), *Turn.Speaker, *Turn.Utterance);
            Messages.Add(FDTEChatMessage(TEXT("user"), Content));
        }
    }
}

FString ULanguageSystem::DescribeEmotionalTone(float EmotionalTone) const
{
    if (EmotionalTone > 0.6f)  return TEXT("very positive, warm and energised");
    if (EmotionalTone > 0.2f)  return TEXT("mildly positive and engaged");
    if (EmotionalTone < -0.6f) return TEXT("markedly negative, heavy or troubled");
    if (EmotionalTone < -0.2f) return TEXT("mildly negative or subdued");
    return TEXT("neutral, calm and receptive");
}

FString ULanguageSystem::DescribeFormality(float Formality) const
{
    if (Formality > 0.75f) return TEXT("formal and precise");
    if (Formality > 0.4f)  return TEXT("conversational, neither stiff nor slangy");
    return TEXT("casual and relaxed");
}

FString ULanguageSystem::DescribeSpeechAct(ESpeechAct Act) const
{
    switch (Act)
    {
        case ESpeechAct::Assertion:      return TEXT("statement");
        case ESpeechAct::Question:       return TEXT("question");
        case ESpeechAct::Command:        return TEXT("command");
        case ESpeechAct::Request:        return TEXT("request");
        case ESpeechAct::Promise:        return TEXT("promise");
        case ESpeechAct::Greeting:       return TEXT("greeting");
        case ESpeechAct::Farewell:       return TEXT("farewell");
        case ESpeechAct::Acknowledgment: return TEXT("acknowledgment");
        case ESpeechAct::Expression:     return TEXT("expression of feeling");
        case ESpeechAct::Declaration:    return TEXT("declaration");
        default:                         return TEXT("statement");
    }
}

bool ULanguageSystem::IsQuestion(const FString& Text) const
{
    if (Text.EndsWith(TEXT("?")))
    {
        return true;
    }

    FString LowerText = Text.ToLower();
    return LowerText.StartsWith(TEXT("what")) || LowerText.StartsWith(TEXT("who")) ||
           LowerText.StartsWith(TEXT("where")) || LowerText.StartsWith(TEXT("when")) ||
           LowerText.StartsWith(TEXT("why")) || LowerText.StartsWith(TEXT("how")) ||
           LowerText.StartsWith(TEXT("is ")) || LowerText.StartsWith(TEXT("are ")) ||
           LowerText.StartsWith(TEXT("do ")) || LowerText.StartsWith(TEXT("does ")) ||
           LowerText.StartsWith(TEXT("can ")) || LowerText.StartsWith(TEXT("could "));
}

bool ULanguageSystem::IsCommand(const FString& Text) const
{
    FString LowerText = Text.ToLower();
    return LowerText.StartsWith(TEXT("do ")) || LowerText.StartsWith(TEXT("make ")) ||
           LowerText.StartsWith(TEXT("create ")) || LowerText.StartsWith(TEXT("show ")) ||
           LowerText.StartsWith(TEXT("tell ")) || LowerText.StartsWith(TEXT("give ")) ||
           LowerText.StartsWith(TEXT("find ")) || LowerText.StartsWith(TEXT("get ")) ||
           LowerText.StartsWith(TEXT("go ")) || LowerText.StartsWith(TEXT("stop "));
}

bool ULanguageSystem::IsGreeting(const FString& Text) const
{
    FString LowerText = Text.ToLower();
    return LowerText.Contains(TEXT("hello")) || LowerText.Contains(TEXT("hi ")) ||
           LowerText.StartsWith(TEXT("hi")) || LowerText.Contains(TEXT("hey")) ||
           LowerText.Contains(TEXT("greetings")) || LowerText.Contains(TEXT("good morning")) ||
           LowerText.Contains(TEXT("good afternoon")) || LowerText.Contains(TEXT("good evening"));
}

FString ULanguageSystem::GenerateUtteranceID()
{
    return FString::Printf(TEXT("UTT_%d_%d"), ++UtteranceIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateTurnID()
{
    return FString::Printf(TEXT("TURN_%d_%d"), ++TurnIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateResponseID()
{
    return FString::Printf(TEXT("RESP_%d_%d"), ++ResponseIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateNarrativeID()
{
    return FString::Printf(TEXT("NAR_%d_%d"), ++NarrativeIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateConversationID()
{
    return FString::Printf(TEXT("CONV_%d_%d"), ++ConversationIDCounter, FMath::RandRange(1000, 9999));
}

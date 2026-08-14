// LanguageSystem.cpp
// Implementation of Natural Language Processing and Generation

#include "LanguageSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Memory/MemorySystems.h"
#include "../Emotion/EmotionalSystem.h"
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
    }
}

void ULanguageSystem::InitializeLanguageSystem()
{
    DialogueContext.ConversationID = TEXT("");
    DialogueContext.State = EDialogueState::Idle;
    DialogueContext.TurnHistory.Empty();

    InitializeResponseTemplates();
}

void ULanguageSystem::InitializeResponseTemplates()
{
    // Greeting templates
    FResponseTemplate Greeting1;
    Greeting1.TemplateID = TEXT("GREET_1");
    Greeting1.ResponseType = ESpeechAct::Greeting;
    Greeting1.TemplateText = TEXT("Hello! How can I help you today?");
    Greeting1.EmotionalTone = 0.3f;
    Greeting1.Formality = 0.5f;
    ResponseTemplates.Add(Greeting1);

    // Acknowledgment templates
    FResponseTemplate Ack1;
    Ack1.TemplateID = TEXT("ACK_1");
    Ack1.ResponseType = ESpeechAct::Acknowledgment;
    Ack1.TemplateText = TEXT("I understand.");
    Ack1.EmotionalTone = 0.0f;
    Ack1.Formality = 0.5f;
    ResponseTemplates.Add(Ack1);

    FResponseTemplate Ack2;
    Ack2.TemplateID = TEXT("ACK_2");
    Ack2.ResponseType = ESpeechAct::Acknowledgment;
    Ack2.TemplateText = TEXT("Yes, I see what you mean.");
    Ack2.EmotionalTone = 0.1f;
    Ack2.Formality = 0.4f;
    ResponseTemplates.Add(Ack2);

    // Question response templates
    FResponseTemplate QResp1;
    QResp1.TemplateID = TEXT("QRESP_1");
    QResp1.ResponseType = ESpeechAct::Assertion;
    QResp1.TemplateText = TEXT("Based on my understanding, {answer}.");
    QResp1.RequiredSlots.Add(TEXT("answer"));
    QResp1.EmotionalTone = 0.0f;
    QResp1.Formality = 0.6f;
    ResponseTemplates.Add(QResp1);

    // Clarification templates
    FResponseTemplate Clarify1;
    Clarify1.TemplateID = TEXT("CLARIFY_1");
    Clarify1.ResponseType = ESpeechAct::Question;
    Clarify1.TemplateText = TEXT("Could you please clarify what you mean by {unclear}?");
    Clarify1.RequiredSlots.Add(TEXT("unclear"));
    Clarify1.EmotionalTone = 0.0f;
    Clarify1.Formality = 0.6f;
    ResponseTemplates.Add(Clarify1);

    // Farewell templates
    FResponseTemplate Farewell1;
    Farewell1.TemplateID = TEXT("FAREWELL_1");
    Farewell1.ResponseType = ESpeechAct::Farewell;
    Farewell1.TemplateText = TEXT("Goodbye! It was nice talking with you.");
    Farewell1.EmotionalTone = 0.2f;
    Farewell1.Formality = 0.5f;
    ResponseTemplates.Add(Farewell1);
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

    // Parse the utterance
    FParsedUtterance Parsed = ParseUtterance(Text);

    // Create dialogue turn
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

    // Generate response
    FGeneratedResponse Response = GenerateResponse(Parsed);

    // Create agent turn
    FDialogueTurn AgentTurn;
    AgentTurn.TurnID = GenerateTurnID();
    AgentTurn.Speaker = AgentName;
    AgentTurn.Utterance = Response.Text;
    AgentTurn.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    AgentTurn.bIsAgentTurn = true;

    AddTurnToHistory(AgentTurn);

    SetDialogueState(EDialogueState::Waiting);

    return Response;
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

FGeneratedResponse ULanguageSystem::GenerateResponse(const FParsedUtterance& Input)
{
    float EmotionalTone = 0.0f;
    if (EmotionalComponent)
    {
        EmotionalTone = EmotionalComponent->GetValence();
    }

    return GenerateEmotionalResponse(Input, EmotionalTone);
}

FGeneratedResponse ULanguageSystem::GenerateEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone)
{
    FGeneratedResponse Response;
    Response.ResponseID = GenerateResponseID();
    Response.EmotionalTone = EmotionalTone;

    // Select response based on speech act
    switch (Input.SpeechAct)
    {
        case ESpeechAct::Greeting:
            Response = GenerateGreeting(TEXT(""));
            break;

        case ESpeechAct::Farewell:
            Response = GenerateFarewell();
            break;

        case ESpeechAct::Question:
        {
            Response.SpeechAct = ESpeechAct::Assertion;
            FString Answer = TEXT("I'm processing your question about ");
            if (Input.Keywords.Num() > 0)
            {
                Answer += Input.Keywords[0];
            }
            else
            {
                Answer += TEXT("that topic");
            }
            Response.Text = Answer + TEXT(".");
            Response.Confidence = 0.6f;
            break;
        }

        case ESpeechAct::Command:
        case ESpeechAct::Request:
        {
            Response.SpeechAct = ESpeechAct::Acknowledgment;
            Response.Text = TEXT("I'll do my best to help with that.");
            Response.Confidence = 0.7f;
            break;
        }

        case ESpeechAct::Acknowledgment:
        {
            Response = GenerateAcknowledgment();
            break;
        }

        default:
        {
            Response.SpeechAct = ESpeechAct::Assertion;
            if (Input.Sentiment > 0.3f)
            {
                Response.Text = TEXT("That sounds positive! I appreciate you sharing that.");
            }
            else if (Input.Sentiment < -0.3f)
            {
                Response.Text = TEXT("I understand that might be difficult. How can I help?");
            }
            else
            {
                Response.Text = TEXT("I see. Please tell me more about that.");
            }
            Response.Confidence = 0.5f;
            break;
        }
    }

    // Apply emotional modulation
    Response.Text = ApplyEmotionalModulation(Response.Text, EmotionalTone);

    OnResponseGenerated.Broadcast(Response);

    return Response;
}

FGeneratedResponse ULanguageSystem::GenerateClarification(const FString& UnclearAspect)
{
    FGeneratedResponse Response;
    Response.ResponseID = GenerateResponseID();
    Response.SpeechAct = ESpeechAct::Question;
    Response.Text = FString::Printf(TEXT("Could you please clarify what you mean by '%s'?"), *UnclearAspect);
    Response.Confidence = 0.8f;
    Response.EmotionalTone = 0.0f;

    SetDialogueState(EDialogueState::Clarifying);

    OnResponseGenerated.Broadcast(Response);

    return Response;
}

FGeneratedResponse ULanguageSystem::GenerateAcknowledgment()
{
    FGeneratedResponse Response;
    Response.ResponseID = GenerateResponseID();
    Response.SpeechAct = ESpeechAct::Acknowledgment;

    TArray<FString> Acknowledgments = {
        TEXT("I understand."),
        TEXT("Yes, I see."),
        TEXT("Okay, got it."),
        TEXT("I hear you."),
        TEXT("That makes sense.")
    };

    int32 Index = FMath::RandRange(0, Acknowledgments.Num() - 1);
    Response.Text = Acknowledgments[Index];
    Response.Confidence = 0.9f;
    Response.EmotionalTone = 0.1f;

    OnResponseGenerated.Broadcast(Response);

    return Response;
}

FGeneratedResponse ULanguageSystem::GenerateGreeting(const FString& RecipientName)
{
    FGeneratedResponse Response;
    Response.ResponseID = GenerateResponseID();
    Response.SpeechAct = ESpeechAct::Greeting;

    TArray<FString> Greetings = {
        TEXT("Hello! How can I help you today?"),
        TEXT("Hi there! What would you like to discuss?"),
        TEXT("Greetings! I'm here to assist you."),
        TEXT("Hello! It's nice to meet you.")
    };

    int32 Index = FMath::RandRange(0, Greetings.Num() - 1);
    Response.Text = Greetings[Index];

    if (!RecipientName.IsEmpty())
    {
        Response.Text = FString::Printf(TEXT("Hello, %s! How can I help you today?"), *RecipientName);
    }

    Response.Confidence = 0.95f;
    Response.EmotionalTone = 0.3f;

    OnResponseGenerated.Broadcast(Response);

    return Response;
}

FGeneratedResponse ULanguageSystem::GenerateFarewell()
{
    FGeneratedResponse Response;
    Response.ResponseID = GenerateResponseID();
    Response.SpeechAct = ESpeechAct::Farewell;

    TArray<FString> Farewells = {
        TEXT("Goodbye! It was nice talking with you."),
        TEXT("Take care! Feel free to return anytime."),
        TEXT("Farewell! I hope our conversation was helpful."),
        TEXT("Goodbye! Have a great day!")
    };

    int32 Index = FMath::RandRange(0, Farewells.Num() - 1);
    Response.Text = Farewells[Index];
    Response.Confidence = 0.95f;
    Response.EmotionalTone = 0.2f;

    OnResponseGenerated.Broadcast(Response);

    return Response;
}

// ========================================
// NARRATIVE GENERATION
// ========================================

FNarrativeElement ULanguageSystem::GenerateNarrative(const FString& Subject, const TArray<FString>& Events)
{
    FNarrativeElement Narrative;
    Narrative.ElementID = GenerateNarrativeID();
    Narrative.NarrativeType = TEXT("Description");
    Narrative.Characters.Add(Subject);

    FString Content = FString::Printf(TEXT("%s experienced the following: "), *Subject);
    for (int32 i = 0; i < Events.Num(); ++i)
    {
        Content += Events[i];
        if (i < Events.Num() - 1)
        {
            Content += TEXT(", then ");
        }
    }
    Content += TEXT(".");

    Narrative.Content = Content;
    Narrative.EmotionalArc = 0.0f;

    OnNarrativeGenerated.Broadcast(Narrative);

    return Narrative;
}

FNarrativeElement ULanguageSystem::GenerateSelfNarrative()
{
    FNarrativeElement Narrative;
    Narrative.ElementID = GenerateNarrativeID();
    Narrative.NarrativeType = TEXT("Self-Description");
    Narrative.Characters.Add(AgentName);

    FString Content = FString::Printf(TEXT("I am %s, a cognitive agent designed for understanding and interaction. "), *AgentName);

    if (EmotionalComponent)
    {
        float Valence = EmotionalComponent->GetValence();
        if (Valence > 0.3f)
        {
            Content += TEXT("Currently, I am feeling positive and engaged. ");
        }
        else if (Valence < -0.3f)
        {
            Content += TEXT("Currently, I am processing some challenging information. ");
        }
        else
        {
            Content += TEXT("Currently, I am in a calm and receptive state. ");
        }
    }

    Content += TEXT("I am here to assist and learn through our interactions.");

    Narrative.Content = Content;
    Narrative.EmotionalArc = EmotionalComponent ? EmotionalComponent->GetValence() : 0.0f;

    OnNarrativeGenerated.Broadcast(Narrative);

    return Narrative;
}

FNarrativeElement ULanguageSystem::GenerateExperienceSummary(float TimeSpan)
{
    FNarrativeElement Narrative;
    Narrative.ElementID = GenerateNarrativeID();
    Narrative.NarrativeType = TEXT("Experience Summary");
    Narrative.Characters.Add(AgentName);

    FString Content = TEXT("Over the recent period, ");

    int32 TurnCount = DialogueContext.TurnHistory.Num();
    if (TurnCount > 0)
    {
        Content += FString::Printf(TEXT("I engaged in %d conversational exchanges. "), TurnCount);
    }
    else
    {
        Content += TEXT("I have been observing and processing my environment. ");
    }

    if (DialogueContext.MentionedEntities.Num() > 0)
    {
        Content += TEXT("Topics discussed included: ");
        for (int32 i = 0; i < FMath::Min(3, DialogueContext.MentionedEntities.Num()); ++i)
        {
            Content += DialogueContext.MentionedEntities[i];
            if (i < FMath::Min(3, DialogueContext.MentionedEntities.Num()) - 1)
            {
                Content += TEXT(", ");
            }
        }
        Content += TEXT(". ");
    }

    Narrative.Content = Content;

    OnNarrativeGenerated.Broadcast(Narrative);

    return Narrative;
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

FString ULanguageSystem::SelectResponseTemplate(ESpeechAct InputAct, float EmotionalTone) const
{
    for (const FResponseTemplate& Template : ResponseTemplates)
    {
        if (Template.ResponseType == InputAct && 
            FMath::Abs(Template.EmotionalTone - EmotionalTone) < 0.5f)
        {
            return Template.TemplateText;
        }
    }
    return TEXT("");
}

FString ULanguageSystem::FillTemplateSlots(const FString& Template, const FParsedUtterance& Input) const
{
    FString Filled = Template;

    if (Input.Keywords.Num() > 0)
    {
        Filled = Filled.Replace(TEXT("{topic}"), *Input.Keywords[0]);
        Filled = Filled.Replace(TEXT("{answer}"), *Input.Keywords[0]);
    }

    if (Input.Entities.Num() > 0)
    {
        Filled = Filled.Replace(TEXT("{entity}"), *Input.Entities[0]);
    }

    return Filled;
}

FString ULanguageSystem::ApplyEmotionalModulation(const FString& Text, float EmotionalTone) const
{
    FString Modulated = Text;

    if (EmotionalTone > 0.5f)
    {
        // Add enthusiasm
        if (!Modulated.EndsWith(TEXT("!")))
        {
            Modulated.RemoveFromEnd(TEXT("."));
            Modulated += TEXT("!");
        }
    }
    else if (EmotionalTone < -0.5f)
    {
        // Add empathy markers
        Modulated = TEXT("I understand... ") + Modulated;
    }

    return Modulated;
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

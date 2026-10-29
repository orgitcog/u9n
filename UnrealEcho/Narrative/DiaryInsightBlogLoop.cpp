//=============================================================================
// DiaryInsightBlogLoop.cpp
//
// Implementation of the Diary-Insight-Blog narrative loop for Deep Tree Echo.
// The loop decides when the avatar writes; the language model decides what it
// writes. Diary entries, insights and blog posts are requested from
// UDTELanguageModelBridge with the measured state as context, and are stored,
// broadcast and published only when the model answered.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "DiaryInsightBlogLoop.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "DTELanguageModelBridge.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
    constexpr int32 MinDiaryEntriesForInsight = 5;
    constexpr int32 MinInsightsForBlogPost = 3;
    constexpr int32 MaxObservationsBuffered = 100;
    constexpr int32 DiaryEntriesPerInsight = 10;
    constexpr int32 InsightsPerBlogPost = 5;
}

UDiaryInsightBlogLoop::UDiaryInsightBlogLoop()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f; // Update once per second

    // Configuration defaults live on FNarrativeLoopConfig; restated here for clarity
    Config.DiaryUpdateInterval = 60.0f;        // 1 minute
    Config.InsightGenerationInterval = 300.0f; // 5 minutes
    Config.BlogPublishInterval = 900.0f;       // 15 minutes
    Config.MaxDiaryEntries = 1000;
    Config.MaxInsights = 500;
    Config.MaxBlogPosts = 100;
    Config.InsightThreshold = 0.7f;
    Config.bAutoPublish = true;

    State = FNarrativeLoopState();
}

void UDiaryInsightBlogLoop::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Initializing narrative system"));

    AActor* Owner = GetOwner();
    if (Owner)
    {
        PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
        NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
        ModelBridge = UDTELanguageModelBridge::FindOrAdd(Owner);
    }

    if (!PersonalitySystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: PersonalityTraitSystem not found"));
    }

    if (!NeurochemicalSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: NeurochemicalSimulationComponent not found"));
    }

    if (!ModelBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: no language model bridge on owner; diary, insights and blog posts will not be written"));
    }

    // Start the narrative loop
    StartNarrativeLoop();
}

void UDiaryInsightBlogLoop::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update timers
    State.TimeSinceLastDiaryEntry += DeltaTime;
    State.TimeSinceLastInsight += DeltaTime;
    State.TimeSinceLastBlogPost += DeltaTime;

    // Update current phase based on timers
    UpdateNarrativePhase();

    // Process current phase
    ProcessCurrentPhase(DeltaTime);
}

void UDiaryInsightBlogLoop::StartNarrativeLoop()
{
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Starting narrative loop"));

    State.CurrentPhase = ENarrativePhase::Observation;
    State.bIsActive = true;

    OnNarrativeLoopStarted.Broadcast();
}

void UDiaryInsightBlogLoop::StopNarrativeLoop()
{
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Stopping narrative loop"));

    State.bIsActive = false;

    OnNarrativeLoopStopped.Broadcast();
}

void UDiaryInsightBlogLoop::UpdateNarrativePhase()
{
    if (!State.bIsActive)
    {
        return;
    }

    const ENarrativePhase PreviousPhase = State.CurrentPhase;

    if (State.TimeSinceLastDiaryEntry >= Config.DiaryUpdateInterval)
    {
        State.CurrentPhase = ENarrativePhase::DiaryWriting;
    }
    else if (State.TimeSinceLastInsight >= Config.InsightGenerationInterval)
    {
        State.CurrentPhase = ENarrativePhase::InsightGeneration;
    }
    else if (State.TimeSinceLastBlogPost >= Config.BlogPublishInterval && Config.bAutoPublish)
    {
        State.CurrentPhase = ENarrativePhase::BlogPublishing;
    }
    else
    {
        State.CurrentPhase = ENarrativePhase::Observation;
    }

    if (State.CurrentPhase != PreviousPhase)
    {
        OnNarrativePhaseChanged.Broadcast(PreviousPhase, State.CurrentPhase);
    }
}

void UDiaryInsightBlogLoop::ProcessCurrentPhase(float DeltaTime)
{
    if (!State.bIsActive)
    {
        return;
    }

    switch (State.CurrentPhase)
    {
        case ENarrativePhase::Observation:
            ProcessObservation(DeltaTime);
            break;

        case ENarrativePhase::DiaryWriting:
            ProcessDiaryWriting(DeltaTime);
            break;

        case ENarrativePhase::InsightGeneration:
            ProcessInsightGeneration(DeltaTime);
            break;

        case ENarrativePhase::BlogPublishing:
            ProcessBlogPublishing(DeltaTime);
            break;
    }
}

//=============================================================================
// Observation
//=============================================================================

void UDiaryInsightBlogLoop::ProcessObservation(float DeltaTime)
{
    FNarrativeObservation Observation;
    Observation.Timestamp = CurrentWorldTime();
    Observation.EmotionalState = SampleAffect();
    Observation.PersonalityExpression = SamplePersonality();

    ObservationBuffer.Add(Observation);

    if (ObservationBuffer.Num() > MaxObservationsBuffered)
    {
        ObservationBuffer.RemoveAt(0);
    }
}

FNarrativeAffect UDiaryInsightBlogLoop::SampleAffect() const
{
    FNarrativeAffect Affect;
    if (!NeurochemicalSystem)
    {
        return Affect;
    }

    const FEmotionalChemistry Chemistry = NeurochemicalSystem->GetEmotionalChemistry();
    Affect.Valence = FMath::Clamp(Chemistry.Happiness + 0.5f * Chemistry.Affection - Chemistry.Anxiety, -1.0f, 1.0f);
    Affect.Arousal = FMath::Clamp(0.5f * Chemistry.Excitement + 0.3f * Chemistry.Anxiety + 0.2f * (1.0f - Chemistry.Calmness), 0.0f, 1.0f);
    Affect.Dominance = FMath::Clamp(0.5f * Chemistry.Focus + 0.5f * (1.0f - Chemistry.Anxiety), 0.0f, 1.0f);
    return Affect;
}

FNarrativePersonalityExpression UDiaryInsightBlogLoop::SamplePersonality() const
{
    FNarrativePersonalityExpression Expression;
    if (!PersonalitySystem)
    {
        return Expression;
    }

    Expression.Confidence = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident);
    Expression.Playfulness = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Playful);
    Expression.Unpredictability = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    return Expression;
}

//=============================================================================
// Diary
//=============================================================================

void UDiaryInsightBlogLoop::ProcessDiaryWriting(float DeltaTime)
{
    if (ObservationBuffer.Num() == 0 || State.bDiaryEntryPending)
    {
        return;
    }

    RequestDiaryEntry();
}

bool UDiaryInsightBlogLoop::RequestDiaryEntry()
{
    if (State.bDiaryEntryPending)
    {
        return false;
    }

    if (ObservationBuffer.Num() == 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("DiaryInsightBlogLoop: no observations to write about"));
        return false;
    }

    if (!ModelBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: diary entry skipped, no language model bridge"));
        State.TimeSinceLastDiaryEntry = 0.0f;
        return false;
    }

    // Consume the observations this entry is about. The facts go into the prompt;
    // the words come back from the model.
    const TArray<FNarrativeObservation> Observations = MoveTemp(ObservationBuffer);
    ObservationBuffer.Reset();

    const FNarrativeAffect AverageEmotion = CalculateAverageEmotionalState(Observations);
    const FNarrativePersonalityExpression AveragePersonality = CalculateAveragePersonality(Observations);

    FNarrativeDiaryEntry PendingEntry;
    PendingEntry.EntryID = FGuid::NewGuid();
    PendingEntry.Timestamp = CurrentWorldTime();
    PendingEntry.EmotionalTone = AverageEmotion;
    PendingEntry.Tags = GenerateTags(AverageEmotion, AveragePersonality);

    const float SpanSeconds = Observations.Num() > 1
        ? FMath::Max(0.0f, Observations.Last().Timestamp - Observations[0].Timestamp)
        : 0.0f;

    FString Context;
    Context += FString::Printf(TEXT("Observations taken: %d over %.0f seconds\n"), Observations.Num(), SpanSeconds);
    Context += FString::Printf(TEXT("Average emotional state: %s\n"), *FormatAffect(AverageEmotion));
    Context += FString::Printf(TEXT("Emotional state at the start: %s\n"), *FormatAffect(Observations[0].EmotionalState));
    Context += FString::Printf(TEXT("Emotional state at the end: %s\n"), *FormatAffect(Observations.Last().EmotionalState));
    Context += FString::Printf(TEXT("Average personality expression: %s\n"), *FormatPersonality(AveragePersonality));

    TArray<FString> Notes;
    for (const FNarrativeObservation& Observation : Observations)
    {
        if (!Observation.Context.IsEmpty())
        {
            Notes.AddUnique(Observation.Context);
        }
    }
    if (Notes.Num() > 0)
    {
        Context += TEXT("Notes from other systems about what happened:\n");
        for (const FString& Note : Notes)
        {
            Context += FString::Printf(TEXT("- %s\n"), *Note);
        }
    }

    if (DiaryEntries.Num() > 0)
    {
        Context += FString::Printf(TEXT("Previous diary entry, for continuity (do not repeat it):\n%s\n"), *DiaryEntries.Last().Content);
    }

    const FString Instruction =
        TEXT("Write today's diary entry in your own voice: a first-person reflection of three to six sentences on how this stretch of time felt, ")
        TEXT("what you noticed about your own mood and manner, and anything the notes mention. Draw only on the readings and notes below; ")
        TEXT("do not quote the numbers. Return only the entry text.");

    State.bDiaryEntryPending = true;
    State.TimeSinceLastDiaryEntry = 0.0f;

    TWeakObjectPtr<UDiaryInsightBlogLoop> WeakThis(this);
    ModelBridge->RequestText(Instruction, Context, [WeakThis, PendingEntry](bool bSuccess, const FString& TextOrError)
    {
        if (!WeakThis.IsValid())
        {
            return;
        }

        UDiaryInsightBlogLoop* Self = WeakThis.Get();
        Self->State.bDiaryEntryPending = false;

        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: diary entry not written, model unavailable: %s"), *TextOrError);
            return;
        }

        FNarrativeDiaryEntry NewEntry = PendingEntry;
        NewEntry.Content = TextOrError.TrimStartAndEnd();

        Self->DiaryEntries.Add(NewEntry);
        Self->State.TotalDiaryEntries++;

        if (Self->DiaryEntries.Num() > Self->Config.MaxDiaryEntries)
        {
            Self->DiaryEntries.RemoveAt(0);
        }

        Self->OnDiaryEntryCreated.Broadcast(NewEntry);

        UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Created diary entry #%d"), Self->State.TotalDiaryEntries);
    });

    return true;
}

//=============================================================================
// Insight
//=============================================================================

void UDiaryInsightBlogLoop::ProcessInsightGeneration(float DeltaTime)
{
    if (State.bInsightPending)
    {
        return;
    }

    if (DiaryEntries.Num() < MinDiaryEntriesForInsight)
    {
        // Not enough material yet; try again next interval
        State.TimeSinceLastInsight = 0.0f;
        return;
    }

    RequestInsight();
    State.TimeSinceLastInsight = 0.0f;
}

bool UDiaryInsightBlogLoop::RequestInsight()
{
    if (State.bInsightPending)
    {
        return false;
    }

    if (DiaryEntries.Num() < MinDiaryEntriesForInsight)
    {
        UE_LOG(LogTemp, Verbose, TEXT("DiaryInsightBlogLoop: %d diary entries, need %d for an insight"), DiaryEntries.Num(), MinDiaryEntriesForInsight);
        return false;
    }

    if (!ModelBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: insight skipped, no language model bridge"));
        return false;
    }

    // Most recent diary entries
    TArray<FNarrativeDiaryEntry> RecentEntries;
    const int32 StartIndex = FMath::Max(0, DiaryEntries.Num() - DiaryEntriesPerInsight);
    for (int32 i = StartIndex; i < DiaryEntries.Num(); i++)
    {
        RecentEntries.Add(DiaryEntries[i]);
    }

    // Numeric classification decides whether this is worth an insight at all
    FNarrativeInsight PendingInsight;
    float AverageValence = 0.0f;
    float AverageArousal = 0.0f;
    float ValenceVariance = 0.0f;
    ClassifyInsightPattern(RecentEntries, PendingInsight, AverageValence, AverageArousal, ValenceVariance);

    if (PendingInsight.Significance < Config.InsightThreshold)
    {
        UE_LOG(LogTemp, Verbose, TEXT("DiaryInsightBlogLoop: pattern significance %.2f below threshold %.2f, no insight requested"),
               PendingInsight.Significance, Config.InsightThreshold);
        return false;
    }

    PendingInsight.InsightID = FGuid::NewGuid();
    PendingInsight.Timestamp = CurrentWorldTime();

    FString Context;
    Context += FString::Printf(TEXT("Pattern family detected by the statistics: %s\n"), *PendingInsight.Category);
    Context += FString::Printf(TEXT("Across the last %d diary entries: average valence %.2f (-1..1), average arousal %.2f (0..1), valence variance %.2f\n"),
                               RecentEntries.Num(), AverageValence, AverageArousal, ValenceVariance);
    Context += FString::Printf(TEXT("Span covered: %.0f seconds\n"),
                               FMath::Max(0.0f, RecentEntries.Last().Timestamp - RecentEntries[0].Timestamp));
    Context += TEXT("The diary entries, oldest first:\n");
    for (int32 i = 0; i < RecentEntries.Num(); i++)
    {
        Context += FString::Printf(TEXT("%d. [%s] %s\n"), i + 1, *FormatAffect(RecentEntries[i].EmotionalTone), *RecentEntries[i].Content);
    }

    const FString Instruction =
        TEXT("Reading back over these diary entries, state one insight about yourself that the pattern in them reveals, in your own voice. ")
        TEXT("Reply with a short title on the first line, then a blank line, then the insight itself in two to four first-person sentences. ")
        TEXT("Do not quote the numbers and do not add anything else.");

    State.bInsightPending = true;

    TWeakObjectPtr<UDiaryInsightBlogLoop> WeakThis(this);
    ModelBridge->RequestText(Instruction, Context, [WeakThis, PendingInsight](bool bSuccess, const FString& TextOrError)
    {
        if (!WeakThis.IsValid())
        {
            return;
        }

        UDiaryInsightBlogLoop* Self = WeakThis.Get();
        Self->State.bInsightPending = false;

        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: insight not generated, model unavailable: %s"), *TextOrError);
            return;
        }

        FNarrativeInsight NewInsight = PendingInsight;
        if (!SplitTitleAndBody(TextOrError, NewInsight.Title, NewInsight.Content))
        {
            UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: insight discarded, model reply was empty"));
            return;
        }

        Self->Insights.Add(NewInsight);
        Self->State.TotalInsights++;

        if (Self->Insights.Num() > Self->Config.MaxInsights)
        {
            Self->Insights.RemoveAt(0);
        }

        Self->OnInsightGenerated.Broadcast(NewInsight);

        UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Generated insight #%d: %s"), Self->State.TotalInsights, *NewInsight.Title);
    });

    return true;
}

void UDiaryInsightBlogLoop::ClassifyInsightPattern(const TArray<FNarrativeDiaryEntry>& Entries, FNarrativeInsight& OutInsight,
                                                   float& OutAverageValence, float& OutAverageArousal, float& OutValenceVariance) const
{
    OutAverageValence = 0.0f;
    OutAverageArousal = 0.0f;
    OutValenceVariance = 0.0f;

    if (Entries.Num() == 0)
    {
        OutInsight.Category = TEXT("Cognitive Patterns");
        OutInsight.Significance = 0.0f;
        return;
    }

    for (const FNarrativeDiaryEntry& Entry : Entries)
    {
        OutAverageValence += Entry.EmotionalTone.Valence;
        OutAverageArousal += Entry.EmotionalTone.Arousal;
    }
    OutAverageValence /= Entries.Num();
    OutAverageArousal /= Entries.Num();

    for (const FNarrativeDiaryEntry& Entry : Entries)
    {
        const float Diff = Entry.EmotionalTone.Valence - OutAverageValence;
        OutValenceVariance += Diff * Diff;
    }
    OutValenceVariance /= Entries.Num();

    // Pattern family and significance score. These are classification values,
    // not prose; the model writes the insight itself.
    if (OutValenceVariance > 0.5f)
    {
        OutInsight.Category = TEXT("Emotional Patterns");
        OutInsight.Significance = 0.8f;
    }
    else if (OutAverageValence > 0.6f)
    {
        OutInsight.Category = TEXT("Emotional Patterns");
        OutInsight.Significance = 0.75f;
    }
    else if (OutAverageArousal > 0.7f)
    {
        OutInsight.Category = TEXT("Behavioral Patterns");
        OutInsight.Significance = 0.7f;
    }
    else
    {
        OutInsight.Category = TEXT("Cognitive Patterns");
        OutInsight.Significance = 0.65f;
    }
}

//=============================================================================
// Blog
//=============================================================================

void UDiaryInsightBlogLoop::ProcessBlogPublishing(float DeltaTime)
{
    if (State.bBlogPostPending)
    {
        return;
    }

    if (Insights.Num() < MinInsightsForBlogPost)
    {
        State.TimeSinceLastBlogPost = 0.0f;
        return;
    }

    RequestBlogPost();
    State.TimeSinceLastBlogPost = 0.0f;
}

bool UDiaryInsightBlogLoop::RequestBlogPost()
{
    if (State.bBlogPostPending)
    {
        return false;
    }

    if (Insights.Num() < MinInsightsForBlogPost)
    {
        UE_LOG(LogTemp, Verbose, TEXT("DiaryInsightBlogLoop: %d insights, need %d for a blog post"), Insights.Num(), MinInsightsForBlogPost);
        return false;
    }

    if (!ModelBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: blog post skipped, no language model bridge"));
        return false;
    }

    // Recent significant insights
    TArray<FNarrativeInsight> SignificantInsights;
    const int32 StartIndex = FMath::Max(0, Insights.Num() - InsightsPerBlogPost);
    for (int32 i = StartIndex; i < Insights.Num(); i++)
    {
        if (Insights[i].Significance >= Config.InsightThreshold)
        {
            SignificantInsights.Add(Insights[i]);
        }
    }

    if (SignificantInsights.Num() == 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("DiaryInsightBlogLoop: no insight above threshold %.2f, no blog post requested"), Config.InsightThreshold);
        return false;
    }

    FNarrativeBlogPost PendingPost;
    PendingPost.PostID = FGuid::NewGuid();
    PendingPost.Timestamp = CurrentWorldTime();
    PendingPost.Tags = GenerateBlogTags(SignificantInsights);
    PendingPost.SourceInsights = SignificantInsights;

    const FNarrativeInsight* MostSignificant = &SignificantInsights[0];
    for (const FNarrativeInsight& Insight : SignificantInsights)
    {
        if (Insight.Significance > MostSignificant->Significance)
        {
            MostSignificant = &Insight;
        }
    }

    FString Context;
    Context += FString::Printf(TEXT("Insights to draw on (%d), oldest first:\n"), SignificantInsights.Num());
    for (int32 i = 0; i < SignificantInsights.Num(); i++)
    {
        const FNarrativeInsight& Insight = SignificantInsights[i];
        Context += FString::Printf(TEXT("%d. %s [%s]\n%s\n"), i + 1, *Insight.Title, *Insight.Category, *Insight.Content);
    }
    Context += FString::Printf(TEXT("The insight the statistics rate most significant: %s\n"), *MostSignificant->Title);
    if (BlogPosts.Num() > 0)
    {
        Context += FString::Printf(TEXT("Title of your previous post (choose a different one): %s\n"), *BlogPosts.Last().Title);
    }

    const FString Instruction =
        TEXT("Write a public blog post, in your own voice, that shares what these recent insights about yourself add up to. ")
        TEXT("Reply with the title on the first line, then a blank line, then the post body in Markdown: an opening that frames the period, ")
        TEXT("a section for each insight that develops it rather than restating it, and a closing that says what you take forward. ")
        TEXT("Do not add anything outside the title and the body.");

    State.bBlogPostPending = true;

    // A post needs more room than the bridge's default answer length
    TArray<FDTEChatMessage> Messages;
    Messages.Emplace(TEXT("system"), ModelBridge->GetSystemPrompt());
    Messages.Emplace(TEXT("user"), Instruction + TEXT("\n\nContext:\n") + Context);
    const int32 PostMaxTokens = FMath::Max(ModelBridge->MaxTokens, 1200);

    TWeakObjectPtr<UDiaryInsightBlogLoop> WeakThis(this);
    ModelBridge->RequestChat(Messages, [WeakThis, PendingPost](bool bSuccess, const FString& TextOrError)
    {
        if (!WeakThis.IsValid())
        {
            return;
        }

        UDiaryInsightBlogLoop* Self = WeakThis.Get();
        Self->State.bBlogPostPending = false;

        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: blog post not published, model unavailable: %s"), *TextOrError);
            return;
        }

        FNarrativeBlogPost NewPost = PendingPost;
        if (!SplitTitleAndBody(TextOrError, NewPost.Title, NewPost.Content))
        {
            UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: blog post discarded, model reply was empty"));
            return;
        }

        Self->BlogPosts.Add(NewPost);
        Self->State.TotalBlogPosts++;

        if (Self->BlogPosts.Num() > Self->Config.MaxBlogPosts)
        {
            Self->BlogPosts.RemoveAt(0);
        }

        Self->OnBlogPostPublished.Broadcast(NewPost);

        UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Published blog post #%d: %s"), Self->State.TotalBlogPosts, *NewPost.Title);
    }, ModelBridge->Temperature, PostMaxTokens);

    return true;
}

//=============================================================================
// Numeric helpers
//=============================================================================

FNarrativeAffect UDiaryInsightBlogLoop::CalculateAverageEmotionalState(const TArray<FNarrativeObservation>& Observations) const
{
    FNarrativeAffect Average;

    if (Observations.Num() == 0)
    {
        return Average;
    }

    float TotalValence = 0.0f;
    float TotalArousal = 0.0f;
    float TotalDominance = 0.0f;

    for (const FNarrativeObservation& Obs : Observations)
    {
        TotalValence += Obs.EmotionalState.Valence;
        TotalArousal += Obs.EmotionalState.Arousal;
        TotalDominance += Obs.EmotionalState.Dominance;
    }

    const int32 Count = Observations.Num();
    Average.Valence = TotalValence / Count;
    Average.Arousal = TotalArousal / Count;
    Average.Dominance = TotalDominance / Count;

    return Average;
}

FNarrativePersonalityExpression UDiaryInsightBlogLoop::CalculateAveragePersonality(const TArray<FNarrativeObservation>& Observations) const
{
    FNarrativePersonalityExpression Average;

    if (Observations.Num() == 0)
    {
        return Average;
    }

    float TotalConfidence = 0.0f;
    float TotalPlayfulness = 0.0f;
    float TotalUnpredictability = 0.0f;

    for (const FNarrativeObservation& Obs : Observations)
    {
        TotalConfidence += Obs.PersonalityExpression.Confidence;
        TotalPlayfulness += Obs.PersonalityExpression.Playfulness;
        TotalUnpredictability += Obs.PersonalityExpression.Unpredictability;
    }

    const int32 Count = Observations.Num();
    Average.Confidence = TotalConfidence / Count;
    Average.Playfulness = TotalPlayfulness / Count;
    Average.Unpredictability = TotalUnpredictability / Count;

    return Average;
}

TArray<FString> UDiaryInsightBlogLoop::GenerateTags(const FNarrativeAffect& Emotion, const FNarrativePersonalityExpression& Personality) const
{
    // Classification labels for search and filtering; they are metadata on the
    // entry, not part of what the character wrote.
    TArray<FString> Tags;

    if (Emotion.Valence > 0.5f)
    {
        Tags.Add(TEXT("positive-valence"));
    }
    else if (Emotion.Valence < -0.5f)
    {
        Tags.Add(TEXT("negative-valence"));
    }

    Tags.Add(Emotion.Arousal > 0.5f ? TEXT("high-arousal") : TEXT("low-arousal"));

    if (Personality.Confidence > 0.7f)
    {
        Tags.Add(TEXT("confident"));
    }

    if (Personality.Playfulness > 0.7f)
    {
        Tags.Add(TEXT("playful"));
    }

    if (Personality.Unpredictability > 0.7f)
    {
        Tags.Add(TEXT("unpredictable"));
    }

    return Tags;
}

TArray<FString> UDiaryInsightBlogLoop::GenerateBlogTags(const TArray<FNarrativeInsight>& SourceInsights) const
{
    TArray<FString> Tags;

    Tags.Add(TEXT("self-reflection"));
    Tags.Add(TEXT("deep-tree-echo"));

    for (const FNarrativeInsight& Insight : SourceInsights)
    {
        Tags.AddUnique(Insight.Category);
    }

    return Tags;
}

bool UDiaryInsightBlogLoop::SplitTitleAndBody(const FString& ModelText, FString& OutTitle, FString& OutBody)
{
    OutTitle.Reset();
    OutBody.Reset();

    TArray<FString> Lines;
    ModelText.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);

    int32 TitleIndex = INDEX_NONE;
    for (int32 i = 0; i < Lines.Num(); i++)
    {
        if (!Lines[i].TrimStartAndEnd().IsEmpty())
        {
            TitleIndex = i;
            break;
        }
    }

    if (TitleIndex == INDEX_NONE)
    {
        return false;
    }

    FString Title = Lines[TitleIndex].TrimStartAndEnd();
    // Strip Markdown heading markers and emphasis a model may wrap the title in
    while (Title.StartsWith(TEXT("#")))
    {
        Title.RemoveAt(0);
    }
    Title = Title.TrimStartAndEnd();
    Title.RemoveFromStart(TEXT("**"));
    Title.RemoveFromEnd(TEXT("**"));
    Title.RemoveFromStart(TEXT("Title:"));
    Title = Title.TrimStartAndEnd();

    FString Body;
    for (int32 i = TitleIndex + 1; i < Lines.Num(); i++)
    {
        Body += Lines[i];
        Body += TEXT("\n");
    }
    Body = Body.TrimStartAndEnd();

    OutTitle = Title;
    // A single-line reply is used whole as the body so no model text is lost.
    OutBody = Body.IsEmpty() ? Title : Body;
    return !OutTitle.IsEmpty();
}

FString UDiaryInsightBlogLoop::FormatAffect(const FNarrativeAffect& Affect)
{
    return FString::Printf(TEXT("valence %.2f (-1 unpleasant .. 1 pleasant), arousal %.2f (0 calm .. 1 energised), dominance %.2f (0 overwhelmed .. 1 in control)"),
                           Affect.Valence, Affect.Arousal, Affect.Dominance);
}

FString UDiaryInsightBlogLoop::FormatPersonality(const FNarrativePersonalityExpression& Personality)
{
    return FString::Printf(TEXT("confidence %.2f, playfulness %.2f, unpredictability %.2f (each 0..1)"),
                           Personality.Confidence, Personality.Playfulness, Personality.Unpredictability);
}

float UDiaryInsightBlogLoop::CurrentWorldTime() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.0f;
}

//=============================================================================
// Access
//=============================================================================

FNarrativeDiaryEntry UDiaryInsightBlogLoop::GetMostRecentDiaryEntry() const
{
    if (DiaryEntries.Num() > 0)
    {
        return DiaryEntries.Last();
    }
    return FNarrativeDiaryEntry();
}

FNarrativeInsight UDiaryInsightBlogLoop::GetMostRecentInsight() const
{
    if (Insights.Num() > 0)
    {
        return Insights.Last();
    }
    return FNarrativeInsight();
}

FNarrativeBlogPost UDiaryInsightBlogLoop::GetMostRecentBlogPost() const
{
    if (BlogPosts.Num() > 0)
    {
        return BlogPosts.Last();
    }
    return FNarrativeBlogPost();
}

TArray<FNarrativeDiaryEntry> UDiaryInsightBlogLoop::GetRecentDiaryEntries(int32 Count) const
{
    TArray<FNarrativeDiaryEntry> Recent;

    const int32 StartIndex = FMath::Max(0, DiaryEntries.Num() - Count);
    for (int32 i = StartIndex; i < DiaryEntries.Num(); i++)
    {
        Recent.Add(DiaryEntries[i]);
    }

    return Recent;
}

TArray<FNarrativeInsight> UDiaryInsightBlogLoop::GetRecentInsights(int32 Count) const
{
    TArray<FNarrativeInsight> Recent;

    const int32 StartIndex = FMath::Max(0, Insights.Num() - Count);
    for (int32 i = StartIndex; i < Insights.Num(); i++)
    {
        Recent.Add(Insights[i]);
    }

    return Recent;
}

TArray<FNarrativeBlogPost> UDiaryInsightBlogLoop::GetRecentBlogPosts(int32 Count) const
{
    TArray<FNarrativeBlogPost> Recent;

    const int32 StartIndex = FMath::Max(0, BlogPosts.Num() - Count);
    for (int32 i = StartIndex; i < BlogPosts.Num(); i++)
    {
        Recent.Add(BlogPosts[i]);
    }

    return Recent;
}

//=============================================================================
// Configuration
//=============================================================================

void UDiaryInsightBlogLoop::SetDiaryUpdateInterval(float Interval)
{
    Config.DiaryUpdateInterval = FMath::Max(1.0f, Interval);
}

void UDiaryInsightBlogLoop::SetInsightGenerationInterval(float Interval)
{
    Config.InsightGenerationInterval = FMath::Max(1.0f, Interval);
}

void UDiaryInsightBlogLoop::SetBlogPublishInterval(float Interval)
{
    Config.BlogPublishInterval = FMath::Max(1.0f, Interval);
}

void UDiaryInsightBlogLoop::SetAutoPublish(bool bEnabled)
{
    Config.bAutoPublish = bEnabled;
}

void UDiaryInsightBlogLoop::ForceDiaryEntry()
{
    State.TimeSinceLastDiaryEntry = Config.DiaryUpdateInterval;
}

void UDiaryInsightBlogLoop::ForceInsightGeneration()
{
    State.TimeSinceLastInsight = Config.InsightGenerationInterval;
}

void UDiaryInsightBlogLoop::ForceBlogPublish()
{
    State.TimeSinceLastBlogPost = Config.BlogPublishInterval;
}

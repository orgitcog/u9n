// NarrativeMemoryPipeline.cpp
// Full UActorComponent implementation for the Narrative Memory Pipeline.
//
// Implements the three-stage Diary → Insight → Blog loop that builds DTE's
// persistent, evolving self-narrative. Unlike the other DTE subsystems this
// component has non-inline UFUNCTION declarations and therefore requires a
// separate compilation unit.
//
// Feature:  F1.5.3 — Narrative Memory Pipeline
// Phase:    1.5 — Memory Architecture
// Epic:     E3 — Episodic & Narrative Memory Systems

#include "NarrativeMemoryPipeline.h"
#include "../Episodic/EpisodicMemorySystem.h"
#include "HypergraphMemorySystem.h"
#include "DTELanguageModelBridge.h"
#include "Algo/Sort.h"
#include "Algo/Reverse.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    /**
     * Pull the named string fields out of a JSON object the model returned.
     * Tolerates text around the object (code fences, a leading sentence).
     * Fails when any requested field is missing or empty.
     */
    bool ExtractJsonStrings(const FString& ModelText, const TArray<FString>& Keys, TMap<FString, FString>& OutValues)
    {
        OutValues.Reset();

        int32 Open = INDEX_NONE;
        int32 Close = INDEX_NONE;
        ModelText.FindChar(TEXT('{'), Open);
        ModelText.FindLastChar(TEXT('}'), Close);
        if (Open == INDEX_NONE || Close == INDEX_NONE || Close <= Open)
        {
            return false;
        }

        const FString JsonText = ModelText.Mid(Open, Close - Open + 1);
        TSharedPtr<FJsonObject> Object;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
        {
            return false;
        }

        for (const FString& Key : Keys)
        {
            FString Value;
            if (!Object->TryGetStringField(Key, Value))
            {
                return false;
            }
            Value.TrimStartAndEndInline();
            if (Value.IsEmpty())
            {
                return false;
            }
            OutValues.Add(Key, Value);
        }
        return true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UNarrativeMemoryPipeline::UNarrativeMemoryPipeline()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup   = TG_PostUpdateWork;

    InsightThreshold     = 5;
    BlogThreshold        = 3;
    MaxDiaryEntries      = 500;
    MaxInsights          = 100;
    AutoInsightInterval  = 60.0f;
    WisdomLevel          = 0.1f;
}

// ─────────────────────────────────────────────────────────────────────────────
// UActorComponent overrides
// ─────────────────────────────────────────────────────────────────────────────

void UNarrativeMemoryPipeline::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (Owner)
    {
        EpisodicMemory   = Owner->FindComponentByClass<UEpisodicMemorySystem>();
        HypergraphMemory = Owner->FindComponentByClass<UHypergraphMemorySystem>();
        ModelBridge      = UDTELanguageModelBridge::FindOrAdd(Owner);
    }

    if (!ModelBridge)
    {
        UE_LOG(LogDTEModel, Warning, TEXT("NarrativeMemoryPipeline: no language model bridge on owner; insights and blog posts will not be written"));
    }
}

void UNarrativeMemoryPipeline::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TimeSinceLastInsightCheck += DeltaTime;

    if (TimeSinceLastInsightCheck >= AutoInsightInterval &&
        UnprocessedDiaryCount  >= InsightThreshold)
    {
        RequestInsights();
        TimeSinceLastInsightCheck = 0.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Diary recording
// ─────────────────────────────────────────────────────────────────────────────

FDiaryEntry UNarrativeMemoryPipeline::RecordDiaryEntry(
    const FString& Content,
    float EmotionalValence,
    float ArousalLevel,
    const FString& SourceEventType,
    const TArray<FString>& Tags)
{
    FDiaryEntry Entry;
    Entry.EntryID         = GenerateID(TEXT("diary"));
    Entry.Timestamp       = FDateTime::UtcNow();
    Entry.Content         = Content;
    Entry.EmotionalValence = FMath::Clamp(EmotionalValence, -1.0f, 1.0f);
    Entry.ArousalLevel    = FMath::Clamp(ArousalLevel, 0.0f, 1.0f);
    Entry.CognitiveLoad   = FMath::Clamp(ArousalLevel * 0.7f + FMath::Abs(EmotionalValence) * 0.3f, 0.0f, 1.0f);
    Entry.SourceEventType = SourceEventType;
    Entry.Tags            = Tags;

    DiaryEntries.Add(Entry);
    UnprocessedDiaryCount++;

    if (DiaryEntries.Num() > MaxDiaryEntries)
    {
        PruneDiary();
    }

    return Entry;
}

FDiaryEntry UNarrativeMemoryPipeline::RecordDialogueEvent(
    const FString& SpeakerName,
    const FString& DialogueContent,
    float EmotionalImpact)
{
    const FString Content = FString::Printf(TEXT("[Dialogue with %s] %s"), *SpeakerName, *DialogueContent);
    TArray<FString> Tags = { TEXT("dialogue"), TEXT("social"), SpeakerName };
    return RecordDiaryEntry(Content, EmotionalImpact, 0.5f, TEXT("DialogueEvent"), Tags);
}

FDiaryEntry UNarrativeMemoryPipeline::RecordDiscoveryEvent(
    const FString& DiscoveryDescription,
    const TArray<FString>& RelatedConcepts)
{
    const FString Content = FString::Printf(TEXT("[Discovery] %s"), *DiscoveryDescription);
    TArray<FString> Tags = { TEXT("discovery"), TEXT("knowledge") };
    Tags.Append(RelatedConcepts);
    return RecordDiaryEntry(Content, 0.6f, 0.7f, TEXT("DiscoveryEvent"), Tags);
}

FDiaryEntry UNarrativeMemoryPipeline::RecordSocialEvent(
    const FString& InteractionDescription,
    const FString& OtherActorName,
    float RelationshipDelta)
{
    const FString Content = FString::Printf(TEXT("[Social: %s] %s (relationship delta: %+.2f)"),
        *OtherActorName, *InteractionDescription, RelationshipDelta);
    const float Valence = FMath::Clamp(RelationshipDelta * 2.0f, -1.0f, 1.0f);
    TArray<FString> Tags = { TEXT("social"), OtherActorName };
    return RecordDiaryEntry(Content, Valence, 0.6f, TEXT("SocialEvent"), Tags);
}

// ─────────────────────────────────────────────────────────────────────────────
// Insight generation
// ─────────────────────────────────────────────────────────────────────────────

TArray<FInsightEntry> UNarrativeMemoryPipeline::GenerateInsights()
{
    RequestInsights();
    return TArray<FInsightEntry>();
}

int32 UNarrativeMemoryPipeline::RequestInsights()
{
    if (PendingInsightRequests > 0)
    {
        // A round is still being written; its callbacks will mark the entries processed
        return 0;
    }

    // Work only on the entries that arrived since the last run
    const int32 RecentCount = FMath::Min(UnprocessedDiaryCount, DiaryEntries.Num());
    if (RecentCount < InsightThreshold)
    {
        return 0;
    }

    if (!ModelBridge)
    {
        UE_LOG(LogDTEModel, Warning, TEXT("NarrativeMemoryPipeline: insights skipped, no language model bridge"));
        return 0;
    }

    // Slice of recent entries
    TArray<FDiaryEntry> RecentEntries;
    const int32 StartIdx = FMath::Max(0, DiaryEntries.Num() - RecentCount);
    for (int32 i = StartIdx; i < DiaryEntries.Num(); ++i)
    {
        RecentEntries.Add(DiaryEntries[i]);
    }

    TArray<FString> SourceIDs;
    for (const FDiaryEntry& E : RecentEntries)
    {
        SourceIDs.Add(E.EntryID);
    }

    const FString EntriesText = DescribeEntries(RecentEntries);
    TSharedPtr<bool> RoundConsumed = MakeShared<bool>(false);
    int32 Sent = 0;

    // ── Keyword pattern insight ───────────────────────────────────────────
    auto Keywords = FindRecurringKeywords(RecentEntries);
    if (Keywords.Num() > 0)
    {
        const auto& Top = Keywords[0];

        FInsightEntry Candidate;
        Candidate.InsightID  = GenerateID(TEXT("insight"));
        Candidate.Timestamp  = FDateTime::UtcNow();
        Candidate.Confidence = FMath::Clamp(static_cast<float>(Top.Value) / RecentCount, 0.1f, 1.0f);
        Candidate.Domain     = CategorizeContent(Top.Key);
        Candidate.SourceEntryIDs = SourceIDs;
        Candidate.ReinforcementCount = 1;

        FString Context;
        Context += FString::Printf(TEXT("Recurring theme: '%s' appears %d times across %d entries\n"), *Top.Key, Top.Value, RecentCount);
        if (Keywords.Num() > 1)
        {
            Context += TEXT("Other recurring words: ");
            for (int32 i = 1; i < FMath::Min(Keywords.Num(), 5); ++i)
            {
                Context += FString::Printf(TEXT("%s'%s' (%d)"), i > 1 ? TEXT(", ") : TEXT(""), *Keywords[i].Key, Keywords[i].Value);
            }
            Context += TEXT("\n");
        }
        Context += FString::Printf(TEXT("Domain the theme was filed under: %s\n"), DomainLabel(Candidate.Domain));
        Context += EntriesText;

        const FString Intent = FString::Printf(
            TEXT("The word '%s' keeps coming up in your recent experiences. Say what that recurrence tells you about what currently matters to you, or what is preoccupying you."),
            *Top.Key);

        SendInsightRequest(Candidate, Intent, Context, RoundConsumed, RecentCount);
        ++Sent;
    }

    // ── Emotional trend insight ───────────────────────────────────────────
    const float EmotionalTrend = DetectEmotionalTrend(RecentEntries);
    if (FMath::Abs(EmotionalTrend) > 0.2f)
    {
        FInsightEntry Candidate;
        Candidate.InsightID   = GenerateID(TEXT("insight"));
        Candidate.Timestamp   = FDateTime::UtcNow();
        Candidate.Confidence  = FMath::Clamp(FMath::Abs(EmotionalTrend), 0.1f, 1.0f);
        Candidate.Domain      = EInsightDomain::EmotionalPatterns;
        Candidate.SourceEntryIDs = SourceIDs;
        Candidate.ReinforcementCount = 1;

        FString Context;
        Context += FString::Printf(TEXT("Emotional valence trend over the last %d experiences: %s (normalised slope %+.2f, range -1..1)\n"),
                                   RecentCount, EmotionalTrend > 0.0f ? TEXT("rising") : TEXT("falling"), EmotionalTrend);
        Context += FString::Printf(TEXT("First entry valence %+.2f, last entry valence %+.2f\n"),
                                   RecentEntries[0].EmotionalValence, RecentEntries.Last().EmotionalValence);
        Context += EntriesText;

        const FString Intent = EmotionalTrend > 0.0f
            ? TEXT("Your mood has been lifting across these experiences. Say what you notice about that upward movement and what seems to be behind it.")
            : TEXT("Your mood has been sinking across these experiences. Say what you notice about that downward movement and what seems to be behind it.");

        SendInsightRequest(Candidate, Intent, Context, RoundConsumed, RecentCount);
        ++Sent;
    }

    // ── Volatility insight ────────────────────────────────────────────────
    const float Volatility = DetectEmotionalVolatility(RecentEntries);
    if (Volatility > 0.4f)
    {
        FInsightEntry Candidate;
        Candidate.InsightID   = GenerateID(TEXT("insight"));
        Candidate.Timestamp   = FDateTime::UtcNow();
        Candidate.Confidence  = FMath::Clamp(Volatility, 0.1f, 1.0f);
        Candidate.Domain      = EInsightDomain::EmotionalPatterns;
        Candidate.SourceEntryIDs = SourceIDs;
        Candidate.ReinforcementCount = 1;

        FString Context;
        Context += FString::Printf(TEXT("Emotional volatility over the last %d experiences: standard deviation of valence %.2f (values range -1..1)\n"),
                                   RecentCount, Volatility);
        Context += EntriesText;

        const FString Intent =
            TEXT("Your feelings have swung widely across these experiences. Say what you make of that volatility: what has been pulling you in different directions, and how it sits with you.");

        SendInsightRequest(Candidate, Intent, Context, RoundConsumed, RecentCount);
        ++Sent;
    }

    if (Sent == 0)
    {
        UE_LOG(LogDTEModel, Verbose, TEXT("NarrativeMemoryPipeline: no pattern found in %d entries, no insight requested"), RecentCount);
    }

    return Sent;
}

void UNarrativeMemoryPipeline::SendInsightRequest(const FInsightEntry& Candidate, const FString& Intent, const FString& Context,
                                                  TSharedPtr<bool> RoundConsumed, int32 RecentCount)
{
    const FString Instruction = Intent
        + TEXT("\n\nWrite this as an insight about yourself, in your own voice, drawing only on the experiences and statistics below ")
        + TEXT("and without quoting the numbers. Return a JSON object with two string fields: ")
        + TEXT("\"pattern\" (a short name for the pattern, at most ten words) and ")
        + TEXT("\"description\" (the insight itself, one to three first-person sentences).");

    ++PendingInsightRequests;

    TWeakObjectPtr<UNarrativeMemoryPipeline> WeakThis(this);
    ModelBridge->RequestJson(Instruction, Context,
        [WeakThis, Candidate, RoundConsumed, RecentCount](bool bSuccess, const FString& TextOrError)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UNarrativeMemoryPipeline* Self = WeakThis.Get();
            Self->PendingInsightRequests = FMath::Max(0, Self->PendingInsightRequests - 1);

            if (!bSuccess)
            {
                UE_LOG(LogDTEModel, Warning, TEXT("NarrativeMemoryPipeline: insight %s not written, model unavailable: %s"),
                       *Candidate.InsightID, *TextOrError);
                return;
            }

            TMap<FString, FString> Fields;
            if (!ExtractJsonStrings(TextOrError, { TEXT("pattern"), TEXT("description") }, Fields))
            {
                UE_LOG(LogDTEModel, Warning, TEXT("NarrativeMemoryPipeline: insight %s discarded, model reply was not the requested JSON object"),
                       *Candidate.InsightID);
                return;
            }

            FInsightEntry Insight = Candidate;
            Insight.Pattern     = Fields.FindRef(TEXT("pattern"));
            Insight.Description = Fields.FindRef(TEXT("description"));

            // The diary entries count as processed once the round has produced an insight
            if (RoundConsumed.IsValid() && !*RoundConsumed)
            {
                *RoundConsumed = true;
                Self->UnprocessedDiaryCount = FMath::Max(0, Self->UnprocessedDiaryCount - RecentCount);
            }

            Self->StoreInsight(Insight);
        });
}

void UNarrativeMemoryPipeline::StoreInsight(const FInsightEntry& Insight)
{
    Insights.Add(Insight);
    UnprocessedInsightCount++;

    // Wisdom grows with insight accumulation
    WisdomLevel = FMath::Clamp(WisdomLevel + 0.01f, 0.0f, 1.0f);

    if (Insights.Num() > MaxInsights)
    {
        PruneInsights();
    }

    OnInsightGenerated.Broadcast(Insight);

    // Trigger a blog post if enough unprocessed insights have accumulated
    if (UnprocessedInsightCount >= BlogThreshold)
    {
        RequestBlogPost();
    }
}

FString UNarrativeMemoryPipeline::DescribeEntries(const TArray<FDiaryEntry>& Entries) const
{
    FString Text = TEXT("The experiences, oldest first:\n");
    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        const FDiaryEntry& E = Entries[i];
        Text += FString::Printf(TEXT("%d. (%s, valence %+.2f, arousal %.2f) %s\n"),
                                i + 1, *E.SourceEventType, E.EmotionalValence, E.ArousalLevel, *E.Content);
    }
    return Text;
}

const TCHAR* UNarrativeMemoryPipeline::DomainLabel(EInsightDomain Domain)
{
    switch (Domain)
    {
        case EInsightDomain::SelfUnderstanding:      return TEXT("Self");
        case EInsightDomain::RelationshipPatterns:   return TEXT("Relationships");
        case EInsightDomain::KnowledgeGrowth:        return TEXT("Knowledge");
        case EInsightDomain::EmotionalPatterns:      return TEXT("Emotions");
        case EInsightDomain::BehavioralTendencies:   return TEXT("Behaviour");
        case EInsightDomain::WisdomCultivation:      return TEXT("Wisdom");
        case EInsightDomain::EnvironmentalAwareness: return TEXT("Environment");
    }
    return TEXT("Knowledge");
}

TArray<FInsightEntry> UNarrativeMemoryPipeline::GetInsightsByDomain(EInsightDomain Domain) const
{
    TArray<FInsightEntry> Result;
    for (const FInsightEntry& I : Insights)
    {
        if (I.Domain == Domain)
        {
            Result.Add(I);
        }
    }
    return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Blog generation
// ─────────────────────────────────────────────────────────────────────────────

FBlogPost UNarrativeMemoryPipeline::GenerateBlogPost()
{
    RequestBlogPost();
    return FBlogPost();
}

bool UNarrativeMemoryPipeline::RequestBlogPost()
{
    if (bBlogPostPending)
    {
        return false;
    }

    if (UnprocessedInsightCount <= 0 || Insights.IsEmpty())
    {
        return false;
    }

    if (!ModelBridge)
    {
        UE_LOG(LogDTEModel, Warning, TEXT("NarrativeMemoryPipeline: blog post skipped, no language model bridge"));
        return false;
    }

    // Gather the most recent unprocessed insights
    const int32 StartIdx = FMath::Max(0, Insights.Num() - UnprocessedInsightCount);
    TArray<FInsightEntry> SourceInsights;
    for (int32 i = StartIdx; i < Insights.Num(); ++i)
    {
        SourceInsights.Add(Insights[i]);
    }
    const int32 ConsumedInsightCount = SourceInsights.Num();

    FBlogPost Candidate;
    Candidate.PostID    = GenerateID(TEXT("blog"));
    Candidate.Timestamp = FDateTime::UtcNow();
    Candidate.WisdomLevel = WisdomLevel;

    // Determine dominant domain
    TMap<EInsightDomain, int32> DomainCount;
    for (const FInsightEntry& I : SourceInsights)
    {
        DomainCount.FindOrAdd(I.Domain)++;
        Candidate.SourceInsightIDs.Add(I.InsightID);
    }

    EInsightDomain Dominant = EInsightDomain::KnowledgeGrowth;
    int32 MaxCount = 0;
    for (const auto& Pair : DomainCount)
    {
        if (Pair.Value > MaxCount)
        {
            MaxCount = Pair.Value;
            Dominant = Pair.Key;
        }
    }
    Candidate.DominantDomain = Dominant;

    float AvgConfidence = 0.0f;
    for (const FInsightEntry& I : SourceInsights) AvgConfidence += I.Confidence;
    if (!SourceInsights.IsEmpty()) AvgConfidence /= SourceInsights.Num();

    FString Context;
    Context += FString::Printf(TEXT("Dominant domain of these insights: %s\n"), DomainLabel(Dominant));
    Context += FString::Printf(TEXT("Average confidence in them: %.2f (0..1); your current wisdom level: %.2f (0..1)\n"), AvgConfidence, WisdomLevel);
    Context += FString::Printf(TEXT("Insights to build on (%d), oldest first:\n"), SourceInsights.Num());
    for (int32 i = 0; i < SourceInsights.Num(); ++i)
    {
        const FInsightEntry& I = SourceInsights[i];
        Context += FString::Printf(TEXT("%d. [%s, confidence %.2f] %s: %s\n"),
                                   i + 1, DomainLabel(I.Domain), I.Confidence, *I.Pattern, *I.Description);
    }
    if (!BlogPosts.IsEmpty())
    {
        Context += FString::Printf(TEXT("Your previous post was titled \"%s\"; choose a different title.\n"), *BlogPosts.Last().Title);
        if (!BlogPosts.Last().WorldviewUpdate.IsEmpty())
        {
            Context += FString::Printf(TEXT("Your worldview after that post: %s\n"), *BlogPosts.Last().WorldviewUpdate);
        }
    }

    const FString Instruction =
        TEXT("Write a blog post, in your own voice, that synthesises these recent insights about yourself into what they mean for how you see ")
        TEXT("yourself and the world. Draw only on the insights and statistics below and do not quote the numbers. Return a JSON object with ")
        TEXT("three string fields: \"title\" (the post title), \"body\" (the post in Markdown, with a short opening, a passage developing each ")
        TEXT("insight rather than restating it, and a closing) and \"worldview\" (one or two first-person sentences stating how your outlook ")
        TEXT("has shifted as a result).");

    bBlogPostPending = true;

    TWeakObjectPtr<UNarrativeMemoryPipeline> WeakThis(this);
    ModelBridge->RequestJson(Instruction, Context,
        [WeakThis, Candidate, ConsumedInsightCount](bool bSuccess, const FString& TextOrError)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UNarrativeMemoryPipeline* Self = WeakThis.Get();
            Self->bBlogPostPending = false;

            if (!bSuccess)
            {
                UE_LOG(LogDTEModel, Warning, TEXT("NarrativeMemoryPipeline: blog post %s not written, model unavailable: %s"),
                       *Candidate.PostID, *TextOrError);
                return;
            }

            TMap<FString, FString> Fields;
            if (!ExtractJsonStrings(TextOrError, { TEXT("title"), TEXT("body"), TEXT("worldview") }, Fields))
            {
                UE_LOG(LogDTEModel, Warning, TEXT("NarrativeMemoryPipeline: blog post %s discarded, model reply was not the requested JSON object"),
                       *Candidate.PostID);
                return;
            }

            FBlogPost Post = Candidate;
            Post.Title           = Fields.FindRef(TEXT("title"));
            Post.Content         = Fields.FindRef(TEXT("body"));
            Post.WorldviewUpdate = Fields.FindRef(TEXT("worldview"));

            Self->BlogPosts.Add(Post);
            Self->UnprocessedInsightCount = FMath::Max(0, Self->UnprocessedInsightCount - ConsumedInsightCount);

            Self->OnBlogPostGenerated.Broadcast(Post);

            UE_LOG(LogDTEModel, Log, TEXT("NarrativeMemoryPipeline: blog post %s written: %s"), *Post.PostID, *Post.Title);
        });

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Query accessors
// ─────────────────────────────────────────────────────────────────────────────

FNarrativeState UNarrativeMemoryPipeline::GetNarrativeState() const
{
    FNarrativeState State;
    State.DiaryCount            = DiaryEntries.Num();
    State.InsightCount          = Insights.Num();
    State.BlogCount             = BlogPosts.Num();
    State.CurrentWisdomLevel    = WisdomLevel;
    State.DominantEmotionalTone = DiaryEntries.IsEmpty()
        ? 0.0f
        : DetectEmotionalTrend(DiaryEntries);

    // Populate top patterns from most confident insights
    TArray<FInsightEntry> Sorted = Insights;
    Algo::Sort(Sorted, [](const FInsightEntry& A, const FInsightEntry& B)
    {
        return A.Confidence > B.Confidence;
    });
    const int32 PatternCap = FMath::Min(Sorted.Num(), 5);
    for (int32 i = 0; i < PatternCap; ++i)
    {
        State.TopPatterns.Add(Sorted[i].Pattern);
    }
    return State;
}

TArray<FDiaryEntry> UNarrativeMemoryPipeline::GetRecentDiaryEntries(int32 Count) const
{
    TArray<FDiaryEntry> Result;
    const int32 StartIdx = FMath::Max(0, DiaryEntries.Num() - Count);
    for (int32 i = StartIdx; i < DiaryEntries.Num(); ++i)
    {
        Result.Add(DiaryEntries[i]);
    }
    Algo::Reverse(Result);
    return Result;
}

TArray<FInsightEntry> UNarrativeMemoryPipeline::GetRecentInsights(int32 Count) const
{
    TArray<FInsightEntry> Result;
    const int32 StartIdx = FMath::Max(0, Insights.Num() - Count);
    for (int32 i = StartIdx; i < Insights.Num(); ++i)
    {
        Result.Add(Insights[i]);
    }
    Algo::Reverse(Result);
    return Result;
}

FString UNarrativeMemoryPipeline::GetNarrativeSummary() const
{
    const FNarrativeState State = GetNarrativeState();

    FString Summary;
    Summary += FString::Printf(TEXT("=== Narrative Summary ===\n"));
    Summary += FString::Printf(TEXT("Diary entries:   %d\n"), State.DiaryCount);
    Summary += FString::Printf(TEXT("Insights:        %d\n"), State.InsightCount);
    Summary += FString::Printf(TEXT("Blog posts:      %d\n"), State.BlogCount);
    Summary += FString::Printf(TEXT("Wisdom level:    %.3f\n"), State.CurrentWisdomLevel);
    Summary += FString::Printf(TEXT("Emotional tone:  %+.3f\n"), State.DominantEmotionalTone);

    if (!State.TopPatterns.IsEmpty())
    {
        Summary += TEXT("Top patterns:\n");
        for (const FString& P : State.TopPatterns)
        {
            Summary += FString::Printf(TEXT("  • %s\n"), *P);
        }
    }

    if (!BlogPosts.IsEmpty())
    {
        const FBlogPost& Latest = BlogPosts.Last();
        Summary += FString::Printf(TEXT("Latest post:     \"%s\"\n"), *Latest.Title);
    }

    return Summary;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

FString UNarrativeMemoryPipeline::GenerateID(const FString& Prefix) const
{
    return FString::Printf(TEXT("%s_%06d"), *Prefix, NextEntryID++);
}

EInsightDomain UNarrativeMemoryPipeline::CategorizeContent(const FString& Content) const
{
    const FString Lower = Content.ToLower();

    if (Lower.Contains(TEXT("learn")) || Lower.Contains(TEXT("discover")) || Lower.Contains(TEXT("understand")))
        return EInsightDomain::KnowledgeGrowth;

    if (Lower.Contains(TEXT("feel")) || Lower.Contains(TEXT("emotion")) || Lower.Contains(TEXT("mood")))
        return EInsightDomain::EmotionalPatterns;

    if (Lower.Contains(TEXT("friend")) || Lower.Contains(TEXT("social")) || Lower.Contains(TEXT("talk")) || Lower.Contains(TEXT("dialogue")))
        return EInsightDomain::RelationshipPatterns;

    if (Lower.Contains(TEXT("i ")) || Lower.Contains(TEXT("myself")) || Lower.Contains(TEXT("self")))
        return EInsightDomain::SelfUnderstanding;

    if (Lower.Contains(TEXT("tend")) || Lower.Contains(TEXT("habit")) || Lower.Contains(TEXT("always")) || Lower.Contains(TEXT("often")))
        return EInsightDomain::BehavioralTendencies;

    if (Lower.Contains(TEXT("wise")) || Lower.Contains(TEXT("wisdom")) || Lower.Contains(TEXT("insight")))
        return EInsightDomain::WisdomCultivation;

    if (Lower.Contains(TEXT("world")) || Lower.Contains(TEXT("environment")) || Lower.Contains(TEXT("place")))
        return EInsightDomain::EnvironmentalAwareness;

    return EInsightDomain::KnowledgeGrowth;
}

TArray<TPair<FString, int32>> UNarrativeMemoryPipeline::FindRecurringKeywords(
    const TArray<FDiaryEntry>& Entries) const
{
    TMap<FString, int32> Freq;

    // Minimal stop-word set
    static const TSet<FString> StopWords = {
        TEXT("the"), TEXT("a"), TEXT("an"), TEXT("and"), TEXT("or"), TEXT("but"),
        TEXT("in"), TEXT("on"), TEXT("at"), TEXT("to"), TEXT("for"), TEXT("of"),
        TEXT("with"), TEXT("is"), TEXT("was"), TEXT("are"), TEXT("were"), TEXT("i"),
        TEXT("it"), TEXT("this"), TEXT("that"), TEXT("my"), TEXT("me"), TEXT("you"),
    };

    for (const FDiaryEntry& Entry : Entries)
    {
        // Tokenise on whitespace and punctuation
        TArray<FString> Tokens;
        Entry.Content.ParseIntoArray(Tokens, TEXT(" "), true);

        for (FString& Token : Tokens)
        {
            // Strip leading/trailing non-alpha
            Token.TrimStartAndEndInline();
            // Lower-case
            Token = Token.ToLower();
            // Remove trailing punctuation
            while (!Token.IsEmpty() && !FChar::IsAlpha(Token[Token.Len() - 1]))
                Token.RemoveAt(Token.Len() - 1);

            if (Token.Len() < 3) continue;
            if (StopWords.Contains(Token)) continue;

            Freq.FindOrAdd(Token)++;
        }

        // Also process explicit tags
        for (const FString& Tag : Entry.Tags)
        {
            const FString LTag = Tag.ToLower();
            if (LTag.Len() >= 3 && !StopWords.Contains(LTag))
            {
                Freq.FindOrAdd(LTag)++;
            }
        }
    }

    // Sort descending by frequency
    TArray<TPair<FString, int32>> Sorted;
    for (const auto& Pair : Freq)
    {
        if (Pair.Value >= 2) Sorted.Add(Pair);
    }
    Algo::Sort(Sorted, [](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
    {
        return A.Value > B.Value;
    });

    return Sorted;
}

float UNarrativeMemoryPipeline::DetectEmotionalTrend(const TArray<FDiaryEntry>& Entries) const
{
    if (Entries.Num() < 2) return 0.0f;

    // Linear regression slope of EmotionalValence over index
    float SumX = 0.0f, SumY = 0.0f, SumXY = 0.0f, SumXX = 0.0f;
    float N = static_cast<float>(Entries.Num());

    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        float X = static_cast<float>(i);
        float Y = Entries[i].EmotionalValence;
        SumX  += X;
        SumY  += Y;
        SumXY += X * Y;
        SumXX += X * X;
    }

    float Denom = N * SumXX - SumX * SumX;
    if (FMath::IsNearlyZero(Denom)) return 0.0f;

    float Slope = (N * SumXY - SumX * SumY) / Denom;
    // Normalise slope so it is bounded roughly ±1 across typical window sizes
    return FMath::Clamp(Slope * N, -1.0f, 1.0f);
}

float UNarrativeMemoryPipeline::DetectEmotionalVolatility(const TArray<FDiaryEntry>& Entries) const
{
    if (Entries.Num() < 2) return 0.0f;

    // Compute mean
    float Mean = 0.0f;
    for (const FDiaryEntry& E : Entries) Mean += E.EmotionalValence;
    Mean /= Entries.Num();

    // Compute population standard deviation
    float Variance = 0.0f;
    for (const FDiaryEntry& E : Entries)
    {
        float Diff = E.EmotionalValence - Mean;
        Variance += Diff * Diff;
    }
    Variance /= Entries.Num();

    return FMath::Sqrt(Variance);
}

void UNarrativeMemoryPipeline::PruneDiary()
{
    if (DiaryEntries.Num() <= MaxDiaryEntries) return;

    int32 ToRemove = DiaryEntries.Num() - MaxDiaryEntries;

    // Reduce unprocessed count by number of unprocessed entries being removed
    // (oldest entries are most likely to be unprocessed if insight gen hasn't run)
    UnprocessedDiaryCount = FMath::Max(0, UnprocessedDiaryCount - ToRemove);

    DiaryEntries.RemoveAt(0, ToRemove, /*bAllowShrinking=*/false);
}

void UNarrativeMemoryPipeline::PruneInsights()
{
    if (Insights.Num() <= MaxInsights) return;

    int32 ToRemove = Insights.Num() - MaxInsights;

    // Remove lowest-confidence entries without reordering the array.
    // Find and remove them one at a time to preserve insertion order (recency).
    for (int32 i = 0; i < ToRemove; ++i)
    {
        int32 WeakestIdx = 0;
        float WeakestConf = Insights[0].Confidence;
        for (int32 j = 1; j < Insights.Num(); ++j)
        {
            if (Insights[j].Confidence < WeakestConf)
            {
                WeakestConf = Insights[j].Confidence;
                WeakestIdx = j;
            }
        }
        Insights.RemoveAt(WeakestIdx, 1, /*bAllowShrinking=*/false);
    }
}

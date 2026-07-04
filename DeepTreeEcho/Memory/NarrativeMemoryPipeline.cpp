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
#include "Algo/Sort.h"
#include "Algo/Reverse.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformTime.h"

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
        GenerateInsights();
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
    TArray<FInsightEntry> NewInsights;

    // Work only on the entries that arrived since the last run
    int32 RecentCount = FMath::Min(UnprocessedDiaryCount, DiaryEntries.Num());
    if (RecentCount < InsightThreshold)
    {
        return NewInsights;
    }

    // Slice of recent entries
    TArray<FDiaryEntry> RecentEntries;
    const int32 StartIdx = FMath::Max(0, DiaryEntries.Num() - RecentCount);
    for (int32 i = StartIdx; i < DiaryEntries.Num(); ++i)
    {
        RecentEntries.Add(DiaryEntries[i]);
    }

    // ── Keyword pattern insight ───────────────────────────────────────────
    auto Keywords = FindRecurringKeywords(RecentEntries);
    if (Keywords.Num() > 0)
    {
        const auto& Top = Keywords[0];
        FInsightEntry Insight;
        Insight.InsightID  = GenerateID(TEXT("insight"));
        Insight.Timestamp  = FDateTime::UtcNow();
        Insight.Pattern    = FString::Printf(TEXT("Recurring theme: '%s' (%d occurrences)"), *Top.Key, Top.Value);
        Insight.Description = FString::Printf(
            TEXT("The concept '%s' appears frequently in recent experiences, suggesting its importance to current goals or concerns."),
            *Top.Key);
        Insight.Confidence = FMath::Clamp(static_cast<float>(Top.Value) / RecentCount, 0.1f, 1.0f);
        Insight.Domain     = CategorizeContent(Top.Key);
        for (const FDiaryEntry& E : RecentEntries) Insight.SourceEntryIDs.Add(E.EntryID);
        Insight.ReinforcementCount = 1;

        Insights.Add(Insight);
        NewInsights.Add(Insight);
        UnprocessedInsightCount++;
    }

    // ── Emotional trend insight ───────────────────────────────────────────
    float EmotionalTrend = DetectEmotionalTrend(RecentEntries);
    if (FMath::Abs(EmotionalTrend) > 0.2f)
    {
        FInsightEntry EmotionalInsight;
        EmotionalInsight.InsightID   = GenerateID(TEXT("insight"));
        EmotionalInsight.Timestamp   = FDateTime::UtcNow();
        EmotionalInsight.Pattern     = EmotionalTrend > 0.0f
            ? TEXT("Positive emotional trajectory")
            : TEXT("Negative emotional trajectory");
        EmotionalInsight.Description = FString::Printf(
            TEXT("Emotional valence has been trending %s (delta: %+.2f) over the last %d experiences."),
            EmotionalTrend > 0.0f ? TEXT("upward") : TEXT("downward"),
            EmotionalTrend, RecentCount);
        EmotionalInsight.Confidence   = FMath::Clamp(FMath::Abs(EmotionalTrend), 0.1f, 1.0f);
        EmotionalInsight.Domain       = EInsightDomain::EmotionalPatterns;
        for (const FDiaryEntry& E : RecentEntries) EmotionalInsight.SourceEntryIDs.Add(E.EntryID);
        EmotionalInsight.ReinforcementCount = 1;

        Insights.Add(EmotionalInsight);
        NewInsights.Add(EmotionalInsight);
        UnprocessedInsightCount++;
    }

    // ── Volatility insight ────────────────────────────────────────────────
    float Volatility = DetectEmotionalVolatility(RecentEntries);
    if (Volatility > 0.4f)
    {
        FInsightEntry VolInsight;
        VolInsight.InsightID   = GenerateID(TEXT("insight"));
        VolInsight.Timestamp   = FDateTime::UtcNow();
        VolInsight.Pattern     = TEXT("High emotional volatility");
        VolInsight.Description = FString::Printf(
            TEXT("Emotional state has been highly volatile (σ=%.2f). This may indicate unresolved stress or significant external stimuli."),
            Volatility);
        VolInsight.Confidence  = FMath::Clamp(Volatility, 0.1f, 1.0f);
        VolInsight.Domain      = EInsightDomain::EmotionalPatterns;
        for (const FDiaryEntry& E : RecentEntries) VolInsight.SourceEntryIDs.Add(E.EntryID);
        VolInsight.ReinforcementCount = 1;

        Insights.Add(VolInsight);
        NewInsights.Add(VolInsight);
        UnprocessedInsightCount++;
    }

    // Wisdom grows with insight accumulation
    WisdomLevel += NewInsights.Num() * 0.01f;
    WisdomLevel  = FMath::Clamp(WisdomLevel, 0.0f, 1.0f);

    // Only mark diary entries as processed if we actually generated insights
    if (NewInsights.Num() > 0)
    {
        UnprocessedDiaryCount = 0;
    }

    if (Insights.Num() > MaxInsights)
    {
        PruneInsights();
    }

    // Trigger blog post if enough unprocessed insights have accumulated
    if (UnprocessedInsightCount >= BlogThreshold)
    {
        GenerateBlogPost();
    }

    return NewInsights;
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
    FBlogPost Post;

    // Gather the most recent unprocessed insights
    int32 StartIdx = FMath::Max(0, Insights.Num() - UnprocessedInsightCount);
    TArray<FInsightEntry> SourceInsights;
    for (int32 i = StartIdx; i < Insights.Num(); ++i)
    {
        SourceInsights.Add(Insights[i]);
    }

    Post.PostID    = GenerateID(TEXT("blog"));
    Post.Timestamp = FDateTime::UtcNow();

    // Determine dominant domain
    TMap<EInsightDomain, int32> DomainCount;
    for (const FInsightEntry& I : SourceInsights)
    {
        DomainCount.FindOrAdd(I.Domain)++;
        Post.SourceInsightIDs.Add(I.InsightID);
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
    Post.DominantDomain = Dominant;

    // Build human-readable title
    static const TCHAR* DomainNames[] = {
        TEXT("Self"),
        TEXT("Relationships"),
        TEXT("Knowledge"),
        TEXT("Emotions"),
        TEXT("Behaviour"),
        TEXT("Wisdom"),
        TEXT("Environment")
    };
    const TCHAR* DomainLabel = DomainNames[static_cast<int32>(Dominant)];
    Post.Title = FString::Printf(TEXT("Reflections on %s — WisdomLevel %.2f"), DomainLabel, WisdomLevel);

    // Compose content from source insights
    FString ContentBuilder;
    ContentBuilder += FString::Printf(TEXT("## %s\n\n"), *Post.Title);
    ContentBuilder += FString::Printf(TEXT("*Generated at wisdom level %.2f*\n\n"), WisdomLevel);
    for (const FInsightEntry& I : SourceInsights)
    {
        ContentBuilder += FString::Printf(TEXT("### %s\n%s\n\n"), *I.Pattern, *I.Description);
    }
    Post.Content = ContentBuilder;

    // Worldview update summarises any emotional trend found
    float AvgConfidence = 0.0f;
    for (const FInsightEntry& I : SourceInsights) AvgConfidence += I.Confidence;
    if (!SourceInsights.IsEmpty()) AvgConfidence /= SourceInsights.Num();

    Post.WorldviewUpdate = FString::Printf(
        TEXT("Average insight confidence: %.2f. Dominant concern: %s."),
        AvgConfidence, DomainLabel);
    Post.WisdomLevel = WisdomLevel;

    BlogPosts.Add(Post);
    UnprocessedInsightCount = 0;

    return Post;
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

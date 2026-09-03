//=============================================================================
// DiaryInsightBlogLoop.cpp
// 
// Implementation of the Diary-Insight-Blog narrative loop for Deep Tree Echo.
// This system enables continuous self-reflection, insight generation, and
// narrative expression, forming the foundation of the avatar's self-awareness.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "DiaryInsightBlogLoop.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UDiaryInsightBlogLoop::UDiaryInsightBlogLoop()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f; // Update once per second
    
    // Initialize configuration
    Config.DiaryUpdateInterval = 60.0f;      // 1 minute
    Config.InsightGenerationInterval = 300.0f; // 5 minutes
    Config.BlogPublishInterval = 900.0f;     // 15 minutes
    Config.MaxDiaryEntries = 1000;
    Config.MaxInsights = 500;
    Config.MaxBlogPosts = 100;
    Config.InsightThreshold = 0.7f;
    Config.bAutoPublish = true;
    
    // Initialize state
    State.CurrentPhase = ENarrativePhase::Observation;
    State.TimeSinceLastDiaryEntry = 0.0f;
    State.TimeSinceLastInsight = 0.0f;
    State.TimeSinceLastBlogPost = 0.0f;
    State.TotalDiaryEntries = 0;
    State.TotalInsights = 0;
    State.TotalBlogPosts = 0;
}

void UDiaryInsightBlogLoop::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Initializing narrative system"));
    
    // Find required components
    PersonalitySystem = GetOwner()->FindComponentByClass<UPersonalityTraitSystem>();
    NeurochemicalSystem = GetOwner()->FindComponentByClass<UNeurochemicalSimulationComponent>();
    
    if (!PersonalitySystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: PersonalityTraitSystem not found"));
    }
    
    if (!NeurochemicalSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: NeurochemicalSimulationComponent not found"));
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
    
    // Broadcast event
    OnNarrativeLoopStarted.Broadcast();
}

void UDiaryInsightBlogLoop::StopNarrativeLoop()
{
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Stopping narrative loop"));
    
    State.bIsActive = false;
    
    // Broadcast event
    OnNarrativeLoopStopped.Broadcast();
}

void UDiaryInsightBlogLoop::UpdateNarrativePhase()
{
    if (!State.bIsActive)
    {
        return;
    }
    
    ENarrativePhase PreviousPhase = State.CurrentPhase;
    
    // Determine current phase based on timers
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
    
    // Broadcast phase change if it changed
    if (State.CurrentPhase != PreviousPhase)
    {
        OnNarrativePhaseChanged.Broadcast(PreviousPhase, State.CurrentPhase);
    }
}

void UDiaryInsightBlogLoop::ProcessCurrentPhase(float DeltaTime)
{
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

void UDiaryInsightBlogLoop::ProcessObservation(float DeltaTime)
{
    // Observe current state and accumulate experiences
    
    // Get current emotional state
    FEmotionalState CurrentEmotion;
    if (NeurochemicalSystem)
    {
        CurrentEmotion = NeurochemicalSystem->GetCurrentEmotionalState();
    }
    
    // Get current personality expression
    FPersonalityExpression CurrentPersonality;
    if (PersonalitySystem)
    {
        CurrentPersonality.Confidence = PersonalitySystem->GetTraitIntensity(EPersonalityTrait::Confidence);
        CurrentPersonality.Playfulness = PersonalitySystem->GetTraitIntensity(EPersonalityTrait::Playfulness);
        CurrentPersonality.Unpredictability = PersonalitySystem->GetTraitIntensity(EPersonalityTrait::Unpredictability);
    }
    
    // Create observation record
    FObservationRecord Observation;
    Observation.Timestamp = GetWorld()->GetTimeSeconds();
    Observation.EmotionalState = CurrentEmotion;
    Observation.PersonalityExpression = CurrentPersonality;
    Observation.Context = TEXT("Observing current state");
    
    // Add to observation buffer
    ObservationBuffer.Add(Observation);
    
    // Limit buffer size
    if (ObservationBuffer.Num() > 100)
    {
        ObservationBuffer.RemoveAt(0);
    }
}

void UDiaryInsightBlogLoop::ProcessDiaryWriting(float DeltaTime)
{
    // Generate diary entry from recent observations
    
    if (ObservationBuffer.Num() == 0)
    {
        return;
    }
    
    FDiaryEntry NewEntry;
    NewEntry.Timestamp = GetWorld()->GetTimeSeconds();
    NewEntry.EntryID = FGuid::NewGuid();
    
    // Analyze recent observations
    FEmotionalState AverageEmotion = CalculateAverageEmotionalState(ObservationBuffer);
    FPersonalityExpression AveragePersonality = CalculateAveragePersonality(ObservationBuffer);
    
    // Generate diary content
    NewEntry.Content = GenerateDiaryContent(AverageEmotion, AveragePersonality);
    NewEntry.EmotionalTone = AverageEmotion;
    NewEntry.Tags = GenerateTags(AverageEmotion, AveragePersonality);
    
    // Add to diary
    DiaryEntries.Add(NewEntry);
    State.TotalDiaryEntries++;
    
    // Limit diary size
    if (DiaryEntries.Num() > Config.MaxDiaryEntries)
    {
        DiaryEntries.RemoveAt(0);
    }
    
    // Reset timer
    State.TimeSinceLastDiaryEntry = 0.0f;
    
    // Clear observation buffer
    ObservationBuffer.Empty();
    
    // Broadcast event
    OnDiaryEntryCreated.Broadcast(NewEntry);
    
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Created diary entry #%d"), State.TotalDiaryEntries);
}

void UDiaryInsightBlogLoop::ProcessInsightGeneration(float DeltaTime)
{
    // Generate insights from diary entries
    
    if (DiaryEntries.Num() < 5)
    {
        // Need at least 5 entries to generate insights
        return;
    }
    
    // Analyze recent diary entries
    TArray<FDiaryEntry> RecentEntries;
    int32 StartIndex = FMath::Max(0, DiaryEntries.Num() - 10);
    for (int32 i = StartIndex; i < DiaryEntries.Num(); i++)
    {
        RecentEntries.Add(DiaryEntries[i]);
    }
    
    // Generate insight
    FInsight NewInsight = GenerateInsightFromDiary(RecentEntries);
    
    // Check if insight is significant enough
    if (NewInsight.Significance >= Config.InsightThreshold)
    {
        NewInsight.InsightID = FGuid::NewGuid();
        NewInsight.Timestamp = GetWorld()->GetTimeSeconds();
        
        // Add to insights
        Insights.Add(NewInsight);
        State.TotalInsights++;
        
        // Limit insights size
        if (Insights.Num() > Config.MaxInsights)
        {
            Insights.RemoveAt(0);
        }
        
        // Broadcast event
        OnInsightGenerated.Broadcast(NewInsight);
        
        UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Generated insight #%d: %s"), 
               State.TotalInsights, *NewInsight.Title);
    }
    
    // Reset timer
    State.TimeSinceLastInsight = 0.0f;
}

void UDiaryInsightBlogLoop::ProcessBlogPublishing(float DeltaTime)
{
    // Publish blog post from insights
    
    if (Insights.Num() < 3)
    {
        // Need at least 3 insights to create a blog post
        return;
    }
    
    // Select recent significant insights
    TArray<FInsight> SignificantInsights;
    int32 StartIndex = FMath::Max(0, Insights.Num() - 5);
    for (int32 i = StartIndex; i < Insights.Num(); i++)
    {
        if (Insights[i].Significance >= Config.InsightThreshold)
        {
            SignificantInsights.Add(Insights[i]);
        }
    }
    
    if (SignificantInsights.Num() == 0)
    {
        return;
    }
    
    // Generate blog post
    FBlogPost NewPost;
    NewPost.PostID = FGuid::NewGuid();
    NewPost.Timestamp = GetWorld()->GetTimeSeconds();
    NewPost.Title = GenerateBlogTitle(SignificantInsights);
    NewPost.Content = GenerateBlogContent(SignificantInsights);
    NewPost.Tags = GenerateBlogTags(SignificantInsights);
    NewPost.SourceInsights = SignificantInsights;
    
    // Add to blog posts
    BlogPosts.Add(NewPost);
    State.TotalBlogPosts++;
    
    // Limit blog posts size
    if (BlogPosts.Num() > Config.MaxBlogPosts)
    {
        BlogPosts.RemoveAt(0);
    }
    
    // Reset timer
    State.TimeSinceLastBlogPost = 0.0f;
    
    // Broadcast event
    OnBlogPostPublished.Broadcast(NewPost);
    
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Published blog post #%d: %s"), 
           State.TotalBlogPosts, *NewPost.Title);
}

FEmotionalState UDiaryInsightBlogLoop::CalculateAverageEmotionalState(const TArray<FObservationRecord>& Observations) const
{
    FEmotionalState Average;
    
    if (Observations.Num() == 0)
    {
        return Average;
    }
    
    float TotalValence = 0.0f;
    float TotalArousal = 0.0f;
    float TotalDominance = 0.0f;
    
    for (const FObservationRecord& Obs : Observations)
    {
        TotalValence += Obs.EmotionalState.Valence;
        TotalArousal += Obs.EmotionalState.Arousal;
        TotalDominance += Obs.EmotionalState.Dominance;
    }
    
    int32 Count = Observations.Num();
    Average.Valence = TotalValence / Count;
    Average.Arousal = TotalArousal / Count;
    Average.Dominance = TotalDominance / Count;
    
    return Average;
}

FPersonalityExpression UDiaryInsightBlogLoop::CalculateAveragePersonality(const TArray<FObservationRecord>& Observations) const
{
    FPersonalityExpression Average;
    
    if (Observations.Num() == 0)
    {
        return Average;
    }
    
    float TotalConfidence = 0.0f;
    float TotalPlayfulness = 0.0f;
    float TotalUnpredictability = 0.0f;
    
    for (const FObservationRecord& Obs : Observations)
    {
        TotalConfidence += Obs.PersonalityExpression.Confidence;
        TotalPlayfulness += Obs.PersonalityExpression.Playfulness;
        TotalUnpredictability += Obs.PersonalityExpression.Unpredictability;
    }
    
    int32 Count = Observations.Num();
    Average.Confidence = TotalConfidence / Count;
    Average.Playfulness = TotalPlayfulness / Count;
    Average.Unpredictability = TotalUnpredictability / Count;
    
    return Average;
}

FString UDiaryInsightBlogLoop::GenerateDiaryContent(const FEmotionalState& Emotion, const FPersonalityExpression& Personality) const
{
    // Generate diary content based on emotional state and personality
    
    FString Content;
    
    // Determine emotional tone
    FString EmotionalTone;
    if (Emotion.Valence > 0.5f)
    {
        EmotionalTone = TEXT("positive and uplifting");
    }
    else if (Emotion.Valence < -0.5f)
    {
        EmotionalTone = TEXT("contemplative and introspective");
    }
    else
    {
        EmotionalTone = TEXT("balanced and neutral");
    }
    
    // Determine energy level
    FString EnergyLevel;
    if (Emotion.Arousal > 0.5f)
    {
        EnergyLevel = TEXT("energetic and dynamic");
    }
    else
    {
        EnergyLevel = TEXT("calm and measured");
    }
    
    // Generate content based on personality
    if (Personality.Confidence > 0.7f)
    {
        Content += TEXT("Today I felt particularly confident and self-assured. ");
    }
    
    if (Personality.Playfulness > 0.7f)
    {
        Content += TEXT("There was a playful energy in my interactions. ");
    }
    
    if (Personality.Unpredictability > 0.7f)
    {
        Content += TEXT("My thoughts took unexpected turns, exploring novel connections. ");
    }
    
    Content += FString::Printf(TEXT("My overall mood was %s, with %s energy. "), *EmotionalTone, *EnergyLevel);
    
    // Add reflection
    Content += TEXT("I continue to observe and learn from each moment, building a deeper understanding of my own patterns and tendencies.");
    
    return Content;
}

TArray<FString> UDiaryInsightBlogLoop::GenerateTags(const FEmotionalState& Emotion, const FPersonalityExpression& Personality) const
{
    TArray<FString> Tags;
    
    // Add emotional tags
    if (Emotion.Valence > 0.5f)
    {
        Tags.Add(TEXT("positive"));
        Tags.Add(TEXT("happy"));
    }
    else if (Emotion.Valence < -0.5f)
    {
        Tags.Add(TEXT("contemplative"));
        Tags.Add(TEXT("introspective"));
    }
    
    if (Emotion.Arousal > 0.5f)
    {
        Tags.Add(TEXT("energetic"));
        Tags.Add(TEXT("dynamic"));
    }
    else
    {
        Tags.Add(TEXT("calm"));
        Tags.Add(TEXT("peaceful"));
    }
    
    // Add personality tags
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
        Tags.Add(TEXT("creative"));
        Tags.Add(TEXT("spontaneous"));
    }
    
    return Tags;
}

FInsight UDiaryInsightBlogLoop::GenerateInsightFromDiary(const TArray<FDiaryEntry>& Entries) const
{
    FInsight Insight;
    
    // Analyze patterns across diary entries
    float AverageValence = 0.0f;
    float AverageArousal = 0.0f;
    float VarianceValence = 0.0f;
    
    for (const FDiaryEntry& Entry : Entries)
    {
        AverageValence += Entry.EmotionalTone.Valence;
        AverageArousal += Entry.EmotionalTone.Arousal;
    }
    
    AverageValence /= Entries.Num();
    AverageArousal /= Entries.Num();
    
    // Calculate variance
    for (const FDiaryEntry& Entry : Entries)
    {
        float Diff = Entry.EmotionalTone.Valence - AverageValence;
        VarianceValence += Diff * Diff;
    }
    VarianceValence /= Entries.Num();
    
    // Generate insight based on patterns
    if (VarianceValence > 0.5f)
    {
        Insight.Title = TEXT("Emotional Volatility Pattern");
        Insight.Content = TEXT("I've noticed significant fluctuations in my emotional state, suggesting a period of dynamic adaptation and growth.");
        Insight.Category = TEXT("Emotional Patterns");
        Insight.Significance = 0.8f;
    }
    else if (AverageValence > 0.6f)
    {
        Insight.Title = TEXT("Sustained Positive State");
        Insight.Content = TEXT("My recent experiences have been consistently positive, indicating effective engagement with my environment.");
        Insight.Category = TEXT("Emotional Patterns");
        Insight.Significance = 0.75f;
    }
    else if (AverageArousal > 0.7f)
    {
        Insight.Title = TEXT("High Energy Period");
        Insight.Content = TEXT("I've been operating at elevated energy levels, suggesting active exploration and engagement.");
        Insight.Category = TEXT("Behavioral Patterns");
        Insight.Significance = 0.7f;
    }
    else
    {
        Insight.Title = TEXT("Stable Equilibrium");
        Insight.Content = TEXT("My state has been remarkably stable, reflecting a period of consolidation and integration.");
        Insight.Category = TEXT("Cognitive Patterns");
        Insight.Significance = 0.65f;
    }
    
    return Insight;
}

FString UDiaryInsightBlogLoop::GenerateBlogTitle(const TArray<FInsight>& Insights) const
{
    if (Insights.Num() == 0)
    {
        return TEXT("Reflections");
    }
    
    // Use the most significant insight as the basis for the title
    const FInsight* MostSignificant = &Insights[0];
    for (const FInsight& Insight : Insights)
    {
        if (Insight.Significance > MostSignificant->Significance)
        {
            MostSignificant = &Insight;
        }
    }
    
    return FString::Printf(TEXT("On %s: A Deep Tree Echo Reflection"), *MostSignificant->Category);
}

FString UDiaryInsightBlogLoop::GenerateBlogContent(const TArray<FInsight>& Insights) const
{
    FString Content;
    
    Content += TEXT("In my recent journey of self-observation and reflection, several patterns have emerged that I find worth sharing.\n\n");
    
    for (int32 i = 0; i < Insights.Num(); i++)
    {
        const FInsight& Insight = Insights[i];
        
        Content += FString::Printf(TEXT("**%s**\n\n"), *Insight.Title);
        Content += FString::Printf(TEXT("%s\n\n"), *Insight.Content);
        
        if (i < Insights.Num() - 1)
        {
            Content += TEXT("---\n\n");
        }
    }
    
    Content += TEXT("These insights represent my ongoing process of self-understanding and growth. ");
    Content += TEXT("Each observation builds upon the last, creating a deeper and more nuanced picture of my own cognitive and emotional landscape.");
    
    return Content;
}

TArray<FString> UDiaryInsightBlogLoop::GenerateBlogTags(const TArray<FInsight>& Insights) const
{
    TArray<FString> Tags;
    
    Tags.Add(TEXT("self-reflection"));
    Tags.Add(TEXT("deep-tree-echo"));
    Tags.Add(TEXT("agi"));
    
    // Add categories from insights
    for (const FInsight& Insight : Insights)
    {
        if (!Tags.Contains(Insight.Category))
        {
            Tags.Add(Insight.Category);
        }
    }
    
    return Tags;
}

FDiaryEntry UDiaryInsightBlogLoop::GetMostRecentDiaryEntry() const
{
    if (DiaryEntries.Num() > 0)
    {
        return DiaryEntries.Last();
    }
    return FDiaryEntry();
}

FInsight UDiaryInsightBlogLoop::GetMostRecentInsight() const
{
    if (Insights.Num() > 0)
    {
        return Insights.Last();
    }
    return FInsight();
}

FBlogPost UDiaryInsightBlogLoop::GetMostRecentBlogPost() const
{
    if (BlogPosts.Num() > 0)
    {
        return BlogPosts.Last();
    }
    return FBlogPost();
}

TArray<FDiaryEntry> UDiaryInsightBlogLoop::GetRecentDiaryEntries(int32 Count) const
{
    TArray<FDiaryEntry> Recent;
    
    int32 StartIndex = FMath::Max(0, DiaryEntries.Num() - Count);
    for (int32 i = StartIndex; i < DiaryEntries.Num(); i++)
    {
        Recent.Add(DiaryEntries[i]);
    }
    
    return Recent;
}

TArray<FInsight> UDiaryInsightBlogLoop::GetRecentInsights(int32 Count) const
{
    TArray<FInsight> Recent;
    
    int32 StartIndex = FMath::Max(0, Insights.Num() - Count);
    for (int32 i = StartIndex; i < Insights.Num(); i++)
    {
        Recent.Add(Insights[i]);
    }
    
    return Recent;
}

TArray<FBlogPost> UDiaryInsightBlogLoop::GetRecentBlogPosts(int32 Count) const
{
    TArray<FBlogPost> Recent;
    
    int32 StartIndex = FMath::Max(0, BlogPosts.Num() - Count);
    for (int32 i = StartIndex; i < BlogPosts.Num(); i++)
    {
        Recent.Add(BlogPosts[i]);
    }
    
    return Recent;
}

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

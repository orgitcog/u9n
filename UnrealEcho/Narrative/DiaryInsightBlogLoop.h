#pragma once

/**
 * Diary-Insight-Blog Narrative Loop
 *
 * Continuous self-reflection for the avatar: observations of emotional and
 * personality state accumulate, are written up as diary entries, condensed
 * into insights and finally published as blog posts.
 *
 * Every piece of prose this component stores or publishes (diary entries,
 * insight titles and bodies, blog titles and bodies) is requested from the
 * language model through UDTELanguageModelBridge. The loop itself only
 * decides *when* to write and supplies the facts (averaged state readings,
 * recent entries, time spans) the model should draw on. When the model does
 * not answer, nothing is written or published for that occasion and an
 * operator-facing warning is logged; the "created"/"generated"/"published"
 * delegates fire only with model-produced text.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiaryInsightBlogLoop.generated.h"

class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;
class UDTELanguageModelBridge;

/**
 * Phases of the narrative loop
 */
UENUM(BlueprintType)
enum class ENarrativePhase : uint8
{
    Observation UMETA(DisplayName = "Observation"),
    DiaryWriting UMETA(DisplayName = "Diary Writing"),
    InsightGeneration UMETA(DisplayName = "Insight Generation"),
    BlogPublishing UMETA(DisplayName = "Blog Publishing")
};

/**
 * Affective reading derived from the neurochemical simulation.
 * Valence is -1..1, Arousal and Dominance are 0..1.
 */
USTRUCT(BlueprintType)
struct FNarrativeAffect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Valence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Arousal = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Dominance = 0.0f;
};

/**
 * Personality expression sampled from the trait system (0..1 each)
 */
USTRUCT(BlueprintType)
struct FNarrativePersonalityExpression
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Playfulness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Unpredictability = 0.0f;
};

/**
 * One observation of the avatar's state
 */
USTRUCT(BlueprintType)
struct FNarrativeObservation
{
    GENERATED_BODY()

    /** World time (seconds) when the observation was taken */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNarrativeAffect EmotionalState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNarrativePersonalityExpression PersonalityExpression;

    /** Optional free-form note supplied by other systems about what was happening */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Context;
};

/**
 * Diary entry written by the model from a stretch of observations
 */
USTRUCT(BlueprintType)
struct FNarrativeDiaryEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid EntryID;

    /** World time (seconds) when the entry was requested */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Model-written entry text */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNarrativeAffect EmotionalTone;

    /** Classification labels derived from the state readings (not prose) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;
};

/**
 * Insight distilled by the model from recent diary entries
 */
USTRUCT(BlueprintType)
struct FNarrativeInsight
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid InsightID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Model-written title */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    /** Model-written insight text */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    /** Pattern family the statistics placed this insight in (classification label) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Category;

    /** 0..1, computed from the variance / level of the source entries */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Significance = 0.0f;
};

/**
 * Blog post composed by the model from significant insights
 */
USTRUCT(BlueprintType)
struct FNarrativeBlogPost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid PostID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Model-written title */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    /** Model-written body */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FNarrativeInsight> SourceInsights;
};

/**
 * Loop timing and capacity configuration
 */
USTRUCT(BlueprintType)
struct FNarrativeLoopConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DiaryUpdateInterval = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InsightGenerationInterval = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BlogPublishInterval = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxDiaryEntries = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxInsights = 500;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxBlogPosts = 100;

    /** Minimum insight significance for storage and publication */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InsightThreshold = 0.7f;

    /** When true, blog posts are requested from the model and published automatically */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoPublish = true;
};

/**
 * Runtime state of the loop
 */
USTRUCT(BlueprintType)
struct FNarrativeLoopState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ENarrativePhase CurrentPhase = ENarrativePhase::Observation;

    UPROPERTY(BlueprintReadOnly)
    bool bIsActive = false;

    UPROPERTY(BlueprintReadOnly)
    float TimeSinceLastDiaryEntry = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float TimeSinceLastInsight = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float TimeSinceLastBlogPost = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalDiaryEntries = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalInsights = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalBlogPosts = 0;

    /** A diary entry has been requested from the model and has not yet arrived */
    UPROPERTY(BlueprintReadOnly)
    bool bDiaryEntryPending = false;

    /** An insight has been requested from the model and has not yet arrived */
    UPROPERTY(BlueprintReadOnly)
    bool bInsightPending = false;

    /** A blog post has been requested from the model and has not yet arrived */
    UPROPERTY(BlueprintReadOnly)
    bool bBlogPostPending = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNarrativeLoopStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNarrativeLoopStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNarrativePhaseChanged, ENarrativePhase, PreviousPhase, ENarrativePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeDiaryEntryCreated, const FNarrativeDiaryEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeInsightGenerated, const FNarrativeInsight&, Insight);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeBlogPostPublished, const FNarrativeBlogPost&, Post);

/**
 * Diary-Insight-Blog Narrative Loop Component
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UDiaryInsightBlogLoop : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiaryInsightBlogLoop();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Loop Management =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void StartNarrativeLoop();

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void StopNarrativeLoop();

    UFUNCTION(BlueprintPure, Category = "Narrative|Loop")
    FNarrativeLoopState GetNarrativeState() const { return State; }

    UFUNCTION(BlueprintPure, Category = "Narrative|Loop")
    FNarrativeLoopConfig GetNarrativeConfig() const { return Config; }

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void SetDiaryUpdateInterval(float Interval);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void SetInsightGenerationInterval(float Interval);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void SetBlogPublishInterval(float Interval);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void SetAutoPublish(bool bEnabled);

    /** Make the next tick enter the diary-writing phase */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void ForceDiaryEntry();

    /** Make the next tick enter the insight-generation phase */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void ForceInsightGeneration();

    /** Make the next tick enter the blog-publishing phase */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void ForceBlogPublish();

    // ===== Model Requests =====
    // Each returns true when a request was sent. The text arrives later, on the
    // game thread, through OnDiaryEntryCreated / OnInsightGenerated /
    // OnBlogPostPublished. Nothing is stored or broadcast when the model fails.

    /** Ask the model to write a diary entry from the buffered observations. */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Diary")
    bool RequestDiaryEntry();

    /** Ask the model to distil an insight from the most recent diary entries. */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Insight")
    bool RequestInsight();

    /** Ask the model to compose a blog post from recent significant insights. */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Blog")
    bool RequestBlogPost();

    // ===== Access =====

    UFUNCTION(BlueprintPure, Category = "Narrative|Diary")
    FNarrativeDiaryEntry GetMostRecentDiaryEntry() const;

    UFUNCTION(BlueprintPure, Category = "Narrative|Insight")
    FNarrativeInsight GetMostRecentInsight() const;

    UFUNCTION(BlueprintPure, Category = "Narrative|Blog")
    FNarrativeBlogPost GetMostRecentBlogPost() const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Diary")
    TArray<FNarrativeDiaryEntry> GetRecentDiaryEntries(int32 Count) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Insight")
    TArray<FNarrativeInsight> GetRecentInsights(int32 Count) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Blog")
    TArray<FNarrativeBlogPost> GetRecentBlogPosts(int32 Count) const;

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeLoopStarted OnNarrativeLoopStarted;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeLoopStopped OnNarrativeLoopStopped;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativePhaseChanged OnNarrativePhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeDiaryEntryCreated OnDiaryEntryCreated;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeInsightGenerated OnInsightGenerated;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeBlogPostPublished OnBlogPostPublished;

protected:
    // ===== Phase processing =====

    void UpdateNarrativePhase();
    void ProcessCurrentPhase(float DeltaTime);
    void ProcessObservation(float DeltaTime);
    void ProcessDiaryWriting(float DeltaTime);
    void ProcessInsightGeneration(float DeltaTime);
    void ProcessBlogPublishing(float DeltaTime);

    // ===== Numeric helpers (state, not prose) =====

    FNarrativeAffect SampleAffect() const;
    FNarrativePersonalityExpression SamplePersonality() const;
    FNarrativeAffect CalculateAverageEmotionalState(const TArray<FNarrativeObservation>& Observations) const;
    FNarrativePersonalityExpression CalculateAveragePersonality(const TArray<FNarrativeObservation>& Observations) const;
    TArray<FString> GenerateTags(const FNarrativeAffect& Emotion, const FNarrativePersonalityExpression& Personality) const;
    TArray<FString> GenerateBlogTags(const TArray<FNarrativeInsight>& SourceInsights) const;

    /** Places recent entries in a pattern family and scores it; fills Category and Significance only. */
    void ClassifyInsightPattern(const TArray<FNarrativeDiaryEntry>& Entries, FNarrativeInsight& OutInsight,
                                float& OutAverageValence, float& OutAverageArousal, float& OutValenceVariance) const;

    /** Splits a model reply of the form "title line, blank line, body" into its parts. */
    static bool SplitTitleAndBody(const FString& ModelText, FString& OutTitle, FString& OutBody);

    static FString FormatAffect(const FNarrativeAffect& Affect);
    static FString FormatPersonality(const FNarrativePersonalityExpression& Personality);

    float CurrentWorldTime() const;

    // ===== Linked components =====

    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem = nullptr;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem = nullptr;

    /** The only source of diary, insight and blog text. Resolved in BeginPlay. */
    UPROPERTY()
    UDTELanguageModelBridge* ModelBridge = nullptr;

    // ===== Configuration and state =====

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
    FNarrativeLoopConfig Config;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FNarrativeLoopState State;

    // ===== Narrative data =====

    UPROPERTY()
    TArray<FNarrativeObservation> ObservationBuffer;

    UPROPERTY()
    TArray<FNarrativeDiaryEntry> DiaryEntries;

    UPROPERTY()
    TArray<FNarrativeInsight> Insights;

    UPROPERTY()
    TArray<FNarrativeBlogPost> BlogPosts;
};

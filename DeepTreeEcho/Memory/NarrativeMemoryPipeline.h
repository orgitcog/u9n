#pragma once

/**
 * Narrative Memory Pipeline for Deep Tree Echo
 *
 * Implements the Diary -> Insight -> Blog narrative loop that gives
 * the cognitive system a persistent, evolving self-narrative.
 *
 * Stages:
 * 1. DIARY: Raw experience recording with emotional context and tags
 * 2. INSIGHT: Pattern extraction across accumulated diary entries
 * 3. BLOG: Worldview synthesis from accumulated insights
 *
 * The statistics (recurring keywords, emotional trend, volatility, dominant
 * domain, confidence, wisdom level) are computed here. The words of every
 * insight and blog post are requested from the language model through
 * UDTELanguageModelBridge with those statistics and the source entries as
 * context. Insights and posts are stored, and the Diary -> Insight -> Blog
 * state advances, only when the model produced the text; when it does not,
 * an operator-facing warning is logged and nothing is recorded.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeMemoryPipeline.generated.h"

class UEpisodicMemorySystem;
class UHypergraphMemorySystem;
class UDTELanguageModelBridge;

UENUM(BlueprintType)
enum class EInsightDomain : uint8
{
	SelfUnderstanding        UMETA(DisplayName = "Self Understanding"),
	RelationshipPatterns     UMETA(DisplayName = "Relationship Patterns"),
	KnowledgeGrowth          UMETA(DisplayName = "Knowledge Growth"),
	EmotionalPatterns        UMETA(DisplayName = "Emotional Patterns"),
	BehavioralTendencies     UMETA(DisplayName = "Behavioral Tendencies"),
	WisdomCultivation        UMETA(DisplayName = "Wisdom Cultivation"),
	EnvironmentalAwareness   UMETA(DisplayName = "Environmental Awareness"),
};

USTRUCT(BlueprintType)
struct FDiaryEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString EntryID;
	UPROPERTY(BlueprintReadOnly) FDateTime Timestamp;
	UPROPERTY(BlueprintReadOnly) FString Content;
	UPROPERTY(BlueprintReadOnly) float EmotionalValence = 0.0f;
	UPROPERTY(BlueprintReadOnly) float ArousalLevel = 0.0f;
	UPROPERTY(BlueprintReadOnly) float CognitiveLoad = 0.0f;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Tags;
	UPROPERTY(BlueprintReadOnly) FString SourceEventType;
};

USTRUCT(BlueprintType)
struct FInsightEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString InsightID;
	UPROPERTY(BlueprintReadOnly) FDateTime Timestamp;
	UPROPERTY(BlueprintReadOnly) FString Pattern;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) float Confidence = 0.0f;
	UPROPERTY(BlueprintReadOnly) EInsightDomain Domain = EInsightDomain::KnowledgeGrowth;
	UPROPERTY(BlueprintReadOnly) TArray<FString> SourceEntryIDs;
	UPROPERTY(BlueprintReadOnly) int32 ReinforcementCount = 1;
};

USTRUCT(BlueprintType)
struct FBlogPost
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString PostID;
	UPROPERTY(BlueprintReadOnly) FDateTime Timestamp;
	UPROPERTY(BlueprintReadOnly) FString Title;
	UPROPERTY(BlueprintReadOnly) FString Content;
	UPROPERTY(BlueprintReadOnly) FString WorldviewUpdate;
	UPROPERTY(BlueprintReadOnly) TArray<FString> SourceInsightIDs;
	UPROPERTY(BlueprintReadOnly) float WisdomLevel = 0.0f;
	UPROPERTY(BlueprintReadOnly) EInsightDomain DominantDomain = EInsightDomain::KnowledgeGrowth;
};

USTRUCT(BlueprintType)
struct FNarrativeState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 DiaryCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 InsightCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 BlogCount = 0;
	UPROPERTY(BlueprintReadOnly) float CurrentWisdomLevel = 0.0f;
	UPROPERTY(BlueprintReadOnly) float DominantEmotionalTone = 0.0f;
	UPROPERTY(BlueprintReadOnly) TArray<FString> TopPatterns;
};

/** Fired when the model has written an insight and it has been stored. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPipelineInsightGenerated, const FInsightEntry&, Insight);

/** Fired when the model has written a blog post and it has been stored. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPipelineBlogPostGenerated, const FBlogPost&, Post);

UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class UNarrativeMemoryPipeline : public UActorComponent
{
	GENERATED_BODY()

public:
	UNarrativeMemoryPipeline();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	FDiaryEntry RecordDiaryEntry(const FString& Content, float EmotionalValence, float ArousalLevel, const FString& SourceEventType, const TArray<FString>& Tags);

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	FDiaryEntry RecordDialogueEvent(const FString& SpeakerName, const FString& DialogueContent, float EmotionalImpact);

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	FDiaryEntry RecordDiscoveryEvent(const FString& DiscoveryDescription, const TArray<FString>& RelatedConcepts);

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	FDiaryEntry RecordSocialEvent(const FString& InteractionDescription, const FString& OtherActorName, float RelationshipDelta);

	/**
	 * Analyse the unprocessed diary entries and ask the model to write one insight
	 * per pattern found. Insights arrive asynchronously through OnInsightGenerated.
	 * Returns the number of insight requests sent (0 when nothing qualified, a
	 * round is already in flight, or no model bridge is available).
	 */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	int32 RequestInsights();

	/**
	 * Compatibility wrapper around RequestInsights(). Insight text is written by
	 * the model and delivered through OnInsightGenerated, so the returned array is
	 * always empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	TArray<FInsightEntry> GenerateInsights();

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	TArray<FInsightEntry> GetInsightsByDomain(EInsightDomain Domain) const;

	/**
	 * Ask the model to write a blog post from the unprocessed insights. The post
	 * arrives asynchronously through OnBlogPostGenerated. Returns true when a
	 * request was sent.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	bool RequestBlogPost();

	/**
	 * Compatibility wrapper around RequestBlogPost(). The post is written by the
	 * model and delivered through OnBlogPostGenerated, so the returned struct is
	 * always empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	FBlogPost GenerateBlogPost();

	UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Narrative")
	FOnPipelineInsightGenerated OnInsightGenerated;

	UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Narrative")
	FOnPipelineBlogPostGenerated OnBlogPostGenerated;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DeepTreeEcho|Narrative")
	FNarrativeState GetNarrativeState() const;

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	TArray<FDiaryEntry> GetRecentDiaryEntries(int32 Count = 10) const;

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	TArray<FInsightEntry> GetRecentInsights(int32 Count = 5) const;

	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
	FString GetNarrativeSummary() const;

protected:
	UPROPERTY(EditAnywhere, Category = "Narrative|Config") int32 InsightThreshold = 5;
	UPROPERTY(EditAnywhere, Category = "Narrative|Config") int32 BlogThreshold = 3;
	UPROPERTY(EditAnywhere, Category = "Narrative|Config") int32 MaxDiaryEntries = 500;
	UPROPERTY(EditAnywhere, Category = "Narrative|Config") int32 MaxInsights = 100;
	UPROPERTY(EditAnywhere, Category = "Narrative|Config") float AutoInsightInterval = 60.0f;

	UPROPERTY() UEpisodicMemorySystem* EpisodicMemory = nullptr;
	UPROPERTY() UHypergraphMemorySystem* HypergraphMemory = nullptr;

	/** The only source of insight and blog text. Resolved in BeginPlay. */
	UPROPERTY() UDTELanguageModelBridge* ModelBridge = nullptr;

private:
	TArray<FDiaryEntry> DiaryEntries;
	TArray<FInsightEntry> Insights;
	TArray<FBlogPost> BlogPosts;
	int32 UnprocessedDiaryCount = 0;
	int32 UnprocessedInsightCount = 0;
	float WisdomLevel = 0.1f;
	float TimeSinceLastInsightCheck = 0.0f;
	int32 PendingInsightRequests = 0;
	bool bBlogPostPending = false;
	mutable int32 NextEntryID = 1;

	/**
	 * Send one insight request. Candidate carries the computed metadata (ID,
	 * confidence, domain, sources); Intent says which pattern the model should
	 * write about; Context holds the statistics and source entries. RoundConsumed
	 * is shared by all requests of one round so the diary entries are marked
	 * processed once, on the first insight the model actually produces.
	 */
	void SendInsightRequest(const FInsightEntry& Candidate, const FString& Intent, const FString& Context,
	                        TSharedPtr<bool> RoundConsumed, int32 RecentCount);
	void StoreInsight(const FInsightEntry& Insight);
	FString DescribeEntries(const TArray<FDiaryEntry>& Entries) const;
	static const TCHAR* DomainLabel(EInsightDomain Domain);

	FString GenerateID(const FString& Prefix) const;
	EInsightDomain CategorizeContent(const FString& Content) const;
	TArray<TPair<FString, int32>> FindRecurringKeywords(const TArray<FDiaryEntry>& Entries) const;
	float DetectEmotionalTrend(const TArray<FDiaryEntry>& Entries) const;
	float DetectEmotionalVolatility(const TArray<FDiaryEntry>& Entries) const;
	void PruneDiary();
	void PruneInsights();
};

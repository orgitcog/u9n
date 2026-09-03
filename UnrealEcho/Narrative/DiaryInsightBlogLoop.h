#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiaryInsightBlogLoop.generated.h"

/**
 * Narrative entry types
 */
UENUM(BlueprintType)
enum class ENarrativeEntryType : uint8
{
    DiaryEntry UMETA(DisplayName = "Diary Entry"),
    Insight UMETA(DisplayName = "Insight"),
    BlogPost UMETA(DisplayName = "Blog Post"),
    Reflection UMETA(DisplayName = "Reflection"),
    Memory UMETA(DisplayName = "Memory")
};

/**
 * Narrative entry structure
 */
USTRUCT(BlueprintType)
struct FNarrativeEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ENarrativeEntryType EntryType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence; // -1 to 1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Significance; // 0 to 1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RelatedEntryIDs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EntryID;

    FNarrativeEntry()
        : EntryType(ENarrativeEntryType::DiaryEntry)
        , Title(TEXT(""))
        , Content(TEXT(""))
        , Timestamp(FDateTime::Now())
        , EmotionalValence(0.0f)
        , Significance(0.5f)
        , EntryID(FGuid::NewGuid().ToString())
    {}
};

/**
 * Insight generation parameters
 */
USTRUCT(BlueprintType)
struct FInsightGenerationParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LookbackDays; // How many days to analyze

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinimumSignificance; // Minimum significance threshold

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> FocusTags; // Specific tags to focus on

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIncludeEmotionalPatterns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIncludeBehavioralPatterns;

    FInsightGenerationParams()
        : LookbackDays(7)
        , MinimumSignificance(0.5f)
        , bIncludeEmotionalPatterns(true)
        , bIncludeBehavioralPatterns(true)
    {}
};

/**
 * Diary-Insight-Blog Narrative Loop Component
 * Implements the continuous self-reflection and narrative generation system
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

    // ===== Diary Management =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Diary")
    void WriteDiaryEntry(const FString& Title, const FString& Content, 
                         float EmotionalValence, const TArray<FString>& Tags);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Diary")
    TArray<FNarrativeEntry> GetDiaryEntries(int32 Count = 10) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Diary")
    TArray<FNarrativeEntry> GetDiaryEntriesByDateRange(FDateTime StartDate, FDateTime EndDate) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Diary")
    TArray<FNarrativeEntry> GetDiaryEntriesByTag(const FString& Tag) const;

    // ===== Insight Generation =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Insight")
    void GenerateInsights(const FInsightGenerationParams& Params);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Insight")
    TArray<FNarrativeEntry> GetInsights(int32 Count = 5) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Insight")
    FNarrativeEntry GenerateInsightFromPattern(const TArray<FNarrativeEntry>& Entries);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Insight")
    void AnalyzeEmotionalPatterns();

    UFUNCTION(BlueprintCallable, Category = "Narrative|Insight")
    void AnalyzeBehavioralPatterns();

    // ===== Blog Post Creation =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Blog")
    void CreateBlogPost(const FString& Title, const FString& Content, 
                        const TArray<FString>& SourceInsightIDs);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Blog")
    TArray<FNarrativeEntry> GetBlogPosts(int32 Count = 5) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Blog")
    FNarrativeEntry SynthesizeBlogFromInsights(const TArray<FNarrativeEntry>& Insights);

    // ===== Reflection and Meta-Cognition =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Reflection")
    void PerformSelfReflection();

    UFUNCTION(BlueprintCallable, Category = "Narrative|Reflection")
    FString GenerateNarrativeSummary(int32 DaysToSummarize = 7);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Reflection")
    TArray<FString> IdentifyRecurringThemes();

    UFUNCTION(BlueprintCallable, Category = "Narrative|Reflection")
    float CalculateNarrativeCoherence() const;

    // ===== Loop Management =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void StartNarrativeLoop();

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void StopNarrativeLoop();

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void SetLoopInterval(float IntervalSeconds);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    bool IsLoopActive() const { return bLoopActive; }

    // ===== Query and Search =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Query")
    TArray<FNarrativeEntry> SearchNarrative(const FString& SearchQuery) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Query")
    FNarrativeEntry GetEntryByID(const FString& EntryID) const;

    UFUNCTION(BlueprintCallable, Category = "Narrative|Query")
    TArray<FNarrativeEntry> GetRelatedEntries(const FString& EntryID, int32 MaxDepth = 2) const;

protected:
    // ===== Internal Methods =====

    void UpdateNarrativeLoop(float DeltaTime);
    void ProcessDiaryToInsight();
    void ProcessInsightToBlog();
    void MaintainNarrativeDatabase();

    FString GenerateInsightContent(const TArray<FNarrativeEntry>& SourceEntries);
    FString GenerateBlogContent(const TArray<FNarrativeEntry>& SourceInsights);
    
    TArray<FNarrativeEntry> FilterEntriesBySignificance(const TArray<FNarrativeEntry>& Entries, 
                                                         float MinSignificance) const;
    TArray<FNarrativeEntry> ClusterEntriesByTheme(const TArray<FNarrativeEntry>& Entries) const;

    void SaveNarrativeToFile();
    void LoadNarrativeFromFile();

private:
    // ===== Narrative Database =====

    UPROPERTY()
    TArray<FNarrativeEntry> AllEntries;

    UPROPERTY()
    TMap<FString, FNarrativeEntry> EntryDatabase;

    // ===== Loop State =====

    UPROPERTY(EditAnywhere, Category = "Narrative")
    bool bLoopActive;

    UPROPERTY(EditAnywhere, Category = "Narrative")
    float LoopInterval; // Seconds between loop iterations

    float LoopTimer;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "Narrative")
    int32 MaxEntriesInMemory;

    UPROPERTY(EditAnywhere, Category = "Narrative")
    float InsightGenerationThreshold;

    UPROPERTY(EditAnywhere, Category = "Narrative")
    int32 MinEntriesForInsight;

    UPROPERTY(EditAnywhere, Category = "Narrative")
    FString NarrativeSaveFilePath;

    // ===== Statistics =====

    UPROPERTY()
    int32 TotalDiaryEntries;

    UPROPERTY()
    int32 TotalInsights;

    UPROPERTY()
    int32 TotalBlogPosts;

    UPROPERTY()
    FDateTime LastInsightGeneration;

    UPROPERTY()
    FDateTime LastBlogCreation;
};

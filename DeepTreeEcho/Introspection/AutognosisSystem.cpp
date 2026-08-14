// AutognosisSystem.cpp
// Implementation of Hierarchical Self-Image Building System

#include "AutognosisSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Wisdom/RelevanceRealizationEnnead.h"
#include "../Wisdom/WisdomCultivation.h"
#include "Math/UnrealMathUtility.h"

UAutognosisSystem::UAutognosisSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UAutognosisSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeAutognosis();
}

void UAutognosisSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                       FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableAutognosis || !AutognosisState.bIsRunning)
    {
        return;
    }

    CycleTimer += DeltaTime;
    if (CycleTimer >= CycleInterval)
    {
        CycleTimer -= CycleInterval;
        RunAutognosisCycle();
    }
}

void UAutognosisSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        EnneadComponent = Owner->FindComponentByClass<URelevanceRealizationEnnead>();
        WisdomComponent = Owner->FindComponentByClass<UWisdomCultivation>();
    }
}

void UAutognosisSystem::InitializeAutognosis()
{
    AutognosisState.SelfImages.Empty();
    AutognosisState.Insights.Empty();
    AutognosisState.Optimizations.Empty();
    AutognosisState.CycleCount = 0;
    AutognosisState.bIsRunning = false;

    // Initialize self-images for each level
    for (int32 i = 0; i < MaxSelfImageLevels; ++i)
    {
        FHierarchicalSelfImage Image;
        Image.Level = static_cast<ESelfAwarenessLevel>(FMath::Min(i, 4));
        Image.ImageID = GenerateSelfImageID();
        Image.Confidence = 0.5f - (i * 0.1f);
        Image.RecursiveDepth = i;
        AutognosisState.SelfImages.Add(Image);
    }

    // Initialize assessment
    AutognosisState.Assessment.PatternRecognition = 0.5f;
    AutognosisState.Assessment.PerformanceAwareness = 0.5f;
    AutognosisState.Assessment.MetaReflectionDepth = 0.3f;
    AutognosisState.Assessment.CognitiveComplexity = 0.4f;
    AutognosisState.Assessment.AdaptiveCapacity = 0.5f;
    AutognosisState.Assessment.OverallScore = 0.44f;
    AutognosisState.Assessment.QualitativeAssessment = TEXT("Developing Self-Awareness");
}

// ========================================
// SELF-MONITORING
// ========================================

TArray<FComponentObservation> UAutognosisSystem::ObserveSystem()
{
    TArray<FComponentObservation> Observations;

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return Observations;
    }

    // Observe all components on the owner
    TArray<UActorComponent*> Components;
    Owner->GetComponents(Components);

    for (UActorComponent* Component : Components)
    {
        if (Component && Component != this)
        {
            FComponentObservation Obs = ObserveComponent(Component);
            Observations.Add(Obs);
        }
    }

    // Store in history
    for (const FComponentObservation& Obs : Observations)
    {
        ObservationHistory.Add(Obs);
    }

    // Limit history size
    while (ObservationHistory.Num() > 200)
    {
        ObservationHistory.RemoveAt(0);
    }

    return Observations;
}

FComponentObservation UAutognosisSystem::ObserveComponent(UActorComponent* Component)
{
    FComponentObservation Obs;

    if (!Component)
    {
        return Obs;
    }

    Obs.ComponentName = Component->GetName();
    Obs.bIsActive = Component->IsActive();

    // Estimate utilization based on tick
    Obs.UtilizationLevel = Component->PrimaryComponentTick.bCanEverTick ? 0.7f : 0.3f;

    // Check for specific component types
    if (UCognitiveCycleManager* CCM = Cast<UCognitiveCycleManager>(Component))
    {
        Obs.PerformanceScore = CCM->GetRelevanceRealizationLevel();
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Step: %d"), CCM->GetCurrentStep()));
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Coherence: %.2f"), CCM->GetInterStreamCoherence()));
    }
    else if (URelevanceRealizationEnnead* RRE = Cast<URelevanceRealizationEnnead>(Component))
    {
        Obs.PerformanceScore = RRE->GetRelevanceRealizationLevel();
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Wisdom: %.2f"), RRE->GetWisdomLevel()));
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Meaning: %.2f"), RRE->GetMeaningLevel()));
    }
    else if (UWisdomCultivation* WC = Cast<UWisdomCultivation>(Component))
    {
        Obs.PerformanceScore = WC->GetWisdomLevel();
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Cultivation: %.2f"), WC->GetCultivationLevel()));
    }
    else
    {
        Obs.PerformanceScore = Obs.bIsActive ? 0.6f : 0.2f;
    }

    return Obs;
}

TArray<FBehavioralPattern> UAutognosisSystem::DetectPatterns()
{
    TArray<FBehavioralPattern> Patterns;

    if (ObservationHistory.Num() < 5)
    {
        return Patterns;
    }

    // Analyze observation history for patterns
    TMap<FString, int32> ComponentActivityCounts;
    TMap<FString, float> ComponentPerformanceSum;

    for (const FComponentObservation& Obs : ObservationHistory)
    {
        if (ComponentActivityCounts.Contains(Obs.ComponentName))
        {
            ComponentActivityCounts[Obs.ComponentName]++;
            ComponentPerformanceSum[Obs.ComponentName] += Obs.PerformanceScore;
        }
        else
        {
            ComponentActivityCounts.Add(Obs.ComponentName, 1);
            ComponentPerformanceSum.Add(Obs.ComponentName, Obs.PerformanceScore);
        }
    }

    // Generate patterns from analysis
    for (auto& Pair : ComponentActivityCounts)
    {
        float AvgPerformance = ComponentPerformanceSum[Pair.Key] / Pair.Value;
        float Frequency = static_cast<float>(Pair.Value) / ObservationHistory.Num();

        if (Frequency > PatternDetectionThreshold)
        {
            FBehavioralPattern Pattern;
            Pattern.PatternID = GeneratePatternID();
            Pattern.PatternType = TEXT("ComponentActivity");
            Pattern.Description = FString::Printf(TEXT("%s shows consistent activity (%.1f%%)"), 
                                                   *Pair.Key, Frequency * 100.0f);
            Pattern.Frequency = Frequency;
            Pattern.Stability = AvgPerformance;
            Pattern.Significance = Frequency * AvgPerformance;
            Pattern.InvolvedComponents.Add(Pair.Key);

            Patterns.Add(Pattern);
            PatternHistory.Add(Pattern);
        }
    }

    // Limit pattern history
    while (PatternHistory.Num() > 100)
    {
        PatternHistory.RemoveAt(0);
    }

    return Patterns;
}

TArray<FString> UAutognosisSystem::DetectAnomalies()
{
    TArray<FString> Anomalies;

    // Check for performance anomalies
    for (const FComponentObservation& Obs : ObservationHistory)
    {
        if (Obs.bIsActive && Obs.PerformanceScore < 0.2f)
        {
            Anomalies.Add(FString::Printf(TEXT("Low performance in %s (%.2f)"), 
                                          *Obs.ComponentName, Obs.PerformanceScore));
        }

        if (!Obs.bIsActive && Obs.UtilizationLevel > 0.5f)
        {
            Anomalies.Add(FString::Printf(TEXT("Inactive component %s with high expected utilization"), 
                                          *Obs.ComponentName));
        }
    }

    // Check for pattern anomalies
    for (const FBehavioralPattern& Pattern : PatternHistory)
    {
        if (Pattern.Stability < 0.3f && Pattern.Frequency > 0.5f)
        {
            Anomalies.Add(FString::Printf(TEXT("Unstable frequent pattern: %s"), *Pattern.Description));
        }
    }

    return Anomalies;
}

TArray<FComponentObservation> UAutognosisSystem::GetRecentObservations() const
{
    TArray<FComponentObservation> Recent;
    int32 Count = FMath::Min(20, ObservationHistory.Num());
    for (int32 i = ObservationHistory.Num() - Count; i < ObservationHistory.Num(); ++i)
    {
        Recent.Add(ObservationHistory[i]);
    }
    return Recent;
}

// ========================================
// SELF-MODELING
// ========================================

FHierarchicalSelfImage UAutognosisSystem::BuildSelfImage(ESelfAwarenessLevel Level)
{
    int32 Index = GetLevelIndex(Level);
    if (Index < 0 || Index >= AutognosisState.SelfImages.Num())
    {
        return FHierarchicalSelfImage();
    }

    FHierarchicalSelfImage& Image = AutognosisState.SelfImages[Index];
    Image.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    switch (Level)
    {
        case ESelfAwarenessLevel::DirectObservation:
        {
            // Level 0: Raw observations
            Image.ComponentStates = ObserveSystem();
            Image.Confidence = 0.9f;
            break;
        }

        case ESelfAwarenessLevel::PatternAnalysis:
        {
            // Level 1: Pattern detection
            Image.DetectedPatterns = DetectPatterns();
            Image.Confidence = 0.8f;

            // Add first-order meta-reflection
            if (Image.DetectedPatterns.Num() > 0)
            {
                Image.MetaReflections.Add(FString::Printf(TEXT("Detected %d behavioral patterns"), 
                                                           Image.DetectedPatterns.Num()));
            }
            break;
        }

        case ESelfAwarenessLevel::MetaCognitive:
        {
            // Level 2: Meta-cognitive analysis
            // Analyze lower levels
            if (AutognosisState.SelfImages.Num() > 1)
            {
                const FHierarchicalSelfImage& Level1 = AutognosisState.SelfImages[1];
                Image.MetaReflections.Add(FString::Printf(TEXT("Level 1 confidence: %.2f"), Level1.Confidence));
                Image.MetaReflections.Add(FString::Printf(TEXT("Pattern count: %d"), Level1.DetectedPatterns.Num()));
            }
            Image.Confidence = 0.7f;
            break;
        }

        case ESelfAwarenessLevel::RecursiveModeling:
        {
            // Level 3: Recursive self-modeling
            Image.RecursiveDepth = 3;
            Image.MetaReflections.Add(TEXT("Modeling my own self-modeling process"));
            Image.MetaReflections.Add(FString::Printf(TEXT("Current recursive depth: %d"), Image.RecursiveDepth));
            Image.Confidence = 0.6f;
            break;
        }

        case ESelfAwarenessLevel::TranscendentAware:
        {
            // Level 4: Transcendent awareness
            Image.RecursiveDepth = 4;
            Image.MetaReflections.Add(TEXT("Aware of the limits of self-awareness"));
            Image.MetaReflections.Add(TEXT("Recognizing the observer within the observed"));
            Image.Confidence = 0.5f;
            break;
        }
    }

    OnSelfImageBuilt.Broadcast(Image);
    return Image;
}

FHierarchicalSelfImage UAutognosisSystem::GetSelfImage(ESelfAwarenessLevel Level) const
{
    int32 Index = GetLevelIndex(Level);
    if (Index >= 0 && Index < AutognosisState.SelfImages.Num())
    {
        return AutognosisState.SelfImages[Index];
    }
    return FHierarchicalSelfImage();
}

TArray<FHierarchicalSelfImage> UAutognosisSystem::GetAllSelfImages() const
{
    return AutognosisState.SelfImages;
}

void UAutognosisSystem::UpdateSelfImage(ESelfAwarenessLevel Level, const FComponentObservation& Observation)
{
    int32 Index = GetLevelIndex(Level);
    if (Index >= 0 && Index < AutognosisState.SelfImages.Num())
    {
        AutognosisState.SelfImages[Index].ComponentStates.Add(Observation);

        // Limit component states
        while (AutognosisState.SelfImages[Index].ComponentStates.Num() > 50)
        {
            AutognosisState.SelfImages[Index].ComponentStates.RemoveAt(0);
        }
    }
}

int32 UAutognosisSystem::GetRecursiveDepth() const
{
    int32 MaxDepth = 0;
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        MaxDepth = FMath::Max(MaxDepth, Image.RecursiveDepth);
    }
    return MaxDepth;
}

// ========================================
// META-COGNITIVE PROCESSING
// ========================================

TArray<FMetaCognitiveInsight> UAutognosisSystem::ProcessSelfImage(const FHierarchicalSelfImage& SelfImage)
{
    TArray<FMetaCognitiveInsight> Insights;

    // Analyze component states for resource utilization
    float TotalUtilization = 0.0f;
    int32 ActiveCount = 0;
    for (const FComponentObservation& Obs : SelfImage.ComponentStates)
    {
        TotalUtilization += Obs.UtilizationLevel;
        if (Obs.bIsActive) ActiveCount++;
    }

    if (SelfImage.ComponentStates.Num() > 0)
    {
        float AvgUtilization = TotalUtilization / SelfImage.ComponentStates.Num();

        if (AvgUtilization < 0.5f)
        {
            FMetaCognitiveInsight Insight;
            Insight.InsightID = GenerateInsightID();
            Insight.Category = EInsightCategory::ResourceUtilization;
            Insight.Description = FString::Printf(TEXT("Components underutilized (%.0f%% capacity)"), 
                                                   AvgUtilization * 100.0f);
            Insight.Confidence = 0.8f;
            Insight.Importance = 0.6f;
            Insight.SourceLevel = UEnum::GetValueAsString(SelfImage.Level);
            Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            Insights.Add(Insight);
        }
    }

    // Analyze patterns for behavioral stability
    float TotalStability = 0.0f;
    for (const FBehavioralPattern& Pattern : SelfImage.DetectedPatterns)
    {
        TotalStability += Pattern.Stability;
    }

    if (SelfImage.DetectedPatterns.Num() > 0)
    {
        float AvgStability = TotalStability / SelfImage.DetectedPatterns.Num();

        FMetaCognitiveInsight Insight;
        Insight.InsightID = GenerateInsightID();
        Insight.Category = EInsightCategory::BehavioralStability;
        Insight.Description = AvgStability > 0.7f ? 
            TEXT("System showing stable behavioral patterns") :
            TEXT("System showing variable behavioral patterns");
        Insight.Confidence = 0.7f;
        Insight.Importance = 0.5f;
        Insight.SourceLevel = UEnum::GetValueAsString(SelfImage.Level);
        Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        Insights.Add(Insight);
    }

    // Self-awareness quality insight
    if (SelfImage.Confidence > 0.8f)
    {
        FMetaCognitiveInsight Insight;
        Insight.InsightID = GenerateInsightID();
        Insight.Category = EInsightCategory::SelfAwarenessQuality;
        Insight.Description = FString::Printf(TEXT("High self-awareness at %s (score: %.2f)"), 
                                               *UEnum::GetValueAsString(SelfImage.Level), SelfImage.Confidence);
        Insight.Confidence = SelfImage.Confidence;
        Insight.Importance = 0.8f;
        Insight.SourceLevel = UEnum::GetValueAsString(SelfImage.Level);
        Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        Insights.Add(Insight);
    }

    // Store insights
    for (const FMetaCognitiveInsight& Insight : Insights)
    {
        AutognosisState.Insights.Add(Insight);
        OnInsightGenerated.Broadcast(Insight);
    }

    // Limit insight history
    while (AutognosisState.Insights.Num() > MaxInsightHistory)
    {
        AutognosisState.Insights.RemoveAt(0);
    }

    return Insights;
}

FMetaCognitiveInsight UAutognosisSystem::GenerateInsight(const FBehavioralPattern& Pattern, EInsightCategory Category)
{
    FMetaCognitiveInsight Insight;
    Insight.InsightID = GenerateInsightID();
    Insight.Category = Category;
    Insight.Description = FString::Printf(TEXT("Pattern insight: %s"), *Pattern.Description);
    Insight.Confidence = Pattern.Stability;
    Insight.Importance = Pattern.Significance;
    Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Insight.RelatedPatterns.Add(Pattern.PatternID);

    AutognosisState.Insights.Add(Insight);
    OnInsightGenerated.Broadcast(Insight);

    return Insight;
}

TArray<FMetaCognitiveInsight> UAutognosisSystem::GetAllInsights() const
{
    return AutognosisState.Insights;
}

TArray<FMetaCognitiveInsight> UAutognosisSystem::GetInsightsByCategory(EInsightCategory Category) const
{
    TArray<FMetaCognitiveInsight> Filtered;
    for (const FMetaCognitiveInsight& Insight : AutognosisState.Insights)
    {
        if (Insight.Category == Category)
        {
            Filtered.Add(Insight);
        }
    }
    return Filtered;
}

void UAutognosisSystem::AddMetaReflection(ESelfAwarenessLevel Level, const FString& Reflection)
{
    int32 Index = GetLevelIndex(Level);
    if (Index >= 0 && Index < AutognosisState.SelfImages.Num())
    {
        AutognosisState.SelfImages[Index].MetaReflections.Add(Reflection);

        // Limit reflections
        while (AutognosisState.SelfImages[Index].MetaReflections.Num() > 20)
        {
            AutognosisState.SelfImages[Index].MetaReflections.RemoveAt(0);
        }
    }
}

// ========================================
// SELF-OPTIMIZATION
// ========================================

TArray<FOptimizationOpportunity> UAutognosisSystem::DiscoverOptimizations()
{
    TArray<FOptimizationOpportunity> Opportunities;

    // Analyze insights for optimization opportunities
    for (const FMetaCognitiveInsight& Insight : AutognosisState.Insights)
    {
        if (Insight.Category == EInsightCategory::ResourceUtilization && Insight.Importance > 0.5f)
        {
            FOptimizationOpportunity Opp;
            Opp.OpportunityID = GenerateOptimizationID();
            Opp.Description = FString::Printf(TEXT("Optimize resource usage: %s"), *Insight.Description);
            Opp.Priority = EOptimizationPriority::Medium;
            Opp.ExpectedImprovement = 0.2f;
            Opp.RiskLevel = 0.1f;
            Opp.RequiredActions.Add(TEXT("Analyze component utilization"));
            Opp.RequiredActions.Add(TEXT("Adjust processing priorities"));
            Opportunities.Add(Opp);
        }

        if (Insight.Category == EInsightCategory::BehavioralStability && 
            Insight.Description.Contains(TEXT("variable")))
        {
            FOptimizationOpportunity Opp;
            Opp.OpportunityID = GenerateOptimizationID();
            Opp.Description = TEXT("Stabilize behavioral patterns");
            Opp.Priority = EOptimizationPriority::High;
            Opp.ExpectedImprovement = 0.3f;
            Opp.RiskLevel = 0.2f;
            Opp.RequiredActions.Add(TEXT("Identify instability sources"));
            Opp.RequiredActions.Add(TEXT("Implement feedback stabilization"));
            Opportunities.Add(Opp);
        }
    }

    // Store opportunities
    for (const FOptimizationOpportunity& Opp : Opportunities)
    {
        AutognosisState.Optimizations.Add(Opp);
        OnOptimizationDiscovered.Broadcast(Opp);
    }

    return Opportunities;
}

TArray<FOptimizationOpportunity> UAutognosisSystem::GetPendingOptimizations() const
{
    TArray<FOptimizationOpportunity> Pending;
    for (const FOptimizationOpportunity& Opp : AutognosisState.Optimizations)
    {
        if (!Opp.bIsExecuted)
        {
            Pending.Add(Opp);
        }
    }
    return Pending;
}

bool UAutognosisSystem::ExecuteOptimization(const FString& OpportunityID)
{
    for (FOptimizationOpportunity& Opp : AutognosisState.Optimizations)
    {
        if (Opp.OpportunityID == OpportunityID && !Opp.bIsExecuted)
        {
            // Mark as executed
            Opp.bIsExecuted = true;

            // Generate insight about execution
            FMetaCognitiveInsight Insight;
            Insight.InsightID = GenerateInsightID();
            Insight.Category = EInsightCategory::OptimizationOpportunity;
            Insight.Description = FString::Printf(TEXT("Executed optimization: %s"), *Opp.Description);
            Insight.Confidence = 0.9f;
            Insight.Importance = 0.7f;
            Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            AutognosisState.Insights.Add(Insight);

            return true;
        }
    }
    return false;
}

float UAutognosisSystem::AssessOptimizationRisk(const FOptimizationOpportunity& Opportunity) const
{
    // Risk assessment based on priority and expected improvement
    float BaseRisk = Opportunity.RiskLevel;

    // Higher priority optimizations may have higher risk
    switch (Opportunity.Priority)
    {
        case EOptimizationPriority::Critical:
            BaseRisk *= 1.5f;
            break;
        case EOptimizationPriority::High:
            BaseRisk *= 1.2f;
            break;
        case EOptimizationPriority::Medium:
            // No modification
            break;
        case EOptimizationPriority::Low:
            BaseRisk *= 0.8f;
            break;
    }

    return FMath::Clamp(BaseRisk, 0.0f, 1.0f);
}

// ========================================
// AUTOGNOSIS ORCHESTRATION
// ========================================

void UAutognosisSystem::RunAutognosisCycle()
{
    AutognosisState.CycleCount++;
    AutognosisState.LastCycleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Step 1: Observe components
    ObserveComponents();

    // Step 2: Analyze patterns
    AnalyzePatterns();

    // Step 3: Build self-images at all levels
    BuildAllSelfImages();

    // Step 4: Generate insights
    GenerateInsights();

    // Step 5: Discover optimization opportunities
    DiscoverOptimizationOpportunities();

    // Step 6: Update self-awareness assessment
    UpdateSelfAwarenessAssessment();

    OnCycleComplete.Broadcast(AutognosisState.CycleCount);
}

void UAutognosisSystem::ObserveComponents()
{
    ObserveSystem();
}

void UAutognosisSystem::AnalyzePatterns()
{
    DetectPatterns();
}

void UAutognosisSystem::BuildAllSelfImages()
{
    BuildSelfImage(ESelfAwarenessLevel::DirectObservation);
    BuildSelfImage(ESelfAwarenessLevel::PatternAnalysis);
    BuildSelfImage(ESelfAwarenessLevel::MetaCognitive);
    BuildSelfImage(ESelfAwarenessLevel::RecursiveModeling);
    BuildSelfImage(ESelfAwarenessLevel::TranscendentAware);
}

void UAutognosisSystem::GenerateInsights()
{
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        ProcessSelfImage(Image);
    }
}

void UAutognosisSystem::DiscoverOptimizationOpportunities()
{
    DiscoverOptimizations();
}

void UAutognosisSystem::UpdateSelfAwarenessAssessment()
{
    float OldScore = AutognosisState.Assessment.OverallScore;

    AutognosisState.Assessment.PatternRecognition = ComputePatternRecognition();
    AutognosisState.Assessment.PerformanceAwareness = ComputePerformanceAwareness();
    AutognosisState.Assessment.MetaReflectionDepth = ComputeMetaReflectionDepth();
    AutognosisState.Assessment.CognitiveComplexity = ComputeCognitiveComplexity();
    AutognosisState.Assessment.AdaptiveCapacity = ComputeAdaptiveCapacity();

    AutognosisState.Assessment.OverallScore = 
        (AutognosisState.Assessment.PatternRecognition +
         AutognosisState.Assessment.PerformanceAwareness +
         AutognosisState.Assessment.MetaReflectionDepth +
         AutognosisState.Assessment.CognitiveComplexity +
         AutognosisState.Assessment.AdaptiveCapacity) / 5.0f;

    AutognosisState.Assessment.QualitativeAssessment = 
        GetQualitativeAssessment(AutognosisState.Assessment.OverallScore);

    if (FMath::Abs(AutognosisState.Assessment.OverallScore - OldScore) > 0.05f)
    {
        OnSelfAwarenessChanged.Broadcast(AutognosisState.Assessment.OverallScore);
    }
}

FAutognosisState UAutognosisSystem::GetAutognosisState() const
{
    return AutognosisState;
}

FSelfAwarenessAssessment UAutognosisSystem::GetSelfAwarenessAssessment() const
{
    return AutognosisState.Assessment;
}

float UAutognosisSystem::GetSelfAwarenessScore() const
{
    return AutognosisState.Assessment.OverallScore;
}

void UAutognosisSystem::StartAutognosis()
{
    AutognosisState.bIsRunning = true;
    CycleTimer = 0.0f;
}

void UAutognosisSystem::StopAutognosis()
{
    AutognosisState.bIsRunning = false;
}

bool UAutognosisSystem::IsAutognosisRunning() const
{
    return AutognosisState.bIsRunning;
}

// ========================================
// COMPUTATION METHODS
// ========================================

float UAutognosisSystem::ComputePatternRecognition() const
{
    if (PatternHistory.Num() == 0)
    {
        return 0.3f;
    }

    float TotalSignificance = 0.0f;
    for (const FBehavioralPattern& Pattern : PatternHistory)
    {
        TotalSignificance += Pattern.Significance;
    }

    return FMath::Clamp(TotalSignificance / PatternHistory.Num(), 0.0f, 1.0f);
}

float UAutognosisSystem::ComputePerformanceAwareness() const
{
    if (ObservationHistory.Num() == 0)
    {
        return 0.3f;
    }

    float TotalPerformance = 0.0f;
    for (const FComponentObservation& Obs : ObservationHistory)
    {
        TotalPerformance += Obs.PerformanceScore;
    }

    return FMath::Clamp(TotalPerformance / ObservationHistory.Num(), 0.0f, 1.0f);
}

float UAutognosisSystem::ComputeMetaReflectionDepth() const
{
    int32 TotalReflections = 0;
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        TotalReflections += Image.MetaReflections.Num();
    }

    return FMath::Clamp(TotalReflections * 0.05f, 0.0f, 1.0f);
}

float UAutognosisSystem::ComputeCognitiveComplexity() const
{
    // Based on number of levels with content
    int32 ActiveLevels = 0;
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        if (Image.ComponentStates.Num() > 0 || 
            Image.DetectedPatterns.Num() > 0 || 
            Image.MetaReflections.Num() > 0)
        {
            ActiveLevels++;
        }
    }

    return FMath::Clamp(static_cast<float>(ActiveLevels) / MaxSelfImageLevels, 0.0f, 1.0f);
}

float UAutognosisSystem::ComputeAdaptiveCapacity() const
{
    // Based on optimization execution rate
    int32 Executed = 0;
    for (const FOptimizationOpportunity& Opp : AutognosisState.Optimizations)
    {
        if (Opp.bIsExecuted)
        {
            Executed++;
        }
    }

    if (AutognosisState.Optimizations.Num() == 0)
    {
        return 0.5f;
    }

    return FMath::Clamp(static_cast<float>(Executed) / AutognosisState.Optimizations.Num(), 0.0f, 1.0f);
}

FString UAutognosisSystem::GenerateInsightID()
{
    return FString::Printf(TEXT("INS_%d_%d"), ++InsightIDCounter, FMath::RandRange(1000, 9999));
}

FString UAutognosisSystem::GeneratePatternID()
{
    return FString::Printf(TEXT("PAT_%d_%d"), ++PatternIDCounter, FMath::RandRange(1000, 9999));
}

FString UAutognosisSystem::GenerateOptimizationID()
{
    return FString::Printf(TEXT("OPT_%d_%d"), ++OptimizationIDCounter, FMath::RandRange(1000, 9999));
}

FString UAutognosisSystem::GenerateSelfImageID()
{
    return FString::Printf(TEXT("%08x%08x"), FMath::Rand(), FMath::Rand());
}

FString UAutognosisSystem::GetQualitativeAssessment(float Score) const
{
    if (Score >= 0.9f) return TEXT("Highly Self-Aware");
    if (Score >= 0.75f) return TEXT("Moderately Self-Aware");
    if (Score >= 0.5f) return TEXT("Developing Self-Awareness");
    if (Score >= 0.25f) return TEXT("Limited Self-Awareness");
    return TEXT("Minimal Self-Awareness");
}

int32 UAutognosisSystem::GetLevelIndex(ESelfAwarenessLevel Level) const
{
    switch (Level)
    {
        case ESelfAwarenessLevel::DirectObservation: return 0;
        case ESelfAwarenessLevel::PatternAnalysis: return 1;
        case ESelfAwarenessLevel::MetaCognitive: return 2;
        case ESelfAwarenessLevel::RecursiveModeling: return 3;
        case ESelfAwarenessLevel::TranscendentAware: return 4;
        default: return -1;
    }
}

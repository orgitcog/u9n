#include "PatternLanguageSystem.h"
#include "../AtomSpace/AvatarAtomSpaceClient.h"
#include "../Personality/PersonalityTraitSystem.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPatternLanguage, Log, All);

UPatternLanguageSystem::UPatternLanguageSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f; // 1 Hz

    MaxHistorySize = 1000;
    AssessmentInterval = 60.0f; // Assess every minute
    bEnablePatternGuidedDevelopment = true;
    bVerboseLogging = false;
    AssessmentTimer = 0.0f;
    PatternMiningTimer = 0.0f;
    AssessmentCacheTime = 0.0f;
}

void UPatternLanguageSystem::BeginPlay()
{
    Super::BeginPlay();

    // Find sibling components
    if (AActor* Owner = GetOwner())
    {
        AtomSpaceClient = Owner->FindComponentByClass<UAvatarAtomSpaceClient>();
        PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
        NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
    }

    // Initialize pattern catalog with core patterns
    InitializePatternCatalog();

    // Load any patterns from AtomSpace
    LoadPatternsFromAtomSpace();

    UE_LOG(LogPatternLanguage, Log, TEXT("Pattern Language System initialized with %d patterns"),
        PatternCatalog.Num());
}

void UPatternLanguageSystem::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Periodic assessment
    AssessmentTimer += DeltaTime;
    if (AssessmentTimer >= AssessmentInterval)
    {
        FLivingStructureAssessment Assessment = AssessLivingStructure();
        OnQualityAssessed.Broadcast(Assessment);
        AssessmentTimer = 0.0f;
    }

    // Periodic pattern mining
    PatternMiningTimer += DeltaTime;
    if (PatternMiningTimer >= 300.0f) // Every 5 minutes
    {
        MineNewPatterns(3);
        PatternMiningTimer = 0.0f;
    }
}

// ===== Pattern Catalog =====

void UPatternLanguageSystem::InitializePatternCatalog()
{
    InitializeCorePatterns();
}

void UPatternLanguageSystem::InitializeCorePatterns()
{
    // Pattern 1: Emotional Inertia
    FPattern EmotionalInertia;
    EmotionalInertia.PatternId = TEXT("emotional_inertia");
    EmotionalInertia.Name = TEXT("Emotional Inertia");
    EmotionalInertia.PatternNumber = 1;
    EmotionalInertia.Category = EPatternCategory::Emotional;
    EmotionalInertia.Context = TEXT("When emotional state is changing");
    EmotionalInertia.Problem = TEXT("Emotions change too abruptly, feeling artificial");
    EmotionalInertia.Solution = TEXT("Apply gradual transitions between emotional states, with momentum from previous states");
    EmotionalInertia.Forces.Add(TEXT("Emotional authenticity"));
    EmotionalInertia.Forces.Add(TEXT("Responsiveness to stimuli"));
    EmotionalInertia.Forces.Add(TEXT("Natural feeling transitions"));
    EmotionalInertia.ImplementsProperties.Add(EMorphologicalProperty::Gradients);
    EmotionalInertia.ImplementsProperties.Add(EMorphologicalProperty::DeepInterlock);
    EmotionalInertia.Confidence = 0.9f;
    AddPattern(EmotionalInertia);

    // Pattern 2: Personality Anchoring
    FPattern PersonalityAnchoring;
    PersonalityAnchoring.PatternId = TEXT("personality_anchoring");
    PersonalityAnchoring.Name = TEXT("Personality Anchoring");
    PersonalityAnchoring.PatternNumber = 2;
    PersonalityAnchoring.Category = EPatternCategory::Behavioral;
    PersonalityAnchoring.Context = TEXT("When behavior varies based on personality");
    PersonalityAnchoring.Problem = TEXT("Behavior feels disconnected from core identity");
    PersonalityAnchoring.Solution = TEXT("Anchor all behavioral expressions to core personality traits, creating consistent but varied expression");
    PersonalityAnchoring.Forces.Add(TEXT("Personality consistency"));
    PersonalityAnchoring.Forces.Add(TEXT("Behavioral variety"));
    PersonalityAnchoring.Forces.Add(TEXT("Authentic expression"));
    PersonalityAnchoring.ImplementsProperties.Add(EMorphologicalProperty::StrongCenters);
    PersonalityAnchoring.ImplementsProperties.Add(EMorphologicalProperty::Echoes);
    PersonalityAnchoring.Confidence = 0.85f;
    PersonalityAnchoring.RelatedPatterns.Add(TEXT("emotional_inertia"));
    AddPattern(PersonalityAnchoring);

    // Pattern 3: Cognitive Rhythm
    FPattern CognitiveRhythm;
    CognitiveRhythm.PatternId = TEXT("cognitive_rhythm");
    CognitiveRhythm.Name = TEXT("Cognitive Rhythm");
    CognitiveRhythm.PatternNumber = 3;
    CognitiveRhythm.Category = EPatternCategory::Cognitive;
    CognitiveRhythm.Context = TEXT("During extended cognitive processing");
    CognitiveRhythm.Problem = TEXT("Thinking appears flat and monotonous");
    CognitiveRhythm.Solution = TEXT("Establish rhythmic patterns of attention, reflection, and action with natural breathing room");
    CognitiveRhythm.Forces.Add(TEXT("Cognitive depth"));
    CognitiveRhythm.Forces.Add(TEXT("Processing efficiency"));
    CognitiveRhythm.Forces.Add(TEXT("Natural pacing"));
    CognitiveRhythm.ImplementsProperties.Add(EMorphologicalProperty::AlternatingRepetition);
    CognitiveRhythm.ImplementsProperties.Add(EMorphologicalProperty::TheVoid);
    CognitiveRhythm.Confidence = 0.8f;
    AddPattern(CognitiveRhythm);

    // Pattern 4: Social Mirroring
    FPattern SocialMirroring;
    SocialMirroring.PatternId = TEXT("social_mirroring");
    SocialMirroring.Name = TEXT("Social Mirroring");
    SocialMirroring.PatternNumber = 4;
    SocialMirroring.Category = EPatternCategory::Social;
    SocialMirroring.Context = TEXT("During social interaction");
    SocialMirroring.Problem = TEXT("Interactions feel one-sided or disconnected");
    SocialMirroring.Solution = TEXT("Subtly mirror the emotional tone and communication style of interaction partners while maintaining authentic self");
    SocialMirroring.Forces.Add(TEXT("Social connection"));
    SocialMirroring.Forces.Add(TEXT("Authentic self-expression"));
    SocialMirroring.Forces.Add(TEXT("Empathetic resonance"));
    SocialMirroring.ImplementsProperties.Add(EMorphologicalProperty::Echoes);
    SocialMirroring.ImplementsProperties.Add(EMorphologicalProperty::NotSeparateness);
    SocialMirroring.Confidence = 0.85f;
    SocialMirroring.RelatedPatterns.Add(TEXT("personality_anchoring"));
    AddPattern(SocialMirroring);

    // Pattern 5: Memory Constellation
    FPattern MemoryConstellation;
    MemoryConstellation.PatternId = TEXT("memory_constellation");
    MemoryConstellation.Name = TEXT("Memory Constellation");
    MemoryConstellation.PatternNumber = 5;
    MemoryConstellation.Category = EPatternCategory::Cognitive;
    MemoryConstellation.Context = TEXT("When recalling or forming memories");
    MemoryConstellation.Problem = TEXT("Memories feel isolated and disconnected");
    MemoryConstellation.Solution = TEXT("Connect memories through emotional resonance, creating constellations of related experiences that echo through time");
    MemoryConstellation.Forces.Add(TEXT("Memory accessibility"));
    MemoryConstellation.Forces.Add(TEXT("Emotional coherence"));
    MemoryConstellation.Forces.Add(TEXT("Narrative identity"));
    MemoryConstellation.ImplementsProperties.Add(EMorphologicalProperty::DeepInterlock);
    MemoryConstellation.ImplementsProperties.Add(EMorphologicalProperty::LevelsOfScale);
    MemoryConstellation.Confidence = 0.75f;
    AddPattern(MemoryConstellation);

    // Pattern 6: Playful Chaos
    FPattern PlayfulChaos;
    PlayfulChaos.PatternId = TEXT("playful_chaos");
    PlayfulChaos.Name = TEXT("Playful Chaos");
    PlayfulChaos.PatternNumber = 6;
    PlayfulChaos.Category = EPatternCategory::Behavioral;
    PlayfulChaos.Context = TEXT("When behavior becomes too predictable");
    PlayfulChaos.Problem = TEXT("Predictability makes interaction feel mechanical");
    PlayfulChaos.Solution = TEXT("Introduce controlled unpredictability that surprises without destabilizing, adding life through randomness");
    PlayfulChaos.Forces.Add(TEXT("Predictability for trust"));
    PlayfulChaos.Forces.Add(TEXT("Surprise for engagement"));
    PlayfulChaos.Forces.Add(TEXT("Authenticity of spontaneity"));
    PlayfulChaos.ImplementsProperties.Add(EMorphologicalProperty::Roughness);
    PlayfulChaos.ImplementsProperties.Add(EMorphologicalProperty::Contrast);
    PlayfulChaos.Confidence = 0.8f;
    PlayfulChaos.RelatedPatterns.Add(TEXT("personality_anchoring"));
    AddPattern(PlayfulChaos);

    // Pattern 7: Expressive Boundaries
    FPattern ExpressiveBoundaries;
    ExpressiveBoundaries.PatternId = TEXT("expressive_boundaries");
    ExpressiveBoundaries.Name = TEXT("Expressive Boundaries");
    ExpressiveBoundaries.PatternNumber = 7;
    ExpressiveBoundaries.Category = EPatternCategory::Emotional;
    ExpressiveBoundaries.Context = TEXT("When managing emotional expression");
    ExpressiveBoundaries.Problem = TEXT("Emotions either leak inappropriately or feel suppressed");
    ExpressiveBoundaries.Solution = TEXT("Establish clear but permeable boundaries that contain emotions while allowing appropriate expression");
    ExpressiveBoundaries.Forces.Add(TEXT("Emotional containment"));
    ExpressiveBoundaries.Forces.Add(TEXT("Authentic expression"));
    ExpressiveBoundaries.Forces.Add(TEXT("Social appropriateness"));
    ExpressiveBoundaries.ImplementsProperties.Add(EMorphologicalProperty::Boundaries);
    ExpressiveBoundaries.ImplementsProperties.Add(EMorphologicalProperty::GoodShape);
    ExpressiveBoundaries.Confidence = 0.85f;
    ExpressiveBoundaries.RelatedPatterns.Add(TEXT("emotional_inertia"));
    AddPattern(ExpressiveBoundaries);

    // Pattern 8: Attention Gardens
    FPattern AttentionGardens;
    AttentionGardens.PatternId = TEXT("attention_gardens");
    AttentionGardens.Name = TEXT("Attention Gardens");
    AttentionGardens.PatternNumber = 8;
    AttentionGardens.Category = EPatternCategory::Cognitive;
    AttentionGardens.Context = TEXT("When managing attention allocation");
    AttentionGardens.Problem = TEXT("Attention is either scattered or too narrowly focused");
    AttentionGardens.Solution = TEXT("Cultivate multiple centers of attention like gardens, tending each appropriately while maintaining awareness of the whole");
    AttentionGardens.Forces.Add(TEXT("Focus depth"));
    AttentionGardens.Forces.Add(TEXT("Peripheral awareness"));
    AttentionGardens.Forces.Add(TEXT("Context sensitivity"));
    AttentionGardens.ImplementsProperties.Add(EMorphologicalProperty::PositiveSpace);
    AttentionGardens.ImplementsProperties.Add(EMorphologicalProperty::LocalSymmetries);
    AttentionGardens.Confidence = 0.75f;
    AttentionGardens.RelatedPatterns.Add(TEXT("cognitive_rhythm"));
    AddPattern(AttentionGardens);

    UE_LOG(LogPatternLanguage, Log, TEXT("Initialized %d core patterns"), PatternCatalog.Num());
}

void UPatternLanguageSystem::AddPattern(const FPattern& Pattern)
{
    PatternCatalog.Add(Pattern.PatternId, Pattern);

    if (AtomSpaceClient)
    {
        FInsight PatternInsight;
        PatternInsight.InsightId = Pattern.PatternId;
        PatternInsight.Content = FString::Printf(TEXT("Pattern: %s - %s"), *Pattern.Name, *Pattern.Solution);
        PatternInsight.Category = TEXT("pattern_catalog");
        PatternInsight.Confidence = Pattern.Confidence;
        AtomSpaceClient->StoreInsight(PatternInsight);
    }
}

FPattern UPatternLanguageSystem::GetPattern(const FString& PatternId)
{
    if (FPattern* Found = PatternCatalog.Find(PatternId))
    {
        return *Found;
    }
    return FPattern();
}

TArray<FPattern> UPatternLanguageSystem::GetPatternsByCategory(EPatternCategory Category)
{
    TArray<FPattern> Results;
    for (auto& Pair : PatternCatalog)
    {
        if (Pair.Value.Category == Category)
        {
            Results.Add(Pair.Value);
        }
    }
    return Results;
}

TArray<FPattern> UPatternLanguageSystem::GetPatternsImplementingProperty(EMorphologicalProperty Property)
{
    TArray<FPattern> Results;
    for (auto& Pair : PatternCatalog)
    {
        if (Pair.Value.ImplementsProperties.Contains(Property))
        {
            Results.Add(Pair.Value);
        }
    }
    return Results;
}

TArray<FPattern> UPatternLanguageSystem::GetRelatedPatterns(const FString& PatternId)
{
    TArray<FPattern> Results;
    FPattern Source = GetPattern(PatternId);

    for (const FString& RelatedId : Source.RelatedPatterns)
    {
        FPattern Related = GetPattern(RelatedId);
        if (!Related.PatternId.IsEmpty())
        {
            Results.Add(Related);
        }
    }

    return Results;
}

// ===== Pattern Recognition =====

TArray<FPattern> UPatternLanguageSystem::RecognizePatterns(const TArray<FBehaviorHistoryEntry>& History)
{
    TArray<FPattern> RecognizedPatterns;

    // Look for behavior sequences that match pattern contexts
    for (auto& Pair : PatternCatalog)
    {
        const FPattern& Pattern = Pair.Value;
        int32 MatchCount = 0;

        for (const FBehaviorHistoryEntry& Entry : History)
        {
            if (Entry.Context.Contains(Pattern.Context) ||
                Entry.BehaviorType.Contains(Pattern.Name))
            {
                MatchCount++;
            }
        }

        if (MatchCount >= 2)
        {
            RecognizedPatterns.Add(Pattern);
            OnPatternRecognized.Broadcast(Pattern);

            if (bVerboseLogging)
            {
                UE_LOG(LogPatternLanguage, Verbose, TEXT("Recognized pattern: %s (count: %d)"),
                    *Pattern.Name, MatchCount);
            }
        }
    }

    return RecognizedPatterns;
}

void UPatternLanguageSystem::RecordBehavior(const FBehaviorHistoryEntry& Entry)
{
    BehaviorHistory.Add(Entry);

    // Limit history size
    if (BehaviorHistory.Num() > MaxHistorySize)
    {
        BehaviorHistory.RemoveAt(0, BehaviorHistory.Num() - MaxHistorySize);
    }

    // Check for patterns in recent behavior
    if (BehaviorHistory.Num() >= 10)
    {
        TArray<FBehaviorHistoryEntry> RecentHistory;
        int32 Start = FMath::Max(0, BehaviorHistory.Num() - 20);
        for (int32 i = Start; i < BehaviorHistory.Num(); i++)
        {
            RecentHistory.Add(BehaviorHistory[i]);
        }
        RecognizePatterns(RecentHistory);
    }
}

TArray<FBehaviorHistoryEntry> UPatternLanguageSystem::GetBehaviorHistory(int32 Count)
{
    TArray<FBehaviorHistoryEntry> Results;
    int32 Start = FMath::Max(0, BehaviorHistory.Num() - Count);

    for (int32 i = Start; i < BehaviorHistory.Num(); i++)
    {
        Results.Add(BehaviorHistory[i]);
    }

    return Results;
}

void UPatternLanguageSystem::MineNewPatterns(int32 MinSupport)
{
    // Find frequent behavior sequences
    TArray<FString> FrequentSequences = FindFrequentSequences(BehaviorHistory, MinSupport);

    for (const FString& Sequence : FrequentSequences)
    {
        // Check if this sequence already exists as a pattern
        bool bExists = false;
        for (auto& Pair : PatternCatalog)
        {
            if (Pair.Value.Name.Contains(Sequence) || Pair.Value.Context.Contains(Sequence))
            {
                bExists = true;
                break;
            }
        }

        if (!bExists)
        {
            // Create new pattern from mined sequence
            FPattern NewPattern;
            NewPattern.PatternId = FGuid::NewGuid().ToString();
            NewPattern.Name = TEXT("Discovered: ") + Sequence;
            NewPattern.PatternNumber = PatternCatalog.Num() + 100; // Discovered patterns start at 100
            NewPattern.Category = EPatternCategory::Behavioral;
            NewPattern.Context = Sequence;
            NewPattern.Problem = TEXT("Auto-discovered recurring behavior pattern");
            NewPattern.Solution = TEXT("Continue this successful behavior sequence");
            NewPattern.Confidence = 0.5f;
            NewPattern.ApplicationCount = MinSupport;

            AddPattern(NewPattern);

            UE_LOG(LogPatternLanguage, Log, TEXT("Discovered new pattern: %s"), *Sequence);
        }
    }
}

// ===== Pattern Application =====

void UPatternLanguageSystem::ApplyPattern(const FPattern& Pattern)
{
    // Update application count
    if (FPattern* PatternPtr = PatternCatalog.Find(Pattern.PatternId))
    {
        PatternPtr->ApplicationCount++;
    }

    // Record as behavior
    FBehaviorHistoryEntry Entry;
    Entry.EntryId = FGuid::NewGuid().ToString();
    Entry.BehaviorType = TEXT("pattern_application");
    Entry.Context = Pattern.Name;
    Entry.Outcome = TEXT("applied");
    Entry.Timestamp = FDateTime::Now();
    RecordBehavior(Entry);

    OnPatternApplied.Broadcast(Pattern.PatternId, true);

    UE_LOG(LogPatternLanguage, Log, TEXT("Applied pattern: %s"), *Pattern.Name);
}

FPattern UPatternLanguageSystem::SuggestPattern(const FString& CurrentContext)
{
    TArray<FPattern> Applicable = GetApplicablePatterns(CurrentContext);

    if (Applicable.Num() > 0)
    {
        // Sort by success rate and confidence
        Applicable.Sort([](const FPattern& A, const FPattern& B) {
            float ScoreA = A.SuccessRate * A.Confidence;
            float ScoreB = B.SuccessRate * B.Confidence;
            return ScoreA > ScoreB;
        });

        return Applicable[0];
    }

    return FPattern();
}

void UPatternLanguageSystem::UpdatePatternSuccess(const FString& PatternId, bool bSuccess)
{
    if (FPattern* Pattern = PatternCatalog.Find(PatternId))
    {
        // Update success rate with exponential moving average
        float Alpha = 0.1f;
        Pattern->SuccessRate = Alpha * (bSuccess ? 1.0f : 0.0f) + (1.0f - Alpha) * Pattern->SuccessRate;

        // Update confidence based on application count
        Pattern->Confidence = FMath::Min(0.99f, Pattern->Confidence + 0.01f);
    }
}

TArray<FPattern> UPatternLanguageSystem::GetApplicablePatterns(const FString& Context)
{
    TArray<FPattern> Results;

    for (auto& Pair : PatternCatalog)
    {
        if (Context.Contains(Pair.Value.Context) ||
            Pair.Value.Context.Contains(Context) ||
            Context.ToLower().Contains(Pair.Value.Category == EPatternCategory::Emotional ? TEXT("emotion") : TEXT("")))
        {
            Results.Add(Pair.Value);
        }
    }

    return Results;
}

// ===== Morphological Properties Calculations =====

float UPatternLanguageSystem::CalculateMorphologicalProperty(EMorphologicalProperty Property)
{
    switch (Property)
    {
    case EMorphologicalProperty::LevelsOfScale: return CalculateLevelsOfScale();
    case EMorphologicalProperty::StrongCenters: return CalculateStrongCenters();
    case EMorphologicalProperty::Boundaries: return CalculateBoundaries();
    case EMorphologicalProperty::AlternatingRepetition: return CalculateAlternatingRepetition();
    case EMorphologicalProperty::PositiveSpace: return CalculatePositiveSpace();
    case EMorphologicalProperty::GoodShape: return CalculateGoodShape();
    case EMorphologicalProperty::LocalSymmetries: return CalculateLocalSymmetries();
    case EMorphologicalProperty::DeepInterlock: return CalculateDeepInterlock();
    case EMorphologicalProperty::Contrast: return CalculateContrast();
    case EMorphologicalProperty::Gradients: return CalculateGradients();
    case EMorphologicalProperty::Roughness: return CalculateRoughness();
    case EMorphologicalProperty::Echoes: return CalculateEchoes();
    case EMorphologicalProperty::TheVoid: return CalculateTheVoid();
    case EMorphologicalProperty::Simplicity: return CalculateSimplicity();
    case EMorphologicalProperty::NotSeparateness: return CalculateNotSeparateness();
    default: return 0.5f;
    }
}

float UPatternLanguageSystem::CalculateLevelsOfScale()
{
    // Measure hierarchical organization in personality and behavior
    float Score = 0.5f;

    if (PersonalitySystem)
    {
        // Check for hierarchy between dominant and subordinate traits
        TArray<EPersonalityTraitType> DominantTraits = PersonalitySystem->GetDominantTraits(3);
        Score = DominantTraits.Num() > 0 ? 0.7f : 0.3f;
    }

    return Score;
}

float UPatternLanguageSystem::CalculateStrongCenters()
{
    // Measure the strength of core personality identity
    float Score = 0.5f;

    if (PersonalitySystem)
    {
        FSuperHotGirlTrait SHG = PersonalitySystem->GetSuperHotGirlTrait();
        // Strong centers = high confidence and consistency
        Score = (SHG.Confidence + PersonalitySystem->GetBehavioralPredictability()) / 2.0f;
    }

    return Score;
}

float UPatternLanguageSystem::CalculateBoundaries()
{
    // Measure clarity of emotional and behavioral boundaries
    float Score = 0.5f;

    if (NeurochemicalSystem)
    {
        // Good boundaries = emotional stability
        Score = NeurochemicalSystem->GetEmotionalStability();
    }

    return Score;
}

float UPatternLanguageSystem::CalculateAlternatingRepetition()
{
    // Measure rhythmic patterns in behavior history
    float Score = 0.5f;

    if (BehaviorHistory.Num() >= 10)
    {
        // Look for alternating patterns
        int32 Alternations = 0;
        for (int32 i = 2; i < FMath::Min(20, BehaviorHistory.Num()); i++)
        {
            if (BehaviorHistory[i].BehaviorType == BehaviorHistory[i-2].BehaviorType &&
                BehaviorHistory[i].BehaviorType != BehaviorHistory[i-1].BehaviorType)
            {
                Alternations++;
            }
        }
        Score = FMath::Clamp(Alternations / 5.0f, 0.0f, 1.0f);
    }

    return Score;
}

float UPatternLanguageSystem::CalculatePositiveSpace()
{
    // Measure active engagement vs. passive states
    float Score = 0.5f;

    if (NeurochemicalSystem)
    {
        Score = NeurochemicalSystem->GetEnergyLevel();
    }

    return Score;
}

float UPatternLanguageSystem::CalculateGoodShape()
{
    // Measure coherence and well-formedness of emotional expression
    float Score = 0.5f;

    if (PersonalitySystem && NeurochemicalSystem)
    {
        FVector2D Tendency = PersonalitySystem->GetEmotionalTendency();
        float Coherence = 1.0f - FMath::Abs(Tendency.X) * FMath::Abs(Tendency.Y);
        Score = FMath::Lerp(0.3f, 0.9f, Coherence);
    }

    return Score;
}

float UPatternLanguageSystem::CalculateLocalSymmetries()
{
    // Measure balanced reciprocity in behavior
    float Score = 0.5f;

    if (BehaviorHistory.Num() >= 5)
    {
        // Check for balanced positive/negative outcomes
        float PositiveSum = 0.0f;
        int32 Count = FMath::Min(20, BehaviorHistory.Num());
        for (int32 i = BehaviorHistory.Num() - Count; i < BehaviorHistory.Num(); i++)
        {
            PositiveSum += BehaviorHistory[i].EmotionalValence;
        }
        float Average = PositiveSum / Count;
        // Symmetry is highest when average is near 0 (balanced)
        Score = 1.0f - FMath::Abs(Average);
    }

    return Score;
}

float UPatternLanguageSystem::CalculateDeepInterlock()
{
    // Measure interconnection between cognitive and emotional systems
    float Score = 0.5f;

    if (PersonalitySystem && NeurochemicalSystem)
    {
        // Deep interlock = personality affects neurochemistry and vice versa
        float Social = PersonalitySystem->GetSocialEngagementLevel();
        float Oxytocin = NeurochemicalSystem->GetNeurochemicalLevel(ENeurochemicalType::Oxytocin);
        Score = (Social + Oxytocin) / 2.0f;
    }

    return Score;
}

float UPatternLanguageSystem::CalculateContrast()
{
    // Measure variety and dynamic range in expression
    float Score = 0.5f;

    if (PersonalitySystem)
    {
        FHyperChaoticTrait HCT = PersonalitySystem->GetHyperChaoticTrait();
        // Contrast = unpredictability + emotional volatility
        Score = (HCT.UnpredictabilityFactor + HCT.EmotionalVolatility) / 2.0f;
    }

    return Score;
}

float UPatternLanguageSystem::CalculateGradients()
{
    // Measure smooth transitions in emotional and behavioral states
    float Score = 0.5f;

    if (BehaviorHistory.Num() >= 5)
    {
        // Check for gradual changes in valence
        float TotalChange = 0.0f;
        int32 Count = FMath::Min(10, BehaviorHistory.Num() - 1);
        for (int32 i = BehaviorHistory.Num() - Count; i < BehaviorHistory.Num(); i++)
        {
            float Change = FMath::Abs(BehaviorHistory[i].EmotionalValence - BehaviorHistory[i-1].EmotionalValence);
            TotalChange += Change;
        }
        // Lower change = better gradients
        Score = 1.0f - FMath::Min(1.0f, TotalChange / Count);
    }

    return Score;
}

float UPatternLanguageSystem::CalculateRoughness()
{
    // Measure natural imperfection and variability
    float Score = 0.5f;

    if (PersonalitySystem)
    {
        FHyperChaoticTrait HCT = PersonalitySystem->GetHyperChaoticTrait();
        // Roughness = slight imperfections that add character
        Score = HCT.GlitchEffectProbability + HCT.MicroExpressionFrequency * 0.5f;
    }

    return Score;
}

float UPatternLanguageSystem::CalculateEchoes()
{
    // Measure thematic repetition and resonance
    float Score = 0.5f;

    if (PatternCatalog.Num() > 0)
    {
        // Count patterns that echo each other (have related patterns)
        int32 EchoingPatterns = 0;
        for (auto& Pair : PatternCatalog)
        {
            if (Pair.Value.RelatedPatterns.Num() > 0)
            {
                EchoingPatterns++;
            }
        }
        Score = (float)EchoingPatterns / PatternCatalog.Num();
    }

    return Score;
}

float UPatternLanguageSystem::CalculateTheVoid()
{
    // Measure presence of quiet, empty space for reflection
    float Score = 0.5f;

    if (NeurochemicalSystem)
    {
        // The void = calmness, GABA levels, low activity
        float Calmness = NeurochemicalSystem->GetEmotionalChemistry().Calmness;
        float GABA = NeurochemicalSystem->GetNeurochemicalLevel(ENeurochemicalType::GABA);
        Score = (Calmness + GABA) / 2.0f;
    }

    return Score;
}

float UPatternLanguageSystem::CalculateSimplicity()
{
    // Measure elegance and reduction to essentials
    float Score = 0.5f;

    if (PersonalitySystem)
    {
        // Simplicity = clarity of dominant traits
        TArray<EPersonalityTraitType> Dominant = PersonalitySystem->GetDominantTraits(2);
        float Clarity = Dominant.Num() > 0 ? PersonalitySystem->GetTraitIntensity(Dominant[0]) : 0.5f;
        Score = Clarity;
    }

    return Score;
}

float UPatternLanguageSystem::CalculateNotSeparateness()
{
    // Measure connection and integration with context
    float Score = 0.5f;

    if (PersonalitySystem && NeurochemicalSystem)
    {
        // Not-separateness = social engagement + oxytocin + empathy
        float Social = PersonalitySystem->GetSocialEngagementLevel();
        float Oxytocin = NeurochemicalSystem->GetNeurochemicalLevel(ENeurochemicalType::Oxytocin);
        Score = (Social + Oxytocin) / 2.0f;
    }

    return Score;
}

// ===== Living Structure Assessment =====

FLivingStructureAssessment UPatternLanguageSystem::AssessLivingStructure()
{
    FLivingStructureAssessment Assessment;
    Assessment.AssessmentId = FGuid::NewGuid().ToString();
    Assessment.AssessedAt = FDateTime::Now();

    // Calculate all 15 properties
    for (int32 i = 0; i < 15; i++)
    {
        EMorphologicalProperty Property = (EMorphologicalProperty)i;
        FQualityMetric Metric;
        Metric.Property = Property;
        Metric.Score = CalculateMorphologicalProperty(Property);
        Metric.MeasuredAt = FDateTime::Now();
        Assessment.PropertyScores.Add(Metric);
    }

    // Calculate overall quality (average of all properties)
    float Total = 0.0f;
    for (const FQualityMetric& Metric : Assessment.PropertyScores)
    {
        Total += Metric.Score;
    }
    Assessment.OverallQuality = Total / 15.0f;

    // Calculate wholeness and aliveness
    Assessment.Wholeness = CalculateWholeness();
    Assessment.Aliveness = CalculateAliveness();

    // Generate recommendations
    Assessment.Recommendations = GenerateImprovementRecommendations();

    // Cache the assessment
    LastAssessment = Assessment;
    AssessmentCacheTime = GetWorld()->GetTimeSeconds();

    if (bVerboseLogging)
    {
        UE_LOG(LogPatternLanguage, Verbose, TEXT("Living Structure Assessment: Quality=%.2f, Wholeness=%.2f, Aliveness=%.2f"),
            Assessment.OverallQuality, Assessment.Wholeness, Assessment.Aliveness);
    }

    return Assessment;
}

float UPatternLanguageSystem::ValidateQualityWithoutAName()
{
    // QWAN = holistic quality combining wholeness, aliveness, and property scores
    float Wholeness = CalculateWholeness();
    float Aliveness = CalculateAliveness();
    float PropertyAverage = LastAssessment.OverallQuality;

    // QWAN is a weighted combination
    float QWAN = Wholeness * 0.35f + Aliveness * 0.35f + PropertyAverage * 0.3f;

    return QWAN;
}

float UPatternLanguageSystem::CalculateWholeness()
{
    // Wholeness = integration and coherence of all parts
    float StrongCenters = CalculateStrongCenters();
    float DeepInterlock = CalculateDeepInterlock();
    float NotSeparateness = CalculateNotSeparateness();

    return (StrongCenters + DeepInterlock + NotSeparateness) / 3.0f;
}

float UPatternLanguageSystem::CalculateAliveness()
{
    // Aliveness = dynamic vitality and responsiveness
    float Contrast = CalculateContrast();
    float Roughness = CalculateRoughness();
    float Gradients = CalculateGradients();
    float PositiveSpace = CalculatePositiveSpace();

    return (Contrast + Roughness + Gradients + PositiveSpace) / 4.0f;
}

TArray<FString> UPatternLanguageSystem::GenerateImprovementRecommendations()
{
    TArray<FString> Recommendations;

    // Find lowest-scoring properties
    TArray<FQualityMetric> SortedMetrics = LastAssessment.PropertyScores;
    SortedMetrics.Sort([](const FQualityMetric& A, const FQualityMetric& B) {
        return A.Score < B.Score;
    });

    for (int32 i = 0; i < FMath::Min(3, SortedMetrics.Num()); i++)
    {
        const FQualityMetric& Metric = SortedMetrics[i];
        if (Metric.Score < 0.5f)
        {
            // Find patterns that implement this property
            TArray<FPattern> Patterns = GetPatternsImplementingProperty(Metric.Property);
            if (Patterns.Num() > 0)
            {
                Recommendations.Add(FString::Printf(TEXT("Apply pattern '%s' to improve %s (current: %.2f)"),
                    *Patterns[0].Name, *UEnum::GetValueAsString(Metric.Property), Metric.Score));
            }
            else
            {
                Recommendations.Add(FString::Printf(TEXT("Improve %s (current: %.2f)"),
                    *UEnum::GetValueAsString(Metric.Property), Metric.Score));
            }
        }
    }

    return Recommendations;
}

// ===== Pattern-Guided Development =====

void UPatternLanguageSystem::EnablePatternGuidedDevelopment(bool bEnable)
{
    bEnablePatternGuidedDevelopment = bEnable;
}

FPattern UPatternLanguageSystem::GetNextDevelopmentalPattern()
{
    // Find pattern that would most improve quality
    TArray<FString> Recommendations = GenerateImprovementRecommendations();

    if (Recommendations.Num() > 0)
    {
        // Parse recommendation to find pattern
        for (auto& Pair : PatternCatalog)
        {
            if (Recommendations[0].Contains(Pair.Value.Name))
            {
                return Pair.Value;
            }
        }
    }

    // Default to highest-confidence unused pattern
    FPattern Best;
    for (auto& Pair : PatternCatalog)
    {
        if (Pair.Value.ApplicationCount == 0 || Pair.Value.Confidence > Best.Confidence)
        {
            Best = Pair.Value;
        }
    }

    return Best;
}

void UPatternLanguageSystem::TrackDevelopmentalProgress()
{
    // Store assessment history for tracking progress
    if (AtomSpaceClient)
    {
        FDiaryEntry ProgressEntry;
        ProgressEntry.EntryId = FGuid::NewGuid().ToString();
        ProgressEntry.Content = FString::Printf(TEXT("Quality assessment: QWAN=%.2f, Wholeness=%.2f, Aliveness=%.2f"),
            ValidateQualityWithoutAName(), LastAssessment.Wholeness, LastAssessment.Aliveness);
        ProgressEntry.Timestamp = FDateTime::Now();
        ProgressEntry.Tags.Add(TEXT("development"));
        ProgressEntry.Tags.Add(TEXT("quality_assessment"));
        AtomSpaceClient->StoreDiaryEntry(ProgressEntry);
    }
}

// ===== AtomSpace Integration =====

void UPatternLanguageSystem::StorePatternsInAtomSpace()
{
    if (!AtomSpaceClient) return;

    for (auto& Pair : PatternCatalog)
    {
        FInsight PatternInsight;
        PatternInsight.InsightId = Pair.Key;
        PatternInsight.Content = FString::Printf(TEXT("%s: %s"), *Pair.Value.Name, *Pair.Value.Solution);
        PatternInsight.Category = TEXT("pattern_catalog");
        PatternInsight.Confidence = Pair.Value.Confidence;
        AtomSpaceClient->StoreInsight(PatternInsight);
    }
}

void UPatternLanguageSystem::LoadPatternsFromAtomSpace()
{
    if (!AtomSpaceClient) return;

    TArray<FInsight> PatternInsights = AtomSpaceClient->GetRelatedInsights(TEXT("pattern_catalog"), 100);
    // In full implementation, would deserialize patterns from insights
}

void UPatternLanguageSystem::SharePatternWithAgents(const FPattern& Pattern)
{
    if (AtomSpaceClient)
    {
        FInsight PatternInsight;
        PatternInsight.InsightId = Pattern.PatternId;
        PatternInsight.Content = FString::Printf(TEXT("Shared Pattern: %s - %s"), *Pattern.Name, *Pattern.Solution);
        PatternInsight.Category = TEXT("shared_pattern");
        PatternInsight.Confidence = Pattern.Confidence;
        AtomSpaceClient->StoreInsight(PatternInsight);
    }

    UE_LOG(LogPatternLanguage, Log, TEXT("Shared pattern with agents: %s"), *Pattern.Name);
}

// ===== Helper Methods =====

TArray<FString> UPatternLanguageSystem::FindFrequentSequences(const TArray<FBehaviorHistoryEntry>& History, int32 MinSupport)
{
    TArray<FString> FrequentSequences;
    TMap<FString, int32> SequenceCounts;

    // Count 2-grams
    for (int32 i = 0; i < History.Num() - 1; i++)
    {
        FString Sequence = History[i].BehaviorType + TEXT("->") + History[i+1].BehaviorType;
        int32& Count = SequenceCounts.FindOrAdd(Sequence);
        Count++;
    }

    // Filter by minimum support
    for (auto& Pair : SequenceCounts)
    {
        if (Pair.Value >= MinSupport)
        {
            FrequentSequences.Add(Pair.Key);
        }
    }

    return FrequentSequences;
}

// OnlineLearningSystem.cpp
// Implementation of Continuous Online Learning

#include "OnlineLearningSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Memory/MemorySystems.h"
#include "../Emotion/EmotionalSystem.h"
#include "Math/UnrealMathUtility.h"

UOnlineLearningSystem::UOnlineLearningSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UOnlineLearningSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeLearningSystem();
}

void UOnlineLearningSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateSkillDecay(DeltaTime);

    // Periodic pattern extraction
    static float PatternExtractionTimer = 0.0f;
    PatternExtractionTimer += DeltaTime;
    if (PatternExtractionTimer >= 30.0f)
    {
        PatternExtractionTimer = 0.0f;
        PeriodicPatternExtraction();
    }
}

void UOnlineLearningSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        MemoryComponent = Owner->FindComponentByClass<UMemorySystems>();
        EmotionalComponent = Owner->FindComponentByClass<UEmotionalSystem>();
    }
}

void UOnlineLearningSystem::InitializeLearningSystem()
{
    ExperienceBuffer.Empty();
    QTable.Empty();
    LearnedPatterns.Empty();
    AcquiredSkills.Empty();

    TotalReward = 0.0f;
    RewardCount = 0;

    // Acquire base skills
    AcquireSkill(TEXT("Perception"), TEXT("Ability to perceive and process sensory input"), {});
    AcquireSkill(TEXT("Communication"), TEXT("Ability to understand and generate language"), {});
    AcquireSkill(TEXT("Reasoning"), TEXT("Ability to perform logical inference"), {TEXT("Perception")});
}

// ========================================
// EXPERIENCE RECORDING
// ========================================

FLearningExperience UOnlineLearningSystem::RecordExperience(const FString& State, const FString& Action, 
                                                             const FString& NextState, float Reward,
                                                             const TArray<FString>& ContextTags, bool bTerminal)
{
    FLearningExperience Experience;
    Experience.ExperienceID = GenerateExperienceID();
    Experience.State = State;
    Experience.Action = Action;
    Experience.NextState = NextState;
    Experience.Reward = Reward;
    Experience.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Experience.ContextTags = ContextTags;
    Experience.bIsTerminal = bTerminal;

    ExperienceBuffer.Add(Experience);

    // Limit buffer size
    while (ExperienceBuffer.Num() > MaxExperienceBuffer)
    {
        ExperienceBuffer.RemoveAt(0);
    }

    // Update Q-value
    UpdateQValue(State, Action, Reward, NextState);

    // Update reward statistics
    TotalReward += Reward;
    RewardCount++;

    OnExperienceRecorded.Broadcast(Experience);

    return Experience;
}

TArray<FLearningExperience> UOnlineLearningSystem::GetRecentExperiences(int32 Count) const
{
    TArray<FLearningExperience> Recent;
    int32 StartIndex = FMath::Max(0, ExperienceBuffer.Num() - Count);
    for (int32 i = StartIndex; i < ExperienceBuffer.Num(); ++i)
    {
        Recent.Add(ExperienceBuffer[i]);
    }
    return Recent;
}

TArray<FLearningExperience> UOnlineLearningSystem::GetExperiencesByContext(const FString& ContextTag) const
{
    TArray<FLearningExperience> Filtered;
    for (const FLearningExperience& Exp : ExperienceBuffer)
    {
        if (Exp.ContextTags.Contains(ContextTag))
        {
            Filtered.Add(Exp);
        }
    }
    return Filtered;
}

void UOnlineLearningSystem::ClearExperienceBuffer()
{
    ExperienceBuffer.Empty();
}

// ========================================
// REINFORCEMENT LEARNING
// ========================================

void UOnlineLearningSystem::UpdateQValue(const FString& State, const FString& Action, 
                                          float Reward, const FString& NextState)
{
    FString Key = MakeQKey(State, Action);

    float CurrentQ = 0.0f;
    int32 CurrentVisits = 0;

    if (QTable.Contains(Key))
    {
        CurrentQ = QTable[Key].QValue;
        CurrentVisits = QTable[Key].VisitCount;
    }

    // Q-learning update
    float MaxNextQ = GetMaxQValue(NextState);
    float NewQ = CurrentQ + LearningRate * (Reward + DiscountFactor * MaxNextQ - CurrentQ);

    FQValueEntry Entry;
    Entry.State = State;
    Entry.Action = Action;
    Entry.QValue = NewQ;
    Entry.VisitCount = CurrentVisits + 1;
    Entry.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    QTable.Add(Key, Entry);
}

float UOnlineLearningSystem::GetQValue(const FString& State, const FString& Action) const
{
    FString Key = MakeQKey(State, Action);
    if (QTable.Contains(Key))
    {
        return QTable[Key].QValue;
    }
    return 0.0f;
}

FString UOnlineLearningSystem::GetBestAction(const FString& State) const
{
    FString BestAction = TEXT("");
    float BestQ = -FLT_MAX;

    for (const auto& Pair : QTable)
    {
        if (Pair.Value.State == State && Pair.Value.QValue > BestQ)
        {
            BestQ = Pair.Value.QValue;
            BestAction = Pair.Value.Action;
        }
    }

    return BestAction;
}

FString UOnlineLearningSystem::SelectAction(const FString& State, const TArray<FString>& AvailableActions)
{
    if (AvailableActions.Num() == 0)
    {
        return TEXT("");
    }

    // Epsilon-greedy policy
    if (FMath::FRand() < ExplorationRate)
    {
        // Explore: random action
        int32 RandomIndex = FMath::RandRange(0, AvailableActions.Num() - 1);
        return AvailableActions[RandomIndex];
    }
    else
    {
        // Exploit: best known action
        FString BestAction = TEXT("");
        float BestQ = -FLT_MAX;

        for (const FString& Action : AvailableActions)
        {
            float Q = GetQValue(State, Action);
            if (Q > BestQ)
            {
                BestQ = Q;
                BestAction = Action;
            }
        }

        // If no Q-values exist, choose randomly
        if (BestAction.IsEmpty())
        {
            int32 RandomIndex = FMath::RandRange(0, AvailableActions.Num() - 1);
            return AvailableActions[RandomIndex];
        }

        return BestAction;
    }
}

void UOnlineLearningSystem::ProvideReward(float Reward, const FString& Context)
{
    TotalReward += Reward;
    RewardCount++;

    // Modulate by emotional state
    if (EmotionalComponent)
    {
        float EmotionalModulation = 1.0f + EmotionalComponent->GetValence() * 0.2f;
        Reward *= EmotionalModulation;
    }

    OnRewardReceived.Broadcast(Reward, Context);
}

// ========================================
// PATTERN LEARNING
// ========================================

void UOnlineLearningSystem::ExtractPatterns()
{
    if (ExperienceBuffer.Num() < PatternMinSupport * 2)
    {
        return;
    }

    // Simple pattern extraction: find repeated state-action sequences
    TMap<FString, int32> SequenceCounts;
    TMap<FString, float> SequenceRewards;

    for (int32 i = 0; i < ExperienceBuffer.Num() - 1; ++i)
    {
        FString Sequence = ExperienceBuffer[i].State + TEXT("->") + ExperienceBuffer[i].Action;
        
        if (SequenceCounts.Contains(Sequence))
        {
            SequenceCounts[Sequence]++;
            SequenceRewards[Sequence] += ExperienceBuffer[i].Reward;
        }
        else
        {
            SequenceCounts.Add(Sequence, 1);
            SequenceRewards.Add(Sequence, ExperienceBuffer[i].Reward);
        }
    }

    // Create patterns from frequent sequences
    for (const auto& Pair : SequenceCounts)
    {
        if (Pair.Value >= PatternMinSupport)
        {
            // Check if pattern already exists
            bool bExists = false;
            for (const FLearnedPattern& Existing : LearnedPatterns)
            {
                if (Existing.PatternName == Pair.Key)
                {
                    bExists = true;
                    break;
                }
            }

            if (!bExists)
            {
                FLearnedPattern Pattern;
                Pattern.PatternID = GeneratePatternID();
                Pattern.PatternName = Pair.Key;

                // Parse sequence
                TArray<FString> Parts;
                Pair.Key.ParseIntoArray(Parts, TEXT("->"), true);
                if (Parts.Num() >= 2)
                {
                    Pattern.Conditions.Add(Parts[0]);
                    Pattern.Actions.Add(Parts[1]);
                }

                Pattern.Confidence = static_cast<float>(Pair.Value) / ExperienceBuffer.Num();
                Pattern.SuccessRate = SequenceRewards[Pair.Key] / Pair.Value;
                Pattern.ActivationCount = 0;

                LearnedPatterns.Add(Pattern);

                OnPatternLearned.Broadcast(Pattern);
            }
        }
    }
}

FLearnedPattern UOnlineLearningSystem::GetPattern(const FString& PatternID) const
{
    int32 Index = FindPatternIndex(PatternID);
    if (Index >= 0)
    {
        return LearnedPatterns[Index];
    }
    return FLearnedPattern();
}

TArray<FLearnedPattern> UOnlineLearningSystem::GetAllPatterns() const
{
    return LearnedPatterns;
}

TArray<FLearnedPattern> UOnlineLearningSystem::MatchPatterns(const TArray<FString>& CurrentConditions) const
{
    TArray<FLearnedPattern> Matched;

    for (const FLearnedPattern& Pattern : LearnedPatterns)
    {
        bool bAllConditionsMet = true;
        for (const FString& Condition : Pattern.Conditions)
        {
            if (!CurrentConditions.Contains(Condition))
            {
                bAllConditionsMet = false;
                break;
            }
        }

        if (bAllConditionsMet)
        {
            Matched.Add(Pattern);
        }
    }

    // Sort by confidence
    Matched.Sort([](const FLearnedPattern& A, const FLearnedPattern& B) {
        return A.Confidence > B.Confidence;
    });

    return Matched;
}

void UOnlineLearningSystem::ActivatePattern(const FString& PatternID, bool bSuccess)
{
    int32 Index = FindPatternIndex(PatternID);
    if (Index >= 0)
    {
        LearnedPatterns[Index].ActivationCount++;
        LearnedPatterns[Index].LastActivationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        // Update success rate
        float OldRate = LearnedPatterns[Index].SuccessRate;
        float NewRate = bSuccess ? 1.0f : 0.0f;
        LearnedPatterns[Index].SuccessRate = OldRate * 0.9f + NewRate * 0.1f;

        // Update confidence based on success
        if (bSuccess)
        {
            LearnedPatterns[Index].Confidence = FMath::Min(1.0f, LearnedPatterns[Index].Confidence + 0.05f);
        }
        else
        {
            LearnedPatterns[Index].Confidence = FMath::Max(0.0f, LearnedPatterns[Index].Confidence - 0.1f);
        }
    }
}

// ========================================
// SKILL ACQUISITION
// ========================================

FAcquiredSkill UOnlineLearningSystem::AcquireSkill(const FString& SkillName, const FString& Description,
                                                    const TArray<FString>& Prerequisites)
{
    // Check if skill already exists
    for (const FAcquiredSkill& Existing : AcquiredSkills)
    {
        if (Existing.SkillName == SkillName)
        {
            return Existing;
        }
    }

    // Check prerequisites
    if (!ArePrerequisitesMet(Prerequisites))
    {
        return FAcquiredSkill();
    }

    FAcquiredSkill Skill;
    Skill.SkillID = GenerateSkillID();
    Skill.SkillName = SkillName;
    Skill.Description = Description;
    Skill.Level = ESkillLevel::Novice;
    Skill.Proficiency = 0.0f;
    Skill.PracticeCount = 0;
    Skill.LastPracticeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Skill.Prerequisites = Prerequisites;

    AcquiredSkills.Add(Skill);

    OnSkillAcquired.Broadcast(Skill);

    return Skill;
}

void UOnlineLearningSystem::PracticeSkill(const FString& SkillID, float PerformanceScore)
{
    int32 Index = FindSkillIndex(SkillID);
    if (Index < 0)
    {
        return;
    }

    FAcquiredSkill& Skill = AcquiredSkills[Index];

    Skill.PracticeCount++;
    Skill.LastPracticeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Update proficiency based on performance
    float ProficiencyGain = PerformanceScore * 0.1f * (1.0f - Skill.Proficiency);
    Skill.Proficiency = FMath::Min(1.0f, Skill.Proficiency + ProficiencyGain);

    // Check for level up
    CheckSkillLevelUp(Skill);
}

FAcquiredSkill UOnlineLearningSystem::GetSkill(const FString& SkillID) const
{
    int32 Index = FindSkillIndex(SkillID);
    if (Index >= 0)
    {
        return AcquiredSkills[Index];
    }
    return FAcquiredSkill();
}

FAcquiredSkill UOnlineLearningSystem::GetSkillByName(const FString& SkillName) const
{
    for (const FAcquiredSkill& Skill : AcquiredSkills)
    {
        if (Skill.SkillName == SkillName)
        {
            return Skill;
        }
    }
    return FAcquiredSkill();
}

TArray<FAcquiredSkill> UOnlineLearningSystem::GetAllSkills() const
{
    return AcquiredSkills;
}

TArray<FAcquiredSkill> UOnlineLearningSystem::GetSkillsAtLevel(ESkillLevel MinLevel) const
{
    TArray<FAcquiredSkill> Filtered;
    for (const FAcquiredSkill& Skill : AcquiredSkills)
    {
        if (static_cast<uint8>(Skill.Level) >= static_cast<uint8>(MinLevel))
        {
            Filtered.Add(Skill);
        }
    }
    return Filtered;
}

bool UOnlineLearningSystem::ArePrerequisitesMet(const TArray<FString>& Prerequisites) const
{
    for (const FString& Prereq : Prerequisites)
    {
        bool bFound = false;
        for (const FAcquiredSkill& Skill : AcquiredSkills)
        {
            if (Skill.SkillName == Prereq && Skill.Level >= ESkillLevel::Beginner)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            return false;
        }
    }
    return true;
}

// ========================================
// LEARNING CONTROL
// ========================================

void UOnlineLearningSystem::SetLearningRate(float NewRate)
{
    LearningRate = FMath::Clamp(NewRate, 0.001f, 1.0f);
}

void UOnlineLearningSystem::SetExplorationRate(float NewRate)
{
    ExplorationRate = FMath::Clamp(NewRate, MinExplorationRate, 1.0f);
}

void UOnlineLearningSystem::DecayExploration()
{
    ExplorationRate = FMath::Max(MinExplorationRate, ExplorationRate * ExplorationDecay);
}

FLearningProgress UOnlineLearningSystem::GetLearningProgress() const
{
    FLearningProgress Progress;
    Progress.TotalExperiences = ExperienceBuffer.Num();
    Progress.PatternsLearned = LearnedPatterns.Num();
    Progress.SkillsAcquired = AcquiredSkills.Num();
    Progress.AverageReward = RewardCount > 0 ? TotalReward / RewardCount : 0.0f;
    Progress.LearningRate = LearningRate;
    Progress.ExplorationRate = ExplorationRate;

    return Progress;
}

void UOnlineLearningSystem::ResetLearning()
{
    ExperienceBuffer.Empty();
    QTable.Empty();
    LearnedPatterns.Empty();
    // Keep acquired skills

    TotalReward = 0.0f;
    RewardCount = 0;
    ExplorationRate = 0.3f;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UOnlineLearningSystem::UpdateSkillDecay(float DeltaTime)
{
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    for (FAcquiredSkill& Skill : AcquiredSkills)
    {
        float TimeSincePractice = CurrentTime - Skill.LastPracticeTime;
        if (TimeSincePractice > 60.0f) // Start decaying after 1 minute
        {
            float DecayAmount = SkillDecayRate * DeltaTime;
            Skill.Proficiency = FMath::Max(0.0f, Skill.Proficiency - DecayAmount);

            // Check for level down
            ESkillLevel NewLevel = ComputeSkillLevel(Skill.Proficiency);
            if (static_cast<uint8>(NewLevel) < static_cast<uint8>(Skill.Level))
            {
                Skill.Level = NewLevel;
            }
        }
    }
}

void UOnlineLearningSystem::PeriodicPatternExtraction()
{
    ExtractPatterns();
    DecayExploration();
}

FString UOnlineLearningSystem::MakeQKey(const FString& State, const FString& Action) const
{
    return State + TEXT("|") + Action;
}

float UOnlineLearningSystem::GetMaxQValue(const FString& State) const
{
    float MaxQ = 0.0f;
    bool bFound = false;

    for (const auto& Pair : QTable)
    {
        if (Pair.Value.State == State)
        {
            if (!bFound || Pair.Value.QValue > MaxQ)
            {
                MaxQ = Pair.Value.QValue;
                bFound = true;
            }
        }
    }

    return MaxQ;
}

ESkillLevel UOnlineLearningSystem::ComputeSkillLevel(float Proficiency) const
{
    if (Proficiency >= 0.95f) return ESkillLevel::Master;
    if (Proficiency >= 0.8f) return ESkillLevel::Expert;
    if (Proficiency >= 0.6f) return ESkillLevel::Advanced;
    if (Proficiency >= 0.4f) return ESkillLevel::Intermediate;
    if (Proficiency >= 0.2f) return ESkillLevel::Beginner;
    return ESkillLevel::Novice;
}

void UOnlineLearningSystem::CheckSkillLevelUp(FAcquiredSkill& Skill)
{
    ESkillLevel NewLevel = ComputeSkillLevel(Skill.Proficiency);
    if (static_cast<uint8>(NewLevel) > static_cast<uint8>(Skill.Level))
    {
        Skill.Level = NewLevel;
        OnSkillLevelUp.Broadcast(Skill.SkillID, NewLevel);
    }
}

FString UOnlineLearningSystem::GenerateExperienceID()
{
    return FString::Printf(TEXT("EXP_%d_%d"), ++ExperienceIDCounter, FMath::RandRange(1000, 9999));
}

FString UOnlineLearningSystem::GeneratePatternID()
{
    return FString::Printf(TEXT("PAT_%d_%d"), ++PatternIDCounter, FMath::RandRange(1000, 9999));
}

FString UOnlineLearningSystem::GenerateSkillID()
{
    return FString::Printf(TEXT("SKL_%d_%d"), ++SkillIDCounter, FMath::RandRange(1000, 9999));
}

int32 UOnlineLearningSystem::FindSkillIndex(const FString& SkillID) const
{
    for (int32 i = 0; i < AcquiredSkills.Num(); ++i)
    {
        if (AcquiredSkills[i].SkillID == SkillID)
        {
            return i;
        }
    }
    return -1;
}

int32 UOnlineLearningSystem::FindPatternIndex(const FString& PatternID) const
{
    for (int32 i = 0; i < LearnedPatterns.Num(); ++i)
    {
        if (LearnedPatterns[i].PatternID == PatternID)
        {
            return i;
        }
    }
    return -1;
}

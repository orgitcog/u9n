// GameSkillTrainingSystem.cpp
// Implementation of Game-Specific Skill Acquisition and Training

#include "GameSkillTrainingSystem.h"
#include "../Learning/OnlineLearningSystem.h"
#include "../4ECognition/EmbodiedCognitionComponent.h"

// ============================================================================
// FGameSkill Implementation
// ============================================================================

void FGameSkill::UpdateSuccessRate()
{
    if (PracticeCount > 0)
    {
        SuccessRate = static_cast<float>(SuccessCount) / static_cast<float>(PracticeCount);
    }
}

void FGameSkill::UpdateMastery()
{
    ESkillMastery PreviousMastery = Mastery;

    if (Proficiency < 0.2f)
    {
        Mastery = ESkillMastery::Learning;
    }
    else if (Proficiency < 0.4f)
    {
        Mastery = ESkillMastery::Competent;
    }
    else if (Proficiency < 0.6f)
    {
        Mastery = ESkillMastery::Proficient;
    }
    else if (Proficiency < 0.85f)
    {
        Mastery = ESkillMastery::Expert;
    }
    else
    {
        Mastery = ESkillMastery::Mastered;
    }
}

// ============================================================================
// UGameSkillTrainingSystem Implementation
// ============================================================================

UGameSkillTrainingSystem::UGameSkillTrainingSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f;  // Update once per second
}

void UGameSkillTrainingSystem::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();
    InitializeDefaultSkills();
}

void UGameSkillTrainingSystem::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateSkillDecay(DeltaTime);
    CheckCurriculumProgress();
}

void UGameSkillTrainingSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    ControllerInterface = Owner->FindComponentByClass<UGameControllerInterface>();
    TrainingEnvironment = Owner->FindComponentByClass<UGameTrainingEnvironment>();
    LearningSystem = Owner->FindComponentByClass<UOnlineLearningSystem>();
    EmbodimentComponent = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
}

void UGameSkillTrainingSystem::InitializeDefaultSkills()
{
    RegisterMovementSkills();
    RegisterCombatSkills();
}

void UGameSkillTrainingSystem::UpdateSkillDecay(float DeltaTime)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();

    for (auto& Pair : Skills)
    {
        FGameSkill& Skill = Pair.Value;

        // Only decay if above threshold and hasn't been practiced recently
        if (Skill.Proficiency > DecayThreshold)
        {
            float TimeSincePractice = CurrentTime - Skill.LastPracticeTime;
            float HoursSincePractice = TimeSincePractice / 3600.0f;

            if (HoursSincePractice > 1.0f)
            {
                float Decay = SkillDecayRate * (HoursSincePractice - 1.0f) * DeltaTime;
                Skill.Proficiency = FMath::Max(Skill.Proficiency - Decay, DecayThreshold);
                Skill.UpdateMastery();
            }
        }
    }
}

void UGameSkillTrainingSystem::CheckCurriculumProgress()
{
    if (Curriculum.Num() == 0 || CurrentStageIndex >= Curriculum.Num())
    {
        return;
    }

    FCurriculumStage& CurrentStage = Curriculum[CurrentStageIndex];
    if (CurrentStage.bCompleted)
    {
        return;
    }

    // Check if all skills in stage meet requirements
    bool bAllMet = true;
    for (const FString& SkillID : CurrentStage.SkillsToLearn)
    {
        if (FGameSkill* Skill = Skills.Find(SkillID))
        {
            if (Skill->Proficiency < CurrentStage.RequiredProficiency ||
                Skill->PracticeCount < CurrentStage.MinPracticeAttempts)
            {
                bAllMet = false;
                break;
            }
        }
        else
        {
            bAllMet = false;
            break;
        }
    }

    if (bAllMet)
    {
        CurrentStage.bCompleted = true;
        OnCurriculumStageCompleted.Broadcast(CurrentStage);
        AdvanceCurriculum();
    }
}

FString UGameSkillTrainingSystem::GenerateSkillID()
{
    return FString::Printf(TEXT("SKILL_%d"), ++SkillIDCounter);
}

int32 UGameSkillTrainingSystem::FindSkillIndex(const FString& SkillID) const
{
    // For TMap this doesn't apply, but kept for interface consistency
    return Skills.Contains(SkillID) ? 0 : -1;
}

// ============================================================================
// Skill Management
// ============================================================================

FGameSkill UGameSkillTrainingSystem::RegisterSkill(const FString& SkillName, EGameSkillCategory Category,
                                                    const TArray<FString>& Prerequisites, int32 Difficulty)
{
    FGameSkill Skill;
    Skill.SkillID = GenerateSkillID();
    Skill.SkillName = SkillName;
    Skill.Category = Category;
    Skill.Prerequisites = Prerequisites;
    Skill.Difficulty = FMath::Clamp(Difficulty, 1, 10);
    Skill.Mastery = ESkillMastery::Unknown;

    Skills.Add(Skill.SkillID, Skill);

    // Check if unlocked (prerequisites met)
    if (ArePrerequisitesMet(Skill.SkillID))
    {
        OnSkillUnlocked.Broadcast(Skill.SkillID, Skill);
    }

    return Skill;
}

FGameSkill UGameSkillTrainingSystem::RegisterSkillWithPattern(const FString& SkillName, EGameSkillCategory Category,
                                                               const TArray<FControllerInputState>& InputPattern,
                                                               float TimingWindow, int32 Difficulty)
{
    FGameSkill Skill = RegisterSkill(SkillName, Category, TArray<FString>(), Difficulty);

    // Update with pattern
    if (FGameSkill* StoredSkill = Skills.Find(Skill.SkillID))
    {
        StoredSkill->InputPattern = InputPattern;
        StoredSkill->TimingWindow = TimingWindow;
        StoredSkill->bIsCombo = InputPattern.Num() > 1;
    }

    return Skill;
}

FGameSkill UGameSkillTrainingSystem::GetSkill(const FString& SkillID) const
{
    if (const FGameSkill* Skill = Skills.Find(SkillID))
    {
        return *Skill;
    }
    return FGameSkill();
}

FGameSkill UGameSkillTrainingSystem::GetSkillByName(const FString& SkillName) const
{
    for (const auto& Pair : Skills)
    {
        if (Pair.Value.SkillName == SkillName)
        {
            return Pair.Value;
        }
    }
    return FGameSkill();
}

TArray<FGameSkill> UGameSkillTrainingSystem::GetAllSkills() const
{
    TArray<FGameSkill> Result;
    for (const auto& Pair : Skills)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FGameSkill> UGameSkillTrainingSystem::GetSkillsByCategory(EGameSkillCategory Category) const
{
    TArray<FGameSkill> Result;
    for (const auto& Pair : Skills)
    {
        if (Pair.Value.Category == Category)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FGameSkill> UGameSkillTrainingSystem::GetSkillsAtMastery(ESkillMastery MinMastery) const
{
    TArray<FGameSkill> Result;
    for (const auto& Pair : Skills)
    {
        if (static_cast<int32>(Pair.Value.Mastery) >= static_cast<int32>(MinMastery))
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

bool UGameSkillTrainingSystem::ArePrerequisitesMet(const FString& SkillID) const
{
    const FGameSkill* Skill = Skills.Find(SkillID);
    if (!Skill)
    {
        return false;
    }

    for (const FString& PrereqID : Skill->Prerequisites)
    {
        const FGameSkill* Prereq = Skills.Find(PrereqID);
        if (!Prereq || Prereq->Mastery < ESkillMastery::Competent)
        {
            return false;
        }
    }

    return true;
}

TArray<FGameSkill> UGameSkillTrainingSystem::GetAvailableSkills() const
{
    TArray<FGameSkill> Result;
    for (const auto& Pair : Skills)
    {
        if (ArePrerequisitesMet(Pair.Key) && Pair.Value.Mastery < ESkillMastery::Mastered)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

// ============================================================================
// Skill Practice
// ============================================================================

FSkillAttempt UGameSkillTrainingSystem::RecordAttempt(const FString& SkillID, bool bSuccess, float Quality,
                                                       const TArray<FControllerInputState>& ActualInputs,
                                                       const FString& Context)
{
    FSkillAttempt Attempt;
    Attempt.SkillID = SkillID;
    Attempt.Timestamp = GetWorld()->GetTimeSeconds();
    Attempt.bSuccess = bSuccess;
    Attempt.Quality = FMath::Clamp(Quality, 0.0f, 1.0f);
    Attempt.Context = Context;
    Attempt.ActualInputs = ActualInputs;

    // Compute timing and input accuracy
    FGameSkill* Skill = Skills.Find(SkillID);
    if (Skill && Skill->InputPattern.Num() > 0)
    {
        Attempt.InputAccuracy = GetPatternMatchQuality(SkillID, ActualInputs);
    }
    else
    {
        Attempt.InputAccuracy = Quality;
    }

    // Update skill stats
    if (Skill)
    {
        UpdateSkillProficiency(*Skill, Attempt);
    }

    // Record attempt
    RecentAttempts.Add(Attempt);
    while (RecentAttempts.Num() > MaxRecentAttempts)
    {
        RecentAttempts.RemoveAt(0);
    }

    // Update practice session if active
    if (bInPracticeSession && CurrentSession.SkillID == SkillID)
    {
        CurrentSession.Attempts++;
        if (bSuccess)
        {
            CurrentSession.Successes++;
        }
        CurrentSession.QualityHistory.Add(Quality);

        float Sum = 0.0f;
        for (float Q : CurrentSession.QualityHistory)
        {
            Sum += Q;
        }
        CurrentSession.AverageQuality = Sum / CurrentSession.QualityHistory.Num();
    }

    // Broadcast events
    OnSkillAttempted.Broadcast(SkillID, Attempt);
    BroadcastToLearningSystem(*Skill, Attempt);

    return Attempt;
}

FSkillAttempt UGameSkillTrainingSystem::EvaluateExecution(const FString& SkillID,
                                                          const TArray<FControllerInputState>& Inputs)
{
    float MatchQuality = GetPatternMatchQuality(SkillID, Inputs);
    bool bSuccess = MatchQuality >= 0.7f;

    return RecordAttempt(SkillID, bSuccess, MatchQuality, Inputs, TEXT("Auto-Evaluated"));
}

void UGameSkillTrainingSystem::StartPracticeSession(const FString& SkillID)
{
    if (bInPracticeSession)
    {
        EndPracticeSession();
    }

    CurrentSession = FSkillPracticeSession();
    CurrentSession.SkillID = SkillID;
    CurrentSession.StartTime = GetWorld()->GetTimeSeconds();
    bInPracticeSession = true;

    // Record starting proficiency for improvement tracking
    if (const FGameSkill* Skill = Skills.Find(SkillID))
    {
        CurrentSession.Improvement = -Skill->Proficiency;  // Will add final proficiency later
    }

    OnPracticeSessionStarted.Broadcast(SkillID);
}

FSkillPracticeSession UGameSkillTrainingSystem::EndPracticeSession()
{
    if (!bInPracticeSession)
    {
        return FSkillPracticeSession();
    }

    CurrentSession.EndTime = GetWorld()->GetTimeSeconds();

    // Calculate improvement
    if (const FGameSkill* Skill = Skills.Find(CurrentSession.SkillID))
    {
        CurrentSession.Improvement += Skill->Proficiency;
    }

    bInPracticeSession = false;

    OnPracticeSessionEnded.Broadcast(CurrentSession);

    return CurrentSession;
}

FSkillPracticeSession UGameSkillTrainingSystem::GetCurrentSession() const
{
    return CurrentSession;
}

bool UGameSkillTrainingSystem::IsPracticing() const
{
    return bInPracticeSession;
}

TArray<FSkillAttempt> UGameSkillTrainingSystem::GetRecentAttempts(const FString& SkillID, int32 Count) const
{
    TArray<FSkillAttempt> Result;

    for (int32 i = RecentAttempts.Num() - 1; i >= 0 && Result.Num() < Count; --i)
    {
        if (RecentAttempts[i].SkillID == SkillID)
        {
            Result.Add(RecentAttempts[i]);
        }
    }

    return Result;
}

void UGameSkillTrainingSystem::UpdateSkillProficiency(FGameSkill& Skill, const FSkillAttempt& Attempt)
{
    ESkillMastery PreviousMastery = Skill.Mastery;

    // Update counts
    Skill.PracticeCount++;
    if (Attempt.bSuccess)
    {
        Skill.SuccessCount++;
    }
    Skill.UpdateSuccessRate();

    // Update quality tracking
    if (Attempt.Quality > Skill.BestQuality)
    {
        Skill.BestQuality = Attempt.Quality;
    }

    Skill.AverageQuality = (Skill.AverageQuality * (Skill.PracticeCount - 1) + Attempt.Quality) / Skill.PracticeCount;

    // Compute learning rate
    float LearningRate = ComputeLearningRate(Skill);

    // Update proficiency
    float ProficiencyDelta = 0.0f;
    if (Attempt.bSuccess)
    {
        // Positive learning from success
        ProficiencyDelta = LearningRate * Attempt.Quality * (1.0f - Skill.Proficiency);
    }
    else
    {
        // Small negative from failure (helps avoid bad habits)
        ProficiencyDelta = -LearningRate * 0.1f * Skill.Proficiency;
    }

    Skill.Proficiency = FMath::Clamp(Skill.Proficiency + ProficiencyDelta, 0.0f, 1.0f);
    Skill.LastPracticeTime = GetWorld()->GetTimeSeconds();

    // Update mastery
    Skill.UpdateMastery();

    // Broadcast mastery change
    if (Skill.Mastery != PreviousMastery)
    {
        OnSkillMasteryChanged.Broadcast(Skill.SkillID, Skill.Mastery);
    }
}

float UGameSkillTrainingSystem::ComputeLearningRate(const FGameSkill& Skill) const
{
    float Rate = BaseLearningRate;

    // Difficulty modifier (harder skills learn slower)
    Rate *= (11 - Skill.Difficulty) / 10.0f;

    // Current proficiency modifier (diminishing returns)
    Rate *= (1.0f - Skill.Proficiency * 0.5f);

    // Adaptive difficulty modifier
    if (bAdaptiveDifficulty && Skill.PracticeCount > 10)
    {
        // Increase learning rate if struggling
        if (Skill.SuccessRate < 0.3f)
        {
            Rate *= 1.5f;
        }
        // Decrease if too easy
        else if (Skill.SuccessRate > 0.9f)
        {
            Rate *= 0.7f;
        }
    }

    return Rate;
}

void UGameSkillTrainingSystem::BroadcastToLearningSystem(const FGameSkill& Skill, const FSkillAttempt& Attempt)
{
    if (!LearningSystem)
    {
        return;
    }

    // Record as experience for reinforcement learning
    float Reward = Attempt.bSuccess ? Attempt.Quality : -0.1f;

    TArray<FString> Tags;
    Tags.Add(TEXT("SkillPractice"));
    Tags.Add(Skill.SkillName);
    Tags.Add(FString::Printf(TEXT("Category_%d"), static_cast<int32>(Skill.Category)));

    LearningSystem->RecordExperience(
        FString::Printf(TEXT("Skill_%s_Pre"), *Skill.SkillID),
        Attempt.ActualInputs.Num() > 0 ? Attempt.ActualInputs[0].ToActionString() : TEXT("NoInput"),
        FString::Printf(TEXT("Skill_%s_Post"), *Skill.SkillID),
        Reward,
        Tags,
        false
    );

    // Practice the corresponding cognitive skill
    LearningSystem->PracticeSkill(Skill.SkillID, Attempt.Quality);

    // Update sensorimotor contingency
    if (EmbodimentComponent)
    {
        EmbodimentComponent->LearnContingency(
            Skill.SkillName,
            Attempt.bSuccess ? TEXT("Success") : TEXT("Failure"),
            Attempt.bSuccess ? TEXT("Success") : TEXT("Failure")
        );
    }
}

// ============================================================================
// Input Pattern Matching
// ============================================================================

TArray<FString> UGameSkillTrainingSystem::MatchInputToSkills(const TArray<FControllerInputState>& Inputs) const
{
    TArray<FString> MatchedSkills;

    for (const auto& Pair : Skills)
    {
        const FGameSkill& Skill = Pair.Value;
        if (Skill.InputPattern.Num() > 0)
        {
            float Match = GetPatternMatchQuality(Skill.SkillID, Inputs);
            if (Match >= (1.0f - PatternMatchTolerance))
            {
                MatchedSkills.Add(Skill.SkillID);
            }
        }
    }

    return MatchedSkills;
}

float UGameSkillTrainingSystem::GetPatternMatchQuality(const FString& SkillID,
                                                        const TArray<FControllerInputState>& Inputs) const
{
    const FGameSkill* Skill = Skills.Find(SkillID);
    if (!Skill || Skill->InputPattern.Num() == 0)
    {
        return 0.0f;
    }

    return ComputeSequenceSimilarity(Skill->InputPattern, Inputs);
}

float UGameSkillTrainingSystem::ComputeInputSimilarity(const FControllerInputState& A,
                                                        const FControllerInputState& B) const
{
    float Similarity = 0.0f;
    int32 Dimensions = 0;

    // Compare analog values
    Similarity += 1.0f - FMath::Abs(A.LeftStickX - B.LeftStickX);
    Similarity += 1.0f - FMath::Abs(A.LeftStickY - B.LeftStickY);
    Similarity += 1.0f - FMath::Abs(A.RightStickX - B.RightStickX);
    Similarity += 1.0f - FMath::Abs(A.RightStickY - B.RightStickY);
    Similarity += 1.0f - FMath::Abs(A.LeftTrigger - B.LeftTrigger);
    Similarity += 1.0f - FMath::Abs(A.RightTrigger - B.RightTrigger);
    Dimensions += 6;

    // Compare button sets
    int32 CommonButtons = 0;
    int32 TotalButtons = FMath::Max(A.PressedButtons.Num(), B.PressedButtons.Num());

    if (TotalButtons > 0)
    {
        for (const EGamepadButton& Button : A.PressedButtons)
        {
            if (B.PressedButtons.Contains(Button))
            {
                CommonButtons++;
            }
        }
        Similarity += static_cast<float>(CommonButtons) / static_cast<float>(TotalButtons);
        Dimensions++;
    }
    else
    {
        Similarity += 1.0f;  // Both have no buttons pressed
        Dimensions++;
    }

    return Similarity / Dimensions;
}

float UGameSkillTrainingSystem::ComputeSequenceSimilarity(const TArray<FControllerInputState>& A,
                                                           const TArray<FControllerInputState>& B) const
{
    if (A.Num() == 0 || B.Num() == 0)
    {
        return 0.0f;
    }

    // Use dynamic time warping for sequence comparison
    int32 N = A.Num();
    int32 M = B.Num();

    TArray<TArray<float>> DTW;
    DTW.SetNum(N + 1);
    for (int32 i = 0; i <= N; ++i)
    {
        DTW[i].SetNum(M + 1);
        for (int32 j = 0; j <= M; ++j)
        {
            DTW[i][j] = FLT_MAX;
        }
    }
    DTW[0][0] = 0.0f;

    for (int32 i = 1; i <= N; ++i)
    {
        for (int32 j = 1; j <= M; ++j)
        {
            float Cost = 1.0f - ComputeInputSimilarity(A[i-1], B[j-1]);
            DTW[i][j] = Cost + FMath::Min3(
                DTW[i-1][j],      // Insertion
                DTW[i][j-1],      // Deletion
                DTW[i-1][j-1]     // Match
            );
        }
    }

    // Convert distance to similarity (0-1)
    float MaxDistance = static_cast<float>(FMath::Max(N, M));
    float Similarity = 1.0f - FMath::Clamp(DTW[N][M] / MaxDistance, 0.0f, 1.0f);

    return Similarity;
}

FString UGameSkillTrainingSystem::DetectSkillExecution(const TArray<FControllerInputState>& RecentInputs)
{
    FString BestMatch;
    float BestQuality = 0.0f;

    for (const auto& Pair : Skills)
    {
        const FGameSkill& Skill = Pair.Value;
        if (Skill.InputPattern.Num() > 0 && RecentInputs.Num() >= Skill.InputPattern.Num())
        {
            // Check recent inputs against pattern
            TArray<FControllerInputState> Window;
            int32 StartIdx = RecentInputs.Num() - Skill.InputPattern.Num();
            for (int32 i = StartIdx; i < RecentInputs.Num(); ++i)
            {
                Window.Add(RecentInputs[i]);
            }

            float Quality = GetPatternMatchQuality(Skill.SkillID, Window);
            if (Quality > BestQuality && Quality >= (1.0f - PatternMatchTolerance))
            {
                BestQuality = Quality;
                BestMatch = Skill.SkillID;
            }
        }
    }

    return BestMatch;
}

// ============================================================================
// Curriculum
// ============================================================================

void UGameSkillTrainingSystem::AddCurriculumStage(const FCurriculumStage& Stage)
{
    Curriculum.Add(Stage);
}

FCurriculumStage UGameSkillTrainingSystem::GetCurrentStage() const
{
    if (CurrentStageIndex < Curriculum.Num())
    {
        return Curriculum[CurrentStageIndex];
    }
    return FCurriculumStage();
}

TArray<FCurriculumStage> UGameSkillTrainingSystem::GetCurriculum() const
{
    return Curriculum;
}

bool UGameSkillTrainingSystem::AdvanceCurriculum()
{
    if (CurrentStageIndex < Curriculum.Num() - 1)
    {
        CurrentStageIndex++;
        return true;
    }
    return false;
}

FString UGameSkillTrainingSystem::GetRecommendedSkill() const
{
    // Prioritize skills from current curriculum stage
    if (CurrentStageIndex < Curriculum.Num())
    {
        const FCurriculumStage& Stage = Curriculum[CurrentStageIndex];
        for (const FString& SkillID : Stage.SkillsToLearn)
        {
            if (const FGameSkill* Skill = Skills.Find(SkillID))
            {
                if (Skill->Proficiency < Stage.RequiredProficiency)
                {
                    return SkillID;
                }
            }
        }
    }

    // Otherwise find skill with lowest proficiency that has prerequisites met
    FString BestSkill;
    float LowestProficiency = 1.0f;

    for (const auto& Pair : Skills)
    {
        if (ArePrerequisitesMet(Pair.Key) &&
            Pair.Value.Mastery < ESkillMastery::Mastered &&
            Pair.Value.Proficiency < LowestProficiency)
        {
            LowestProficiency = Pair.Value.Proficiency;
            BestSkill = Pair.Key;
        }
    }

    return BestSkill;
}

void UGameSkillTrainingSystem::LoadPresetCurriculum(EGameGenre Genre)
{
    Curriculum.Empty();
    CurrentStageIndex = 0;

    switch (Genre)
    {
        case EGameGenre::ActionRPG:
        case EGameGenre::FPS:
        {
            FCurriculumStage Stage1;
            Stage1.StageName = TEXT("Basic Movement");
            Stage1.SkillsToLearn = {TEXT("Walk"), TEXT("Run"), TEXT("Jump")};
            Stage1.RequiredProficiency = 0.5f;
            Stage1.MinPracticeAttempts = 20;
            AddCurriculumStage(Stage1);

            FCurriculumStage Stage2;
            Stage2.StageName = TEXT("Combat Basics");
            Stage2.SkillsToLearn = {TEXT("BasicAttack"), TEXT("Block"), TEXT("Dodge")};
            Stage2.RequiredProficiency = 0.5f;
            Stage2.MinPracticeAttempts = 30;
            AddCurriculumStage(Stage2);

            FCurriculumStage Stage3;
            Stage3.StageName = TEXT("Advanced Combat");
            Stage3.SkillsToLearn = {TEXT("HeavyAttack"), TEXT("Parry"), TEXT("ComboAttack")};
            Stage3.RequiredProficiency = 0.6f;
            Stage3.MinPracticeAttempts = 50;
            AddCurriculumStage(Stage3);

            break;
        }

        case EGameGenre::Fighting:
        {
            FCurriculumStage Stage1;
            Stage1.StageName = TEXT("Basic Moves");
            Stage1.SkillsToLearn = {TEXT("LightPunch"), TEXT("LightKick"), TEXT("Block")};
            Stage1.RequiredProficiency = 0.6f;
            AddCurriculumStage(Stage1);

            FCurriculumStage Stage2;
            Stage2.StageName = TEXT("Heavy Attacks");
            Stage2.SkillsToLearn = {TEXT("HeavyPunch"), TEXT("HeavyKick")};
            Stage2.RequiredProficiency = 0.6f;
            AddCurriculumStage(Stage2);

            FCurriculumStage Stage3;
            Stage3.StageName = TEXT("Basic Combos");
            Stage3.SkillsToLearn = {TEXT("TwoHitCombo"), TEXT("ThreeHitCombo")};
            Stage3.RequiredProficiency = 0.5f;
            AddCurriculumStage(Stage3);

            break;
        }

        case EGameGenre::Racing:
        {
            FCurriculumStage Stage1;
            Stage1.StageName = TEXT("Basic Control");
            Stage1.SkillsToLearn = {TEXT("Accelerate"), TEXT("Brake"), TEXT("Steer")};
            Stage1.RequiredProficiency = 0.6f;
            AddCurriculumStage(Stage1);

            FCurriculumStage Stage2;
            Stage2.StageName = TEXT("Racing Lines");
            Stage2.SkillsToLearn = {TEXT("ApexTurn"), TEXT("TrailBrake")};
            Stage2.RequiredProficiency = 0.5f;
            AddCurriculumStage(Stage2);

            FCurriculumStage Stage3;
            Stage3.StageName = TEXT("Advanced");
            Stage3.SkillsToLearn = {TEXT("Drift"), TEXT("Boost")};
            Stage3.RequiredProficiency = 0.5f;
            AddCurriculumStage(Stage3);

            break;
        }

        default:
            // Generic curriculum
            FCurriculumStage Stage1;
            Stage1.StageName = TEXT("Fundamentals");
            Stage1.SkillsToLearn = {TEXT("Walk"), TEXT("Run"), TEXT("Jump")};
            Stage1.RequiredProficiency = 0.5f;
            AddCurriculumStage(Stage1);
            break;
    }
}

// ============================================================================
// Analytics
// ============================================================================

TArray<float> UGameSkillTrainingSystem::GetLearningCurve(const FString& SkillID) const
{
    TArray<float> Curve;

    for (const FSkillAttempt& Attempt : RecentAttempts)
    {
        if (Attempt.SkillID == SkillID)
        {
            Curve.Add(Attempt.Quality);
        }
    }

    return Curve;
}

TMap<EGameSkillCategory, float> UGameSkillTrainingSystem::GetSkillProfile() const
{
    TMap<EGameSkillCategory, float> Profile;
    TMap<EGameSkillCategory, int32> Counts;

    for (const auto& Pair : Skills)
    {
        const FGameSkill& Skill = Pair.Value;
        if (float* CurrentSum = Profile.Find(Skill.Category))
        {
            *CurrentSum += Skill.Proficiency;
            Counts[Skill.Category]++;
        }
        else
        {
            Profile.Add(Skill.Category, Skill.Proficiency);
            Counts.Add(Skill.Category, 1);
        }
    }

    // Compute averages
    for (auto& ProfilePair : Profile)
    {
        if (int32* Count = Counts.Find(ProfilePair.Key))
        {
            if (*Count > 0)
            {
                ProfilePair.Value /= *Count;
            }
        }
    }

    return Profile;
}

float UGameSkillTrainingSystem::EstimateTimeToMastery(const FString& SkillID) const
{
    const FGameSkill* Skill = Skills.Find(SkillID);
    if (!Skill)
    {
        return -1.0f;
    }

    float RemainingProficiency = 0.85f - Skill->Proficiency;
    if (RemainingProficiency <= 0.0f)
    {
        return 0.0f;
    }

    // Estimate based on current learning rate and practice frequency
    float EffectiveLearningRate = ComputeLearningRate(*Skill);
    float AverageQuality = Skill->AverageQuality > 0 ? Skill->AverageQuality : 0.5f;

    // Approximate time assuming 1 attempt per 10 seconds on average
    float AttemptsNeeded = RemainingProficiency / (EffectiveLearningRate * AverageQuality * 0.5f);
    float SecondsNeeded = AttemptsNeeded * 10.0f;

    return SecondsNeeded;
}

TMap<FString, float> UGameSkillTrainingSystem::GetSkillCorrelations(const FString& SkillID) const
{
    TMap<FString, float> Correlations;

    const FGameSkill* TargetSkill = Skills.Find(SkillID);
    if (!TargetSkill)
    {
        return Correlations;
    }

    for (const auto& Pair : Skills)
    {
        if (Pair.Key == SkillID)
        {
            continue;
        }

        // Simple correlation: same category = higher correlation
        float Correlation = 0.0f;
        if (Pair.Value.Category == TargetSkill->Category)
        {
            Correlation += 0.5f;
        }

        // Prerequisites suggest strong correlation
        if (Pair.Value.Prerequisites.Contains(SkillID))
        {
            Correlation += 0.4f;
        }
        if (TargetSkill->Prerequisites.Contains(Pair.Key))
        {
            Correlation += 0.4f;
        }

        // Similar difficulty
        int32 DiffDiff = FMath::Abs(Pair.Value.Difficulty - TargetSkill->Difficulty);
        Correlation += (10 - DiffDiff) * 0.01f;

        Correlations.Add(Pair.Key, FMath::Clamp(Correlation, 0.0f, 1.0f));
    }

    return Correlations;
}

// ============================================================================
// Presets
// ============================================================================

void UGameSkillTrainingSystem::LoadSkillPresets(EGameGenre Genre)
{
    Skills.Empty();
    SkillIDCounter = 0;

    RegisterMovementSkills();

    switch (Genre)
    {
        case EGameGenre::Fighting:
            RegisterCombatSkills();
            // Register fighting-specific skills
            RegisterSkill(TEXT("LightPunch"), EGameSkillCategory::Combat, {}, 2);
            RegisterSkill(TEXT("HeavyPunch"), EGameSkillCategory::Combat, {TEXT("LightPunch")}, 3);
            RegisterSkill(TEXT("LightKick"), EGameSkillCategory::Combat, {}, 2);
            RegisterSkill(TEXT("HeavyKick"), EGameSkillCategory::Combat, {TEXT("LightKick")}, 3);
            RegisterSkill(TEXT("TwoHitCombo"), EGameSkillCategory::Combo, {TEXT("LightPunch")}, 4);
            RegisterSkill(TEXT("ThreeHitCombo"), EGameSkillCategory::Combo, {TEXT("TwoHitCombo")}, 5);
            break;

        case EGameGenre::Racing:
            RegisterSkill(TEXT("Accelerate"), EGameSkillCategory::Movement, {}, 1);
            RegisterSkill(TEXT("Brake"), EGameSkillCategory::Movement, {}, 1);
            RegisterSkill(TEXT("Steer"), EGameSkillCategory::Movement, {}, 2);
            RegisterSkill(TEXT("ApexTurn"), EGameSkillCategory::Timing, {TEXT("Steer")}, 4);
            RegisterSkill(TEXT("TrailBrake"), EGameSkillCategory::Timing, {TEXT("Brake")}, 5);
            RegisterSkill(TEXT("Drift"), EGameSkillCategory::Timing, {TEXT("ApexTurn")}, 6);
            RegisterSkill(TEXT("Boost"), EGameSkillCategory::Resource, {}, 2);
            break;

        default:
            RegisterCombatSkills();
            break;
    }

    LoadPresetCurriculum(Genre);
}

void UGameSkillTrainingSystem::RegisterMovementSkills()
{
    RegisterSkill(TEXT("Walk"), EGameSkillCategory::Movement, {}, 1);
    RegisterSkill(TEXT("Run"), EGameSkillCategory::Movement, {TEXT("Walk")}, 2);
    RegisterSkill(TEXT("Jump"), EGameSkillCategory::Movement, {}, 2);
    RegisterSkill(TEXT("Crouch"), EGameSkillCategory::Movement, {}, 1);
    RegisterSkill(TEXT("Sprint"), EGameSkillCategory::Movement, {TEXT("Run")}, 3);
    RegisterSkill(TEXT("DoubleJump"), EGameSkillCategory::Movement, {TEXT("Jump")}, 4);
    RegisterSkill(TEXT("WallJump"), EGameSkillCategory::Movement, {TEXT("DoubleJump")}, 6);
}

void UGameSkillTrainingSystem::RegisterCombatSkills()
{
    RegisterSkill(TEXT("BasicAttack"), EGameSkillCategory::Combat, {}, 2);
    RegisterSkill(TEXT("HeavyAttack"), EGameSkillCategory::Combat, {TEXT("BasicAttack")}, 3);
    RegisterSkill(TEXT("Block"), EGameSkillCategory::Defense, {}, 2);
    RegisterSkill(TEXT("Dodge"), EGameSkillCategory::Defense, {}, 3);
    RegisterSkill(TEXT("Parry"), EGameSkillCategory::Defense, {TEXT("Block")}, 5);
    RegisterSkill(TEXT("ComboAttack"), EGameSkillCategory::Combo, {TEXT("BasicAttack"), TEXT("HeavyAttack")}, 5);
    RegisterSkill(TEXT("CounterAttack"), EGameSkillCategory::Combat, {TEXT("Parry")}, 6);
}

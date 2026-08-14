// GameTrainingEnvironment.cpp
// Implementation of Game State and Training Episode Management

#include "GameTrainingEnvironment.h"
#include "GameControllerInterface.h"
#include "../Learning/OnlineLearningSystem.h"
#include "../4ECognition/EmbodiedCognitionComponent.h"

// ============================================================================
// FGameStateObservation Implementation
// ============================================================================

TArray<float> FGameStateObservation::ToNormalizedVector() const
{
    TArray<float> Vector;
    Vector.Reserve(50 + RawObservation.Num());

    // Add raw observation first
    Vector.Append(RawObservation);

    // Position (normalized by typical world scale)
    Vector.Add(PlayerPosition.X / 10000.0f);
    Vector.Add(PlayerPosition.Y / 10000.0f);
    Vector.Add(PlayerPosition.Z / 1000.0f);

    // Velocity (normalized)
    Vector.Add(PlayerVelocity.X / 1000.0f);
    Vector.Add(PlayerVelocity.Y / 1000.0f);
    Vector.Add(PlayerVelocity.Z / 1000.0f);

    // Rotation (normalized to -1 to 1)
    Vector.Add(PlayerRotation.Pitch / 180.0f);
    Vector.Add(PlayerRotation.Yaw / 180.0f);
    Vector.Add(PlayerRotation.Roll / 180.0f);

    // Health and stamina (already 0-1)
    Vector.Add(Health);
    Vector.Add(Stamina);

    // Score (normalized)
    Vector.Add(FMath::Clamp(Score / 10000.0f, -1.0f, 1.0f));

    // Time remaining (normalized)
    Vector.Add(TimeRemaining > 0 ? FMath::Clamp(TimeRemaining / 300.0f, 0.0f, 1.0f) : -1.0f);

    // Objective direction and distance
    FVector ToObjective = ObjectivePosition - PlayerPosition;
    float ObjectiveDistance = ToObjective.Size();
    if (ObjectiveDistance > 0.01f)
    {
        ToObjective.Normalize();
        Vector.Add(ToObjective.X);
        Vector.Add(ToObjective.Y);
        Vector.Add(ToObjective.Z);
    }
    else
    {
        Vector.Add(0.0f);
        Vector.Add(0.0f);
        Vector.Add(0.0f);
    }
    Vector.Add(FMath::Clamp(ObjectiveDistance / 1000.0f, 0.0f, 1.0f));

    // Binary flags
    Vector.Add(bIsGrounded ? 1.0f : 0.0f);
    Vector.Add(bInCombat ? 1.0f : 0.0f);

    // Game phase (one-hot would be better but simplified here)
    Vector.Add(static_cast<float>(GamePhase) / 10.0f);

    // Nearby entities (first 10, normalized directions)
    for (int32 i = 0; i < 10; ++i)
    {
        if (i < NearbyEntities.Num())
        {
            FVector ToEntity = NearbyEntities[i] - PlayerPosition;
            float EntityDistance = ToEntity.Size();
            if (EntityDistance > 0.01f)
            {
                ToEntity.Normalize();
                Vector.Add(ToEntity.X);
                Vector.Add(ToEntity.Y);
                Vector.Add(ToEntity.Z);
                Vector.Add(FMath::Clamp(EntityDistance / 500.0f, 0.0f, 1.0f));
            }
            else
            {
                Vector.Add(0.0f);
                Vector.Add(0.0f);
                Vector.Add(0.0f);
                Vector.Add(0.0f);
            }
        }
        else
        {
            // Padding for missing entities
            Vector.Add(0.0f);
            Vector.Add(0.0f);
            Vector.Add(0.0f);
            Vector.Add(-1.0f);  // -1 indicates no entity
        }
    }

    return Vector;
}

FString FGameStateObservation::GetStateHash() const
{
    // Create discretized state hash for Q-learning
    int32 PosX = FMath::RoundToInt(PlayerPosition.X / 100.0f);
    int32 PosY = FMath::RoundToInt(PlayerPosition.Y / 100.0f);
    int32 PosZ = FMath::RoundToInt(PlayerPosition.Z / 50.0f);
    int32 Yaw = FMath::RoundToInt(PlayerRotation.Yaw / 45.0f) % 8;
    int32 HealthBucket = FMath::RoundToInt(Health * 4);
    int32 StaminaBucket = FMath::RoundToInt(Stamina * 4);

    return FString::Printf(TEXT("P%d_%d_%d_Y%d_H%d_S%d_C%d_G%d"),
        PosX, PosY, PosZ, Yaw, HealthBucket, StaminaBucket,
        bInCombat ? 1 : 0, bIsGrounded ? 1 : 0);
}

// ============================================================================
// FRewardBreakdown Implementation
// ============================================================================

void FRewardBreakdown::ComputeTotal()
{
    Total = Progress + Survival + Combat + Exploration + Efficiency + SkillExecution + Penalty;
}

// ============================================================================
// UGameTrainingEnvironment Implementation
// ============================================================================

UGameTrainingEnvironment::UGameTrainingEnvironment()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UGameTrainingEnvironment::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();
    InitializeEnvironment();
}

void UGameTrainingEnvironment::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEpisodeActive)
    {
        return;
    }

    // Update observation at configured rate
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastObservationTime >= ObservationInterval)
    {
        UpdateObservation();
        LastObservationTime = CurrentTime;
    }

    // Check for episode termination
    CheckEpisodeTermination();
}

void UGameTrainingEnvironment::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    ControllerInterface = Owner->FindComponentByClass<UGameControllerInterface>();
    LearningSystem = Owner->FindComponentByClass<UOnlineLearningSystem>();
    EmbodimentComponent = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
}

void UGameTrainingEnvironment::InitializeEnvironment()
{
    ObservationInterval = 1.0f / ObservationRate;
    LoadGenrePreset(GameGenre);
}

// ============================================================================
// Environment Interface
// ============================================================================

FGameStateObservation UGameTrainingEnvironment::Reset()
{
    // End previous episode if active
    if (bEpisodeActive)
    {
        EndEpisode(EEpisodeTermination::UserAbort);
    }

    // Start new episode
    Stats.TotalEpisodes++;
    bEpisodeActive = true;
    CurrentStep = 0;
    EpisodeStartTime = GetWorld()->GetTimeSeconds();
    AccumulatedReward = 0.0f;

    CurrentEpisode = FTrainingEpisode();
    CurrentEpisode.EpisodeNumber = Stats.TotalEpisodes;
    CurrentEpisode.StartTime = EpisodeStartTime;

    // Clear exploration tracking
    ExploredCells.Empty();

    // Reset observation
    PreviousObservation = FGameStateObservation();
    CurrentObservation = FGameStateObservation();
    CurrentObservation.Timestamp = EpisodeStartTime;

    CurrentEpisode.InitialState = CurrentObservation;

    OnEpisodeStarted.Broadcast(CurrentEpisode.EpisodeNumber);

    return CurrentObservation;
}

void UGameTrainingEnvironment::Step(const TArray<float>& Action, FGameStateObservation& NextState,
                                     float& Reward, bool& bDone, FString& Info)
{
    if (!bEpisodeActive)
    {
        NextState = CurrentObservation;
        Reward = 0.0f;
        bDone = true;
        Info = TEXT("Episode not active");
        return;
    }

    // Store previous state
    PreviousObservation = CurrentObservation;

    // Execute action through controller interface
    if (ControllerInterface)
    {
        FControllerInputState InputState = FControllerInputState::FromActionVector(Action);
        FControllerOutputCommand Command;
        Command.DesiredState = InputState;
        Command.Duration = ObservationInterval;
        ControllerInterface->QueueOutputCommand(Command);
    }

    // Update step count
    CurrentStep++;
    CurrentEpisode.StepCount = CurrentStep;

    // Observation will be updated in tick, but we need immediate feedback
    UpdateObservation();

    // Compute reward
    FRewardBreakdown RewardBreakdown = ComputeReward(PreviousObservation, CurrentObservation);
    Reward = RewardBreakdown.Total;
    AccumulatedReward += Reward;

    // Record experience
    ApplyRewardToLearning(Reward, TEXT("Step"));

    OnRewardReceived.Broadcast(RewardBreakdown);
    OnStepCompleted.Broadcast(CurrentObservation, Reward);

    // Check termination
    bDone = false;
    if (CurrentStep >= MaxStepsPerEpisode)
    {
        EndEpisode(EEpisodeTermination::Timeout);
        bDone = true;
        Info = TEXT("Max steps reached");
    }
    else if (CurrentObservation.Health <= 0.0f)
    {
        EndEpisode(EEpisodeTermination::Death);
        bDone = true;
        Info = TEXT("Player died");
    }

    NextState = CurrentObservation;
}

FGameStateObservation UGameTrainingEnvironment::GetObservation() const
{
    return CurrentObservation;
}

void UGameTrainingEnvironment::EndEpisode(EEpisodeTermination Reason)
{
    if (!bEpisodeActive)
    {
        return;
    }

    bEpisodeActive = false;

    CurrentEpisode.EndTime = GetWorld()->GetTimeSeconds();
    CurrentEpisode.Duration = CurrentEpisode.EndTime - CurrentEpisode.StartTime;
    CurrentEpisode.TotalReward = AccumulatedReward;
    CurrentEpisode.TerminationReason = Reason;
    CurrentEpisode.FinalState = CurrentObservation;
    CurrentEpisode.FinalScore = CurrentObservation.Score;

    // Apply terminal rewards
    if (Reason == EEpisodeTermination::Success)
    {
        AccumulatedReward += RewardConfig.SuccessBonus;
        Stats.SuccessfulEpisodes++;
    }
    else if (Reason == EEpisodeTermination::Death)
    {
        AccumulatedReward += RewardConfig.DeathPenalty;
    }

    CurrentEpisode.TotalReward = AccumulatedReward;

    // Record to history
    RecordEpisodeToHistory();
    UpdateStatistics();

    OnEpisodeEnded.Broadcast(CurrentEpisode);

    // Auto-reset if configured
    if (bAutoReset)
    {
        Reset();
    }
}

bool UGameTrainingEnvironment::IsEpisodeActive() const
{
    return bEpisodeActive;
}

int32 UGameTrainingEnvironment::GetCurrentStep() const
{
    return CurrentStep;
}

float UGameTrainingEnvironment::GetEpisodeElapsedTime() const
{
    if (!bEpisodeActive)
    {
        return 0.0f;
    }
    return GetWorld()->GetTimeSeconds() - EpisodeStartTime;
}

// ============================================================================
// State Management
// ============================================================================

void UGameTrainingEnvironment::SetPlayerState(const FVector& Position, const FRotator& Rotation,
                                               float Health, float Stamina)
{
    CurrentObservation.PlayerPosition = Position;
    CurrentObservation.PlayerRotation = Rotation;
    CurrentObservation.Health = FMath::Clamp(Health, 0.0f, 1.0f);
    CurrentObservation.Stamina = FMath::Clamp(Stamina, 0.0f, 1.0f);
}

void UGameTrainingEnvironment::SetObjective(const FVector& Position, const FString& ObjectiveType)
{
    CurrentObservation.ObjectivePosition = Position;
    CurrentObservation.DistanceToObjective = FVector::Dist(CurrentObservation.PlayerPosition, Position);
}

void UGameTrainingEnvironment::AddNearbyEntity(const FVector& Position, const FString& EntityType)
{
    CurrentObservation.NearbyEntities.Add(Position);
    CurrentObservation.EntityTypes.Add(EntityType);
}

void UGameTrainingEnvironment::ClearNearbyEntities()
{
    CurrentObservation.NearbyEntities.Empty();
    CurrentObservation.EntityTypes.Empty();
}

void UGameTrainingEnvironment::SetCustomData(const FString& Key, float Value)
{
    CurrentObservation.CustomData.Add(Key, Value);
}

void UGameTrainingEnvironment::SetScore(float NewScore)
{
    CurrentObservation.Score = NewScore;
}

void UGameTrainingEnvironment::AddScore(float Delta)
{
    CurrentObservation.Score += Delta;
}

void UGameTrainingEnvironment::UpdateObservation()
{
    // Get player character if available
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CurrentObservation.PlayerPosition = Owner->GetActorLocation();
        CurrentObservation.PlayerRotation = Owner->GetActorRotation();

        // Compute velocity from position delta
        if (PreviousObservation.Timestamp > 0)
        {
            float DeltaTime = CurrentObservation.Timestamp - PreviousObservation.Timestamp;
            if (DeltaTime > 0)
            {
                CurrentObservation.PlayerVelocity =
                    (CurrentObservation.PlayerPosition - PreviousObservation.PlayerPosition) / DeltaTime;
            }
        }
    }

    // Update distance to objective
    CurrentObservation.DistanceToObjective =
        FVector::Dist(CurrentObservation.PlayerPosition, CurrentObservation.ObjectivePosition);

    CurrentObservation.Timestamp = GetWorld()->GetTimeSeconds();

    OnStateObserved.Broadcast(CurrentObservation);
}

void UGameTrainingEnvironment::CheckEpisodeTermination()
{
    if (!bEpisodeActive)
    {
        return;
    }

    float ElapsedTime = GetEpisodeElapsedTime();

    // Timeout
    if (ElapsedTime >= MaxEpisodeDuration)
    {
        EndEpisode(EEpisodeTermination::Timeout);
        return;
    }

    // Death
    if (CurrentObservation.Health <= 0.0f)
    {
        EndEpisode(EEpisodeTermination::Death);
        return;
    }
}

// ============================================================================
// Reward System
// ============================================================================

FRewardBreakdown UGameTrainingEnvironment::ComputeReward(const FGameStateObservation& PrevState,
                                                          const FGameStateObservation& CurrState)
{
    FRewardBreakdown Reward;

    // Progress reward (moving toward objective)
    float PrevDist = FVector::Dist(PrevState.PlayerPosition, PrevState.ObjectivePosition);
    float CurrDist = FVector::Dist(CurrState.PlayerPosition, CurrState.ObjectivePosition);
    float ProgressDelta = PrevDist - CurrDist;
    Reward.Progress = ProgressDelta * RewardConfig.ProgressWeight / 100.0f;

    // Survival reward (small positive for staying alive)
    Reward.Survival = RewardConfig.SurvivalWeight * ObservationInterval;

    // Health change penalty
    float HealthDelta = CurrState.Health - PrevState.Health;
    if (HealthDelta < 0)
    {
        Reward.Penalty += HealthDelta * 2.0f;  // Penalty for taking damage
    }

    // Exploration reward
    if (IsNewExploration(CurrState.PlayerPosition))
    {
        Reward.Exploration = RewardConfig.ExplorationWeight;
    }

    // Efficiency (reward for moving, penalize idling)
    float Speed = CurrState.PlayerVelocity.Size();
    Reward.Efficiency = FMath::Min(Speed / 500.0f, 1.0f) * RewardConfig.EfficiencyWeight * 0.1f;

    Reward.ComputeTotal();

    // Apply scaling and clipping
    Reward.Total *= RewardConfig.RewardScale;
    Reward.Total = FMath::Clamp(Reward.Total, RewardConfig.RewardClipMin, RewardConfig.RewardClipMax);

    return Reward;
}

void UGameTrainingEnvironment::ApplyReward(float Reward, const FString& Category)
{
    AccumulatedReward += Reward;
    ApplyRewardToLearning(Reward, Category);
}

void UGameTrainingEnvironment::ApplyPenalty(float Penalty, const FString& Reason)
{
    AccumulatedReward -= FMath::Abs(Penalty);
    ApplyRewardToLearning(-FMath::Abs(Penalty), Reason);
}

void UGameTrainingEnvironment::SignalCombatHit(float DamageDealt)
{
    float Reward = DamageDealt * RewardConfig.CombatWeight / 100.0f;
    ApplyReward(Reward, TEXT("CombatHit"));
}

void UGameTrainingEnvironment::SignalCombatDamage(float DamageReceived)
{
    float Penalty = DamageReceived * RewardConfig.CombatWeight / 50.0f;
    ApplyPenalty(Penalty, TEXT("CombatDamage"));
}

void UGameTrainingEnvironment::SignalSkillSuccess(const FString& SkillName, float Quality)
{
    float Reward = Quality * RewardConfig.SkillWeight;
    ApplyReward(Reward, FString::Printf(TEXT("Skill:%s"), *SkillName));
}

void UGameTrainingEnvironment::SignalExploration(const FVector& NewArea)
{
    if (IsNewExploration(NewArea))
    {
        ApplyReward(RewardConfig.ExplorationWeight, TEXT("Exploration"));
    }
}

FIntVector UGameTrainingEnvironment::WorldToCell(const FVector& WorldPos) const
{
    return FIntVector(
        FMath::FloorToInt(WorldPos.X / ExplorationCellSize),
        FMath::FloorToInt(WorldPos.Y / ExplorationCellSize),
        FMath::FloorToInt(WorldPos.Z / ExplorationCellSize)
    );
}

bool UGameTrainingEnvironment::IsNewExploration(const FVector& Position)
{
    FIntVector Cell = WorldToCell(Position);
    if (!ExploredCells.Contains(Cell))
    {
        ExploredCells.Add(Cell);
        return true;
    }
    return false;
}

void UGameTrainingEnvironment::ApplyRewardToLearning(float Reward, const FString& Context)
{
    if (LearningSystem)
    {
        LearningSystem->ProvideReward(Reward, Context);

        // Record experience
        TArray<FString> Tags;
        Tags.Add(Context);
        Tags.Add(FString::Printf(TEXT("Episode%d"), CurrentEpisode.EpisodeNumber));

        LearningSystem->RecordExperience(
            PreviousObservation.GetStateHash(),
            CurrentObservation.GetStateHash(),
            CurrentObservation.GetStateHash(),
            Reward,
            Tags,
            !bEpisodeActive
        );
    }
}

// ============================================================================
// Training Control
// ============================================================================

void UGameTrainingEnvironment::SetTrainingMode(ETrainingMode Mode)
{
    TrainingMode = Mode;

    // Adjust exploration rate based on mode
    switch (Mode)
    {
        case ETrainingMode::Exploration:
            ExplorationRate = 0.5f;
            break;
        case ETrainingMode::Exploitation:
            ExplorationRate = 0.1f;
            break;
        case ETrainingMode::Imitation:
            ExplorationRate = 0.0f;
            break;
        case ETrainingMode::Evaluation:
            ExplorationRate = 0.0f;
            break;
        case ETrainingMode::CurriculumEasy:
            CurriculumDifficulty = 0.25f;
            break;
        case ETrainingMode::CurriculumMed:
            CurriculumDifficulty = 0.5f;
            break;
        case ETrainingMode::CurriculumHard:
            CurriculumDifficulty = 0.75f;
            break;
        default:
            break;
    }

    if (LearningSystem)
    {
        LearningSystem->SetExplorationRate(ExplorationRate);
    }

    OnTrainingModeChanged.Broadcast(Mode);
}

float UGameTrainingEnvironment::GetExplorationRate() const
{
    return ExplorationRate;
}

void UGameTrainingEnvironment::SetExplorationRate(float Rate)
{
    ExplorationRate = FMath::Clamp(Rate, 0.0f, 1.0f);

    if (LearningSystem)
    {
        LearningSystem->SetExplorationRate(ExplorationRate);
    }
}

float UGameTrainingEnvironment::GetCurriculumDifficulty() const
{
    return CurriculumDifficulty;
}

void UGameTrainingEnvironment::SetCurriculumDifficulty(float Difficulty)
{
    CurriculumDifficulty = FMath::Clamp(Difficulty, 0.0f, 1.0f);
}

void UGameTrainingEnvironment::AdvanceCurriculum()
{
    // Advance difficulty based on performance
    if (Stats.WinRate > 0.7f)
    {
        CurriculumDifficulty = FMath::Min(CurriculumDifficulty + 0.1f, 1.0f);
    }
    else if (Stats.WinRate < 0.3f)
    {
        CurriculumDifficulty = FMath::Max(CurriculumDifficulty - 0.1f, 0.0f);
    }

    // Update training mode based on difficulty
    if (CurriculumDifficulty < 0.33f)
    {
        SetTrainingMode(ETrainingMode::CurriculumEasy);
    }
    else if (CurriculumDifficulty < 0.66f)
    {
        SetTrainingMode(ETrainingMode::CurriculumMed);
    }
    else
    {
        SetTrainingMode(ETrainingMode::CurriculumHard);
    }
}

// ============================================================================
// Statistics
// ============================================================================

FTrainingStats UGameTrainingEnvironment::GetTrainingStats() const
{
    return Stats;
}

FTrainingEpisode UGameTrainingEnvironment::GetCurrentEpisode() const
{
    return CurrentEpisode;
}

TArray<FTrainingEpisode> UGameTrainingEnvironment::GetEpisodeHistory() const
{
    return EpisodeHistory;
}

FTrainingEpisode UGameTrainingEnvironment::GetBestEpisode() const
{
    FTrainingEpisode Best;
    for (const FTrainingEpisode& Episode : EpisodeHistory)
    {
        if (Episode.TotalReward > Best.TotalReward)
        {
            Best = Episode;
        }
    }
    return Best;
}

void UGameTrainingEnvironment::ClearStatistics()
{
    Stats = FTrainingStats();
    EpisodeHistory.Empty();
}

void UGameTrainingEnvironment::RecordEpisodeToHistory()
{
    if (!bRecordHistory)
    {
        return;
    }

    EpisodeHistory.Add(CurrentEpisode);

    // Limit history size
    while (EpisodeHistory.Num() > MaxHistorySize)
    {
        EpisodeHistory.RemoveAt(0);
    }
}

void UGameTrainingEnvironment::UpdateStatistics()
{
    Stats.TotalSteps += CurrentEpisode.StepCount;
    Stats.TotalTrainingTime += CurrentEpisode.Duration;

    // Update win rate
    Stats.WinRate = Stats.TotalEpisodes > 0 ?
        static_cast<float>(Stats.SuccessfulEpisodes) / Stats.TotalEpisodes : 0.0f;

    // Update reward statistics
    if (CurrentEpisode.TotalReward > Stats.BestReward)
    {
        Stats.BestReward = CurrentEpisode.TotalReward;
    }

    // Moving average reward
    Stats.RecentRewards.Add(CurrentEpisode.TotalReward);
    while (Stats.RecentRewards.Num() > 100)
    {
        Stats.RecentRewards.RemoveAt(0);
    }

    float Sum = 0.0f;
    for (float R : Stats.RecentRewards)
    {
        Sum += R;
    }
    Stats.MovingAverageReward = Stats.RecentRewards.Num() > 0 ?
        Sum / Stats.RecentRewards.Num() : 0.0f;

    // Overall average
    Stats.AverageReward = Stats.TotalEpisodes > 0 ?
        (Stats.AverageReward * (Stats.TotalEpisodes - 1) + CurrentEpisode.TotalReward) / Stats.TotalEpisodes : 0.0f;

    Stats.AverageDuration = Stats.TotalEpisodes > 0 ?
        Stats.TotalTrainingTime / Stats.TotalEpisodes : 0.0f;
}

// ============================================================================
// Presets
// ============================================================================

void UGameTrainingEnvironment::LoadGenrePreset(EGameGenre Genre)
{
    GameGenre = Genre;

    switch (Genre)
    {
        case EGameGenre::ActionRPG:
            RewardConfig.ProgressWeight = 1.0f;
            RewardConfig.SurvivalWeight = 0.2f;
            RewardConfig.CombatWeight = 0.8f;
            RewardConfig.ExplorationWeight = 0.3f;
            RewardConfig.EfficiencyWeight = 0.1f;
            RewardConfig.SkillWeight = 0.5f;
            break;

        case EGameGenre::FPS:
            RewardConfig.ProgressWeight = 0.5f;
            RewardConfig.SurvivalWeight = 0.3f;
            RewardConfig.CombatWeight = 1.0f;
            RewardConfig.ExplorationWeight = 0.1f;
            RewardConfig.EfficiencyWeight = 0.2f;
            RewardConfig.SkillWeight = 0.4f;
            RewardConfig.DeathPenalty = -10.0f;
            break;

        case EGameGenre::Fighting:
            RewardConfig.ProgressWeight = 0.0f;
            RewardConfig.SurvivalWeight = 0.1f;
            RewardConfig.CombatWeight = 1.5f;
            RewardConfig.ExplorationWeight = 0.0f;
            RewardConfig.EfficiencyWeight = 0.0f;
            RewardConfig.SkillWeight = 1.0f;
            break;

        case EGameGenre::Racing:
            RewardConfig.ProgressWeight = 1.5f;
            RewardConfig.SurvivalWeight = 0.1f;
            RewardConfig.CombatWeight = 0.0f;
            RewardConfig.ExplorationWeight = 0.0f;
            RewardConfig.EfficiencyWeight = 0.5f;
            RewardConfig.SkillWeight = 0.3f;
            break;

        case EGameGenre::Platformer:
            RewardConfig.ProgressWeight = 1.0f;
            RewardConfig.SurvivalWeight = 0.5f;
            RewardConfig.CombatWeight = 0.2f;
            RewardConfig.ExplorationWeight = 0.5f;
            RewardConfig.EfficiencyWeight = 0.3f;
            RewardConfig.SkillWeight = 0.4f;
            break;

        case EGameGenre::Survival:
            RewardConfig.ProgressWeight = 0.3f;
            RewardConfig.SurvivalWeight = 1.0f;
            RewardConfig.CombatWeight = 0.5f;
            RewardConfig.ExplorationWeight = 0.8f;
            RewardConfig.EfficiencyWeight = 0.4f;
            RewardConfig.SkillWeight = 0.3f;
            break;

        case EGameGenre::Puzzle:
            RewardConfig.ProgressWeight = 1.5f;
            RewardConfig.SurvivalWeight = 0.0f;
            RewardConfig.CombatWeight = 0.0f;
            RewardConfig.ExplorationWeight = 0.2f;
            RewardConfig.EfficiencyWeight = 1.0f;
            RewardConfig.SkillWeight = 0.5f;
            break;

        default:
            // Use default values
            break;
    }
}

FString UGameTrainingEnvironment::SaveConfiguration() const
{
    // Simplified JSON export
    FString Config = TEXT("{");
    Config += FString::Printf(TEXT("\"genre\": %d,"), static_cast<int32>(GameGenre));
    Config += FString::Printf(TEXT("\"progressWeight\": %.2f,"), RewardConfig.ProgressWeight);
    Config += FString::Printf(TEXT("\"survivalWeight\": %.2f,"), RewardConfig.SurvivalWeight);
    Config += FString::Printf(TEXT("\"combatWeight\": %.2f,"), RewardConfig.CombatWeight);
    Config += FString::Printf(TEXT("\"explorationWeight\": %.2f,"), RewardConfig.ExplorationWeight);
    Config += FString::Printf(TEXT("\"efficiencyWeight\": %.2f,"), RewardConfig.EfficiencyWeight);
    Config += FString::Printf(TEXT("\"skillWeight\": %.2f"), RewardConfig.SkillWeight);
    Config += TEXT("}");
    return Config;
}

bool UGameTrainingEnvironment::LoadConfiguration(const FString& ConfigJson)
{
    // Simplified - in production would use FJsonSerializer
    return true;
}

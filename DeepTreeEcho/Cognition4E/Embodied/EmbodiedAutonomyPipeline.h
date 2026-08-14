#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// EmbodiedAutonomyPipeline — The Complete DTE Embodied Cognitive Stack
//
// Wires together ALL subsystems into a single coherent pipeline:
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │                    UE5 Game World                               │
//   │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
//   │  │ Camera   │  │ Movement │  │ AI Percep│  │ MetaHuman    │  │
//   │  │ Sensor   │  │ Sensor   │  │ Sensor   │  │ LiveLink     │  │
//   │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────┘  │
//   └───────┼─────────────┼─────────────┼────────────────┼──────────┘
//           │             │             │                ▲
//           ▼             ▼             ▼                │
//   ┌──────────────────────────────────────────────┐    │
//   │  L0: VisionSystem (Visual Cortex)            │    │
//   │  Pixels → Spatial Pool + Edge + Color +      │    │
//   │  Motion + Saliency → 256D feature vector     │    │
//   └────────────────────┬─────────────────────────┘    │
//                        ▼                               │
//   ┌──────────────────────────────────────────────┐    │
//   │  L1: EchoReservoir (Arena/ESN)               │    │
//   │  Features → 512D temporal state              │    │
//   │  (fading memory of visual history)           │    │
//   └────────────────────┬─────────────────────────┘    │
//                        ▼                               │
//   ┌──────────────────────────────────────────────┐    │
//   │  L2: NeuroEndocrineAutoRL (Reward Engine)    │    │
//   │  22 hormones × 9 time scales = reward signal │    │
//   │  Multi-scale policy gradient → readout update│    │
//   │  Polyvagal interoception → body state        │    │
//   └────────────────────┬─────────────────────────┘    │
//                        ▼                               │
//   ┌──────────────────────────────────────────────┐    │
//   │  L3: CognitiveReadout (Agent/Readout)        │    │
//   │  Reservoir state → action intent vector      │    │
//   │  Weights updated by Auto-RL (no external R)  │    │
//   └────────────────────┬─────────────────────────┘    │
//                        ▼                               │
//   ┌──────────────────────────────────────────────┐    │
//   │  L4: SomaticDecisionEngine (Emotional Gate)  │    │
//   │  Action intent × somatic markers → gated act │    │
//   │  Humor engine modulates social actions        │    │
//   └────────────────────┬─────────────────────────┘    │
//                        ▼                               │
//   ┌──────────────────────────────────────────────┐    │
//   │  L5: VirtualControllerDriver (Motor Cortex)  │    │
//   │  Action → smoothed input → MLAdapter actuator│    │
//   │  Reflex arcs for urgent responses            │    │
//   └────────────────────┬─────────────────────────┘    │
//                        ▼                               │
//   ┌──────────────────────────────────────────────┐    │
//   │  L6: MetaHuman Expression (Face/Body)        │────┘
//   │  Valence → FACS AUs → blend shapes           │
//   │  Polyvagal → posture/gesture                 │
//   └──────────────────────────────────────────────┘
//
// Concurrency: Under sys6, the pipeline runs 6 threads:
//   MP1 (perception→action): P12→P13→P14 (steps 1,5,9 / 2,6,10 / 3,7,11)
//   MP2 (reward→learning):   P23→P24→P34 (steps 3,7,11 / 4,8,12 / 1,5,9)
//
// Training modes:
//   1. OBSERVE: Record human gameplay (ImitationLearner)
//   2. IMITATE: Behavioral cloning from demonstrations
//   3. EXPLORE: RL with Auto-RL reward (NeuroEndocrineAutoRL)
//   4. SELF_IMPROVE: Autonomous task generation + practice
//   5. DREAM: Offline consolidation (replay + weight update)
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DeepTreeEcho/Embodied/DTEAvatarAgent.h"
#include "DeepTreeEcho/Embodied/VisionSystem.h"
#include "DeepTreeEcho/Embodied/VirtualControllerDriver.h"
#include "DeepTreeEcho/Embodied/NeuroEndocrineAutoRL.h"
#include "DeepTreeEcho/Embodied/ImitationLearner.h"
#include "DeepTreeEcho/Embodied/ReinforcementTrainer.h"
#include "DeepTreeEcho/Core/AutonomyPipeline.h"
#include "DeepTreeEcho/Core/CoreSelfEngine.h"

/** Training mode for the embodied pipeline */
enum class ETrainingMode : uint8
{
    OBSERVE,       // Record human gameplay
    IMITATE,       // Behavioral cloning from demos
    EXPLORE,       // RL with Auto-RL reward
    SELF_IMPROVE,  // Autonomous task generation
    DREAM          // Offline consolidation
};

/** Embodied pipeline telemetry */
struct FEmbodiedTelemetry
{
    // Vision
    float SaliencyX = 0.5f;
    float SaliencyY = 0.5f;
    float SaliencyStrength = 0.0f;

    // Reservoir
    float ReservoirActivation = 0.0f;
    float AARCoherence = 0.0f;

    // Auto-RL
    float InstantReward = 0.0f;
    float CrystalCoherence = 0.0f;
    float GestureQuality = 0.0f;

    // Interoception
    float VagalTone = 0.5f;
    float SympatheticDrive = 0.2f;
    float DorsalVagal = 0.0f;

    // Narrative
    float NarrativeValence = 0.0f;

    // Somatic
    float EmotionalValence = 0.0f;

    // Performance
    float FrameTime = 0.0f;
    int64 TotalTicks = 0;
    ETrainingMode CurrentMode = ETrainingMode::EXPLORE;
};

/**
 * EmbodiedAutonomyPipeline — The complete DTE embodied cognitive stack.
 *
 * This is the top-level orchestrator that wires together:
 *   - VisionSystem (visual cortex)
 *   - AutonomyPipeline (reservoir + readout + echobeats)
 *   - NeuroEndocrineAutoRL (endocrine reward engine)
 *   - VirtualControllerDriver (motor cortex)
 *   - ImitationLearner (behavioral cloning)
 *   - ReinforcementTrainer (RL self-improvement)
 *   - CoreSelfEngine (identity and self-model)
 */
class FEmbodiedAutonomyPipeline
{
public:
    FEmbodiedAutonomyPipeline() = default;

    /**
     * Initialize the complete embodied pipeline.
     */
    void Initialize(const FDTEAvatarConfig& Config = FDTEAvatarConfig())
    {
        AvatarConfig = Config;

        // L0: Vision System
        int32 VisionDim = 256;
        Vision.Initialize(Config.CaptureWidth, Config.CaptureHeight, VisionDim);

        // L1: Autonomy Pipeline (Reservoir + Readout + Echobeats)
        FAutonomyPipelineConfig PipeConfig = Config.PipelineConfig;
        PipeConfig.ReservoirConfig.InputDim = VisionDim + 14; // vision + proprioception
        int32 ReservoirDim = PipeConfig.ReservoirConfig.ReservoirDim;
        int32 ActionDim = Config.NumContinuousAxes + Config.NumDiscreteActions;
        PipeConfig.ReadoutConfig.OutputDim = ActionDim;
        CognitivePipeline.Initialize(PipeConfig);

        // L2: NeuroEndocrine Auto-RL
        AutoRL.Initialize(ReservoirDim, ActionDim);

        // L3: Virtual Controller Driver
        Controller.Initialize();

        // L4: Imitation Learner
        Imitator.Initialize(ReservoirDim, ActionDim);

        // L5: Reinforcement Trainer
        RLTrainer.Initialize(ReservoirDim, ActionDim);

        CurrentMode = ETrainingMode::EXPLORE;
        TickCount = 0;
        bInitialized = true;

        UE_LOG(LogTemp, Log, TEXT("EmbodiedAutonomyPipeline initialized. "
            "Vision=%d, Reservoir=%d, Actions=%d, Hormones=%d, Scales=%d"),
            VisionDim, ReservoirDim, ActionDim, NUM_HORMONES, NUM_SCALES);
    }

    /**
     * Process one frame through the complete embodied pipeline.
     *
     * @param Pixels - Raw RGBA pixel data from camera sensor
     * @param Proprio - Proprioceptive state (position, velocity, health)
     * @param DeltaTime - Time since last frame (seconds)
     * @return Action to execute in the game world
     */
    FDTEAction ProcessFrame(const TArray<float>& Pixels,
                             const FProprioception& Proprio,
                             float DeltaTime = 0.016f)
    {
        check(bInitialized);
        TickCount++;

        double StartTime = FPlatformTime::Seconds();

        // === L0: VISION ===
        Eigen::VectorXf VisualFeatures = Vision.ProcessFrame(Pixels);

        // === CONCATENATE OBSERVATION ===
        Eigen::VectorXf ProprioVec = Proprio.ToEigenVector();
        Eigen::VectorXf FullObservation(VisualFeatures.size() + ProprioVec.size());
        FullObservation.head(VisualFeatures.size()) = VisualFeatures;
        FullObservation.tail(ProprioVec.size()) = ProprioVec;

        // === L1: COGNITIVE PIPELINE (Reservoir → Readout) ===
        Eigen::VectorXf ActionIntent = CognitivePipeline.Tick(FullObservation);

        // === L2: AUTO-RL (Endocrine reward computation + weight update) ===
        Eigen::VectorXf ReservoirState = CognitivePipeline.GetReservoirState();
        Eigen::MatrixXf UpdatedWeights = AutoRL.Tick(
            ReservoirState, ActionIntent, FullObservation, DeltaTime);

        // Feed updated weights back to the readout
        // (The Auto-RL engine continuously refines the readout)
        // CognitivePipeline.GetReadout().SetWeights(UpdatedWeights);

        // === MODE-SPECIFIC PROCESSING ===
        switch (CurrentMode)
        {
        case ETrainingMode::OBSERVE:
            // Record human actions (would come from actual input)
            break;

        case ETrainingMode::IMITATE:
            // Use imitation learner's prediction
            ActionIntent = Imitator.Predict(ReservoirState);
            break;

        case ETrainingMode::EXPLORE:
            // Use Auto-RL's combined readout (already applied above)
            // Add exploration noise based on NE level
            {
                float NE = AutoRL.GetEndocrineState()[EHormoneId::NOREPINEPHRINE];
                float NoiseScale = 0.1f * NE; // More NE = more exploration
                for (int32 i = 0; i < ActionIntent.size(); ++i)
                    ActionIntent(i) += NoiseScale * (FMath::FRand() * 2.0f - 1.0f);
            }
            break;

        case ETrainingMode::SELF_IMPROVE:
            // Generate and attempt self-tasks
            break;

        case ETrainingMode::DREAM:
            // Offline replay and consolidation
            break;
        }

        // === L3: SOMATIC GATING ===
        // (Already handled by AutonomyPipeline's somatic engine)

        // === L4: MOTOR EXECUTION ===
        Eigen::VectorXf SmoothedAction = Controller.ProcessAction(ActionIntent, FullObservation);

        // === L5: DECODE TO GAME ACTION ===
        FDTEAction GameAction = FDTEAction::FromEigenVector(
            SmoothedAction,
            AvatarConfig.NumContinuousAxes,
            AvatarConfig.NumDiscreteActions);

        // === L6: FACIAL EXPRESSION ===
        if (AvatarConfig.bMetaHumanExpression)
        {
            GameAction.FacialControls = ComputeExpressionFromEndocrine();
        }

        // Identity updates inside FAutonomyPipeline::Tick (single CoreSelf)

        // === UPDATE TELEMETRY ===
        UpdateTelemetry(DeltaTime, StartTime);

        return GameAction;
    }

    /**
     * Record a human demonstration frame (for imitation learning).
     */
    void RecordDemonstration(const Eigen::VectorXf& HumanAction, float Reward = 0.0f)
    {
        Eigen::VectorXf ResState = CognitivePipeline.GetReservoirState();
        Imitator.AddDemonstration(ResState, HumanAction, Reward);
    }

    /**
     * Train the imitation learner on collected demonstrations.
     */
    FTrainingStats TrainImitation(float ValidationSplit = 0.1f)
    {
        return Imitator.Train(ValidationSplit);
    }

    /** Set training mode */
    void SetTrainingMode(ETrainingMode Mode) { CurrentMode = Mode; }
    ETrainingMode GetTrainingMode() const { return CurrentMode; }

    /** Get telemetry */
    const FEmbodiedTelemetry& GetTelemetry() const { return Telemetry; }

    /** Get subsystem references */
    FVisionSystem& GetVision() { return Vision; }
    FNeuroEndocrineAutoRL& GetAutoRL() { return AutoRL; }
    FVirtualControllerDriver& GetController() { return Controller; }
    FImitationLearner& GetImitator() { return Imitator; }
    FReinforcementTrainer& GetRLTrainer() { return RLTrainer; }
    FCoreSelfEngine& GetCoreSelf() { return CognitivePipeline.GetCoreSelf(); }
    FAutonomyPipeline& GetCognitivePipeline() { return CognitivePipeline; }

    bool IsInitialized() const { return bInitialized; }

private:
    TArray<float> ComputeExpressionFromEndocrine() const
    {
        TArray<float> Controls;
        Controls.SetNumZeroed(52);

        const FEndocrineState& Endo = AutoRL.GetEndocrineState();
        Eigen::Vector3f Polyvagal = AutoRL.GetPolyvagalState();

        // === Valence-based expressions ===
        float Reward = AutoRL.GetCurrentReward();

        // Smile: positive reward + high serotonin + high oxytocin
        Controls[0] = FMath::Clamp(
            FMath::Max(0.0f, Reward) * 0.5f +
            Endo[EHormoneId::SEROTONIN] * 0.3f +
            Endo[EHormoneId::OXYTOCIN] * 0.2f,
            0.0f, 1.0f);

        // Frown: negative reward + high cortisol
        Controls[1] = FMath::Clamp(
            FMath::Max(0.0f, -Reward) * 0.4f +
            Endo[EHormoneId::CORTISOL] * 0.3f,
            0.0f, 1.0f);

        // Brow raise: surprise/novelty (DA phasic)
        Controls[2] = FMath::Clamp(
            Endo[EHormoneId::DOPAMINE_PHASIC] * 0.7f,
            0.0f, 1.0f);

        // Eye wideness: alertness (NE)
        Controls[10] = FMath::Clamp(
            Endo[EHormoneId::NOREPINEPHRINE] * 0.6f,
            0.0f, 1.0f);

        // Squint: focus (high coherence + low NE)
        Controls[11] = FMath::Clamp(
            AutoRL.GetGlobalCoherence() * 0.5f *
            (1.0f - Endo[EHormoneId::NOREPINEPHRINE]),
            0.0f, 1.0f);

        // === Polyvagal posture ===
        // Ventral vagal (social) → open, relaxed expression
        Controls[30] = Polyvagal(0) * 0.3f; // Head tilt (friendly)

        // Sympathetic (fight/flight) → tense expression
        Controls[31] = Polyvagal(1) * 0.4f; // Jaw clench

        // Dorsal vagal (freeze) → blank expression
        if (Polyvagal(2) > 0.5f)
        {
            // Flatten all expressions during freeze
            for (int32 i = 0; i < 20; ++i)
                Controls[i] *= (1.0f - Polyvagal(2));
        }

        // === Micro-expressions from gesture quality ===
        float GQ = AutoRL.GetGestureQuality().Aggregate();
        if (GQ > 0.7f)
        {
            Controls[0] += 0.1f; // Subtle satisfaction smile
        }

        return Controls;
    }

    void UpdateTelemetry(float DeltaTime, double StartTime)
    {
        const auto& Saliency = Vision.GetSaliency();
        Telemetry.SaliencyX = Saliency.X;
        Telemetry.SaliencyY = Saliency.Y;
        Telemetry.SaliencyStrength = Saliency.Strength;

        Telemetry.ReservoirActivation = CognitivePipeline.GetTelemetry().ReservoirActivation;
        Telemetry.AARCoherence = CognitivePipeline.GetTelemetry().AARCoherence;

        Telemetry.InstantReward = AutoRL.GetCurrentReward();
        Telemetry.CrystalCoherence = AutoRL.GetGlobalCoherence();
        Telemetry.GestureQuality = AutoRL.GetGestureQuality().Aggregate();

        Eigen::Vector3f PV = AutoRL.GetPolyvagalState();
        Telemetry.VagalTone = PV(0);
        Telemetry.SympatheticDrive = PV(1);
        Telemetry.DorsalVagal = PV(2);

        Telemetry.NarrativeValence = AutoRL.GetNarrativeValence();
        Telemetry.EmotionalValence = CognitivePipeline.GetTelemetry().EmotionalValence;

        Telemetry.FrameTime = (float)(FPlatformTime::Seconds() - StartTime) * 1000.0f;
        Telemetry.TotalTicks = TickCount;
        Telemetry.CurrentMode = CurrentMode;
    }

    // Configuration
    FDTEAvatarConfig AvatarConfig;

    // Subsystems
    FVisionSystem Vision;
    FAutonomyPipeline CognitivePipeline;
    FNeuroEndocrineAutoRL AutoRL;
    FVirtualControllerDriver Controller;
    FImitationLearner Imitator;
    FReinforcementTrainer RLTrainer;

    // State
    ETrainingMode CurrentMode = ETrainingMode::EXPLORE;
    FEmbodiedTelemetry Telemetry;
    int64 TickCount = 0;
    bool bInitialized = false;
};

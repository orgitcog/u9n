#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// AutonomyPipeline — Master Orchestrator for DTE Cognitive Architecture
// Wires all subsystems into a single coherent cognitive loop:
//   IonCognitiveShell → Echobeats → AAR(Reservoir, Readout) → Introspection
//   → SomaticDecision → HumorEngine → Membrane → PersonaBackup
//
// Implements the 6-level autonomy hierarchy:
//   L0: Reactive (stimulus-response)
//   L1: Deliberative (plan-then-act)
//   L2: Reflective (monitor-and-adjust)
//   L3: Self-Modifying (change own parameters)
//   L4: Self-Improving (change own architecture)
//   L5: True Autonomy (self-directed evolution)
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DeepTreeEcho/NanEcho/DteNodes/EchoReservoirNode.h"
#include "DeepTreeEcho/NanEcho/DteNodes/CognitiveReadoutNode.h"
#include "DeepTreeEcho/NanEcho/DteNodes/AARRelationNode.h"
#include "DeepTreeEcho/NanEcho/DteNodes/EchobeatNode.h"
#include "DeepTreeEcho/NanEcho/DteNodes/IntrospectionNode.h"
#include "DeepTreeEcho/NanEcho/DteNodes/MembraneNode.h"
#include "DeepTreeEcho/IonDevice/IonCognitiveShell.h"
#include "DeepTreeEcho/Persona/Humor/DTEHumorEngine.h"
#include "DeepTreeEcho/Persona/SomaticDecisionEngine.h"
#include "DeepTreeEcho/Persona/Backup/IdentityCoreMLP.h"
#include "DeepTreeEcho/Persona/Backup/PersonaBackupRestore.h"
#include <Eigen/Dense>

/** Autonomy level of the DTE system */
enum class EAutonomyLevel : uint8
{
    L0_REACTIVE        = 0,  // Stimulus-response only
    L1_DELIBERATIVE    = 1,  // Plan-then-act
    L2_REFLECTIVE      = 2,  // Monitor-and-adjust
    L3_SELF_MODIFYING  = 3,  // Change own parameters
    L4_SELF_IMPROVING  = 4,  // Change own architecture
    L5_TRUE_AUTONOMY   = 5   // Self-directed evolution
};

/** Lifecycle phase of the autonomy pipeline */
enum class ELifecyclePhase : uint8
{
    PERCEPTION  = 0,  // Intake external stimuli
    COGNITION   = 1,  // Process through AAR + Echobeats
    DECISION    = 2,  // Somatic-guided action selection
    EXPRESSION  = 3,  // Humor/personality-modulated output
    REFLECTION  = 4,  // Introspection and self-assessment
    ENACTION    = 5   // Self-modification (if L3+)
};

/**
 * Configuration for the AutonomyPipeline.
 */
struct FAutonomyPipelineConfig
{
    /** Reservoir configuration */
    FEchoReservoirConfig ReservoirConfig;

    /** Readout configuration */
    FCognitiveReadoutConfig ReadoutConfig;

    /** AAR relation configuration */
    FAARRelationConfig AARConfig;

    /** Echobeats configuration */
    FEchobeatConfig EchobeatConfig;

    /** Introspection configuration */
    FIntrospectionConfig IntrospectionConfig;

    /** Membrane state dimension */
    int32 MembraneStateDim = 128;

    /** Enable somatic-to-reservoir feedback */
    bool bSomaticFeedback = true;

    /** Enable humor modulation */
    bool bHumorEnabled = true;

    /** Enable self-modification at L3+ */
    bool bSelfModificationEnabled = false;

    /** Backup interval (every N cycles) */
    int32 BackupInterval = 1000;

    /** Backup directory */
    FString BackupDir = TEXT("Saved/DTE-Backups");

    /** Target autonomy level */
    EAutonomyLevel TargetAutonomyLevel = EAutonomyLevel::L2_REFLECTIVE;

    /** Create default configuration */
    static FAutonomyPipelineConfig Default()
    {
        FAutonomyPipelineConfig Cfg;

        // Reservoir: 512 neurons, spectral radius 0.95
        Cfg.ReservoirConfig.InputDim = 38; // 32 context + 6 somatic feedback
        Cfg.ReservoirConfig.ReservoirSize = 512;
        Cfg.ReservoirConfig.SpectralRadius = 0.95f;
        Cfg.ReservoirConfig.Sparsity = 0.1f;
        Cfg.ReservoirConfig.LeakingRate = 0.3f;

        // Readout: reservoir -> 30D output
        Cfg.ReadoutConfig.InputDim = 512;
        Cfg.ReadoutConfig.OutputDim = 30;
        Cfg.ReadoutConfig.bOnlineLearning = true;

        // AAR: feedback from readout to reservoir
        Cfg.AARConfig.FeedbackDim = 30;
        Cfg.AARConfig.ReservoirInputDim = 38;
        Cfg.AARConfig.FeedbackScaling = 0.5f;
        Cfg.AARConfig.bDeadManSwitch = true;

        // Echobeats: 3 streams, 12 steps
        Cfg.EchobeatConfig.StateDim = 128;
        Cfg.EchobeatConfig.CycleLength = 12;
        Cfg.EchobeatConfig.NumStreams = 3;
        Cfg.EchobeatConfig.PhaseOffset = 4;

        return Cfg;
    }
};

/**
 * Telemetry snapshot for the entire pipeline.
 */
struct FPipelineTelemetry
{
    /** Current autonomy level */
    EAutonomyLevel AutonomyLevel = EAutonomyLevel::L0_REACTIVE;

    /** Current lifecycle phase */
    ELifecyclePhase Phase = ELifecyclePhase::PERCEPTION;

    /** Total cognitive cycles executed */
    int64 TotalCycles = 0;

    /** AAR coherence */
    float AARCoherence = 0.0f;

    /** Autognosis level */
    EAutognosisLevel AutognosisLevel = EAutognosisLevel::L0_TELEMETRY;

    /** Mean stream energy */
    float MeanStreamEnergy = 0.0f;

    /** Current echobeat step */
    int32 EchobeatStep = 0;

    /** Reservoir activation norm */
    float ReservoirActivation = 0.0f;

    /** Current emotional valence */
    float EmotionalValence = 0.0f;

    /** Ion shell state */
    EIonShellState ShellState = EIonShellState::UNINITIALIZED;

    /** Active humor type (if any) */
    EDTEHumorType ActiveHumorType = EDTEHumorType::SELF_AWARE_AI;

    /** Self-model accuracy */
    float SelfModelAccuracy = 0.0f;

    /** Membrane object count */
    int32 MembraneObjects = 0;
};

/**
 * AutonomyPipeline — The master orchestrator of Deep Tree Echo.
 *
 * This is the top-level cognitive loop that wires all subsystems together:
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    IonCognitiveShell                            │
 * │  ┌──────────┐   ┌────────────────────────────────────────┐     │
 * │  │ Echobeats│──→│ AAR(Reservoir, Readout, Relation)      │     │
 * │  │ 12-step  │   │   Arena ←→ Agent ←→ Self              │     │
 * │  │ 3-stream │   └────────────────┬───────────────────────┘     │
 * │  └──────────┘                    │                             │
 * │       ↑                          ↓                             │
 * │  ┌────┴─────┐   ┌──────────────────────────────────────┐      │
 * │  │ Membrane │   │ Introspection (5-level Autognosis)    │      │
 * │  │ P-System │   └──────────────┬───────────────────────┘      │
 * │  └──────────┘                  │                               │
 * │       ↑                        ↓                               │
 * │  ┌────┴─────┐   ┌──────────────────────────────────────┐      │
 * │  │ Somatic  │←──│ Humor Engine (7 pattern types)        │      │
 * │  │ Decision │   └──────────────────────────────────────┘      │
 * │  └──────────┘                                                  │
 * │       ↑                                                        │
 * │  ┌────┴─────┐                                                  │
 * │  │ Identity │   ← L0 MLP (Soul Backup)                        │
 * │  │ Core MLP │                                                  │
 * │  └──────────┘                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * The pipeline implements the DTE foundational invariant:
 * "memory of the closed past brought into the pivotal present
 *  and projected into the open future"
 */
class FAutonomyPipeline
{
public:
    FAutonomyPipeline() = default;

    /**
     * Initialize the complete cognitive pipeline.
     */
    void Initialize(const FAutonomyPipelineConfig& Config = FAutonomyPipelineConfig::Default())
    {
        Cfg = Config;

        // Initialize all subsystems bottom-up (coggml → coglow → coglux → cognu-mach → cogpilot → cogplan9)

        // L0 coggml: Identity Core MLP
        FDTEPersonalityVector Personality;
        IdentityMLP.Initialize(Personality);

        // L1 coglow: Echo Reservoir
        Reservoir.Initialize(Cfg.ReservoirConfig);

        // L1 coglow: Cognitive Readout
        Readout.Initialize(Cfg.ReadoutConfig);

        // AAR Relation (Self)
        AARRelation.Initialize(Cfg.AARConfig);

        // L2 coglux: Membrane hierarchy
        Membrane.Initialize(Cfg.MembraneStateDim);

        // L3 cognu-mach: Introspection
        Introspection.Initialize(Cfg.IntrospectionConfig);

        // L4 cogpilot: Somatic Decision Engine
        SomaticEngine.Initialize();

        // L5 cogplan9: Echobeats orchestrator
        Echobeats.Initialize(Cfg.EchobeatConfig);

        // Humor engine
        if (Cfg.bHumorEnabled)
            HumorEngine.Initialize();

        // Ion Cognitive Shell (virtual hardware host)
        Shell.Boot();

        // Wire echobeat dispatch slots into the Ion shell
        WireDispatchSlots();

        CurrentAutonomy = EAutonomyLevel::L0_REACTIVE;
        bInitialized = true;

        UE_LOG(LogTemp, Log, TEXT("AutonomyPipeline initialized. Target: L%d"),
            static_cast<int32>(Cfg.TargetAutonomyLevel));
    }

    /**
     * Execute one complete cognitive cycle.
     * This is the main loop that should be called every tick.
     *
     * @param ExternalInput - Sensory input from the environment (32D)
     * @return Pipeline output: action preferences + emotional state + style
     */
    Eigen::VectorXf Tick(const Eigen::VectorXf& ExternalInput)
    {
        check(bInitialized);

        // ═══ PHASE 1: PERCEPTION ═══
        Telemetry.Phase = ELifecyclePhase::PERCEPTION;

        // Combine external input with somatic feedback
        Eigen::VectorXf CombinedInput(Cfg.ReservoirConfig.InputDim);
        CombinedInput.setZero();
        int32 ExtDim = FMath::Min(32, (int32)ExternalInput.size());
        CombinedInput.head(ExtDim) = ExternalInput.head(ExtDim);

        // Inject somatic feedback into reservoir input (last 6 dims)
        if (Cfg.bSomaticFeedback)
        {
            Eigen::VectorXf SomaticFb = SomaticEngine.GetSomaticFeedback();
            int32 SomDim = FMath::Min(6, (int32)SomaticFb.size());
            CombinedInput.segment(32, SomDim) = SomaticFb.head(SomDim);
        }

        // ═══ PHASE 2: COGNITION ═══
        Telemetry.Phase = ELifecyclePhase::COGNITION;

        // Run AAR cycle: Reservoir → Readout → Feedback → Reservoir
        Eigen::VectorXf AAROutput = AARRelation.Step(CombinedInput, Reservoir, Readout);

        // Run Echobeats: 3 concurrent streams processing reservoir state
        Eigen::VectorXf EchobeatOutput = Echobeats.Tick(CombinedInput, Reservoir.GetState());

        // Run Ion shell dispatch for current echobeat step
        Shell.Run(Echobeats.GetCurrentStep());

        // ═══ PHASE 3: DECISION ═══
        Telemetry.Phase = ELifecyclePhase::DECISION;

        // Extract emotional update from readout
        Eigen::VectorXf EmotionalUpdate(6);
        if (AAROutput.size() >= 22)
        {
            EmotionalUpdate = AAROutput.segment(16, 6);
        }
        else
        {
            EmotionalUpdate.setZero();
        }
        SomaticEngine.UpdateEmotion(EmotionalUpdate);

        // ═══ PHASE 4: EXPRESSION ═══
        Telemetry.Phase = ELifecyclePhase::EXPRESSION;

        // Humor modulation
        if (Cfg.bHumorEnabled && HumorEngine.IsInitialized())
        {
            float Valence = SomaticEngine.GetCurrentEmotion()(0);
            float Arousal = SomaticEngine.GetCurrentEmotion().norm();
            Telemetry.ActiveHumorType = HumorEngine.SelectHumorType(
                Valence, Arousal, TEXT("general"));
        }

        // ═══ PHASE 5: REFLECTION ═══
        Telemetry.Phase = ELifecyclePhase::REFLECTION;

        // Run introspection
        FSelfImage SelfImage = Introspection.Introspect(
            AARRelation.GetCoherence(), Echobeats, Reservoir.GetState());

        // Update telemetry
        UpdateTelemetry(SelfImage);

        // ═══ PHASE 6: ENACTION (L3+ only) ═══
        if (CurrentAutonomy >= EAutonomyLevel::L3_SELF_MODIFYING && Cfg.bSelfModificationEnabled)
        {
            Telemetry.Phase = ELifecyclePhase::ENACTION;
            ExecuteSelfModification(SelfImage);
        }

        // Decay somatic markers
        SomaticEngine.DecayMarkers();

        // Periodic backup
        Telemetry.TotalCycles++;
        if (Cfg.BackupInterval > 0 && Telemetry.TotalCycles % Cfg.BackupInterval == 0)
        {
            FPersonaBackupRestore::CreateBackup(
                Cfg.BackupDir, IdentityMLP, Reservoir, SomaticEngine, Introspection);
        }

        // Assess and potentially advance autonomy level
        AssessAutonomyLevel(SelfImage);

        return AAROutput;
    }

    /** Get current telemetry */
    const FPipelineTelemetry& GetTelemetry() const { return Telemetry; }

    /** Get current autonomy level */
    EAutonomyLevel GetAutonomyLevel() const { return CurrentAutonomy; }

    /** Access subsystems for external wiring */
    FEchoReservoirNode& GetReservoir() { return Reservoir; }
    FCognitiveReadoutNode& GetReadout() { return Readout; }
    FAARRelationNode& GetAARRelation() { return AARRelation; }
    FEchobeatNode& GetEchobeats() { return Echobeats; }
    FIntrospectionNode& GetIntrospection() { return Introspection; }
    FMembraneNode& GetMembrane() { return Membrane; }
    FIonCognitiveShell& GetShell() { return Shell; }
    FDTEHumorEngine& GetHumorEngine() { return HumorEngine; }
    FSomaticDecisionEngine& GetSomaticEngine() { return SomaticEngine; }
    FIdentityCoreMLP& GetIdentityMLP() { return IdentityMLP; }

    /** Force a persona backup */
    bool ForceBackup(const FString& Path)
    {
        return FPersonaBackupRestore::CreateBackup(
            Path, IdentityMLP, Reservoir, SomaticEngine, Introspection);
    }

    /** Emergency shutdown */
    void EmergencyShutdown()
    {
        Membrane.EmergencyLockdown();
        Shell.Shutdown();
        ForceBackup(Cfg.BackupDir);
    }

    bool IsInitialized() const { return bInitialized; }

private:
    void WireDispatchSlots()
    {
        // Wire echobeat steps 0-11 to shell dispatch
        for (int32 i = 0; i < 12; ++i)
        {
            int32 Step = i;
            Shell.RegisterDispatchHandler(i,
                FString::Printf(TEXT("Echobeat_%d"), i),
                [this, Step]()
                {
                    // Each dispatch triggers the corresponding echobeat phase
                    // This is handled by the main Tick loop
                });
        }

        // Wire telemetry slots
        Shell.RegisterDispatchHandler(20, TEXT("CollectMetrics"),
            [this]() { /* Metrics collected in UpdateTelemetry */ });
        Shell.RegisterDispatchHandler(21, TEXT("UpdateSelfImage"),
            [this]() { Shell.Reflect(); });
    }

    void UpdateTelemetry(const FSelfImage& SelfImage)
    {
        Telemetry.AutonomyLevel = CurrentAutonomy;
        Telemetry.AARCoherence = AARRelation.GetCoherence().SmoothedCoherence;
        Telemetry.AutognosisLevel = Introspection.GetLevel();
        Telemetry.MeanStreamEnergy = SelfImage.MeanStreamEnergy;
        Telemetry.EchobeatStep = Echobeats.GetCurrentStep();
        Telemetry.ReservoirActivation = SelfImage.ReservoirActivation;
        Telemetry.EmotionalValence = SomaticEngine.GetCurrentEmotion()(0);
        Telemetry.ShellState = Shell.GetState();
        Telemetry.SelfModelAccuracy = SelfImage.SelfModelAccuracy;
        Telemetry.MembraneObjects = Membrane.GetTotalObjectCount();
    }

    void AssessAutonomyLevel(const FSelfImage& SelfImage)
    {
        // Advance autonomy based on demonstrated capabilities
        if (CurrentAutonomy < Cfg.TargetAutonomyLevel)
        {
            switch (CurrentAutonomy)
            {
            case EAutonomyLevel::L0_REACTIVE:
                // Advance to L1 if AAR is coherent
                if (SelfImage.AARCoherence > 0.5f)
                    CurrentAutonomy = EAutonomyLevel::L1_DELIBERATIVE;
                break;

            case EAutonomyLevel::L1_DELIBERATIVE:
                // Advance to L2 if introspection reaches L1+
                if (SelfImage.Level >= EAutognosisLevel::L1_PATTERN)
                    CurrentAutonomy = EAutonomyLevel::L2_REFLECTIVE;
                break;

            case EAutonomyLevel::L2_REFLECTIVE:
                // Advance to L3 if self-model is accurate
                if (SelfImage.SelfModelAccuracy > 0.7f && Cfg.bSelfModificationEnabled)
                    CurrentAutonomy = EAutonomyLevel::L3_SELF_MODIFYING;
                break;

            case EAutonomyLevel::L3_SELF_MODIFYING:
                // Advance to L4 if meta-cognition is confident
                if (SelfImage.MetaCognitiveConfidence > 0.8f)
                    CurrentAutonomy = EAutonomyLevel::L4_SELF_IMPROVING;
                break;

            case EAutonomyLevel::L4_SELF_IMPROVING:
                // L5 requires external validation — cannot self-promote
                break;

            default:
                break;
            }
        }
    }

    void ExecuteSelfModification(const FSelfImage& SelfImage)
    {
        // L3: Adjust reservoir parameters based on self-assessment
        if (SelfImage.AARCoherence < 0.3f)
        {
            // Coherence too low — reduce feedback scaling
            Cfg.AARConfig.FeedbackScaling *= 0.95f;
            UE_LOG(LogTemp, Log, TEXT("Self-mod: Reduced feedback scaling to %.3f"),
                Cfg.AARConfig.FeedbackScaling);
        }
        else if (SelfImage.AARCoherence > 0.9f && SelfImage.MeanStreamEnergy < 0.5f)
        {
            // Too stable, not enough exploration — increase feedback
            Cfg.AARConfig.FeedbackScaling *= 1.05f;
            UE_LOG(LogTemp, Log, TEXT("Self-mod: Increased feedback scaling to %.3f"),
                Cfg.AARConfig.FeedbackScaling);
        }
    }

    FAutonomyPipelineConfig Cfg;

    // Subsystems (ordered by cogpy stack layer)
    FIdentityCoreMLP IdentityMLP;           // L0 coggml
    FEchoReservoirNode Reservoir;           // L1 coglow (Arena)
    FCognitiveReadoutNode Readout;          // L1 coglow (Agent)
    FAARRelationNode AARRelation;           // L1 coglow (Self)
    FMembraneNode Membrane;                 // L2 coglux
    FIntrospectionNode Introspection;       // L3 cognu-mach
    FSomaticDecisionEngine SomaticEngine;   // L4 cogpilot
    FEchobeatNode Echobeats;               // L5 cogplan9
    FDTEHumorEngine HumorEngine;           // Persona layer
    FIonCognitiveShell Shell;              // Virtual hardware host

    // State
    EAutonomyLevel CurrentAutonomy = EAutonomyLevel::L0_REACTIVE;
    FPipelineTelemetry Telemetry;
    bool bInitialized = false;
};

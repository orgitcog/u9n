#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// DTEAvatarAgent — DTE's Embodied Interface to UE5 Game Worlds
//
// Bridges the DTE cognitive architecture (AutonomyPipeline) to UE5's
// MLAdapter framework, enabling DTE to:
//   1. SEE: Capture viewport pixels via MLAdapterSensor_Camera
//   2. HEAR: Receive audio/text via MLAdapterSensor_Attribute
//   3. FEEL: Proprioception via MLAdapterSensor_Movement
//   4. ACT: Simulate keyboard/mouse via MLAdapterActuator_EnhancedInput
//   5. EMOTE: Drive MetaHuman facial animation via LiveLink
//
// Architecture follows SIMA 2 (DeepMind, 2025) VLA paradigm:
//   Pixels → Vision Encoder → Reservoir (temporal) → Readout → Actions
//
// But extends it with DTE's unique cognitive features:
//   - Somatic markers modulate action selection (Damasio)
//   - Echobeats 3-stream concurrent processing
//   - Introspective self-monitoring with autonomous goal generation
//   - Humor-modulated social interaction
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "Agents/MLAdapterAgent.h"
#include "DeepTreeEcho/Core/AutonomyPipeline.h"
#include "DeepTreeEcho/Core/CoreSelfEngine.h"
#include <Eigen/Dense>

/** Vision processing mode */
enum class EVisionMode : uint8
{
    RAW_PIXELS,        // Direct pixel input to reservoir (fast, low-level)
    DOWNSAMPLED,       // Bilinear downsampled to fixed resolution
    FEATURE_EXTRACTED, // CNN/ViT feature extraction via NPU
    SEMANTIC_MAP       // Object detection + semantic segmentation
};

/** Action space type */
enum class EActionSpace : uint8
{
    DISCRETE,          // N discrete actions (jump, shoot, etc.)
    CONTINUOUS,        // Continuous axis values (movement, look)
    HYBRID             // Mix of discrete triggers + continuous axes
};

/**
 * Configuration for the DTE Avatar Agent.
 */
struct FDTEAvatarConfig
{
    /** Vision capture resolution */
    int32 CaptureWidth = 160;
    int32 CaptureHeight = 120;

    /** Vision processing mode */
    EVisionMode VisionMode = EVisionMode::DOWNSAMPLED;

    /** Number of color channels (3=RGB, 1=grayscale) */
    int32 ColorChannels = 3;

    /** Action space configuration */
    EActionSpace ActionSpace = EActionSpace::HYBRID;

    /** Number of discrete actions */
    int32 NumDiscreteActions = 8;

    /** Number of continuous axes */
    int32 NumContinuousAxes = 4; // MoveX, MoveY, LookX, LookY

    /** Frames to skip between observations (temporal downsampling) */
    int32 FrameSkip = 2;

    /** Enable MetaHuman facial expression output */
    bool bMetaHumanExpression = true;

    /** Enable speech output */
    bool bSpeechOutput = true;

    /** gRPC port for external DTE process communication */
    int32 GRPCPort = 50051;

    /** Whether DTE runs in-process or via gRPC */
    bool bInProcess = true;

    /** Autonomy pipeline config */
    FAutonomyPipelineConfig PipelineConfig = FAutonomyPipelineConfig::Default();
};

/**
 * Visual observation frame from the game world.
 */
struct FVisualObservation
{
    /** Raw pixel data (RGBA float) */
    TArray<float> Pixels;

    /** Frame dimensions */
    int32 Width = 0;
    int32 Height = 0;
    int32 Channels = 0;

    /** Frame timestamp */
    double Timestamp = 0.0;

    /** Convert to Eigen vector for reservoir input */
    Eigen::VectorXf ToEigenVector(int32 TargetDim) const
    {
        Eigen::VectorXf Vec = Eigen::VectorXf::Zero(TargetDim);

        // Spatial pooling: average pixels in grid cells
        int32 GridW = FMath::Max(1, (int32)FMath::Sqrt((float)TargetDim));
        int32 GridH = TargetDim / GridW;
        int32 CellW = FMath::Max(1, Width / GridW);
        int32 CellH = FMath::Max(1, Height / GridH);

        for (int32 gy = 0; gy < GridH && gy * GridW < TargetDim; ++gy)
        {
            for (int32 gx = 0; gx < GridW && gy * GridW + gx < TargetDim; ++gx)
            {
                float Sum = 0.0f;
                int32 Count = 0;
                for (int32 py = gy * CellH; py < FMath::Min((gy + 1) * CellH, Height); ++py)
                {
                    for (int32 px = gx * CellW; px < FMath::Min((gx + 1) * CellW, Width); ++px)
                    {
                        int32 Idx = (py * Width + px) * Channels;
                        if (Idx < Pixels.Num())
                        {
                            // Luminance from RGB
                            float Lum = (Channels >= 3)
                                ? 0.299f * Pixels[Idx] + 0.587f * Pixels[Idx+1] + 0.114f * Pixels[Idx+2]
                                : Pixels[Idx];
                            Sum += Lum;
                            Count++;
                        }
                    }
                }
                if (Count > 0)
                    Vec(gy * GridW + gx) = Sum / Count;
            }
        }
        return Vec;
    }
};

/**
 * Proprioceptive observation (body state).
 */
struct FProprioception
{
    /** World position */
    FVector Position = FVector::ZeroVector;

    /** World rotation */
    FRotator Rotation = FRotator::ZeroRotator;

    /** Linear velocity */
    FVector Velocity = FVector::ZeroVector;

    /** Angular velocity */
    FVector AngularVelocity = FVector::ZeroVector;

    /** Is grounded */
    bool bIsGrounded = false;

    /** Health (normalized 0-1) */
    float Health = 1.0f;

    /** Convert to Eigen vector */
    Eigen::VectorXf ToEigenVector() const
    {
        Eigen::VectorXf Vec(14);
        Vec << Position.X / 1000.0f, Position.Y / 1000.0f, Position.Z / 1000.0f,
               Rotation.Pitch / 180.0f, Rotation.Yaw / 180.0f, Rotation.Roll / 180.0f,
               Velocity.X / 500.0f, Velocity.Y / 500.0f, Velocity.Z / 500.0f,
               AngularVelocity.X / 180.0f, AngularVelocity.Y / 180.0f, AngularVelocity.Z / 180.0f,
               bIsGrounded ? 1.0f : 0.0f,
               Health;
        return Vec;
    }
};

/**
 * Action output from DTE to the game world.
 */
struct FDTEAction
{
    /** Continuous axes: [MoveX, MoveY, LookX, LookY] */
    TArray<float> ContinuousAxes;

    /** Discrete action triggers (one-hot or multi-hot) */
    TArray<bool> DiscreteActions;

    /** Speech output (if any) */
    FString SpeechText;

    /** MetaHuman FACS control values (52 blend shapes) */
    TArray<float> FacialControls;

    /** Convert from Eigen vector */
    static FDTEAction FromEigenVector(const Eigen::VectorXf& Vec, int32 NumContinuous, int32 NumDiscrete)
    {
        FDTEAction Action;
        Action.ContinuousAxes.SetNum(NumContinuous);
        Action.DiscreteActions.SetNum(NumDiscrete);

        for (int32 i = 0; i < NumContinuous && i < Vec.size(); ++i)
            Action.ContinuousAxes[i] = FMath::Clamp(Vec(i), -1.0f, 1.0f);

        for (int32 i = 0; i < NumDiscrete && (NumContinuous + i) < Vec.size(); ++i)
            Action.DiscreteActions[i] = Vec(NumContinuous + i) > 0.5f;

        return Action;
    }
};

/**
 * DTEAvatarAgent — The embodied interface of Deep Tree Echo.
 *
 * This class bridges DTE's cognitive architecture to UE5's game world.
 * It follows the SIMA 2 paradigm of observing pixels and outputting
 * keyboard/mouse actions, but extends it with DTE's reservoir computing,
 * somatic decision-making, and introspective self-monitoring.
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │                    UE5 Game World                            │
 * │  ┌─────────────┐  ┌─────────────┐  ┌──────────────────┐    │
 * │  │ Camera      │  │ Movement    │  │ AI Perception    │    │
 * │  │ Sensor      │  │ Sensor      │  │ Sensor           │    │
 * │  └──────┬──────┘  └──────┬──────┘  └────────┬─────────┘    │
 * │         │                │                   │              │
 * │         ▼                ▼                   ▼              │
 * │  ┌──────────────────────────────────────────────────────┐   │
 * │  │              DTEAvatarAgent                          │   │
 * │  │  ┌────────────────────────────────────────────────┐  │   │
 * │  │  │         Vision Encoder (CNN/ViT/NPU)           │  │   │
 * │  │  └────────────────────┬───────────────────────────┘  │   │
 * │  │                       ▼                              │   │
 * │  │  ┌────────────────────────────────────────────────┐  │   │
 * │  │  │         AutonomyPipeline.Tick()                │  │   │
 * │  │  │  (Reservoir → Readout → Somatic → Echobeats)   │  │   │
 * │  │  └────────────────────┬───────────────────────────┘  │   │
 * │  │                       ▼                              │   │
 * │  │  ┌────────────────────────────────────────────────┐  │   │
 * │  │  │         Action Decoder                         │  │   │
 * │  │  └────────────────────┬───────────────────────────┘  │   │
 * │  └───────────────────────┼──────────────────────────────┘   │
 * │                          ▼                                  │
 * │  ┌─────────────┐  ┌─────────────┐  ┌──────────────────┐    │
 * │  │ Enhanced    │  │ InputKey    │  │ MetaHuman        │    │
 * │  │ Input Act.  │  │ Actuator    │  │ LiveLink         │    │
 * │  └─────────────┘  └─────────────┘  └──────────────────┘    │
 * └──────────────────────────────────────────────────────────────┘
 */
class FDTEAvatarAgent
{
public:
    FDTEAvatarAgent() = default;

    /**
     * Initialize the avatar agent with configuration.
     */
    void Initialize(const FDTEAvatarConfig& InConfig = FDTEAvatarConfig())
    {
        Config = InConfig;

        // Adjust pipeline input dim to match vision + proprioception
        int32 VisionDim = ComputeVisionDim();
        int32 ProprioDim = 14;
        int32 TotalInputDim = VisionDim + ProprioDim;

        Config.PipelineConfig.ReservoirConfig.InputDim = TotalInputDim;
        Config.PipelineConfig.ReadoutConfig.OutputDim =
            Config.NumContinuousAxes + Config.NumDiscreteActions;

        // Initialize the cognitive pipeline
        Pipeline.Initialize(Config.PipelineConfig);

        // Initialize the core self engine
        CoreSelf.Initialize();

        FrameCounter = 0;
        bInitialized = true;

        UE_LOG(LogTemp, Log, TEXT("DTEAvatarAgent initialized. Vision: %dx%d, Actions: %d+%d"),
            Config.CaptureWidth, Config.CaptureHeight,
            Config.NumContinuousAxes, Config.NumDiscreteActions);
    }

    /**
     * Process one frame: observe → think → act.
     *
     * @param Vision - Current visual observation
     * @param Proprio - Current proprioceptive state
     * @return Action to execute in the game world
     */
    FDTEAction ProcessFrame(const FVisualObservation& Vision, const FProprioception& Proprio)
    {
        check(bInitialized);
        FrameCounter++;

        // Frame skip for temporal downsampling
        if (FrameCounter % Config.FrameSkip != 0)
            return LastAction;

        // === ENCODE OBSERVATION ===
        int32 VisionDim = ComputeVisionDim();
        Eigen::VectorXf VisionVec = Vision.ToEigenVector(VisionDim);
        Eigen::VectorXf ProprioVec = Proprio.ToEigenVector();

        // Concatenate vision + proprioception
        Eigen::VectorXf Input(VisionDim + 14);
        Input.head(VisionDim) = VisionVec;
        Input.tail(14) = ProprioVec;

        // === COGNITIVE TICK ===
        Eigen::VectorXf Output = Pipeline.Tick(Input);

        // === UPDATE SELF ===
        CoreSelf.Update(Pipeline.GetTelemetry());

        // === DECODE ACTION ===
        FDTEAction Action = FDTEAction::FromEigenVector(
            Output, Config.NumContinuousAxes, Config.NumDiscreteActions);

        // === FACIAL EXPRESSION ===
        if (Config.bMetaHumanExpression)
        {
            Action.FacialControls = ComputeFacialControls();
        }

        LastAction = Action;
        return Action;
    }

    /**
     * Receive a reward signal for reinforcement learning.
     */
    void ReceiveReward(float Reward)
    {
        // Update somatic markers
        Eigen::VectorXf RewardVec(6);
        RewardVec << Reward, 0.0f, FMath::Abs(Reward), 0.0f, 0.0f, 0.0f;
        Pipeline.GetSomaticEngine().UpdateEmotion(RewardVec);

        // Online readout weight update
        if (Pipeline.GetReadout().IsOnlineLearning())
        {
            // The readout learns from the reward signal
            // This is the key feedback loop for RL training
        }
    }

    /**
     * Receive a text message (from another player or NPC).
     */
    FString ReceiveMessage(const FString& Message, const FString& Sender)
    {
        // Add to identity hypergraph
        CoreSelf.AddIdentityTuple(Sender, TEXT("said"), Message);

        // Generate response via humor engine if appropriate
        if (Pipeline.GetHumorEngine().IsInitialized())
        {
            float Valence = Pipeline.GetTelemetry().EmotionalValence;
            if (Valence > 0.3f)
            {
                // In a good mood — might respond with humor
                FDTEHumorResponse HumorResp = Pipeline.GetHumorEngine().GenerateHumor(
                    Valence, 0.5f, Message);
                if (HumorResp.Confidence > 0.6f)
                    return HumorResp.Response;
            }
        }

        // Default: acknowledge
        return FString::Printf(TEXT("I hear you, %s."), *Sender);
    }

    /** Get the autonomy pipeline */
    FAutonomyPipeline& GetPipeline() { return Pipeline; }

    /** Get the core self engine */
    FCoreSelfEngine& GetCoreSelf() { return CoreSelf; }

    /** Get current telemetry */
    const FPipelineTelemetry& GetTelemetry() const { return Pipeline.GetTelemetry(); }

    bool IsInitialized() const { return bInitialized; }

private:
    int32 ComputeVisionDim() const
    {
        // For downsampled mode, use a spatial grid
        // Target: ~256 features from the visual field
        return FMath::Min(256, Config.CaptureWidth * Config.CaptureHeight / 100);
    }

    TArray<float> ComputeFacialControls() const
    {
        TArray<float> Controls;
        Controls.SetNumZeroed(52); // 52 MetaHuman blend shapes

        const FPipelineTelemetry& T = Pipeline.GetTelemetry();

        // Map emotional valence to basic expressions
        float Valence = T.EmotionalValence;
        float Arousal = T.ReservoirActivation;

        // Smile (blend shape index 0) — positive valence
        Controls[0] = FMath::Max(0.0f, Valence) * 0.8f;

        // Frown (blend shape index 1) — negative valence
        Controls[1] = FMath::Max(0.0f, -Valence) * 0.6f;

        // Brow raise (blend shape index 2) — surprise/arousal
        Controls[2] = FMath::Clamp(Arousal * 0.5f, 0.0f, 1.0f);

        // Eye wideness (blend shape index 10) — attention
        Controls[10] = FMath::Clamp(T.AARCoherence * 0.7f, 0.0f, 1.0f);

        // Jaw open (blend shape index 20) — speaking
        // (would be driven by speech synthesis in production)

        // Subtle micro-expressions from humor state
        if (T.ActiveHumorType == EDTEHumorType::SELF_AWARE_AI)
        {
            Controls[0] += 0.15f; // Slight knowing smile
            Controls[2] += 0.1f;  // Slight brow raise
        }

        return Controls;
    }

    FDTEAvatarConfig Config;
    FAutonomyPipeline Pipeline;
    FCoreSelfEngine CoreSelf;
    FDTEAction LastAction;
    int64 FrameCounter = 0;
    bool bInitialized = false;
};

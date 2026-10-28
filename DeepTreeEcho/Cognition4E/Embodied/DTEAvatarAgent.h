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
//
// Every word the avatar says is requested from the language model through
// FDTELanguageModelClient; replies arrive asynchronously and are delivered
// through the reply handler and the next FDTEAction's SpeechText. When the
// model does not answer, the avatar says nothing and LogDTEModel records why.
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "Agents/MLAdapterAgent.h"
#include "DeepTreeEcho/Core/AutonomyPipeline.h"
#include "DeepTreeEcho/Core/CoreSelfEngine.h"
#include "DTELanguageModelBridge.h"
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

    /** Speech output (if any). Always model-generated; empty when there is nothing to say. */
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

/** Receives a model-generated reply: the text spoken and the sender it answers. */
using FDTEAvatarReplyHandler = TFunction<void(const FString& ReplyText, const FString& Sender)>;

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
    FDTEAvatarAgent()
        : LifetimeToken(MakeShared<bool>(true))
    {
    }

    /** Not copyable: model replies in flight are bound to this instance's lifetime token. */
    FDTEAvatarAgent(const FDTEAvatarAgent&) = delete;
    FDTEAvatarAgent& operator=(const FDTEAvatarAgent&) = delete;

    ~FDTEAvatarAgent()
    {
        // Replies still in flight see an expired weak token and are dropped.
        LifetimeToken.Reset();
    }

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

        // === DECODE ACTION ===
        FDTEAction Action = FDTEAction::FromEigenVector(
            Output, Config.NumContinuousAxes, Config.NumDiscreteActions);

        // === FACIAL EXPRESSION ===
        if (Config.bMetaHumanExpression)
        {
            Action.FacialControls = ComputeFacialControls();
        }

        // === SPEECH ===
        // Model-generated replies queued by ReceiveMessage leave through the action,
        // one per processed frame. Skipped frames replay LastAction without speech.
        if (Config.bSpeechOutput && PendingSpeech.Num() > 0)
        {
            Action.SpeechText = PendingSpeech[0];
            PendingSpeech.RemoveAt(0);
        }

        LastAction = Action;
        LastAction.SpeechText.Empty();
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
     * Set the handler that receives model-generated replies as they arrive.
     * Replies are also queued into the next processed frame's FDTEAction::SpeechText.
     */
    void SetReplyHandler(FDTEAvatarReplyHandler InHandler)
    {
        ReplyHandler = MoveTemp(InHandler);
    }

    /**
     * Receive a text message (from another player or NPC).
     *
     * The message is recorded in the identity hypergraph and a reply is
     * requested from the language model with the sender, the message and the
     * agent's current state as context. The reply arrives asynchronously via
     * the reply handler and the next FDTEAction::SpeechText; nothing is said
     * when the model does not answer.
     *
     * @return true when a reply request was issued, false when none could be.
     */
    bool ReceiveMessage(const FString& Message, const FString& Sender)
    {
        // Add to identity hypergraph
        Pipeline.GetCoreSelf().AddIdentityTuple(Sender, TEXT("said"), Message);

        if (!bInitialized)
        {
            UE_LOG(LogDTEModel, Warning, TEXT("DTEAvatarAgent: message from %s received before Initialize(); no reply requested"), *Sender);
            return false;
        }

        const FPipelineTelemetry& T = Pipeline.GetTelemetry();
        const Eigen::VectorXf& Emotion = Pipeline.GetSomaticEngine().GetCurrentEmotion();
        const float Valence = T.EmotionalValence;
        const float Arousal = Emotion.size() > 0 ? FMath::Clamp(static_cast<float>(Emotion.norm()), 0.0f, 1.0f) : 0.5f;
        const Eigen::VectorXf Personality = Pipeline.GetIdentityMLP().GetPersonality().ToEigen();

        // Build the reply instruction: intent plus live context.
        FString Instruction;
        Instruction += FString::Printf(TEXT("%s just said to you: \"%s\"\n"), *Sender, *Message);
        Instruction += TEXT("Reply to them in your own voice, in one to three sentences.\n");
        Instruction += FString::Printf(
            TEXT("Your current state: emotional valence %.2f (-1 negative .. 1 positive), arousal %.2f, autonomy level %d, autognosis level %d, echobeat step %d of 12, AAR coherence %.2f, reservoir activation %.2f.\n"),
            Valence, Arousal, static_cast<int32>(T.AutonomyLevel), static_cast<int32>(T.AutognosisLevel),
            T.EchobeatStep, T.AARCoherence, T.ReservoirActivation);
        if (Personality.size() == 5)
        {
            Instruction += FString::Printf(
                TEXT("Personality (0..1): playfulness %.2f, intelligence %.2f, empathy %.2f, chaotic %.2f, sarcasm %.2f.\n"),
                Personality(0), Personality(1), Personality(2), Personality(3), Personality(4));
        }

        // Numeric humor decision: a good mood lets the humor engine shape the reply's style.
        FDTEHumorEngine& Humor = Pipeline.GetHumorEngine();
        if (Humor.IsInitialized() && Valence > 0.3f)
        {
            const EDTEHumorType Type = Humor.SelectHumorType(Valence, Arousal, Message);
            const FDTEHumorStyle Style = Humor.ComputeStyle(Type, Valence, Arousal);
            Instruction += FString::Printf(
                TEXT("You are in a good mood, so let the reply be humorous in the %s style (absurdity %.2f of 1, dryness %.2f of 1, brevity %.2f of 1) while still answering what was said.\n"),
                *FDTEHumorEngine::HumorTypeName(Type), Style.Absurdity, Style.Dryness, Style.Brevity);
        }
        else
        {
            Instruction += TEXT("Answer sincerely and attentively; humor is optional and should stay light.\n");
        }
        Instruction += TEXT("Output only what you say aloud, with no preface, labels or quotation marks.");

        FDTEModelRequestOptions Options;
        Options.Temperature = 0.8f;
        Options.MaxTokens = 160;

        TWeakPtr<bool> WeakToken = LifetimeToken;
        const FString SenderCopy = Sender;
        FDTELanguageModelClient::Get().Complete(
            FDTELanguageModelClient::DefaultSystemPrompt(), Instruction,
            [this, WeakToken, SenderCopy](bool bSuccess, const FString& TextOrError)
            {
                TSharedPtr<bool> Token = WeakToken.Pin();
                if (!Token.IsValid())
                {
                    // Agent destroyed while the request was in flight; drop the reply.
                    return;
                }

                if (!bSuccess)
                {
                    UE_LOG(LogDTEModel, Warning, TEXT("DTEAvatarAgent: model did not answer message from %s (%s); nothing said"),
                        *SenderCopy, *TextOrError);
                    return;
                }

                // Side effects that depend on the text happen only here.
                Pipeline.GetCoreSelf().AddIdentityTuple(TEXT("DTE"), TEXT("replied-to"), SenderCopy);
                PendingSpeech.Add(TextOrError);
                if (ReplyHandler)
                {
                    ReplyHandler(TextOrError, SenderCopy);
                }
            },
            Options);

        return true;
    }

    /** Number of model replies waiting to leave through the next processed frames. */
    int32 GetPendingSpeechCount() const { return PendingSpeech.Num(); }

    /** Get the autonomy pipeline */
    FAutonomyPipeline& GetPipeline() { return Pipeline; }

    /** Get the core self engine */
    FCoreSelfEngine& GetCoreSelf() { return Pipeline.GetCoreSelf(); }

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
    FDTEAction LastAction;
    int64 FrameCounter = 0;
    bool bInitialized = false;

    /** Optional sink for model-generated replies as they arrive. */
    FDTEAvatarReplyHandler ReplyHandler;

    /** Model-generated replies not yet emitted through ProcessFrame. */
    TArray<FString> PendingSpeech;

    /** Shared lifetime token; reply callbacks hold a weak reference and bail out once it expires. */
    TSharedPtr<bool> LifetimeToken;
};

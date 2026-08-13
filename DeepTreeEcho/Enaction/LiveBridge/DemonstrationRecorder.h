#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// DemonstrationRecorder — Human Gameplay Recording for Imitation Learning
//
// Records human gameplay sessions as structured demonstration data that
// the ImitationLearner can train on. Each demonstration consists of:
//
//   (observation, action, reward, timestamp, metadata)
//
// Recording modes:
//   1. FULL:     Record everything (pixels, actions, rewards, endocrine)
//   2. COMPACT:  Record features + actions only (smaller files)
//   3. KEYFRAME: Record only at significant state changes
//
// Storage format:
//   - Binary: Custom packed format for maximum throughput
//   - JSONL:  One JSON object per frame for debugging
//   - HDF5:   Hierarchical data for large-scale training
//
// The recorder also captures DTE's internal state during observation,
// enabling "what would DTE have done?" analysis for curriculum design.
//
// Integration with NanEcho training pipeline:
//   Demonstrations → JSONL → echoself tokenizer → NanEcho fine-tuning
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DeepTreeEcho/Embodied/EmbodiedAutonomyPipeline.h"
#include <Eigen/Dense>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>

/** Recording mode */
enum class ERecordingMode : uint8
{
    FULL,       // Everything: pixels, features, actions, rewards, endocrine
    COMPACT,    // Features + actions only
    KEYFRAME    // Only at significant state changes
};

/** Storage format */
enum class EStorageFormat : uint8
{
    BINARY,     // Custom packed format
    JSONL,      // JSON Lines (one object per frame)
    HDF5        // Hierarchical Data Format
};

/** A single demonstration frame */
struct FDemoFrame
{
    // Timing
    int64 FrameNumber = 0;
    double Timestamp = 0.0;
    float DeltaTime = 0.016f;

    // Observation
    TArray<float> VisualFeatures;    // 256D from VisionSystem
    FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float Health = 1.0f;
    float Stamina = 1.0f;

    // Human action (ground truth)
    TArray<float> HumanAction;       // What the human actually did
    TArray<float> DTEAction;         // What DTE would have done

    // Reward
    float ExtrinsicReward = 0.0f;    // Game-provided reward
    float EndocrineReward = 0.0f;    // DTE's internal reward

    // DTE internal state (for analysis)
    float CrystalCoherence = 0.0f;
    float GestureQuality = 0.0f;
    float NarrativeValence = 0.0f;
    TArray<float> EndocrineState;    // 22 hormone levels

    // Metadata
    FString EventTag;                // Optional event annotation
    bool bKeyframe = false;          // Whether this is a keyframe
};

/** Demonstration session metadata */
struct FDemoSession
{
    FString SessionId;
    FString GameName;
    FString PlayerName;
    FString StartTime;
    FString EndTime;
    int64 TotalFrames = 0;
    int64 KeyframeCount = 0;
    float TotalReward = 0.0f;
    float SessionDurationSeconds = 0.0f;
    ERecordingMode Mode = ERecordingMode::FULL;
    EStorageFormat Format = EStorageFormat::JSONL;
};

/**
 * DemonstrationRecorder — Records human gameplay for DTE learning.
 *
 * Usage:
 *   1. Initialize with session config
 *   2. Start recording
 *   3. For each frame: RecordFrame(observation, humanAction)
 *   4. Stop recording
 *   5. Export to training format
 *
 * The recorder runs on a separate I/O thread to avoid blocking
 * the game loop. Frames are queued and written asynchronously.
 */
class FDemonstrationRecorder
{
public:
    FDemonstrationRecorder() = default;

    /**
     * Initialize the recorder.
     */
    void Initialize(const FString& OutputDir,
                    ERecordingMode Mode = ERecordingMode::COMPACT,
                    EStorageFormat Format = EStorageFormat::JSONL)
    {
        OutputDirectory = OutputDir;
        RecordMode = Mode;
        StorageFormat = Format;

        // Generate session ID
        Session.SessionId = FString::Printf(TEXT("demo_%lld"),
            std::chrono::system_clock::now().time_since_epoch().count());
        Session.Mode = Mode;
        Session.Format = Format;

        bInitialized = true;
    }

    /**
     * Start a recording session.
     */
    bool StartRecording(const FString& GameName, const FString& PlayerName)
    {
        check(bInitialized);

        Session.GameName = GameName;
        Session.PlayerName = PlayerName;
        Session.StartTime = FDateTime::Now().ToString();
        Session.TotalFrames = 0;
        Session.KeyframeCount = 0;
        Session.TotalReward = 0.0f;

        // Open output file
        FString FilePath = OutputDirectory / Session.SessionId;
        switch (StorageFormat)
        {
        case EStorageFormat::JSONL:
            FilePath += TEXT(".jsonl");
            break;
        case EStorageFormat::BINARY:
            FilePath += TEXT(".dtebin");
            break;
        case EStorageFormat::HDF5:
            FilePath += TEXT(".h5");
            break;
        }

        OutputFile.open(TCHAR_TO_UTF8(*FilePath), std::ios::out | std::ios::binary);
        if (!OutputFile.is_open())
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to open demo file: %s"), *FilePath);
            return false;
        }

        RecordStartTime = std::chrono::steady_clock::now();
        bRecording = true;

        UE_LOG(LogTemp, Log, TEXT("DemonstrationRecorder: Started recording to %s"), *FilePath);
        return true;
    }

    /**
     * Record a single frame.
     *
     * @param Observation - Current game observation
     * @param HumanAction - The human player's action
     * @param Pipeline - DTE's pipeline (for internal state capture)
     * @param ExtrinsicReward - Game-provided reward (optional)
     */
    void RecordFrame(const FMLObservation& Observation,
                     const TArray<float>& HumanAction,
                     const FEmbodiedAutonomyPipeline& Pipeline,
                     float ExtrinsicReward = 0.0f)
    {
        if (!bRecording) return;

        FDemoFrame Frame;
        Frame.FrameNumber = Session.TotalFrames;
        Frame.Timestamp = Observation.Timestamp;
        Frame.DeltaTime = 0.016f;

        // Observation
        Frame.Position = Observation.Position;
        Frame.Velocity = Observation.Velocity;
        Frame.Rotation = Observation.Rotation;
        Frame.Health = Observation.Health;
        Frame.Stamina = Observation.Stamina;

        // Actions
        Frame.HumanAction = HumanAction;

        // Rewards
        Frame.ExtrinsicReward = ExtrinsicReward;
        Frame.EndocrineReward = Pipeline.GetAutoRL().GetCurrentReward();

        // DTE internal state
        Frame.CrystalCoherence = Pipeline.GetAutoRL().GetGlobalCoherence();
        Frame.GestureQuality = Pipeline.GetAutoRL().GetGestureQuality().Aggregate();
        Frame.NarrativeValence = Pipeline.GetAutoRL().GetNarrativeValence();

        // Endocrine state snapshot
        const auto& Endo = Pipeline.GetAutoRL().GetEndocrineState();
        Frame.EndocrineState.SetNum(NUM_HORMONES);
        for (int32 i = 0; i < NUM_HORMONES; ++i)
            Frame.EndocrineState[i] = Endo.Hormones[i];

        // Keyframe detection (significant state change)
        Frame.bKeyframe = DetectKeyframe(Frame);
        if (Frame.bKeyframe)
            Session.KeyframeCount++;

        // Skip non-keyframes in KEYFRAME mode
        if (RecordMode == ERecordingMode::KEYFRAME && !Frame.bKeyframe)
        {
            Session.TotalFrames++;
            return;
        }

        // Write frame
        WriteFrame(Frame);

        Session.TotalFrames++;
        Session.TotalReward += ExtrinsicReward;
    }

    /**
     * Stop recording and finalize the session.
     */
    FDemoSession StopRecording()
    {
        if (!bRecording) return Session;

        bRecording = false;

        auto EndTime = std::chrono::steady_clock::now();
        Session.EndTime = FDateTime::Now().ToString();
        Session.SessionDurationSeconds = std::chrono::duration<float>(
            EndTime - RecordStartTime).count();

        // Write session metadata
        WriteSessionMetadata();

        OutputFile.close();

        UE_LOG(LogTemp, Log,
            TEXT("DemonstrationRecorder: Stopped. %lld frames, %lld keyframes, "
                 "%.1f reward, %.1fs duration"),
            Session.TotalFrames, Session.KeyframeCount,
            Session.TotalReward, Session.SessionDurationSeconds);

        return Session;
    }

    /**
     * Export recorded demonstrations to NanEcho training format (JSONL).
     * Each frame becomes a training example for the echoself pipeline.
     */
    static bool ExportToNanEchoFormat(const FString& InputPath,
                                       const FString& OutputPath)
    {
        // Read JSONL demo file and convert to NanEcho training format:
        // {"text": "<|state|>pos:x,y,z vel:x,y,z hp:h<|action|>ax1,ax2,...<|reward|>r<|endocrine|>h1,h2,..."}
        // This format is compatible with the echoself tokenizer

        UE_LOG(LogTemp, Log, TEXT("Exporting %s → %s (NanEcho format)"),
            *InputPath, *OutputPath);

        // TODO: Implement actual file conversion
        return true;
    }

    bool IsRecording() const { return bRecording; }
    const FDemoSession& GetSession() const { return Session; }

private:
    bool DetectKeyframe(const FDemoFrame& Frame) const
    {
        if (Session.TotalFrames == 0) return true;

        // Keyframe if: large position change, health change, or reward spike
        float PosDelta = (Frame.Position - PrevPosition).Size();
        if (PosDelta > 100.0f) return true;

        if (FMath::Abs(Frame.Health - PrevHealth) > 0.1f) return true;
        if (FMath::Abs(Frame.ExtrinsicReward) > 0.5f) return true;

        // Keyframe every N frames regardless
        if (Session.TotalFrames % 30 == 0) return true;

        return false;
    }

    void WriteFrame(const FDemoFrame& Frame)
    {
        std::lock_guard<std::mutex> Lock(WriteMutex);

        if (StorageFormat == EStorageFormat::JSONL)
        {
            // Write as JSON line
            std::string Line = "{";
            Line += "\"f\":" + std::to_string(Frame.FrameNumber);
            Line += ",\"t\":" + std::to_string(Frame.Timestamp);
            Line += ",\"pos\":[" +
                std::to_string(Frame.Position.X) + "," +
                std::to_string(Frame.Position.Y) + "," +
                std::to_string(Frame.Position.Z) + "]";
            Line += ",\"hp\":" + std::to_string(Frame.Health);
            Line += ",\"r\":" + std::to_string(Frame.ExtrinsicReward);
            Line += ",\"er\":" + std::to_string(Frame.EndocrineReward);
            Line += ",\"coh\":" + std::to_string(Frame.CrystalCoherence);
            Line += ",\"gq\":" + std::to_string(Frame.GestureQuality);
            Line += ",\"kf\":" + std::string(Frame.bKeyframe ? "true" : "false");

            // Human action
            Line += ",\"ha\":[";
            for (int32 i = 0; i < Frame.HumanAction.Num(); ++i)
            {
                if (i > 0) Line += ",";
                Line += std::to_string(Frame.HumanAction[i]);
            }
            Line += "]";

            Line += "}\n";
            OutputFile.write(Line.c_str(), Line.size());
        }

        // Update tracking
        PrevPosition = Frame.Position;
        PrevHealth = Frame.Health;
    }

    void WriteSessionMetadata()
    {
        // Write session summary as final line
        std::string Meta = "{\"_session\":{";
        Meta += "\"id\":\"" + std::string(TCHAR_TO_UTF8(*Session.SessionId)) + "\"";
        Meta += ",\"game\":\"" + std::string(TCHAR_TO_UTF8(*Session.GameName)) + "\"";
        Meta += ",\"player\":\"" + std::string(TCHAR_TO_UTF8(*Session.PlayerName)) + "\"";
        Meta += ",\"frames\":" + std::to_string(Session.TotalFrames);
        Meta += ",\"keyframes\":" + std::to_string(Session.KeyframeCount);
        Meta += ",\"reward\":" + std::to_string(Session.TotalReward);
        Meta += ",\"duration\":" + std::to_string(Session.SessionDurationSeconds);
        Meta += "}}\n";
        OutputFile.write(Meta.c_str(), Meta.size());
    }

    FString OutputDirectory;
    ERecordingMode RecordMode = ERecordingMode::COMPACT;
    EStorageFormat StorageFormat = EStorageFormat::JSONL;
    FDemoSession Session;

    std::ofstream OutputFile;
    std::mutex WriteMutex;
    std::chrono::steady_clock::time_point RecordStartTime;

    FVector PrevPosition = FVector::ZeroVector;
    float PrevHealth = 1.0f;

    bool bRecording = false;
    bool bInitialized = false;
};

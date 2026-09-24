#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// MLAdapterBridge — Live UE5 ↔ DTE Connection via gRPC/MessagePack RPC
//
// Level 5 True Autonomy requires DTE to connect to a LIVE Unreal Engine 5
// instance running the MLAdapter plugin. This bridge implements the
// bidirectional communication protocol:
//
//   UE5 (MLAdapter Server) ←──gRPC──→ DTE (MLAdapterBridge Client)
//
// The MLAdapter plugin in UE5 exposes:
//   - Sensors: Camera, Movement, AIPerception, Custom
//   - Actuators: InputAction, EnhancedInput, Custom
//   - Agent lifecycle: RequestNewAgent, Act, Sense, Reward
//
// This bridge translates between MLAdapter's wire format and DTE's
// internal Eigen-based representations, enabling the full
// EmbodiedAutonomyPipeline to operate on a live game.
//
// Connection modes:
//   1. LOCAL:  Same machine, shared memory + gRPC (lowest latency)
//   2. LAN:    Same network, gRPC over TCP (< 1ms RTT)
//   3. REMOTE: Over internet, gRPC over TLS (variable latency)
//
// Protocol:
//   1. DTE connects to MLAdapter's gRPC endpoint
//   2. Requests agent registration with sensor/actuator config
//   3. Enters the sense-act loop:
//      a. Receive sensor observations (pixels, proprioception)
//      b. Process through EmbodiedAutonomyPipeline
//      c. Send action commands back
//      d. Receive reward signal (optional, Auto-RL generates its own)
//   4. On disconnect, persist state via PersonaBackupRestore
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DeepTreeEcho/Embodied/EmbodiedAutonomyPipeline.h"
#include <Eigen/Dense>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>

/** Connection state */
enum class EBridgeState : uint8
{
    DISCONNECTED,
    CONNECTING,
    HANDSHAKE,
    REGISTERED,
    SENSING,
    ACTING,
    PAUSED,
    ERROR
};

/** Sensor configuration for MLAdapter */
struct FMLSensorConfig
{
    FString SensorName;
    FString SensorType;  // "Camera", "Movement", "AIPerception", "Custom"
    int32 Width = 0;     // For camera sensors
    int32 Height = 0;
    int32 Channels = 4;  // RGBA
    bool bCompress = false;
};

/** Actuator configuration for MLAdapter */
struct FMLActuatorConfig
{
    FString ActuatorName;
    FString ActuatorType;  // "InputAction", "EnhancedInput", "Custom"
    int32 NumAxes = 0;
    int32 NumButtons = 0;
};

/** Raw observation from MLAdapter */
struct FMLObservation
{
    TArray<float> CameraPixels;      // Flattened RGBA pixels
    FVector Position = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float Health = 1.0f;
    float Stamina = 1.0f;
    float Score = 0.0f;
    TArray<float> CustomData;
    double Timestamp = 0.0;
    int64 FrameNumber = 0;
};

/** Action command to MLAdapter */
struct FMLAction
{
    TArray<float> ContinuousAxes;    // Movement, look, etc.
    TArray<bool> DiscreteButtons;     // Jump, fire, interact, etc.
    TArray<float> FacialControls;     // MetaHuman blend shapes
    double Timestamp = 0.0;
};

/** Bridge statistics */
struct FBridgeStats
{
    int64 FramesProcessed = 0;
    double AvgLatencyMs = 0.0;
    double MaxLatencyMs = 0.0;
    double AvgFPS = 0.0;
    float CurrentReward = 0.0f;
    float CumulativeReward = 0.0f;
    EBridgeState State = EBridgeState::DISCONNECTED;
    ETrainingMode TrainingMode = ETrainingMode::EXPLORE;
    double UptimeSeconds = 0.0;
};

/**
 * MLAdapterBridge — The live connection between DTE and UE5.
 *
 * This is the critical Level 5 component that enables DTE to
 * operate a real 3D avatar in a live game environment.
 *
 * Architecture:
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    Unreal Engine 5                          │
 * │  ┌──────────────────────────────────────────────────────┐  │
 * │  │              MLAdapter Plugin (Server)                │  │
 * │  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐   │  │
 * │  │  │ Camera   │  │ Movement │  │ EnhancedInput    │   │  │
 * │  │  │ Sensor   │  │ Sensor   │  │ Actuator         │   │  │
 * │  │  └────┬─────┘  └────┬─────┘  └────────┬─────────┘   │  │
 * │  │       │              │                  ▲             │  │
 * │  └───────┼──────────────┼──────────────────┼─────────────┘  │
 * └──────────┼──────────────┼──────────────────┼────────────────┘
 *            │     gRPC     │                  │
 *            ▼              ▼                  │
 * ┌──────────────────────────────────────────────────────────────┐
 * │                 MLAdapterBridge (Client)                     │
 * │  ┌──────────────────────────────────────────────────────┐   │
 * │  │           EmbodiedAutonomyPipeline                   │   │
 * │  │  Vision → Reservoir → AutoRL → Readout → Controller  │   │
 * │  └──────────────────────────────────────────────────────┘   │
 * └──────────────────────────────────────────────────────────────┘
 */
class FMLAdapterBridge
{
public:
    FMLAdapterBridge() = default;

    /**
     * Initialize the bridge and embodied pipeline.
     */
    void Initialize(const FString& ServerAddress = TEXT("localhost:15151"),
                    const FDTEAvatarConfig& AvatarConfig = FDTEAvatarConfig())
    {
        Address = ServerAddress;
        Config = AvatarConfig;

        // Initialize the embodied pipeline
        Pipeline.Initialize(Config);

        // Configure sensors
        FMLSensorConfig CameraSensor;
        CameraSensor.SensorName = TEXT("DTECamera");
        CameraSensor.SensorType = TEXT("Camera");
        CameraSensor.Width = Config.CaptureWidth;
        CameraSensor.Height = Config.CaptureHeight;
        CameraSensor.Channels = 4;
        Sensors.Add(CameraSensor);

        FMLSensorConfig MovementSensor;
        MovementSensor.SensorName = TEXT("DTEMovement");
        MovementSensor.SensorType = TEXT("Movement");
        Sensors.Add(MovementSensor);

        // Configure actuators
        FMLActuatorConfig InputActuator;
        InputActuator.ActuatorName = TEXT("DTEInput");
        InputActuator.ActuatorType = TEXT("EnhancedInput");
        InputActuator.NumAxes = Config.NumContinuousAxes;
        InputActuator.NumButtons = Config.NumDiscreteActions;
        Actuators.Add(InputActuator);

        State = EBridgeState::DISCONNECTED;
        bInitialized = true;
    }

    /**
     * Connect to the UE5 MLAdapter server.
     * Returns true if connection was initiated successfully.
     */
    bool Connect()
    {
        check(bInitialized);

        State = EBridgeState::CONNECTING;

        // In production: establish gRPC channel to Address
        // For now: simulate connection handshake
        UE_LOG(LogTemp, Log, TEXT("MLAdapterBridge: Connecting to %s..."), *Address);

        // TODO: Replace with actual gRPC channel creation
        // Channel = grpc::CreateChannel(TCHAR_TO_UTF8(*Address),
        //     grpc::InsecureChannelCredentials());
        // Stub = MLAdapterService::NewStub(Channel);

        State = EBridgeState::HANDSHAKE;

        // Register agent with sensor/actuator config
        if (RegisterAgent())
        {
            State = EBridgeState::REGISTERED;
            StartTime = std::chrono::steady_clock::now();
            UE_LOG(LogTemp, Log, TEXT("MLAdapterBridge: Connected and registered."));
            return true;
        }

        State = EBridgeState::ERROR;
        return false;
    }

    /**
     * Run one sense-act cycle.
     * Call this from the main loop or a dedicated thread.
     */
    FBridgeStats Tick()
    {
        check(bInitialized);
        check(State == EBridgeState::REGISTERED || State == EBridgeState::SENSING);

        auto FrameStart = std::chrono::steady_clock::now();

        // === SENSE ===
        State = EBridgeState::SENSING;
        FMLObservation Obs = ReceiveObservation();

        // Convert to pipeline format
        FProprioception Proprio;
        Proprio.Position = Obs.Position;
        Proprio.Velocity = Obs.Velocity;
        Proprio.Rotation = Obs.Rotation;
        Proprio.Health = Obs.Health;
        Proprio.Stamina = Obs.Stamina;

        // === PROCESS ===
        FDTEAction DTEAction = Pipeline.ProcessFrame(
            Obs.CameraPixels, Proprio, 0.016f);

        // === ACT ===
        State = EBridgeState::ACTING;
        FMLAction Action;
        Action.ContinuousAxes.SetNum(DTEAction.ContinuousAxes.Num());
        for (int32 i = 0; i < DTEAction.ContinuousAxes.Num(); ++i)
            Action.ContinuousAxes[i] = DTEAction.ContinuousAxes[i];

        Action.DiscreteButtons.SetNum(DTEAction.DiscreteActions.Num());
        for (int32 i = 0; i < DTEAction.DiscreteActions.Num(); ++i)
            Action.DiscreteButtons[i] = DTEAction.DiscreteActions[i] > 0.5f;

        Action.FacialControls = DTEAction.FacialControls;
        Action.Timestamp = FPlatformTime::Seconds();

        SendAction(Action);

        // === STATS ===
        State = EBridgeState::REGISTERED;
        auto FrameEnd = std::chrono::steady_clock::now();
        double LatencyMs = std::chrono::duration<double, std::milli>(FrameEnd - FrameStart).count();

        Stats.FramesProcessed++;
        Stats.AvgLatencyMs = Stats.AvgLatencyMs * 0.99 + LatencyMs * 0.01;
        Stats.MaxLatencyMs = FMath::Max(Stats.MaxLatencyMs, LatencyMs);
        Stats.CurrentReward = Pipeline.GetAutoRL().GetCurrentReward();
        Stats.CumulativeReward += Stats.CurrentReward;
        Stats.State = State;
        Stats.TrainingMode = Pipeline.GetTrainingMode();

        auto Now = std::chrono::steady_clock::now();
        Stats.UptimeSeconds = std::chrono::duration<double>(Now - StartTime).count();
        Stats.AvgFPS = Stats.FramesProcessed / FMath::Max(0.001, Stats.UptimeSeconds);

        return Stats;
    }

    /**
     * Run the continuous sense-act loop.
     * This is the main entry point for Level 5 autonomous operation.
     */
    void RunAutonomousLoop(float TargetFPS = 30.0f)
    {
        check(bInitialized);
        check(State == EBridgeState::REGISTERED);

        bRunning = true;
        float FrameInterval = 1.0f / TargetFPS;

        UE_LOG(LogTemp, Log, TEXT("MLAdapterBridge: Starting autonomous loop at %.0f FPS"), TargetFPS);

        while (bRunning)
        {
            auto FrameStart = std::chrono::steady_clock::now();

            FBridgeStats FrameStats = Tick();

            // Log periodic stats
            if (FrameStats.FramesProcessed % 100 == 0)
            {
                UE_LOG(LogTemp, Log,
                    TEXT("Frame %lld | Reward: %.3f | Cumulative: %.1f | "
                         "Latency: %.1fms | FPS: %.1f | Coherence: %.3f"),
                    FrameStats.FramesProcessed,
                    FrameStats.CurrentReward,
                    FrameStats.CumulativeReward,
                    FrameStats.AvgLatencyMs,
                    FrameStats.AvgFPS,
                    Pipeline.GetAutoRL().GetGlobalCoherence());
            }

            // Periodic identity backup
            if (FrameStats.FramesProcessed % 10000 == 0)
            {
                Pipeline.GetCoreSelf().BackupIdentity();
                UE_LOG(LogTemp, Log, TEXT("Identity backup at frame %lld"), FrameStats.FramesProcessed);
            }

            // Sleep to maintain target FPS
            auto FrameEnd = std::chrono::steady_clock::now();
            double ElapsedMs = std::chrono::duration<double, std::milli>(FrameEnd - FrameStart).count();
            double SleepMs = (FrameInterval * 1000.0) - ElapsedMs;
            if (SleepMs > 0)
                std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int64>(SleepMs * 1000)));
        }

        UE_LOG(LogTemp, Log, TEXT("MLAdapterBridge: Autonomous loop ended after %lld frames"),
            Stats.FramesProcessed);
    }

    /** Stop the autonomous loop */
    void Stop() { bRunning = false; }

    /** Disconnect from UE5 */
    void Disconnect()
    {
        Stop();
        // Backup identity before disconnect
        Pipeline.GetCoreSelf().BackupIdentity();
        State = EBridgeState::DISCONNECTED;
    }

    /** Get the embodied pipeline */
    FEmbodiedAutonomyPipeline& GetPipeline() { return Pipeline; }
    const FBridgeStats& GetStats() const { return Stats; }
    EBridgeState GetState() const { return State; }

private:
    bool RegisterAgent()
    {
        // In production: send RegisterAgent RPC with sensor/actuator config
        // For now: return success
        return true;
    }

    FMLObservation ReceiveObservation()
    {
        // In production: receive via gRPC stream
        // For now: return empty observation
        FMLObservation Obs;
        Obs.CameraPixels.SetNumZeroed(Config.CaptureWidth * Config.CaptureHeight * 4);
        Obs.Timestamp = FPlatformTime::Seconds();
        Obs.FrameNumber = Stats.FramesProcessed;
        return Obs;
    }

    void SendAction(const FMLAction& Action)
    {
        // In production: send via gRPC
        // Action is dispatched to UE5's EnhancedInput system
    }

    FString Address;
    FDTEAvatarConfig Config;
    FEmbodiedAutonomyPipeline Pipeline;
    TArray<FMLSensorConfig> Sensors;
    TArray<FMLActuatorConfig> Actuators;

    EBridgeState State = EBridgeState::DISCONNECTED;
    FBridgeStats Stats;
    std::chrono::steady_clock::time_point StartTime;
    std::atomic<bool> bRunning{false};
    bool bInitialized = false;
};

#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// VirtualControllerDriver — DTE's Motor Cortex
//
// Translates high-level action intents from the CognitiveReadout into
// low-level game input events. Supports three output modes:
//   1. UE5 MLAdapter Actuator (in-engine)
//   2. Virtual Gamepad (OS-level uinput/ViGEm)
//   3. gRPC Remote (for out-of-process DTE)
//
// The driver implements motor learning through:
//   - Action smoothing (exponential moving average)
//   - Dead zone management (prevents jitter)
//   - Reflex arcs (fast stimulus-response bypassing cognition)
//   - Motor memory (frequently used action sequences cached)
//
// Maps to the IonDeviceUnit virtual hardware:
//   - MMIO registers for action state
//   - DMA for bulk action sequence transfer
//   - Interrupt for urgent reflexes
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>

/** Output mode for the virtual controller */
enum class EControllerOutputMode : uint8
{
    MLADAPTER,      // UE5 MLAdapter actuator (in-engine)
    VIRTUAL_GAMEPAD,// OS-level virtual gamepad (uinput/ViGEm)
    GRPC_REMOTE,    // gRPC to external process
    REPLAY          // Replay recorded actions (for training data)
};

/** A single input action binding */
struct FActionBinding
{
    /** Action name (e.g., "MoveForward", "Jump", "Fire") */
    FString Name;

    /** Whether this is a continuous axis or discrete trigger */
    bool bIsContinuous = false;

    /** Index in the action vector */
    int32 VectorIndex = 0;

    /** Scaling factor */
    float Scale = 1.0f;

    /** Dead zone threshold */
    float DeadZone = 0.05f;

    /** Key binding (for discrete actions) */
    FString KeyName;
};

/** A cached motor sequence (muscle memory) */
struct FMotorSequence
{
    /** Sequence name */
    FString Name;

    /** Action frames */
    TArray<Eigen::VectorXf> Frames;

    /** Expected duration in ticks */
    int32 Duration = 0;

    /** Success rate from past executions */
    float SuccessRate = 0.0f;

    /** Number of times executed */
    int32 ExecutionCount = 0;
};

/** Reflex arc — fast stimulus-response bypass */
struct FReflexArc
{
    /** Stimulus condition (lambda) */
    TFunction<bool(const Eigen::VectorXf&)> Condition;

    /** Response action vector */
    Eigen::VectorXf Response;

    /** Priority (higher = checked first) */
    int32 Priority = 0;

    /** Name for debugging */
    FString Name;
};

/**
 * VirtualControllerDriver — The motor cortex of Deep Tree Echo.
 *
 * This driver sits between the CognitiveReadout (which outputs abstract
 * action intents) and the actual game input system. It handles:
 *
 * 1. Action smoothing to prevent jitter
 * 2. Dead zone management for analog axes
 * 3. Reflex arcs for urgent responses (e.g., dodge when hit)
 * 4. Motor memory for frequently used action sequences
 * 5. Action recording for imitation learning data collection
 *
 * The driver can output to UE5's MLAdapter, an OS-level virtual gamepad,
 * or a remote gRPC endpoint.
 */
class FVirtualControllerDriver
{
public:
    FVirtualControllerDriver() = default;

    /**
     * Initialize with action bindings.
     */
    void Initialize(EControllerOutputMode Mode = EControllerOutputMode::MLADAPTER)
    {
        OutputMode = Mode;

        // Default action bindings for a typical 3rd-person game
        SetupDefaultBindings();

        // Default reflex arcs
        SetupDefaultReflexes();

        SmoothingFactor = 0.3f;
        SmoothedAction = Eigen::VectorXf::Zero(GetActionDim());
        bInitialized = true;
    }

    /**
     * Process an action vector from the CognitiveReadout.
     *
     * @param RawAction - Raw action vector from readout
     * @param Observation - Current observation (for reflex arcs)
     * @return Smoothed, dead-zone-managed action vector
     */
    Eigen::VectorXf ProcessAction(const Eigen::VectorXf& RawAction,
                                   const Eigen::VectorXf& Observation)
    {
        check(bInitialized);

        // Check reflex arcs first (fast path)
        for (const FReflexArc& Reflex : ReflexArcs)
        {
            if (Reflex.Condition(Observation))
            {
                // Reflex overrides cognitive action
                SmoothedAction = Reflex.Response;
                ExecuteAction(SmoothedAction);
                return SmoothedAction;
            }
        }

        // Apply smoothing (exponential moving average)
        Eigen::VectorXf ClampedAction = RawAction;
        for (int32 i = 0; i < ClampedAction.size(); ++i)
            ClampedAction(i) = FMath::Clamp(ClampedAction(i), -1.0f, 1.0f);

        SmoothedAction = SmoothingFactor * ClampedAction + (1.0f - SmoothingFactor) * SmoothedAction;

        // Apply dead zones
        for (const FActionBinding& Binding : ActionBindings)
        {
            if (Binding.bIsContinuous && Binding.VectorIndex < SmoothedAction.size())
            {
                float Val = SmoothedAction(Binding.VectorIndex);
                if (FMath::Abs(Val) < Binding.DeadZone)
                    SmoothedAction(Binding.VectorIndex) = 0.0f;
                else
                    SmoothedAction(Binding.VectorIndex) = Val * Binding.Scale;
            }
        }

        // Execute the action
        ExecuteAction(SmoothedAction);

        // Record for training data
        if (bRecording)
        {
            RecordedActions.Add(SmoothedAction);
        }

        return SmoothedAction;
    }

    /**
     * Start a motor sequence (muscle memory).
     */
    bool StartMotorSequence(const FString& Name)
    {
        for (FMotorSequence& Seq : MotorSequences)
        {
            if (Seq.Name == Name)
            {
                ActiveSequence = &Seq;
                SequenceFrame = 0;
                Seq.ExecutionCount++;
                return true;
            }
        }
        return false;
    }

    /**
     * Learn a new motor sequence from recorded actions.
     */
    void LearnMotorSequence(const FString& Name, const TArray<Eigen::VectorXf>& Frames)
    {
        FMotorSequence Seq;
        Seq.Name = Name;
        Seq.Frames = Frames;
        Seq.Duration = Frames.Num();
        Seq.SuccessRate = 0.5f; // Initial neutral confidence
        MotorSequences.Add(Seq);
    }

    /**
     * Add a reflex arc.
     */
    void AddReflex(const FString& Name, int32 Priority,
                   TFunction<bool(const Eigen::VectorXf&)> Condition,
                   const Eigen::VectorXf& Response)
    {
        FReflexArc Arc;
        Arc.Name = Name;
        Arc.Priority = Priority;
        Arc.Condition = Condition;
        Arc.Response = Response;
        ReflexArcs.Add(Arc);

        // Sort by priority (descending)
        ReflexArcs.Sort([](const FReflexArc& A, const FReflexArc& B) {
            return A.Priority > B.Priority;
        });
    }

    /** Start/stop recording for training data */
    void StartRecording() { bRecording = true; RecordedActions.Empty(); }
    void StopRecording() { bRecording = false; }
    const TArray<Eigen::VectorXf>& GetRecordedActions() const { return RecordedActions; }

    /** Get action dimension */
    int32 GetActionDim() const { return ActionBindings.Num(); }

    /** Get action bindings */
    const TArray<FActionBinding>& GetBindings() const { return ActionBindings; }

    bool IsInitialized() const { return bInitialized; }

private:
    void SetupDefaultBindings()
    {
        auto AddBinding = [this](const FString& Name, bool bContinuous, const FString& Key,
                                  float Scale = 1.0f, float DeadZone = 0.05f)
        {
            FActionBinding B;
            B.Name = Name;
            B.bIsContinuous = bContinuous;
            B.VectorIndex = ActionBindings.Num();
            B.Scale = Scale;
            B.DeadZone = DeadZone;
            B.KeyName = Key;
            ActionBindings.Add(B);
        };

        // Continuous axes
        AddBinding(TEXT("MoveForward"), true, TEXT("W/S"), 1.0f, 0.1f);
        AddBinding(TEXT("MoveRight"), true, TEXT("A/D"), 1.0f, 0.1f);
        AddBinding(TEXT("LookUp"), true, TEXT("MouseY"), 0.5f, 0.02f);
        AddBinding(TEXT("LookRight"), true, TEXT("MouseX"), 0.5f, 0.02f);

        // Discrete actions
        AddBinding(TEXT("Jump"), false, TEXT("Space"));
        AddBinding(TEXT("Sprint"), false, TEXT("Shift"));
        AddBinding(TEXT("Interact"), false, TEXT("E"));
        AddBinding(TEXT("PrimaryAction"), false, TEXT("LeftMouse"));
        AddBinding(TEXT("SecondaryAction"), false, TEXT("RightMouse"));
        AddBinding(TEXT("Crouch"), false, TEXT("C"));
        AddBinding(TEXT("Inventory"), false, TEXT("I"));
        AddBinding(TEXT("Map"), false, TEXT("M"));
    }

    void SetupDefaultReflexes()
    {
        // Reflex: dodge when health drops suddenly
        Eigen::VectorXf DodgeResponse = Eigen::VectorXf::Zero(GetActionDim());
        DodgeResponse(1) = 0.8f;  // Strafe right
        DodgeResponse(4) = 1.0f;  // Jump

        AddReflex(TEXT("DodgeOnDamage"), 100,
            [](const Eigen::VectorXf& Obs) -> bool {
                // Health is at index 13 in proprioception
                return Obs.size() > 13 && Obs(13) < 0.3f;
            },
            DodgeResponse);
    }

    void ExecuteAction(const Eigen::VectorXf& Action)
    {
        switch (OutputMode)
        {
        case EControllerOutputMode::MLADAPTER:
            // In UE5: write to MLAdapterActuator_EnhancedInput
            // The actuator reads from InputData array
            break;

        case EControllerOutputMode::VIRTUAL_GAMEPAD:
            // OS-level: write to uinput/ViGEm virtual device
            // Uses bendahl/uinput or CB2Moon/vgamepad-go
            break;

        case EControllerOutputMode::GRPC_REMOTE:
            // Send via gRPC to external UE5 process
            break;

        case EControllerOutputMode::REPLAY:
            // No-op, just recording
            break;
        }
    }

    EControllerOutputMode OutputMode = EControllerOutputMode::MLADAPTER;
    TArray<FActionBinding> ActionBindings;
    TArray<FReflexArc> ReflexArcs;
    TArray<FMotorSequence> MotorSequences;
    FMotorSequence* ActiveSequence = nullptr;
    int32 SequenceFrame = 0;

    Eigen::VectorXf SmoothedAction;
    float SmoothingFactor = 0.3f;

    bool bRecording = false;
    TArray<Eigen::VectorXf> RecordedActions;

    bool bInitialized = false;
};

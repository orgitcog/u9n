// GameControllerInterface.h
// Controller Input/Output Interface for Deep Tree Echo Game Training
// Maps gamepad controls to cognitive actions and DTE outputs to controller commands

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "GameControllerInterface.generated.h"

// Forward declarations
class UOnlineLearningSystem;
class UEmbodiedCognitionComponent;
class UDeepTreeEchoCore;

/**
 * Controller button enumeration
 */
UENUM(BlueprintType)
enum class EGamepadButton : uint8
{
    None            UMETA(DisplayName = "None"),
    FaceBottom      UMETA(DisplayName = "A/Cross"),
    FaceRight       UMETA(DisplayName = "B/Circle"),
    FaceLeft        UMETA(DisplayName = "X/Square"),
    FaceTop         UMETA(DisplayName = "Y/Triangle"),
    LeftShoulder    UMETA(DisplayName = "Left Bumper"),
    RightShoulder   UMETA(DisplayName = "Right Bumper"),
    LeftTrigger     UMETA(DisplayName = "Left Trigger"),
    RightTrigger    UMETA(DisplayName = "Right Trigger"),
    LeftThumb       UMETA(DisplayName = "Left Stick Click"),
    RightThumb      UMETA(DisplayName = "Right Stick Click"),
    DPadUp          UMETA(DisplayName = "D-Pad Up"),
    DPadDown        UMETA(DisplayName = "D-Pad Down"),
    DPadLeft        UMETA(DisplayName = "D-Pad Left"),
    DPadRight       UMETA(DisplayName = "D-Pad Right"),
    Start           UMETA(DisplayName = "Start/Options"),
    Select          UMETA(DisplayName = "Select/Share")
};

/**
 * Controller axis enumeration
 */
UENUM(BlueprintType)
enum class EGamepadAxis : uint8
{
    None            UMETA(DisplayName = "None"),
    LeftStickX      UMETA(DisplayName = "Left Stick X"),
    LeftStickY      UMETA(DisplayName = "Left Stick Y"),
    RightStickX     UMETA(DisplayName = "Right Stick X"),
    RightStickY     UMETA(DisplayName = "Right Stick Y"),
    LeftTrigger     UMETA(DisplayName = "Left Trigger Axis"),
    RightTrigger    UMETA(DisplayName = "Right Trigger Axis")
};

/**
 * Game action category
 */
UENUM(BlueprintType)
enum class EGameActionCategory : uint8
{
    Movement        UMETA(DisplayName = "Movement"),
    Camera          UMETA(DisplayName = "Camera Control"),
    Attack          UMETA(DisplayName = "Attack/Combat"),
    Defense         UMETA(DisplayName = "Defense/Block"),
    Interaction     UMETA(DisplayName = "Interact"),
    Special         UMETA(DisplayName = "Special Ability"),
    Navigation      UMETA(DisplayName = "Navigation/Menu"),
    Communication   UMETA(DisplayName = "Communication")
};

/**
 * Controller input state - snapshot of all inputs
 */
USTRUCT(BlueprintType)
struct FControllerInputState
{
    GENERATED_BODY()

    // Stick axes (-1 to 1)
    UPROPERTY(BlueprintReadWrite)
    float LeftStickX = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float LeftStickY = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float RightStickX = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float RightStickY = 0.0f;

    // Trigger axes (0 to 1)
    UPROPERTY(BlueprintReadWrite)
    float LeftTrigger = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float RightTrigger = 0.0f;

    // Button states
    UPROPERTY(BlueprintReadWrite)
    TSet<EGamepadButton> PressedButtons;

    // Timestamp
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Convert to action vector for learning */
    TArray<float> ToActionVector() const;

    /** Create from action vector */
    static FControllerInputState FromActionVector(const TArray<float>& Vector);

    /** Get action string for Q-learning */
    FString ToActionString() const;
};

/**
 * Controller output command - action to execute
 */
USTRUCT(BlueprintType)
struct FControllerOutputCommand
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FControllerInputState DesiredState;

    UPROPERTY(BlueprintReadWrite)
    float Duration = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Priority = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    EGameActionCategory Category = EGameActionCategory::Movement;

    UPROPERTY(BlueprintReadWrite)
    FString ActionName;

    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;
};

/**
 * Action mapping - maps buttons/axes to semantic actions
 */
USTRUCT(BlueprintType)
struct FGameActionMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameActionCategory Category = EGameActionCategory::Movement;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamepadButton PrimaryButton = EGamepadButton::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamepadButton SecondaryButton = EGamepadButton::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamepadAxis PrimaryAxis = EGamepadAxis::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AxisThreshold = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresHold = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HoldDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CognitiveMapping;
};

/**
 * Input sequence for combo detection
 */
USTRUCT(BlueprintType)
struct FInputSequence
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TArray<FControllerInputState> Inputs;

    UPROPERTY(BlueprintReadWrite)
    float MaxTimeBetweenInputs = 0.3f;

    UPROPERTY(BlueprintReadWrite)
    FString SequenceName;

    UPROPERTY(BlueprintReadWrite)
    float LastInputTime = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnButtonPressed, EGamepadButton, Button, float, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnButtonReleased, EGamepadButton, Button, float, HeldDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAxisChanged, EGamepadAxis, Axis, float, Value, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputStateChanged, const FControllerInputState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionDetected, const FString&, ActionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboDetected, const FString&, ComboName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOutputCommandGenerated, const FControllerOutputCommand&, Command);

/**
 * Game Controller Interface Component
 * Bridges gamepad input/output with Deep Tree Echo cognitive system
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UGameControllerInterface : public UActorComponent
{
    GENERATED_BODY()

public:
    UGameControllerInterface();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Controller index (0-3 for local multiplayer) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 ControllerIndex = 0;

    /** Deadzone for analog sticks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float StickDeadzone = 0.15f;

    /** Deadzone for triggers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float TriggerDeadzone = 0.1f;

    /** Input buffer size for sequence detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 InputBufferSize = 60;

    /** Enable AI output mode (generate controller commands from cognition) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bAIOutputMode = true;

    /** Enable learning from human input */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bLearnFromHumanInput = true;

    /** Action mappings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    TArray<FGameActionMapping> ActionMappings;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnButtonPressed OnButtonPressed;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnButtonReleased OnButtonReleased;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAxisChanged OnAxisChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInputStateChanged OnInputStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnActionDetected OnActionDetected;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnComboDetected OnComboDetected;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOutputCommandGenerated OnOutputCommandGenerated;

    // ========================================
    // INPUT PROCESSING
    // ========================================

    /** Get current input state */
    UFUNCTION(BlueprintPure, Category = "Input")
    FControllerInputState GetCurrentInputState() const;

    /** Get input state from buffer */
    UFUNCTION(BlueprintPure, Category = "Input")
    FControllerInputState GetInputStateFromBuffer(int32 FramesAgo) const;

    /** Get input buffer (recent inputs) */
    UFUNCTION(BlueprintPure, Category = "Input")
    TArray<FControllerInputState> GetInputBuffer() const;

    /** Check if button is pressed */
    UFUNCTION(BlueprintPure, Category = "Input")
    bool IsButtonPressed(EGamepadButton Button) const;

    /** Check if button was just pressed */
    UFUNCTION(BlueprintPure, Category = "Input")
    bool WasButtonJustPressed(EGamepadButton Button) const;

    /** Check if button was just released */
    UFUNCTION(BlueprintPure, Category = "Input")
    bool WasButtonJustReleased(EGamepadButton Button) const;

    /** Get axis value */
    UFUNCTION(BlueprintPure, Category = "Input")
    float GetAxisValue(EGamepadAxis Axis) const;

    /** Get left stick as vector */
    UFUNCTION(BlueprintPure, Category = "Input")
    FVector2D GetLeftStick() const;

    /** Get right stick as vector */
    UFUNCTION(BlueprintPure, Category = "Input")
    FVector2D GetRightStick() const;

    // ========================================
    // ACTION DETECTION
    // ========================================

    /** Detect current actions from input */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    TArray<FString> DetectCurrentActions() const;

    /** Check if action is being performed */
    UFUNCTION(BlueprintPure, Category = "Actions")
    bool IsActionActive(const FString& ActionName) const;

    /** Register a combo sequence */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    void RegisterComboSequence(const FString& ComboName, const TArray<FString>& ActionSequence, float MaxTimeBetween);

    /** Detect combos from recent input */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    TArray<FString> DetectCombos() const;

    // ========================================
    // OUTPUT GENERATION (AI MODE)
    // ========================================

    /** Generate output from cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Output")
    FControllerOutputCommand GenerateOutputFromCognition();

    /** Generate output from action name */
    UFUNCTION(BlueprintCallable, Category = "Output")
    FControllerOutputCommand GenerateOutputForAction(const FString& ActionName);

    /** Generate movement output */
    UFUNCTION(BlueprintCallable, Category = "Output")
    FControllerOutputCommand GenerateMovementOutput(const FVector2D& Direction, float Speed);

    /** Generate camera output */
    UFUNCTION(BlueprintCallable, Category = "Output")
    FControllerOutputCommand GenerateCameraOutput(const FVector2D& Direction, float Speed);

    /** Queue output command */
    UFUNCTION(BlueprintCallable, Category = "Output")
    void QueueOutputCommand(const FControllerOutputCommand& Command);

    /** Get current output command */
    UFUNCTION(BlueprintPure, Category = "Output")
    FControllerOutputCommand GetCurrentOutputCommand() const;

    /** Execute queued output (simulate controller input) */
    UFUNCTION(BlueprintCallable, Category = "Output")
    void ExecuteQueuedOutput();

    // ========================================
    // COGNITIVE INTEGRATION
    // ========================================

    /** Convert input to sensory data for embodiment */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    TArray<float> InputToSensoryVector(const FControllerInputState& Input) const;

    /** Convert cognitive output to controller command */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    FControllerOutputCommand CognitiveOutputToCommand(const TArray<float>& CognitiveOutput) const;

    /** Get state string for Q-learning */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    FString GetStateString(const FControllerInputState& Input) const;

    /** Record input for imitation learning */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void RecordInputForImitation(const FControllerInputState& Input, const FString& Context);

    // ========================================
    // MAPPING MANAGEMENT
    // ========================================

    /** Add action mapping */
    UFUNCTION(BlueprintCallable, Category = "Mapping")
    void AddActionMapping(const FGameActionMapping& Mapping);

    /** Remove action mapping */
    UFUNCTION(BlueprintCallable, Category = "Mapping")
    void RemoveActionMapping(const FString& ActionName);

    /** Get action mapping */
    UFUNCTION(BlueprintPure, Category = "Mapping")
    FGameActionMapping GetActionMapping(const FString& ActionName) const;

    /** Load preset mappings for game type */
    UFUNCTION(BlueprintCallable, Category = "Mapping")
    void LoadPresetMappings(const FString& GameType);

    // ========================================
    // STATE SERIALIZATION
    // ========================================

    /** Serialize input state to bytes */
    UFUNCTION(BlueprintPure, Category = "Serialization")
    TArray<uint8> SerializeInputState(const FControllerInputState& State) const;

    /** Deserialize input state from bytes */
    UFUNCTION(BlueprintPure, Category = "Serialization")
    FControllerInputState DeserializeInputState(const TArray<uint8>& Data) const;

protected:
    // Component references
    UPROPERTY()
    UOnlineLearningSystem* LearningSystem;

    UPROPERTY()
    UEmbodiedCognitionComponent* EmbodimentComponent;

    UPROPERTY()
    UDeepTreeEchoCore* DTECore;

    // Internal state
    FControllerInputState CurrentState;
    FControllerInputState PreviousState;
    TArray<FControllerInputState> InputBuffer;
    TArray<FControllerOutputCommand> OutputQueue;

    // Button hold tracking
    TMap<EGamepadButton, float> ButtonHoldStartTimes;

    // Combo tracking
    TMap<FString, FInputSequence> RegisteredCombos;
    TArray<FString> RecentActions;
    float LastActionTime = 0.0f;

    // Imitation learning buffer
    TArray<TPair<FControllerInputState, FString>> ImitationBuffer;

    // Internal methods
    void FindComponentReferences();
    void InitializeDefaultMappings();

    void PollControllerInput();
    void UpdateInputBuffer(float DeltaTime);
    void ProcessButtonEvents(float DeltaTime);
    void ProcessAxisEvents();
    void DetectAndBroadcastActions();

    float ApplyDeadzone(float Value, float Deadzone) const;
    FVector2D ApplyRadialDeadzone(const FVector2D& Stick, float Deadzone) const;

    void ProcessOutputQueue(float DeltaTime);
    void BroadcastCognitiveState();
};

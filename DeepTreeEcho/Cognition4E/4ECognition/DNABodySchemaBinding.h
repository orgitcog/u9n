// DNABodySchemaBinding.h
// Binds MetaHuman DNA joint/blend shape definitions to 4E embodied cognition body schema
// Enables dynamic body representation based on avatar DNA calibration

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DNABodySchemaBinding.generated.h"

// Forward declarations
class UEmbodiedCognitionComponent;
class UDeepTreeEchoReservoir;

// ========================================
// DNA BINDING ENUMERATIONS
// ========================================

/**
 * Body region classification for proprioceptive mapping
 */
UENUM(BlueprintType)
enum class EBodyRegion : uint8
{
    Head            UMETA(DisplayName = "Head"),
    Neck            UMETA(DisplayName = "Neck"),
    Torso           UMETA(DisplayName = "Torso"),
    Spine           UMETA(DisplayName = "Spine"),
    Pelvis          UMETA(DisplayName = "Pelvis"),
    LeftArm         UMETA(DisplayName = "Left Arm"),
    RightArm        UMETA(DisplayName = "Right Arm"),
    LeftHand        UMETA(DisplayName = "Left Hand"),
    RightHand       UMETA(DisplayName = "Right Hand"),
    LeftLeg         UMETA(DisplayName = "Left Leg"),
    RightLeg        UMETA(DisplayName = "Right Leg"),
    LeftFoot        UMETA(DisplayName = "Left Foot"),
    RightFoot       UMETA(DisplayName = "Right Foot"),
    Face            UMETA(DisplayName = "Face"),
    Unknown         UMETA(DisplayName = "Unknown")
};

/**
 * Motor effector type for sensorimotor capabilities
 */
UENUM(BlueprintType)
enum class EMotorEffectorType : uint8
{
    Locomotion      UMETA(DisplayName = "Locomotion"),
    Manipulation    UMETA(DisplayName = "Manipulation"),
    Gaze            UMETA(DisplayName = "Gaze"),
    Expression      UMETA(DisplayName = "Expression"),
    Vocalization    UMETA(DisplayName = "Vocalization"),
    Balance         UMETA(DisplayName = "Balance"),
    Posture         UMETA(DisplayName = "Posture")
};

/**
 * Blend shape category for facial expression mapping
 */
UENUM(BlueprintType)
enum class EBlendShapeCategory : uint8
{
    Brow            UMETA(DisplayName = "Brow"),
    Eye             UMETA(DisplayName = "Eye"),
    Nose            UMETA(DisplayName = "Nose"),
    Cheek           UMETA(DisplayName = "Cheek"),
    Mouth           UMETA(DisplayName = "Mouth"),
    Jaw             UMETA(DisplayName = "Jaw"),
    Tongue          UMETA(DisplayName = "Tongue"),
    Neck            UMETA(DisplayName = "Neck"),
    Other           UMETA(DisplayName = "Other")
};

/**
 * Laterality for symmetric body parts
 */
UENUM(BlueprintType)
enum class ELaterality : uint8
{
    Center          UMETA(DisplayName = "Center"),
    Left            UMETA(DisplayName = "Left"),
    Right           UMETA(DisplayName = "Right"),
    Bilateral       UMETA(DisplayName = "Bilateral")
};

// ========================================
// DNA BINDING STRUCTURES
// ========================================

/**
 * Joint binding between DNA definition and skeleton/body schema
 */
USTRUCT(BlueprintType)
struct FDNAJointBinding
{
    GENERATED_BODY()

    /** DNA joint index */
    UPROPERTY(BlueprintReadWrite)
    int32 DNAJointIndex = -1;

    /** DNA joint name (e.g., "spine_01", "hand_l") */
    UPROPERTY(BlueprintReadWrite)
    FString DNAJointName;

    /** Parent joint index in DNA hierarchy */
    UPROPERTY(BlueprintReadWrite)
    int32 DNAParentIndex = -1;

    /** Skeleton bone name in UE */
    UPROPERTY(BlueprintReadWrite)
    FName SkeletonBoneName;

    /** Skeleton bone index */
    UPROPERTY(BlueprintReadWrite)
    int32 SkeletonBoneIndex = -1;

    /** Body schema part identifier */
    UPROPERTY(BlueprintReadWrite)
    FString BodySchemaPart;

    /** Body region classification */
    UPROPERTY(BlueprintReadWrite)
    EBodyRegion BodyRegion = EBodyRegion::Unknown;

    /** Laterality */
    UPROPERTY(BlueprintReadWrite)
    ELaterality Laterality = ELaterality::Center;

    /** Associated motor effector types */
    UPROPERTY(BlueprintReadWrite)
    TArray<EMotorEffectorType> MotorEffectors;

    /** Joint rotation limits (min) */
    UPROPERTY(BlueprintReadWrite)
    FRotator RotationMin = FRotator(-180.0f, -180.0f, -180.0f);

    /** Joint rotation limits (max) */
    UPROPERTY(BlueprintReadWrite)
    FRotator RotationMax = FRotator(180.0f, 180.0f, 180.0f);

    /** Joint is critical for body schema coherence */
    UPROPERTY(BlueprintReadWrite)
    bool bIsCriticalJoint = false;

    /** Proprioceptive weight (importance in body awareness) */
    UPROPERTY(BlueprintReadWrite)
    float ProprioceptiveWeight = 1.0f;
};

/**
 * Blend shape binding between DNA and facial expression system
 */
USTRUCT(BlueprintType)
struct FDNABlendShapeBinding
{
    GENERATED_BODY()

    /** DNA blend shape channel index */
    UPROPERTY(BlueprintReadWrite)
    int32 DNAChannelIndex = -1;

    /** DNA channel name (e.g., "brow_lateral_L", "smile") */
    UPROPERTY(BlueprintReadWrite)
    FString DNAChannelName;

    /** Morph target name in UE skeletal mesh */
    UPROPERTY(BlueprintReadWrite)
    FName MorphTargetName;

    /** Category of blend shape */
    UPROPERTY(BlueprintReadWrite)
    EBlendShapeCategory Category = EBlendShapeCategory::Other;

    /** Laterality */
    UPROPERTY(BlueprintReadWrite)
    ELaterality Laterality = ELaterality::Center;

    /** Current activation value (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CurrentValue = 0.0f;

    /** Baseline value (neutral expression) */
    UPROPERTY(BlueprintReadWrite)
    float BaselineValue = 0.0f;

    /** Emotional valence influence (-1 to 1) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    /** Emotional arousal influence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalArousal = 0.0f;

    /** Is part of primary expression set */
    UPROPERTY(BlueprintReadWrite)
    bool bIsPrimaryExpression = false;
};

/**
 * Proprioceptive state for a body part
 */
USTRUCT(BlueprintType)
struct FProprioceptiveState
{
    GENERATED_BODY()

    /** Body part identifier */
    UPROPERTY(BlueprintReadWrite)
    FString BodyPart;

    /** Current world position */
    UPROPERTY(BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    /** Current orientation */
    UPROPERTY(BlueprintReadWrite)
    FRotator Orientation = FRotator::ZeroRotator;

    /** Angular velocity */
    UPROPERTY(BlueprintReadWrite)
    FVector AngularVelocity = FVector::ZeroVector;

    /** Linear velocity */
    UPROPERTY(BlueprintReadWrite)
    FVector LinearVelocity = FVector::ZeroVector;

    /** Muscle tension (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float MuscleTension = 0.0f;

    /** Fatigue level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Fatigue = 0.0f;

    /** Is in contact with environment */
    UPROPERTY(BlueprintReadWrite)
    bool bIsInContact = false;

    /** Contact force magnitude */
    UPROPERTY(BlueprintReadWrite)
    float ContactForce = 0.0f;
};

/**
 * Body measurements derived from DNA
 */
USTRUCT(BlueprintType)
struct FDNABodyMeasurements
{
    GENERATED_BODY()

    /** Total height estimate */
    UPROPERTY(BlueprintReadWrite)
    float Height = 175.0f;

    /** Arm span */
    UPROPERTY(BlueprintReadWrite)
    float ArmSpan = 175.0f;

    /** Shoulder width */
    UPROPERTY(BlueprintReadWrite)
    float ShoulderWidth = 45.0f;

    /** Hip width */
    UPROPERTY(BlueprintReadWrite)
    float HipWidth = 35.0f;

    /** Head circumference */
    UPROPERTY(BlueprintReadWrite)
    float HeadCircumference = 56.0f;

    /** Reach distance (computed from arm span) */
    UPROPERTY(BlueprintReadWrite)
    float ReachDistance = 80.0f;

    /** Step length estimate */
    UPROPERTY(BlueprintReadWrite)
    float StepLength = 70.0f;

    /** Body mass estimate (kg) */
    UPROPERTY(BlueprintReadWrite)
    float BodyMass = 70.0f;

    /** Center of mass offset from root */
    UPROPERTY(BlueprintReadWrite)
    FVector CenterOfMass = FVector(0.0f, 0.0f, 100.0f);
};

/**
 * Binding synchronization state
 */
USTRUCT(BlueprintType)
struct FBindingSyncState
{
    GENERATED_BODY()

    /** Is DNA loaded */
    UPROPERTY(BlueprintReadOnly)
    bool bDNALoaded = false;

    /** Is skeleton bound */
    UPROPERTY(BlueprintReadOnly)
    bool bSkeletonBound = false;

    /** Is body schema synchronized */
    UPROPERTY(BlueprintReadOnly)
    bool bBodySchemaSynced = false;

    /** Last sync timestamp */
    UPROPERTY(BlueprintReadOnly)
    float LastSyncTime = 0.0f;

    /** Number of bound joints */
    UPROPERTY(BlueprintReadOnly)
    int32 BoundJointCount = 0;

    /** Number of bound blend shapes */
    UPROPERTY(BlueprintReadOnly)
    int32 BoundBlendShapeCount = 0;

    /** Binding quality score (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float BindingQuality = 0.0f;
};

// ========================================
// DELEGATE DECLARATIONS
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDNALoaded, int32, JointCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBindingComplete, float, BindingQuality);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBodySchemaUpdated, FString, BodyPart, FVector, NewPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProprioceptiveChange, FString, BodyPart, FProprioceptiveState, State);

/**
 * DNABodySchemaBinding
 *
 * Bridges MetaHuman DNA calibration system with 4E embodied cognition body schema.
 * Provides dynamic mapping between:
 * - DNA joint definitions → Skeleton bones → Body schema parts
 * - DNA blend shapes → Morph targets → Facial expression system
 * - DNA body measurements → Peripersonal space → Motor capabilities
 *
 * Supports:
 * - Automatic joint/blend shape discovery from DNA files
 * - Hierarchical body part organization
 * - Proprioceptive state tracking
 * - Real-time synchronization with skeletal animation
 * - Integration with reservoir computing for movement patterns
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDNABodySchemaBinding : public UActorComponent
{
    GENERATED_BODY()

public:
    UDNABodySchemaBinding();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Path to DNA file (relative to content directory) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA Binding|Config")
    FString DNAFilePath;

    /** Auto-load DNA on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA Binding|Config")
    bool bAutoLoadDNA = false;

    /** Auto-bind to skeletal mesh on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA Binding|Config")
    bool bAutoBindSkeleton = true;

    /** Sync rate (times per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA Binding|Config", meta = (ClampMin = "1", ClampMax = "120"))
    float SyncRate = 30.0f;

    /** Enable proprioceptive tracking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA Binding|Config")
    bool bEnableProprioceptiveTracking = true;

    /** Enable velocity computation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA Binding|Config")
    bool bComputeVelocities = true;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "DNA Binding|Events")
    FOnDNALoaded OnDNALoaded;

    UPROPERTY(BlueprintAssignable, Category = "DNA Binding|Events")
    FOnBindingComplete OnBindingComplete;

    UPROPERTY(BlueprintAssignable, Category = "DNA Binding|Events")
    FOnBodySchemaUpdated OnBodySchemaUpdated;

    UPROPERTY(BlueprintAssignable, Category = "DNA Binding|Events")
    FOnProprioceptiveChange OnProprioceptiveChange;

    // ========================================
    // DNA LOADING
    // ========================================

    /**
     * Load DNA from file
     * @param FilePath Path to .dna file
     * @return True if loaded successfully
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Loading")
    bool LoadDNA(const FString& FilePath);

    /**
     * Load DNA from binary data
     * @param DNAData Raw DNA binary data
     * @return True if loaded successfully
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Loading")
    bool LoadDNAFromMemory(const TArray<uint8>& DNAData);

    /**
     * Check if DNA is loaded
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Loading")
    bool IsDNALoaded() const;

    /**
     * Get DNA joint count
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Loading")
    int32 GetDNAJointCount() const;

    /**
     * Get DNA blend shape count
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Loading")
    int32 GetDNABlendShapeCount() const;

    // ========================================
    // SKELETON BINDING
    // ========================================

    /**
     * Bind to skeletal mesh component
     * @param SkeletalMesh The skeletal mesh to bind to
     * @return True if binding successful
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Skeleton")
    bool BindToSkeleton(USkeletalMeshComponent* SkeletalMesh);

    /**
     * Unbind from current skeleton
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Skeleton")
    void UnbindSkeleton();

    /**
     * Get joint binding for DNA joint name
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Skeleton")
    FDNAJointBinding GetJointBinding(const FString& DNAJointName) const;

    /**
     * Get all joint bindings
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Skeleton")
    TArray<FDNAJointBinding> GetAllJointBindings() const;

    /**
     * Get blend shape binding
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Skeleton")
    FDNABlendShapeBinding GetBlendShapeBinding(const FString& DNAChannelName) const;

    /**
     * Get all blend shape bindings
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Skeleton")
    TArray<FDNABlendShapeBinding> GetAllBlendShapeBindings() const;

    // ========================================
    // BODY SCHEMA SYNCHRONIZATION
    // ========================================

    /**
     * Sync DNA/skeleton state to body schema
     * @param BodySchemaComponent The 4E embodied cognition component to update
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Sync")
    void SyncToBodySchema(UEmbodiedCognitionComponent* BodySchemaComponent);

    /**
     * Sync body schema commands back to skeleton
     * @param BodySchemaComponent Source of body schema commands
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Sync")
    void SyncFromBodySchema(UEmbodiedCognitionComponent* BodySchemaComponent);

    /**
     * Get current sync state
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Sync")
    FBindingSyncState GetSyncState() const;

    /**
     * Force full resynchronization
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Sync")
    void ForceResync();

    // ========================================
    // PROPRIOCEPTIVE STATE
    // ========================================

    /**
     * Get proprioceptive state for body part
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Proprioception")
    FProprioceptiveState GetProprioceptiveState(const FString& BodyPart) const;

    /**
     * Get all proprioceptive states
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Proprioception")
    TMap<FString, FProprioceptiveState> GetAllProprioceptiveStates() const;

    /**
     * Get proprioceptive vector (flattened state for reservoir)
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Proprioception")
    TArray<float> GetProprioceptiveVector() const;

    /**
     * Set muscle tension for body part
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Proprioception")
    void SetMuscleTension(const FString& BodyPart, float Tension);

    /**
     * Set fatigue for body part
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Proprioception")
    void SetFatigue(const FString& BodyPart, float Fatigue);

    // ========================================
    // BODY MEASUREMENTS
    // ========================================

    /**
     * Get body measurements from DNA
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Measurements")
    FDNABodyMeasurements GetBodyMeasurements() const;

    /**
     * Compute peripersonal space radius from body measurements
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Measurements")
    float ComputePeripersonalRadius() const;

    /**
     * Get reach capability for direction
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Measurements")
    float GetReachCapability(const FVector& Direction) const;

    // ========================================
    // EXPRESSION BINDING
    // ========================================

    /**
     * Set blend shape value
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Expression")
    void SetBlendShapeValue(const FString& ChannelName, float Value);

    /**
     * Get blend shape value
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Expression")
    float GetBlendShapeValue(const FString& ChannelName) const;

    /**
     * Apply emotional state to blend shapes
     * @param Valence Emotional valence (-1 to 1)
     * @param Arousal Emotional arousal (0 to 1)
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Expression")
    void ApplyEmotionalState(float Valence, float Arousal);

    /**
     * Get blend shapes by category
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Expression")
    TArray<FDNABlendShapeBinding> GetBlendShapesByCategory(EBlendShapeCategory Category) const;

    // ========================================
    // MOTOR CAPABILITIES
    // ========================================

    /**
     * Get motor capability score for effector type
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Motor")
    float GetMotorCapability(EMotorEffectorType EffectorType) const;

    /**
     * Get joints for effector type
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Motor")
    TArray<FString> GetJointsForEffector(EMotorEffectorType EffectorType) const;

    /**
     * Check if movement is within joint limits
     */
    UFUNCTION(BlueprintPure, Category = "DNA Binding|Motor")
    bool IsMovementWithinLimits(const FString& JointName, const FRotator& TargetRotation) const;

    // ========================================
    // RESERVOIR INTEGRATION
    // ========================================

    /**
     * Encode current body state through reservoir
     * @param StreamID Reservoir stream to use
     * @return Reservoir-processed embedding
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Reservoir")
    TArray<float> EncodeBodyState(int32 StreamID = 1);

    /**
     * Get movement pattern embedding
     * @param DurationSeconds Duration of movement to encode
     * @return Movement pattern embedding
     */
    UFUNCTION(BlueprintCallable, Category = "DNA Binding|Reservoir")
    TArray<float> GetMovementPatternEmbedding(float DurationSeconds = 1.0f);

protected:
    // Component references
    UPROPERTY()
    USkeletalMeshComponent* BoundSkeletalMesh;

    UPROPERTY()
    UEmbodiedCognitionComponent* LinkedBodySchema;

    UPROPERTY()
    UDeepTreeEchoReservoir* ReservoirComponent;

    // DNA data (simulated - actual DNA SDK not directly linked)
    TArray<FString> DNAJointNames;
    TArray<int32> DNAJointParents;
    TArray<FString> DNABlendShapeNames;

    // Binding maps
    TMap<FString, FDNAJointBinding> JointBindings;
    TMap<FString, FDNABlendShapeBinding> BlendShapeBindings;

    // Proprioceptive state
    TMap<FString, FProprioceptiveState> ProprioceptiveStates;
    TMap<FString, FVector> PreviousPositions;
    TMap<FString, FRotator> PreviousOrientations;

    // Body measurements
    FDNABodyMeasurements BodyMeasurements;

    // Sync state
    FBindingSyncState SyncState;
    float LastSyncTimestamp = 0.0f;
    float AccumulatedTime = 0.0f;

    // Movement history for pattern encoding
    TArray<TArray<float>> MovementHistory;
    int32 MaxMovementHistoryFrames = 60;

    // Internal methods
    void FindComponentReferences();
    void InitializeDefaultJointBindings();
    void InitializeDefaultBlendShapeBindings();
    void BuildJointHierarchy();
    void ComputeBodyMeasurements();
    void UpdateProprioceptiveStates(float DeltaTime);
    void RecordMovementFrame();

    EBodyRegion ClassifyJointToRegion(const FString& JointName) const;
    ELaterality DetermineLaterality(const FString& JointName) const;
    EBlendShapeCategory ClassifyBlendShape(const FString& ChannelName) const;
    TArray<EMotorEffectorType> DetermineMotorEffectors(const FString& JointName) const;
    FString MapJointToBodySchemaPart(const FString& JointName) const;
};

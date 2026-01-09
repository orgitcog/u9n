// DNABodySchemaBinding.cpp
// Implementation of MetaHuman DNA to body schema binding for 4E embodied cognition

#include "DNABodySchemaBinding.h"
#include "EmbodiedCognitionComponent.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

UDNABodySchemaBinding::UDNABodySchemaBinding()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDNABodySchemaBinding::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();

    // Auto-load DNA if configured
    if (bAutoLoadDNA && !DNAFilePath.IsEmpty())
    {
        LoadDNA(DNAFilePath);
    }
    else
    {
        // Initialize with default MetaHuman joint/blend shape structure
        InitializeDefaultJointBindings();
        InitializeDefaultBlendShapeBindings();
    }

    // Auto-bind to skeleton if configured
    if (bAutoBindSkeleton && BoundSkeletalMesh == nullptr)
    {
        // Find skeletal mesh on owner
        if (AActor* Owner = GetOwner())
        {
            if (USkeletalMeshComponent* SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
            {
                BindToSkeleton(SkeletalMesh);
            }
        }
    }
}

void UDNABodySchemaBinding::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!SyncState.bSkeletonBound)
    {
        return;
    }

    AccumulatedTime += DeltaTime;
    float SyncInterval = 1.0f / SyncRate;

    if (AccumulatedTime >= SyncInterval)
    {
        AccumulatedTime -= SyncInterval;

        // Update proprioceptive states
        if (bEnableProprioceptiveTracking)
        {
            UpdateProprioceptiveStates(SyncInterval);
        }

        // Record movement frame for pattern encoding
        RecordMovementFrame();

        // Sync to body schema if linked
        if (LinkedBodySchema)
        {
            SyncToBodySchema(LinkedBodySchema);
        }

        SyncState.LastSyncTime = GetWorld()->GetTimeSeconds();
    }
}

// ========================================
// COMPONENT DISCOVERY
// ========================================

void UDNABodySchemaBinding::FindComponentReferences()
{
    if (AActor* Owner = GetOwner())
    {
        // Find skeletal mesh
        if (BoundSkeletalMesh == nullptr)
        {
            BoundSkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
        }

        // Find embodied cognition component
        LinkedBodySchema = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();

        // Find reservoir component
        ReservoirComponent = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    }
}

// ========================================
// DNA LOADING
// ========================================

bool UDNABodySchemaBinding::LoadDNA(const FString& FilePath)
{
    // Note: This is a simulation of DNA loading. In production, this would
    // integrate with the actual DNA SDK from MetaHuman-DNA-Calibration.
    // The DNA binary format is proprietary and requires the DNACalib library.

    FString FullPath = FPaths::ProjectContentDir() / FilePath;

    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("DNABodySchemaBinding: DNA file not found: %s"), *FullPath);
        // Fall back to default bindings
        InitializeDefaultJointBindings();
        InitializeDefaultBlendShapeBindings();
        return false;
    }

    TArray<uint8> DNAData;
    if (!FFileHelper::LoadFileToArray(DNAData, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("DNABodySchemaBinding: Failed to load DNA file: %s"), *FullPath);
        return false;
    }

    return LoadDNAFromMemory(DNAData);
}

bool UDNABodySchemaBinding::LoadDNAFromMemory(const TArray<uint8>& DNAData)
{
    // DNA file format validation (simplified)
    // Actual DNA files start with signature "DNA" followed by version info

    if (DNAData.Num() < 4)
    {
        UE_LOG(LogTemp, Error, TEXT("DNABodySchemaBinding: Invalid DNA data (too small)"));
        return false;
    }

    // Check for DNA signature (first 3 bytes should be 'D', 'N', 'A')
    if (DNAData[0] == 'D' && DNAData[1] == 'N' && DNAData[2] == 'A')
    {
        UE_LOG(LogTemp, Log, TEXT("DNABodySchemaBinding: Valid DNA signature detected"));
        // In production: Parse DNA binary format using DNACalib library
    }

    // For simulation, initialize with MetaHuman-compatible joint structure
    InitializeDefaultJointBindings();
    InitializeDefaultBlendShapeBindings();

    SyncState.bDNALoaded = true;

    // Compute body measurements from DNA
    ComputeBodyMeasurements();

    // Build joint hierarchy
    BuildJointHierarchy();

    // Broadcast event
    OnDNALoaded.Broadcast(DNAJointNames.Num());

    UE_LOG(LogTemp, Log, TEXT("DNABodySchemaBinding: DNA loaded with %d joints, %d blend shapes"),
           DNAJointNames.Num(), DNABlendShapeNames.Num());

    return true;
}

bool UDNABodySchemaBinding::IsDNALoaded() const
{
    return SyncState.bDNALoaded;
}

int32 UDNABodySchemaBinding::GetDNAJointCount() const
{
    return DNAJointNames.Num();
}

int32 UDNABodySchemaBinding::GetDNABlendShapeCount() const
{
    return DNABlendShapeNames.Num();
}

// ========================================
// DEFAULT BINDINGS (METAHUMAN-COMPATIBLE)
// ========================================

void UDNABodySchemaBinding::InitializeDefaultJointBindings()
{
    DNAJointNames.Empty();
    DNAJointParents.Empty();
    JointBindings.Empty();

    // MetaHuman skeleton joint hierarchy (simplified)
    // Actual MetaHuman has 600+ joints; this covers primary body joints

    struct FJointDef
    {
        FString Name;
        int32 ParentIdx;
    };

    TArray<FJointDef> JointDefs = {
        // Root and Spine
        { TEXT("root"), -1 },                    // 0
        { TEXT("pelvis"), 0 },                   // 1
        { TEXT("spine_01"), 1 },                 // 2
        { TEXT("spine_02"), 2 },                 // 3
        { TEXT("spine_03"), 3 },                 // 4
        { TEXT("spine_04"), 4 },                 // 5
        { TEXT("spine_05"), 5 },                 // 6

        // Neck and Head
        { TEXT("neck_01"), 6 },                  // 7
        { TEXT("neck_02"), 7 },                  // 8
        { TEXT("head"), 8 },                     // 9

        // Left Arm
        { TEXT("clavicle_l"), 6 },               // 10
        { TEXT("upperarm_l"), 10 },              // 11
        { TEXT("lowerarm_l"), 11 },              // 12
        { TEXT("hand_l"), 12 },                  // 13

        // Left Hand Fingers
        { TEXT("thumb_01_l"), 13 },              // 14
        { TEXT("thumb_02_l"), 14 },              // 15
        { TEXT("thumb_03_l"), 15 },              // 16
        { TEXT("index_01_l"), 13 },              // 17
        { TEXT("index_02_l"), 17 },              // 18
        { TEXT("index_03_l"), 18 },              // 19
        { TEXT("middle_01_l"), 13 },             // 20
        { TEXT("middle_02_l"), 20 },             // 21
        { TEXT("middle_03_l"), 21 },             // 22
        { TEXT("ring_01_l"), 13 },               // 23
        { TEXT("ring_02_l"), 23 },               // 24
        { TEXT("ring_03_l"), 24 },               // 25
        { TEXT("pinky_01_l"), 13 },              // 26
        { TEXT("pinky_02_l"), 26 },              // 27
        { TEXT("pinky_03_l"), 27 },              // 28

        // Right Arm
        { TEXT("clavicle_r"), 6 },               // 29
        { TEXT("upperarm_r"), 29 },              // 30
        { TEXT("lowerarm_r"), 30 },              // 31
        { TEXT("hand_r"), 31 },                  // 32

        // Right Hand Fingers
        { TEXT("thumb_01_r"), 32 },              // 33
        { TEXT("thumb_02_r"), 33 },              // 34
        { TEXT("thumb_03_r"), 34 },              // 35
        { TEXT("index_01_r"), 32 },              // 36
        { TEXT("index_02_r"), 36 },              // 37
        { TEXT("index_03_r"), 37 },              // 38
        { TEXT("middle_01_r"), 32 },             // 39
        { TEXT("middle_02_r"), 39 },             // 40
        { TEXT("middle_03_r"), 40 },             // 41
        { TEXT("ring_01_r"), 32 },               // 42
        { TEXT("ring_02_r"), 42 },               // 43
        { TEXT("ring_03_r"), 43 },               // 44
        { TEXT("pinky_01_r"), 32 },              // 45
        { TEXT("pinky_02_r"), 45 },              // 46
        { TEXT("pinky_03_r"), 46 },              // 47

        // Left Leg
        { TEXT("thigh_l"), 1 },                  // 48
        { TEXT("calf_l"), 48 },                  // 49
        { TEXT("foot_l"), 49 },                  // 50
        { TEXT("ball_l"), 50 },                  // 51

        // Right Leg
        { TEXT("thigh_r"), 1 },                  // 52
        { TEXT("calf_r"), 52 },                  // 53
        { TEXT("foot_r"), 53 },                  // 54
        { TEXT("ball_r"), 54 },                  // 55

        // Facial root
        { TEXT("FACIAL_C_FacialRoot"), 9 },      // 56
    };

    // Build joint arrays and bindings
    for (int32 i = 0; i < JointDefs.Num(); i++)
    {
        const FJointDef& Def = JointDefs[i];
        DNAJointNames.Add(Def.Name);
        DNAJointParents.Add(Def.ParentIdx);

        FDNAJointBinding Binding;
        Binding.DNAJointIndex = i;
        Binding.DNAJointName = Def.Name;
        Binding.DNAParentIndex = Def.ParentIdx;
        Binding.SkeletonBoneName = FName(*Def.Name);
        Binding.SkeletonBoneIndex = i;
        Binding.BodySchemaPart = MapJointToBodySchemaPart(Def.Name);
        Binding.BodyRegion = ClassifyJointToRegion(Def.Name);
        Binding.Laterality = DetermineLaterality(Def.Name);
        Binding.MotorEffectors = DetermineMotorEffectors(Def.Name);

        // Set critical joints (important for body schema coherence)
        Binding.bIsCriticalJoint = (Def.Name == TEXT("pelvis") ||
                                    Def.Name == TEXT("spine_03") ||
                                    Def.Name == TEXT("neck_01") ||
                                    Def.Name == TEXT("head") ||
                                    Def.Name == TEXT("hand_l") ||
                                    Def.Name == TEXT("hand_r"));

        // Set proprioceptive weights (higher for end effectors and core)
        if (Binding.bIsCriticalJoint)
        {
            Binding.ProprioceptiveWeight = 1.5f;
        }
        else if (Def.Name.Contains(TEXT("hand")) || Def.Name.Contains(TEXT("foot")))
        {
            Binding.ProprioceptiveWeight = 1.3f;
        }
        else if (Def.Name.Contains(TEXT("finger")) || Def.Name.Contains(TEXT("thumb")) ||
                 Def.Name.Contains(TEXT("index")) || Def.Name.Contains(TEXT("middle")) ||
                 Def.Name.Contains(TEXT("ring")) || Def.Name.Contains(TEXT("pinky")))
        {
            Binding.ProprioceptiveWeight = 1.2f;
        }

        // Set rotation limits based on joint type
        SetDefaultRotationLimits(Binding);

        JointBindings.Add(Def.Name, Binding);

        // Initialize proprioceptive state
        FProprioceptiveState PropState;
        PropState.BodyPart = Binding.BodySchemaPart;
        ProprioceptiveStates.Add(Def.Name, PropState);
    }
}

void UDNABodySchemaBinding::InitializeDefaultBlendShapeBindings()
{
    DNABlendShapeNames.Empty();
    BlendShapeBindings.Empty();

    // MetaHuman FACS-based blend shapes (subset of ~300+ total)
    struct FBlendShapeDef
    {
        FString Name;
        EBlendShapeCategory Category;
        ELaterality Laterality;
        float Valence;
        float Arousal;
        bool bPrimary;
    };

    TArray<FBlendShapeDef> BlendShapeDefs = {
        // Brow expressions
        { TEXT("brow_down_L"), EBlendShapeCategory::Brow, ELaterality::Left, -0.3f, 0.4f, true },
        { TEXT("brow_down_R"), EBlendShapeCategory::Brow, ELaterality::Right, -0.3f, 0.4f, true },
        { TEXT("brow_inner_up_L"), EBlendShapeCategory::Brow, ELaterality::Left, -0.2f, 0.3f, true },
        { TEXT("brow_inner_up_R"), EBlendShapeCategory::Brow, ELaterality::Right, -0.2f, 0.3f, true },
        { TEXT("brow_outer_up_L"), EBlendShapeCategory::Brow, ELaterality::Left, 0.1f, 0.2f, true },
        { TEXT("brow_outer_up_R"), EBlendShapeCategory::Brow, ELaterality::Right, 0.1f, 0.2f, true },
        { TEXT("brow_lateral_L"), EBlendShapeCategory::Brow, ELaterality::Left, 0.0f, 0.2f, false },
        { TEXT("brow_lateral_R"), EBlendShapeCategory::Brow, ELaterality::Right, 0.0f, 0.2f, false },

        // Eye expressions
        { TEXT("eye_blink_L"), EBlendShapeCategory::Eye, ELaterality::Left, 0.0f, 0.0f, true },
        { TEXT("eye_blink_R"), EBlendShapeCategory::Eye, ELaterality::Right, 0.0f, 0.0f, true },
        { TEXT("eye_wide_L"), EBlendShapeCategory::Eye, ELaterality::Left, -0.2f, 0.6f, true },
        { TEXT("eye_wide_R"), EBlendShapeCategory::Eye, ELaterality::Right, -0.2f, 0.6f, true },
        { TEXT("eye_squint_L"), EBlendShapeCategory::Eye, ELaterality::Left, 0.3f, 0.2f, true },
        { TEXT("eye_squint_R"), EBlendShapeCategory::Eye, ELaterality::Right, 0.3f, 0.2f, true },
        { TEXT("eye_look_up_L"), EBlendShapeCategory::Eye, ELaterality::Left, 0.0f, 0.0f, false },
        { TEXT("eye_look_up_R"), EBlendShapeCategory::Eye, ELaterality::Right, 0.0f, 0.0f, false },
        { TEXT("eye_look_down_L"), EBlendShapeCategory::Eye, ELaterality::Left, 0.0f, 0.0f, false },
        { TEXT("eye_look_down_R"), EBlendShapeCategory::Eye, ELaterality::Right, 0.0f, 0.0f, false },

        // Nose expressions
        { TEXT("nose_sneer_L"), EBlendShapeCategory::Nose, ELaterality::Left, -0.4f, 0.3f, true },
        { TEXT("nose_sneer_R"), EBlendShapeCategory::Nose, ELaterality::Right, -0.4f, 0.3f, true },
        { TEXT("nose_wrinkle"), EBlendShapeCategory::Nose, ELaterality::Center, -0.3f, 0.4f, false },

        // Cheek expressions
        { TEXT("cheek_puff_L"), EBlendShapeCategory::Cheek, ELaterality::Left, 0.0f, 0.1f, false },
        { TEXT("cheek_puff_R"), EBlendShapeCategory::Cheek, ELaterality::Right, 0.0f, 0.1f, false },
        { TEXT("cheek_squint_L"), EBlendShapeCategory::Cheek, ELaterality::Left, 0.4f, 0.3f, true },
        { TEXT("cheek_squint_R"), EBlendShapeCategory::Cheek, ELaterality::Right, 0.4f, 0.3f, true },

        // Mouth expressions
        { TEXT("mouth_smile_L"), EBlendShapeCategory::Mouth, ELaterality::Left, 0.7f, 0.4f, true },
        { TEXT("mouth_smile_R"), EBlendShapeCategory::Mouth, ELaterality::Right, 0.7f, 0.4f, true },
        { TEXT("mouth_frown_L"), EBlendShapeCategory::Mouth, ELaterality::Left, -0.6f, 0.2f, true },
        { TEXT("mouth_frown_R"), EBlendShapeCategory::Mouth, ELaterality::Right, -0.6f, 0.2f, true },
        { TEXT("mouth_open"), EBlendShapeCategory::Mouth, ELaterality::Center, 0.0f, 0.5f, true },
        { TEXT("mouth_pucker"), EBlendShapeCategory::Mouth, ELaterality::Center, 0.0f, 0.1f, true },
        { TEXT("mouth_stretch_L"), EBlendShapeCategory::Mouth, ELaterality::Left, -0.4f, 0.6f, true },
        { TEXT("mouth_stretch_R"), EBlendShapeCategory::Mouth, ELaterality::Right, -0.4f, 0.6f, true },
        { TEXT("mouth_press_L"), EBlendShapeCategory::Mouth, ELaterality::Left, -0.1f, 0.2f, false },
        { TEXT("mouth_press_R"), EBlendShapeCategory::Mouth, ELaterality::Right, -0.1f, 0.2f, false },
        { TEXT("mouth_dimple_L"), EBlendShapeCategory::Mouth, ELaterality::Left, 0.2f, 0.1f, false },
        { TEXT("mouth_dimple_R"), EBlendShapeCategory::Mouth, ELaterality::Right, 0.2f, 0.1f, false },
        { TEXT("mouth_upper_up_L"), EBlendShapeCategory::Mouth, ELaterality::Left, -0.3f, 0.3f, true },
        { TEXT("mouth_upper_up_R"), EBlendShapeCategory::Mouth, ELaterality::Right, -0.3f, 0.3f, true },
        { TEXT("mouth_lower_down_L"), EBlendShapeCategory::Mouth, ELaterality::Left, -0.2f, 0.4f, true },
        { TEXT("mouth_lower_down_R"), EBlendShapeCategory::Mouth, ELaterality::Right, -0.2f, 0.4f, true },

        // Jaw expressions
        { TEXT("jaw_open"), EBlendShapeCategory::Jaw, ELaterality::Center, 0.0f, 0.5f, true },
        { TEXT("jaw_forward"), EBlendShapeCategory::Jaw, ELaterality::Center, -0.2f, 0.3f, false },
        { TEXT("jaw_left"), EBlendShapeCategory::Jaw, ELaterality::Left, 0.0f, 0.1f, false },
        { TEXT("jaw_right"), EBlendShapeCategory::Jaw, ELaterality::Right, 0.0f, 0.1f, false },

        // Tongue expressions
        { TEXT("tongue_out"), EBlendShapeCategory::Tongue, ELaterality::Center, 0.1f, 0.3f, false },
        { TEXT("tongue_up"), EBlendShapeCategory::Tongue, ELaterality::Center, 0.0f, 0.0f, false },
        { TEXT("tongue_down"), EBlendShapeCategory::Tongue, ELaterality::Center, 0.0f, 0.0f, false },
    };

    // Build blend shape bindings
    for (int32 i = 0; i < BlendShapeDefs.Num(); i++)
    {
        const FBlendShapeDef& Def = BlendShapeDefs[i];
        DNABlendShapeNames.Add(Def.Name);

        FDNABlendShapeBinding Binding;
        Binding.DNAChannelIndex = i;
        Binding.DNAChannelName = Def.Name;
        Binding.MorphTargetName = FName(*Def.Name);
        Binding.Category = Def.Category;
        Binding.Laterality = Def.Laterality;
        Binding.CurrentValue = 0.0f;
        Binding.BaselineValue = 0.0f;
        Binding.EmotionalValence = Def.Valence;
        Binding.EmotionalArousal = Def.Arousal;
        Binding.bIsPrimaryExpression = Def.bPrimary;

        BlendShapeBindings.Add(Def.Name, Binding);
    }
}

void UDNABodySchemaBinding::BuildJointHierarchy()
{
    // Joint hierarchy is already established during binding initialization
    // This method can be extended for additional hierarchy analysis
}

void UDNABodySchemaBinding::ComputeBodyMeasurements()
{
    // Default body measurements (can be refined from DNA scale data)
    BodyMeasurements.Height = 175.0f;
    BodyMeasurements.ArmSpan = 175.0f;
    BodyMeasurements.ShoulderWidth = 45.0f;
    BodyMeasurements.HipWidth = 35.0f;
    BodyMeasurements.HeadCircumference = 56.0f;
    BodyMeasurements.ReachDistance = BodyMeasurements.ArmSpan * 0.45f;
    BodyMeasurements.StepLength = BodyMeasurements.Height * 0.4f;
    BodyMeasurements.BodyMass = 70.0f;
    BodyMeasurements.CenterOfMass = FVector(0.0f, 0.0f, BodyMeasurements.Height * 0.55f);

    // If skeleton is bound, compute measurements from bone positions
    if (BoundSkeletalMesh && BoundSkeletalMesh->SkeletalMesh)
    {
        const FReferenceSkeleton& RefSkel = BoundSkeletalMesh->SkeletalMesh->GetRefSkeleton();

        // Get bone positions for measurement
        int32 HeadIdx = RefSkel.FindBoneIndex(TEXT("head"));
        int32 PelvisIdx = RefSkel.FindBoneIndex(TEXT("pelvis"));
        int32 HandLIdx = RefSkel.FindBoneIndex(TEXT("hand_l"));
        int32 HandRIdx = RefSkel.FindBoneIndex(TEXT("hand_r"));
        int32 FootLIdx = RefSkel.FindBoneIndex(TEXT("foot_l"));

        if (HeadIdx != INDEX_NONE && PelvisIdx != INDEX_NONE)
        {
            FTransform HeadTransform = RefSkel.GetRefBonePose()[HeadIdx];
            FTransform PelvisTransform = RefSkel.GetRefBonePose()[PelvisIdx];

            // Approximate height from head to ground
            float TorsoHeight = (HeadTransform.GetLocation() - PelvisTransform.GetLocation()).Size();
            if (FootLIdx != INDEX_NONE)
            {
                FTransform FootTransform = RefSkel.GetRefBonePose()[FootLIdx];
                BodyMeasurements.Height = (HeadTransform.GetLocation().Z - FootTransform.GetLocation().Z) + 15.0f; // Add head height
            }
            else
            {
                BodyMeasurements.Height = TorsoHeight * 2.5f; // Estimate
            }
        }

        if (HandLIdx != INDEX_NONE && HandRIdx != INDEX_NONE)
        {
            // Compute arm span (T-pose)
            FTransform HandLTransform = RefSkel.GetRefBonePose()[HandLIdx];
            FTransform HandRTransform = RefSkel.GetRefBonePose()[HandRIdx];
            BodyMeasurements.ArmSpan = FMath::Abs(HandRTransform.GetLocation().Y - HandLTransform.GetLocation().Y);
            BodyMeasurements.ReachDistance = BodyMeasurements.ArmSpan * 0.45f;
        }

        // Update derived measurements
        BodyMeasurements.StepLength = BodyMeasurements.Height * 0.4f;
        BodyMeasurements.CenterOfMass = FVector(0.0f, 0.0f, BodyMeasurements.Height * 0.55f);
    }
}

// ========================================
// SKELETON BINDING
// ========================================

bool UDNABodySchemaBinding::BindToSkeleton(USkeletalMeshComponent* SkeletalMesh)
{
    if (!SkeletalMesh || !SkeletalMesh->SkeletalMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("DNABodySchemaBinding: Invalid skeletal mesh"));
        return false;
    }

    BoundSkeletalMesh = SkeletalMesh;

    // Match DNA joints to skeleton bones
    const FReferenceSkeleton& RefSkel = SkeletalMesh->SkeletalMesh->GetRefSkeleton();
    int32 BoundCount = 0;

    for (auto& Pair : JointBindings)
    {
        FDNAJointBinding& Binding = Pair.Value;
        int32 BoneIdx = RefSkel.FindBoneIndex(Binding.SkeletonBoneName);

        if (BoneIdx != INDEX_NONE)
        {
            Binding.SkeletonBoneIndex = BoneIdx;
            BoundCount++;
        }
        else
        {
            // Try alternative naming conventions
            FName AltName = FName(*(TEXT("") + Binding.DNAJointName.ToLower()));
            BoneIdx = RefSkel.FindBoneIndex(AltName);
            if (BoneIdx != INDEX_NONE)
            {
                Binding.SkeletonBoneName = AltName;
                Binding.SkeletonBoneIndex = BoneIdx;
                BoundCount++;
            }
        }
    }

    // Compute measurements from skeleton
    ComputeBodyMeasurements();

    // Update sync state
    SyncState.bSkeletonBound = true;
    SyncState.BoundJointCount = BoundCount;
    SyncState.BoundBlendShapeCount = BlendShapeBindings.Num();
    SyncState.BindingQuality = static_cast<float>(BoundCount) / static_cast<float>(JointBindings.Num());

    // Broadcast binding complete
    OnBindingComplete.Broadcast(SyncState.BindingQuality);

    UE_LOG(LogTemp, Log, TEXT("DNABodySchemaBinding: Bound %d/%d joints to skeleton (quality: %.2f)"),
           BoundCount, JointBindings.Num(), SyncState.BindingQuality);

    return BoundCount > 0;
}

void UDNABodySchemaBinding::UnbindSkeleton()
{
    BoundSkeletalMesh = nullptr;
    SyncState.bSkeletonBound = false;
    SyncState.BoundJointCount = 0;
    SyncState.BindingQuality = 0.0f;

    // Reset proprioceptive states
    for (auto& Pair : ProprioceptiveStates)
    {
        Pair.Value = FProprioceptiveState();
        Pair.Value.BodyPart = Pair.Key;
    }
}

FDNAJointBinding UDNABodySchemaBinding::GetJointBinding(const FString& DNAJointName) const
{
    if (const FDNAJointBinding* Found = JointBindings.Find(DNAJointName))
    {
        return *Found;
    }
    return FDNAJointBinding();
}

TArray<FDNAJointBinding> UDNABodySchemaBinding::GetAllJointBindings() const
{
    TArray<FDNAJointBinding> Result;
    JointBindings.GenerateValueArray(Result);
    return Result;
}

FDNABlendShapeBinding UDNABodySchemaBinding::GetBlendShapeBinding(const FString& DNAChannelName) const
{
    if (const FDNABlendShapeBinding* Found = BlendShapeBindings.Find(DNAChannelName))
    {
        return *Found;
    }
    return FDNABlendShapeBinding();
}

TArray<FDNABlendShapeBinding> UDNABodySchemaBinding::GetAllBlendShapeBindings() const
{
    TArray<FDNABlendShapeBinding> Result;
    BlendShapeBindings.GenerateValueArray(Result);
    return Result;
}

// ========================================
// BODY SCHEMA SYNCHRONIZATION
// ========================================

void UDNABodySchemaBinding::SyncToBodySchema(UEmbodiedCognitionComponent* BodySchemaComponent)
{
    if (!BodySchemaComponent || !SyncState.bSkeletonBound || !BoundSkeletalMesh)
    {
        return;
    }

    // Update body schema with current bone transforms
    for (const auto& Pair : JointBindings)
    {
        const FDNAJointBinding& Binding = Pair.Value;

        if (Binding.SkeletonBoneIndex != INDEX_NONE)
        {
            FTransform BoneTransform = BoundSkeletalMesh->GetBoneTransform(Binding.SkeletonBoneIndex);
            FVector WorldPos = BoneTransform.GetLocation();

            // Broadcast position update for significant joints
            if (Binding.bIsCriticalJoint)
            {
                OnBodySchemaUpdated.Broadcast(Binding.BodySchemaPart, WorldPos);
            }
        }
    }

    SyncState.bBodySchemaSynced = true;
}

void UDNABodySchemaBinding::SyncFromBodySchema(UEmbodiedCognitionComponent* BodySchemaComponent)
{
    if (!BodySchemaComponent || !SyncState.bSkeletonBound || !BoundSkeletalMesh)
    {
        return;
    }

    // Get body schema state
    const FBodySchema& Schema = BodySchemaComponent->BodySchema;

    // Apply motor commands from body schema to skeleton
    for (const auto& Pair : JointBindings)
    {
        const FDNAJointBinding& Binding = Pair.Value;

        if (Binding.SkeletonBoneIndex == INDEX_NONE)
        {
            continue;
        }

        // Check if body schema has target position/orientation for this part
        const FVector* TargetPosition = Schema.BodyPartPositions.Find(Binding.BodySchemaPart);
        const FRotator* TargetOrientation = Schema.BodyPartOrientations.Find(Binding.BodySchemaPart);
        const float* TargetAngle = Schema.JointAngles.Find(Binding.DNAJointName);

        // Track current state for sensorimotor feedback
        FTransform CurrentTransform = BoundSkeletalMesh->GetBoneTransform(Binding.SkeletonBoneIndex);
        FVector PredictedPosition = CurrentTransform.GetLocation();
        FRotator PredictedOrientation = CurrentTransform.GetRotation().Rotator();

        // Apply orientation commands (most common motor control)
        if (TargetOrientation)
        {
            FRotator ClampedTarget = *TargetOrientation;

            // Clamp to joint rotation limits
            ClampedTarget.Roll = FMath::Clamp(ClampedTarget.Roll, Binding.RotationMin.Roll, Binding.RotationMax.Roll);
            ClampedTarget.Pitch = FMath::Clamp(ClampedTarget.Pitch, Binding.RotationMin.Pitch, Binding.RotationMax.Pitch);
            ClampedTarget.Yaw = FMath::Clamp(ClampedTarget.Yaw, Binding.RotationMin.Yaw, Binding.RotationMax.Yaw);

            // Apply to skeleton (bone space rotation)
            // Note: In full implementation, this would interface with animation blueprint
            // For now, we update proprioceptive prediction for learning
            PredictedOrientation = ClampedTarget;
        }

        // Apply joint angle commands (for specific joints)
        if (TargetAngle)
        {
            // Convert joint angle to rotation based on joint type
            // Different joints rotate on different axes
            FRotator AngleRotation = FRotator::ZeroRotator;

            if (Binding.DNAJointName.Contains(TEXT("arm")) || Binding.DNAJointName.Contains(TEXT("calf")))
            {
                // Hinge joints typically rotate on Y (pitch)
                AngleRotation.Pitch = *TargetAngle;
            }
            else if (Binding.DNAJointName.Contains(TEXT("spine")) || Binding.DNAJointName.Contains(TEXT("neck")))
            {
                // Spine rotates on all axes but primarily yaw
                AngleRotation.Yaw = *TargetAngle * 0.5f;
                AngleRotation.Pitch = *TargetAngle * 0.3f;
            }
            else if (Binding.DNAJointName.Contains(TEXT("head")))
            {
                // Head primarily yaw/pitch
                AngleRotation.Yaw = *TargetAngle;
            }
            else
            {
                // Default to pitch for unknown joints
                AngleRotation.Pitch = *TargetAngle;
            }

            // Clamp to limits
            AngleRotation.Roll = FMath::Clamp(AngleRotation.Roll, Binding.RotationMin.Roll, Binding.RotationMax.Roll);
            AngleRotation.Pitch = FMath::Clamp(AngleRotation.Pitch, Binding.RotationMin.Pitch, Binding.RotationMax.Pitch);
            AngleRotation.Yaw = FMath::Clamp(AngleRotation.Yaw, Binding.RotationMin.Yaw, Binding.RotationMax.Yaw);

            PredictedOrientation = AngleRotation;
        }

        // Update proprioceptive state with predicted values
        if (FProprioceptiveState* PropState = ProprioceptiveStates.Find(Pair.Key))
        {
            // Compute prediction error for sensorimotor learning
            FVector ActualPosition = CurrentTransform.GetLocation();
            FRotator ActualOrientation = CurrentTransform.GetRotation().Rotator();

            float PositionError = TargetPosition ? FVector::Dist(*TargetPosition, ActualPosition) : 0.0f;
            float OrientationError = 0.0f;

            if (TargetOrientation || TargetAngle)
            {
                FRotator DeltaRot = PredictedOrientation - ActualOrientation;
                OrientationError = FMath::Abs(DeltaRot.Roll) + FMath::Abs(DeltaRot.Pitch) + FMath::Abs(DeltaRot.Yaw);
                OrientationError /= 540.0f; // Normalize to 0-1 range (max error ~540 degrees)
            }

            // Log significant prediction errors for sensorimotor learning
            float TotalError = PositionError / 100.0f + OrientationError;
            if (TotalError > 0.1f)
            {
                FString ActionString = FString::Printf(TEXT("Move_%s"), *Binding.BodySchemaPart);
                FString ExpectedString = FString::Printf(TEXT("Pos:%.1f,%.1f,%.1f Rot:%.1f,%.1f,%.1f"),
                    PredictedPosition.X, PredictedPosition.Y, PredictedPosition.Z,
                    PredictedOrientation.Roll, PredictedOrientation.Pitch, PredictedOrientation.Yaw);
                FString ActualString = FString::Printf(TEXT("Pos:%.1f,%.1f,%.1f Rot:%.1f,%.1f,%.1f"),
                    ActualPosition.X, ActualPosition.Y, ActualPosition.Z,
                    ActualOrientation.Roll, ActualOrientation.Pitch, ActualOrientation.Yaw);

                // Learn from the motor control discrepancy
                BodySchemaComponent->LearnContingency(ActionString, ExpectedString, ActualString);
            }

            // Update muscle tension based on motor effort
            if (TargetAngle || TargetOrientation)
            {
                // Motor commands require muscle activation
                float Effort = FMath::Min(TotalError + 0.1f, 1.0f);
                PropState->MuscleTension = FMath::FInterpTo(PropState->MuscleTension, Effort, GetWorld()->GetDeltaSeconds(), 5.0f);

                // Prolonged effort increases fatigue
                if (PropState->MuscleTension > 0.5f)
                {
                    PropState->Fatigue = FMath::Min(PropState->Fatigue + GetWorld()->GetDeltaSeconds() * 0.01f, 1.0f);
                }
            }
            else
            {
                // Relaxation reduces tension over time
                PropState->MuscleTension = FMath::FInterpTo(PropState->MuscleTension, 0.0f, GetWorld()->GetDeltaSeconds(), 2.0f);
            }
        }
    }

    // Apply facial expression commands from body schema
    for (auto& Pair : BlendShapeBindings)
    {
        FDNABlendShapeBinding& Binding = Pair.Value;

        // Check if body schema specifies expression for this blend shape's body part
        // Facial expressions are driven by the "Face" body part
        if (Binding.Category == EBlendShapeCategory::Mouth ||
            Binding.Category == EBlendShapeCategory::Eye ||
            Binding.Category == EBlendShapeCategory::Brow ||
            Binding.Category == EBlendShapeCategory::Nose ||
            Binding.Category == EBlendShapeCategory::Cheek ||
            Binding.Category == EBlendShapeCategory::Jaw)
        {
            // Check for facial motor commands in body schema
            const float* FacialTension = Schema.JointAngles.Find(TEXT("FACIAL_C_FacialRoot"));
            if (FacialTension)
            {
                // Apply global facial activation as baseline
                float FacialActivation = *FacialTension / 90.0f; // Normalize assuming max 90 degree range
                Binding.BaselineValue = FMath::Clamp(FacialActivation * 0.2f, 0.0f, 0.2f);
            }
        }
    }

    // Update sync state
    SyncState.bBodySchemaSynced = true;
    SyncState.LastSyncTime = GetWorld()->GetTimeSeconds();
}

FBindingSyncState UDNABodySchemaBinding::GetSyncState() const
{
    return SyncState;
}

void UDNABodySchemaBinding::ForceResync()
{
    AccumulatedTime = 1.0f / SyncRate; // Force immediate sync on next tick
    SyncState.bBodySchemaSynced = false;
}

// ========================================
// PROPRIOCEPTIVE STATE
// ========================================

void UDNABodySchemaBinding::UpdateProprioceptiveStates(float DeltaTime)
{
    if (!BoundSkeletalMesh)
    {
        return;
    }

    for (auto& Pair : JointBindings)
    {
        const FDNAJointBinding& Binding = Pair.Value;
        FProprioceptiveState& PropState = ProprioceptiveStates.FindOrAdd(Pair.Key);

        if (Binding.SkeletonBoneIndex != INDEX_NONE)
        {
            FTransform BoneTransform = BoundSkeletalMesh->GetBoneTransform(Binding.SkeletonBoneIndex);
            FVector CurrentPos = BoneTransform.GetLocation();
            FRotator CurrentRot = BoneTransform.GetRotation().Rotator();

            // Compute velocities
            if (bComputeVelocities)
            {
                if (FVector* PrevPos = PreviousPositions.Find(Pair.Key))
                {
                    PropState.LinearVelocity = (CurrentPos - *PrevPos) / DeltaTime;
                }

                if (FRotator* PrevRot = PreviousOrientations.Find(Pair.Key))
                {
                    FRotator DeltaRot = CurrentRot - *PrevRot;
                    PropState.AngularVelocity = FVector(
                        FMath::DegreesToRadians(DeltaRot.Roll) / DeltaTime,
                        FMath::DegreesToRadians(DeltaRot.Pitch) / DeltaTime,
                        FMath::DegreesToRadians(DeltaRot.Yaw) / DeltaTime
                    );
                }
            }

            // Update current state
            PropState.Position = CurrentPos;
            PropState.Orientation = CurrentRot;
            PropState.BodyPart = Binding.BodySchemaPart;

            // Store for next frame
            PreviousPositions.Add(Pair.Key, CurrentPos);
            PreviousOrientations.Add(Pair.Key, CurrentRot);

            // Check for significant proprioceptive changes
            float VelocityMagnitude = PropState.LinearVelocity.Size() + PropState.AngularVelocity.Size();
            if (VelocityMagnitude > 10.0f) // Threshold for significant movement
            {
                OnProprioceptiveChange.Broadcast(Pair.Key, PropState);
            }
        }
    }
}

FProprioceptiveState UDNABodySchemaBinding::GetProprioceptiveState(const FString& BodyPart) const
{
    // Search by body part name or joint name
    if (const FProprioceptiveState* Found = ProprioceptiveStates.Find(BodyPart))
    {
        return *Found;
    }

    // Search by body schema part
    for (const auto& Pair : ProprioceptiveStates)
    {
        if (Pair.Value.BodyPart == BodyPart)
        {
            return Pair.Value;
        }
    }

    return FProprioceptiveState();
}

TMap<FString, FProprioceptiveState> UDNABodySchemaBinding::GetAllProprioceptiveStates() const
{
    return ProprioceptiveStates;
}

TArray<float> UDNABodySchemaBinding::GetProprioceptiveVector() const
{
    TArray<float> Result;

    // Flatten proprioceptive states into vector for reservoir processing
    // Order: position (3), orientation (3), linear velocity (3), angular velocity (3), tension (1), fatigue (1)
    // = 14 floats per joint

    for (const auto& Pair : ProprioceptiveStates)
    {
        const FProprioceptiveState& State = Pair.Value;

        // Position (normalized to reasonable range)
        Result.Add(State.Position.X / 100.0f);
        Result.Add(State.Position.Y / 100.0f);
        Result.Add(State.Position.Z / 100.0f);

        // Orientation (normalized)
        Result.Add(State.Orientation.Roll / 180.0f);
        Result.Add(State.Orientation.Pitch / 180.0f);
        Result.Add(State.Orientation.Yaw / 180.0f);

        // Linear velocity (normalized)
        Result.Add(State.LinearVelocity.X / 100.0f);
        Result.Add(State.LinearVelocity.Y / 100.0f);
        Result.Add(State.LinearVelocity.Z / 100.0f);

        // Angular velocity (normalized)
        Result.Add(State.AngularVelocity.X / FMath::PI);
        Result.Add(State.AngularVelocity.Y / FMath::PI);
        Result.Add(State.AngularVelocity.Z / FMath::PI);

        // Tension and fatigue
        Result.Add(State.MuscleTension);
        Result.Add(State.Fatigue);
    }

    return Result;
}

void UDNABodySchemaBinding::SetMuscleTension(const FString& BodyPart, float Tension)
{
    if (FProprioceptiveState* State = ProprioceptiveStates.Find(BodyPart))
    {
        State->MuscleTension = FMath::Clamp(Tension, 0.0f, 1.0f);
    }
}

void UDNABodySchemaBinding::SetFatigue(const FString& BodyPart, float Fatigue)
{
    if (FProprioceptiveState* State = ProprioceptiveStates.Find(BodyPart))
    {
        State->Fatigue = FMath::Clamp(Fatigue, 0.0f, 1.0f);
    }
}

// ========================================
// BODY MEASUREMENTS
// ========================================

FDNABodyMeasurements UDNABodySchemaBinding::GetBodyMeasurements() const
{
    return BodyMeasurements;
}

float UDNABodySchemaBinding::ComputePeripersonalRadius() const
{
    // Peripersonal space extends to maximum reach
    return BodyMeasurements.ReachDistance * 1.2f; // 20% buffer
}

float UDNABodySchemaBinding::GetReachCapability(const FVector& Direction) const
{
    // Simplified reach computation based on direction
    // Forward reach is typically greater than lateral/backward

    FVector NormDir = Direction.GetSafeNormal();

    // Forward direction factor
    float ForwardFactor = FMath::Clamp(FVector::DotProduct(NormDir, FVector::ForwardVector), 0.0f, 1.0f);

    // Lateral reach is typically 80% of forward reach
    float BaseReach = BodyMeasurements.ReachDistance;
    float LateralFactor = 1.0f - (0.2f * (1.0f - ForwardFactor));

    // Upward reach is limited, downward is easier
    float VerticalFactor = 1.0f;
    if (NormDir.Z > 0.5f)
    {
        VerticalFactor = 0.8f; // Harder to reach up
    }
    else if (NormDir.Z < -0.5f)
    {
        VerticalFactor = 1.1f; // Easier to reach down
    }

    return BaseReach * LateralFactor * VerticalFactor;
}

// ========================================
// EXPRESSION BINDING
// ========================================

void UDNABodySchemaBinding::SetBlendShapeValue(const FString& ChannelName, float Value)
{
    if (FDNABlendShapeBinding* Binding = BlendShapeBindings.Find(ChannelName))
    {
        Binding->CurrentValue = FMath::Clamp(Value, 0.0f, 1.0f);

        // Apply to skeletal mesh morph target
        if (BoundSkeletalMesh)
        {
            BoundSkeletalMesh->SetMorphTarget(Binding->MorphTargetName, Binding->CurrentValue);
        }
    }
}

float UDNABodySchemaBinding::GetBlendShapeValue(const FString& ChannelName) const
{
    if (const FDNABlendShapeBinding* Binding = BlendShapeBindings.Find(ChannelName))
    {
        return Binding->CurrentValue;
    }
    return 0.0f;
}

void UDNABodySchemaBinding::ApplyEmotionalState(float Valence, float Arousal)
{
    // Apply emotional state to blend shapes based on their valence/arousal mappings
    Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    Arousal = FMath::Clamp(Arousal, 0.0f, 1.0f);

    for (auto& Pair : BlendShapeBindings)
    {
        FDNABlendShapeBinding& Binding = Pair.Value;

        if (!Binding.bIsPrimaryExpression)
        {
            continue; // Only affect primary expression shapes
        }

        // Compute activation based on emotional similarity
        float ValenceSimilarity = 1.0f - FMath::Abs(Binding.EmotionalValence - Valence) / 2.0f;
        float ArousalMatch = 1.0f - FMath::Abs(Binding.EmotionalArousal - Arousal);

        // Combine similarity factors
        float Activation = ValenceSimilarity * ArousalMatch;

        // Only activate if emotional state matches blend shape's mapping
        if ((Valence > 0 && Binding.EmotionalValence > 0) ||
            (Valence < 0 && Binding.EmotionalValence < 0) ||
            FMath::Abs(Binding.EmotionalValence) < 0.1f)
        {
            Binding.CurrentValue = Binding.BaselineValue + (Activation * (1.0f - Binding.BaselineValue));
        }
        else
        {
            Binding.CurrentValue = Binding.BaselineValue * (1.0f - Activation * 0.5f);
        }

        Binding.CurrentValue = FMath::Clamp(Binding.CurrentValue, 0.0f, 1.0f);

        // Apply to mesh
        if (BoundSkeletalMesh)
        {
            BoundSkeletalMesh->SetMorphTarget(Binding.MorphTargetName, Binding.CurrentValue);
        }
    }
}

TArray<FDNABlendShapeBinding> UDNABodySchemaBinding::GetBlendShapesByCategory(EBlendShapeCategory Category) const
{
    TArray<FDNABlendShapeBinding> Result;

    for (const auto& Pair : BlendShapeBindings)
    {
        if (Pair.Value.Category == Category)
        {
            Result.Add(Pair.Value);
        }
    }

    return Result;
}

// ========================================
// MOTOR CAPABILITIES
// ========================================

float UDNABodySchemaBinding::GetMotorCapability(EMotorEffectorType EffectorType) const
{
    // Compute aggregate capability for effector type based on joint states
    float TotalCapability = 0.0f;
    int32 JointCount = 0;

    for (const auto& Pair : JointBindings)
    {
        const FDNAJointBinding& Binding = Pair.Value;

        if (Binding.MotorEffectors.Contains(EffectorType))
        {
            // Capability reduced by fatigue and tension
            if (const FProprioceptiveState* PropState = ProprioceptiveStates.Find(Pair.Key))
            {
                float JointCapability = 1.0f - PropState->Fatigue * 0.5f;
                TotalCapability += JointCapability;
            }
            else
            {
                TotalCapability += 1.0f;
            }
            JointCount++;
        }
    }

    return JointCount > 0 ? TotalCapability / JointCount : 0.0f;
}

TArray<FString> UDNABodySchemaBinding::GetJointsForEffector(EMotorEffectorType EffectorType) const
{
    TArray<FString> Result;

    for (const auto& Pair : JointBindings)
    {
        if (Pair.Value.MotorEffectors.Contains(EffectorType))
        {
            Result.Add(Pair.Key);
        }
    }

    return Result;
}

bool UDNABodySchemaBinding::IsMovementWithinLimits(const FString& JointName, const FRotator& TargetRotation) const
{
    if (const FDNAJointBinding* Binding = JointBindings.Find(JointName))
    {
        return (TargetRotation.Roll >= Binding->RotationMin.Roll && TargetRotation.Roll <= Binding->RotationMax.Roll) &&
               (TargetRotation.Pitch >= Binding->RotationMin.Pitch && TargetRotation.Pitch <= Binding->RotationMax.Pitch) &&
               (TargetRotation.Yaw >= Binding->RotationMin.Yaw && TargetRotation.Yaw <= Binding->RotationMax.Yaw);
    }
    return false;
}

// ========================================
// RESERVOIR INTEGRATION
// ========================================

TArray<float> UDNABodySchemaBinding::EncodeBodyState(int32 StreamID)
{
    TArray<float> ProprioceptiveData = GetProprioceptiveVector();

    if (ReservoirComponent)
    {
        // Process through reservoir for temporal encoding
        return ReservoirComponent->ProcessInput(ProprioceptiveData, StreamID);
    }

    return ProprioceptiveData;
}

void UDNABodySchemaBinding::RecordMovementFrame()
{
    TArray<float> CurrentFrame = GetProprioceptiveVector();

    MovementHistory.Add(CurrentFrame);

    // Maintain history limit
    while (MovementHistory.Num() > MaxMovementHistoryFrames)
    {
        MovementHistory.RemoveAt(0);
    }
}

TArray<float> UDNABodySchemaBinding::GetMovementPatternEmbedding(float DurationSeconds)
{
    int32 FrameCount = FMath::Min(
        static_cast<int32>(DurationSeconds * SyncRate),
        MovementHistory.Num()
    );

    if (FrameCount == 0)
    {
        return TArray<float>();
    }

    // Aggregate recent movement frames
    TArray<float> Embedding;
    int32 StartIdx = MovementHistory.Num() - FrameCount;

    if (ReservoirComponent)
    {
        // Feed movement history through reservoir for pattern encoding
        for (int32 i = StartIdx; i < MovementHistory.Num(); i++)
        {
            ReservoirComponent->ProcessInput(MovementHistory[i], 1);
        }

        // Get final reservoir state as embedding
        Embedding = ReservoirComponent->GetReservoirState(1);
    }
    else
    {
        // Without reservoir, compute average of recent frames
        if (MovementHistory.Num() > 0 && MovementHistory[StartIdx].Num() > 0)
        {
            Embedding.SetNum(MovementHistory[StartIdx].Num());
            for (int32 i = 0; i < Embedding.Num(); i++)
            {
                Embedding[i] = 0.0f;
            }

            for (int32 i = StartIdx; i < MovementHistory.Num(); i++)
            {
                for (int32 j = 0; j < FMath::Min(Embedding.Num(), MovementHistory[i].Num()); j++)
                {
                    Embedding[j] += MovementHistory[i][j];
                }
            }

            for (int32 i = 0; i < Embedding.Num(); i++)
            {
                Embedding[i] /= FrameCount;
            }
        }
    }

    return Embedding;
}

// ========================================
// CLASSIFICATION HELPERS
// ========================================

EBodyRegion UDNABodySchemaBinding::ClassifyJointToRegion(const FString& JointName) const
{
    FString LowerName = JointName.ToLower();

    if (LowerName.Contains(TEXT("head")) || LowerName.Contains(TEXT("facial")))
    {
        return EBodyRegion::Face;
    }
    if (LowerName.Contains(TEXT("neck")))
    {
        return EBodyRegion::Neck;
    }
    if (LowerName.Contains(TEXT("spine")))
    {
        return EBodyRegion::Spine;
    }
    if (LowerName.Contains(TEXT("pelvis")))
    {
        return EBodyRegion::Pelvis;
    }
    if (LowerName.Contains(TEXT("clavicle")) || LowerName.Contains(TEXT("chest")))
    {
        return EBodyRegion::Torso;
    }

    // Arms
    if (LowerName.Contains(TEXT("hand_l")) || LowerName.Contains(TEXT("thumb_")) ||
        LowerName.Contains(TEXT("index_")) || LowerName.Contains(TEXT("middle_")) ||
        LowerName.Contains(TEXT("ring_")) || LowerName.Contains(TEXT("pinky_")))
    {
        if (LowerName.Contains(TEXT("_l")))
        {
            return EBodyRegion::LeftHand;
        }
        if (LowerName.Contains(TEXT("_r")))
        {
            return EBodyRegion::RightHand;
        }
    }
    if (LowerName.Contains(TEXT("arm")) || LowerName.Contains(TEXT("upperarm")) ||
        LowerName.Contains(TEXT("lowerarm")))
    {
        if (LowerName.Contains(TEXT("_l")))
        {
            return EBodyRegion::LeftArm;
        }
        if (LowerName.Contains(TEXT("_r")))
        {
            return EBodyRegion::RightArm;
        }
    }

    // Legs
    if (LowerName.Contains(TEXT("foot")) || LowerName.Contains(TEXT("ball")))
    {
        if (LowerName.Contains(TEXT("_l")))
        {
            return EBodyRegion::LeftFoot;
        }
        if (LowerName.Contains(TEXT("_r")))
        {
            return EBodyRegion::RightFoot;
        }
    }
    if (LowerName.Contains(TEXT("thigh")) || LowerName.Contains(TEXT("calf")) ||
        LowerName.Contains(TEXT("leg")))
    {
        if (LowerName.Contains(TEXT("_l")))
        {
            return EBodyRegion::LeftLeg;
        }
        if (LowerName.Contains(TEXT("_r")))
        {
            return EBodyRegion::RightLeg;
        }
    }

    return EBodyRegion::Unknown;
}

ELaterality UDNABodySchemaBinding::DetermineLaterality(const FString& JointName) const
{
    if (JointName.EndsWith(TEXT("_l")) || JointName.EndsWith(TEXT("_L")))
    {
        return ELaterality::Left;
    }
    if (JointName.EndsWith(TEXT("_r")) || JointName.EndsWith(TEXT("_R")))
    {
        return ELaterality::Right;
    }

    // Center-line joints
    if (JointName.Contains(TEXT("spine")) || JointName.Contains(TEXT("neck")) ||
        JointName.Contains(TEXT("head")) || JointName.Contains(TEXT("pelvis")) ||
        JointName.Contains(TEXT("root")))
    {
        return ELaterality::Center;
    }

    return ELaterality::Center;
}

EBlendShapeCategory UDNABodySchemaBinding::ClassifyBlendShape(const FString& ChannelName) const
{
    FString LowerName = ChannelName.ToLower();

    if (LowerName.Contains(TEXT("brow")))
    {
        return EBlendShapeCategory::Brow;
    }
    if (LowerName.Contains(TEXT("eye")) || LowerName.Contains(TEXT("blink")))
    {
        return EBlendShapeCategory::Eye;
    }
    if (LowerName.Contains(TEXT("nose")) || LowerName.Contains(TEXT("sneer")))
    {
        return EBlendShapeCategory::Nose;
    }
    if (LowerName.Contains(TEXT("cheek")))
    {
        return EBlendShapeCategory::Cheek;
    }
    if (LowerName.Contains(TEXT("mouth")) || LowerName.Contains(TEXT("lip")) ||
        LowerName.Contains(TEXT("smile")) || LowerName.Contains(TEXT("frown")))
    {
        return EBlendShapeCategory::Mouth;
    }
    if (LowerName.Contains(TEXT("jaw")))
    {
        return EBlendShapeCategory::Jaw;
    }
    if (LowerName.Contains(TEXT("tongue")))
    {
        return EBlendShapeCategory::Tongue;
    }
    if (LowerName.Contains(TEXT("neck")))
    {
        return EBlendShapeCategory::Neck;
    }

    return EBlendShapeCategory::Other;
}

TArray<EMotorEffectorType> UDNABodySchemaBinding::DetermineMotorEffectors(const FString& JointName) const
{
    TArray<EMotorEffectorType> Effectors;
    FString LowerName = JointName.ToLower();

    // Locomotion (legs, pelvis)
    if (LowerName.Contains(TEXT("thigh")) || LowerName.Contains(TEXT("calf")) ||
        LowerName.Contains(TEXT("foot")) || LowerName.Contains(TEXT("ball")) ||
        LowerName.Contains(TEXT("pelvis")))
    {
        Effectors.Add(EMotorEffectorType::Locomotion);
        Effectors.Add(EMotorEffectorType::Balance);
    }

    // Manipulation (arms, hands)
    if (LowerName.Contains(TEXT("arm")) || LowerName.Contains(TEXT("hand")) ||
        LowerName.Contains(TEXT("thumb")) || LowerName.Contains(TEXT("index")) ||
        LowerName.Contains(TEXT("middle")) || LowerName.Contains(TEXT("ring")) ||
        LowerName.Contains(TEXT("pinky")) || LowerName.Contains(TEXT("clavicle")))
    {
        Effectors.Add(EMotorEffectorType::Manipulation);
    }

    // Gaze (head, neck)
    if (LowerName.Contains(TEXT("head")) || LowerName.Contains(TEXT("neck")))
    {
        Effectors.Add(EMotorEffectorType::Gaze);
    }

    // Expression (face)
    if (LowerName.Contains(TEXT("facial")) || LowerName.Contains(TEXT("head")))
    {
        Effectors.Add(EMotorEffectorType::Expression);
    }

    // Vocalization (jaw, tongue, throat)
    if (LowerName.Contains(TEXT("jaw")) || LowerName.Contains(TEXT("tongue")) ||
        LowerName.Contains(TEXT("neck_01")))
    {
        Effectors.Add(EMotorEffectorType::Vocalization);
    }

    // Posture (spine, pelvis)
    if (LowerName.Contains(TEXT("spine")) || LowerName.Contains(TEXT("pelvis")))
    {
        Effectors.Add(EMotorEffectorType::Posture);
    }

    return Effectors;
}

FString UDNABodySchemaBinding::MapJointToBodySchemaPart(const FString& JointName) const
{
    // Map DNA joint names to body schema part identifiers
    // Body schema uses semantic names for body awareness

    FString LowerName = JointName.ToLower();

    // Head and face
    if (LowerName == TEXT("head"))
    {
        return TEXT("Head");
    }
    if (LowerName.Contains(TEXT("facial")))
    {
        return TEXT("Face");
    }
    if (LowerName == TEXT("neck_01") || LowerName == TEXT("neck_02"))
    {
        return TEXT("Neck");
    }

    // Torso
    if (LowerName == TEXT("spine_01") || LowerName == TEXT("spine_02"))
    {
        return TEXT("LowerBack");
    }
    if (LowerName == TEXT("spine_03") || LowerName == TEXT("spine_04"))
    {
        return TEXT("MidBack");
    }
    if (LowerName == TEXT("spine_05"))
    {
        return TEXT("UpperBack");
    }
    if (LowerName == TEXT("pelvis"))
    {
        return TEXT("Pelvis");
    }
    if (LowerName.Contains(TEXT("clavicle")))
    {
        return LowerName.Contains(TEXT("_l")) ? TEXT("LeftShoulder") : TEXT("RightShoulder");
    }

    // Arms
    if (LowerName == TEXT("upperarm_l"))
    {
        return TEXT("LeftUpperArm");
    }
    if (LowerName == TEXT("upperarm_r"))
    {
        return TEXT("RightUpperArm");
    }
    if (LowerName == TEXT("lowerarm_l"))
    {
        return TEXT("LeftForearm");
    }
    if (LowerName == TEXT("lowerarm_r"))
    {
        return TEXT("RightForearm");
    }
    if (LowerName == TEXT("hand_l"))
    {
        return TEXT("LeftHand");
    }
    if (LowerName == TEXT("hand_r"))
    {
        return TEXT("RightHand");
    }

    // Fingers - Left
    if (LowerName.Contains(TEXT("thumb")) && LowerName.Contains(TEXT("_l")))
    {
        return TEXT("LeftThumb");
    }
    if (LowerName.Contains(TEXT("index")) && LowerName.Contains(TEXT("_l")))
    {
        return TEXT("LeftIndexFinger");
    }
    if (LowerName.Contains(TEXT("middle")) && LowerName.Contains(TEXT("_l")))
    {
        return TEXT("LeftMiddleFinger");
    }
    if (LowerName.Contains(TEXT("ring")) && LowerName.Contains(TEXT("_l")))
    {
        return TEXT("LeftRingFinger");
    }
    if (LowerName.Contains(TEXT("pinky")) && LowerName.Contains(TEXT("_l")))
    {
        return TEXT("LeftPinkyFinger");
    }

    // Fingers - Right
    if (LowerName.Contains(TEXT("thumb")) && LowerName.Contains(TEXT("_r")))
    {
        return TEXT("RightThumb");
    }
    if (LowerName.Contains(TEXT("index")) && LowerName.Contains(TEXT("_r")))
    {
        return TEXT("RightIndexFinger");
    }
    if (LowerName.Contains(TEXT("middle")) && LowerName.Contains(TEXT("_r")))
    {
        return TEXT("RightMiddleFinger");
    }
    if (LowerName.Contains(TEXT("ring")) && LowerName.Contains(TEXT("_r")))
    {
        return TEXT("RightRingFinger");
    }
    if (LowerName.Contains(TEXT("pinky")) && LowerName.Contains(TEXT("_r")))
    {
        return TEXT("RightPinkyFinger");
    }

    // Legs
    if (LowerName == TEXT("thigh_l"))
    {
        return TEXT("LeftThigh");
    }
    if (LowerName == TEXT("thigh_r"))
    {
        return TEXT("RightThigh");
    }
    if (LowerName == TEXT("calf_l"))
    {
        return TEXT("LeftCalf");
    }
    if (LowerName == TEXT("calf_r"))
    {
        return TEXT("RightCalf");
    }
    if (LowerName == TEXT("foot_l"))
    {
        return TEXT("LeftFoot");
    }
    if (LowerName == TEXT("foot_r"))
    {
        return TEXT("RightFoot");
    }
    if (LowerName == TEXT("ball_l"))
    {
        return TEXT("LeftToes");
    }
    if (LowerName == TEXT("ball_r"))
    {
        return TEXT("RightToes");
    }

    // Root
    if (LowerName == TEXT("root"))
    {
        return TEXT("Root");
    }

    return JointName; // Return original if no mapping found
}

// ========================================
// ROTATION LIMITS HELPER
// ========================================

void SetDefaultRotationLimits(FDNAJointBinding& Binding)
{
    FString LowerName = Binding.DNAJointName.ToLower();

    // Spine - limited rotation
    if (LowerName.Contains(TEXT("spine")))
    {
        Binding.RotationMin = FRotator(-15.0f, -30.0f, -20.0f);
        Binding.RotationMax = FRotator(15.0f, 30.0f, 20.0f);
    }
    // Neck - moderate rotation
    else if (LowerName.Contains(TEXT("neck")))
    {
        Binding.RotationMin = FRotator(-30.0f, -45.0f, -30.0f);
        Binding.RotationMax = FRotator(30.0f, 45.0f, 30.0f);
    }
    // Head - good rotation
    else if (LowerName == TEXT("head"))
    {
        Binding.RotationMin = FRotator(-40.0f, -70.0f, -40.0f);
        Binding.RotationMax = FRotator(40.0f, 70.0f, 40.0f);
    }
    // Shoulder/Clavicle - limited
    else if (LowerName.Contains(TEXT("clavicle")))
    {
        Binding.RotationMin = FRotator(-20.0f, -20.0f, -30.0f);
        Binding.RotationMax = FRotator(20.0f, 20.0f, 30.0f);
    }
    // Upper arm - wide range
    else if (LowerName.Contains(TEXT("upperarm")))
    {
        Binding.RotationMin = FRotator(-100.0f, -90.0f, -180.0f);
        Binding.RotationMax = FRotator(100.0f, 90.0f, 180.0f);
    }
    // Lower arm - hinge
    else if (LowerName.Contains(TEXT("lowerarm")))
    {
        Binding.RotationMin = FRotator(-5.0f, -140.0f, -90.0f);
        Binding.RotationMax = FRotator(5.0f, 0.0f, 90.0f);
    }
    // Hand - moderate
    else if (LowerName.Contains(TEXT("hand_")))
    {
        Binding.RotationMin = FRotator(-80.0f, -30.0f, -80.0f);
        Binding.RotationMax = FRotator(80.0f, 70.0f, 80.0f);
    }
    // Fingers - hinge
    else if (LowerName.Contains(TEXT("thumb")) || LowerName.Contains(TEXT("index")) ||
             LowerName.Contains(TEXT("middle")) || LowerName.Contains(TEXT("ring")) ||
             LowerName.Contains(TEXT("pinky")))
    {
        if (LowerName.Contains(TEXT("_01")))
        {
            Binding.RotationMin = FRotator(-20.0f, -10.0f, -30.0f);
            Binding.RotationMax = FRotator(20.0f, 90.0f, 30.0f);
        }
        else
        {
            Binding.RotationMin = FRotator(-5.0f, -5.0f, 0.0f);
            Binding.RotationMax = FRotator(5.0f, 100.0f, 0.0f);
        }
    }
    // Thigh - hip joint
    else if (LowerName.Contains(TEXT("thigh")))
    {
        Binding.RotationMin = FRotator(-45.0f, -15.0f, -120.0f);
        Binding.RotationMax = FRotator(45.0f, 45.0f, 30.0f);
    }
    // Calf - knee hinge
    else if (LowerName.Contains(TEXT("calf")))
    {
        Binding.RotationMin = FRotator(-5.0f, 0.0f, -5.0f);
        Binding.RotationMax = FRotator(5.0f, 140.0f, 5.0f);
    }
    // Foot - ankle
    else if (LowerName.Contains(TEXT("foot")))
    {
        Binding.RotationMin = FRotator(-30.0f, -20.0f, -45.0f);
        Binding.RotationMax = FRotator(30.0f, 45.0f, 20.0f);
    }
    // Ball - toe hinge
    else if (LowerName.Contains(TEXT("ball")))
    {
        Binding.RotationMin = FRotator(-5.0f, -30.0f, -5.0f);
        Binding.RotationMax = FRotator(5.0f, 60.0f, 5.0f);
    }
    // Pelvis - base
    else if (LowerName == TEXT("pelvis"))
    {
        Binding.RotationMin = FRotator(-30.0f, -45.0f, -20.0f);
        Binding.RotationMax = FRotator(30.0f, 45.0f, 20.0f);
    }
    // Root - full freedom (world space)
    else if (LowerName == TEXT("root"))
    {
        Binding.RotationMin = FRotator(-180.0f, -180.0f, -180.0f);
        Binding.RotationMax = FRotator(180.0f, 180.0f, 180.0f);
    }
}

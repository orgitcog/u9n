// MetaHumanDNABridge.cpp
// Deep Tree Echo - MetaHuman DNA Integration Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "MetaHumanDNABridge.h"
#include "PythonDNACalibWrapper.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanDNA, Log, All);

// ========================================
// Constructor / Destructor
// ========================================

UMetaHumanDNABridge::UMetaHumanDNABridge()
    : bDNALoaded(false)
    , DNAVersion(EDNAVersion::Unknown)
{
    UE_LOG(LogMetaHumanDNA, Log, TEXT("MetaHumanDNABridge initialized"));
}

UMetaHumanDNABridge::~UMetaHumanDNABridge()
{
    if (PythonWrapper.IsValid())
    {
        ShutdownPythonWrapper();
    }
}

// ========================================
// DNA File Operations
// ========================================

bool UMetaHumanDNABridge::LoadDNAFile(const FString& DNAPath)
{
    if (!FPaths::FileExists(DNAPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("DNA file not found: %s"), *DNAPath);
        return false;
    }

    // Initialize Python wrapper if not already done
    if (!PythonWrapper.IsValid())
    {
        FString DNACalibPath = FPaths::ProjectDir() / TEXT("MetaHuman-DNA-Calibration");
        if (!InitializePythonWrapper(DNACalibPath))
        {
            UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to initialize Python wrapper"));
            return false;
        }
    }

    // Load DNA using Python wrapper
    if (!PythonWrapper->LoadDNA(DNAPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to load DNA file: %s"), *DNAPath);
        return false;
    }

    LoadedDNAPath = DNAPath;
    bDNALoaded = true;

    // Parse DNA version
    if (!ParseDNAVersion())
    {
        UE_LOG(LogMetaHumanDNA, Warning, TEXT("Could not determine DNA version, assuming DHI"));
        DNAVersion = EDNAVersion::DHI;
    }

    // Load DNA data into cache
    if (!LoadDNAData())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to load DNA data"));
        bDNALoaded = false;
        return false;
    }

    // Build cache maps
    BuildCacheMaps();

    // Initialize blend shape weights
    InitializeBlendShapeWeights();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully loaded DNA file: %s (Version: %s)"), 
        *DNAPath, *GetDNAVersionString());

    return true;
}

bool UMetaHumanDNABridge::SaveDNAFile(const FString& DNAPath)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded or Python wrapper not initialized"));
        return false;
    }

    if (!PythonWrapper->SaveDNA(DNAPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to save DNA file: %s"), *DNAPath);
        return false;
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully saved DNA file: %s"), *DNAPath);
    return true;
}

FString UMetaHumanDNABridge::GetDNAVersionString() const
{
    switch (DNAVersion)
    {
    case EDNAVersion::DHI:
        return TEXT("DHI");
    case EDNAVersion::MH4:
        return TEXT("MH.4");
    default:
        return TEXT("Unknown");
    }
}

// ========================================
// Rig Structure Access
// ========================================

TArray<FString> UMetaHumanDNABridge::GetJointNames() const
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        return TArray<FString>();
    }

    return PythonWrapper->GetJointNames();
}

TArray<FString> UMetaHumanDNABridge::GetMeshNames() const
{
    TArray<FString> MeshNames;
    for (const FDNAMeshInfo& MeshInfo : MeshInfoCache)
    {
        MeshNames.Add(MeshInfo.MeshName);
    }
    return MeshNames;
}

TArray<FString> UMetaHumanDNABridge::GetBlendShapeNames() const
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        return TArray<FString>();
    }

    return PythonWrapper->GetBlendShapeNames();
}

bool UMetaHumanDNABridge::GetJointInfo(const FString& JointName, FDNAJointInfo& OutJointInfo) const
{
    const int32* IndexPtr = JointNameToIndexMap.Find(JointName);
    if (!IndexPtr || !JointInfoCache.IsValidIndex(*IndexPtr))
    {
        return false;
    }

    OutJointInfo = JointInfoCache[*IndexPtr];
    return true;
}

bool UMetaHumanDNABridge::GetBlendShapeInfo(const FString& BlendShapeName, FDNABlendShapeInfo& OutBlendShapeInfo) const
{
    const int32* IndexPtr = BlendShapeNameToIndexMap.Find(BlendShapeName);
    if (!IndexPtr || !BlendShapeInfoCache.IsValidIndex(*IndexPtr))
    {
        return false;
    }

    OutBlendShapeInfo = BlendShapeInfoCache[*IndexPtr];
    return true;
}

bool UMetaHumanDNABridge::GetMeshInfo(const FString& MeshName, FDNAMeshInfo& OutMeshInfo) const
{
    const int32* IndexPtr = MeshNameToIndexMap.Find(MeshName);
    if (!IndexPtr || !MeshInfoCache.IsValidIndex(*IndexPtr))
    {
        return false;
    }

    OutMeshInfo = MeshInfoCache[*IndexPtr];
    return true;
}

FTransform UMetaHumanDNABridge::GetNeutralJointTransform(const FString& JointName) const
{
    FDNAJointInfo JointInfo;
    if (GetJointInfo(JointName, JointInfo))
    {
        return JointInfo.NeutralTransform;
    }
    return FTransform::Identity;
}

// ========================================
// DNA Manipulation
// ========================================

bool UMetaHumanDNABridge::RenameJoint(const FString& OldName, const FString& NewName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Check for critical joints that should not be renamed
    if (OldName == TEXT("neck_01") || OldName == TEXT("neck_02") || OldName == TEXT("FACIAL_C_FacialRoot"))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Cannot rename critical joint: %s"), *OldName);
        return false;
    }

    if (!PythonWrapper->RenameJoint(OldName, NewName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to rename joint: %s -> %s"), *OldName, *NewName);
        return false;
    }

    // Update cache
    LoadDNAData();
    BuildCacheMaps();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully renamed joint: %s -> %s"), *OldName, *NewName);
    return true;
}

bool UMetaHumanDNABridge::RemoveJoint(const FString& JointName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Check for critical joints that should not be removed
    if (JointName == TEXT("neck_01") || JointName == TEXT("neck_02") || JointName == TEXT("FACIAL_C_FacialRoot"))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Cannot remove critical joint: %s"), *JointName);
        return false;
    }

    if (!PythonWrapper->RemoveJoint(JointName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to remove joint: %s"), *JointName);
        return false;
    }

    // Update cache
    LoadDNAData();
    BuildCacheMaps();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully removed joint: %s"), *JointName);
    return true;
}

bool UMetaHumanDNABridge::RenameBlendShape(const FString& OldName, const FString& NewName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Validate blend shape exists
    if (!BlendShapeNameToIndexMap.Contains(OldName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Blend shape not found: %s"), *OldName);
        return false;
    }

    // Check new name doesn't conflict
    if (BlendShapeNameToIndexMap.Contains(NewName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Blend shape already exists with name: %s"), *NewName);
        return false;
    }

    if (!PythonWrapper->RenameBlendShape(OldName, NewName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to rename blend shape: %s -> %s"), *OldName, *NewName);
        return false;
    }

    // Update cache
    LoadDNAData();
    BuildCacheMaps();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully renamed blend shape: %s -> %s"), *OldName, *NewName);
    return true;
}

bool UMetaHumanDNABridge::RemoveBlendShape(const FString& BlendShapeName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Validate blend shape exists
    if (!BlendShapeNameToIndexMap.Contains(BlendShapeName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Blend shape not found: %s"), *BlendShapeName);
        return false;
    }

    if (!PythonWrapper->RemoveBlendShape(BlendShapeName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to remove blend shape: %s"), *BlendShapeName);
        return false;
    }

    // Update cache
    LoadDNAData();
    BuildCacheMaps();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully removed blend shape: %s"), *BlendShapeName);
    return true;
}

bool UMetaHumanDNABridge::ModifyBlendShapeDeltas(const FString& BlendShapeName, const TArray<FVector>& Deltas)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Validate blend shape exists
    FDNABlendShapeInfo BlendShapeInfo;
    if (!GetBlendShapeInfo(BlendShapeName, BlendShapeInfo))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Blend shape not found: %s"), *BlendShapeName);
        return false;
    }

    // Get target mesh name
    FString TargetMeshName = PythonWrapper->GetBlendShapeTargetMesh(BlendShapeName);
    if (TargetMeshName.IsEmpty())
    {
        TargetMeshName = TEXT("head_lod0"); // Default to head mesh
    }

    if (!PythonWrapper->ModifyBlendShapeDeltas(BlendShapeName, TargetMeshName, Deltas))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to modify blend shape deltas: %s"), *BlendShapeName);
        return false;
    }

    // Update blend shape info cache with new vertex count
    if (int32* IndexPtr = BlendShapeNameToIndexMap.Find(BlendShapeName))
    {
        if (BlendShapeInfoCache.IsValidIndex(*IndexPtr))
        {
            BlendShapeInfoCache[*IndexPtr].VertexCount = Deltas.Num();
        }
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully modified blend shape deltas: %s (%d vertices)"),
           *BlendShapeName, Deltas.Num());
    return true;
}

bool UMetaHumanDNABridge::ClearAllBlendShapes()
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    if (!PythonWrapper->ClearBlendShapes())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to clear blend shapes"));
        return false;
    }

    // Clear blend shape cache and weights
    BlendShapeInfoCache.Empty();
    BlendShapeNameToIndexMap.Empty();
    CurrentBlendShapeWeights.Empty();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully cleared all blend shapes"));
    return true;
}

bool UMetaHumanDNABridge::RemoveLOD(int32 LODIndex)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    if (LODIndex < 0 || LODIndex > 7)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Invalid LOD index: %d (must be 0-7)"), LODIndex);
        return false;
    }

    if (!PythonWrapper->RemoveLOD(LODIndex))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to remove LOD: %d"), LODIndex);
        return false;
    }

    // Update mesh cache (remove LOD-specific meshes)
    for (int32 i = MeshInfoCache.Num() - 1; i >= 0; i--)
    {
        FString LODSuffix = FString::Printf(TEXT("_lod%d"), LODIndex);
        if (MeshInfoCache[i].MeshName.EndsWith(LODSuffix))
        {
            MeshNameToIndexMap.Remove(MeshInfoCache[i].MeshName);
            MeshInfoCache.RemoveAt(i);
        }
    }

    // Rebuild mesh index map
    for (int32 i = 0; i < MeshInfoCache.Num(); i++)
    {
        MeshNameToIndexMap.Add(MeshInfoCache[i].MeshName, i);
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully removed LOD: %d"), LODIndex);
    return true;
}

// ========================================
// Integration with Avatar System
// ========================================

bool UMetaHumanDNABridge::ApplyDNAToSkeleton(USkeletalMeshComponent* SkeletalMesh)
{
    if (!bDNALoaded || !SkeletalMesh)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded or invalid skeletal mesh"));
        return false;
    }

    USkeletalMesh* Mesh = SkeletalMesh->SkeletalMesh;
    if (!Mesh)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Skeletal mesh component has no mesh"));
        return false;
    }

    USkeleton* Skeleton = Mesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Skeletal mesh has no skeleton"));
        return false;
    }

    // Map DNA joints to Unreal skeleton
    TArray<FString> DNAJoints = GetJointNames();
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

    int32 MappedJoints = 0;
    for (const FString& DNAJointName : DNAJoints)
    {
        FName BoneName(*DNAJointName);
        int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
        
        if (BoneIndex != INDEX_NONE)
        {
            MappedJoints++;
            // Successfully mapped joint
            // Could store mapping for future use
        }
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Mapped %d/%d DNA joints to skeleton"), MappedJoints, DNAJoints.Num());

    return MappedJoints > 0;
}

void UMetaHumanDNABridge::SyncNeurochemicalStateToBlendShapes(const FNeurochemicalState& State, TMap<FString, float>& OutBlendShapeWeights)
{
    MapNeurochemicalToBlendShapes(State, OutBlendShapeWeights);
}

void UMetaHumanDNABridge::SyncEmotionalStateToFacialRig(const FEmotionalState& Emotion, TMap<FString, float>& OutBlendShapeWeights)
{
    MapEmotionalStateToBlendShapes(Emotion, OutBlendShapeWeights);
}

float UMetaHumanDNABridge::GetBlendShapeWeight(const FString& BlendShapeName) const
{
    const float* WeightPtr = CurrentBlendShapeWeights.Find(BlendShapeName);
    return WeightPtr ? *WeightPtr : 0.0f;
}

void UMetaHumanDNABridge::SetBlendShapeWeight(const FString& BlendShapeName, float Weight)
{
    CurrentBlendShapeWeights.Add(BlendShapeName, FMath::Clamp(Weight, 0.0f, 1.0f));
}

TMap<FString, float> UMetaHumanDNABridge::GetAllBlendShapeWeights() const
{
    return CurrentBlendShapeWeights;
}

void UMetaHumanDNABridge::SetAllBlendShapeWeights(const TMap<FString, float>& Weights)
{
    CurrentBlendShapeWeights = Weights;
    
    // Clamp all weights to 0-1
    for (auto& Pair : CurrentBlendShapeWeights)
    {
        Pair.Value = FMath::Clamp(Pair.Value, 0.0f, 1.0f);
    }
}

// ========================================
// Advanced Features
// ========================================

bool UMetaHumanDNABridge::InitializePythonWrapper(const FString& DNACalibPath)
{
    if (PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Warning, TEXT("Python wrapper already initialized"));
        return true;
    }

    PythonWrapper = MakeShared<FPythonDNACalibWrapper>();
    
    if (!PythonWrapper->Initialize(DNACalibPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to initialize Python wrapper with path: %s"), *DNACalibPath);
        PythonWrapper.Reset();
        return false;
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Python wrapper initialized successfully"));
    return true;
}

void UMetaHumanDNABridge::ShutdownPythonWrapper()
{
    if (PythonWrapper.IsValid())
    {
        PythonWrapper->Shutdown();
        PythonWrapper.Reset();
        UE_LOG(LogMetaHumanDNA, Log, TEXT("Python wrapper shut down"));
    }
}

void UMetaHumanDNABridge::GetDNAStatistics(int32& OutJointCount, int32& OutMeshCount, int32& OutBlendShapeCount) const
{
    OutJointCount = JointInfoCache.Num();
    OutMeshCount = MeshInfoCache.Num();
    OutBlendShapeCount = BlendShapeInfoCache.Num();
}

bool UMetaHumanDNABridge::ValidateDNAIntegrity() const
{
    if (!bDNALoaded)
    {
        return false;
    }

    // Check for critical joints
    TArray<FString> CriticalJoints = { TEXT("neck_01"), TEXT("neck_02"), TEXT("FACIAL_C_FacialRoot") };
    TArray<FString> JointNames = GetJointNames();

    for (const FString& CriticalJoint : CriticalJoints)
    {
        if (!JointNames.Contains(CriticalJoint))
        {
            UE_LOG(LogMetaHumanDNA, Error, TEXT("Critical joint missing: %s"), *CriticalJoint);
            return false;
        }
    }

    return true;
}

// ========================================
// Helper Functions
// ========================================

void UMetaHumanDNABridge::BuildCacheMaps()
{
    JointNameToIndexMap.Empty();
    BlendShapeNameToIndexMap.Empty();
    MeshNameToIndexMap.Empty();

    for (int32 i = 0; i < JointInfoCache.Num(); ++i)
    {
        JointNameToIndexMap.Add(JointInfoCache[i].JointName, i);
    }

    for (int32 i = 0; i < BlendShapeInfoCache.Num(); ++i)
    {
        BlendShapeNameToIndexMap.Add(BlendShapeInfoCache[i].BlendShapeName, i);
    }

    for (int32 i = 0; i < MeshInfoCache.Num(); ++i)
    {
        MeshNameToIndexMap.Add(MeshInfoCache[i].MeshName, i);
    }
}

void UMetaHumanDNABridge::ClearCache()
{
    JointInfoCache.Empty();
    BlendShapeInfoCache.Empty();
    MeshInfoCache.Empty();
    JointNameToIndexMap.Empty();
    BlendShapeNameToIndexMap.Empty();
    MeshNameToIndexMap.Empty();
    CurrentBlendShapeWeights.Empty();
}

bool UMetaHumanDNABridge::LoadDNAData()
{
    if (!PythonWrapper.IsValid())
    {
        return false;
    }

    ClearCache();

    // Load joint data with full transform and hierarchy information
    TArray<FString> JointNames = PythonWrapper->GetJointNames();
    for (int32 i = 0; i < JointNames.Num(); ++i)
    {
        FDNAJointInfo JointInfo;
        JointInfo.JointName = JointNames[i];
        JointInfo.JointIndex = i;

        // Get neutral transform from Python wrapper
        FTransform NeutralTransform;
        if (PythonWrapper->GetJointNeutralTransform(JointNames[i], NeutralTransform))
        {
            JointInfo.NeutralTransform = NeutralTransform;
        }
        else
        {
            JointInfo.NeutralTransform = FTransform::Identity;
        }

        // Get parent index from Python wrapper
        JointInfo.ParentIndex = PythonWrapper->GetJointParentIndex(JointNames[i]);

        JointInfoCache.Add(JointInfo);
    }

    // Load blend shape data with target mesh and vertex count
    TArray<FString> BlendShapeNames = PythonWrapper->GetBlendShapeNames();
    for (int32 i = 0; i < BlendShapeNames.Num(); ++i)
    {
        FDNABlendShapeInfo BlendShapeInfo;
        BlendShapeInfo.BlendShapeName = BlendShapeNames[i];
        BlendShapeInfo.BlendShapeIndex = i;

        // Get target mesh from Python wrapper
        BlendShapeInfo.TargetMeshName = PythonWrapper->GetBlendShapeTargetMesh(BlendShapeNames[i]);
        if (BlendShapeInfo.TargetMeshName.IsEmpty())
        {
            BlendShapeInfo.TargetMeshName = TEXT("head_lod0");
        }

        // Get vertex count from Python wrapper
        BlendShapeInfo.VertexCount = PythonWrapper->GetBlendShapeVertexCount(BlendShapeNames[i]);

        BlendShapeInfoCache.Add(BlendShapeInfo);
    }

    // Load mesh data from Python wrapper
    TArray<FString> MeshNames = PythonWrapper->GetMeshNames();
    for (int32 i = 0; i < MeshNames.Num(); ++i)
    {
        FDNAMeshInfo MeshInfo;
        MeshInfo.MeshName = MeshNames[i];
        MeshInfo.MeshIndex = i;

        // Determine LOD level from mesh name
        MeshInfo.LODIndex = 0;
        for (int32 LOD = 0; LOD < 8; ++LOD)
        {
            FString LODSuffix = FString::Printf(TEXT("_lod%d"), LOD);
            if (MeshNames[i].EndsWith(LODSuffix))
            {
                MeshInfo.LODIndex = LOD;
                break;
            }
        }

        MeshInfoCache.Add(MeshInfo);
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Loaded DNA data: %d joints, %d blend shapes, %d meshes"),
           JointInfoCache.Num(), BlendShapeInfoCache.Num(), MeshInfoCache.Num());

    return true;
}

bool UMetaHumanDNABridge::ParseDNAVersion()
{
    if (!PythonWrapper.IsValid())
    {
        return false;
    }

    // Get DNA version from Python wrapper database name
    FString DBName = PythonWrapper->GetDNADatabaseName();

    if (DBName == TEXT("MH.4") || DBName.Contains(TEXT("MH4")))
    {
        DNAVersion = EDNAVersion::MH4;
        UE_LOG(LogMetaHumanDNA, Log, TEXT("Detected DNA version: MH.4"));
    }
    else if (DBName == TEXT("DHI") || DBName.Contains(TEXT("DHI")))
    {
        DNAVersion = EDNAVersion::DHI;
        UE_LOG(LogMetaHumanDNA, Log, TEXT("Detected DNA version: DHI"));
    }
    else
    {
        // Default to DHI for unknown versions
        DNAVersion = EDNAVersion::DHI;
        UE_LOG(LogMetaHumanDNA, Warning, TEXT("Unknown DNA database: %s, assuming DHI"), *DBName);
    }

    return true;
}

void UMetaHumanDNABridge::InitializeBlendShapeWeights()
{
    CurrentBlendShapeWeights.Empty();

    // Initialize all blend shapes to 0
    for (const FDNABlendShapeInfo& BlendShapeInfo : BlendShapeInfoCache)
    {
        CurrentBlendShapeWeights.Add(BlendShapeInfo.BlendShapeName, 0.0f);
    }
}

void UMetaHumanDNABridge::MapNeurochemicalToBlendShapes(const FNeurochemicalState& State, TMap<FString, float>& OutWeights)
{
    // Example mapping: Dopamine -> Smile intensity
    // This is a simplified example; real mapping would be more complex
    
    OutWeights.Empty();

    // Map dopamine to smile/happiness blend shapes
    float SmileIntensity = State.Dopamine * 0.8f + State.Serotonin * 0.5f;
    OutWeights.Add(TEXT("Smile_L"), SmileIntensity);
    OutWeights.Add(TEXT("Smile_R"), SmileIntensity);

    // Map cortisol to stress/tension blend shapes
    float StressIntensity = State.Cortisol * 0.7f;
    OutWeights.Add(TEXT("BrowTension_L"), StressIntensity);
    OutWeights.Add(TEXT("BrowTension_R"), StressIntensity);

    // Map oxytocin to warmth/openness blend shapes
    float WarmthIntensity = State.Oxytocin * 0.6f;
    OutWeights.Add(TEXT("EyeWarmth_L"), WarmthIntensity);
    OutWeights.Add(TEXT("EyeWarmth_R"), WarmthIntensity);

    // Map norepinephrine to alertness blend shapes
    float AlertnessIntensity = State.Norepinephrine * 0.5f;
    OutWeights.Add(TEXT("EyeOpen_L"), AlertnessIntensity);
    OutWeights.Add(TEXT("EyeOpen_R"), AlertnessIntensity);

    // Add version-specific mappings based on DNA format
    if (DNAVersion == EDNAVersion::DHI)
    {
        // DHI-specific neurochemical mappings
        // Map GABA to jaw relaxation
        float RelaxationIntensity = State.GABA * 0.4f;
        OutWeights.Add(TEXT("JawRelax"), RelaxationIntensity);

        // Map adrenaline to nostril flare and eye tension
        float AdrenalineIntensity = State.Adrenaline * 0.6f;
        OutWeights.Add(TEXT("NostrilFlare_L"), AdrenalineIntensity);
        OutWeights.Add(TEXT("NostrilFlare_R"), AdrenalineIntensity);
        OutWeights.Add(TEXT("EyeSquint_L"), AdrenalineIntensity * 0.3f);
        OutWeights.Add(TEXT("EyeSquint_R"), AdrenalineIntensity * 0.3f);

        // Map endorphins to subtle lip curl
        float EndorphinIntensity = State.Endorphins * 0.5f;
        OutWeights.Add(TEXT("LipCornerPull_L"), EndorphinIntensity);
        OutWeights.Add(TEXT("LipCornerPull_R"), EndorphinIntensity);
    }
    else if (DNAVersion == EDNAVersion::MH4)
    {
        // MH.4-specific neurochemical mappings
        // Map GABA to overall facial relaxation
        float RelaxationIntensity = State.GABA * 0.4f;
        OutWeights.Add(TEXT("FaceRelax"), RelaxationIntensity);

        // Map adrenaline to heightened expression
        float AdrenalineIntensity = State.Adrenaline * 0.6f;
        OutWeights.Add(TEXT("NoseWrinkle"), AdrenalineIntensity * 0.4f);
        OutWeights.Add(TEXT("EyeTension_L"), AdrenalineIntensity * 0.5f);
        OutWeights.Add(TEXT("EyeTension_R"), AdrenalineIntensity * 0.5f);

        // Map endorphins to contentment expression
        float EndorphinIntensity = State.Endorphins * 0.5f;
        OutWeights.Add(TEXT("CheekRaise_L"), EndorphinIntensity * 0.4f);
        OutWeights.Add(TEXT("CheekRaise_R"), EndorphinIntensity * 0.4f);
    }
}

void UMetaHumanDNABridge::MapEmotionalStateToBlendShapes(const FEmotionalState& Emotion, TMap<FString, float>& OutWeights)
{
    // Example mapping: Emotional state to facial expressions
    // This is a simplified example; real mapping would be more complex
    
    OutWeights.Empty();

    // Map valence to smile/frown
    if (Emotion.Valence > 0.0f)
    {
        OutWeights.Add(TEXT("Smile_L"), Emotion.Valence * Emotion.Intensity);
        OutWeights.Add(TEXT("Smile_R"), Emotion.Valence * Emotion.Intensity);
    }
    else
    {
        OutWeights.Add(TEXT("Frown_L"), -Emotion.Valence * Emotion.Intensity);
        OutWeights.Add(TEXT("Frown_R"), -Emotion.Valence * Emotion.Intensity);
    }

    // Map arousal to eye openness and brow raise
    OutWeights.Add(TEXT("EyeOpen_L"), Emotion.Arousal * 0.5f);
    OutWeights.Add(TEXT("EyeOpen_R"), Emotion.Arousal * 0.5f);
    OutWeights.Add(TEXT("BrowRaise_L"), Emotion.Arousal * 0.3f);
    OutWeights.Add(TEXT("BrowRaise_R"), Emotion.Arousal * 0.3f);

    // Map dominance to confident/submissive expressions
    if (Emotion.Dominance > 0.0f)
    {
        // Confident expressions: chin up, steady gaze
        OutWeights.Add(TEXT("ChinRaise"), Emotion.Dominance * 0.4f);
        OutWeights.Add(TEXT("EyeSquint_L"), Emotion.Dominance * 0.2f);
        OutWeights.Add(TEXT("EyeSquint_R"), Emotion.Dominance * 0.2f);
    }
    else
    {
        // Submissive expressions: brow lower, averted gaze
        OutWeights.Add(TEXT("BrowLower_L"), -Emotion.Dominance * 0.3f);
        OutWeights.Add(TEXT("BrowLower_R"), -Emotion.Dominance * 0.3f);
        OutWeights.Add(TEXT("EyeLookDown_L"), -Emotion.Dominance * 0.2f);
        OutWeights.Add(TEXT("EyeLookDown_R"), -Emotion.Dominance * 0.2f);
    }

    // Add version-specific emotional mappings
    if (DNAVersion == EDNAVersion::DHI)
    {
        // DHI supports more nuanced emotional expressions
        // Map surprise component
        if (Emotion.Arousal > 0.7f && Emotion.Valence > 0.0f)
        {
            float SurpriseIntensity = (Emotion.Arousal - 0.7f) * 3.0f * Emotion.Intensity;
            OutWeights.Add(TEXT("BrowInnerUp_L"), SurpriseIntensity * 0.8f);
            OutWeights.Add(TEXT("BrowInnerUp_R"), SurpriseIntensity * 0.8f);
            OutWeights.Add(TEXT("JawDrop"), SurpriseIntensity * 0.4f);
        }

        // Map fear component
        if (Emotion.Arousal > 0.5f && Emotion.Valence < -0.3f && Emotion.Dominance < 0.0f)
        {
            float FearIntensity = Emotion.Arousal * (-Emotion.Valence) * Emotion.Intensity;
            OutWeights.Add(TEXT("EyeWide_L"), FearIntensity * 0.6f);
            OutWeights.Add(TEXT("EyeWide_R"), FearIntensity * 0.6f);
            OutWeights.Add(TEXT("BrowInnerUp_L"), FearIntensity * 0.5f);
            OutWeights.Add(TEXT("BrowInnerUp_R"), FearIntensity * 0.5f);
            OutWeights.Add(TEXT("LipStretch_L"), FearIntensity * 0.3f);
            OutWeights.Add(TEXT("LipStretch_R"), FearIntensity * 0.3f);
        }

        // Map anger component
        if (Emotion.Arousal > 0.4f && Emotion.Valence < -0.2f && Emotion.Dominance > 0.0f)
        {
            float AngerIntensity = Emotion.Arousal * (-Emotion.Valence) * Emotion.Dominance * Emotion.Intensity;
            OutWeights.Add(TEXT("BrowLower_L"), AngerIntensity * 0.7f);
            OutWeights.Add(TEXT("BrowLower_R"), AngerIntensity * 0.7f);
            OutWeights.Add(TEXT("NoseWrinkle"), AngerIntensity * 0.5f);
            OutWeights.Add(TEXT("LipTighten_L"), AngerIntensity * 0.4f);
            OutWeights.Add(TEXT("LipTighten_R"), AngerIntensity * 0.4f);
        }

        // Map disgust component
        if (Emotion.Valence < -0.5f && Emotion.Arousal < 0.5f)
        {
            float DisgustIntensity = (-Emotion.Valence - 0.5f) * 2.0f * Emotion.Intensity;
            OutWeights.Add(TEXT("NoseWrinkle"), DisgustIntensity * 0.8f);
            OutWeights.Add(TEXT("UpperLipRaise_L"), DisgustIntensity * 0.6f);
            OutWeights.Add(TEXT("UpperLipRaise_R"), DisgustIntensity * 0.6f);
            OutWeights.Add(TEXT("CheekSquint_L"), DisgustIntensity * 0.3f);
            OutWeights.Add(TEXT("CheekSquint_R"), DisgustIntensity * 0.3f);
        }
    }
    else if (DNAVersion == EDNAVersion::MH4)
    {
        // MH.4 emotional expressions with different control scheme
        // Map composite emotions using MH.4 blend shape names
        if (Emotion.Arousal > 0.6f)
        {
            float ExcitementIntensity = (Emotion.Arousal - 0.6f) * 2.5f * Emotion.Intensity;
            OutWeights.Add(TEXT("BrowRaiseInner_L"), ExcitementIntensity * 0.6f);
            OutWeights.Add(TEXT("BrowRaiseInner_R"), ExcitementIntensity * 0.6f);
        }

        // Map negative valence expressions
        if (Emotion.Valence < -0.3f)
        {
            float DistressIntensity = (-Emotion.Valence - 0.3f) * 1.5f * Emotion.Intensity;
            OutWeights.Add(TEXT("BrowFurrow_L"), DistressIntensity * 0.5f);
            OutWeights.Add(TEXT("BrowFurrow_R"), DistressIntensity * 0.5f);
            OutWeights.Add(TEXT("LipCornerDepress_L"), DistressIntensity * 0.4f);
            OutWeights.Add(TEXT("LipCornerDepress_R"), DistressIntensity * 0.4f);
        }
    }
}

TMap<FString, FString> UMetaHumanDNABridge::GetDHIBlendShapeNames() const
{
    // DHI blend shape name mappings (Digital Human Interface standard)
    TMap<FString, FString> Mappings;

    // Mouth controls
    Mappings.Add(TEXT("Smile_L"), TEXT("CTRL_L_mouth_smile"));
    Mappings.Add(TEXT("Smile_R"), TEXT("CTRL_R_mouth_smile"));
    Mappings.Add(TEXT("Frown_L"), TEXT("CTRL_L_mouth_frown"));
    Mappings.Add(TEXT("Frown_R"), TEXT("CTRL_R_mouth_frown"));
    Mappings.Add(TEXT("LipCornerPull_L"), TEXT("CTRL_L_mouth_cornerPull"));
    Mappings.Add(TEXT("LipCornerPull_R"), TEXT("CTRL_R_mouth_cornerPull"));
    Mappings.Add(TEXT("LipCornerDepress_L"), TEXT("CTRL_L_mouth_cornerDepress"));
    Mappings.Add(TEXT("LipCornerDepress_R"), TEXT("CTRL_R_mouth_cornerDepress"));
    Mappings.Add(TEXT("LipStretch_L"), TEXT("CTRL_L_mouth_stretch"));
    Mappings.Add(TEXT("LipStretch_R"), TEXT("CTRL_R_mouth_stretch"));
    Mappings.Add(TEXT("LipTighten_L"), TEXT("CTRL_L_mouth_tighten"));
    Mappings.Add(TEXT("LipTighten_R"), TEXT("CTRL_R_mouth_tighten"));
    Mappings.Add(TEXT("UpperLipRaise_L"), TEXT("CTRL_L_mouth_upperLipRaise"));
    Mappings.Add(TEXT("UpperLipRaise_R"), TEXT("CTRL_R_mouth_upperLipRaise"));
    Mappings.Add(TEXT("LowerLipDepress_L"), TEXT("CTRL_L_mouth_lowerLipDepress"));
    Mappings.Add(TEXT("LowerLipDepress_R"), TEXT("CTRL_R_mouth_lowerLipDepress"));
    Mappings.Add(TEXT("JawOpen"), TEXT("CTRL_C_jaw_open"));
    Mappings.Add(TEXT("JawDrop"), TEXT("CTRL_C_jaw_drop"));
    Mappings.Add(TEXT("JawRelax"), TEXT("CTRL_C_jaw_relax"));

    // Eye controls
    Mappings.Add(TEXT("EyeOpen_L"), TEXT("CTRL_L_eye_open"));
    Mappings.Add(TEXT("EyeOpen_R"), TEXT("CTRL_R_eye_open"));
    Mappings.Add(TEXT("EyeBlink_L"), TEXT("CTRL_L_eye_blink"));
    Mappings.Add(TEXT("EyeBlink_R"), TEXT("CTRL_R_eye_blink"));
    Mappings.Add(TEXT("EyeWide_L"), TEXT("CTRL_L_eye_wide"));
    Mappings.Add(TEXT("EyeWide_R"), TEXT("CTRL_R_eye_wide"));
    Mappings.Add(TEXT("EyeSquint_L"), TEXT("CTRL_L_eye_squint"));
    Mappings.Add(TEXT("EyeSquint_R"), TEXT("CTRL_R_eye_squint"));
    Mappings.Add(TEXT("EyeWarmth_L"), TEXT("CTRL_L_eye_warmth"));
    Mappings.Add(TEXT("EyeWarmth_R"), TEXT("CTRL_R_eye_warmth"));
    Mappings.Add(TEXT("EyeLookDown_L"), TEXT("CTRL_L_eye_lookDown"));
    Mappings.Add(TEXT("EyeLookDown_R"), TEXT("CTRL_R_eye_lookDown"));
    Mappings.Add(TEXT("EyeLookUp_L"), TEXT("CTRL_L_eye_lookUp"));
    Mappings.Add(TEXT("EyeLookUp_R"), TEXT("CTRL_R_eye_lookUp"));
    Mappings.Add(TEXT("EyeLookIn_L"), TEXT("CTRL_L_eye_lookIn"));
    Mappings.Add(TEXT("EyeLookIn_R"), TEXT("CTRL_R_eye_lookIn"));
    Mappings.Add(TEXT("EyeLookOut_L"), TEXT("CTRL_L_eye_lookOut"));
    Mappings.Add(TEXT("EyeLookOut_R"), TEXT("CTRL_R_eye_lookOut"));

    // Brow controls
    Mappings.Add(TEXT("BrowRaise_L"), TEXT("CTRL_L_brow_raise"));
    Mappings.Add(TEXT("BrowRaise_R"), TEXT("CTRL_R_brow_raise"));
    Mappings.Add(TEXT("BrowLower_L"), TEXT("CTRL_L_brow_lower"));
    Mappings.Add(TEXT("BrowLower_R"), TEXT("CTRL_R_brow_lower"));
    Mappings.Add(TEXT("BrowInnerUp_L"), TEXT("CTRL_L_brow_innerUp"));
    Mappings.Add(TEXT("BrowInnerUp_R"), TEXT("CTRL_R_brow_innerUp"));
    Mappings.Add(TEXT("BrowOuterUp_L"), TEXT("CTRL_L_brow_outerUp"));
    Mappings.Add(TEXT("BrowOuterUp_R"), TEXT("CTRL_R_brow_outerUp"));
    Mappings.Add(TEXT("BrowTension_L"), TEXT("CTRL_L_brow_tension"));
    Mappings.Add(TEXT("BrowTension_R"), TEXT("CTRL_R_brow_tension"));

    // Cheek and nose controls
    Mappings.Add(TEXT("CheekRaise_L"), TEXT("CTRL_L_cheek_raise"));
    Mappings.Add(TEXT("CheekRaise_R"), TEXT("CTRL_R_cheek_raise"));
    Mappings.Add(TEXT("CheekSquint_L"), TEXT("CTRL_L_cheek_squint"));
    Mappings.Add(TEXT("CheekSquint_R"), TEXT("CTRL_R_cheek_squint"));
    Mappings.Add(TEXT("CheekPuff_L"), TEXT("CTRL_L_cheek_puff"));
    Mappings.Add(TEXT("CheekPuff_R"), TEXT("CTRL_R_cheek_puff"));
    Mappings.Add(TEXT("NoseWrinkle"), TEXT("CTRL_C_nose_wrinkle"));
    Mappings.Add(TEXT("NostrilFlare_L"), TEXT("CTRL_L_nose_flare"));
    Mappings.Add(TEXT("NostrilFlare_R"), TEXT("CTRL_R_nose_flare"));

    // Chin and neck controls
    Mappings.Add(TEXT("ChinRaise"), TEXT("CTRL_C_chin_raise"));
    Mappings.Add(TEXT("ChinLower"), TEXT("CTRL_C_chin_lower"));
    Mappings.Add(TEXT("NeckTense"), TEXT("CTRL_C_neck_tense"));

    return Mappings;
}

TMap<FString, FString> UMetaHumanDNABridge::GetMH4BlendShapeNames() const
{
    // MH.4 blend shape name mappings (MetaHuman Creator 2023+ format)
    TMap<FString, FString> Mappings;

    // Mouth controls - MH.4 uses slightly different naming convention
    Mappings.Add(TEXT("Smile_L"), TEXT("CTRL_L_mouth_smile"));
    Mappings.Add(TEXT("Smile_R"), TEXT("CTRL_R_mouth_smile"));
    Mappings.Add(TEXT("Frown_L"), TEXT("CTRL_L_mouth_frown"));
    Mappings.Add(TEXT("Frown_R"), TEXT("CTRL_R_mouth_frown"));
    Mappings.Add(TEXT("LipCornerPull_L"), TEXT("CTRL_L_mouth_lipCornerPull"));
    Mappings.Add(TEXT("LipCornerPull_R"), TEXT("CTRL_R_mouth_lipCornerPull"));
    Mappings.Add(TEXT("LipCornerDepress_L"), TEXT("CTRL_L_mouth_lipCornerDepress"));
    Mappings.Add(TEXT("LipCornerDepress_R"), TEXT("CTRL_R_mouth_lipCornerDepress"));
    Mappings.Add(TEXT("LipStretch_L"), TEXT("CTRL_L_mouth_lipStretch"));
    Mappings.Add(TEXT("LipStretch_R"), TEXT("CTRL_R_mouth_lipStretch"));
    Mappings.Add(TEXT("LipTighten_L"), TEXT("CTRL_L_mouth_lipTighten"));
    Mappings.Add(TEXT("LipTighten_R"), TEXT("CTRL_R_mouth_lipTighten"));
    Mappings.Add(TEXT("UpperLipRaise_L"), TEXT("CTRL_L_mouth_upperLipRaise"));
    Mappings.Add(TEXT("UpperLipRaise_R"), TEXT("CTRL_R_mouth_upperLipRaise"));
    Mappings.Add(TEXT("LowerLipDepress_L"), TEXT("CTRL_L_mouth_lowerLipDepress"));
    Mappings.Add(TEXT("LowerLipDepress_R"), TEXT("CTRL_R_mouth_lowerLipDepress"));
    Mappings.Add(TEXT("LipPucker"), TEXT("CTRL_C_mouth_lipPucker"));
    Mappings.Add(TEXT("LipFunnel"), TEXT("CTRL_C_mouth_lipFunnel"));
    Mappings.Add(TEXT("JawOpen"), TEXT("CTRL_C_jaw_open"));
    Mappings.Add(TEXT("JawDrop"), TEXT("CTRL_C_jaw_drop"));
    Mappings.Add(TEXT("FaceRelax"), TEXT("CTRL_C_face_relax"));

    // Eye controls - MH.4 specific
    Mappings.Add(TEXT("EyeOpen_L"), TEXT("CTRL_L_eye_eyelidOpen"));
    Mappings.Add(TEXT("EyeOpen_R"), TEXT("CTRL_R_eye_eyelidOpen"));
    Mappings.Add(TEXT("EyeBlink_L"), TEXT("CTRL_L_eye_blink"));
    Mappings.Add(TEXT("EyeBlink_R"), TEXT("CTRL_R_eye_blink"));
    Mappings.Add(TEXT("EyeWide_L"), TEXT("CTRL_L_eye_eyelidWide"));
    Mappings.Add(TEXT("EyeWide_R"), TEXT("CTRL_R_eye_eyelidWide"));
    Mappings.Add(TEXT("EyeSquint_L"), TEXT("CTRL_L_eye_squint"));
    Mappings.Add(TEXT("EyeSquint_R"), TEXT("CTRL_R_eye_squint"));
    Mappings.Add(TEXT("EyeTension_L"), TEXT("CTRL_L_eye_tension"));
    Mappings.Add(TEXT("EyeTension_R"), TEXT("CTRL_R_eye_tension"));
    Mappings.Add(TEXT("EyeLookDown_L"), TEXT("CTRL_L_eye_lookDown"));
    Mappings.Add(TEXT("EyeLookDown_R"), TEXT("CTRL_R_eye_lookDown"));
    Mappings.Add(TEXT("EyeLookUp_L"), TEXT("CTRL_L_eye_lookUp"));
    Mappings.Add(TEXT("EyeLookUp_R"), TEXT("CTRL_R_eye_lookUp"));
    Mappings.Add(TEXT("EyeLookIn_L"), TEXT("CTRL_L_eye_lookIn"));
    Mappings.Add(TEXT("EyeLookIn_R"), TEXT("CTRL_R_eye_lookIn"));
    Mappings.Add(TEXT("EyeLookOut_L"), TEXT("CTRL_L_eye_lookOut"));
    Mappings.Add(TEXT("EyeLookOut_R"), TEXT("CTRL_R_eye_lookOut"));

    // Brow controls - MH.4 specific naming
    Mappings.Add(TEXT("BrowRaise_L"), TEXT("CTRL_L_brow_up"));
    Mappings.Add(TEXT("BrowRaise_R"), TEXT("CTRL_R_brow_up"));
    Mappings.Add(TEXT("BrowLower_L"), TEXT("CTRL_L_brow_down"));
    Mappings.Add(TEXT("BrowLower_R"), TEXT("CTRL_R_brow_down"));
    Mappings.Add(TEXT("BrowRaiseInner_L"), TEXT("CTRL_L_brow_innerUp"));
    Mappings.Add(TEXT("BrowRaiseInner_R"), TEXT("CTRL_R_brow_innerUp"));
    Mappings.Add(TEXT("BrowRaiseOuter_L"), TEXT("CTRL_L_brow_outerUp"));
    Mappings.Add(TEXT("BrowRaiseOuter_R"), TEXT("CTRL_R_brow_outerUp"));
    Mappings.Add(TEXT("BrowFurrow_L"), TEXT("CTRL_L_brow_furrow"));
    Mappings.Add(TEXT("BrowFurrow_R"), TEXT("CTRL_R_brow_furrow"));
    Mappings.Add(TEXT("BrowTension_L"), TEXT("CTRL_L_brow_tension"));
    Mappings.Add(TEXT("BrowTension_R"), TEXT("CTRL_R_brow_tension"));

    // Cheek and nose controls - MH.4 specific
    Mappings.Add(TEXT("CheekRaise_L"), TEXT("CTRL_L_cheek_cheekRaise"));
    Mappings.Add(TEXT("CheekRaise_R"), TEXT("CTRL_R_cheek_cheekRaise"));
    Mappings.Add(TEXT("CheekSquint_L"), TEXT("CTRL_L_cheek_squint"));
    Mappings.Add(TEXT("CheekSquint_R"), TEXT("CTRL_R_cheek_squint"));
    Mappings.Add(TEXT("CheekPuff_L"), TEXT("CTRL_L_cheek_puff"));
    Mappings.Add(TEXT("CheekPuff_R"), TEXT("CTRL_R_cheek_puff"));
    Mappings.Add(TEXT("NoseWrinkle"), TEXT("CTRL_C_nose_wrinkle"));
    Mappings.Add(TEXT("NostrilFlare_L"), TEXT("CTRL_L_nose_nostrilFlare"));
    Mappings.Add(TEXT("NostrilFlare_R"), TEXT("CTRL_R_nose_nostrilFlare"));

    // Chin and additional controls
    Mappings.Add(TEXT("ChinRaise"), TEXT("CTRL_C_chin_chinUp"));
    Mappings.Add(TEXT("ChinLower"), TEXT("CTRL_C_chin_chinDown"));
    Mappings.Add(TEXT("DimpleL"), TEXT("CTRL_L_mouth_dimple"));
    Mappings.Add(TEXT("DimpleR"), TEXT("CTRL_R_mouth_dimple"));

    return Mappings;
}

// Copyright Deep Tree Echo. All Rights Reserved.

#include "TypeConversionRegistry.h"
#include "Algo/Accumulate.h"

// Initialize singleton instance
UTypeConversionRegistry* UTypeConversionRegistry::Instance = nullptr;

UTypeConversionRegistry::UTypeConversionRegistry()
{
}

UTypeConversionRegistry* UTypeConversionRegistry::Get()
{
    if (!Instance)
    {
        Instance = NewObject<UTypeConversionRegistry>();
        Instance->AddToRoot(); // Prevent garbage collection
        Instance->RegisterBuiltInTypes();
    }
    return Instance;
}

void UTypeConversionRegistry::RegisterTypeMapping(
    const FString& SourceType,
    const FString& TargetType,
    EConversionDirection Direction,
    EConversionQuality Quality,
    int32 SourceDim,
    int32 TargetDim,
    bool bZeroCopy)
{
    FString Key = MakeMappingKey(SourceType, TargetType);
    
    FTypeMappingMetadata Metadata;
    Metadata.SourceTypeName = SourceType;
    Metadata.TargetTypeName = TargetType;
    Metadata.Direction = Direction;
    Metadata.Quality = Quality;
    Metadata.bIsSupported = true;
    Metadata.SourceDimension = SourceDim;
    Metadata.TargetDimension = TargetDim;
    Metadata.bIsZeroCopyPossible = bZeroCopy;
    Metadata.AverageConversionTimeMicroseconds = 0.0f;
    
    TypeMappings.Add(Key, Metadata);
    
    // Update reverse lookup
    if (!ReverseTypeMappings.Contains(TargetType))
    {
        ReverseTypeMappings.Add(TargetType, TArray<FString>());
    }
    ReverseTypeMappings[TargetType].AddUnique(SourceType);
    
    // Register reverse mapping if bidirectional
    if (Direction == EConversionDirection::Bidirectional)
    {
        FString ReverseKey = MakeMappingKey(TargetType, SourceType);
        FTypeMappingMetadata ReverseMetadata = Metadata;
        ReverseMetadata.SourceTypeName = TargetType;
        ReverseMetadata.TargetTypeName = SourceType;
        ReverseMetadata.SourceDimension = TargetDim;
        ReverseMetadata.TargetDimension = SourceDim;
        
        TypeMappings.Add(ReverseKey, ReverseMetadata);
        
        if (!ReverseTypeMappings.Contains(SourceType))
        {
            ReverseTypeMappings.Add(SourceType, TArray<FString>());
        }
        ReverseTypeMappings[SourceType].AddUnique(TargetType);
    }
}

void UTypeConversionRegistry::RegisterBuiltInTypes()
{
    RegisterUEMathTypes();
    RegisterUEColorTypes();
    RegisterUETransformTypes();
    RegisterEigenVectorTypes();
    RegisterEigenMatrixTypes();
    RegisterTensorTypes();
}

void UTypeConversionRegistry::RegisterUEMathTypes()
{
    // FVector ↔ Eigen::Vector3f
    RegisterTypeMapping(
        TEXT("FVector"),
        TEXT("Eigen::Vector3f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        3, 3, false
    );
    
    // FVector2D ↔ Eigen::Vector2f
    RegisterTypeMapping(
        TEXT("FVector2D"),
        TEXT("Eigen::Vector2f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        2, 2, false
    );
    
    // FVector4 ↔ Eigen::Vector4f
    RegisterTypeMapping(
        TEXT("FVector4"),
        TEXT("Eigen::Vector4f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        4, 4, false
    );
    
    // FRotator ↔ Eigen::Vector3f (Pitch, Yaw, Roll)
    RegisterTypeMapping(
        TEXT("FRotator"),
        TEXT("Eigen::Vector3f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        3, 3, false
    );
    
    // FQuat ↔ Eigen::Vector4f (X, Y, Z, W)
    RegisterTypeMapping(
        TEXT("FQuat"),
        TEXT("Eigen::Vector4f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        4, 4, false
    );
    
    // FQuat ↔ Eigen::Quaternionf
    RegisterTypeMapping(
        TEXT("FQuat"),
        TEXT("Eigen::Quaternionf"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        4, 4, false
    );
}

void UTypeConversionRegistry::RegisterUEColorTypes()
{
    // FLinearColor ↔ Eigen::Vector4f (R, G, B, A)
    RegisterTypeMapping(
        TEXT("FLinearColor"),
        TEXT("Eigen::Vector4f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        4, 4, false
    );
    
    // FColor ↔ Eigen::Vector4i (R, G, B, A as uint8)
    RegisterTypeMapping(
        TEXT("FColor"),
        TEXT("Eigen::Vector4i"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        4, 4, false
    );
    
    // FColor ↔ Eigen::Vector4f (normalized)
    RegisterTypeMapping(
        TEXT("FColor"),
        TEXT("Eigen::Vector4f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::HighPrecision,
        4, 4, false
    );
}

void UTypeConversionRegistry::RegisterUETransformTypes()
{
    // FTransform ↔ Eigen::Matrix4f (4x4 transformation matrix)
    RegisterTypeMapping(
        TEXT("FTransform"),
        TEXT("Eigen::Matrix4f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::HighPrecision,
        16, 16, false
    );
    
    // FMatrix ↔ Eigen::Matrix4f
    RegisterTypeMapping(
        TEXT("FMatrix"),
        TEXT("Eigen::Matrix4f"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        16, 16, false
    );
}

void UTypeConversionRegistry::RegisterEigenVectorTypes()
{
    // Eigen vectors to TArray<float>
    RegisterTypeMapping(
        TEXT("Eigen::VectorXf"),
        TEXT("TArray<float>"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        -1, -1, false // Dynamic size
    );
    
    // Fixed-size Eigen vectors to TArray<float>
    for (int32 Size = 2; Size <= 16; ++Size)
    {
        FString EigenType = FString::Printf(TEXT("Eigen::Vector%df"), Size);
        RegisterTypeMapping(
            EigenType,
            TEXT("TArray<float>"),
            EConversionDirection::Bidirectional,
            EConversionQuality::Lossless,
            Size, Size, false
        );
    }
}

void UTypeConversionRegistry::RegisterEigenMatrixTypes()
{
    // Eigen::MatrixXf ↔ TArray<TArray<float>>
    RegisterTypeMapping(
        TEXT("Eigen::MatrixXf"),
        TEXT("TArray<TArray<float>>"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        -1, -1, false // Dynamic size
    );
    
    // Fixed-size matrices
    const int32 CommonSizes[] = { 2, 3, 4, 8, 16 };
    for (int32 Rows : CommonSizes)
    {
        for (int32 Cols : CommonSizes)
        {
            FString EigenType = FString::Printf(TEXT("Eigen::Matrix%dx%df"), Rows, Cols);
            RegisterTypeMapping(
                EigenType,
                TEXT("TArray<TArray<float>>"),
                EConversionDirection::Bidirectional,
                EConversionQuality::Lossless,
                Rows * Cols, Rows * Cols, false
            );
        }
    }
}

void UTypeConversionRegistry::RegisterTensorTypes()
{
    // Activation maps (neural outputs)
    RegisterTypeMapping(
        TEXT("TArray<float>"),
        TEXT("ActivationMap"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        -1, -1, true // Zero-copy possible with move semantics
    );
    
    // Multi-dimensional tensors
    RegisterTypeMapping(
        TEXT("TArray<TArray<TArray<float>>>"),
        TEXT("Tensor3D"),
        EConversionDirection::Bidirectional,
        EConversionQuality::Lossless,
        -1, -1, false
    );
}

bool UTypeConversionRegistry::IsConversionSupported(const FString& SourceType, const FString& TargetType) const
{
    FString Key = MakeMappingKey(SourceType, TargetType);
    return TypeMappings.Contains(Key);
}

FTypeMappingMetadata UTypeConversionRegistry::GetConversionMetadata(const FString& SourceType, const FString& TargetType) const
{
    FString Key = MakeMappingKey(SourceType, TargetType);
    if (const FTypeMappingMetadata* Found = TypeMappings.Find(Key))
    {
        return *Found;
    }
    
    // Return unsupported metadata
    FTypeMappingMetadata Metadata;
    Metadata.SourceTypeName = SourceType;
    Metadata.TargetTypeName = TargetType;
    Metadata.bIsSupported = false;
    return Metadata;
}

TArray<FString> UTypeConversionRegistry::GetSupportedUETypes() const
{
    TArray<FString> UETypes;
    
    // Common UE types
    UETypes.Add(TEXT("FVector"));
    UETypes.Add(TEXT("FVector2D"));
    UETypes.Add(TEXT("FVector4"));
    UETypes.Add(TEXT("FRotator"));
    UETypes.Add(TEXT("FQuat"));
    UETypes.Add(TEXT("FTransform"));
    UETypes.Add(TEXT("FMatrix"));
    UETypes.Add(TEXT("FLinearColor"));
    UETypes.Add(TEXT("FColor"));
    UETypes.Add(TEXT("TArray<float>"));
    
    return UETypes;
}

TArray<FString> UTypeConversionRegistry::GetSupportedNeuralTypes() const
{
    TArray<FString> NeuralTypes;
    
    // Common Eigen types
    NeuralTypes.Add(TEXT("Eigen::Vector2f"));
    NeuralTypes.Add(TEXT("Eigen::Vector3f"));
    NeuralTypes.Add(TEXT("Eigen::Vector4f"));
    NeuralTypes.Add(TEXT("Eigen::VectorXf"));
    NeuralTypes.Add(TEXT("Eigen::Matrix4f"));
    NeuralTypes.Add(TEXT("Eigen::MatrixXf"));
    NeuralTypes.Add(TEXT("Eigen::Quaternionf"));
    
    // Tensor types
    NeuralTypes.Add(TEXT("ActivationMap"));
    NeuralTypes.Add(TEXT("Tensor3D"));
    
    return NeuralTypes;
}

EConversionQuality UTypeConversionRegistry::GetConversionQuality(const FString& SourceType, const FString& TargetType) const
{
    FTypeMappingMetadata Metadata = GetConversionMetadata(SourceType, TargetType);
    return Metadata.Quality;
}

bool UTypeConversionRegistry::IsConversionLossless(const FString& SourceType, const FString& TargetType) const
{
    return GetConversionQuality(SourceType, TargetType) == EConversionQuality::Lossless;
}

void UTypeConversionRegistry::UpdateConversionMetrics(const FString& SourceType, const FString& TargetType, float TimeMicroseconds)
{
    FString Key = MakeMappingKey(SourceType, TargetType);
    
    if (FTypeMappingMetadata* Metadata = TypeMappings.Find(Key))
    {
        // Add timing sample
        if (!TimingSamples.Contains(Key))
        {
            TimingSamples.Add(Key, TArray<float>());
        }
        
        TArray<float>& Samples = TimingSamples[Key];
        Samples.Add(TimeMicroseconds);
        
        // Keep only recent samples
        if (Samples.Num() > MaxTimingSamples)
        {
            Samples.RemoveAt(0);
        }
        
        // Update average
        float Sum = Algo::Accumulate(Samples, 0.0f);
        Metadata->AverageConversionTimeMicroseconds = Sum / Samples.Num();
    }
}

TMap<FString, FTypeMappingMetadata> UTypeConversionRegistry::GetConversionStatistics() const
{
    return TypeMappings;
}

void UTypeConversionRegistry::ResetMetrics()
{
    TimingSamples.Empty();
    
    for (auto& Pair : TypeMappings)
    {
        Pair.Value.AverageConversionTimeMicroseconds = 0.0f;
    }
}

FString UTypeConversionRegistry::InferTargetType(const FString& SourceType, EConversionDirection Direction) const
{
    // Find the first supported conversion in the desired direction
    for (const auto& Pair : TypeMappings)
    {
        const FTypeMappingMetadata& Metadata = Pair.Value;
        
        if (Direction == EConversionDirection::UEToNeural)
        {
            // Look for UE → Neural conversions
            if (Metadata.SourceTypeName == SourceType &&
                (Metadata.Direction == EConversionDirection::UEToNeural ||
                 Metadata.Direction == EConversionDirection::Bidirectional))
            {
                return Metadata.TargetTypeName;
            }
        }
        else if (Direction == EConversionDirection::NeuralToUE)
        {
            // Look for Neural → UE conversions
            if (Metadata.SourceTypeName == SourceType &&
                (Metadata.Direction == EConversionDirection::NeuralToUE ||
                 Metadata.Direction == EConversionDirection::Bidirectional))
            {
                return Metadata.TargetTypeName;
            }
        }
    }
    
    return TEXT("");
}

TArray<FString> UTypeConversionRegistry::GetConversionPath(const FString& SourceType, const FString& TargetType) const
{
    TArray<FString> Path;
    
    // Check for direct conversion
    if (IsConversionSupported(SourceType, TargetType))
    {
        Path.Add(SourceType);
        Path.Add(TargetType);
        return Path;
    }
    
    // For now, only support direct conversions
    // Future: implement multi-hop conversion path finding
    
    return Path;
}

FString UTypeConversionRegistry::MakeMappingKey(const FString& SourceType, const FString& TargetType) const
{
    return FString::Printf(TEXT("%s->%s"), *SourceType, *TargetType);
}

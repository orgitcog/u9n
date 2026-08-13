#pragma once

/**
 * Type Conversion Registry
 * 
 * Central registry for type mappings between Unreal Engine types and neural computing types.
 * Provides automatic type inference and maintains conversion metadata for optimization.
 * 
 * Feature: F1.1.4 - Type Conversion System
 * Phase: 1.1 - Neural-Symbolic Bridge Architecture
 * Epic: E1 - Foundation & Core Integration
 */

#include "CoreMinimal.h"
#include "TypeConversionRegistry.generated.h"

/**
 * Type Conversion Direction
 */
UENUM(BlueprintType)
enum class EConversionDirection : uint8
{
    /** Unreal Engine → Eigen/Neural */
    UEToNeural UMETA(DisplayName = "UE to Neural"),
    
    /** Eigen/Neural → Unreal Engine */
    NeuralToUE UMETA(DisplayName = "Neural to UE"),
    
    /** Bidirectional (both directions supported) */
    Bidirectional UMETA(DisplayName = "Bidirectional")
};

/**
 * Type Conversion Quality
 */
UENUM(BlueprintType)
enum class EConversionQuality : uint8
{
    /** Lossless conversion (exact representation) */
    Lossless UMETA(DisplayName = "Lossless"),
    
    /** High precision (minimal loss) */
    HighPrecision UMETA(DisplayName = "High Precision"),
    
    /** Normal precision (acceptable loss) */
    NormalPrecision UMETA(DisplayName = "Normal Precision"),
    
    /** Low precision (significant loss) */
    LowPrecision UMETA(DisplayName = "Low Precision")
};

/**
 * Type Mapping Metadata
 * Stores information about a type conversion
 */
USTRUCT(BlueprintType)
struct FTypeMappingMetadata
{
    GENERATED_BODY()

    /** Source type name */
    UPROPERTY(BlueprintReadWrite)
    FString SourceTypeName;

    /** Target type name */
    UPROPERTY(BlueprintReadWrite)
    FString TargetTypeName;

    /** Conversion direction */
    UPROPERTY(BlueprintReadWrite)
    EConversionDirection Direction = EConversionDirection::Bidirectional;

    /** Conversion quality */
    UPROPERTY(BlueprintReadWrite)
    EConversionQuality Quality = EConversionQuality::Lossless;

    /** Is conversion supported */
    UPROPERTY(BlueprintReadWrite)
    bool bIsSupported = false;

    /** Source dimension (number of components) */
    UPROPERTY(BlueprintReadWrite)
    int32 SourceDimension = 0;

    /** Target dimension */
    UPROPERTY(BlueprintReadWrite)
    int32 TargetDimension = 0;

    /** Is zero-copy possible */
    UPROPERTY(BlueprintReadWrite)
    bool bIsZeroCopyPossible = false;

    /** Average conversion time (microseconds) */
    UPROPERTY(BlueprintReadWrite)
    float AverageConversionTimeMicroseconds = 0.0f;
};

/**
 * Type Conversion Registry
 * Singleton registry for managing type conversions
 */
UCLASS()
class DEEPTREEECHO_API UTypeConversionRegistry : public UObject
{
    GENERATED_BODY()

public:
    UTypeConversionRegistry();

    // ========================================
    // SINGLETON ACCESS
    // ========================================

    /** Get the global type conversion registry instance */
    static UTypeConversionRegistry* Get();

    // ========================================
    // REGISTRATION
    // ========================================

    /** Register a type conversion mapping */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    void RegisterTypeMapping(
        const FString& SourceType,
        const FString& TargetType,
        EConversionDirection Direction,
        EConversionQuality Quality,
        int32 SourceDim,
        int32 TargetDim,
        bool bZeroCopy = false
    );

    /** Register all built-in type mappings */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    void RegisterBuiltInTypes();

    // ========================================
    // QUERY
    // ========================================

    /** Check if conversion is supported */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    bool IsConversionSupported(const FString& SourceType, const FString& TargetType) const;

    /** Get conversion metadata */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    FTypeMappingMetadata GetConversionMetadata(const FString& SourceType, const FString& TargetType) const;

    /** Get all supported UE types */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    TArray<FString> GetSupportedUETypes() const;

    /** Get all supported neural types */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    TArray<FString> GetSupportedNeuralTypes() const;

    /** Get conversion quality */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    EConversionQuality GetConversionQuality(const FString& SourceType, const FString& TargetType) const;

    /** Check if conversion is lossless */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    bool IsConversionLossless(const FString& SourceType, const FString& TargetType) const;

    // ========================================
    // METRICS
    // ========================================

    /** Update conversion timing metrics */
    void UpdateConversionMetrics(const FString& SourceType, const FString& TargetType, float TimeMicroseconds);

    /** Get conversion statistics */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    TMap<FString, FTypeMappingMetadata> GetConversionStatistics() const;

    /** Reset all metrics */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    void ResetMetrics();

    // ========================================
    // TYPE INFERENCE
    // ========================================

    /** Infer target type from source type */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    FString InferTargetType(const FString& SourceType, EConversionDirection Direction) const;

    /** Get recommended conversion path (may involve intermediate types) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Registry")
    TArray<FString> GetConversionPath(const FString& SourceType, const FString& TargetType) const;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Type mapping registry (key: "SourceType->TargetType") */
    TMap<FString, FTypeMappingMetadata> TypeMappings;

    /** Reverse lookup map (TargetType -> SourceTypes) */
    TMap<FString, TArray<FString>> ReverseTypeMappings;

    /** Conversion timing samples for metrics */
    TMap<FString, TArray<float>> TimingSamples;

    /** Maximum timing samples to keep */
    static constexpr int32 MaxTimingSamples = 100;

    /** Singleton instance */
    static UTypeConversionRegistry* Instance;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Generate mapping key */
    FString MakeMappingKey(const FString& SourceType, const FString& TargetType) const;

    /** Register UE math types */
    void RegisterUEMathTypes();

    /** Register UE color types */
    void RegisterUEColorTypes();

    /** Register UE transform types */
    void RegisterUETransformTypes();

    /** Register Eigen vector types */
    void RegisterEigenVectorTypes();

    /** Register Eigen matrix types */
    void RegisterEigenMatrixTypes();

    /** Register tensor types */
    void RegisterTensorTypes();
};

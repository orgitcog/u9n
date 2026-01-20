#pragma once

/**
 * Type Converter
 * 
 * Main interface for type conversion with automatic type inference,
 * validation, and error handling. Provides high-level API for converting
 * between Unreal Engine types and neural computing types.
 * 
 * Feature: F1.1.4 - Type Conversion System
 * Phase: 1.1 - Neural-Symbolic Bridge Architecture
 * Epic: E1 - Foundation & Core Integration
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TypeConversionRegistry.h"
#include "UEToEigenConverter.h"
#include "EigenToUEConverter.h"
#include "TypeConverter.generated.h"

/**
 * Type Conversion Result
 * Indicates success/failure of a conversion operation
 */
UENUM(BlueprintType)
enum class EConversionResult : uint8
{
    /** Conversion successful */
    Success UMETA(DisplayName = "Success"),
    
    /** Conversion not supported */
    NotSupported UMETA(DisplayName = "Not Supported"),
    
    /** Validation failed (NaN, Inf, out of range) */
    ValidationFailed UMETA(DisplayName = "Validation Failed"),
    
    /** Dimension mismatch */
    DimensionMismatch UMETA(DisplayName = "Dimension Mismatch"),
    
    /** Unknown error */
    UnknownError UMETA(DisplayName = "Unknown Error")
};

/**
 * Type Conversion Statistics
 * Tracks conversion performance metrics
 */
USTRUCT(BlueprintType)
struct FTypeConversionStats
{
    GENERATED_BODY()

    /** Total conversions performed */
    UPROPERTY(BlueprintReadWrite)
    int64 TotalConversions = 0;

    /** Successful conversions */
    UPROPERTY(BlueprintReadWrite)
    int64 SuccessfulConversions = 0;

    /** Failed conversions */
    UPROPERTY(BlueprintReadWrite)
    int64 FailedConversions = 0;

    /** Average conversion time (microseconds) */
    UPROPERTY(BlueprintReadWrite)
    float AverageTimeMicroseconds = 0.0f;

    /** Peak conversion time (microseconds) */
    UPROPERTY(BlueprintReadWrite)
    float PeakTimeMicroseconds = 0.0f;

    /** Total conversion time (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float TotalTimeSeconds = 0.0f;
};

/**
 * Type Converter Component
 * High-level interface for type conversions
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UTypeConverter : public UActorComponent
{
    GENERATED_BODY()

public:
    UTypeConverter();

    virtual void BeginPlay() override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic type inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeConversion|Config")
    bool bEnableTypeInference = true;

    /** Enable validation before conversion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeConversion|Config")
    bool bEnableValidation = true;

    /** Enable performance tracking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeConversion|Config")
    bool bEnablePerformanceTracking = true;

    /** Log conversion warnings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeConversion|Config")
    bool bLogWarnings = true;

    /** Log conversion errors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TypeConversion|Config")
    bool bLogErrors = true;

    // ========================================
    // STATE
    // ========================================

    /** Conversion statistics */
    UPROPERTY(BlueprintReadOnly, Category = "TypeConversion|Stats")
    FTypeConversionStats ConversionStats;

    /** Last conversion result */
    UPROPERTY(BlueprintReadOnly, Category = "TypeConversion|Stats")
    EConversionResult LastConversionResult = EConversionResult::Success;

    /** Last error message */
    UPROPERTY(BlueprintReadOnly, Category = "TypeConversion|Stats")
    FString LastErrorMessage;

    // ========================================
    // VECTOR CONVERSIONS (UE → EIGEN)
    // ========================================

    /** Convert FVector to Eigen::Vector3f */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertVectorToEigen(const FVector& Vector, bool& bSuccess);

    /** Convert FVector2D to Eigen::Vector2f */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertVector2DToEigen(const FVector2D& Vector, bool& bSuccess);

    /** Convert FVector4 to Eigen::Vector4f */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertVector4ToEigen(const FVector4& Vector, bool& bSuccess);

    /** Convert TArray<float> to Eigen::VectorXf */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertFloatArrayToEigen(const TArray<float>& Array, bool& bSuccess);

    // ========================================
    // VECTOR CONVERSIONS (EIGEN → UE)
    // ========================================

    /** Convert Eigen::Vector3f to FVector */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FVector ConvertEigenToVector(const TArray<float>& EigenVector, bool& bSuccess);

    /** Convert Eigen::Vector2f to FVector2D */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FVector2D ConvertEigenToVector2D(const TArray<float>& EigenVector, bool& bSuccess);

    /** Convert Eigen::Vector4f to FVector4 */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FVector4 ConvertEigenToVector4(const TArray<float>& EigenVector, bool& bSuccess);

    // ========================================
    // ROTATION CONVERSIONS
    // ========================================

    /** Convert FRotator to Eigen::Vector3f */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertRotatorToEigen(const FRotator& Rotator, bool& bSuccess);

    /** Convert Eigen::Vector3f to FRotator */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FRotator ConvertEigenToRotator(const TArray<float>& EigenVector, bool& bSuccess);

    /** Convert FQuat to Eigen::Vector4f */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertQuatToEigen(const FQuat& Quat, bool& bSuccess);

    /** Convert Eigen::Vector4f to FQuat */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FQuat ConvertEigenToQuat(const TArray<float>& EigenVector, bool& bSuccess);

    // ========================================
    // COLOR CONVERSIONS
    // ========================================

    /** Convert FLinearColor to Eigen::Vector4f */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertLinearColorToEigen(const FLinearColor& Color, bool& bSuccess);

    /** Convert Eigen::Vector4f to FLinearColor */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FLinearColor ConvertEigenToLinearColor(const TArray<float>& EigenVector, bool& bSuccess);

    /** Convert FColor to Eigen::Vector4f (normalized) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertColorToEigen(const FColor& Color, bool& bSuccess);

    /** Convert Eigen::Vector4f to FColor (denormalized) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FColor ConvertEigenToColor(const TArray<float>& EigenVector, bool& bSuccess);

    // ========================================
    // TRANSFORM CONVERSIONS
    // ========================================

    /** Convert FTransform to Eigen::Matrix4f (as flat array) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertTransformToEigen(const FTransform& Transform, bool& bSuccess);

    /** Convert Eigen::Matrix4f to FTransform (from flat array) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FTransform ConvertEigenToTransform(const TArray<float>& EigenMatrix, bool& bSuccess);

    /** Convert FMatrix to Eigen::Matrix4f (as flat array) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|UEToEigen")
    TArray<float> ConvertMatrixToEigen(const FMatrix& Matrix, bool& bSuccess);

    /** Convert Eigen::Matrix4f to FMatrix (from flat array) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|EigenToUE")
    FMatrix ConvertEigenToMatrix(const TArray<float>& EigenMatrix, bool& bSuccess);

    // ========================================
    // BATCH CONVERSIONS
    // ========================================

    /** Batch convert FVectors to Eigen matrix */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Batch")
    TArray<TArray<float>> ConvertVectorArrayToEigen(const TArray<FVector>& Vectors, bool& bSuccess);

    /** Batch convert Eigen matrix to FVectors */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Batch")
    TArray<FVector> ConvertEigenToVectorArray(const TArray<TArray<float>>& EigenMatrix, bool& bSuccess);

    /** Batch convert FLinearColors to Eigen matrix */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Batch")
    TArray<TArray<float>> ConvertColorArrayToEigen(const TArray<FLinearColor>& Colors, bool& bSuccess);

    /** Batch convert Eigen matrix to FLinearColors */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Batch")
    TArray<FLinearColor> ConvertEigenToColorArray(const TArray<TArray<float>>& EigenMatrix, bool& bSuccess);

    // ========================================
    // VALIDATION
    // ========================================

    /** Validate array dimensions */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Validation")
    bool ValidateArrayDimension(const TArray<float>& Array, int32 ExpectedSize, FString& OutErrorMessage);

    /** Validate array values (no NaN, Inf) */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Validation")
    bool ValidateArrayValues(const TArray<float>& Array, FString& OutErrorMessage);

    /** Check if conversion is supported */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Validation")
    bool IsConversionSupported(const FString& SourceType, const FString& TargetType);

    // ========================================
    // STATISTICS
    // ========================================

    /** Get conversion statistics */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Stats")
    FTypeConversionStats GetConversionStatistics() const;

    /** Reset conversion statistics */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Stats")
    void ResetStatistics();

    /** Get last error message */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Stats")
    FString GetLastErrorMessage() const;

    /** Get registry statistics */
    UFUNCTION(BlueprintCallable, Category = "TypeConversion|Stats")
    TMap<FString, FTypeMappingMetadata> GetRegistryStatistics();

protected:
    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Begin conversion timing */
    double BeginConversionTiming();

    /** End conversion timing and update stats */
    void EndConversionTiming(double StartTime, bool bSuccess);

    /** Set error and log */
    void SetError(EConversionResult Result, const FString& ErrorMessage);

    /** Validate flat matrix array size */
    bool ValidateMatrixSize(const TArray<float>& Matrix, int32 Rows, int32 Cols, FString& OutError);

private:
    /** Type conversion registry */
    UPROPERTY()
    UTypeConversionRegistry* Registry;

    /** Timing samples for statistics */
    TArray<float> TimingSamples;

    /** Maximum timing samples to keep */
    static constexpr int32 MaxTimingSamples = 1000;
};

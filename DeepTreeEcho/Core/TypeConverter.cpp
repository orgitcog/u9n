// Copyright Deep Tree Echo. All Rights Reserved.

#include "TypeConverter.h"
#include "Misc/DateTime.h"
#include "Algo/Accumulate.h"

UTypeConverter::UTypeConverter()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTypeConverter::BeginPlay()
{
    Super::BeginPlay();
    
    // Get type conversion registry
    Registry = UTypeConversionRegistry::Get();
    
    if (!Registry)
    {
        UE_LOG(LogTemp, Error, TEXT("TypeConverter: Failed to get TypeConversionRegistry"));
    }
}

// ========================================
// VECTOR CONVERSIONS (UE → EIGEN)
// ========================================

TArray<float> UTypeConverter::ConvertVectorToEigen(const FVector& Vector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Vector3f EigenVec = UEToEigenConverter::ToEigenVector3(Vector);
    
    if (bEnableValidation && !UEToEigenConverter::IsConversionValid(EigenVec))
    {
        SetError(EConversionResult::ValidationFailed, TEXT("Vector contains invalid values"));
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return TArray<float>();
    }
    
    TArray<float> Result = { EigenVec(0), EigenVec(1), EigenVec(2) };
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<float> UTypeConverter::ConvertVector2DToEigen(const FVector2D& Vector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Vector2f EigenVec = UEToEigenConverter::ToEigenVector2(Vector);
    
    if (bEnableValidation && !UEToEigenConverter::IsConversionValid(EigenVec))
    {
        SetError(EConversionResult::ValidationFailed, TEXT("Vector2D contains invalid values"));
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return TArray<float>();
    }
    
    TArray<float> Result = { EigenVec(0), EigenVec(1) };
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<float> UTypeConverter::ConvertVector4ToEigen(const FVector4& Vector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Vector4f EigenVec = UEToEigenConverter::ToEigenVector4(Vector);
    
    if (bEnableValidation && !UEToEigenConverter::IsConversionValid(EigenVec))
    {
        SetError(EConversionResult::ValidationFailed, TEXT("Vector4 contains invalid values"));
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return TArray<float>();
    }
    
    TArray<float> Result = { EigenVec(0), EigenVec(1), EigenVec(2), EigenVec(3) };
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<float> UTypeConverter::ConvertFloatArrayToEigen(const TArray<float>& Array, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    if (bEnableValidation)
    {
        FString ErrorMsg;
        if (!ValidateArrayValues(Array, ErrorMsg))
        {
            SetError(EConversionResult::ValidationFailed, ErrorMsg);
            bSuccess = false;
            EndConversionTiming(StartTime, false);
            return TArray<float>();
        }
    }
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Array; // Pass through, already in correct format
}

// ========================================
// VECTOR CONVERSIONS (EIGEN → UE)
// ========================================

FVector UTypeConverter::ConvertEigenToVector(const TArray<float>& EigenVector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateArrayDimension(EigenVector, 3, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FVector::ZeroVector;
    }
    
    Eigen::Vector3f EigenVec(EigenVector[0], EigenVector[1], EigenVector[2]);
    FVector Result = EigenToUEConverter::ToFVector(EigenVec);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FVector2D UTypeConverter::ConvertEigenToVector2D(const TArray<float>& EigenVector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateArrayDimension(EigenVector, 2, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FVector2D::ZeroVector;
    }
    
    Eigen::Vector2f EigenVec(EigenVector[0], EigenVector[1]);
    FVector2D Result = EigenToUEConverter::ToFVector2D(EigenVec);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FVector4 UTypeConverter::ConvertEigenToVector4(const TArray<float>& EigenVector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateArrayDimension(EigenVector, 4, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FVector4(0, 0, 0, 0);
    }
    
    Eigen::Vector4f EigenVec(EigenVector[0], EigenVector[1], EigenVector[2], EigenVector[3]);
    FVector4 Result = EigenToUEConverter::ToFVector4(EigenVec);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

// ========================================
// ROTATION CONVERSIONS
// ========================================

TArray<float> UTypeConverter::ConvertRotatorToEigen(const FRotator& Rotator, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Vector3f EigenVec = UEToEigenConverter::ToEigenVector3(Rotator);
    
    TArray<float> Result = { EigenVec(0), EigenVec(1), EigenVec(2) };
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FRotator UTypeConverter::ConvertEigenToRotator(const TArray<float>& EigenVector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateArrayDimension(EigenVector, 3, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FRotator::ZeroRotator;
    }
    
    Eigen::Vector3f EigenVec(EigenVector[0], EigenVector[1], EigenVector[2]);
    FRotator Result = EigenToUEConverter::ToFRotator(EigenVec);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<float> UTypeConverter::ConvertQuatToEigen(const FQuat& Quat, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Vector4f EigenVec = UEToEigenConverter::ToEigenVector4(Quat);
    
    TArray<float> Result = { EigenVec(0), EigenVec(1), EigenVec(2), EigenVec(3) };
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FQuat UTypeConverter::ConvertEigenToQuat(const TArray<float>& EigenVector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateArrayDimension(EigenVector, 4, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FQuat::Identity;
    }
    
    Eigen::Vector4f EigenVec(EigenVector[0], EigenVector[1], EigenVector[2], EigenVector[3]);
    FQuat Result = EigenToUEConverter::ToFQuat(EigenVec);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

// ========================================
// COLOR CONVERSIONS
// ========================================

TArray<float> UTypeConverter::ConvertLinearColorToEigen(const FLinearColor& Color, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Vector4f EigenVec = UEToEigenConverter::ToEigenVector4(Color);
    
    TArray<float> Result = { EigenVec(0), EigenVec(1), EigenVec(2), EigenVec(3) };
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FLinearColor UTypeConverter::ConvertEigenToLinearColor(const TArray<float>& EigenVector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateArrayDimension(EigenVector, 4, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FLinearColor::Black;
    }
    
    Eigen::Vector4f EigenVec(EigenVector[0], EigenVector[1], EigenVector[2], EigenVector[3]);
    FLinearColor Result = EigenToUEConverter::ToFLinearColor(EigenVec);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<float> UTypeConverter::ConvertColorToEigen(const FColor& Color, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Vector4f EigenVec = UEToEigenConverter::ToEigenVector4Normalized(Color);
    
    TArray<float> Result = { EigenVec(0), EigenVec(1), EigenVec(2), EigenVec(3) };
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FColor UTypeConverter::ConvertEigenToColor(const TArray<float>& EigenVector, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateArrayDimension(EigenVector, 4, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FColor::Black;
    }
    
    Eigen::Vector4f EigenVec(EigenVector[0], EigenVector[1], EigenVector[2], EigenVector[3]);
    FColor Result = EigenToUEConverter::ToFColorFromNormalized(EigenVec);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

// ========================================
// TRANSFORM CONVERSIONS
// ========================================

TArray<float> UTypeConverter::ConvertTransformToEigen(const FTransform& Transform, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Matrix4f EigenMat = UEToEigenConverter::ToEigenMatrix4(Transform);
    
    TArray<float> Result;
    Result.Reserve(16);
    for (int32 i = 0; i < 4; ++i)
    {
        for (int32 j = 0; j < 4; ++j)
        {
            Result.Add(EigenMat(i, j));
        }
    }
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FTransform UTypeConverter::ConvertEigenToTransform(const TArray<float>& EigenMatrix, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateMatrixSize(EigenMatrix, 4, 4, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FTransform::Identity;
    }
    
    Eigen::Matrix4f EigenMat;
    for (int32 i = 0; i < 4; ++i)
    {
        for (int32 j = 0; j < 4; ++j)
        {
            EigenMat(i, j) = EigenMatrix[i * 4 + j];
        }
    }
    
    FTransform Result = EigenToUEConverter::ToFTransform(EigenMat);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<float> UTypeConverter::ConvertMatrixToEigen(const FMatrix& Matrix, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::Matrix4f EigenMat = UEToEigenConverter::ToEigenMatrix4(Matrix);
    
    TArray<float> Result;
    Result.Reserve(16);
    for (int32 i = 0; i < 4; ++i)
    {
        for (int32 j = 0; j < 4; ++j)
        {
            Result.Add(EigenMat(i, j));
        }
    }
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

FMatrix UTypeConverter::ConvertEigenToMatrix(const TArray<float>& EigenMatrix, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    FString ErrorMsg;
    if (!ValidateMatrixSize(EigenMatrix, 4, 4, ErrorMsg))
    {
        SetError(EConversionResult::DimensionMismatch, ErrorMsg);
        bSuccess = false;
        EndConversionTiming(StartTime, false);
        return FMatrix::Identity;
    }
    
    Eigen::Matrix4f EigenMat;
    for (int32 i = 0; i < 4; ++i)
    {
        for (int32 j = 0; j < 4; ++j)
        {
            EigenMat(i, j) = EigenMatrix[i * 4 + j];
        }
    }
    
    FMatrix Result = EigenToUEConverter::ToFMatrix(EigenMat);
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

// ========================================
// BATCH CONVERSIONS
// ========================================

TArray<TArray<float>> UTypeConverter::ConvertVectorArrayToEigen(const TArray<FVector>& Vectors, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::MatrixXf EigenMat = UEToEigenConverter::ToEigenMatrix(Vectors);
    
    TArray<TArray<float>> Result;
    Result.Reserve(EigenMat.rows());
    
    for (int32 Row = 0; Row < EigenMat.rows(); ++Row)
    {
        TArray<float> RowArray = { EigenMat(Row, 0), EigenMat(Row, 1), EigenMat(Row, 2) };
        Result.Add(RowArray);
    }
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<FVector> UTypeConverter::ConvertEigenToVectorArray(const TArray<TArray<float>>& EigenMatrix, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    if (EigenMatrix.Num() == 0)
    {
        bSuccess = true;
        EndConversionTiming(StartTime, true);
        return TArray<FVector>();
    }
    
    // Validate all rows have 3 columns
    for (const TArray<float>& Row : EigenMatrix)
    {
        if (Row.Num() != 3)
        {
            SetError(EConversionResult::DimensionMismatch, TEXT("Matrix rows must have 3 columns for FVector conversion"));
            bSuccess = false;
            EndConversionTiming(StartTime, false);
            return TArray<FVector>();
        }
    }
    
    TArray<FVector> Result;
    Result.Reserve(EigenMatrix.Num());
    
    for (const TArray<float>& Row : EigenMatrix)
    {
        Eigen::Vector3f EigenVec(Row[0], Row[1], Row[2]);
        Result.Add(EigenToUEConverter::ToFVector(EigenVec));
    }
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<TArray<float>> UTypeConverter::ConvertColorArrayToEigen(const TArray<FLinearColor>& Colors, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    Eigen::MatrixXf EigenMat = UEToEigenConverter::ToEigenMatrix(Colors);
    
    TArray<TArray<float>> Result;
    Result.Reserve(EigenMat.rows());
    
    for (int32 Row = 0; Row < EigenMat.rows(); ++Row)
    {
        TArray<float> RowArray = { EigenMat(Row, 0), EigenMat(Row, 1), EigenMat(Row, 2), EigenMat(Row, 3) };
        Result.Add(RowArray);
    }
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

TArray<FLinearColor> UTypeConverter::ConvertEigenToColorArray(const TArray<TArray<float>>& EigenMatrix, bool& bSuccess)
{
    double StartTime = BeginConversionTiming();
    
    if (EigenMatrix.Num() == 0)
    {
        bSuccess = true;
        EndConversionTiming(StartTime, true);
        return TArray<FLinearColor>();
    }
    
    // Validate all rows have 4 columns
    for (const TArray<float>& Row : EigenMatrix)
    {
        if (Row.Num() != 4)
        {
            SetError(EConversionResult::DimensionMismatch, TEXT("Matrix rows must have 4 columns for FLinearColor conversion"));
            bSuccess = false;
            EndConversionTiming(StartTime, false);
            return TArray<FLinearColor>();
        }
    }
    
    TArray<FLinearColor> Result;
    Result.Reserve(EigenMatrix.Num());
    
    for (const TArray<float>& Row : EigenMatrix)
    {
        Eigen::Vector4f EigenVec(Row[0], Row[1], Row[2], Row[3]);
        Result.Add(EigenToUEConverter::ToFLinearColor(EigenVec));
    }
    
    bSuccess = true;
    EndConversionTiming(StartTime, true);
    return Result;
}

// ========================================
// VALIDATION
// ========================================

bool UTypeConverter::ValidateArrayDimension(const TArray<float>& Array, int32 ExpectedSize, FString& OutErrorMessage)
{
    if (Array.Num() != ExpectedSize)
    {
        OutErrorMessage = FString::Printf(TEXT("Array size mismatch: expected %d, got %d"), ExpectedSize, Array.Num());
        return false;
    }
    return true;
}

bool UTypeConverter::ValidateArrayValues(const TArray<float>& Array, FString& OutErrorMessage)
{
    for (int32 i = 0; i < Array.Num(); ++i)
    {
        if (!FMath::IsFinite(Array[i]))
        {
            OutErrorMessage = FString::Printf(TEXT("Invalid value at index %d: %f"), i, Array[i]);
            return false;
        }
    }
    return true;
}

bool UTypeConverter::IsConversionSupported(const FString& SourceType, const FString& TargetType)
{
    if (!Registry)
    {
        return false;
    }
    return Registry->IsConversionSupported(SourceType, TargetType);
}

bool UTypeConverter::ValidateMatrixSize(const TArray<float>& Matrix, int32 Rows, int32 Cols, FString& OutError)
{
    int32 ExpectedSize = Rows * Cols;
    if (Matrix.Num() != ExpectedSize)
    {
        OutError = FString::Printf(TEXT("Matrix size mismatch: expected %dx%d (%d elements), got %d"), Rows, Cols, ExpectedSize, Matrix.Num());
        return false;
    }
    return true;
}

// ========================================
// STATISTICS
// ========================================

FTypeConversionStats UTypeConverter::GetConversionStatistics() const
{
    return ConversionStats;
}

void UTypeConverter::ResetStatistics()
{
    ConversionStats = FTypeConversionStats();
    TimingSamples.Empty();
}

FString UTypeConverter::GetLastErrorMessage() const
{
    return LastErrorMessage;
}

TMap<FString, FTypeMappingMetadata> UTypeConverter::GetRegistryStatistics()
{
    if (!Registry)
    {
        return TMap<FString, FTypeMappingMetadata>();
    }
    return Registry->GetConversionStatistics();
}

// ========================================
// INTERNAL METHODS
// ========================================

double UTypeConverter::BeginConversionTiming()
{
    if (!bEnablePerformanceTracking)
    {
        return 0.0;
    }
    return FPlatformTime::Seconds();
}

void UTypeConverter::EndConversionTiming(double StartTime, bool bSuccess)
{
    if (!bEnablePerformanceTracking)
    {
        return;
    }
    
    double EndTime = FPlatformTime::Seconds();
    float ElapsedMicroseconds = static_cast<float>((EndTime - StartTime) * 1000000.0);
    
    ConversionStats.TotalConversions++;
    if (bSuccess)
    {
        ConversionStats.SuccessfulConversions++;
    }
    else
    {
        ConversionStats.FailedConversions++;
    }
    
    ConversionStats.TotalTimeSeconds += (EndTime - StartTime);
    
    TimingSamples.Add(ElapsedMicroseconds);
    if (TimingSamples.Num() > MaxTimingSamples)
    {
        TimingSamples.RemoveAt(0);
    }
    
    float Sum = Algo::Accumulate(TimingSamples, 0.0f);
    ConversionStats.AverageTimeMicroseconds = Sum / TimingSamples.Num();
    
    if (ElapsedMicroseconds > ConversionStats.PeakTimeMicroseconds)
    {
        ConversionStats.PeakTimeMicroseconds = ElapsedMicroseconds;
    }
}

void UTypeConverter::SetError(EConversionResult Result, const FString& ErrorMessage)
{
    LastConversionResult = Result;
    LastErrorMessage = ErrorMessage;
    
    if (bLogErrors && Result != EConversionResult::Success)
    {
        UE_LOG(LogTemp, Error, TEXT("TypeConverter: %s"), *ErrorMessage);
    }
    else if (bLogWarnings && Result == EConversionResult::ValidationFailed)
    {
        UE_LOG(LogTemp, Warning, TEXT("TypeConverter: %s"), *ErrorMessage);
    }
}

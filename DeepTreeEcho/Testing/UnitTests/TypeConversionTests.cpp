// Copyright Deep Tree Echo. All Rights Reserved.

/**
 * Type Conversion System Unit Tests
 * 
 * Comprehensive tests for bidirectional type conversion between
 * Unreal Engine types and Eigen matrices/vectors.
 * 
 * Feature: F1.1.4 - Type Conversion System
 * Tests:
 * - Vector conversions (FVector, FVector2D, FVector4)
 * - Rotation conversions (FRotator, FQuat)
 * - Color conversions (FLinearColor, FColor)
 * - Transform/Matrix conversions
 * - Batch conversions
 * - Error handling and validation
 * - Performance benchmarks
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "TypeConverter.h"
#include "TypeConversionRegistry.h"
#include "UEToEigenConverter.h"
#include "EigenToUEConverter.h"

// Test flags
#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// VECTOR CONVERSION TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FVector_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Vector.FVector_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FVector_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FVector → Eigen → FVector roundtrip
    FVector Original(100.0, 200.0, 300.0);
    
    // Convert to Eigen
    Eigen::Vector3f EigenVec = UEToEigenConverter::ToEigenVector3(Original);
    
    TestEqual(TEXT("FVector.X to Eigen"), EigenVec(0), static_cast<float>(Original.X));
    TestEqual(TEXT("FVector.Y to Eigen"), EigenVec(1), static_cast<float>(Original.Y));
    TestEqual(TEXT("FVector.Z to Eigen"), EigenVec(2), static_cast<float>(Original.Z));
    
    // Convert back to FVector
    FVector Result = EigenToUEConverter::ToFVector(EigenVec);
    
    TestEqual(TEXT("FVector roundtrip X"), Result.X, Original.X, 0.0001);
    TestEqual(TEXT("FVector roundtrip Y"), Result.Y, Original.Y, 0.0001);
    TestEqual(TEXT("FVector roundtrip Z"), Result.Z, Original.Z, 0.0001);
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FVector2D_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Vector.FVector2D_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FVector2D_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FVector2D → Eigen → FVector2D roundtrip
    FVector2D Original(50.0, 100.0);
    
    Eigen::Vector2f EigenVec = UEToEigenConverter::ToEigenVector2(Original);
    FVector2D Result = EigenToUEConverter::ToFVector2D(EigenVec);
    
    TestEqual(TEXT("FVector2D roundtrip X"), Result.X, Original.X, 0.0001);
    TestEqual(TEXT("FVector2D roundtrip Y"), Result.Y, Original.Y, 0.0001);
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FVector4_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Vector.FVector4_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FVector4_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FVector4 → Eigen → FVector4 roundtrip
    FVector4 Original(10.0, 20.0, 30.0, 40.0);
    
    Eigen::Vector4f EigenVec = UEToEigenConverter::ToEigenVector4(Original);
    FVector4 Result = EigenToUEConverter::ToFVector4(EigenVec);
    
    TestEqual(TEXT("FVector4 roundtrip X"), Result.X, Original.X, 0.0001);
    TestEqual(TEXT("FVector4 roundtrip Y"), Result.Y, Original.Y, 0.0001);
    TestEqual(TEXT("FVector4 roundtrip Z"), Result.Z, Original.Z, 0.0001);
    TestEqual(TEXT("FVector4 roundtrip W"), Result.W, Original.W, 0.0001);
    
    return true;
}

// ============================================================================
// ROTATION CONVERSION TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FRotator_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Rotation.FRotator_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FRotator_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FRotator → Eigen → FRotator roundtrip
    FRotator Original(45.0, 90.0, 180.0);
    
    Eigen::Vector3f EigenVec = UEToEigenConverter::ToEigenVector3(Original);
    FRotator Result = EigenToUEConverter::ToFRotator(EigenVec);
    
    TestEqual(TEXT("FRotator roundtrip Pitch"), Result.Pitch, Original.Pitch, 0.0001);
    TestEqual(TEXT("FRotator roundtrip Yaw"), Result.Yaw, Original.Yaw, 0.0001);
    TestEqual(TEXT("FRotator roundtrip Roll"), Result.Roll, Original.Roll, 0.0001);
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FQuat_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Rotation.FQuat_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FQuat_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FQuat → Eigen → FQuat roundtrip
    FQuat Original = FRotator(30.0, 60.0, 90.0).Quaternion();
    
    // Test as Vector4f
    Eigen::Vector4f EigenVec4 = UEToEigenConverter::ToEigenVector4(Original);
    FQuat Result4 = EigenToUEConverter::ToFQuat(EigenVec4);
    
    TestEqual(TEXT("FQuat (vec4) roundtrip X"), Result4.X, Original.X, 0.0001);
    TestEqual(TEXT("FQuat (vec4) roundtrip Y"), Result4.Y, Original.Y, 0.0001);
    TestEqual(TEXT("FQuat (vec4) roundtrip Z"), Result4.Z, Original.Z, 0.0001);
    TestEqual(TEXT("FQuat (vec4) roundtrip W"), Result4.W, Original.W, 0.0001);
    
    // Test as Quaternionf
    Eigen::Quaternionf EigenQuat = UEToEigenConverter::ToEigenQuaternion(Original);
    FQuat ResultQuat = EigenToUEConverter::ToFQuat(EigenQuat);
    
    TestEqual(TEXT("FQuat (quat) roundtrip X"), ResultQuat.X, Original.X, 0.0001);
    TestEqual(TEXT("FQuat (quat) roundtrip Y"), ResultQuat.Y, Original.Y, 0.0001);
    TestEqual(TEXT("FQuat (quat) roundtrip Z"), ResultQuat.Z, Original.Z, 0.0001);
    TestEqual(TEXT("FQuat (quat) roundtrip W"), ResultQuat.W, Original.W, 0.0001);
    
    return true;
}

// ============================================================================
// COLOR CONVERSION TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FLinearColor_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Color.FLinearColor_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FLinearColor_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FLinearColor → Eigen → FLinearColor roundtrip
    FLinearColor Original(0.5f, 0.75f, 0.25f, 1.0f);
    
    Eigen::Vector4f EigenVec = UEToEigenConverter::ToEigenVector4(Original);
    FLinearColor Result = EigenToUEConverter::ToFLinearColor(EigenVec);
    
    TestEqual(TEXT("FLinearColor roundtrip R"), Result.R, Original.R, 0.0001f);
    TestEqual(TEXT("FLinearColor roundtrip G"), Result.G, Original.G, 0.0001f);
    TestEqual(TEXT("FLinearColor roundtrip B"), Result.B, Original.B, 0.0001f);
    TestEqual(TEXT("FLinearColor roundtrip A"), Result.A, Original.A, 0.0001f);
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FColor_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Color.FColor_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FColor_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FColor → Eigen (normalized) → FColor roundtrip
    FColor Original(128, 200, 50, 255);
    
    Eigen::Vector4f EigenVec = UEToEigenConverter::ToEigenVector4Normalized(Original);
    FColor Result = EigenToUEConverter::ToFColorFromNormalized(EigenVec);
    
    TestEqual(TEXT("FColor roundtrip R"), Result.R, Original.R);
    TestEqual(TEXT("FColor roundtrip G"), Result.G, Original.G);
    TestEqual(TEXT("FColor roundtrip B"), Result.B, Original.B);
    TestEqual(TEXT("FColor roundtrip A"), Result.A, Original.A);
    
    return true;
}

// ============================================================================
// TRANSFORM/MATRIX CONVERSION TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FMatrix_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Matrix.FMatrix_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FMatrix_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FMatrix → Eigen → FMatrix roundtrip
    FMatrix Original = FRotationMatrix::Make(FRotator(30.0, 45.0, 60.0));
    
    Eigen::Matrix4f EigenMat = UEToEigenConverter::ToEigenMatrix4(Original);
    FMatrix Result = EigenToUEConverter::ToFMatrix(EigenMat);
    
    // Check all matrix elements
    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Col = 0; Col < 4; ++Col)
        {
            FString TestName = FString::Printf(TEXT("FMatrix roundtrip [%d][%d]"), Row, Col);
            TestEqual(*TestName, Result.M[Row][Col], Original.M[Row][Col], 0.0001);
        }
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_FTransform_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Matrix.FTransform_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_FTransform_Bidirectional::RunTest(const FString& Parameters)
{
    // Test FTransform → Eigen → FTransform roundtrip
    FVector Location(100.0, 200.0, 300.0);
    FRotator Rotation(30.0, 45.0, 60.0);
    FVector Scale(1.5, 2.0, 0.5);
    FTransform Original(Rotation, Location, Scale);
    
    Eigen::Matrix4f EigenMat = UEToEigenConverter::ToEigenMatrix4(Original);
    FTransform Result = EigenToUEConverter::ToFTransform(EigenMat);
    
    // Test translation
    TestEqual(TEXT("FTransform roundtrip Translation.X"), Result.GetTranslation().X, Original.GetTranslation().X, 0.01);
    TestEqual(TEXT("FTransform roundtrip Translation.Y"), Result.GetTranslation().Y, Original.GetTranslation().Y, 0.01);
    TestEqual(TEXT("FTransform roundtrip Translation.Z"), Result.GetTranslation().Z, Original.GetTranslation().Z, 0.01);
    
    // Test rotation (quaternion comparison)
    FQuat OriginalQuat = Original.GetRotation();
    FQuat ResultQuat = Result.GetRotation();
    TestEqual(TEXT("FTransform roundtrip Rotation.X"), ResultQuat.X, OriginalQuat.X, 0.001);
    TestEqual(TEXT("FTransform roundtrip Rotation.Y"), ResultQuat.Y, OriginalQuat.Y, 0.001);
    TestEqual(TEXT("FTransform roundtrip Rotation.Z"), ResultQuat.Z, OriginalQuat.Z, 0.001);
    TestEqual(TEXT("FTransform roundtrip Rotation.W"), ResultQuat.W, OriginalQuat.W, 0.001);
    
    // Test scale
    TestEqual(TEXT("FTransform roundtrip Scale.X"), Result.GetScale3D().X, Original.GetScale3D().X, 0.01);
    TestEqual(TEXT("FTransform roundtrip Scale.Y"), Result.GetScale3D().Y, Original.GetScale3D().Y, 0.01);
    TestEqual(TEXT("FTransform roundtrip Scale.Z"), Result.GetScale3D().Z, Original.GetScale3D().Z, 0.01);
    
    return true;
}

// ============================================================================
// BATCH CONVERSION TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_VectorArray_Bidirectional, 
    "DeepTreeEcho.TypeConversion.Batch.VectorArray_Bidirectional", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_VectorArray_Bidirectional::RunTest(const FString& Parameters)
{
    // Test TArray<FVector> → Eigen → TArray<FVector> roundtrip
    TArray<FVector> Original;
    Original.Add(FVector(1.0, 2.0, 3.0));
    Original.Add(FVector(4.0, 5.0, 6.0));
    Original.Add(FVector(7.0, 8.0, 9.0));
    
    Eigen::MatrixXf EigenMat = UEToEigenConverter::ToEigenMatrix(Original);
    TArray<FVector> Result = EigenToUEConverter::ToFVectorArray(EigenMat);
    
    TestEqual(TEXT("VectorArray size"), Result.Num(), Original.Num());
    
    for (int32 i = 0; i < Original.Num(); ++i)
    {
        FString TestNameX = FString::Printf(TEXT("VectorArray[%d].X"), i);
        FString TestNameY = FString::Printf(TEXT("VectorArray[%d].Y"), i);
        FString TestNameZ = FString::Printf(TEXT("VectorArray[%d].Z"), i);
        
        TestEqual(*TestNameX, Result[i].X, Original[i].X, 0.0001);
        TestEqual(*TestNameY, Result[i].Y, Original[i].Y, 0.0001);
        TestEqual(*TestNameZ, Result[i].Z, Original[i].Z, 0.0001);
    }
    
    return true;
}

// ============================================================================
// VALIDATION TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_Validation_NaN, 
    "DeepTreeEcho.TypeConversion.Validation.NaN", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_Validation_NaN::RunTest(const FString& Parameters)
{
    // Test NaN detection
    Eigen::Vector3f InvalidVec(1.0f, NAN, 3.0f);
    
    bool bIsValid = UEToEigenConverter::IsConversionValid(InvalidVec);
    TestFalse(TEXT("NaN detection"), bIsValid);
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_Validation_Infinity, 
    "DeepTreeEcho.TypeConversion.Validation.Infinity", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_Validation_Infinity::RunTest(const FString& Parameters)
{
    // Test Infinity detection
    Eigen::Vector3f InvalidVec(1.0f, 2.0f, std::numeric_limits<float>::infinity());
    
    bool bIsValid = UEToEigenConverter::IsConversionValid(InvalidVec);
    TestFalse(TEXT("Infinity detection"), bIsValid);
    
    return true;
}

// ============================================================================
// TYPE REGISTRY TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_Registry_BuiltInTypes, 
    "DeepTreeEcho.TypeConversion.Registry.BuiltInTypes", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_Registry_BuiltInTypes::RunTest(const FString& Parameters)
{
    UTypeConversionRegistry* Registry = UTypeConversionRegistry::Get();
    TestNotNull(TEXT("Registry instance"), Registry);
    
    // Test common conversions are registered
    TestTrue(TEXT("FVector → Eigen::Vector3f"), Registry->IsConversionSupported(TEXT("FVector"), TEXT("Eigen::Vector3f")));
    TestTrue(TEXT("FRotator → Eigen::Vector3f"), Registry->IsConversionSupported(TEXT("FRotator"), TEXT("Eigen::Vector3f")));
    TestTrue(TEXT("FLinearColor → Eigen::Vector4f"), Registry->IsConversionSupported(TEXT("FLinearColor"), TEXT("Eigen::Vector4f")));
    TestTrue(TEXT("FTransform → Eigen::Matrix4f"), Registry->IsConversionSupported(TEXT("FTransform"), TEXT("Eigen::Matrix4f")));
    
    // Test reverse conversions
    TestTrue(TEXT("Eigen::Vector3f → FVector"), Registry->IsConversionSupported(TEXT("Eigen::Vector3f"), TEXT("FVector")));
    TestTrue(TEXT("Eigen::Vector4f → FLinearColor"), Registry->IsConversionSupported(TEXT("Eigen::Vector4f"), TEXT("FLinearColor")));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_Registry_ConversionQuality, 
    "DeepTreeEcho.TypeConversion.Registry.ConversionQuality", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_Registry_ConversionQuality::RunTest(const FString& Parameters)
{
    UTypeConversionRegistry* Registry = UTypeConversionRegistry::Get();
    
    // Test lossless conversions
    bool bLossless = Registry->IsConversionLossless(TEXT("FVector"), TEXT("Eigen::Vector3f"));
    TestTrue(TEXT("FVector → Eigen::Vector3f is lossless"), bLossless);
    
    // Test conversion metadata
    FTypeMappingMetadata Metadata = Registry->GetConversionMetadata(TEXT("FVector"), TEXT("Eigen::Vector3f"));
    TestTrue(TEXT("Metadata is supported"), Metadata.bIsSupported);
    TestEqual(TEXT("Metadata source dimension"), Metadata.SourceDimension, 3);
    TestEqual(TEXT("Metadata target dimension"), Metadata.TargetDimension, 3);
    
    return true;
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTypeConversionTest_Performance_VectorConversion, 
    "DeepTreeEcho.TypeConversion.Performance.VectorConversion", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTypeConversionTest_Performance_VectorConversion::RunTest(const FString& Parameters)
{
    const int32 NumIterations = 10000;
    FVector TestVector(100.0, 200.0, 300.0);
    
    double StartTime = FPlatformTime::Seconds();
    
    for (int32 i = 0; i < NumIterations; ++i)
    {
        Eigen::Vector3f EigenVec = UEToEigenConverter::ToEigenVector3(TestVector);
        FVector Result = EigenToUEConverter::ToFVector(EigenVec);
    }
    
    double EndTime = FPlatformTime::Seconds();
    double TotalTime = (EndTime - StartTime) * 1000000.0; // Convert to microseconds
    double AvgTime = TotalTime / NumIterations;
    
    AddInfo(FString::Printf(TEXT("Average conversion time: %.3f microseconds"), AvgTime));
    
    // Conversion should be very fast (< 1 microsecond average)
    TestTrue(TEXT("Conversion performance"), AvgTime < 1.0);
    
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

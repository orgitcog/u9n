#pragma once

/**
 * Eigen to UE Converter
 * 
 * Converts Eigen matrices and vectors to Unreal Engine types.
 * Optimized for performance with validation and error handling.
 * 
 * Feature: F1.1.4 - Type Conversion System
 * Phase: 1.1 - Neural-Symbolic Bridge Architecture
 * Epic: E1 - Foundation & Core Integration
 */

#include "CoreMinimal.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Matrix.h"
#include "Math/Color.h"

// Eigen includes (from ReservoirEcho external dependencies)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100 4127 4244 4456 4459 4702)
#endif

#include <Eigen/Core>
#include <Eigen/Geometry>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace EigenToUEConverter
{
    // ========================================
    // VECTOR CONVERSIONS
    // ========================================

    /**
     * Convert Eigen::Vector3f to FVector
     * @param Vector - Eigen 3D vector
     * @return Unreal Engine vector
     */
    inline FVector ToFVector(const Eigen::Vector3f& Vector)
    {
        return FVector(
            static_cast<double>(Vector(0)),
            static_cast<double>(Vector(1)),
            static_cast<double>(Vector(2))
        );
    }

    /**
     * Convert Eigen::Vector2f to FVector2D
     * @param Vector - Eigen 2D vector
     * @return Unreal Engine 2D vector
     */
    inline FVector2D ToFVector2D(const Eigen::Vector2f& Vector)
    {
        return FVector2D(
            static_cast<double>(Vector(0)),
            static_cast<double>(Vector(1))
        );
    }

    /**
     * Convert Eigen::Vector4f to FVector4
     * @param Vector - Eigen 4D vector
     * @return Unreal Engine 4D vector
     */
    inline FVector4 ToFVector4(const Eigen::Vector4f& Vector)
    {
        return FVector4(
            static_cast<double>(Vector(0)),
            static_cast<double>(Vector(1)),
            static_cast<double>(Vector(2)),
            static_cast<double>(Vector(3))
        );
    }

    /**
     * Convert Eigen::VectorXf to TArray<float>
     * @param Vector - Eigen dynamic vector
     * @return Unreal Engine dynamic array
     */
    inline TArray<float> ToFloatArray(const Eigen::VectorXf& Vector)
    {
        TArray<float> Result;
        Result.Reserve(Vector.size());
        
        for (int32 i = 0; i < Vector.size(); ++i)
        {
            Result.Add(Vector(i));
        }
        
        return Result;
    }

    /**
     * Convert Eigen::VectorXd to TArray<double>
     * @param Vector - Eigen dynamic vector (double precision)
     * @return Unreal Engine dynamic array
     */
    inline TArray<double> ToDoubleArray(const Eigen::VectorXd& Vector)
    {
        TArray<double> Result;
        Result.Reserve(Vector.size());
        
        for (int32 i = 0; i < Vector.size(); ++i)
        {
            Result.Add(Vector(i));
        }
        
        return Result;
    }

    // ========================================
    // ROTATION CONVERSIONS
    // ========================================

    /**
     * Convert Eigen::Vector3f to FRotator (Pitch, Yaw, Roll)
     * @param Vector - Eigen 3D vector (degrees)
     * @return Unreal Engine rotator
     */
    inline FRotator ToFRotator(const Eigen::Vector3f& Vector)
    {
        return FRotator(
            static_cast<double>(Vector(0)), // Pitch
            static_cast<double>(Vector(1)), // Yaw
            static_cast<double>(Vector(2))  // Roll
        );
    }

    /**
     * Convert Eigen::Vector4f to FQuat (X, Y, Z, W)
     * @param Vector - Eigen 4D vector
     * @return Unreal Engine quaternion
     */
    inline FQuat ToFQuat(const Eigen::Vector4f& Vector)
    {
        return FQuat(
            static_cast<double>(Vector(0)), // X
            static_cast<double>(Vector(1)), // Y
            static_cast<double>(Vector(2)), // Z
            static_cast<double>(Vector(3))  // W
        );
    }

    /**
     * Convert Eigen::Quaternionf to FQuat
     * @param Quat - Eigen quaternion
     * @return Unreal Engine quaternion
     */
    inline FQuat ToFQuat(const Eigen::Quaternionf& Quat)
    {
        return FQuat(
            static_cast<double>(Quat.x()),
            static_cast<double>(Quat.y()),
            static_cast<double>(Quat.z()),
            static_cast<double>(Quat.w())
        );
    }

    // ========================================
    // COLOR CONVERSIONS
    // ========================================

    /**
     * Convert Eigen::Vector4f to FLinearColor (R, G, B, A)
     * @param Vector - Eigen 4D vector
     * @return Unreal Engine linear color
     */
    inline FLinearColor ToFLinearColor(const Eigen::Vector4f& Vector)
    {
        return FLinearColor(
            Vector(0), // R
            Vector(1), // G
            Vector(2), // B
            Vector(3)  // A
        );
    }

    /**
     * Convert Eigen::Vector4i to FColor (R, G, B, A as uint8)
     * @param Vector - Eigen 4D integer vector
     * @return Unreal Engine color
     */
    inline FColor ToFColor(const Eigen::Vector4i& Vector)
    {
        return FColor(
            static_cast<uint8>(FMath::Clamp(Vector(0), 0, 255)),
            static_cast<uint8>(FMath::Clamp(Vector(1), 0, 255)),
            static_cast<uint8>(FMath::Clamp(Vector(2), 0, 255)),
            static_cast<uint8>(FMath::Clamp(Vector(3), 0, 255))
        );
    }

    /**
     * Convert normalized Eigen::Vector4f to FColor (R, G, B, A)
     * @param Vector - Eigen 4D vector (normalized [0, 1])
     * @return Unreal Engine color
     */
    inline FColor ToFColorFromNormalized(const Eigen::Vector4f& Vector)
    {
        return FColor(
            static_cast<uint8>(FMath::Clamp(Vector(0) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8>(FMath::Clamp(Vector(1) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8>(FMath::Clamp(Vector(2) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8>(FMath::Clamp(Vector(3) * 255.0f, 0.0f, 255.0f))
        );
    }

    // ========================================
    // MATRIX CONVERSIONS
    // ========================================

    /**
     * Convert Eigen::Matrix4f to FMatrix
     * @param Matrix - Eigen 4x4 matrix
     * @return Unreal Engine 4x4 matrix
     */
    inline FMatrix ToFMatrix(const Eigen::Matrix4f& Matrix)
    {
        FMatrix Result;
        
        // Eigen uses column-major, Unreal uses row-major
        // Transpose during conversion
        for (int32 Row = 0; Row < 4; ++Row)
        {
            for (int32 Col = 0; Col < 4; ++Col)
            {
                Result.M[Row][Col] = static_cast<double>(Matrix(Row, Col));
            }
        }
        
        return Result;
    }

    /**
     * Convert Eigen::Matrix4f to FTransform
     * @param Matrix - Eigen 4x4 transformation matrix
     * @return Unreal Engine transform
     */
    inline FTransform ToFTransform(const Eigen::Matrix4f& Matrix)
    {
        FMatrix UEMatrix = ToFMatrix(Matrix);
        
        // Extract transform components
        FVector Translation = UEMatrix.GetOrigin();
        FQuat Rotation = UEMatrix.ToQuat();
        FVector Scale = UEMatrix.GetScaleVector();
        
        return FTransform(Rotation, Translation, Scale);
    }

    /**
     * Convert Eigen::MatrixXf to TArray<TArray<float>>
     * @param Matrix - Eigen dynamic matrix
     * @return 2D array (rows x cols)
     */
    inline TArray<TArray<float>> ToFloatArray2D(const Eigen::MatrixXf& Matrix)
    {
        TArray<TArray<float>> Result;
        Result.Reserve(Matrix.rows());
        
        for (int32 Row = 0; Row < Matrix.rows(); ++Row)
        {
            TArray<float> RowArray;
            RowArray.Reserve(Matrix.cols());
            
            for (int32 Col = 0; Col < Matrix.cols(); ++Col)
            {
                RowArray.Add(Matrix(Row, Col));
            }
            
            Result.Add(RowArray);
        }
        
        return Result;
    }

    // ========================================
    // SPECIALIZED CONVERSIONS
    // ========================================

    /**
     * Create FTransform from separate Eigen components
     * @param Translation - Translation vector
     * @param Rotation - Rotation quaternion
     * @param Scale - Scale vector
     * @return Unreal Engine transform
     */
    inline FTransform ToFTransform(
        const Eigen::Vector3f& Translation,
        const Eigen::Quaternionf& Rotation,
        const Eigen::Vector3f& Scale)
    {
        return FTransform(
            ToFQuat(Rotation),
            ToFVector(Translation),
            ToFVector(Scale)
        );
    }

    /**
     * Convert Eigen matrix (Nx3) to array of FVectors
     * @param Matrix - Eigen matrix (rows = vectors, cols = 3)
     * @return Array of Unreal Engine vectors
     */
    inline TArray<FVector> ToFVectorArray(const Eigen::MatrixXf& Matrix)
    {
        check(Matrix.cols() == 3); // Ensure 3 columns
        
        TArray<FVector> Result;
        Result.Reserve(Matrix.rows());
        
        for (int32 i = 0; i < Matrix.rows(); ++i)
        {
            Result.Add(FVector(
                static_cast<double>(Matrix(i, 0)),
                static_cast<double>(Matrix(i, 1)),
                static_cast<double>(Matrix(i, 2))
            ));
        }
        
        return Result;
    }

    /**
     * Convert Eigen matrix (Nx4) to array of FLinearColors
     * @param Matrix - Eigen matrix (rows = colors, cols = 4 RGBA)
     * @return Array of Unreal Engine colors
     */
    inline TArray<FLinearColor> ToFLinearColorArray(const Eigen::MatrixXf& Matrix)
    {
        check(Matrix.cols() == 4); // Ensure 4 columns
        
        TArray<FLinearColor> Result;
        Result.Reserve(Matrix.rows());
        
        for (int32 i = 0; i < Matrix.rows(); ++i)
        {
            Result.Add(FLinearColor(
                Matrix(i, 0), // R
                Matrix(i, 1), // G
                Matrix(i, 2), // B
                Matrix(i, 3)  // A
            ));
        }
        
        return Result;
    }

    // ========================================
    // VALIDATION
    // ========================================

    /**
     * Validate Eigen vector/matrix before conversion
     * @param Vector - Eigen vector to validate
     * @param OutErrorMessage - Output error message if invalid
     * @return True if valid, false otherwise
     */
    template<typename Derived>
    inline bool ValidateConversion(
        const Eigen::MatrixBase<Derived>& Vector,
        FString& OutErrorMessage)
    {
        if (!Vector.allFinite())
        {
            OutErrorMessage = TEXT("Conversion failed: Eigen vector contains NaN or Inf values");
            return false;
        }
        
        return true;
    }

    /**
     * Safe conversion with validation
     * @param Vector - Eigen 3D vector
     * @param OutResult - Output Unreal Engine vector
     * @return True if successful, false if validation failed
     */
    inline bool SafeToFVector(const Eigen::Vector3f& Vector, FVector& OutResult)
    {
        FString ErrorMsg;
        if (!ValidateConversion(Vector, ErrorMsg))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMsg);
            OutResult = FVector::ZeroVector;
            return false;
        }
        
        OutResult = ToFVector(Vector);
        return true;
    }

    /**
     * Safe conversion with validation (Matrix4)
     * @param Matrix - Eigen 4x4 matrix
     * @param OutResult - Output Unreal Engine matrix
     * @return True if successful, false if validation failed
     */
    inline bool SafeToFMatrix(const Eigen::Matrix4f& Matrix, FMatrix& OutResult)
    {
        FString ErrorMsg;
        if (!ValidateConversion(Matrix, ErrorMsg))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMsg);
            OutResult = FMatrix::Identity;
            return false;
        }
        
        OutResult = ToFMatrix(Matrix);
        return true;
    }

} // namespace EigenToUEConverter

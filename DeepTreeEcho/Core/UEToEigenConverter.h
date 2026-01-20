#pragma once

/**
 * UE to Eigen Converter
 * 
 * Converts Unreal Engine types to Eigen matrices and vectors for neural computing.
 * Optimized for performance with zero-copy semantics where possible.
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

namespace UEToEigenConverter
{
    // ========================================
    // VECTOR CONVERSIONS
    // ========================================

    /**
     * Convert FVector to Eigen::Vector3f
     * @param Vector - Unreal Engine vector
     * @return Eigen 3D vector
     */
    inline Eigen::Vector3f ToEigenVector3(const FVector& Vector)
    {
        return Eigen::Vector3f(
            static_cast<float>(Vector.X),
            static_cast<float>(Vector.Y),
            static_cast<float>(Vector.Z)
        );
    }

    /**
     * Convert FVector2D to Eigen::Vector2f
     * @param Vector - Unreal Engine 2D vector
     * @return Eigen 2D vector
     */
    inline Eigen::Vector2f ToEigenVector2(const FVector2D& Vector)
    {
        return Eigen::Vector2f(
            static_cast<float>(Vector.X),
            static_cast<float>(Vector.Y)
        );
    }

    /**
     * Convert FVector4 to Eigen::Vector4f
     * @param Vector - Unreal Engine 4D vector
     * @return Eigen 4D vector
     */
    inline Eigen::Vector4f ToEigenVector4(const FVector4& Vector)
    {
        return Eigen::Vector4f(
            static_cast<float>(Vector.X),
            static_cast<float>(Vector.Y),
            static_cast<float>(Vector.Z),
            static_cast<float>(Vector.W)
        );
    }

    /**
     * Convert TArray<float> to Eigen::VectorXf
     * @param Array - Unreal Engine dynamic array
     * @return Eigen dynamic vector
     */
    inline Eigen::VectorXf ToEigenVectorX(const TArray<float>& Array)
    {
        Eigen::VectorXf Result(Array.Num());
        for (int32 i = 0; i < Array.Num(); ++i)
        {
            Result(i) = Array[i];
        }
        return Result;
    }

    /**
     * Convert TArray<double> to Eigen::VectorXd
     * @param Array - Unreal Engine dynamic array
     * @return Eigen dynamic vector (double precision)
     */
    inline Eigen::VectorXd ToEigenVectorXd(const TArray<double>& Array)
    {
        Eigen::VectorXd Result(Array.Num());
        for (int32 i = 0; i < Array.Num(); ++i)
        {
            Result(i) = Array[i];
        }
        return Result;
    }

    // ========================================
    // ROTATION CONVERSIONS
    // ========================================

    /**
     * Convert FRotator to Eigen::Vector3f (Pitch, Yaw, Roll)
     * @param Rotator - Unreal Engine rotator
     * @return Eigen 3D vector (degrees)
     */
    inline Eigen::Vector3f ToEigenVector3(const FRotator& Rotator)
    {
        return Eigen::Vector3f(
            static_cast<float>(Rotator.Pitch),
            static_cast<float>(Rotator.Yaw),
            static_cast<float>(Rotator.Roll)
        );
    }

    /**
     * Convert FQuat to Eigen::Vector4f (X, Y, Z, W)
     * @param Quat - Unreal Engine quaternion
     * @return Eigen 4D vector
     */
    inline Eigen::Vector4f ToEigenVector4(const FQuat& Quat)
    {
        return Eigen::Vector4f(
            static_cast<float>(Quat.X),
            static_cast<float>(Quat.Y),
            static_cast<float>(Quat.Z),
            static_cast<float>(Quat.W)
        );
    }

    /**
     * Convert FQuat to Eigen::Quaternionf
     * @param Quat - Unreal Engine quaternion
     * @return Eigen quaternion
     */
    inline Eigen::Quaternionf ToEigenQuaternion(const FQuat& Quat)
    {
        // Eigen quaternion constructor: w, x, y, z
        return Eigen::Quaternionf(
            static_cast<float>(Quat.W),
            static_cast<float>(Quat.X),
            static_cast<float>(Quat.Y),
            static_cast<float>(Quat.Z)
        );
    }

    // ========================================
    // COLOR CONVERSIONS
    // ========================================

    /**
     * Convert FLinearColor to Eigen::Vector4f (R, G, B, A)
     * @param Color - Unreal Engine linear color
     * @return Eigen 4D vector
     */
    inline Eigen::Vector4f ToEigenVector4(const FLinearColor& Color)
    {
        return Eigen::Vector4f(
            Color.R,
            Color.G,
            Color.B,
            Color.A
        );
    }

    /**
     * Convert FColor to Eigen::Vector4i (R, G, B, A as uint8)
     * @param Color - Unreal Engine color
     * @return Eigen 4D integer vector
     */
    inline Eigen::Vector4i ToEigenVector4i(const FColor& Color)
    {
        return Eigen::Vector4i(
            Color.R,
            Color.G,
            Color.B,
            Color.A
        );
    }

    /**
     * Convert FColor to Eigen::Vector4f (normalized R, G, B, A)
     * @param Color - Unreal Engine color
     * @return Eigen 4D vector (normalized to [0, 1])
     */
    inline Eigen::Vector4f ToEigenVector4Normalized(const FColor& Color)
    {
        return Eigen::Vector4f(
            Color.R / 255.0f,
            Color.G / 255.0f,
            Color.B / 255.0f,
            Color.A / 255.0f
        );
    }

    // ========================================
    // MATRIX CONVERSIONS
    // ========================================

    /**
     * Convert FMatrix to Eigen::Matrix4f
     * @param Matrix - Unreal Engine 4x4 matrix
     * @return Eigen 4x4 matrix
     */
    inline Eigen::Matrix4f ToEigenMatrix4(const FMatrix& Matrix)
    {
        Eigen::Matrix4f Result;
        
        // Unreal uses row-major, Eigen uses column-major by default
        // Transpose during conversion
        for (int32 Row = 0; Row < 4; ++Row)
        {
            for (int32 Col = 0; Col < 4; ++Col)
            {
                Result(Row, Col) = static_cast<float>(Matrix.M[Row][Col]);
            }
        }
        
        return Result;
    }

    /**
     * Convert FTransform to Eigen::Matrix4f (4x4 transformation matrix)
     * @param Transform - Unreal Engine transform
     * @return Eigen 4x4 transformation matrix
     */
    inline Eigen::Matrix4f ToEigenMatrix4(const FTransform& Transform)
    {
        // Convert to FMatrix first, then to Eigen
        FMatrix Matrix = Transform.ToMatrixWithScale();
        return ToEigenMatrix4(Matrix);
    }

    /**
     * Convert TArray<TArray<float>> to Eigen::MatrixXf
     * @param Array2D - 2D array (rows x cols)
     * @return Eigen dynamic matrix
     */
    inline Eigen::MatrixXf ToEigenMatrixX(const TArray<TArray<float>>& Array2D)
    {
        if (Array2D.Num() == 0)
        {
            return Eigen::MatrixXf(0, 0);
        }
        
        int32 Rows = Array2D.Num();
        int32 Cols = Array2D[0].Num();
        
        Eigen::MatrixXf Result(Rows, Cols);
        
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            for (int32 Col = 0; Col < Cols; ++Col)
            {
                Result(Row, Col) = Array2D[Row][Col];
            }
        }
        
        return Result;
    }

    // ========================================
    // SPECIALIZED CONVERSIONS
    // ========================================

    /**
     * Convert FTransform to separate translation, rotation, and scale vectors
     * @param Transform - Unreal Engine transform
     * @param OutTranslation - Output translation vector
     * @param OutRotation - Output rotation quaternion
     * @param OutScale - Output scale vector
     */
    inline void ToEigenComponents(
        const FTransform& Transform,
        Eigen::Vector3f& OutTranslation,
        Eigen::Quaternionf& OutRotation,
        Eigen::Vector3f& OutScale)
    {
        OutTranslation = ToEigenVector3(Transform.GetTranslation());
        OutRotation = ToEigenQuaternion(Transform.GetRotation());
        OutScale = ToEigenVector3(Transform.GetScale3D());
    }

    /**
     * Convert array of FVectors to Eigen matrix (Nx3)
     * @param Vectors - Array of Unreal Engine vectors
     * @return Eigen matrix (rows = vectors, cols = 3)
     */
    inline Eigen::MatrixXf ToEigenMatrix(const TArray<FVector>& Vectors)
    {
        Eigen::MatrixXf Result(Vectors.Num(), 3);
        
        for (int32 i = 0; i < Vectors.Num(); ++i)
        {
            Result(i, 0) = static_cast<float>(Vectors[i].X);
            Result(i, 1) = static_cast<float>(Vectors[i].Y);
            Result(i, 2) = static_cast<float>(Vectors[i].Z);
        }
        
        return Result;
    }

    /**
     * Convert array of FLinearColors to Eigen matrix (Nx4)
     * @param Colors - Array of Unreal Engine colors
     * @return Eigen matrix (rows = colors, cols = 4 RGBA)
     */
    inline Eigen::MatrixXf ToEigenMatrix(const TArray<FLinearColor>& Colors)
    {
        Eigen::MatrixXf Result(Colors.Num(), 4);
        
        for (int32 i = 0; i < Colors.Num(); ++i)
        {
            Result(i, 0) = Colors[i].R;
            Result(i, 1) = Colors[i].G;
            Result(i, 2) = Colors[i].B;
            Result(i, 3) = Colors[i].A;
        }
        
        return Result;
    }

    // ========================================
    // VALIDATION
    // ========================================

    /**
     * Check if conversion is valid (no NaN or Inf values)
     * @param Vector - Eigen vector to validate
     * @return True if valid, false otherwise
     */
    template<typename Derived>
    inline bool IsConversionValid(const Eigen::MatrixBase<Derived>& Vector)
    {
        return Vector.allFinite();
    }

} // namespace UEToEigenConverter

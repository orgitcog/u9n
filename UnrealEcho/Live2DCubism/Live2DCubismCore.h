// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Live2D Cubism Core Integration

#pragma once

#include "CoreMinimal.h"
#include "Live2DCubismCore.generated.h"

/**
 * Live2D Cubism SDK Core Types and Structures
 * 
 * This header provides the core data structures and types for integrating
 * with the Live2D Cubism SDK. It abstracts the SDK's C API into Unreal-friendly
 * structures while maintaining full compatibility with the native SDK.
 */

// Forward declarations for Cubism SDK types
struct csmMoc;
struct csmModel;

/**
 * FCubismMocInfo
 * 
 * Information about a loaded .moc3 file.
 */
USTRUCT(BlueprintType)
struct FCubismMocInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 Version;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 ParameterCount;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 PartCount;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 DrawableCount;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	FVector2D CanvasSize;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float PixelsPerUnit;

	FCubismMocInfo()
		: Version(0)
		, ParameterCount(0)
		, PartCount(0)
		, DrawableCount(0)
		, CanvasSize(FVector2D::ZeroVector)
		, PixelsPerUnit(1.0f)
	{}
};

/**
 * FCubismParameter
 * 
 * A single parameter in a Live2D model.
 */
USTRUCT(BlueprintType)
struct FCubismParameter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	FName Id;

	UPROPERTY(BlueprintReadWrite, Category = "Cubism")
	float Value;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float MinValue;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float MaxValue;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float DefaultValue;

	FCubismParameter()
		: Id(NAME_None)
		, Value(0.0f)
		, MinValue(-1.0f)
		, MaxValue(1.0f)
		, DefaultValue(0.0f)
	{}
};

/**
 * FCubismPart
 * 
 * A part (drawable group) in a Live2D model.
 */
USTRUCT(BlueprintType)
struct FCubismPart
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	FName Id;

	UPROPERTY(BlueprintReadWrite, Category = "Cubism")
	float Opacity;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 ParentIndex;

	FCubismPart()
		: Id(NAME_None)
		, Opacity(1.0f)
		, ParentIndex(-1)
	{}
};

/**
 * FCubismDrawable
 * 
 * A drawable element in a Live2D model.
 */
USTRUCT(BlueprintType)
struct FCubismDrawable
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 TextureIndex;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 DrawOrder;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 RenderOrder;

	UPROPERTY(BlueprintReadWrite, Category = "Cubism")
	float Opacity;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	bool bIsDoubleSided;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	bool bIsInvertedMask;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 VertexCount;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	int32 IndexCount;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	TArray<int32> MaskIndices;

	FCubismDrawable()
		: Id(NAME_None)
		, TextureIndex(0)
		, DrawOrder(0)
		, RenderOrder(0)
		, Opacity(1.0f)
		, bIsDoubleSided(false)
		, bIsInvertedMask(false)
		, VertexCount(0)
		, IndexCount(0)
	{}
};

/**
 * FCubismMotion
 * 
 * A motion (animation) for a Live2D model.
 */
USTRUCT(BlueprintType)
struct FCubismMotion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	FName MotionId;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float Duration;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	bool bIsLooping;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float FadeInTime;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float FadeOutTime;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	TMap<FName, TArray<float>> ParameterCurves;

	FCubismMotion()
		: MotionId(NAME_None)
		, Duration(0.0f)
		, bIsLooping(false)
		, FadeInTime(0.0f)
		, FadeOutTime(0.0f)
	{}
};

/**
 * FCubismExpression
 * 
 * An expression preset for a Live2D model.
 */
USTRUCT(BlueprintType)
struct FCubismExpression
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	FName ExpressionId;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float FadeInTime;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	float FadeOutTime;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	TMap<FName, float> ParameterValues;

	UPROPERTY(BlueprintReadOnly, Category = "Cubism")
	TMap<FName, float> ParameterBlendModes; // 0=Override, 1=Add, 2=Multiply

	FCubismExpression()
		: ExpressionId(NAME_None)
		, FadeInTime(0.3f)
		, FadeOutTime(0.3f)
	{}
};

/**
 * FCubismPhysicsSettings
 * 
 * Physics simulation settings for a Live2D model.
 */
USTRUCT(BlueprintType)
struct FCubismPhysicsSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubism")
	FVector2D Gravity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubism")
	FVector2D Wind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubism")
	float FPS;

	FCubismPhysicsSettings()
		: Gravity(FVector2D(0.0f, -1.0f))
		, Wind(FVector2D::ZeroVector)
		, FPS(60.0f)
	{}
};

/**
 * ECubismBlendMode
 * 
 * Blend modes for Live2D drawables.
 */
UENUM(BlueprintType)
enum class ECubismBlendMode : uint8
{
	Normal,
	Additive,
	Multiplicative
};

/**
 * UCubismModelWrapper
 * 
 * Wrapper class for a Live2D Cubism model instance.
 * Provides safe access to the native SDK model.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UCubismModelWrapper : public UObject
{
	GENERATED_BODY()

public:
	UCubismModelWrapper();
	virtual ~UCubismModelWrapper();

	// Initialize from .moc3 data
	bool Initialize(const TArray<uint8>& MocData);

	// Release resources
	void Release();

	// Check if model is valid
	UFUNCTION(BlueprintPure, Category = "Cubism")
	bool IsValid() const;

	// Get model info
	UFUNCTION(BlueprintPure, Category = "Cubism")
	FCubismMocInfo GetMocInfo() const { return MocInfo; }

	// Parameter access
	UFUNCTION(BlueprintCallable, Category = "Cubism")
	void SetParameterValue(FName ParameterId, float Value);

	UFUNCTION(BlueprintPure, Category = "Cubism")
	float GetParameterValue(FName ParameterId) const;

	UFUNCTION(BlueprintPure, Category = "Cubism")
	TArray<FCubismParameter> GetAllParameters() const { return Parameters; }

	// Part access
	UFUNCTION(BlueprintCallable, Category = "Cubism")
	void SetPartOpacity(FName PartId, float Opacity);

	UFUNCTION(BlueprintPure, Category = "Cubism")
	float GetPartOpacity(FName PartId) const;

	UFUNCTION(BlueprintPure, Category = "Cubism")
	TArray<FCubismPart> GetAllParts() const { return Parts; }

	// Drawable access
	UFUNCTION(BlueprintPure, Category = "Cubism")
	TArray<FCubismDrawable> GetAllDrawables() const { return Drawables; }

	// Update model
	UFUNCTION(BlueprintCallable, Category = "Cubism")
	void Update();

	// Get vertex data for rendering
	TArray<FVector2D> GetDrawableVertices(int32 DrawableIndex) const;
	TArray<FVector2D> GetDrawableUVs(int32 DrawableIndex) const;
	TArray<int32> GetDrawableIndices(int32 DrawableIndex) const;

protected:
	// Native SDK pointers
	void* NativeMoc;
	void* NativeModel;
	TArray<uint8> MocBuffer;
	TArray<uint8> ModelBuffer;

	// Cached data
	FCubismMocInfo MocInfo;
	TArray<FCubismParameter> Parameters;
	TArray<FCubismPart> Parts;
	TArray<FCubismDrawable> Drawables;

	// Parameter lookup
	TMap<FName, int32> ParameterIndexMap;
	TMap<FName, int32> PartIndexMap;

	// Internal functions
	void CacheModelData();
	void ApplyParametersToNative();
	void ApplyPartsToNative();
	void UpdateFromNative();
};

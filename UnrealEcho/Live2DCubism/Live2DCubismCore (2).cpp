// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Live2D Cubism Core Implementation

#include "Live2DCubismCore.h"
#include "HAL/UnrealMemory.h"

// Conditional compilation for Cubism SDK
// When the SDK is available, define CUBISM_SDK_AVAILABLE
#if defined(CUBISM_SDK_AVAILABLE)
#include "CubismFramework.hpp"
#include "Model/CubismMoc.hpp"
#include "Model/CubismModel.hpp"
using namespace Live2D::Cubism::Framework;
#endif

//////////////////////////////////////////////////////////////////////////
// UCubismModelWrapper

UCubismModelWrapper::UCubismModelWrapper()
	: NativeMoc(nullptr)
	, NativeModel(nullptr)
{
}

UCubismModelWrapper::~UCubismModelWrapper()
{
	Release();
}

bool UCubismModelWrapper::Initialize(const TArray<uint8>& MocData)
{
	if (MocData.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UCubismModelWrapper: Empty MOC data"));
		return false;
	}

	// Store MOC data
	MocBuffer = MocData;

#if defined(CUBISM_SDK_AVAILABLE)
	// Use actual Cubism SDK
	
	// Create MOC from data
	NativeMoc = CubismMoc::Create(MocBuffer.GetData(), MocBuffer.Num());
	if (!NativeMoc)
	{
		UE_LOG(LogTemp, Error, TEXT("UCubismModelWrapper: Failed to create MOC"));
		return false;
	}

	// Create model from MOC
	NativeModel = static_cast<CubismMoc*>(NativeMoc)->CreateModel();
	if (!NativeModel)
	{
		UE_LOG(LogTemp, Error, TEXT("UCubismModelWrapper: Failed to create model from MOC"));
		CubismMoc::Delete(static_cast<CubismMoc*>(NativeMoc));
		NativeMoc = nullptr;
		return false;
	}

	// Cache model data
	CacheModelData();

#else
	// Fallback implementation without SDK
	// Parse MOC header to extract basic info
	
	if (MocBuffer.Num() < 64)
	{
		UE_LOG(LogTemp, Error, TEXT("UCubismModelWrapper: MOC data too small"));
		return false;
	}

	// MOC3 file signature check
	const char* MocSignature = "MOC3";
	bool bValidSignature = true;
	for (int32 i = 0; i < 4; i++)
	{
		if (MocBuffer[i] != MocSignature[i])
		{
			bValidSignature = false;
			break;
		}
	}

	if (!bValidSignature)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCubismModelWrapper: Invalid MOC3 signature, using fallback"));
	}

	// Parse MOC3 header (simplified)
	// Version is at offset 4 (1 byte)
	MocInfo.Version = MocBuffer[4];

	// Create placeholder native pointers
	NativeMoc = (void*)0x1;
	NativeModel = (void*)0x1;

	// Initialize default parameters for super-hot-girl aesthetic
	InitializeDefaultParameters();

	UE_LOG(LogTemp, Log, TEXT("UCubismModelWrapper: Initialized with fallback implementation (MOC version: %d)"), MocInfo.Version);

#endif

	return true;
}

void UCubismModelWrapper::Release()
{
#if defined(CUBISM_SDK_AVAILABLE)
	if (NativeModel)
	{
		CubismModel* Model = static_cast<CubismModel*>(NativeModel);
		delete Model;
		NativeModel = nullptr;
	}

	if (NativeMoc)
	{
		CubismMoc::Delete(static_cast<CubismMoc*>(NativeMoc));
		NativeMoc = nullptr;
	}
#else
	NativeMoc = nullptr;
	NativeModel = nullptr;
#endif

	MocBuffer.Empty();
	ModelBuffer.Empty();
	Parameters.Empty();
	Parts.Empty();
	Drawables.Empty();
	ParameterIndexMap.Empty();
	PartIndexMap.Empty();
}

bool UCubismModelWrapper::IsValid() const
{
	return NativeMoc != nullptr && NativeModel != nullptr;
}

void UCubismModelWrapper::SetParameterValue(FName ParameterId, float Value)
{
	if (!IsValid())
	{
		return;
	}

	int32* IndexPtr = ParameterIndexMap.Find(ParameterId);
	if (IndexPtr && *IndexPtr >= 0 && *IndexPtr < Parameters.Num())
	{
		FCubismParameter& Param = Parameters[*IndexPtr];
		Param.Value = FMath::Clamp(Value, Param.MinValue, Param.MaxValue);

#if defined(CUBISM_SDK_AVAILABLE)
		CubismModel* Model = static_cast<CubismModel*>(NativeModel);
		Model->SetParameterValue(*IndexPtr, Param.Value);
#endif
	}
}

float UCubismModelWrapper::GetParameterValue(FName ParameterId) const
{
	if (!IsValid())
	{
		return 0.0f;
	}

	const int32* IndexPtr = ParameterIndexMap.Find(ParameterId);
	if (IndexPtr && *IndexPtr >= 0 && *IndexPtr < Parameters.Num())
	{
		return Parameters[*IndexPtr].Value;
	}

	return 0.0f;
}

void UCubismModelWrapper::SetPartOpacity(FName PartId, float Opacity)
{
	if (!IsValid())
	{
		return;
	}

	int32* IndexPtr = PartIndexMap.Find(PartId);
	if (IndexPtr && *IndexPtr >= 0 && *IndexPtr < Parts.Num())
	{
		Parts[*IndexPtr].Opacity = FMath::Clamp(Opacity, 0.0f, 1.0f);

#if defined(CUBISM_SDK_AVAILABLE)
		CubismModel* Model = static_cast<CubismModel*>(NativeModel);
		Model->SetPartOpacity(*IndexPtr, Parts[*IndexPtr].Opacity);
#endif
	}
}

float UCubismModelWrapper::GetPartOpacity(FName PartId) const
{
	if (!IsValid())
	{
		return 1.0f;
	}

	const int32* IndexPtr = PartIndexMap.Find(PartId);
	if (IndexPtr && *IndexPtr >= 0 && *IndexPtr < Parts.Num())
	{
		return Parts[*IndexPtr].Opacity;
	}

	return 1.0f;
}

void UCubismModelWrapper::Update()
{
	if (!IsValid())
	{
		return;
	}

#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);
	Model->Update();
	UpdateFromNative();
#else
	// Fallback: Just apply parameters (no actual model update)
	ApplyParametersToNative();
	ApplyPartsToNative();
#endif
}

TArray<FVector2D> UCubismModelWrapper::GetDrawableVertices(int32 DrawableIndex) const
{
	TArray<FVector2D> Vertices;

	if (!IsValid() || DrawableIndex < 0 || DrawableIndex >= Drawables.Num())
	{
		return Vertices;
	}

#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);
	const float* VertexData = Model->GetDrawableVertices(DrawableIndex);
	int32 VertexCount = Model->GetDrawableVertexCount(DrawableIndex);

	Vertices.Reserve(VertexCount);
	for (int32 i = 0; i < VertexCount; i++)
	{
		Vertices.Add(FVector2D(VertexData[i * 2], VertexData[i * 2 + 1]));
	}
#else
	// Fallback: Return empty array
#endif

	return Vertices;
}

TArray<FVector2D> UCubismModelWrapper::GetDrawableUVs(int32 DrawableIndex) const
{
	TArray<FVector2D> UVs;

	if (!IsValid() || DrawableIndex < 0 || DrawableIndex >= Drawables.Num())
	{
		return UVs;
	}

#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);
	const float* UVData = Model->GetDrawableVertexUvs(DrawableIndex);
	int32 VertexCount = Model->GetDrawableVertexCount(DrawableIndex);

	UVs.Reserve(VertexCount);
	for (int32 i = 0; i < VertexCount; i++)
	{
		UVs.Add(FVector2D(UVData[i * 2], UVData[i * 2 + 1]));
	}
#else
	// Fallback: Return empty array
#endif

	return UVs;
}

TArray<int32> UCubismModelWrapper::GetDrawableIndices(int32 DrawableIndex) const
{
	TArray<int32> Indices;

	if (!IsValid() || DrawableIndex < 0 || DrawableIndex >= Drawables.Num())
	{
		return Indices;
	}

#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);
	const uint16* IndexData = Model->GetDrawableVertexIndices(DrawableIndex);
	int32 IndexCount = Model->GetDrawableIndexCount(DrawableIndex);

	Indices.Reserve(IndexCount);
	for (int32 i = 0; i < IndexCount; i++)
	{
		Indices.Add(IndexData[i]);
	}
#else
	// Fallback: Return empty array
#endif

	return Indices;
}

void UCubismModelWrapper::CacheModelData()
{
#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);

	// Cache MOC info
	MocInfo.ParameterCount = Model->GetParameterCount();
	MocInfo.PartCount = Model->GetPartCount();
	MocInfo.DrawableCount = Model->GetDrawableCount();
	MocInfo.CanvasSize = FVector2D(Model->GetCanvasWidth(), Model->GetCanvasHeight());
	MocInfo.PixelsPerUnit = Model->GetPixelsPerUnit();

	// Cache parameters
	Parameters.Empty();
	Parameters.Reserve(MocInfo.ParameterCount);
	for (int32 i = 0; i < MocInfo.ParameterCount; i++)
	{
		FCubismParameter Param;
		Param.Id = FName(Model->GetParameterId(i)->GetString().GetRawString());
		Param.Value = Model->GetParameterValue(i);
		Param.MinValue = Model->GetParameterMinimumValue(i);
		Param.MaxValue = Model->GetParameterMaximumValue(i);
		Param.DefaultValue = Model->GetParameterDefaultValue(i);
		Parameters.Add(Param);
		ParameterIndexMap.Add(Param.Id, i);
	}

	// Cache parts
	Parts.Empty();
	Parts.Reserve(MocInfo.PartCount);
	for (int32 i = 0; i < MocInfo.PartCount; i++)
	{
		FCubismPart Part;
		Part.Id = FName(Model->GetPartId(i)->GetString().GetRawString());
		Part.Opacity = Model->GetPartOpacity(i);
		Part.ParentIndex = Model->GetPartParentPartIndex(i);
		Parts.Add(Part);
		PartIndexMap.Add(Part.Id, i);
	}

	// Cache drawables
	Drawables.Empty();
	Drawables.Reserve(MocInfo.DrawableCount);
	for (int32 i = 0; i < MocInfo.DrawableCount; i++)
	{
		FCubismDrawable Drawable;
		Drawable.Id = FName(Model->GetDrawableId(i)->GetString().GetRawString());
		Drawable.TextureIndex = Model->GetDrawableTextureIndex(i);
		Drawable.DrawOrder = Model->GetDrawableDrawOrder(i);
		Drawable.RenderOrder = Model->GetDrawableRenderOrder(i);
		Drawable.Opacity = Model->GetDrawableOpacity(i);
		Drawable.bIsDoubleSided = Model->GetDrawableCulling(i) == 0;
		Drawable.VertexCount = Model->GetDrawableVertexCount(i);
		Drawable.IndexCount = Model->GetDrawableIndexCount(i);

		// Cache mask indices
		const int32* MaskIndices = Model->GetDrawableMasks(i);
		int32 MaskCount = Model->GetDrawableMaskCount(i);
		for (int32 j = 0; j < MaskCount; j++)
		{
			Drawable.MaskIndices.Add(MaskIndices[j]);
		}

		Drawables.Add(Drawable);
	}

#else
	// Fallback: Initialize with default parameters
	InitializeDefaultParameters();
#endif
}

void UCubismModelWrapper::ApplyParametersToNative()
{
#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);
	for (int32 i = 0; i < Parameters.Num(); i++)
	{
		Model->SetParameterValue(i, Parameters[i].Value);
	}
#endif
}

void UCubismModelWrapper::ApplyPartsToNative()
{
#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);
	for (int32 i = 0; i < Parts.Num(); i++)
	{
		Model->SetPartOpacity(i, Parts[i].Opacity);
	}
#endif
}

void UCubismModelWrapper::UpdateFromNative()
{
#if defined(CUBISM_SDK_AVAILABLE)
	CubismModel* Model = static_cast<CubismModel*>(NativeModel);

	// Update drawable render orders (may change after model update)
	for (int32 i = 0; i < Drawables.Num(); i++)
	{
		Drawables[i].RenderOrder = Model->GetDrawableRenderOrder(i);
		Drawables[i].Opacity = Model->GetDrawableOpacity(i);
	}
#endif
}

#if !defined(CUBISM_SDK_AVAILABLE)
void UCubismModelWrapper::InitializeDefaultParameters()
{
	// Initialize default parameters for super-hot-girl aesthetic
	// These match the standard Live2D parameter names

	auto AddParameter = [this](const FString& Id, float DefaultValue, float MinValue, float MaxValue)
	{
		FCubismParameter Param;
		Param.Id = FName(*Id);
		Param.Value = DefaultValue;
		Param.DefaultValue = DefaultValue;
		Param.MinValue = MinValue;
		Param.MaxValue = MaxValue;
		Parameters.Add(Param);
		ParameterIndexMap.Add(Param.Id, Parameters.Num() - 1);
	};

	// Facial expression parameters
	AddParameter(TEXT("ParamEyeLOpen"), 1.0f, 0.0f, 1.0f);
	AddParameter(TEXT("ParamEyeROpen"), 1.0f, 0.0f, 1.0f);
	AddParameter(TEXT("ParamEyeBallX"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamEyeBallY"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamBrowLY"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamBrowRY"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamBrowLX"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamBrowRX"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamBrowLAngle"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamBrowRAngle"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamMouthForm"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamMouthOpenY"), 0.0f, 0.0f, 1.0f);
	AddParameter(TEXT("ParamCheek"), 0.0f, 0.0f, 1.0f);

	// Body parameters
	AddParameter(TEXT("ParamAngleX"), 0.0f, -30.0f, 30.0f);
	AddParameter(TEXT("ParamAngleY"), 0.0f, -30.0f, 30.0f);
	AddParameter(TEXT("ParamAngleZ"), 0.0f, -30.0f, 30.0f);
	AddParameter(TEXT("ParamBodyAngleX"), 0.0f, -10.0f, 10.0f);
	AddParameter(TEXT("ParamBodyAngleY"), 0.0f, -10.0f, 10.0f);
	AddParameter(TEXT("ParamBodyAngleZ"), 0.0f, -10.0f, 10.0f);
	AddParameter(TEXT("ParamBreath"), 0.0f, 0.0f, 1.0f);

	// Hair physics parameters
	AddParameter(TEXT("ParamHairFront"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamHairSide"), 0.0f, -1.0f, 1.0f);
	AddParameter(TEXT("ParamHairBack"), 0.0f, -1.0f, 1.0f);

	// Super-hot-girl aesthetic parameters
	AddParameter(TEXT("ParamEyeSparkle"), 0.5f, 0.0f, 1.0f);
	AddParameter(TEXT("ParamBlush"), 0.0f, 0.0f, 1.0f);
	AddParameter(TEXT("ParamHairShine"), 0.5f, 0.0f, 1.0f);

	// Update MOC info
	MocInfo.ParameterCount = Parameters.Num();
	MocInfo.CanvasSize = FVector2D(1024.0f, 1024.0f);
	MocInfo.PixelsPerUnit = 1.0f;

	UE_LOG(LogTemp, Log, TEXT("UCubismModelWrapper: Initialized %d default parameters"), Parameters.Num());
}
#endif

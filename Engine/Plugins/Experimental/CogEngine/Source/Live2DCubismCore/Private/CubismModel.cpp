// Copyright Epic Games, Inc. All Rights Reserved.

#include "CubismModel.h"

UCubismModel::UCubismModel()
	: ModelScale(1.0f)
	, bAutoUpdate(true)
	, AnimationTime(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCubismModel::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Log, TEXT("CubismModel: Component started for model %s"), *ModelData.ModelID);
}

void UCubismModel::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bAutoUpdate)
	{
		UpdateModel(DeltaTime);
	}
}

bool UCubismModel::LoadModel(const FString& ModelPath)
{
	ModelData.ModelPath = ModelPath;
	ModelData.ModelID = FPaths::GetBaseFilename(ModelPath);
	
	// In a real implementation, this would load the actual Live2D model file
	// For now, we'll initialize with default parameters
	ModelData.Parameters.Empty();
	ModelData.Parameters.Add(TEXT("AngleX"), 0.0f);
	ModelData.Parameters.Add(TEXT("AngleY"), 0.0f);
	ModelData.Parameters.Add(TEXT("AngleZ"), 0.0f);
	ModelData.Parameters.Add(TEXT("EyeOpen"), 1.0f);
	ModelData.Parameters.Add(TEXT("MouthOpen"), 0.0f);
	ModelData.Parameters.Add(TEXT("BodyAngleX"), 0.0f);
	
	UE_LOG(LogTemp, Log, TEXT("CubismModel: Loaded model %s from %s"), *ModelData.ModelID, *ModelPath);
	
	return true;
}

void UCubismModel::SetParameter(const FString& ParameterName, float Value)
{
	ModelData.Parameters.Add(ParameterName, Value);
}

float UCubismModel::GetParameter(const FString& ParameterName) const
{
	const float* ValuePtr = ModelData.Parameters.Find(ParameterName);
	return ValuePtr ? *ValuePtr : 0.0f;
}

void UCubismModel::PlayAnimation(const FString& AnimationName)
{
	ModelData.CurrentAnimation = AnimationName;
	AnimationTime = 0.0f;
	
	UE_LOG(LogTemp, Log, TEXT("CubismModel: Playing animation %s"), *AnimationName);
}

void UCubismModel::UpdateModel(float DeltaTime)
{
	AnimationTime += DeltaTime;
	
	UpdateParameters(DeltaTime);
	RenderModel();
}

void UCubismModel::ResetModel()
{
	AnimationTime = 0.0f;
	ModelData.CurrentAnimation = TEXT("Idle");
	
	// Reset parameters to defaults
	for (auto& Param : ModelData.Parameters)
	{
		if (Param.Key == TEXT("EyeOpen"))
		{
			Param.Value = 1.0f;
		}
		else
		{
			Param.Value = 0.0f;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("CubismModel: Model reset"));
}

void UCubismModel::UpdateParameters(float DeltaTime)
{
	// Update parameters based on animation
	// In a real implementation, this would process Live2D animation data
	
	// Example: Simple breathing animation
	if (ModelData.CurrentAnimation == TEXT("Idle"))
	{
		float BreathingCycle = FMath::Sin(AnimationTime * 2.0f) * 0.1f;
		SetParameter(TEXT("BodyAngleX"), BreathingCycle);
	}
}

void UCubismModel::RenderModel()
{
	// In a real implementation, this would render the Live2D model
	// using the Cubism SDK rendering functions
}

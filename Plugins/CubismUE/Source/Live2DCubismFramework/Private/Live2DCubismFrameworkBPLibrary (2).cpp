/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */


#include "Live2DCubismFrameworkBPLibrary.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Model/CubismParameterComponent.h"

ACubismModel* ULive2DCubismFrameworkBPLibrary::SpawnCubismModel(
	UObject* WorldContextObject,
	UCubismModel3Json* Model3Json,
	const FTransform& Transform,
	const bool bRenderInWorldSpace,
	UTextureRenderTarget2D* RenderTarget
)
{
	if (!WorldContextObject || !Model3Json)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnCubismModelFromJson: Invalid context or model asset."));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	if (!World)
	{
		return nullptr;
	}

	ACubismModel* ModelActor = World->SpawnActor<ACubismModel>(ACubismModel::StaticClass());
	if (!ModelActor)
	{
		return nullptr;
	}

	ModelActor->Initialize(Model3Json);
	ModelActor->SetActorTransform(Transform);
	ModelActor->Model->bRenderInWorldSpace = bRenderInWorldSpace;
	ModelActor->Model->SetVisibility(bRenderInWorldSpace, true);
	ModelActor->Model->RenderTarget = RenderTarget;

	return ModelActor;
}

TArray<FString> ULive2DCubismFrameworkBPLibrary::GetAllParameterNames(UCubismModelComponent* ModelComponent)
{
	TArray<FString> ParameterNames;
	
	if (!ModelComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetAllParameterNames: ModelComponent is null."));
		return ParameterNames;
	}

	const int32 ParameterCount = ModelComponent->GetParameterCount();
	ParameterNames.Reserve(ParameterCount);

	for (int32 i = 0; i < ParameterCount; ++i)
	{
		ParameterNames.Add(ModelComponent->GetParameterId(i));
	}

	return ParameterNames;
}

TArray<FString> ULive2DCubismFrameworkBPLibrary::GetAllDrawableNames(UCubismModelComponent* ModelComponent)
{
	TArray<FString> DrawableNames;
	
	if (!ModelComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetAllDrawableNames: ModelComponent is null."));
		return DrawableNames;
	}

	const int32 DrawableCount = ModelComponent->GetDrawableCount();
	DrawableNames.Reserve(DrawableCount);

	for (int32 i = 0; i < DrawableCount; ++i)
	{
		DrawableNames.Add(ModelComponent->GetDrawableId(i));
	}

	return DrawableNames;
}

bool ULive2DCubismFrameworkBPLibrary::SetParameterByName(
	UCubismModelComponent* ModelComponent,
	const FString& ParameterName,
	float Value,
	ECubismParameterBlendMode BlendMode,
	float Weight
)
{
	if (!ModelComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetParameterByName: ModelComponent is null."));
		return false;
	}

	UCubismParameterComponent* Parameter = ModelComponent->GetParameter(ParameterName);
	if (!Parameter)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetParameterByName: Parameter '%s' not found."), *ParameterName);
		return false;
	}

	// Apply value with specified blend mode
	switch (BlendMode)
	{
		case ECubismParameterBlendMode::Overwrite:
			Parameter->SetParameterValue(Value, Weight);
			break;
		case ECubismParameterBlendMode::Additive:
			Parameter->AddParameterValue(Value, Weight);
			break;
		case ECubismParameterBlendMode::Multiplicative:
			Parameter->MultiplyParameterValue(Value, Weight);
			break;
	}

	return true;
}

bool ULive2DCubismFrameworkBPLibrary::GetParameterByName(
	UCubismModelComponent* ModelComponent,
	const FString& ParameterName,
	float& OutValue
)
{
	if (!ModelComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetParameterByName: ModelComponent is null."));
		OutValue = 0.0f;
		return false;
	}

	UCubismParameterComponent* Parameter = ModelComponent->GetParameter(ParameterName);
	if (!Parameter)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetParameterByName: Parameter '%s' not found."), *ParameterName);
		OutValue = 0.0f;
		return false;
	}

	OutValue = Parameter->Value;
	return true;
}

bool ULive2DCubismFrameworkBPLibrary::HasCubismComponent(ACubismModel* ModelActor, TSubclassOf<UActorComponent> ComponentClass)
{
	if (!ModelActor || !ComponentClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("HasCubismComponent: Invalid actor or component class."));
		return false;
	}

	return ModelActor->FindComponentByClass(ComponentClass) != nullptr;
}

bool ULive2DCubismFrameworkBPLibrary::IsValidParameter(UCubismModelComponent* ModelComponent, const FString& ParameterName)
{
	if (!ModelComponent)
	{
		return false;
	}

	return ModelComponent->GetParameter(ParameterName) != nullptr;
}

bool ULive2DCubismFrameworkBPLibrary::IsValidDrawable(UCubismModelComponent* ModelComponent, const FString& DrawableName)
{
	if (!ModelComponent)
	{
		return false;
	}

	return ModelComponent->GetDrawable(DrawableName) != nullptr;
}

bool ULive2DCubismFrameworkBPLibrary::GetParameterRange(
	UCubismModelComponent* ModelComponent,
	const FString& ParameterName,
	float& OutMin,
	float& OutMax,
	float& OutDefault
)
{
	if (!ModelComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetParameterRange: ModelComponent is null."));
		OutMin = 0.0f;
		OutMax = 0.0f;
		OutDefault = 0.0f;
		return false;
	}

	UCubismParameterComponent* Parameter = ModelComponent->GetParameter(ParameterName);
	if (!Parameter)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetParameterRange: Parameter '%s' not found."), *ParameterName);
		OutMin = 0.0f;
		OutMax = 0.0f;
		OutDefault = 0.0f;
		return false;
	}

	OutMin = Parameter->MinimumValue;
	OutMax = Parameter->MaximumValue;
	OutDefault = Parameter->DefaultValue;
	return true;
}

float ULive2DCubismFrameworkBPLibrary::ClampParameterValue(
	UCubismModelComponent* ModelComponent,
	const FString& ParameterName,
	float Value
)
{
	if (!ModelComponent)
	{
		return Value;
	}

	UCubismParameterComponent* Parameter = ModelComponent->GetParameter(ParameterName);
	if (!Parameter)
	{
		return Value;
	}

	return FMath::Clamp(Value, Parameter->MinimumValue, Parameter->MaximumValue);
}

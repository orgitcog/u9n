// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFaceAnimationSolver.h"
#include "MetaHumanConfig.h"
#include "MetaHumanConfigLog.h"
#include "UObject/Package.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"



#if WITH_EDITOR

void UMetaHumanFaceAnimationSolver::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);
	NotifyInternalsChanged();
}

void UMetaHumanFaceAnimationSolver::PostTransacted(const FTransactionObjectEvent& InTransactionEvent)
{
	Super::PostTransacted(InTransactionEvent);
	NotifyInternalsChanged();
}

#endif

bool UMetaHumanFaceAnimationSolver::CanProcess() const
{
	return bOverrideDeviceConfig == false || DeviceConfig != nullptr;
}

bool UMetaHumanFaceAnimationSolver::SettingsOverridden() const
{
	return bOverrideDepthMapInfluence || bOverrideEyeSolveSmoothness || bOverrideTeethMode;
}

bool UMetaHumanFaceAnimationSolver::GetConfigDisplayName(UCaptureData* InCaptureData, FString& OutName) const
{
	bool bSpecifiedCaptureData = true;

	if (bOverrideDeviceConfig && DeviceConfig)
	{
		OutName = DeviceConfig->Name;
	}
	else
	{
		bSpecifiedCaptureData = FMetaHumanConfig::GetInfo(InCaptureData, TEXT("Solver"), OutName);
	}

	if (SettingsOverridden())
	{
		OutName += TEXT("*");
	}

	return bSpecifiedCaptureData;
}

FString UMetaHumanFaceAnimationSolver::GetSolverTemplateData(UCaptureData* InCaptureData) const
{
	return GetEffectiveConfig(InCaptureData)->GetSolverTemplateData();
}

FString UMetaHumanFaceAnimationSolver::GetSolverConfigData(UCaptureData* InCaptureData) const
{
	FString SolverConfigData = GetEffectiveConfig(InCaptureData)->GetSolverConfigData();

	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(SolverConfigData);

	TSharedPtr<FJsonObject> JsonObject = nullptr;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject)
	{
		if (bOverrideDepthMapInfluence)
		{
			const TSharedPtr<FJsonObject>* PCAObject = nullptr;;
			if (JsonObject->TryGetObjectField(TEXT("pca"), PCAObject) && PCAObject)
			{
				const TSharedPtr<FJsonObject>* ICPObject = nullptr;
				if ((*PCAObject)->TryGetObjectField(TEXT("ICP Constraints Configuration"), ICPObject) && ICPObject)
				{
					float GeometryWeightField;
					if ((*ICPObject)->TryGetNumberField(TEXT("geometryWeight"), GeometryWeightField))
					{
						float GeometryWeight = 0;

						switch (DepthMapInfluence)
						{
						case EDepthMapInfluenceValue::None:
							GeometryWeight = 0;
							break;
						case EDepthMapInfluenceValue::Low:
							GeometryWeight = 0.5;
							break;
						case EDepthMapInfluenceValue::High:
							GeometryWeight = 1.0;
							break;
						default:
							GeometryWeight = 0;
							break;
						}

						(*ICPObject)->SetNumberField(TEXT("geometryWeight"), GeometryWeight);
					}
					else
					{
						UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'geometryWeight' field"));
					}

					bool bUseActorDistanceWeight;
					if ((*ICPObject)->TryGetBoolField(TEXT("useActorDistanceWeight"), bUseActorDistanceWeight))
					{
						(*ICPObject)->SetBoolField(TEXT("useActorDistanceWeight"), false);
					}
					else
					{
						UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'useActorDistanceWeight' field"));
					}
				}
				else
				{
					UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'icp' field"));
				}
			}
			else
			{
				UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'pca' field"));
			}
		}

		if (bOverrideEyeSolveSmoothness)
		{
			const TSharedPtr<FJsonObject>* EyeSolveObject = nullptr;;
			if (JsonObject->TryGetObjectField(TEXT("eyesolve"), EyeSolveObject) && EyeSolveObject)
			{
				const TSharedPtr<FJsonObject>* EyeTrackingObject = nullptr;
				if ((*EyeSolveObject)->TryGetObjectField(TEXT("Eye Tracking"), EyeTrackingObject) && EyeTrackingObject)
				{
					float SmoothnessField;
					if ((*EyeTrackingObject)->TryGetNumberField(TEXT("smoothness"), SmoothnessField))
					{
						(*EyeTrackingObject)->SetNumberField(TEXT("smoothness"), EyeSolveSmoothness * 10);
					}
					else
					{
						UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'smoothness' field"));
					}
				}
				else
				{
					UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'eye tracking' field"));
				}
			}
			else
			{
				UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'eyesolve' field"));
			}
		}

		if (bOverrideTeethMode)
		{
			const TSharedPtr<FJsonObject>* TeethSolveObject = nullptr;;
			if (JsonObject->TryGetObjectField(TEXT("teethsolve"), TeethSolveObject) && TeethSolveObject)
			{
				const TSharedPtr<FJsonObject>* TeethTrackingObject = nullptr;
				if ((*TeethSolveObject)->TryGetObjectField(TEXT("Teeth Tracking"), TeethTrackingObject) && TeethTrackingObject)
				{
					bool PoseBasedOnlyField;
					if ((*TeethTrackingObject)->TryGetBoolField(TEXT("poseBasedOnly"), PoseBasedOnlyField))
					{
						(*TeethTrackingObject)->SetBoolField(TEXT("poseBasedOnly"), TeethMode == ETeethMode::Estimated);
					}
					else
					{
						UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'poseBasedOnly' field"));
					}
				}
				else
				{
					UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'teeth tracking' field"));
				}
			}
			else
			{
				UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to find 'teethsolve' field"));
			}
		}

		SolverConfigData = JsonObjectAsString(JsonObject);
	}
	else
	{
		UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to deserialize json"));
	}

	return SolverConfigData;
}

FString UMetaHumanFaceAnimationSolver::GetSolverDefinitionsData(UCaptureData* InCaptureData) const
{
	return GetEffectiveConfig(InCaptureData)->GetSolverDefinitionsData();
}

FString UMetaHumanFaceAnimationSolver::GetSolverHierarchicalDefinitionsData(UCaptureData* InCaptureData) const
{
	return GetEffectiveConfig(InCaptureData)->GetSolverHierarchicalDefinitionsData();
}

FString UMetaHumanFaceAnimationSolver::GetSolverPCAFromDNAData(UCaptureData* InCaptureData) const
{
	return GetEffectiveConfig(InCaptureData)->GetSolverPCAFromDNAData();
}

UMetaHumanFaceAnimationSolver::FOnInternalsChanged& UMetaHumanFaceAnimationSolver::OnInternalsChanged()
{
	return OnInternalsChangedDelegate;
}

UMetaHumanConfig* UMetaHumanFaceAnimationSolver::GetEffectiveConfig(UCaptureData* InCaptureData) const
{
	if (bOverrideDeviceConfig && DeviceConfig)
	{
		return DeviceConfig;
	}
	else
	{
		UMetaHumanConfig* Config = nullptr;
		FMetaHumanConfig::GetInfo(InCaptureData, TEXT("Solver"), Config);

		if (Config)
		{
			return Config;
		}
		else
		{
			check(false);
		}
	}

	return nullptr;
}

FString UMetaHumanFaceAnimationSolver::JsonObjectAsString(TSharedPtr<FJsonObject> InJsonObject) const
{
	FString JsonString;

	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);

	if (!InJsonObject || !FJsonSerializer::Serialize(InJsonObject.ToSharedRef(), JsonWriter))
	{
		UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to serialize json"));
	}

	return JsonString;
}

void UMetaHumanFaceAnimationSolver::NotifyInternalsChanged()
{
	OnInternalsChangedDelegate.Broadcast();
}

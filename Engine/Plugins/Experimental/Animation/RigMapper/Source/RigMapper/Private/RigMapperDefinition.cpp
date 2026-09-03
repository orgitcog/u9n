// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigMapperDefinition.h"

#include "RigMapper.h"
#include "RigMapperProcessor.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "RigMapperLog.h"

bool FRigMapperFeature::IsValid(const TArray<FString>& InputNames, bool bWarn) const
{
	bool bValid = true;
	
	if (Name.IsEmpty())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Invalid (empty) feature name"))
		bValid = false;
	}

	TArray<FString> FeatureInputs;
	GetInputs(FeatureInputs);
	
	if (FeatureInputs.Contains(Name))
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Feature %s is referencing itself"), *Name)
		bValid = false;
	}

	if (FeatureInputs.IsEmpty())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Feature %s does not reference any input"), *Name)
		bValid = false;
	}
	
	for (const FString& Input : FeatureInputs)
	{
		if (!InputNames.Contains(Input))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Undefined input or feature %s referenced in feature %s"), *Input, *Name)
			bValid = false;
		}
	}
	
	return bValid;
}

bool FRigMapperFeature::GetJsonArray(TSharedPtr<FJsonObject> JsonObject, TArray<TSharedPtr<FJsonValue>>& OutArray, const FString& Identifier, const FString& OwnerIdentifier) const
{
	if (!OwnerIdentifier.IsEmpty())
	{
		if (!JsonObject->HasTypedField<EJson::Object>(*OwnerIdentifier))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Missing '%s' field for feature %s"), *OwnerIdentifier, *Name)
			return false;
		}
		JsonObject = JsonObject->GetObjectField(OwnerIdentifier);
	}
	if (!JsonObject->HasTypedField<EJson::Array>(*Identifier))
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Missing '%s' field for feature %s"), *Identifier, *Name)
		return false;
	}
	OutArray = JsonObject->GetArrayField(*Identifier);
	return true;
}

bool FRigMapperMultiplyFeature::LoadFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	TArray<TSharedPtr<FJsonValue>> InputFeatures;
	
	if (!GetJsonArray(JsonObject, InputFeatures, TEXT("input_features")))
	{
		return false;
	}
	if (InputFeatures.Num() < 2)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Feature %s does not reference enough input"), *Name)
		return false;
	}
	for (const auto& InputFeature : InputFeatures)
	{
		Inputs.Add(InputFeature->AsString());
	}
	
	return true;
}

bool FRigMapperMultiplyFeature::IsValid(const TArray<FString>& InputNames, bool bWarn) const
{
	bool bValid = FRigMapperFeature::IsValid(InputNames, bWarn);
	
	if (bValid && Inputs.Num() < 2)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Feature %s does not reference enough input"), *Name)
		return false;
	}
	
	return bValid;
}

bool FRigMapperWsFeature::LoadFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	TArray<TSharedPtr<FJsonValue>> JsonInputs;
	if (!GetJsonArray(JsonObject, JsonInputs, TEXT("input_features")))
	{
		return false;
	}
	
	TArray<TSharedPtr<FJsonValue>> JsonWeights;
	if (!GetJsonArray(JsonObject, JsonWeights, TEXT("weights"), TEXT("params")))
	{
		return false;
	}
	if (JsonWeights.Num() != JsonInputs.Num())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Number of inputs does not match number of weights for feature %s"), *Name)
		return false;
	}
	for (int32 InputIndex = 0; InputIndex < JsonInputs.Num(); InputIndex++)
	{
		Inputs.Add(JsonInputs[InputIndex]->AsString(), JsonWeights[InputIndex]->AsNumber());
	}

	Range.bHasLowerBound = JsonObject->GetObjectField(TEXT("params"))->HasField(TEXT("min"));
	if (Range.bHasLowerBound)
	{
		Range.LowerBound = JsonObject->GetObjectField(TEXT("params"))->GetNumberField(TEXT("min"));
	}
	Range.bHasUpperBound = JsonObject->GetObjectField(TEXT("params"))->HasField(TEXT("max"));
	if (Range.bHasUpperBound)
	{
		Range.UpperBound = JsonObject->GetObjectField(TEXT("params"))->GetNumberField(TEXT("max"));
	}
	if (Range.bHasLowerBound && Range.bHasUpperBound && Range.LowerBound > Range.UpperBound)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Invalid range for feature %s"), *Name)
		return false;
	}
	return true;
}

bool FRigMapperWsFeature::IsValid(const TArray<FString>& InputNames, bool bWarn) const
{
	bool bValid = FRigMapperFeature::IsValid(InputNames, bWarn);

	double TotalWeight = 0;
	for (const TPair<FString, double>& InputWeight : Inputs)
	{
		TotalWeight += InputWeight.Value;
	}
	if (bWarn && TotalWeight == 0)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Total Weights for feature %s add up to 0"), *Name)
	}
	if (bWarn && !Range.bHasLowerBound && TotalWeight < -1.000001f)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Total Weights for feature %s are quite low (%f) even though a lower range bound was not set"), *Name, TotalWeight)
	}
	if (bWarn && !Range.bHasUpperBound && TotalWeight > 1.000001f)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Total Weights for feature %s are quite high (%f) even though an upper range bound was not set"), *Name, TotalWeight)
	}
	if (Range.bHasLowerBound && Range.bHasUpperBound && Range.LowerBound > Range.UpperBound)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Range of [%f-%f] for feature %s is invalid"), Range.LowerBound, Range.UpperBound, *Name)
		bValid = false;
	}
	
	return bValid;
}

void FRigMapperWsFeature::GetInputs(TArray<FString>& OutInputs) const
{
	Inputs.GenerateKeyArray(OutInputs);
}

bool FRigMapperSdkFeature::LoadFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	TArray<TSharedPtr<FJsonValue>> JsonInputs;
	if (!GetJsonArray(JsonObject, JsonInputs, TEXT("input_features")))
	{
		return false;
	}
	if (JsonInputs.Num() != 1)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Sdk feature %s should have a single element in the 'input_features' array field"), *Name)
		return false;
	}
	Input = JsonInputs[0]->AsString();

	TArray<TSharedPtr<FJsonValue>> InValues;
	if (!GetJsonArray(JsonObject, InValues, TEXT("in_val"), TEXT("params")))
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> OutValues;
	if (!GetJsonArray(JsonObject, OutValues, TEXT("out_val"), TEXT("params")))
	{
		return false;
	}

	if (InValues.Num() < 2)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Not enough keys for SDK feature %s (expected minimum 2, got %d)"), *Name, InValues.Num())
	}
	if (InValues.Num() != OutValues.Num())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Number of input values does not match number of output values for feature %s"), *Name)
		return false;
	}
	for (int32 ValueIndex = 0 ; ValueIndex < InValues.Num(); ValueIndex++)
	{
		Keys.Add({ InValues[ValueIndex]->AsNumber(), OutValues[ValueIndex]->AsNumber() });
	}
	Keys.Sort([](const FRigMapperSdkKey& Key1, const FRigMapperSdkKey Key2) { return Key1.In < Key2.In; });
	
	return true;
}

bool FRigMapperSdkFeature::IsValid(const TArray<FString>& InputNames, bool bWarn) const
{
	bool bValid = FRigMapperFeature::IsValid(InputNames, bWarn);

	if (Keys.Num() < 2)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Not enough keys for SDK feature %s (expected minimum 2, got %d)"), *Name, Keys.Num())
		bValid = false;
	}
	
	return bValid;
}

bool FRigMapperFeatureDefinitions::AddFromJsonObject(const FString& FeatureName, const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject->HasTypedField<EJson::String>(TEXT("type")))
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Missing 'type' field for feature %s"), *FeatureName)
		return false;
	}
	const FString FeatureType = JsonObject->GetStringField(TEXT("type"));

	bool bValidFeature = false;
	
	if (FeatureType == "weighted_sum")
	{
		WeightedSums.Add(FRigMapperWsFeature(FeatureName));
		bValidFeature = WeightedSums.Last().LoadFromJsonObject(JsonObject);
	}
	else if (FeatureType == "sdk")
	{
		SDKs.Add(FRigMapperSdkFeature(FeatureName));
		bValidFeature = SDKs.Last().LoadFromJsonObject(JsonObject);
	}
	else if (FeatureType == "multiply")
	{
		Multiply.Add(FRigMapperMultiplyFeature(FeatureName));
		bValidFeature = Multiply.Last().LoadFromJsonObject(JsonObject);
	}
	else
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Invalid type for feature %s (%s)"), *FeatureName, *FeatureType)
		return bValidFeature;
	}
	return bValidFeature;
}

bool FRigMapperFeatureDefinitions::GetFeatureNames(TArray<FString>& OutFeatureNames) const
{
	bool bFoundDuplicate = false;

	// todo: from interface
	for (const FRigMapperMultiplyFeature& Feature : Multiply)
	{
		bFoundDuplicate |= OutFeatureNames.Contains(Feature.Name);
		OutFeatureNames.Add(Feature.Name);
	}
	for (const FRigMapperSdkFeature& Feature : SDKs)
	{
		bFoundDuplicate |= OutFeatureNames.Contains(Feature.Name);
		OutFeatureNames.Add(Feature.Name);
	}
	for (const FRigMapperWsFeature& Feature : WeightedSums)
	{
		bFoundDuplicate |= OutFeatureNames.Contains(Feature.Name);
		OutFeatureNames.Add(Feature.Name);
	}
	return !bFoundDuplicate;
}

bool FRigMapperFeatureDefinitions::IsValid(const TArray<FString>& InputNames, bool bWarn) const
{
	TArray<FString> FeatureNames;
	bool bValid = true;

	const bool bDuplicateFeatureName = GetFeatureNames(FeatureNames);
	TArray<FString> CheckFeatureNames;
	for (const FString& FeatureName : FeatureNames)
	{
		if (InputNames.Contains(FeatureName))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Conflicting input and feature name: %s"), *FeatureName)
			bValid = false;
		}
		if (bDuplicateFeatureName)
		{
			if (CheckFeatureNames.Contains(FeatureName))
			{
				UE_LOG(LogRigMapper, Warning, TEXT("Duplicate feature name: %s"), *FeatureName)
				bValid = false;
			}
			else
			{
				CheckFeatureNames.Add(FeatureName);
			}
		}
	}

	TArray<FString> FeatureAndInputNames = MoveTemp(FeatureNames);
	FeatureAndInputNames.Append(InputNames);

	for (const FRigMapperMultiplyFeature& Feature : Multiply)
	{
		bValid &= Feature.IsValid(FeatureAndInputNames, bWarn);
	}
	for (const FRigMapperSdkFeature& Feature : SDKs)
	{
		bValid &= Feature.IsValid(FeatureAndInputNames, bWarn);
	}
	for (const FRigMapperWsFeature& Feature : WeightedSums)
	{
		bValid &= Feature.IsValid(FeatureAndInputNames, bWarn);
	}
	
	return bValid;
}

void FRigMapperFeatureDefinitions::Empty()
{
	Multiply.Empty();
	WeightedSums.Empty();
	SDKs.Empty();
}

FRigMapperFeature* FRigMapperFeatureDefinitions::Find(const FString& FeatureName, ERigMapperFeatureType& OutFeatureType)
{
	if (FRigMapperMultiplyFeature* Feature = Multiply.FindByPredicate([FeatureName](const FRigMapperMultiplyFeature& Other) { return Other.Name == FeatureName; }))
	{
		OutFeatureType = ERigMapperFeatureType::Multiply;
		return Feature;
	}

	if (FRigMapperWsFeature* Feature = WeightedSums.FindByPredicate([FeatureName](const FRigMapperWsFeature& Other) { return Other.Name == FeatureName; }))
	{
		OutFeatureType = ERigMapperFeatureType::WeightedSum;
		return Feature;
	}

	if (FRigMapperSdkFeature* Feature = SDKs.FindByPredicate([FeatureName](const FRigMapperSdkFeature& Other) { return Other.Name == FeatureName; }))
	{
		OutFeatureType = ERigMapperFeatureType::SDK;
		return Feature;
	}

	return nullptr;
}

bool URigMapperDefinition::LoadFromJsonFile(const FFilePath& JsonFilePath)
{
	FString JsonAsString;
		
	if (!FFileHelper::LoadFileToString(JsonAsString, *JsonFilePath.FilePath))
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Could not open json file"))
		return false;
	}

	return LoadFromJsonString(JsonAsString);
}

void URigMapperDefinition::Empty()
{
	SetDefinitionValid(false);
	Inputs.Empty();
	Outputs.Empty();
	Features.Empty();
	NullOutputs.Empty();
}

bool URigMapperDefinition::LoadFromJsonString(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Could not deserialize json data to load definition"))
		return false;
	}

	Empty();
	SetDefinitionValid(true);
	
	TArray<FString> FeatureNames;

	const bool bValidInputs = LoadInputsFromJsonObject(JsonObject);
	if (!bValidInputs)
	{
		SetDefinitionValid(false);
	}
	const bool bValidFeatures = LoadFeaturesFromJsonObject(JsonObject, FeatureNames);
	if (!bValidFeatures)
	{
		SetDefinitionValid(false);
	}
	const bool bValidOutputs = LoadOutputsFromJsonObject(JsonObject, FeatureNames);
	if (!bValidOutputs)
	{
		SetDefinitionValid(false);
	}
	const bool bValidNullOutputs = LoadNullOutputsFromJsonObject(JsonObject);
	if (!bValidNullOutputs)
	{
		SetDefinitionValid(false);
	}

	OnRigMapperDefinitionUpdated.Broadcast();

	return bValidated;
}

bool URigMapperDefinition::LoadInputsFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject->HasTypedField(TEXT("inputs"), EJson::Array))
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Missing inputs field"))
		SetDefinitionValid(false);
		return false;
	}
	for (const auto& InputAttr : JsonObject->GetArrayField(TEXT("inputs")))
	{
		const FString InputString = InputAttr->AsString();
		if (Inputs.Contains(InputString))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Duplicate input was found: %s"), *InputString)
		}
		else
		{
			Inputs.Add(InputString);	
		}
	}
	if (Inputs.IsEmpty())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Not enough inputs"))
		SetDefinitionValid(false);
	}
	return bValidated;
}

bool URigMapperDefinition::LoadNullOutputsFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (JsonObject->HasTypedField(TEXT("null_outputs"), EJson::Array))
	{
		for (const auto& NullOutputAttr : JsonObject->GetArrayField(TEXT("null_outputs")))
		{
			const FString NullOutputString = NullOutputAttr->AsString();
			if (NullOutputs.Contains(NullOutputString))
			{
				UE_LOG(LogRigMapper, Warning, TEXT("Duplicate null output was found: %s"), *NullOutputString)
			}
			else
			{
				if (Outputs.Contains(NullOutputString))
				{
					UE_LOG(LogRigMapper, Warning, TEXT("Null output conflicts with existing output: %s"), *NullOutputString)
					SetDefinitionValid(false);
				}
				else
				{
					NullOutputs.Add(NullOutputString);	
				}
			}
		}
	}
	return WasDefinitionValidated();
}

void URigMapperDefinition::SetDefinitionValid(bool bValid)
{
	if (bValidated != bValid)
	{
		Modify();
		bValidated = bValid;
		UE_LOG(LogRigMapper, Log, TEXT("Definition %s is now %s"), *GetName(), WasDefinitionValidated() ? TEXT("valid") : TEXT("invalid"))
	}
}

bool URigMapperDefinition::LoadFeaturesFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, TArray<FString>& FeatureNames)
{
	if (!JsonObject->HasTypedField(TEXT("features"), EJson::Object))
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Missing features field"))
		SetDefinitionValid(false);
		return false;
	}
	for (const auto& FeatureInfo : JsonObject->GetObjectField(TEXT("features"))->Values)
	{
		if (Inputs.Contains(FeatureInfo.Key))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Feature conflicting with input of similar name: %s"), *FeatureInfo.Key)
			SetDefinitionValid(false);
			continue;
		}

		if (!Features.AddFromJsonObject(FeatureInfo.Key, FeatureInfo.Value->AsObject()))
		{
			SetDefinitionValid(false);
			continue;
		}

		FeatureNames.Add(FeatureInfo.Key);
	}
	return WasDefinitionValidated();
}

bool URigMapperDefinition::LoadOutputsFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, const TArray<FString>& FeatureNames)
{
	if (!JsonObject->HasTypedField(TEXT("outputs"), EJson::Object))
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Missing outputs field"))
		SetDefinitionValid(false);
		return false;
	}
	for (const auto& OutputAttr : JsonObject->GetObjectField(TEXT("outputs"))->Values)
	{
		if (OutputAttr.Key.IsEmpty())
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Invalid output with empty name"))
			continue;
		}
		FString OutputValue;
		if (!OutputAttr.Value.IsValid() || OutputAttr.Value->IsNull() || OutputAttr.Value->AsString().IsEmpty())
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Invalid value for output %s"), *OutputAttr.Key)
			SetDefinitionValid(false);
		}
		else
		{
			OutputValue = OutputAttr.Value->AsString();
			if (!Inputs.Contains(OutputValue) && !FeatureNames.Contains(OutputValue))
			{
				UE_LOG(LogRigMapper, Warning, TEXT("Could not find corresponding input/feature for output %s (%s)"), *OutputAttr.Key, *OutputValue)
				SetDefinitionValid(false);
			}
		}
		Outputs.Add(OutputAttr.Key, OutputValue);
	}
	if (Outputs.IsEmpty())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Not enough outputs"))
		SetDefinitionValid(false);
	}
	return WasDefinitionValidated();
}

#if WITH_EDITOR
EDataValidationResult URigMapperDefinition::IsDataValid(FDataValidationContext& Context) const
{
	return IsDefinitionValid() ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

bool URigMapperDefinition::ExportAsJsonString(FString& OutJsonString) const
{
	if (!IsDefinitionValid())
	{
		return false;
	}
	
	const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJsonString);

	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("inputs"), Inputs);
	JsonWriter->WriteObjectStart(TEXT("features"));
	for (const FRigMapperMultiplyFeature& Feature: Features.Multiply)
	{
		JsonWriter->WriteObjectStart(Feature.Name);
		JsonWriter->WriteValue(TEXT("type"), TEXT("multiply"));
		JsonWriter->WriteValue(TEXT("input_features"), Feature.Inputs);
		JsonWriter->WriteArrayStart(TEXT("input_params"));
		JsonWriter->WriteArrayEnd();
		JsonWriter->WriteObjectStart(TEXT("params"));
		JsonWriter->WriteObjectEnd();
		JsonWriter->WriteObjectEnd();
	}
	for (const FRigMapperWsFeature& Feature: Features.WeightedSums)
	{
		JsonWriter->WriteObjectStart(Feature.Name);
		JsonWriter->WriteValue(TEXT("type"), TEXT("weighted_sum"));
		TArray<FString> InputNames;
		TArray<double> InputWeights;
		Feature.Inputs.GetKeys(InputNames);
		InputWeights.Reserve(InputNames.Num());
		for (const FString& Input : InputNames)
		{
			InputWeights.Add(Feature.Inputs[Input]);
		}
		JsonWriter->WriteValue(TEXT("input_features"), InputNames);
		JsonWriter->WriteArrayStart(TEXT("input_params"));
		JsonWriter->WriteArrayEnd();
		JsonWriter->WriteObjectStart(TEXT("params"));
		JsonWriter->WriteValue(TEXT("weights"), InputWeights);
		if (Feature.Range.bHasLowerBound)
		{
			JsonWriter->WriteValue(TEXT("min"), Feature.Range.LowerBound);
		}
		if (Feature.Range.bHasUpperBound)
		{
			JsonWriter->WriteValue(TEXT("max"), Feature.Range.UpperBound);
		}
		JsonWriter->WriteObjectEnd();
		JsonWriter->WriteObjectEnd();
	}
	for (const FRigMapperSdkFeature& Feature: Features.SDKs)
	{
		JsonWriter->WriteObjectStart(Feature.Name);
		JsonWriter->WriteValue(TEXT("type"), TEXT("sdk"));
		JsonWriter->WriteValue(TEXT("input_features"), TArray<FString>({Feature.Input}));
		JsonWriter->WriteArrayStart(TEXT("input_params"));
		JsonWriter->WriteArrayEnd();
		JsonWriter->WriteObjectStart(TEXT("params"));
		TArray<double> Keys;
		TArray<double> Values;
		Keys.Reserve(Feature.Keys.Num());
		Values.Reserve(Feature.Keys.Num());
		for (const FRigMapperSdkKey& Key : Feature.Keys)
		{
			Keys.Add(Key.In);
			Values.Add(Key.Out);
		}
		JsonWriter->WriteValue(TEXT("in_val"), Keys);
		JsonWriter->WriteValue(TEXT("out_val"), Values);	
		JsonWriter->WriteObjectEnd();
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteObjectStart(TEXT("parameters"));
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteObjectStart(TEXT("outputs"));
	for (const TPair<FString, FString>& Output : Outputs)
	{
		JsonWriter->WriteValue(Output.Key, Output.Value);	
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteValue(TEXT("null_outputs"), NullOutputs);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	return true;
}

bool URigMapperDefinition::ExportAsJsonFile(const FFilePath& JsonFilePath) const
{
	FString JsonString;
	
	if (ExportAsJsonString(JsonString))
	{
		return FFileHelper::SaveStringToFile(JsonString, *JsonFilePath.FilePath);
	}
	return false;
}

bool URigMapperDefinition::WasDefinitionValidated() const
{
	return bValidated;
}

bool URigMapperDefinition::IsDefinitionValid(bool bWarn, bool bForce) const
{
	if (!bForce && WasDefinitionValidated())
	{
		return true;
	}

	bool bValid = true;

	if (Inputs.IsEmpty())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Not enough inputs"))
		bValid = false;
	}
	if (Outputs.IsEmpty())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Not enough outputs"))
		bValid = false;
	}

	TArray<FString> CheckInputs;
	CheckInputs.Reserve(Inputs.Num());
	for (const FString& Input : Inputs)
	{
		// todo: check with skeleton and or control rig ?
		if (CheckInputs.Contains(Input))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Duplicate input %s"), *Input)
			bValid = false;
		}
		else
		{
			CheckInputs.Add(Input);
		}
	}
	TArray<FString> FeatureNames;
	Features.GetFeatureNames(FeatureNames);
	for (const TPair<FString, FString>& Output : Outputs)
	{
		// todo: check with skeleton and or control rig ?
		if (!Inputs.Contains(Output.Value) && !FeatureNames.Contains(Output.Value))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Output %s does not link to any existing input or feature"), *Output.Key)
			bValid = false;
		}
	}
	
	bValid &= Features.IsValid(Inputs, bWarn);	

	// check that NullOutputs does not contain any keys from Outputs, or any duplicated keys
	TArray<FString> CheckNullOutputs;
	CheckNullOutputs.Reserve(NullOutputs.Num());

	for (const FString& NullOutput : NullOutputs)
	{
		if (Outputs.Contains(NullOutput))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Output is also defined as a NullOutput %s"), *NullOutput)
			bValid = false;
		}
		if (CheckNullOutputs.Contains(NullOutput))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Duplicate NullOutput %s"), *NullOutput)
			bValid = false;
		}
		else
		{
			CheckNullOutputs.Add(NullOutput);
		}
	}

	return bValid;
}

bool URigMapperDefinition::Validate()
{
	UE_LOG(LogRigMapper, Log, TEXT("Validating definition %s"), *GetName())

	const bool bPrev = WasDefinitionValidated();
	
	SetDefinitionValid(IsDefinitionValid(true, true));

	if (WasDefinitionValidated() == bPrev)
	{
		UE_LOG(LogRigMapper, Log, TEXT("Definition %s is still %s"), *GetName(), WasDefinitionValidated() ? TEXT("valid") : TEXT("invalid"))
	}
	
	return WasDefinitionValidated();
}

#if WITH_EDITOR
void URigMapperDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// if the definition exists in the global cache, delete it so that the editor doesn't get out of sync with the cache
	FRigMapperDefinitionsSingleton::Get().ClearFromCache(this);

	OnRigMapperDefinitionUpdated.Broadcast();

	SetDefinitionValid(false);
}
#endif

bool URigMapperLinkedDefinitions::GetBakedInputRec(const FString& InputName, const int32 DefinitionIndex, FRigMapperFeature::FBakedInput& OutBakedInput, bool& bOutMissingIsNotNullOutput)
{
	check(SourceDefinitions[DefinitionIndex])
	
	if (SourceDefinitions[DefinitionIndex]->Inputs.Contains(InputName))
	{
		if (DefinitionIndex > 0)
		{
			if (!SourceDefinitions[DefinitionIndex - 1]->Outputs.Contains(InputName))
			{
				if (!SourceDefinitions[DefinitionIndex - 1]->NullOutputs.Contains(InputName))
				{
					UE_LOG(LogRigMapper, Warning, TEXT("Input %s on definition %s (layer %d) does not match any output from definition %s (layer %d)"), *InputName, *SourceDefinitions[DefinitionIndex]->GetName(), DefinitionIndex, *SourceDefinitions[DefinitionIndex - 1]->GetName(), DefinitionIndex - 1)
					bOutMissingIsNotNullOutput = true;
				}
				return false;
			}
			return GetBakedInputRec(SourceDefinitions[DefinitionIndex - 1]->Outputs[InputName], DefinitionIndex - 1, OutBakedInput, bOutMissingIsNotNullOutput);
		}
		
		OutBakedInput.Key = MakeShared<FRigMapperFeature>(InputName);
		OutBakedInput.Value.Empty();
		return OutBakedInput.Key.IsValid();
	}

	ERigMapperFeatureType FeatureType;
	if (FRigMapperFeature* Feature = SourceDefinitions[DefinitionIndex]->Features.Find(InputName, FeatureType))
	{
		return Feature->BakeInput(this, DefinitionIndex, OutBakedInput);
	}
	UE_LOG(LogRigMapper, Warning, TEXT("Could not bake input %s"), *InputName)
	return false;
}

bool FRigMapperMultiplyFeature::BakeInput(URigMapperLinkedDefinitions* LinkedDefinitions, int32 DefinitionIndex, FBakedInput& OutBakedInput) const
{
	// todo: refactor all bakeinput using baseclass
	const FString BakedInputName = FString::Printf(TEXT("%s:%d"), *Name, DefinitionIndex);
	
	const TSharedPtr<FRigMapperMultiplyFeature> BakedMultFeature = MakeShared<FRigMapperMultiplyFeature>(BakedInputName);
	OutBakedInput.Key = BakedMultFeature;
	OutBakedInput.Value.Empty();
	bool bMissingIsNotNullOutput = false;
	
	for (const FString& FeatureInput : Inputs)
	{
		FBakedInput SubFeatureBakedInput;
		if (!LinkedDefinitions->GetBakedInputRec(FeatureInput, DefinitionIndex, SubFeatureBakedInput, bMissingIsNotNullOutput))
		{
			continue;
		}

		if (SubFeatureBakedInput.Key->GetFeatureType() == ERigMapperFeatureType::Multiply)
		{
			const FRigMapperMultiplyFeature* SubFeatureMultBakedInput = static_cast<FRigMapperMultiplyFeature*>(SubFeatureBakedInput.Key.Get());
			BakedMultFeature->Inputs.Append(SubFeatureMultBakedInput->Inputs);
		}
		else
		{
			BakedMultFeature->Inputs.Add(SubFeatureBakedInput.Key->Name);
			OutBakedInput.Value.Add(SubFeatureBakedInput.Key);
		}
		OutBakedInput.Value.Append(SubFeatureBakedInput.Value);
	}

	return OutBakedInput.Key.IsValid() && !bMissingIsNotNullOutput; // if we have a missing definition which is not defined as a NullOutput, flag a failure
}

bool FRigMapperWsFeature::BakeInput(URigMapperLinkedDefinitions* LinkedDefinitions, int32 DefinitionIndex, FBakedInput& OutBakedInput) const
{
	const FString BakedInputName = FString::Printf(TEXT("%s:%d"), *Name, DefinitionIndex);
	
	const TSharedPtr<FRigMapperWsFeature> BakedWsFeature = MakeShared<FRigMapperWsFeature>(BakedInputName);
	OutBakedInput.Key = BakedWsFeature;
	bool bMissingIsNotNullOutput = false;
			
	for (const TPair<FString, double>& FeatureInput : Inputs)
	{
		FBakedInput SubFeatureBakedInput;
		if (!LinkedDefinitions->GetBakedInputRec(FeatureInput.Key, DefinitionIndex, SubFeatureBakedInput, bMissingIsNotNullOutput))
		{
			continue;
		}

		if (SubFeatureBakedInput.Key->GetFeatureType() == ERigMapperFeatureType::WeightedSum)
		{
			const FRigMapperWsFeature* SubFeatureWsBakedInput = static_cast<FRigMapperWsFeature*>(SubFeatureBakedInput.Key.Get());

			for (const TPair<FString, double>& SubFeatureSubInput : SubFeatureWsBakedInput->Inputs)
			{
				// check for the existance of the SubFeatureSubInput.Key in the map already as we can get 'diamond' structures
				if (BakedWsFeature->Inputs.Contains(SubFeatureSubInput.Key))
				{
					BakedWsFeature->Inputs[SubFeatureSubInput.Key] += SubFeatureSubInput.Value * FeatureInput.Value;
				}
				else
				{
					BakedWsFeature->Inputs.Add(SubFeatureSubInput.Key, SubFeatureSubInput.Value * FeatureInput.Value);
				}
			}
		}
		else
		{
			BakedWsFeature->Inputs.Add(SubFeatureBakedInput.Key->Name, FeatureInput.Value);
			OutBakedInput.Value.Add(SubFeatureBakedInput.Key);
		}
		OutBakedInput.Value.Append(SubFeatureBakedInput.Value);
	}

	return OutBakedInput.Key.IsValid() && !bMissingIsNotNullOutput; // if we have a missing definition which is not defined as a NullOutput, flag a failure
}

bool FRigMapperSdkFeature::BakeInput(URigMapperLinkedDefinitions* LinkedDefinitions, int32 DefinitionIndex, FBakedInput& OutBakedInput) const
{
	const FString BakedInputName = FString::Printf(TEXT("%s:%d"), *Name, DefinitionIndex);
	
	const TSharedPtr<FRigMapperSdkFeature> BakedSdkFeature = MakeShared<FRigMapperSdkFeature>(BakedInputName);
	OutBakedInput.Key = BakedSdkFeature;

	FBakedInput SubFeatureBakedInput;
	bool bMissingIsNotNullOutput = false;

	if (!LinkedDefinitions->GetBakedInputRec(Input, DefinitionIndex, SubFeatureBakedInput, bMissingIsNotNullOutput))
	{
		return false;
	}

	if (SubFeatureBakedInput.Key->GetFeatureType() == ERigMapperFeatureType::SDK)
	{
		const FRigMapperSdkFeature* SubFeatureSdkBakedInput = static_cast<FRigMapperSdkFeature*>(SubFeatureBakedInput.Key.Get());
		BakedSdkFeature->Input = SubFeatureSdkBakedInput->Input;
		BakeKeys(*SubFeatureSdkBakedInput, *this, BakedSdkFeature->Keys);
	}
	else
	{
		BakedSdkFeature->Input = SubFeatureBakedInput.Key->Name;
		BakedSdkFeature->Keys = Keys;
		OutBakedInput.Value.Add(SubFeatureBakedInput.Key);
	}
	OutBakedInput.Value.Append(SubFeatureBakedInput.Value);

	return OutBakedInput.Key.IsValid() && !bMissingIsNotNullOutput; // if we have a missing definition which is not defined as a NullOutput, flag a failure
}

bool FRigMapperSdkFeature::BakeKeys(const FRigMapperSdkFeature& InSdk, const FRigMapperSdkFeature& OutSdk, TArray<FRigMapperSdkKey>& BakedKeys)
{
	// Create an array to backward evaluate InSdk (First Layer)
	TArray<TPair<double,double>> InKeysForEval_Backward;
	InKeysForEval_Backward.Reserve(InSdk.Keys.Num());
	for (const FRigMapperSdkKey& Key : InSdk.Keys)
	{
		InKeysForEval_Backward.Add({ Key.Out, Key.In });
	}
	InKeysForEval_Backward.Sort([](const TPair<double,double>& A, const TPair<double,double>& B) { return A.Key < B.Key; } );

	// Create an array to evaluate OutSdk (Second Layer)
	TArray<TPair<double,double>> OutKeysForEval;
	OutKeysForEval.Reserve(OutSdk.Keys.Num());
	for (const FRigMapperSdkKey& Key : OutSdk.Keys)
	{
		OutKeysForEval.Add({ Key.In, Key.Out });
	}

	// Prepare to store our baked keys 
	BakedKeys.Reserve(FMath::Max(InSdk.Keys.Num(), OutSdk.Keys.Num()));

	// Bake all keys from InSdk (layer 1) using OutSdk
	for (const FRigMapperSdkKey& InKey : InSdk.Keys)
	{
		double Value = 0.f;

		if (!FacialRigMapping::FEvalNodePiecewiseLinear::Evaluate_Static(InKey.Out, OutKeysForEval, Value))
		{
			return false;
		}
		BakedKeys.Add({ InKey.In, Value });
	}

	// Because we might miss the precision from some keys of OutSdk (i.e. if we have 3 keys in OutSdk and 2 in InSdk), we now need to insert missing keys from OutSdk by reverse baking them using InSdk 
	for (const FRigMapperSdkKey& OutKey : OutSdk.Keys)
	{
		double ActualInValue = 0.f;
				
		if (!FacialRigMapping::FEvalNodePiecewiseLinear::Evaluate_Static(OutKey.In, InKeysForEval_Backward, ActualInValue))
		{
			return false;
		}
		
		for (int32 InKeyIndex = 0; InKeyIndex < InSdk.Keys.Num(); InKeyIndex++)
		{
			const bool bInsertBefore = ActualInValue < InSdk.Keys[InKeyIndex].In && (InKeyIndex == 0 || ActualInValue > InSdk.Keys[InKeyIndex - 1].In);
			const bool bInsertAfter = ActualInValue > InSdk.Keys[InKeyIndex].In && (InKeyIndex == InSdk.Keys.Num() - 1 || ActualInValue < InSdk.Keys[InKeyIndex + 1].In);
			
			if (bInsertBefore || bInsertAfter)
			{
				const int32 NewIndex = InKeyIndex + BakedKeys.Num() - InSdk.Keys.Num(); 
				BakedKeys.Insert({ ActualInValue, OutKey.Out }, bInsertBefore ? NewIndex : NewIndex + 1);
				break;
			}
		}
	}

	// Finally, strip any duplicate or incorrectly ordered In keys from the beginning and end.
	// todo: make sure the rig mapper allows duplicate Input keys in the middle (i.e -1;0;0;1 -> -100;-50;50;100 should be a valid use case)
	while (BakedKeys.Num() >= 2 && BakedKeys[0].In >= BakedKeys[1].In)
	{
		BakedKeys.RemoveAt(0);
	}
	while (BakedKeys.Num() >= 2 && BakedKeys[BakedKeys.Num() - 1].In <= BakedKeys[BakedKeys.Num() - 2].In)
	{
		BakedKeys.RemoveAt(BakedKeys.Num() - 1);
	}
	return true;
}

bool URigMapperLinkedDefinitions::AddBakedInputFeature(const TSharedPtr<FRigMapperFeature>& Feature) const
{
	if (Feature->GetFeatureType() == ERigMapperFeatureType::Multiply)
	{
		BakedDefinition->Features.Multiply.Add(*static_cast<FRigMapperMultiplyFeature*>(Feature.Get()));
	}
	else if (Feature->GetFeatureType() == ERigMapperFeatureType::WeightedSum)
	{
		BakedDefinition->Features.WeightedSums.Add(*static_cast<FRigMapperWsFeature*>(Feature.Get()));
	}
	else if (Feature->GetFeatureType() == ERigMapperFeatureType::SDK)
	{
		BakedDefinition->Features.SDKs.Add(*static_cast<FRigMapperSdkFeature*>(Feature.Get()));
	}
	else
	{
		if (!SourceDefinitions[0]->Inputs.Contains(Feature->Name))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Baked input could not be found within the lower level inputs"))
			return false;
		}
		BakedDefinition->Inputs.AddUnique(Feature->Name);
	}
	return true;
}

void URigMapperLinkedDefinitions::AddBakedInputs(const TArray<FRigMapperFeature::FBakedInput>& BakedInputs, const TArray<TPair<FString, FString>>& PairedOutputs) const
{
	TArray<FString> FeatureNames;
	
	BakedDefinition->Empty();
	
	for (int32 OutputIndex = 0; OutputIndex < PairedOutputs.Num(); OutputIndex++)
	{
		BakedDefinition->Outputs.Add(PairedOutputs[OutputIndex].Key, BakedInputs[OutputIndex].Key->Name);

		if (!FeatureNames.Contains(BakedInputs[OutputIndex].Key->Name) && AddBakedInputFeature(BakedInputs[OutputIndex].Key))
		{
			FeatureNames.Add(BakedInputs[OutputIndex].Key->Name);
			
			for (const TSharedPtr<FRigMapperFeature>& SubFeature : BakedInputs[OutputIndex].Value)
			{
				if (!FeatureNames.Contains(SubFeature->Name) && !AddBakedInputFeature(SubFeature))
				{
					break;
				}
				FeatureNames.Add(SubFeature->Name);
			}
		}
	}
}

#if WITH_EDITOR
EDataValidationResult URigMapperLinkedDefinitions::IsDataValid(FDataValidationContext& Context) const
{
	return AreLinkedDefinitionsValid() ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

TArray<FRigMapperFeature::FBakedInput> URigMapperLinkedDefinitions::GetBakedInputs(const TArray<TPair<FString, FString>>& PairedOutputs)
{
	TArray<FRigMapperFeature::FBakedInput> BakedInputs;
	BakedInputs.Reserve(PairedOutputs.Num());
	bool bMissingIsNotNullOutput = false;

	for (const TPair<FString, FString>& Out : PairedOutputs)
	{
		FRigMapperFeature::FBakedInput BakedInput;
		
		if (GetBakedInputRec(Out.Value, SourceDefinitions.Num() - 1, BakedInput, bMissingIsNotNullOutput))
		{
			BakedInputs.Add(BakedInput);
		}
		else
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Could not bake input %s associated to output %s"), *Out.Value, *Out.Key)
		}
	}

	// todo: cleanup baked unique names (prefixes)?

	return BakedInputs;
}

bool URigMapperLinkedDefinitions::BakeDefinitions()
{
	UE_LOG(LogRigMapper, Log, TEXT("Baking linked definition %s"), *GetName())
	
	if (!BakedDefinition)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Baked definition is unset"))
		return false;
	}
	if (SourceDefinitions.Num() < 2 || !SourceDefinitions[0] || !SourceDefinitions.Last())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Baking requires a minimum of 2 source definitions"))
		return false;
	}
	for (int32 DefIndex = 0; DefIndex < SourceDefinitions.Num(); DefIndex++)
	{
		if (!SourceDefinitions[DefIndex]|| !SourceDefinitions[DefIndex]->IsDefinitionValid(true))
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Invalid source definition at index %d. Make sure to revalidate the asset if necessary"), DefIndex)
			return false;
		}
	}
	
	const TArray<TPair<FString, FString>>& PairedOutputs = SourceDefinitions.Last()->Outputs.Array();
	const TArray<FRigMapperFeature::FBakedInput> BakedInputs = GetBakedInputs(PairedOutputs);
	if (BakedInputs.Num() != PairedOutputs.Num())
	{
		UE_LOG(LogRigMapper, Warning, TEXT("The number of baked inputs do not match the expected number of outputs"))
		return false;
	}
	AddBakedInputs(BakedInputs, PairedOutputs);

	UE_LOG(LogRigMapper, Log, TEXT("Finished baking linked definition %s"), *GetName())
	
	return BakedDefinition->Validate();
}

bool URigMapperLinkedDefinitions::Validate()
{
	if (BakedDefinition)
	{
		BakedDefinition->Validate();
	}

	for (URigMapperDefinition* Definition : SourceDefinitions)
	{
		if (Definition)
		{
			Definition->Validate();
		}
	}

	return AreLinkedDefinitionsValid();
}

bool URigMapperLinkedDefinitions::AreLinkedDefinitionsValid() const
{
	bool bOk = IsValid(BakedDefinition) && BakedDefinition->IsDefinitionValid();
	
	if (!bOk)
	{
		UE_LOG(LogRigMapper, Warning, TEXT("Failed to validate the baked definition"))
	}

	for (int32 DefinitionIndex = 0; DefinitionIndex < SourceDefinitions.Num(); DefinitionIndex++)
	{
		if (!SourceDefinitions[DefinitionIndex])
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Source definition %d is unset"), DefinitionIndex)
			bOk = false;
			continue;
		}
		if (!SourceDefinitions[DefinitionIndex]->IsDefinitionValid())
		{
			UE_LOG(LogRigMapper, Warning, TEXT("Source definition %d is invalid"), DefinitionIndex)
			bOk = false;
		}
		if (DefinitionIndex > 0)
		{
			for (const FString& Input : SourceDefinitions[DefinitionIndex]->Inputs)
			{
				if (!SourceDefinitions[DefinitionIndex - 1]->Outputs.Contains(Input) && !SourceDefinitions[DefinitionIndex - 1]->NullOutputs.Contains(Input))
				{
					UE_LOG(LogRigMapper, Warning, TEXT("Could not find matching output in definition %d for input %s in definition %d"), DefinitionIndex - 1, *Input, DefinitionIndex)
					bOk = false;
				}
			}
			for (const TPair<FString, FString>& Output : SourceDefinitions[DefinitionIndex - 1]->Outputs)
			{
				if (!SourceDefinitions[DefinitionIndex]->Inputs.Contains(Output.Key))
				{
					UE_LOG(LogRigMapper, Warning, TEXT("Could not find matching input in definition %d for output %s in definition %d"), DefinitionIndex, *Output.Key, DefinitionIndex - 1)
					bOk = false;
				}
			}
		}
	}
	
	return bOk;
}
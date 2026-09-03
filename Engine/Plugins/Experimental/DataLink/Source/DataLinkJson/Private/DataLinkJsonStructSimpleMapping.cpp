// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkJsonStructSimpleMapping.h"
#include "DataLinkJsonLog.h"
#include "DataLinkJsonUtils.h"
#include "JsonObjectConverter.h"
#include "StructUtils/StructView.h"
#include "UObject/PropertyAccessUtil.h"

bool UDataLinkJsonStructSimpleMapping::Apply(const TSharedRef<FJsonObject>& InSourceJson, const FStructView& InTargetStructView) const
{
	const UScriptStruct* const TargetStruct = InTargetStructView.GetScriptStruct();

	uint8* const TargetMemory = InTargetStructView.GetMemory();

	for (const TPair<FString, FName>& FieldMapping : FieldMappings)
	{
		FProperty* const TargetProperty = PropertyAccessUtil::FindPropertyByName(FieldMapping.Value, TargetStruct);
		if (!TargetProperty)
		{
			UE_LOG(LogDataLinkJson, Error, TEXT("Field '%s' not found in struct '%s'")
				, *FieldMapping.Value.ToString()
				, *TargetStruct->GetName());
			continue;
		}

		const TSharedPtr<FJsonValue> JsonValue = UE::DataLinkJson::FindJsonValue(InSourceJson, FieldMapping.Key);
		if (!JsonValue.IsValid())
		{
			UE_LOG(LogDataLinkJson, Error, TEXT("Field '%s' not found in json")
				, *FieldMapping.Key);
			continue;
		}

		uint8* const TargetPropertyValue = TargetProperty->ContainerPtrToValuePtr<uint8>(TargetMemory);
		if (!FJsonObjectConverter::JsonValueToUProperty(JsonValue, TargetProperty, TargetPropertyValue))
		{
			UE_LOG(LogDataLinkJson, Error, TEXT("Could not copy Json Value with key '%s' to property '%s' in struct '%s'")
				, *FieldMapping.Key
				, *TargetProperty->GetName()
				, *TargetStruct->GetName());
		}
	}

	return true;
}

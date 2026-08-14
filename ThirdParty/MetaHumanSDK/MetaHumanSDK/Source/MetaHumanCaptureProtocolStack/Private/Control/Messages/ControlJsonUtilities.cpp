// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlJsonUtilities.h"
#include "Serialization/MemoryReader.h"

bool FJsonUtility::CreateJsonFromUTF8Data(const TArray<uint8>& InData, TSharedPtr<FJsonObject>& OutObject)
{
	FMemoryReader Reader(InData);

	TSharedRef<TJsonReader<UTF8CHAR>> JsonReader = TJsonReaderFactory<UTF8CHAR>::Create(&Reader);
	return FJsonSerializer::Deserialize(JsonReader, OutObject);
}

bool FJsonUtility::CreateUTF8DataFromJson(TSharedPtr<FJsonObject> InObject, TArray<uint8>& OutData)
{
	FMemoryWriter Writer(OutData);

	using FJsonWriterCondensedFactory = TJsonWriterFactory<UTF8CHAR, TCondensedJsonPrintPolicy<UTF8CHAR>>;
	using FJsonWriterCondensed = TJsonWriter<UTF8CHAR, TCondensedJsonPrintPolicy<UTF8CHAR>>;

	TSharedRef<FJsonWriterCondensed> JsonWriter = FJsonWriterCondensedFactory::Create(&Writer);
	return FJsonSerializer::Serialize(InObject.ToSharedRef(), JsonWriter);
}

TProtocolResult<void> FJsonUtility::ParseString(const TSharedPtr<FJsonObject>& InBody, const FString& InFieldName, FString& OutFieldValue)
{
    if (!InBody->TryGetStringField(InFieldName, OutFieldValue))
    {
        return FCaptureProtocolError(TEXT("Failed to parse key: ") + InFieldName);
    }

    return ResultOk;
}

TProtocolResult<void> FJsonUtility::ParseBool(const TSharedPtr<FJsonObject>& InBody, const FString& InFieldName, bool& OutFieldValue)
{
    if (!InBody->TryGetBoolField(InFieldName, OutFieldValue))
    {
        return FCaptureProtocolError(TEXT("Failed to parse key: ") + InFieldName);
    }

    return ResultOk;
}

TProtocolResult<void> FJsonUtility::ParseObject(const TSharedPtr<FJsonObject>& InBody, const FString& InFieldName, const TSharedPtr<FJsonObject>*& OutFieldValue)
{
    if (!InBody->TryGetObjectField(InFieldName, OutFieldValue))
    {
        return FCaptureProtocolError(TEXT("Failed to parse key: ") + InFieldName);
    }

    return ResultOk;
}

TProtocolResult<void> FJsonUtility::ParseArray(const TSharedPtr<FJsonObject>& InBody, const FString& InFieldName, const TArray<TSharedPtr<FJsonValue>>*& OutFieldValue)
{
    if (!InBody->TryGetArrayField(InFieldName, OutFieldValue))
    {
        return FCaptureProtocolError(TEXT("Failed to parse key: ") + InFieldName);
    }

    return ResultOk;
}
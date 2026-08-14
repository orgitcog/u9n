// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkOAuthUtils.h"
#include "Containers/StringView.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TSharedPtr<FJsonObject> UE::DataLinkOAuth::ResponseStringToJsonObject(FStringView InResponseString)
{
	TSharedPtr<FJsonValue> ResponseJson;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InResponseString.GetData()), ResponseJson))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject>* JsonObject;
	if (!ResponseJson->TryGetObject(JsonObject))
	{
		return nullptr;
	}

	return *JsonObject;
}

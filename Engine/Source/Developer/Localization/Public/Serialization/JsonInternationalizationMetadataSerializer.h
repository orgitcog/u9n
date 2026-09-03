// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "Templates/SharedPointer.h"

#define UE_API LOCALIZATION_API

class FJsonObject;
class FLocMetadataObject;

class FJsonInternationalizationMetaDataSerializer
{
public:
	/**
	 * Deserializes manifest metadata from a JSON object.
	 *
	 * @param InJsonObj The JSON object to serialize from.
	 * @param OutMetadataObj The resulting metadata object or null if the serialize is not successful.
	 */
	static UE_API void DeserializeMetadata( const TSharedRef< FJsonObject > JsonObj, TSharedPtr< FLocMetadataObject >& OutMetadataObj );

	/**
	 * Serializes manifest metadata object to a JSON object.
	 *
	 * @param Manifest The Internationalization metadata to serialize.
	 * @param OutJsonObj The resulting JSON object or NULL if the serilize is not successful.
	 */
	static UE_API void SerializeMetadata(  const TSharedRef< FLocMetadataObject > Metadata, TSharedPtr< FJsonObject >& OutJsonObj );

	/**
	 * Utility function that will convert metadata to string using the JSON metadata serializers.
	 *
	 * @param Manifest The Internationalization metadata to conver to string.
	 */
	static UE_API FString MetadataToString( const TSharedPtr<FLocMetadataObject> Metadata );
};

#undef UE_API

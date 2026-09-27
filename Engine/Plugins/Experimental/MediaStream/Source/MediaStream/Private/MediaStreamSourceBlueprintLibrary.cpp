// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaStreamSourceBlueprintLibrary.h"

#include "MediaStream.h"
#include "MediaStreamModule.h"
#include "MediaStreamObjectHandlerManager.h"
#include "MediaStreamSchemeHandlerManager.h"
#include "SchemeHandlers/MediaStreamAssetSchemeHandler.h"
#include "SchemeHandlers/MediaStreamFileSchemeHandler.h"
#include "SchemeHandlers/MediaStreamManagedSchemeHandler.h"
#include "SchemeHandlers/MediaStreamSubobjectSchemeHandler.h"

bool UMediaStreamSourceBlueprintLibrary::IsValidMediaSource(const FMediaStreamSource& InSource)
{
	return !InSource.Scheme.IsNone() && !InSource.Path.IsEmpty();
}

bool UMediaStreamSourceBlueprintLibrary::IsAssetValid(const TSoftObjectPtr<UObject>& InAsset)
{
	return IsAssetSoftPathValid(InAsset.ToSoftObjectPath());
}

bool UMediaStreamSourceBlueprintLibrary::IsAssetPathValid(const FString& InPath)
{
	return IsAssetSoftPathValid(FSoftObjectPath(InPath));
}

bool UMediaStreamSourceBlueprintLibrary::IsAssetSoftPathValid(const FSoftObjectPath& InPath)
{
	return !InPath.IsNull() && InPath.IsValid() && InPath.IsAsset();
}

FMediaStreamSource UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromSchemePath(UMediaStream* InMediaStream, FName InScheme, const FString& InPath)
{
	if (!IsValid(InMediaStream))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromSchemePath"));
		return {};
	}

	if (InScheme.IsNone())
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Scheme in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromSchemePath"));
		return {};
	}

	if (InPath.IsEmpty())
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Path in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromSchemePath"));
		return {};
	}

	return FMediaStreamSchemeHandlerManager::Get().CreateSource(InMediaStream, InScheme, InPath);
}

FMediaStreamSource UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromAsset(UMediaStream* InMediaStream, const TSoftObjectPtr<UObject>& InObject)
{
	if (!IsValid(InMediaStream))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromAsset"));
		return {};
	}

	if (InObject.IsNull())
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Asset in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromAsset"));
		return {};
	}

	return MakeMediaSourceFromSchemePath(InMediaStream, FMediaStreamAssetSchemeHandler::Scheme, InObject.ToSoftObjectPath().ToString());
}

FMediaStreamSource UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromStreamName(UMediaStream* InMediaStream, FName InStreamName)
{
	if (!IsValid(InMediaStream))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromStreamName"));
		return {};
	}

	if (InStreamName.IsNone())
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Stream Name in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromStreamName"));
		return {};
	}

	return MakeMediaSourceFromSchemePath(InMediaStream, FMediaStreamManagedSchemeHandler::Scheme, InStreamName.GetPlainNameString());
}

FMediaStreamSource UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromFile(UMediaStream* InMediaStream, const FString& InFileName)
{
	if (!IsValid(InMediaStream))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromFile"));
		return {};
	}

	if (InFileName.IsEmpty())
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid File Name in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromFile"));
		return {};
	}

	return MakeMediaSourceFromSchemePath(InMediaStream, FMediaStreamFileSchemeHandler::Scheme, InFileName);
}

FMediaStreamSource UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromSubobject(UMediaStream* InMediaStream, UObject* InObject)
{
	if (!IsValid(InMediaStream))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromObject"));
		return {};
	}

	if (!IsValid(InObject))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Object in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromObject"));
		return {};
	}

	if (InObject->IsAsset())
	{
		UE_LOG(LogMediaStream, Error, TEXT("Asset given instead of Subobject in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromObject [%s]"), *InObject->GetPathName());
		return {};
	}

		const UClass* ObjectClass = InObject->GetClass();

	if (!FMediaStreamObjectHandlerManager::Get().CanHandleObject(ObjectClass))
	{
		UE_LOG(LogMediaStream, Error, TEXT("No registered handler for class in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromObject [%s]"), *ObjectClass->GetName());
		return {};
	}

	const FString FullPath = InObject->GetPathName();
	const FString RelativePath = InObject->GetPathName(InMediaStream);

	if (FullPath == RelativePath)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Subobject is not a descendant of the Root Object in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromObject"));
		return {};
	}

	return MakeMediaSourceFromSchemePath(InMediaStream, FMediaStreamSubobjectSchemeHandler::Scheme, InObject->GetName());
}

FMediaStreamSource UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromSubobjectClass(UMediaStream* InMediaStream, const UClass* InClass)
{
	if (!IsValid(InMediaStream))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromClass"));
		return {};
	}

	if (!IsValid(InClass))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Class in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromClass"));
		return {};
	}

	if (!FMediaStreamObjectHandlerManager::Get().CanHandleObject(InClass))
	{
		UE_LOG(LogMediaStream, Error, TEXT("No registered handler for class in UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromClass [%s]"), *InClass->GetName());
		return {};
	}

	UObject* Object = NewObject<UObject>(InMediaStream, InClass, NAME_None, RF_Transactional);

	return MakeMediaSourceFromSubobject(InMediaStream, Object);
}

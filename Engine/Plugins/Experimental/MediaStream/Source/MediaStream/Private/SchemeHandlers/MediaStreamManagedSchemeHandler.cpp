// Copyright Epic Games, Inc. All Rights Reserved.

#include "SchemeHandlers/MediaStreamManagedSchemeHandler.h"

#include "MediaStream.h"
#include "MediaStreamModule.h"
#include "MediaStreamSource.h"

const FLazyName FMediaStreamManagedSchemeHandler::Scheme = TEXT("Managed");

FMediaStreamSource FMediaStreamManagedSchemeHandler::CreateSource(UObject* InOuter, const FString& InPath)
{
	FMediaStreamSource Source;

	if (!IsValid(InOuter))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in FMediaStreamManagedSchemeHandler::CreateSource"));
		return Source;
	}

	Source.Scheme = Scheme;

	// TODO: Check valid media source manager stream
	if (false)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Managed Stream in FMediaStreamManagedSchemeHandler::CreateSource [%s]"), *InPath);
		return Source;
	}

	Source.Path = InPath;

	return Source;
}

UMediaPlayer* FMediaStreamManagedSchemeHandler::CreateOrUpdatePlayer(const FMediaStreamSchemeHandlerCreatePlayerParams& InParams)
{
	// TODO: Get player from media source manager
	return nullptr;
}

#if WITH_EDITOR
void FMediaStreamManagedSchemeHandler::CreatePropertyCustomization(UMediaStream* InMediaStream, IMediaStreamSchemeHandler::FCustomWidgets& InOutCustomWidgets)
{
	// TODO Implement proper customisation
}
#endif

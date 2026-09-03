// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectHandlers/MediaStreamMediaStreamHandler.h"

#include "IMediaStreamPlayer.h"
#include "MediaPlayer.h"
#include "MediaStream.h"
#include "MediaStreamModule.h"

UClass* FMediaStreamMediaStreamHandler::GetClass()
{
	return UMediaStream::StaticClass();
}

UMediaPlayer* FMediaStreamMediaStreamHandler::CreateOrUpdatePlayer(const FMediaStreamObjectHandlerCreatePlayerParams& InParams)
{
	if (!InParams.MediaStream)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in FMediaStreamMediaStreamHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	if (!InParams.Source)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Source Object in FMediaStreamMediaStreamHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	UMediaStream* MediaStream = Cast<UMediaStream>(InParams.Source);

	if (!MediaStream)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in FMediaStreamMediaStreamHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	IMediaStreamPlayer* MediaStreamPlayer = MediaStream->GetPlayer().GetInterface();

	if (!MediaStreamPlayer)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream Player in FMediaStreamMediaStreamHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	UMediaPlayer* MediaPlayer = MediaStreamPlayer->GetPlayer();

	if (!MediaPlayer)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Player FMediaStreamMediaStreamHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	return MediaPlayer;
}

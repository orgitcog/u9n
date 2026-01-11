// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectHandlers/MediaStreamMediaSourceHandler.h"

#include "IMediaStreamPlayer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaStream.h"
#include "MediaStreamModule.h"

UClass* FMediaStreamMediaSourceHandler::GetClass()
{
	return UMediaSource::StaticClass();
}

UMediaPlayer* FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer(const FMediaStreamObjectHandlerCreatePlayerParams& InParams)
{
	if (!InParams.MediaStream)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	if (!InParams.Source)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Source Object in FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	IMediaStreamPlayer* MediaStreamPlayer = InParams.MediaStream->GetPlayer().GetInterface();

	if (!MediaStreamPlayer)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream Player in FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	UMediaSource* MediaSource = Cast<UMediaSource>(InParams.Source);

	if (!MediaSource)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Source in FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	if (InParams.CurrentPlayer)
	{
		const bool bIsValidPlayer = InParams.bCanOpenSource
			? InParams.CurrentPlayer->OpenSource(MediaSource)
			: InParams.CurrentPlayer->CanPlaySource(MediaSource);

		if (bIsValidPlayer)
		{
			return InParams.CurrentPlayer;
		}
	}

	if (!InParams.bCanOpenSource)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Cannot create new player at the moment in FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	UMediaPlayer* MediaPlayer = NewObject<UMediaPlayer>(InParams.MediaStream, NAME_None, RF_Transactional);
	const FMediaPlayerOptions Options = MediaStreamPlayer->GetPlayerConfig().CreateOptions(MediaStreamPlayer->GetRequestedSeekTime());

	if (!MediaPlayer->OpenSourceWithOptions(MediaSource, Options))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Unable to create player for Media Source in FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	return MediaPlayer;
}

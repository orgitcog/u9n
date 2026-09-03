// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectHandlers/MediaStreamMediaPlaylistHandler.h"

#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "MediaStream.h"
#include "MediaStreamModule.h"

UClass* FMediaStreamMediaPlaylistHandler::GetClass()
{
	return UMediaPlaylist::StaticClass();
}

UMediaPlayer* FMediaStreamMediaPlaylistHandler::CreateOrUpdatePlayer(const FMediaStreamObjectHandlerCreatePlayerParams& InParams)
{
	if (!InParams.MediaStream)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Stream in FMediaStreamMediaPlaylistHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	if (!InParams.Source)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Source Object in FMediaStreamMediaPlaylistHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	UMediaPlaylist* MediaPlaylist = Cast<UMediaPlaylist>(InParams.Source);

	if (!MediaPlaylist)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid Media Playlist in FMediaStreamMediaPlaylistHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	if (InParams.CurrentPlayer)
	{
		UMediaSource* FirstPlaylistEntry = MediaPlaylist->Get(0);

		const bool bIsValidPlayer = InParams.bCanOpenSource
			? InParams.CurrentPlayer->OpenPlaylist(MediaPlaylist)
			: (!FirstPlaylistEntry || InParams.CurrentPlayer->CanPlaySource(FirstPlaylistEntry));

		if (bIsValidPlayer)
		{
			return InParams.CurrentPlayer;
		}
	}

	if (!InParams.bCanOpenSource)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Cannot create new player at the moment in FMediaStreamMediaPlaylistHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	UMediaPlayer* MediaPlayer = NewObject<UMediaPlayer>(InParams.MediaStream);

	if (!MediaPlayer->OpenPlaylist(MediaPlaylist))
	{
		UE_LOG(LogMediaStream, Error, TEXT("Unable to create player for Media Playlist in FMediaStreamMediaSourceHandler::CreateOrUpdatePlayer"));
		return nullptr;
	}

	return MediaPlayer;
}

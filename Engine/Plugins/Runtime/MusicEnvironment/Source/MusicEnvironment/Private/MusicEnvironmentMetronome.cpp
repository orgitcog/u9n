// Copyright Epic Games, Inc. All Rights Reserved.

#include "MusicEnvironmentMetronome.h"

// Add default functionality here for any IMovieSceneMetronome functions that are not pure virtual.
void IMusicEnvironmentMetronome::SetMusicMap(UFrameBasedMusicMap* InMusicMap)
{
	MusicMap = TStrongObjectPtr<UFrameBasedMusicMap>(InMusicMap);
	OnMusicMapSet();
}

bool IMusicEnvironmentMetronome::SetTempo(const float Bpm)
{
	if (MusicMap.IsValid() && !MusicMap->IsSimple())
	{
		return false;
	}
	return OnSetTempo(Bpm);
}

void IMusicEnvironmentMetronome::SetSpeed(float Speed)
{
	OnSetSpeed(Speed);
}

void IMusicEnvironmentMetronome::SetVolume(float InVolume)
{
	OnSetVolume(InVolume);
}

void IMusicEnvironmentMetronome::SetMuted(bool bInMuted)
{
	OnSetMuted(bInMuted);
}




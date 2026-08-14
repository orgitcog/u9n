// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UObject;
class ULevelSequence;
class UMediaStream;

class FMediaStreamEditorSequencerLibrary
{
public:
	static ULevelSequence* GetLevelSequence();
	
	static bool HasTrack(UMediaStream* InMediaStream);

	static bool CanAddTrack(UMediaStream* InMediaStream);

	static bool AddTrack(UMediaStream* InMediaStream);
};

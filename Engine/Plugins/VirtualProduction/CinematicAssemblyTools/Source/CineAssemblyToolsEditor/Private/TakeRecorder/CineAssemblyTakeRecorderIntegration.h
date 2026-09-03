// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UnrealTemplate.h"

class UTakeRecorder;
struct FQualifiedFrameTime;

/** Manages Cine Assembly Tools interactions with Take Recorder */
class FCineAssemblyTakeRecorderIntegration : public FNoncopyable
{
	public:
		FCineAssemblyTakeRecorderIntegration();
		~FCineAssemblyTakeRecorderIntegration();

	private:
		void OnRecordingInitialized(UTakeRecorder* TakeRecorder);

		void OnRecordingStarted(UTakeRecorder* TakeRecorder);
		void OnTickRecording(UTakeRecorder* TakeRecorder, const FQualifiedFrameTime& CurrentFrameTime);
		void OnRecordingStopped(UTakeRecorder* TakeRecorder);
};

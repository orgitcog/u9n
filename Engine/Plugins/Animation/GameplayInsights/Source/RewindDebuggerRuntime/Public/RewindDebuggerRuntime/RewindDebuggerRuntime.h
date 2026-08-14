// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/TraceAuxiliary.h" // for FTraceAuxiliary::EConnectionType

#define UE_API REWINDDEBUGGERRUNTIME_API

REWINDDEBUGGERRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogRewindDebuggerRuntime, Log, All);


namespace RewindDebugger
{

	class FRewindDebuggerRuntime
	{
	public:
		static UE_API void Initialize();
		static UE_API void Shutdown();
		static FRewindDebuggerRuntime* Instance() { return InternalInstance; }
			
		UE_API void StartRecording();
		UE_API void StopRecording();

		UE_API void StartRecordingWithArgs(const TArray<FString>& Args);

		bool IsRecording() const { return bIsRecording; }

		FSimpleMulticastDelegate RecordingStarted;
		FSimpleMulticastDelegate ClearRecording;
		FSimpleMulticastDelegate RecordingStopped;
	private:
		UE_API void StartRecording(FTraceAuxiliary::EConnectionType TraceType, const TCHAR* TraceDestination);

		bool bIsRecording = false;
		static UE_API FRewindDebuggerRuntime* InternalInstance;
	};
}

#undef UE_API

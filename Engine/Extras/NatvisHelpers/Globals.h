// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

typedef unsigned char uint8;
class FChunkedFixedUObjectArray;

extern uint8** GNameBlocksDebug;
extern FChunkedFixedUObjectArray* GObjectArrayForDebugVisualizers;

namespace UE
{
	namespace Core {
		struct FVisualizerDebuggingStateImpl;
		struct FVisualizerDebuggingState
		{
			const char* GuidString;
			void** Ptrs;
			FVisualizerDebuggingStateImpl* PimplData;
		};
	}
}
extern UE::Core::FVisualizerDebuggingState* GDebuggingState;

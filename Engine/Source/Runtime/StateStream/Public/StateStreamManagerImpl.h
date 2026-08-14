// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateStream.h"
#include "StateStreamManager.h"
#include "Tasks/Task.h"

#define UE_API STATESTREAM_API

////////////////////////////////////////////////////////////////////////////////////////////////////
// StateStreamManager implementation. 
// This type should only be known render side.

class FStateStreamManagerImpl : public IStateStreamManager
{
public:
	// IStateStreamManager interface
	UE_API virtual void Game_BeginTick() override;
	UE_API virtual void Game_EndTick(double AbsoluteTime) override;
	UE_API virtual void Game_Exit() override;
	UE_API virtual bool Game_IsInTick() override;
	UE_API virtual void* Game_GetStreamPointer(uint32 Id) override;

	// Register new state streams into manager.
	// TakeOwnership true means that manager will delete stream when shutting down
	UE_API void Render_Register(IStateStream& Stream, bool TakeOwnership);

	// Register dependency between statestreams. FromId will depend on ToId
	UE_API void Render_RegisterDependency(uint32 FromId, uint32 ToId);
	UE_API void Render_RegisterDependency(IStateStream& From, IStateStream& To);

	// Called at the beginning of a render frame. AbsolutTime is the amount of time the render frame consumes
	UE_API void Render_Update(double AbsoluteTime);

	// Called before Render thread exits
	UE_API void Render_Exit();

	// Garbage collect
	UE_API void Render_GarbageCollect(bool AsTask = false);

	// Get state stream from id
	UE_API IStateStream* Render_GetStream(uint32 Id);

	// Debug
	UE_API virtual void Game_DebugRender(IStateStreamDebugRenderer& Renderer);

	UE_API ~FStateStreamManagerImpl();

private:
	struct StateStreamRec
	{
		IStateStream* Stream;
		bool Owned;
	};
	TArray<StateStreamRec> StateStreams;
	TArray<IStateStream*> StateStreamsLookup;
	bool bIsInTick = false;
	bool bGameExited = false;
	bool bRenderExited = false;

	UE::Tasks::FTask GarbageCollectTask;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef UE_API

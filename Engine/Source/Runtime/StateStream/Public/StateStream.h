// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateStreamDefinitions.h"

class IStateStreamDebugRenderer;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface used by StateStreamManagerImpl. This is RT only. Should not be visible/used by GT
// Documentation in StateStreamManagerImpl

class IStateStream
{
public:
	virtual void Game_BeginTick() = 0;
	virtual void Game_EndTick(StateStreamTime AbsoluteTime) = 0;
	virtual void Game_Exit() = 0;
	virtual void* Game_GetVoidPointer() = 0;

	virtual void Render_Update(StateStreamTime AbsoluteTime) = 0;
	virtual void Render_PostUpdate() = 0;
	virtual void Render_Exit() = 0;
	virtual void Render_GarbageCollect() = 0;

	virtual uint32 GetId() = 0;

	virtual const TCHAR* GetDebugName() { return TEXT("Unknown"); }
	virtual void DebugRender(IStateStreamDebugRenderer& Renderer) {}

	virtual ~IStateStream() {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

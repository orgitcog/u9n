// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class IStateStreamDebugRenderer;

////////////////////////////////////////////////////////////////////////////////////////////////////
// StateStreamManager interface. This should be used from Game side

class IStateStreamManager
{
public:
	// Call from Game when a new tick is opened
	// Note, no state stream handles can be created,updated,destroyed outside a Begin/End tick
	virtual void Game_BeginTick() = 0;

	// Close tick and make it available to render side.
	// AbsoluteTime is the amount of time that Game consumed
	virtual void Game_EndTick(double AbsoluteTime) = 0;

	// Should be called when game is exiting.
	virtual void Game_Exit() = 0;

	// Returns true if game is inside an open tick
	virtual bool Game_IsInTick() = 0;

	// Functions to fetch StateStream interface (not IStateStream) game side.
	template<typename T>
	T& Game_Get() { return *static_cast<T*>(Game_GetStreamPointer(T::Id)); }
	virtual void* Game_GetStreamPointer(uint32 Id) = 0;

	// StateStream debug rendering
	virtual void Game_DebugRender(IStateStreamDebugRenderer& Renderer) = 0;

protected:
	virtual ~IStateStreamManager() {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

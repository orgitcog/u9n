// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class ISteamSharedModule : public IModuleInterface
{
public:
	/** Retrieve steam client handle if steam is enabled, otherwise return a nullptr */
	virtual TSharedPtr<class FSteamClientInstanceHandler> ObtainSteamClientInstanceHandle() = 0;
};

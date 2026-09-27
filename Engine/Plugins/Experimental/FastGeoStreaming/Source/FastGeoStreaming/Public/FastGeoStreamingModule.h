// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Modules/ModuleInterface.h"

class FFastGeoPrimitiveComponent;
class UWorld;

class FFastGeoStreamingModule : public IModuleInterface
{
public:
	static FFastGeoStreamingModule& Get();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void AddToComponentsPendingRecreate(FFastGeoPrimitiveComponent* InComponentPendingRecreate);
	void RemoveFromComponentsPendingRecreate(FFastGeoPrimitiveComponent* InComponentPendingRecreate);

private:
	void OnWorldPreSendAllEndOfFrameUpdates(UWorld* InWorld);

private:
	FDelegateHandle Handle_OnWorldPreSendAllEndOfFrameUpdates;
};

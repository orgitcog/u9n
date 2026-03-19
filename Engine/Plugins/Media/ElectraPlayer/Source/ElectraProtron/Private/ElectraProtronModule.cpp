// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraProtronPrivate.h"

#include "Modules/ModuleManager.h"

#include "IElectraProtronModule.h"
#include "Player/ElectraProtronPlayer.h"

DEFINE_LOG_CATEGORY(LogElectraProtron);

class FElectraProtronModule
	: public IElectraProtronModule
{
public:

	//~ IElectraProtronModule interface

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		return MakeShared<FElectraProtronPlayer, ESPMode::ThreadSafe>(EventSink);
	}

public:
	virtual void StartupModule() override
	{
		// We use resource functionality from the Electra Player, so we make sure the modules are loaded.
		FModuleManager::Get().GetModule("ElectraPlayerPlugin");
		FModuleManager::Get().GetModule("ElectraPlayerRuntime");
	}
	virtual void ShutdownModule() override
	{ }

};

IMPLEMENT_MODULE(FElectraProtronModule, ElectraProtron)

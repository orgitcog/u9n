// Copyright Epic Games, Inc. All Rights Reserved.

#include "ITakeRecorderNamingTokensModule.h"
#include "TakeRecorderNamingTokensLog.h"

class FTakeRecorderNamingTokensModule : public ITakeRecorderNamingTokensModule
{
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FTakeRecorderNamingTokensModule, TakeRecorderNamingTokens);
DEFINE_LOG_CATEGORY(LogTakeRecorderNamingTokens);
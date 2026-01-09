// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class SUBTITLESANDCLOSEDCAPTIONSTEST_API FSubtitlesAndClosedCaptionsTestModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked("SubtitlesAndClosedCaptions");
	}
};

IMPLEMENT_MODULE(FSubtitlesAndClosedCaptionsTestModule, SubtitlesAndClosedCaptionsTest);

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Modules/ModuleInterface.h"

class LIVELINKHUB_API ILiveLinkHubModule : public IModuleInterface
{
public:
	UE_DEPRECATED(5.6, "There's no need to call this method anymore, it's now handled by StartupModule and ShutdownModule.")
	virtual void PreinitializeLiveLinkHub() {}

	UE_DEPRECATED(5.6, "There's no need to call this method anymore, it's now handled by StartupModule and ShutdownModule.")
	/** Launch the slate application hosting the live link hub. */
	virtual void StartLiveLinkHub(bool bLauncherDistribution = false) {}

	UE_DEPRECATED(5.6, "There's no need to call this method anymore, it's now handled by StartupModule and ShutdownModule.")
	virtual void ShutdownLiveLinkHub() {}

	/** Retrieve the namespace for Live Link Hub naming tokens. */
	static FString GetLiveLinkHubNamingTokensNamespace()
	{
		return TEXT("llh");
	}
};

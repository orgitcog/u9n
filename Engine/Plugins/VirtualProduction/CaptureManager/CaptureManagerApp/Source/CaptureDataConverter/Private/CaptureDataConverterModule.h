// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FCaptureDataConverterModule : public IModuleInterface
{
public:
	
	bool IsThirdPartyEncoderAvailable();
	FString GetThirdPartyEncoder() const;
	FString GetThirdPartyEncoderVideoCommandArguments() const;
	FString GetThirdPartyEncoderAudioCommandArguments() const;

private:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool CheckThirdPartyEncoderAvailability() const;

	bool bIsThirdPartyEncoderEnabled = false;
};
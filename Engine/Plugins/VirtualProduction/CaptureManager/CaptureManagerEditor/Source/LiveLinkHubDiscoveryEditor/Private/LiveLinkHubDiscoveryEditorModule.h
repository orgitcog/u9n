// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

namespace UE::CaptureManager
{
class FDiscoveryResponder;
}

class FLiveLinkHubDiscoveryEditor : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<UE::CaptureManager::FDiscoveryResponder> DiscoveryResponder;
};

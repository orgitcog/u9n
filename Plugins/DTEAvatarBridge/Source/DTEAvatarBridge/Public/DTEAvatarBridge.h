// Copyright Deep Tree Echo. All Rights Reserved.
#pragma once

#include "Modules/ModuleManager.h"

class FDTEAvatarBridgeModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

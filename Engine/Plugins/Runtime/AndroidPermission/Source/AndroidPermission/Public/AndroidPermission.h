// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

ANDROIDPERMISSION_API DECLARE_LOG_CATEGORY_EXTERN(LogAndroidPermission, Log, All);

class FAndroidPermissionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

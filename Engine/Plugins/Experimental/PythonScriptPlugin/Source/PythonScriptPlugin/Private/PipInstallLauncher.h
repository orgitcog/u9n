// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPipInstall.h"

namespace PipInstallLauncher
{
	bool StartSync(IPipInstall& PipInstall);
	bool StartAsync(IPipInstall& PipInstall);
}

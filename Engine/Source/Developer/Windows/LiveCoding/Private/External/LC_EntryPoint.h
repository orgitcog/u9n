// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if LC_VERSION == 1

#include "CoreTypes.h"
#include "Windows/MinimalWindowsApi.h"

void Startup(Windows::HINSTANCE hInstance);
void Shutdown();


#endif // LC_VERSION
// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"

#define UE_API WAVETABLE_API


namespace WaveTable
{
class FModule : public IModuleInterface
{
public:
	UE_API virtual void StartupModule() override;
	UE_API virtual void ShutdownModule() override;
};
} // namespace WaveTable

#undef UE_API

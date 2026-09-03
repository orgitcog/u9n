// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "SoundFields.h"

class FSoundFieldsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:
	FAmbisonicsSoundfieldFormat SFFactory;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include "Modules/ModuleManager.h"

class FAudioMixerModuleXAudio2 : public IAudioDeviceModule
{
public:
	virtual void StartupModule() override;
	virtual bool IsAudioMixerModule() const override;
	virtual Audio::IAudioMixerPlatformInterface* CreateAudioMixerPlatformInterface() override;
};

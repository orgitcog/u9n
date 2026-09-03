// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerXAudio2.h"
#include "AudioMixerPlatformXAudio2.h"
#include "AudioMixer.h"
#include "Modules/ModuleManager.h"

void FAudioMixerModuleXAudio2::StartupModule() 
{
	IAudioDeviceModule::StartupModule();

	FModuleManager::Get().LoadModuleChecked(TEXT("AudioMixer"));
	FModuleManager::Get().LoadModuleChecked(TEXT("AudioMixerCore"));
}

bool FAudioMixerModuleXAudio2::IsAudioMixerModule() const
{
	return true;
}

Audio::IAudioMixerPlatformInterface* FAudioMixerModuleXAudio2::CreateAudioMixerPlatformInterface() 
{
	return new Audio::FMixerPlatformXAudio2();
}

#if PLATFORM_WINDOWS
IMPLEMENT_MODULE(FAudioMixerModuleXAudio2, AudioMixerXAudio2);
#endif //PLATFORM_WINDOWS

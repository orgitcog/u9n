// Copyright Epic Games, Inc. All Rights Reserved.


#include "AppleProResMediaModule.h"

#include "AppleProResEncoderProtocol.h"

#include "Interfaces/IPluginManager.h"

#include "Misc/Paths.h"

#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#include "Windows/WmfMediaAppleProResDecoder.h"

#include "IWmfMediaModule.h"

#include "WmfMediaCodec/WmfMediaCodecGenerator.h"
#include "WmfMediaCodec/WmfMediaCodecManager.h"

#include "WmfMediaCommon.h"
#endif

#if WITH_EDITOR
	#include "AppleProResMediaSettings.h"
	#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "ProRes"

DEFINE_LOG_CATEGORY(LogAppleProResMedia);

class FAppleProResMediaModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
#if PLATFORM_WINDOWS
		check(LibHandle == nullptr);

		const FString ProResDll = TEXT("ProResToolbox.dll");

		// determine directory paths
		FString ProResDllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("AppleProResMedia"))->GetBaseDir(), TEXT("/Binaries/ThirdParty/Win64"));
		FPlatformProcess::PushDllDirectory(*ProResDllPath);
		ProResDllPath = FPaths::Combine(ProResDllPath, ProResDll);

		if (!FPaths::FileExists(ProResDllPath))
		{
			UE_LOG(LogAppleProResMedia, Error, TEXT("Failed to find the binary folder for the ProRes dll. Plug-in will not be functional."));
			return;
		}

		LibHandle = FPlatformProcess::GetDllHandle(*ProResDllPath);

		if (LibHandle == nullptr)
		{
			UE_LOG(LogAppleProResMedia, Error, TEXT("Failed to load required library %s. Plug-in will not be functional."), *ProResDllPath);
			return;
		}

		if (IWmfMediaModule* Module = IWmfMediaModule::Get())
		{
			if (Module->IsInitialized())
			{
				Module->GetCodecManager()->AddCodec(MakeUnique<WmfMediaCodecGenerator<WmfMediaAppleProResDecoder>>(true));
			}
		}
#endif

#if WITH_EDITOR
		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "AppleProResMedia",
				LOCTEXT("AppleProResMediaSettingsName", "Apple ProRes Media"),
				LOCTEXT("AppleProResMediaSettingsDescription", "Configure the Apple ProRes Media plug-in."),
				GetMutableDefault<UAppleProResMediaSettings>()
			);
		}
#endif //WITH_EDITOR

		// Add exemption to FName::NameToDisplayString formatting to ensure "ProRes" is displayed without a space
		FName::AddNameToDisplayStringExemption(TEXT("ProRes"));
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "AppleProResMedia");
		}
#endif //WITH_EDITOR

		if (LibHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(LibHandle);
			LibHandle = nullptr;
		}
	}

	// Codec could still be in use
	virtual bool SupportsDynamicReloading()
	{
		return false;
	}

private:

	static void* LibHandle;
};

void* FAppleProResMediaModule::LibHandle = nullptr;



IMPLEMENT_MODULE(FAppleProResMediaModule, AppleProResMedia)

#undef LOCTEXT_NAMESPACE

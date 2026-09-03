// Copyright Epic Games, Inc. All Rights Reserved.

#if LC_VERSION == 2

#include "LiveCodingModule2.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "LiveCodingLog.h"

#pragma optimize("", off)

#include "Windows/AllowWindowsPlatformTypes.h"
#include "LPP_API_x64_CPP.h"
#include "Windows/HideWindowsPlatformTypes.h"

IMPLEMENT_MODULE(FLiveCodingModule, LiveCoding)

#define LOCTEXT_NAMESPACE "LiveCodingModule"

LLM_DEFINE_TAG(LiveCoding);

struct FLiveCodingModule::FKeyProcessor : public IInputProcessor
{
	FKeyProcessor(FLiveCodingModule& M) : Module(M) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

    virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
	{
		if (KeyEvent.GetKey() != EKeys::F11)
		{
			return false;
		}

		FModifierKeysState KeyState = SlateApp.GetModifierKeys();
		if (!KeyState.IsLeftControlDown() || !KeyState.IsLeftAltDown())
		{
			return false;
		}

		Module.StartLivePlusPlus(true);
		return true;
	}

	FLiveCodingModule& Module;
};

FLiveCodingModule::FLiveCodingModule()
{
}

FLiveCodingModule::~FLiveCodingModule()
{
}

void FLiveCodingModule::StartupModule()
{
	LLM_SCOPE_BYTAG(LiveCoding);

	if (IsBrokerRunning())
	{
		StartLivePlusPlus(false);
	}
	else if (FSlateApplication::IsInitialized())
    {
		KeyProcessor = MakeShared<FKeyProcessor>(*this);
        FSlateApplication::Get().RegisterInputPreProcessor(KeyProcessor);
    }

	EndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLiveCodingModule::Tick);
}

void FLiveCodingModule::ShutdownModule()
{
	if (KeyProcessor && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(KeyProcessor);
	}
	FCoreDelegates::OnEndFrame.Remove(EndFrameDelegateHandle);
}

void FLiveCodingModule::EnableByDefault(bool bEnable)
{
}

bool FLiveCodingModule::IsEnabledByDefault() const
{
	return true;
}

void FLiveCodingModule::EnableForSession(bool bEnable)
{
}

bool FLiveCodingModule::IsEnabledForSession() const
{
	return true;
}

const FText& FLiveCodingModule::GetEnableErrorText() const
{
	return EnableErrorText;
}

bool FLiveCodingModule::AutomaticallyCompileNewClasses() const
{
	return false;
}

bool FLiveCodingModule::CanEnableForSession() const
{
#if !IS_MONOLITHIC
	FModuleManager& ModuleManager = FModuleManager::Get();
	if(ModuleManager.HasAnyOverridenModuleFilename())
	{
		return false;
	}
#endif
	return true;
}

bool FLiveCodingModule::HasStarted() const
{
	return true;
}

void FLiveCodingModule::ShowConsole()
{
}

void FLiveCodingModule::Compile()
{
}

bool FLiveCodingModule::Compile(ELiveCodingCompileFlags CompileFlags, ELiveCodingCompileResult* Result)
{
	return false;
}

bool FLiveCodingModule::IsCompiling() const
{
	return false;
}

void FLiveCodingModule::Tick()
{
}

ILiveCodingModule::FOnPatchCompleteDelegate& FLiveCodingModule::GetOnPatchCompleteDelegate()
{
	return OnPatchCompleteDelegate;
}

bool FLiveCodingModule::IsBrokerRunning()
{
	FPlatformProcess::FProcEnumerator ProcessEnumerator;
	while (ProcessEnumerator.MoveNext())
	{
		const FString ProcessName = ProcessEnumerator.GetCurrent().GetName();
		if (ProcessName == TEXT("LPP_Broker.exe"))
			return true;
	}
	return false;
}

void FLiveCodingModule::StartLivePlusPlus(bool StartBroker)
{
	if (KeyProcessor)
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(KeyProcessor);
		KeyProcessor = nullptr;
	}

	FString LivePlusPlusPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/") / FPlatformProcess::GetBinariesSubdirectory() / TEXT("LivePlusPlus"));

	if (StartBroker)
	{
		FString BrokerExe = LivePlusPlusPath / TEXT("LPP_Broker.exe");
		uint32 ProcessId;
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*BrokerExe, NULL, true, true, true, &ProcessId, 0, NULL, NULL, NULL);
		if (!ProcessHandle.IsValid())
		{
			UE_LOG(LogLiveCoding, Error, TEXT("Failed to start broker '%s'."), *BrokerExe);
			return;
		}
	}

	using namespace lpp;

	LppLocalPreferences LocalPreferences = LppCreateDefaultLocalPreferences();
	LppProjectPreferences ProjectPreferences = LppCreateDefaultProjectPreferences();

	LppSynchronizedAgent agent = LPP_DEFAULT_INIT(LPP_INVALID_MODULE);

	//HMODULE lppModule = NULL;
	//GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)&GIsCompileActive, &lppModule);

	//FString AgentDllExe = LivePlusPlusPath / TEXT("LPP_Broker.exe");
	LppSynchronizedAgent Agent = LppCreateSynchronizedAgent(&LocalPreferences, *LivePlusPlusPath);
	//LppStartupDefaultAgent(&agent, &LocalPreferences, LPP_NULL, &ProjectPreferences);

	//typedef void LppStartupFunction(LppDefaultAgent*, const LppLocalPreferences* const, const wchar_t* const, const LppProjectPreferences* const);
	//LppStartupFunction* startup = LPP_REINTERPRET_CAST(LppStartupFunction*)(LppPlatformGetFunctionAddress(lppModule, "LppStartupDefaultAgent"));
	//startup(&agent, &LocalPreferences, LPP_NULL, &ProjectPreferences);
}

#undef LOCTEXT_NAMESPACE

#endif // LC_VERSION == 2

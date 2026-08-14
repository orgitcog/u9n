// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPUReshape, Log, All);

IMPLEMENT_APPLICATION(GPUReshapeBootstrapper, "GPUReshapeBootstrapper");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	/**
	 * Gauntlet GPU Reshape Utility Helper
	 *
	 * Bootstrap Tree:
	 *   Gauntlet -> GPUReshapeBootstrapper -> GPUReshape -> Target
	 */
	
	FTaskTagScope Scope(ETaskTag::EGameThread);
	
	ON_SCOPE_EXIT
	{ 
		LLM(FLowLevelMemTracker::Get().UpdateStatsPerFrame());
		RequestEngineExit(TEXT("Exiting"));
		
		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	};

	if (int32 Ret = GEngineLoop.PreInit(ArgC, ArgV))
	{
		return Ret;
	}
	
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("WaitForDebugger")))
	{
		while (!FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformProcess::Sleep(0.1f);
		}
		
		UE_DEBUG_BREAK();
	}
#endif // !UE_BUILD_SHIPPING

	FString BootstrapTarget;
	if (!FParse::Value(FCommandLine::Get(), TEXT("BootstrapTarget="), BootstrapTarget))
	{
		UE_LOG(LogGPUReshape, Error, TEXT("Target executable path not set"));
		return 1u;
	}
	
	FString GPUReshapePath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GRS.Path="), GPUReshapePath))
	{
		UE_LOG(LogGPUReshape, Error, TEXT("GPU Reshape path not set"));
		return 1u;
	}
	
	FString WorkspacePath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GRS.Workspace="), WorkspacePath))
	{
		UE_LOG(LogGPUReshape, Error, TEXT("Workspace path not set"));
		return 1u;
	}
	
	FString ReportPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GRS.Report="), ReportPath))
	{
		UE_LOG(LogGPUReshape, Error, TEXT("Report path not set"));
		return 1u;
	}
	
	int32 Timeout = 7200;
	FParse::Value(FCommandLine::Get(), TEXT("GRS.Timeout="), Timeout);
	
	FString SymbolPath = "";
	FParse::Value(FCommandLine::Get(), TEXT("GRS.SymbolPath="), SymbolPath);
	
	// Start in headless mode
	FStringBuilderBase GRSCommandLine;
	GRSCommandLine << "launch ";
	GRSCommandLine << "-report " << "\"" << ReportPath << "\" ";
	GRSCommandLine << "-workspace \"" << WorkspacePath << "\" ";
	GRSCommandLine << "-timeout " << Timeout << " ";
	GRSCommandLine << "-symbol " << SymbolPath << " ";
	GRSCommandLine << "-app " << BootstrapTarget;

	for (int32_t i = 1; i < ArgC; i++)
	{
		GRSCommandLine << " " << ArgV[i];
	}
	
	// Launch the editor bootstrapped through reshape
	FProcHandle Handle = FPlatformProcess::CreateProc(
		*GPUReshapePath,
		GRSCommandLine.GetData(),
		true,
		false,
		false,
		nullptr,
		0,
		nullptr,
		nullptr
	);
	
	if (!Handle.IsValid())
	{
		UE_LOG(LogGPUReshape, Error, TEXT("Failed to launch bootstrapped application"));
		return 1u;
	}

	// Wait for reshape to finish
	FPlatformProcess::WaitForProc(Handle);

	int32 ReturnCode = 1u;
	FPlatformProcess::GetProcReturnCode(Handle, &ReturnCode);
	FPlatformProcess::CloseProc(Handle);
	return ReturnCode;
}

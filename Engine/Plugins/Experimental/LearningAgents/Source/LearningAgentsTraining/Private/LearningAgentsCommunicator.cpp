// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsCommunicator.h"

#include "LearningAgentsInteractor.h"
#include "LearningExternalTrainer.h"

#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

FLearningAgentsCommunicator ULearningAgentsCommunicatorLibrary::MakeSharedMemoryTrainingProcess(
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSharedMemoryCommunicatorSettings& SharedMemorySettings)
{
	FLearningAgentsSharedMemoryTrainerProcess TrainerProcess = SpawnSharedMemoryTrainingProcess(TrainerProcessSettings, SharedMemorySettings);
	return MakeSharedMemoryCommunicator(TrainerProcess, TrainerProcessSettings, SharedMemorySettings);
}

FLearningAgentsSharedMemoryTrainerProcess ULearningAgentsCommunicatorLibrary::SpawnSharedMemoryTrainingProcess(
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSharedMemoryCommunicatorSettings& SharedMemorySettings)
{
	FLearningAgentsSharedMemoryTrainerProcess TrainerProcess;

	if (FParse::Param(FCommandLine::Get(), TEXT("LearningAgentsUseExternalTrainingProcess")) || SharedMemorySettings.bUseExternalTrainingProcess)
	{
		UE_LOG(LogLearning, Display, TEXT("SpawnSharedMemoryTrainingProcess: Skipping spawning because bUseExternalTrainingProcess is True."));
		return TrainerProcess;
	}

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(TrainerProcessSettings.GetIntermediatePath());
	if (!FPaths::FileExists(PythonExecutablePath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSharedMemoryTrainingProcess: Can't find Python executable \"%s\"."), *PythonExecutablePath);
		return TrainerProcess;
	}

	const FString PythonContentPath = UE::Learning::Trainer::GetPythonContentPath(TrainerProcessSettings.GetEditorEnginePath());
	if (!FPaths::DirectoryExists(PythonContentPath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSharedMemoryTrainingProcess: Can't find LearningAgents plugin Content \"%s\"."), *PythonContentPath);
		return TrainerProcess;
	}

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(TrainerProcessSettings.GetIntermediatePath());

	const FString CustomTrainerModulePath = TrainerProcessSettings.GetCustomTrainerModulePath();
	if (!CustomTrainerModulePath.IsEmpty() and !FPaths::DirectoryExists(CustomTrainerModulePath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSharedMemoryTrainingProcess: Can't find custom trainer module \"%s\"."), *CustomTrainerModulePath);
		return TrainerProcess;
	}

	TrainerProcess.TrainerProcess = MakeShared<UE::Learning::FSharedMemoryTrainerServerProcess>(
		TrainerProcessSettings.TaskName,
		CustomTrainerModulePath,
		TrainerProcessSettings.TrainerFileName,
		PythonExecutablePath,
		PythonContentPath,
		IntermediatePath,
		SharedMemorySettings.Timeout);

	return TrainerProcess;
}

FLearningAgentsCommunicator ULearningAgentsCommunicatorLibrary::MakeSharedMemoryCommunicator(
	FLearningAgentsSharedMemoryTrainerProcess TrainerProcess,
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSharedMemoryCommunicatorSettings& SharedMemorySettings)
{
	FLearningAgentsCommunicator Communicator;

	// Task Name
	FString CmdLineTaskName;
	bool bTaskNameOverridden = FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsTaskName="), CmdLineTaskName);
	if (bTaskNameOverridden)
	{
		UE_LOG(LogLearning, Display, TEXT("Overriding Task Name from the cmdline: %s"), *CmdLineTaskName);
	}

	// Timeout
	float CmdLineTimeout;
	bool bTimeoutOverridden = FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsTimeout="), CmdLineTimeout);
	if (bTimeoutOverridden)
	{
		UE_LOG(LogLearning, Display, TEXT("Overriding Timeout with value from the cmdline: %f"), CmdLineTimeout);
	}

	// Controls Guid
	// We have to use FGuid::Parse to accept more guid formats
	FString CmdLineControlsGuidString;
	FGuid CmdLineControlsGuid;
	if (FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsControlsGuid="), CmdLineControlsGuidString) && FGuid::Parse(CmdLineControlsGuidString, CmdLineControlsGuid))
	{
		UE_LOG(LogLearning, Display, TEXT("Overriding Controls Guid with value from the cmdline: %s"), *CmdLineControlsGuid.ToString(PLATFORM_MAC ? EGuidFormats::Short : EGuidFormats::DigitsWithHyphensInBraces));
	}

	// Config Path
	FString CmdLineConfigPath;
	bool bConfigPathOverridden = FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsConfigPath="), CmdLineConfigPath);
	if (bConfigPathOverridden)
	{
		UE_LOG(LogLearning, Display, TEXT("Overriding Config Path with value from the cmdline: %s"), *CmdLineConfigPath);
	}

	Communicator.Trainer = MakeShared<UE::Learning::FSharedMemoryTrainer>(
		bTaskNameOverridden ? CmdLineTaskName : TrainerProcessSettings.TaskName,
		TrainerProcess.TrainerProcess,
		FParse::Param(FCommandLine::Get(), TEXT("LearningAgentsUseExternalTrainingProcess")) || SharedMemorySettings.bUseExternalTrainingProcess,
		CmdLineControlsGuid.IsValid() ? CmdLineControlsGuid : SharedMemorySettings.ControlsGuid,
		bConfigPathOverridden ? CmdLineConfigPath : SharedMemorySettings.ConfigPath.Path,
		UE::Learning::Trainer::GetIntermediatePath(TrainerProcessSettings.GetIntermediatePath()),
		bTimeoutOverridden ? CmdLineTimeout : SharedMemorySettings.Timeout);

	return Communicator;
}

FLearningAgentsCommunicator ULearningAgentsCommunicatorLibrary::MakeSocketTrainingProcess(
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSocketCommunicatorSettings& SocketSettings)
{
	FLearningAgentsSocketTrainerProcess TrainerProcess = SpawnSocketTrainingProcess(TrainerProcessSettings, SocketSettings);
	return MakeSocketCommunicator(TrainerProcess, SocketSettings);
}

FLearningAgentsSocketTrainerProcess ULearningAgentsCommunicatorLibrary::SpawnSocketTrainingProcess(
	const FLearningAgentsTrainerProcessSettings& TrainerProcessSettings,
	const FLearningAgentsSocketCommunicatorSettings& SocketSettings)
{
	FLearningAgentsSocketTrainerProcess TrainerProcess;

	if (FParse::Param(FCommandLine::Get(), TEXT("LearningAgentsUseExternalTrainingProcess")) || SocketSettings.bUseExternalTrainingProcess)
	{
		UE_LOG(LogLearning, Display, TEXT("SpawnSocketTrainingProcess: Skipping spawning because bUseExternalTrainingProcess is True."));
		return TrainerProcess;
	}

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(TrainerProcessSettings.GetIntermediatePath());
	if (!FPaths::FileExists(PythonExecutablePath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSocketTrainingProcess: Can't find Python executable \"%s\"."), *PythonExecutablePath);
		return TrainerProcess;
	}

	const FString PythonContentPath = UE::Learning::Trainer::GetPythonContentPath(TrainerProcessSettings.GetEditorEnginePath());
	if (!FPaths::DirectoryExists(PythonContentPath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSocketTrainingProcess: Can't find LearningAgents plugin Content \"%s\"."), *PythonContentPath);
		return TrainerProcess;
	}

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(TrainerProcessSettings.GetIntermediatePath());

	const FString CustomTrainerModulePath = TrainerProcessSettings.GetCustomTrainerModulePath();
	if (!CustomTrainerModulePath.IsEmpty() and !FPaths::DirectoryExists(CustomTrainerModulePath))
	{
		UE_LOG(LogLearning, Error, TEXT("SpawnSocketTrainingProcess: Can't find custom trainer module \"%s\"."), *CustomTrainerModulePath);
		return TrainerProcess;
	}

	TrainerProcess.TrainerProcess = MakeShared<UE::Learning::FSocketTrainerServerProcess>(
		TrainerProcessSettings.TaskName,
		CustomTrainerModulePath,
		TrainerProcessSettings.TrainerFileName,
		PythonExecutablePath,
		PythonContentPath,
		IntermediatePath,
		*SocketSettings.IpAddress,
		SocketSettings.Port,
		SocketSettings.Timeout);

	return TrainerProcess;
}

FLearningAgentsCommunicator ULearningAgentsCommunicatorLibrary::MakeSocketCommunicator(
	FLearningAgentsSocketTrainerProcess TrainerProcess,
	const FLearningAgentsSocketCommunicatorSettings& SocketSettings)
{
	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;

	// Ip Address
	FString CmdLineIpAddress;
	bool bIpAddressOverridden = FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsIpAddress="), CmdLineIpAddress);

	// Hostname
	FString CmdLineHostname;
	bool bHostnameOverridden = FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsHostname="), CmdLineHostname);
	if (bHostnameOverridden)
	{
		UE_LOG(LogLearning, Display, TEXT("Using Hostname override from the cmdline: %s"), *CmdLineHostname);
	}
	else if (bIpAddressOverridden)
	{
		UE_LOG(LogLearning, Display, TEXT("Using Ip Address override from the cmdline: %s"), *CmdLineIpAddress);
	}

	// Port
	uint32 CmdLinePort;
	bool bPortOverridden = FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsPort="), CmdLinePort);
	if (bPortOverridden)
	{
		UE_LOG(LogLearning, Display, TEXT("Using Port override from the cmdline: %u"), CmdLinePort);
	}

	// Timeout
	float CmdLineTimeout;
	bool bTimeoutOverridden = FParse::Value(FCommandLine::Get(), TEXT("LearningAgentsTimeout="), CmdLineTimeout);
	if (bTimeoutOverridden)
	{
		UE_LOG(LogLearning, Display, TEXT("Overriding Timeout with value from the cmdline: %f"), CmdLineTimeout);
	}

	FLearningAgentsCommunicator Communicator;
	Communicator.Trainer = MakeShared<UE::Learning::FSocketTrainer>(
		Response,
		TrainerProcess.TrainerProcess,
		FParse::Param(FCommandLine::Get(), TEXT("LearningAgentsUseExternalTrainingProcess")) || SocketSettings.bUseExternalTrainingProcess,
		bHostnameOverridden
			? *CmdLineHostname
			: bIpAddressOverridden ? *CmdLineIpAddress: *SocketSettings.IpAddress,
		bPortOverridden ? CmdLinePort : SocketSettings.Port,
		bTimeoutOverridden ? CmdLineTimeout : SocketSettings.Timeout,
		bHostnameOverridden);

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeSocketCommunicator: Failed to connect to training process: %s. Check log for additional errors."), UE::Learning::Trainer::GetResponseString(Response));
		Communicator.Trainer->Terminate();
	}

	return Communicator;
}

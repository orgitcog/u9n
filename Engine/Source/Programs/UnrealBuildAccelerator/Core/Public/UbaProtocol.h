// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

namespace uba
{
	#define UBA_PROCESS_MESSAGES \
		UBA_PROCESS_MESSAGE(Init) \
		UBA_PROCESS_MESSAGE(CreateFile) \
		UBA_PROCESS_MESSAGE(GetFullFileName) \
		UBA_PROCESS_MESSAGE(GetLongPathName) \
		UBA_PROCESS_MESSAGE(CloseFile) \
		UBA_PROCESS_MESSAGE(DeleteFile) \
		UBA_PROCESS_MESSAGE(CopyFile) \
		UBA_PROCESS_MESSAGE(MoveFile) \
		UBA_PROCESS_MESSAGE(Chmod) \
		UBA_PROCESS_MESSAGE(CreateDirectory) \
		UBA_PROCESS_MESSAGE(RemoveDirectory) \
		UBA_PROCESS_MESSAGE(ListDirectory) \
		UBA_PROCESS_MESSAGE(UpdateTables) \
		UBA_PROCESS_MESSAGE(GetWrittenFiles) \
		UBA_PROCESS_MESSAGE(CreateProcess) \
		UBA_PROCESS_MESSAGE(StartProcess) \
		UBA_PROCESS_MESSAGE(ExitChildProcess) \
		UBA_PROCESS_MESSAGE(VirtualAllocFailed) \
		UBA_PROCESS_MESSAGE(Log) \
		UBA_PROCESS_MESSAGE(EchoOn) \
		UBA_PROCESS_MESSAGE(InputDependencies) \
		UBA_PROCESS_MESSAGE(Exit) \
		UBA_PROCESS_MESSAGE(FlushWrittenFiles) \
		UBA_PROCESS_MESSAGE(UpdateEnvironment) \
		UBA_PROCESS_MESSAGE(GetNextProcess) \
		UBA_PROCESS_MESSAGE(Custom) \
		UBA_PROCESS_MESSAGE(SHGetKnownFolderPath) \
		UBA_PROCESS_MESSAGE(RpcCommunication) \
		UBA_PROCESS_MESSAGE(HostRun) \
		UBA_PROCESS_MESSAGE(ResolveCallstack) \
		UBA_PROCESS_MESSAGE(CheckRemapping) \
		UBA_PROCESS_MESSAGE(RunSpecialProgram) \

	enum MessageType : u8
	{
		MessageType_None = 0,
		#define UBA_PROCESS_MESSAGE(type) MessageType_##type,
		UBA_PROCESS_MESSAGES
		#undef UBA_PROCESS_MESSAGE
	};

	inline constexpr u32 ProcessMessageVersion = 1346;

	inline constexpr u32 CommunicationMemSize = IsWindows ? 64*1024 : 64*1024*2; // Macos expands some commandlines to be crazy long

	inline constexpr u32 FileMappingTableMemSize = 16 * 1024 * 1024;
	inline constexpr u32 DirTableMemSize = 128 * 1024 * 1024;
}

#define UBA_ENABLE_ON_DISK_FILE_MAPPINGS 0

// Currently only used for detoured process
#if UBA_DEBUG
#define UBA_DEBUG_LOG_ENABLED 1
#define UBA_DEBUG_VALIDATE 0 // Does not work with some unit tests
#else
#define UBA_DEBUG_LOG_ENABLED 0
#define UBA_DEBUG_VALIDATE 0
#endif

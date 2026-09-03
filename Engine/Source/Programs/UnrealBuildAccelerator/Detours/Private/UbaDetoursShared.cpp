// Copyright Epic Games, Inc. All Rights Reserved.

#define UBA_IS_DETOURED_INCLUDE 1

#include "UbaDetoursShared.h"
#include "UbaDetoursFileMappingTable.h"
#include "UbaDirectoryTable.h"
#include "UbaTimer.h"

namespace uba
{
	VARIABLE_MEM(StringBuffer<512>, g_virtualApplication);
	VARIABLE_MEM(StringBuffer<512>, g_virtualApplicationDir);
	VARIABLE_MEM(ProcessStats, g_stats);
	VARIABLE_MEM(KernelStats, g_kernelStats);
	VARIABLE_MEM(ReaderWriterLock, g_communicationLock);
	VARIABLE_MEM(StringBuffer<256>, g_logName);
	VARIABLE_MEM(StringBuffer<512>, g_virtualWorkingDir);
	VARIABLE_MEM(StringBuffer<256>, g_exeDir);
	VARIABLE_MEM(StringBuffer<128>, g_systemRoot);
	VARIABLE_MEM(StringBuffer<128>, g_systemTemp);
	VARIABLE_MEM(MemoryBlock, g_memoryBlock);
	VARIABLE_MEM(DirectoryTable, g_directoryTable);
	VARIABLE_MEM(MappedFileTable, g_mappedFileTable);
	VARIABLE_MEM(ReaderWriterLock, g_consoleStringCs);

	bool g_echoOn = true;
	u32 g_rulesIndex;
	ApplicationRules* g_rules;
	bool g_runningRemote;
	bool g_isChild;
	bool g_allowKeepFilesInMemory = IsWindows;
	bool g_allowOutputFiles = IsWindows;
	bool g_suppressLogging = false;

	void InitSharedVariables()
	{
		g_virtualApplicationMem.Create();
		g_virtualApplicationDirMem.Create();
		g_statsMem.Create();
		g_kernelStatsMem.Create();
		g_communicationLockMem.Create();
		g_logNameMem.Create();
		g_virtualWorkingDirMem.Create();
		g_exeDirMem.Create();
		g_systemRootMem.Create();
		g_systemTempMem.Create();

		u64 reserveSizeMb = IsWindows ? 256 : 1024; // The sync primitives on linux/macos is much bigger
		g_memoryBlockMem.Create(reserveSizeMb * 1024 * 1024);
		g_directoryTableMem.Create(g_memoryBlock);
		g_mappedFileTableMem.Create(g_memoryBlock);
		g_consoleStringCsMem.Create();
	}

#if UBA_DEBUG_LOG_ENABLED
	FileHandle g_debugFile = InvalidFileHandle;
	void WriteDebug(const void* data, u32 dataLen);
	constexpr const char g_emptyString[] = "                                                     ";
	constexpr const char* g_emptyStringEnd = ((const char*)g_emptyString) + sizeof_array(g_emptyString) - 1;
	thread_local StringBuffer<LogBufSize> t_a;
	thread_local char t_b[LogBufSize];
	thread_local u32 t_b_size;
	thread_local u32 t_logScopeCount;
	Futex g_logScopeLock;

	void GetPrefixExtra(StringBufferBase& out)
	{
		#if 0
		static u64 startTime = GetTime();
		u64 timeMs = TimeToMs(GetTime() - startTime);
		u64 ms = timeMs % 1000;
		u64 s = timeMs / 1000;

		out.Appendf(TC("[%5llu.%03llu]"), s, ms);
		#endif
		//out.Appendf(TC("[%7u]"), GetCurrentThreadId());
	}
	void FlushDebug()
	{
		WriteDebug(t_b, t_b_size);
		t_b_size = 0;
		t_b[0] = 0;
	}
	void WriteDebugLogWithPrefix(const char* prefix, LogScope& scope, const tchar* command, const tchar* format, ...)
	{
		#if PLATFORM_MAC
		static locale_t safeLocale = newlocale(LC_NUMERIC_MASK, "C", duplocale(LC_GLOBAL_LOCALE));
		locale_t oldLocale = uselocale(safeLocale);
		#endif

		t_a.Clear().Append(command).Append(' ');
		if (*format)
		{
			va_list arg;
			va_start(arg, format);
			t_a.Append(format, arg);
			va_end(arg);
		}
		t_a.Append(TCV("\n"));

		u32 size__ = t_b_size;
		StringBuffer<128> extra;
		GetPrefixExtra(extra);

		#if PLATFORM_WINDOWS
		u32 res__ = sprintf_s(t_b + size__, LogBufSize - size__, "%s %S   %s%S", prefix, extra.data, g_emptyStringEnd - t_logScopeCount * 2, t_a.data);
		#else
		u32 res__ = snprintf(t_b + size__, LogBufSize - size__, "%s %s   %s%s", prefix, extra.data, g_emptyStringEnd - t_logScopeCount * 2, t_a.data);
		#endif
		if (res__ != -1)
			t_b_size += res__;
		scope.Flush();

		#if PLATFORM_MAC
		uselocale(oldLocale);
		#endif
	}

	void WriteDebugLog(const tchar* format, ...)
	{
		t_a.Clear();
		if (*format)
		{
			va_list arg;
			va_start(arg, format);
			t_a.Append(format, arg);
			va_end(arg);
		}
		t_a.Append(TCV("\n"));

		#if PLATFORM_WINDOWS
		if (t_b_size)
			FlushDebug();
		t_b_size = sprintf_s(t_b, LogBufSize, "%S", t_a.data);
		FlushDebug();
		#else
		WriteDebug(t_a.data, t_a.count);
		#endif
	}
	LogScope::LogScope()
	{
		if (++t_logScopeCount > 1)
			return;
		//g_logScopeLock.Enter(); // Deadlocks in a few places
	}
	LogScope::~LogScope()
	{
		if (--t_logScopeCount)
			return;
		if (t_b_size)
			Flush();
		//g_logScopeLock.Leave();
	}
	void LogScope::Flush()
	{
		FlushDebug();
	}
#endif

#if UBA_DEBUG_VALIDATE
	bool g_validateFileAccess = false;
#endif

	thread_local u32 t_disallowDetour = 0; // Set this to 1 to disallow all detouring of I/O interaction
	SuppressDetourScope::SuppressDetourScope() { ++t_disallowDetour; }
	SuppressDetourScope::~SuppressDetourScope() { --t_disallowDetour; }


	bool FixPath(StringBufferBase& out, const tchar* path)
	{
		return FixPath2(path, g_virtualWorkingDir.data, g_virtualWorkingDir.count, out.data, out.capacity, &out.count);
	}

	struct VfsEntry { StringView vfs; StringView local; VfsEntry() : vfs(NoInit), local(NoInit) {}; };
	VfsEntry g_vfsEntries[32];
	u32 g_vfsEntryCount;
	u32 g_vfsMatchingLength;

	void PopulateVfs(BinaryReader& vfsReader)
	{
		while (vfsReader.GetLeft())
		{
			vfsReader.ReadByte(); // Index, unused
			StringBuffer<> str;
			vfsReader.ReadString(str);
			if (!str.count)
			{
				vfsReader.SkipString();
				continue;
			}

			#if PLATFORM_WINDOWS
			str.Replace('/', '\\');
			#endif

			u32 index = g_vfsEntryCount++;
			UBA_ASSERT(index < sizeof_array(g_vfsEntries));
			VfsEntry& vfsEntry = g_vfsEntries[index];
			vfsEntry.vfs = g_memoryBlock.Strdup(str);

			if (index == 0)
				g_vfsMatchingLength = vfsEntry.vfs.count;
			else
			{
				u32 shortest = Min(g_vfsMatchingLength, vfsEntry.vfs.count);
				for (u32 i=0; i!=shortest; ++i)
				{
					if (g_vfsEntries[0].vfs.data[i] == vfsEntry.vfs.data[i])
						continue;
					shortest = i;
					break;
				}
				g_vfsMatchingLength = shortest;
			}
			vfsReader.ReadString(str.Clear());
			vfsEntry.local = g_memoryBlock.Strdup(str);
		}
	}

	bool IsVfsEnabled()
	{
		return g_vfsEntryCount > 0;
	}

	bool DevirtualizePath(StringBufferBase& path)
	{
		if (!g_vfsEntryCount)
			return false;

		if (!Equals(path.data, g_vfsEntries[0].vfs.data, Min(path.count, g_vfsMatchingLength), CaseInsensitiveFs))
			return false;

		// TODO: This is not great, the dirs above the vfs root should be empty except the dir to the roots
		if (path.count < g_vfsMatchingLength)
		{
			path.Clear().Append(g_vfsEntries[0].local);
			return true;
		}

		for (u32 i=0, e=g_vfsEntryCount; i!=e; ++i)
		{
			VfsEntry& entry = g_vfsEntries[i];
			if (!path.StartsWith(entry.vfs.data))
				continue;
			StringBuffer<MaxPath> temp2(path.data + entry.vfs.count);
			path.Clear().Append(entry.local).Append(temp2);
			return true;
		}
		return false;
	}

	bool VirtualizePath(StringBufferBase& path)
	{
		if (!g_vfsEntryCount)
			return false;
		for (u32 i=0, e=g_vfsEntryCount; i!=e; ++i)
		{
			VfsEntry& entry = g_vfsEntries[i];
			if (path.count < entry.local.count || !path.StartsWith(entry.local.data))
				continue;
			StringBuffer<MaxPath> temp2(path.data + entry.local.count);
			path.Clear().Append(entry.vfs).Append(temp2);
			return true;
		}
		return false;
	}

	void LogHeader(StringView cmdLine)
	{
		#if UBA_DEBUG_LOG_ENABLED
		if (g_debugFile == InvalidFileHandle)
			return;
		FlushDebug();
		WriteDebugLog(TC("ProcessId: %u"), g_processId);
		WriteDebug("CmdLine: ", 9);

		#if PLATFORM_WINDOWS
		u32 left = cmdLine.count;
		const tchar* read = cmdLine.data;
		while (left)
		{
			char buf[1024];
			u32 written = 0;
			while (*read && written < sizeof(buf))
				buf[written++] = (char)*read++;
			WriteDebug(buf, written);
			left -= written;
		}
		#else
		WriteDebug(cmdLine.data, cmdLine.count*sizeof(tchar));
		#endif

		WriteDebug("\n", 1);
		WriteDebugLog(TC("WorkingDir: %s"), g_virtualWorkingDir.data);
		WriteDebugLog(TC("ExeDir: %s"), g_virtualApplicationDir.data);
		WriteDebugLog(TC("ExeDir (actual): %s"), g_exeDir.data);
		WriteDebugLog(TC("SystemTemp: %s"), g_systemTemp.data);
		WriteDebugLog(TC("Rules: %u (%u)"), g_rules->index, GetApplicationRules()[g_rules->index].hash);
		if (g_runningRemote)
		{
			StringBuffer<256> computerName;
			GetComputerNameW(computerName);
			WriteDebugLog(TC("Remote: %s"), computerName.data);
		}
		static u32 reuseCounter;
		if (reuseCounter)
			WriteDebugLog(TC("ProcessReuseIndex: %u"), reuseCounter);
		++reuseCounter;
		WriteDebugLog(TC(""));
		FlushDebug();
		#endif
	}

	void LogVfsInfo()
	{
		for (u32 i=0; i!=g_vfsEntryCount; ++i)
		{
			DEBUG_LOG(TC("Vfs: %s -> %s"), g_vfsEntries[i].vfs.data, g_vfsEntries[i].local.data);
		}
	}

	const tchar* GetApplicationShortName()
	{
		const tchar* lastBackslash = TStrrchr(g_virtualApplication.data, '\\');
		const tchar* lastSlash = TStrrchr(g_virtualApplication.data, '/');
		if (lastBackslash || lastSlash)
			return (lastBackslash > lastSlash ? lastBackslash : lastSlash) + 1;
		return g_virtualApplication.data;
	}

	ANALYSIS_NORETURN void FatalError(u32 code, const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		tchar buffer[1024];
		if (Tvsprintf_s(buffer, sizeof_array(buffer), format, arg) <= 0)
			TStrcpy_s(buffer, sizeof_array(buffer), format);
		va_end(arg);
		StringBuffer<2048> sb;
		sb.Append(GetApplicationShortName()).Append(TCV(" ERROR: ")).Append(buffer);
		Rpc_WriteLog(sb.data, sb.count, true, true);

		#if PLATFORM_WINDOWS // Maybe all platforms should call exit()?
		ExitProcess(code);
		#else
		exit(code);
		#endif
	}

	void Rpc_WriteLog(const tchar* text, u64 textCharLength, bool printInSession, bool isError)
	{
		DEBUG_LOG(TC("LOG  %.*s"), u32(textCharLength), text); // TODO: Investigate, deadlocks on non-windows
		// DEBUG_LOG(TC("LOG [%7u] %.*s"), GetCurrentThreadId(), u32(textCharLength), text);
		RPC_MESSAGE(Log, log)
		writer.WriteBool(printInSession);
		writer.WriteBool(isError);
		writer.WriteString(text, textCharLength);
		writer.Flush();
	}

	void Rpc_WriteLogf(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		tchar buffer[1024];
		int count = Tvsprintf_s(buffer, 1024, format, arg);
		if (count <= 0)
		{
			TStrcpy_s(buffer, 1024, format);
			count = int(TStrlen(buffer));
		}
		va_end(arg);
		Rpc_WriteLog(buffer, u32(count), false, false);
	}

	UBA_NOINLINE void Rpc_ResolveCallstack(StringBufferBase& out, u32 skipCallstackCount, void* context)
	{
		u32 tryCount = 0;
		bool hasLock = false;
		while (tryCount++ < 5)
		{
			hasLock = g_communicationLock.TryEnter();
			if (hasLock)
				break;
			Sleep(100);
		}

		BinaryWriter writer;
		writer.WriteByte(MessageType_ResolveCallstack);
		auto written = (u32*)writer.AllocWrite(4);
		if (WriteCallstackInfo(writer, skipCallstackCount, context))
		{
			*written = u32(writer.GetPosition()) - 5;
			writer.Flush();
			BinaryReader reader;
			reader.ReadString(out);
		}
		else
		{
			out.Append(TCV("\n   Failed to resolve callstack\n"));
		}
		// Note, we leave the lock even though we might not have it because we want to be able to report
		g_communicationLock.Leave();
	}

	//TODO: Implement SetConsoleTextAttribute.. clang is using it to color errors

	tchar g_consoleString[4096];
	u32 g_consoleStringIndex;

	template<typename CharType>
	void Shared_WriteConsoleT(const CharType* chars, u32 charCount, bool isError)
	{
		if (!g_echoOn || g_suppressLogging)
			return;

		SCOPED_WRITE_LOCK(g_consoleStringCs, lock);
		const CharType* read = chars;
		tchar* write = g_consoleString + g_consoleStringIndex;
		int left = sizeof_array(g_consoleString) - g_consoleStringIndex - 1;
		int available = charCount;
		while (available)
		{
			if (*read == '\n' || !left)
			{
				*write = 0;
				u32 strLen = u32(write - g_consoleString);
				if (!g_rules->SuppressLogLine(g_consoleString, strLen))
					Rpc_WriteLog(g_consoleString, strLen, false, isError);
				write = g_consoleString;
				left = sizeof_array(g_consoleString) - 1;
			}
			else
			{
				*write = *read;
				++write;
			}
			++read;
			--left;
			--available;
		}
		g_consoleStringIndex = u32(write - g_consoleString);
	}

	void Shared_WriteConsole(const char* chars, u32 charCount, bool isError) { Shared_WriteConsoleT(chars, charCount, isError); }

	#if PLATFORM_WINDOWS
	void Shared_WriteConsole(const wchar_t* chars, u32 charCount, bool isError) { Shared_WriteConsoleT(chars, charCount, isError); }
	#endif


	const tchar* Shared_GetFileAttributes(FileAttributes& outAttr, StringView fileName, bool checkIfDir)
	{
		StringBuffer<MaxPath> fileNameForKey;
		fileNameForKey.Append(fileName);
		if (CaseInsensitiveFs)
			fileNameForKey.MakeLower();

		UBA_ASSERT(fileNameForKey.count);
		CHECK_PATH(fileNameForKey);
		StringKey fileNameKey = ToStringKey(fileNameForKey);

		memset(&outAttr.data, 0, sizeof(outAttr.data));

		bool foundMapping = false;
		u64 fileSize = InvalidValue;

		#if PLATFORM_WINDOWS
		if (fileName[1] == ':' && fileName[3] == 0 && (ToLower(fileName[0]) == ToLower(g_virtualWorkingDir[0]) || ToLower(fileName[0]) == g_systemRoot[0]))
		{
			// This is the root of the drive.. let's just return it as a directory
			outAttr.useCache = true;
			outAttr.exists = true;
			outAttr.lastError = ErrorSuccess;
			outAttr.data.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			return fileName.data;
		}
		#endif

		{
			SCOPED_READ_LOCK(g_mappedFileTable.m_lookupLock, lock);
			auto it = g_mappedFileTable.m_lookup.find(fileNameKey);
			if (it != g_mappedFileTable.m_lookup.end())
			{
				FileInfo& fi = it->second;

				if (fi.deleted)
				{
					//if (isInsideSystemTemp)
					//{
					//	outAttr.useCache = false;
					//	return fileName.data;
					//}
					outAttr.useCache = true;
					outAttr.exists = false;
					outAttr.lastError = ErrorFileNotFound;
					return fileName.data;
				}

				// TODO: need to implement
				//outAttr.data.ftLastWriteTime = ;
				//outAttr.volumeSerial =
				//outAttr.fileIndex = 

#if PLATFORM_WINDOWS
				foundMapping = true;
				fileSize = fi.size;
				outAttr.useCache = true;
				outAttr.exists = true;
				outAttr.lastError = ErrorSuccess;
				LARGE_INTEGER li = ToLargeInteger(fileSize);
				outAttr.data.nFileSizeLow = li.LowPart;
				outAttr.data.nFileSizeHigh = li.HighPart;
				outAttr.data.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
				if (fi.created)
					return fileName.data;
				// We can't exit here because we need fileIndex and volume serial among other things.
				// TODO: make sure fi.created is correct on windows everywhere. CopyFile/MoveFile should probably set this in certain conditions
#else
				if (fi.created)
				{
					outAttr.useCache = true;
					outAttr.exists = true;
					outAttr.lastError = ErrorSuccess;
					outAttr.data.st_mode = (mode_t)(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
					outAttr.data.st_size = fi.size;
					return fileName.data;
				}
#endif
			}
		}

		if (!g_allowDirectoryCache || fileName.StartsWith(g_systemTemp.data)) // We need to skip SystemTemp.. lots of stuff going on there.
		{
			if (foundMapping)
				return fileName.data;
			outAttr.useCache = false;
			return fileName.data;
		}

		// This is an optimization where we populate directory table and use that to figure out if file exists or not..
		// .. in msvc's case it doesn't matter much because these tables are already up to date when msvc use CreateFile.
		// .. clang otoh is using CreateFile with tooons of different paths trying to open files.. in remote worker case this becomes super expensive
		u32 dirTableOffset = Rpc_GetEntryOffset(fileNameKey, fileName, checkIfDir);

		if (dirTableOffset == ~u32(0))
		{
			if (foundMapping)
				return fileName.data;
			outAttr.useCache = true;
			outAttr.exists = false;
			outAttr.lastError = ErrorFileNotFound;
			return fileName.data;
		}

		DirectoryTable::EntryInformation info;
		g_directoryTable.GetEntryInformation(info, dirTableOffset);

		if (!info.attributes)
		{
			if (foundMapping)
				return fileName.data;
			// File used to exist but was deleted
			outAttr.useCache = true;
			outAttr.exists = false;
			outAttr.lastError = ErrorFileNotFound;
			return fileName.data;
		}

		if (fileSize == InvalidValue)
			fileSize = info.size;

		// Could be compressed and then directory table size is wrong
		if (CouldBeCompressedFile(fileName))
		{
			// If file is output file we accept wrong size because size is not supposed to be used anyway.
			// We don't want to trigger unnecessary download/decompress of file
			if (!g_rules->IsOutputFile(fileName, g_systemTemp))
			{
				StringBuffer<> temp;
				u32 closeId;
				Rpc_CreateFileW(fileName, fileNameKey, AccessFlag_Read, temp.data, temp.capacity, fileSize, closeId, false);
			}
		}

		outAttr.useCache = true;
		outAttr.exists = true;
		outAttr.lastError = ErrorSuccess;

		UBA_ASSERT(info.fileIndex);
		outAttr.fileIndex = info.fileIndex;
		outAttr.volumeSerial = info.volumeSerial;

#if PLATFORM_WINDOWS
		LARGE_INTEGER li = ToLargeInteger(fileSize);
		outAttr.data.dwFileAttributes = info.attributes;
		outAttr.data.nFileSizeLow = li.LowPart;
		outAttr.data.nFileSizeHigh = li.HighPart;
		(u64&)outAttr.data.ftCreationTime = info.lastWrite;
		(u64&)outAttr.data.ftLastAccessTime = info.lastWrite;
		(u64&)outAttr.data.ftLastWriteTime = info.lastWrite;
#else
		outAttr.data.st_mtimespec = ToTimeSpec(info.lastWrite);
		outAttr.data.st_mode = (mode_t)info.attributes;
		outAttr.data.st_dev = info.volumeSerial;
		outAttr.data.st_ino = info.fileIndex;
		outAttr.data.st_size = fileSize;
#endif

#if 0//UBA_DEBUG_VALIDATE
		if (g_validateFileAccess && !keepInMemory)
		{
			WIN32_FILE_ATTRIBUTE_DATA validate;
			memset(&validate, 0, sizeof(validate));
			SuppressDetourScope _;
			BOOL res = True_GetFileAttributesExW(fileName, GetFileExInfoStandard, &validate); (void)res;
			if (outAttr.exists)
			{
				UBA_ASSERTF(res != 0, L"File %ls exists even though uba claims it is not..", fileName.data);
				if (validate.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					UBA_ASSERTF((outAttr.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), L"File attributes are wrong for %ls", fileName.data);
				else
				{
					validate.ftCreationTime = outAttr.data.ftCreationTime; // Creation time is not really important
					validate.ftLastAccessTime = outAttr.data.ftLastAccessTime; // Access time is not really important
					validate.ftLastWriteTime = outAttr.data.ftLastWriteTime; // Write time is important, revisit this
					UBA_ASSERTF(memcmp(&validate, &outAttr.data, sizeof(WIN32_FILE_ATTRIBUTE_DATA)) == 0, L"File %ls is not up-to-date in cache", fileName.data);
				}
			}
			else
			{
				UBA_ASSERTF(res == 0, L"Can't find file %ls but validation checked that it is there", fileName.data); // This means most likely that Uba did not update attribute table for added files.
				DWORD lastError2 = GetLastError();
				if (lastError2 == ERROR_PATH_NOT_FOUND || lastError2 == ERROR_INVALID_NAME)
					lastError2 = ERROR_FILE_NOT_FOUND;
				UBA_ASSERT(outAttr.lastError == lastError2);
			}
		}
#endif
		return fileName.data;
	}
}

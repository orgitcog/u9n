// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaPlatform.h"
#include "UbaProcessStats.h"
#include <oodle2.h>

#if PLATFORM_WINDOWS
#include <conio.h>
#include <psapi.h>
#else
#include <execinfo.h>
#include <limits.h>
#include <dlfcn.h>
#if PLATFORM_LINUX
#include <link.h>
#else
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#endif
#endif


namespace uba
{
	KernelStats g_kernelStatsInternal;
	thread_local KernelStats* t_kernelStats;

	KernelStats& KernelStats::GetCurrent()
	{
		KernelStats* stats = t_kernelStats;
		return stats ? *stats : g_kernelStatsInternal;
	}

	KernelStats& KernelStats::GetGlobal()
	{
		return g_kernelStatsInternal;
	}

	KernelStatsScope::KernelStatsScope(KernelStats& s) : stats(s)
	{
		t_kernelStats = &stats;
	}
	KernelStatsScope::~KernelStatsScope()
	{
		t_kernelStats = nullptr;
	}

	bool CreateGuid(Guid& out)
	{
		#if PLATFORM_WINDOWS
		return ::CoCreateGuid((GUID*)&out) == S_OK;
		#elif PLATFORM_MAC
		arc4random_buf(&out, 16);
		return true;
		#else
		const int f = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
		if (f == -1)
			return false;
		size_t bytesRead = read(f, &out, 16);
		close(f);
		return bytesRead == 16;
		//syscall(SYS_getrandom, &out, sizeof(Guid), 0x0001);
		#endif
	}

	const char* GetWineVersion()
	{
		#if PLATFORM_WINDOWS
		static const char* wineVersion = []() -> const char*
			{
				if (HMODULE ntDllModule = GetModuleHandleW(L"ntdll.dll"))
				{
					using wine_get_version_func = const char*();
					if (auto wine_get_version = (wine_get_version_func*)GetProcAddress(ntDllModule, "wine_get_version"))
						return wine_get_version();
				}
				return nullptr;
			}();
		return wineVersion;
		#else
		return nullptr;
		#endif
	}

	bool IsRunningWine()
	{
		return GetWineVersion() != nullptr;
	}

	bool IsRunningArm()
	{
		#if PLATFORM_WINDOWS
		static bool isArm = []()
		{
			USHORT processMachine, nativeMachine;
			if (!IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine))
				return false;
			return nativeMachine == IMAGE_FILE_MACHINE_ARM64;
		}();
		return isArm;
		#else
		return IsArmBinary;
		#endif
	}

	void Sleep(u32 milliseconds)
	{
		#if PLATFORM_WINDOWS
		::Sleep(milliseconds);
		#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
		struct timespec ts;
		ts.tv_sec = milliseconds / 1000;
		ts.tv_nsec = (milliseconds % 1000) * 1000000;
		int res;
		do { res = nanosleep(&ts, &ts); } while (res && errno == EINTR);
		#else
		if (milliseconds >= 1000)
			sleep(milliseconds / 1000);
		usleep((milliseconds % 1000) * 1000);
		#endif
	}

	u32 GetUserDefaultUILanguage()
	{
		#if PLATFORM_WINDOWS
		return ::GetUserDefaultUILanguage();
		#else
		return 1;
		#endif
	}

	thread_local u32 t_lastError;

	u32 GetLastError()
	{
		#if PLATFORM_WINDOWS
		return ::GetLastError();
		#else
		return t_lastError;
		#endif
	}

	void SetLastError(u32 error)
	{
		#if PLATFORM_WINDOWS
		::SetLastError(error);
		#else
		t_lastError = error;
		#endif
	}

	bool GetComputerNameW(StringBufferBase& out)
	{
		#if PLATFORM_WINDOWS
		DWORD size = out.capacity;
		if (!::GetComputerNameExW(ComputerNameDnsHostname, out.data, &size))
			if (!::GetComputerNameExW(ComputerNameNetBIOS, out.data, &size))
				return false;
		out.count = size;
		#else
		if (gethostname(out.data, out.capacity) == -1)
			return false;
		out.count = TStrlen(out.data);
		#endif
		return true;
	}

	bool GetOsVersion(StringBufferBase& outPretty, u32& outValue)
	{
		outValue = 0;

#if PLATFORM_WINDOWS
		if (IsRunningWine())
			outPretty.Append(TC("Linux/Wine-")).Append(GetWineVersion());
		else
			outPretty.Append(TC("Windows"));
#elif PLATFORM_MACOS
		FILE* osRelease = popen("sw_vers -productVersion", "r");
		if (!osRelease)
			return false;
		char buffer[256];
		bool success = fgets(buffer, sizeof_array(buffer), osRelease);
		pclose(osRelease);
		if (!success)
			return false;
		u32 bufferLen = TStrlen(buffer) - 1;
		buffer[bufferLen] = 0;
		outPretty.Append(TCV("MacOS-")).Append(buffer, bufferLen);

		u32 majorVersion = 0;
		u32 minorVersion = 0;
		u32 patchVersion = 0;

		if (char* endOfMajor = TStrchr(buffer, '.'))
		{
			*endOfMajor = 0;
			char* minor = endOfMajor + 1;
			if (char* endOfMinor = TStrchr(minor, '.'))
			{
				*endOfMinor = 0;
				char* patch = endOfMinor + 1;
				if (*patch)
					patchVersion = strtoul(patch, nullptr, 10);
			}
			if (*minor)
				minorVersion = strtoul(minor, nullptr, 10);
		}
		majorVersion = strtoul(buffer, nullptr, 10);

		outValue = (majorVersion << 16) + (minorVersion << 8) + patchVersion;
#elif PLATFORM_LINUX
		FILE* osRelease = fopen("/etc/os-release", "r");
		if (!osRelease)
			return false;
		char buffer[256];
		bool success = fgets(buffer, sizeof_array(buffer), osRelease);
		fclose(osRelease);
		if (!success)
			return false;
		if (!StartsWith(buffer, "PRETTY_NAME=\""))
			return false;
		outPretty.Append(buffer + 13, TStrlen(buffer) - 14);
#endif
		return true;
	}

	void WriteAssertInfo(StringBufferBase& out, const tchar* text, const char* file, u32 line, const char* expr, void* context)
	{
		int signal = 0;
		#if !PLATFORM_WINDOWS
		if (context)
			signal = *(int*)context;
		#endif

		if (!context)
			out.Append(TCV("ASSERT: "));
		out.Append(*text ? text : TC("Unknown"));
		if (!*text && expr && *expr && strcmp(expr, "false") != 0)
			out.Appendf(TC("\n EXPR: ")).Append(expr);
		if (file && *file)
			out.Append(TCV("\n LOCATION: ")).Append(file).Append(':').AppendValue(line);
		if (signal)
			out.Append(TCV("\n SIGNAL: ")).AppendValue(signal);
	}

	#if defined(_M_ARM64)
		#define GET_PC(context) context.Pc
		#define GET_SP(context) context.Sp
	#else
		#define GET_PC(context) context.Rip
		#define GET_SP(context) context.Rsp
	#endif

	UBA_NOINLINE u32 GetCallstack(void** outCallstack, u32 callstackCapacity, u32 skipCallstack, void* contextPtr)
	{
#if PLATFORM_WINDOWS
		constexpr int MaxDepth = 16;// 62;
		void* callers[MaxDepth] = {0};
		u32 count = 0;//RtlCaptureStackBackTrace(0, MaxDepth, callers, NULL); // For some reason this does not work on wine
		if (!count)
		{
			CONTEXT context;
			if (contextPtr)
			{
				context = *(CONTEXT*)contextPtr;
				skipCallstack = 0;
			}
			else
				RtlCaptureContext(&context);

			if (GET_PC(context) && GET_SP(context))
			{
				UNWIND_HISTORY_TABLE unwindHistoryTable;
				RtlZeroMemory(&unwindHistoryTable, sizeof(UNWIND_HISTORY_TABLE));
				while (count < MaxDepth && GET_PC(context))
				{
					callers[count++] = (void*)GET_PC(context);
					ULONG64 ImageBase = 0;
					if (PRUNTIME_FUNCTION RuntimeFunction = RtlLookupFunctionEntry(GET_PC(context), &ImageBase, &unwindHistoryTable); RuntimeFunction)
					{
						KNONVOLATILE_CONTEXT_POINTERS nvcontext;
						RtlZeroMemory(&nvcontext, sizeof(KNONVOLATILE_CONTEXT_POINTERS));
						PVOID HandlerData = nullptr;
						ULONG64 EstablisherFrame = 0;
						RtlVirtualUnwind(0, ImageBase, GET_PC(context), RuntimeFunction, &context, &HandlerData, &EstablisherFrame, &nvcontext);
					}
					else
					{
						u64* spPtr = (u64*)GET_SP(context);
						if (!spPtr)
							break;
						u64 sp = *spPtr;
						if (!sp)
							break;
						GET_PC(context) = sp;
						GET_SP(context) += 8;
					}
				}
			}
		}
#else
		constexpr u32 maxCallers = 64;
		void* callers[maxCallers];
		u32 count = backtrace(callers, maxCallers);
		if (contextPtr) // Use this to know if we come from a signal
			skipCallstack += 2;
#endif
		if (count < skipCallstack)
			skipCallstack = count;
		u32 returnCount = count - skipCallstack;
		if (callstackCapacity < returnCount)
		{
			skipCallstack += returnCount - callstackCapacity;
			returnCount = count - skipCallstack;
		}

		memcpy(outCallstack, callers + skipCallstack, returnCount*sizeof(void*));
		return returnCount;
	}

	UBA_NOINLINE bool WriteCallstackInfo(BinaryWriter& out, u32 skipCallstack, void* contextPtr)
	{
		void* callstack[100];
		u32 callstackCount = GetCallstack(callstack, 100, skipCallstack, contextPtr);
		return WriteCallstackInfo(out, callstack, callstackCount);
	}

	bool WriteCallstackInfo(BinaryWriter& out, void** callstack, u32 callstackCount)
	{
#if PLATFORM_WINDOWS
		out.WriteBool(IsRunningWine());

		struct ModuleRec { u64 start; u64 size; HMODULE handle; u32 index; };
		Map<u64, ModuleRec> moduleEndAddresses;

		HMODULE loadedModules[512];
		DWORD needed = 0;
		if (EnumProcessModules(GetCurrentProcess(), loadedModules, sizeof(loadedModules), &needed))
		{
			for (u32 i = 0, e = needed / sizeof(HMODULE); i != e; ++i)
			{
				MODULEINFO mi;
				GetModuleInformation(GetCurrentProcess(), loadedModules[i], &mi, sizeof(mi));
				ModuleRec& rec = moduleEndAddresses[u64(mi.lpBaseOfDll) + mi.SizeOfImage];
				rec.handle = loadedModules[i];
				rec.start = u64(mi.lpBaseOfDll);
				rec.size = mi.SizeOfImage;
				rec.index = ~0u;
			}
		}

		Vector<ModuleRec*> usedModules;

		out.Write7BitEncoded(callstackCount);

		for (u32 i = 0; i < callstackCount; i++)
		{
			auto addr = u64(callstack[i]);

			auto findIt = moduleEndAddresses.lower_bound(addr);
			if (findIt != moduleEndAddresses.end() && addr >= findIt->second.start)
			{
				ModuleRec& rec = findIt->second;
				if (rec.index == ~0u)
				{
					rec.index = u32(usedModules.size());
					usedModules.push_back(&rec);
				}
				out.Write7BitEncoded(rec.index);
				out.Write7BitEncoded(addr - rec.start);
			}
			else
			{
				out.Write7BitEncoded(~0u);
				out.Write7BitEncoded(addr);
			}
		}

		out.Write7BitEncoded(usedModules.size());
		for (auto* rec : usedModules)
		{
			out.Write7BitEncoded(rec->start);
			out.Write7BitEncoded(rec->size);
			tchar str[1024];
			if (GetModuleFileNameW(rec->handle, str, sizeof_array(str)))
			{
				const tchar* moduleName = str;
				if (const tchar* lastSeparator = TStrrchr(str, PathSeparator))
					moduleName = lastSeparator + 1;
				out.WriteString(moduleName);
			}
			else
			{
				out.WriteString(TC(""));
			}
		}
		return true;
#else

		struct ModuleRec { u64 start; u64 size; TString name; u32 index; };
		Map<u64, ModuleRec> moduleEndAddresses;

		#if PLATFORM_LINUX
		dl_iterate_phdr([](struct dl_phdr_info* info, size_t size, void* data)
			{
				auto& moduleEndAddresses = *(Map<u64, ModuleRec>*)data;
				if (!info->dlpi_name)
					return 0;
				u64 base = info->dlpi_addr;
				u64 end = base;
				for (int i = 0; i < info->dlpi_phnum; i++)
				{
					if (info->dlpi_phdr[i].p_type != PT_LOAD)
						continue;
					u64 start = base + info->dlpi_phdr[i].p_vaddr;
					end = Max(end, start + info->dlpi_phdr[i].p_memsz);
				}

				ModuleRec& rec = moduleEndAddresses[end];
				rec.start = base;
				rec.size = end - base;
				rec.name = info->dlpi_name;
				rec.index = ~0u;
				return 0;
			}, &moduleEndAddresses);
		#else
		{
			Map<u64, ModuleRec> modules;
			for (u32 i = 0, e = _dyld_image_count(); i < e; i++)
			{
				const char* name = _dyld_get_image_name(i);
				if (!name)
					continue;
				auto hdr = _dyld_get_image_header(i);
				u64 start = u64(hdr);// + _dyld_get_image_vmaddr_slide(i);
				ModuleRec& rec = modules[start];
				rec.start = start;
				rec.size = 0;
				rec.name = name;
				rec.index = ~0u;
			}
			ModuleRec* prev = nullptr;
			for (auto& kv : modules)
			{
				if (prev)
				{
					u64 end = kv.second.start;
					auto& rec = moduleEndAddresses[end] = *prev;
					rec.size = end - prev->start;
				}
				prev = &kv.second;
			}
		}
		#endif

		out.Write7BitEncoded(callstackCount);

		Vector<ModuleRec*> usedModules;

		for (int i = 0; i < callstackCount; ++i)
		{
			u64 addr = u64(callstack[i]);
			auto findIt = moduleEndAddresses.lower_bound(addr);
			if (findIt != moduleEndAddresses.end() && addr >= findIt->second.start)
			{
				ModuleRec& rec = findIt->second;
				if (rec.index == ~0u)
				{
					rec.index = u32(usedModules.size());
					usedModules.push_back(&rec);
				}
				out.Write7BitEncoded(rec.index);
				out.Write7BitEncoded(addr - rec.start);
			}
			else
			{
				out.Write7BitEncoded(~0u);
				out.Write7BitEncoded(addr);
			}
		}

		out.Write7BitEncoded(usedModules.size());
		for (auto* rec : usedModules)
		{
			out.Write7BitEncoded(rec->start);
			out.Write7BitEncoded(rec->size);
			out.WriteString(rec->name);
		}
		return true;
#endif
	}

	bool IsEscapePressed()
	{
		#if PLATFORM_WINDOWS
		return _kbhit() && _getch() == 27;
		#else
		return false;
		#endif
	}

	u32 GetCurrentProcessId()
	{
		#if PLATFORM_WINDOWS
		return ::GetCurrentProcessId();
		#else
		return getpid();
		#endif
	}

	MutexHandle CreateMutexW(bool bInitialOwner, const tchar* lpName)
	{
		#if PLATFORM_WINDOWS
		return (MutexHandle)(u64)::CreateMutexW(NULL, bInitialOwner, lpName);
		#else
		// TODO: This is used to check for exclusivity and also for trace streams (only created by host and read by visualizer)
		SetLastError(ERROR_SUCCESS);
		return ((MutexHandle)(u64)1337); // Just some random value
		#endif
	}

	void ReleaseMutex(MutexHandle mutex)
	{
		if (mutex == InvalidMutexHandle)
			return;
		#if PLATFORM_WINDOWS
		::ReleaseMutex((HANDLE)mutex);
		#else
		#endif
	}

	void CloseMutex(MutexHandle mutex)
	{
		if (mutex == InvalidMutexHandle)
			return;
		#if PLATFORM_WINDOWS
		::CloseHandle((HANDLE)mutex);
		#else
		#endif
	}

	u32 GetEnvironmentVariableW(const tchar* name, tchar* buffer, u32 nSize)
	{
		#if PLATFORM_WINDOWS
		return ::GetEnvironmentVariableW(name, buffer, nSize);
		#else
		const char* env = getenv(name);
		if (!env)
		{
			SetLastError(203); // ERROR_ENVVAR_NOT_FOUND
			return 0;
		}

		auto envLen = strlen(env);
		if (nSize <= envLen)
			return envLen + 1;
		memcpy(buffer, env, envLen + 1);
		return envLen;
		#endif
	}

	bool SetEnvironmentVariableW(const tchar* name, const tchar* value)
	{
		#if PLATFORM_WINDOWS
		return ::SetEnvironmentVariableW(name, value);
		#else
		return setenv(name, value, 1) == 0;
		#endif
	}

	u32 ExpandEnvironmentStringsW(const tchar* lpSrc, tchar* lpDst, u32 nSize)
	{
		#if PLATFORM_WINDOWS
		return ::ExpandEnvironmentStringsW(lpSrc, lpDst, nSize);
		#else
		UBA_ASSERTF(false, TC("ExpandEnvironmentStringsW not implemented (%s)"), lpSrc);
		return 0;
		#endif
	}

	ProcHandle GetCurrentProcessHandle()
	{
		#if PLATFORM_WINDOWS
		return (ProcHandle)(u64)GetCurrentProcess();
		#else
		UBA_ASSERTF(false, TC("GetCurrentProcessHandle not implemented"));
		return (ProcHandle)0;
		#endif
	}

	u32 GetLogicalProcessorCount()
	{
		#if PLATFORM_WINDOWS
		return ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		#else
		return (u32)sysconf(_SC_NPROCESSORS_ONLN);
		#endif
	}

	u32 GetProcessorGroupCount()
	{
		#if PLATFORM_WINDOWS
		static u32 s_processorGroupCount = u32(GetActiveProcessorGroupCount());
		if (s_processorGroupCount)
			return s_processorGroupCount;
		#endif
		return 1u;
	}

	void ElevateCurrentThreadPriority()
	{
		#if PLATFORM_WINDOWS
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		#endif
	}

	void PrefetchVirtualMemory(const void* mem, u64 size)
	{
		#if PLATFORM_WINDOWS
		WIN32_MEMORY_RANGE_ENTRY entry;
		entry.VirtualAddress = const_cast<void*>(mem);
		entry.NumberOfBytes = size;
		PrefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
		#endif
	}

#if PLATFORM_WINDOWS
	void* g_startOfCurrentLibrary;
	void* g_endOfCurrentLibrary;
	bool g_reportAllExceptions;
	LONG UbaExceptionHandler(_EXCEPTION_POINTERS* ExceptionInfo)
	{
		auto& record = *ExceptionInfo->ExceptionRecord;
		u32 exceptionCode = record.ExceptionCode;

		if (!g_reportAllExceptions)
		{
			if (exceptionCode != EXCEPTION_STACK_OVERFLOW && exceptionCode != EXCEPTION_ACCESS_VIOLATION)
				return EXCEPTION_CONTINUE_SEARCH;

			if (!g_startOfCurrentLibrary)
			{
				HMODULE currentModule;
				GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&g_startOfCurrentLibrary, &currentModule);
				MODULEINFO mi;
				GetModuleInformation(GetCurrentProcess(), currentModule, &mi, sizeof(mi));
				g_startOfCurrentLibrary = mi.lpBaseOfDll;
				g_endOfCurrentLibrary = (u8*)mi.lpBaseOfDll + mi.SizeOfImage;
			}

			if (record.ExceptionAddress < g_startOfCurrentLibrary || record.ExceptionAddress >= g_endOfCurrentLibrary)
				return EXCEPTION_CONTINUE_SEARCH;
		}
		CONTEXT* contextPtr = ExceptionInfo->ContextRecord;

		StringBuffer<> text;
		if (exceptionCode == EXCEPTION_ACCESS_VIOLATION)
			text.Appendf(L"ERROR: Access violation %s at address: 0x%p", (record.ExceptionInformation[0] == 1 ? L"writing" : L"reading"), (void*)record.ExceptionInformation[1]);
		else
			text.Appendf(L"ERROR: Unhandled Exception (Code: 0x%x)", exceptionCode);

		UbaAssert(text.data, nullptr, 0, nullptr, false, exceptionCode, contextPtr, 0);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	DWORD UbaExceptionHandlerAllowAccessViolation(DWORD code, _EXCEPTION_POINTERS* info, const tchar* fileName)
	{
		if (code != EXCEPTION_ACCESS_VIOLATION)
			return EXCEPTION_CONTINUE_SEARCH;
		StringBuffer<> buf;
		buf.Appendf(L"ERROR: Access violation reading %s", fileName);
		UbaAssert(buf.data, __FILE__, __LINE__, "", true, code, info->ContextRecord, 0);
		return EXCEPTION_EXECUTE_HANDLER;
	}

#else
	static const int g_allSignals[] = {
		//#ifdef SIGHUP
		//SIGHUP,
		//#endif
		//#ifdef SIGQUIT
		//SIGQUIT,
		//#endif
		//#ifdef SIGTRAP
		//SIGTRAP,
		//#endif
		//#ifdef SIGIO
		//SIGIO,
		//#endif
		//SIGABRT,
		//SIGFPE,
		SIGILL,
		//SIGINT,
		SIGSEGV,
		//SIGTERM 
	};
	static void segfault_sigaction(int signal)
	{
		const char* desc = signal == SIGSEGV ? "ERROR: Segmentation fault" : "ERROR: Unhandled signal";
		uba::UbaAssert(desc, "", 0, "", true, -1, &signal, 0);
	}
#endif

	void AddExceptionHandler()
	{
#if PLATFORM_WINDOWS
		::AddVectoredExceptionHandler(TRUE, UbaExceptionHandler);
#else
		struct sigaction sa;
		memset(&sa, 0, sizeof(struct sigaction));
		sigemptyset(&sa.sa_mask);
		sa.sa_handler = segfault_sigaction;
		sa.sa_flags = 0;// SA_SIGINFO;
		for (auto signal : g_allSignals)
			sigaction(signal, &sa, NULL);
#endif
	}

	#if UBA_USE_MIMALLOC
	void* Oodle_MallocAligned(OO_SINTa bytes, OO_S32 alignment)
	{
		return mi_malloc_aligned(bytes, alignment);
	}

	void Oodle_Free(void* ptr)
	{
		mi_free(ptr);
	}
	#endif

	void InitMemory()
	{
		#if UBA_USE_MIMALLOC
		mi_option_set(mi_option_purge_delay, 100);
		//mi_option_set(mi_option_eager_commit, 1);
		//mi_option_set(mi_option_purge_decommits, 0);
		OodleCore_Plugins_SetAllocators(Oodle_MallocAligned, Oodle_Free);
		#endif
	}


#if !PLATFORM_WINDOWS
	void strcpy_s(tchar* dest, u64 destCapacity, const tchar* source)
	{
		u64 toCopy = Min(destCapacity-1, u64(strlen(source)));
		memcpy(dest, source, toCopy);
		dest[toCopy] = 0;
	}

	void strcat_s(tchar* dest, u64 destCapacity, const tchar* source)
	{
		u64 len = strlen(dest);
		strcpy_s(dest + len, destCapacity - len, source);
	}

	void GetMappingHandleName(StringBufferBase& out, u64 uid)
	{
		#if PLATFORM_MAC
		out.Append("/tmp/uba_").AppendHex(uid);
		#else
		out.Append("/uba_").AppendHex(uid);
		#endif
	}

	bool GetPhysicallyInstalledSystemMemory(u64& outKb)
	{
	#if PLATFORM_MAC
		static long mem = []() -> int64_t
		{
			int64_t mem;
			size_t len = sizeof(mem);
			int mib[] = {CTL_HW, HW_MEMSIZE};
			if (sysctl(mib, 2, &mem, &len, NULL, 0) == 0)
				return mem;
			return 0;
		}();
	#else
		static long mem = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
	#endif
		outKb = mem/(1024*1024);
		return true;
	}

#endif

	void BinaryWriter::WriteLongString(const StringView& str, u32 threshold)
	{
		if (str.count < threshold)
		{
			WriteByte(0);
			WriteString(str);
			return;
		}

		WriteByte(sizeof(tchar));
		u32 uncompressedSize = str.count * sizeof(tchar);
		u64 needed = (u64)OodleLZ_GetCompressedBufferSizeNeeded(OodleLZ_Compressor_Kraken, uncompressedSize);
		Vector<u8> mem;
		mem.resize(needed);
		u64 compressedSize = OodleLZ_Compress(OodleLZ_Compressor_Kraken, str.data, (OO_SINTa)uncompressedSize, mem.data(), OodleLZ_CompressionLevel_Normal);
		UBA_ASSERT(compressedSize != OODLELZ_FAILED);
		Write7BitEncoded(str.count);
		Write7BitEncoded(compressedSize);
		WriteBytes(mem.data(), compressedSize);
	}

	TString BinaryReader::ReadLongString()
	{
		u8 s = ReadByte();
		if (!s)
			return ReadString();

		u64 stringLength = Read7BitEncoded();
		u64 uncompressedSize = stringLength * s;
		u64 compressedSize = Read7BitEncoded();

		const u8* data = GetPositionData();
		Skip(compressedSize);

		if (s == sizeof(tchar))
		{
			TString str;
			str.resize(stringLength);
			OO_SINTa decompLen = OodleLZ_Decompress(data, (OO_SINTa)compressedSize, str.data(), (OO_SINTa)uncompressedSize);
			UBA_ASSERT(decompLen == (OO_SINTa)uncompressedSize);(void)decompLen;
			return str;
		}
		else
		{
			UBA_ASSERT(s == 1);
			std::string temp;
			temp.resize(stringLength);
			OO_SINTa decompLen = OodleLZ_Decompress(data, (OO_SINTa)compressedSize, temp.data(), (OO_SINTa)uncompressedSize);
			UBA_ASSERT(decompLen == (OO_SINTa)uncompressedSize);(void)decompLen;
			return TString(temp.begin(), temp.end());
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTrace.h"
#include "UbaFileAccessor.h"
#include "UbaProcessHandle.h"

#if PLATFORM_WINDOWS
#include <tlhelp32.h>
#include <psapi.h>
#endif

namespace uba
{
	constexpr u64 TraceMessageMaxSize = 256 * 1024;

	Trace::Trace(LogWriter& logWriter)
	:	m_logger(logWriter)
	,	m_channel(m_logger)
	{
	}

	Trace::~Trace()
	{
		FreeMemory();
	}

	struct Trace::WriterScope : ScopedFutex, BinaryWriter
	{
		WriterScope(Trace& trace) : ScopedFutex(trace.m_memoryLock), BinaryWriter(trace.m_memoryBegin, trace.m_memoryPos, trace.m_memoryCapacity), m_trace(trace)
		{
			EnsureMemory(TraceMessageMaxSize);
		}

		~WriterScope()
		{
			if (!m_isValid)
				return;
			m_trace.m_memoryPos = GetPosition();
			*(u32*)m_trace.m_memoryBegin = u32(m_trace.m_memoryPos);
		}

		bool IsValid() { return m_isValid; }

		WriterScope(const WriterScope&) = delete;
		void operator=(const WriterScope&) = delete;

		bool EnsureMemory(u64 size)
		{
			if (!m_isValid)
				return false;
			m_trace.m_memoryPos = GetPosition();
			m_isValid = m_trace.EnsureMemory(size);
			return m_isValid;
		}

		Trace& m_trace;
		bool m_isValid = true;
	};

	bool Trace::StartWrite(const tchar* namedTrace, u64 traceMemCapacity)
	{
		m_memoryCapacity = traceMemCapacity;
		m_memoryHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE|SEC_RESERVE, m_memoryCapacity, namedTrace, TC("Trace"));
		if (!m_memoryHandle.IsValid())
			return false;
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			m_memoryBegin = MapViewOfFile(m_logger, m_memoryHandle, FILE_MAP_WRITE, 0, m_memoryCapacity);

		if (!m_memoryBegin)
		{
			m_logger.Warning(TC("Failed to map view of trace mapping '%s' (%s)"), namedTrace, LastErrorToText().data);
			CloseFileMapping(m_logger, m_memoryHandle, TC("Trace"));
			m_memoryHandle = {};
			return false;
		}

		m_memoryPos = 0;
		m_startTime = GetTime();
		u64 systemStartTimeUs = GetSystemTimeUs();

		{
			WriterScope writer(*this);
			if (!writer.IsValid())
				return false;
			writer.AllocWrite(4);
			writer.WriteU32(TraceVersion);
			writer.WriteU32(GetCurrentProcessId());
			writer.Write7BitEncoded(systemStartTimeUs);
			writer.Write7BitEncoded(GetFrequency());
			writer.Write7BitEncoded(m_startTime);
		}


		if (namedTrace && m_channel.Init())
		{
			m_namedTrace = namedTrace;
			m_channel.Write(namedTrace);
		}
		return true;
	}

	bool Trace::Write(const tchar* writeFileName, bool writeSummary)
	{
		if (!m_memoryBegin)
			return true;

		if (!writeFileName || !*writeFileName)
			return true;

		FileAccessor traceFile(m_logger, writeFileName);
		if (!traceFile.CreateWrite(false, DefaultAttributes(), 0, nullptr))
			return false;

		ScopedFutex lock(m_memoryLock);
		u64 fileSize = m_memoryPos;
		if (!traceFile.Write(m_memoryBegin, fileSize))
			return false;
		lock.Leave();

		if (writeSummary)
		{
			StackBinaryWriter<32> summaryWriter;
			summaryWriter.WriteByte(TraceType_Summary);
			summaryWriter.Write7BitEncoded(GetTime() - m_startTime);
			if (!traceFile.Write(summaryWriter.GetData(), summaryWriter.GetPosition()))
				return false;
			fileSize += summaryWriter.GetPosition();
		}

		if (!traceFile.Close())
			return false;
		m_logger.Info(TC("Trace written to file %s with size %s"), writeFileName, BytesToText(fileSize).str);
		return true;
	}

	bool Trace::StopWrite(const tchar* writeFileName)
	{
		if (!m_memoryBegin)
			return true;
		auto g = MakeGuard([this]() { FreeMemory(); });

		if (!m_namedTrace.empty())
			m_channel.Write(TC(""), m_namedTrace.c_str());

		{
			WriterScope writer(*this);
			if (!writer.IsValid())
				return false;
			writer.WriteByte(TraceType_Summary);
			writer.Write7BitEncoded(GetTime() - m_startTime);
		}

		return Write(writeFileName, false); // Don't write summary, already included in memory
	}

	u32 Trace::TrackWorkStart(const StringView& desc, const Color& color)
	{
		u32 workId = m_workCounter++;
		WorkBegin(workId, desc, color);
		return workId;
	}

	void Trace::TrackWorkHint(u32 id, const StringView& hint, u64 startTime)
	{
		WorkHint(id, hint, startTime);
	}

	void Trace::TrackWorkEnd(u32 id)
	{
		WorkEnd(id);
	}

	void Trace::FreeMemory()
	{
		if (m_memoryBegin)
		{
			UnmapViewOfFile(m_logger, m_memoryBegin, m_memoryCapacity, TC("Trace"));
			m_memoryBegin = nullptr;
		}
		if (m_memoryHandle.IsValid())
		{
			CloseFileMapping(m_logger, m_memoryHandle, TC("Trace"));
			m_memoryHandle = {};
		}
	}

	bool Trace::EnsureMemory(u64 size)
	{
		if (!m_memoryBegin)
			return false;

		u64 committedMemoryNeeded = AlignUp(m_memoryPos + size, 64*1024);
		if (m_memoryCommitted >= committedMemoryNeeded)
			return true;

		if (MapViewCommit(m_memoryBegin + m_memoryCommitted, committedMemoryNeeded - m_memoryCommitted))
		{
			m_memoryCommitted = committedMemoryNeeded;
			return true;
		}

		FreeMemory();
		return m_logger.Warning(TC("Failed to commit memory for trace (Pos: %llu Capacity: %llu, Already Committed: %llu, Needed: %llu): %s"), m_memoryPos, m_memoryCapacity, m_memoryCommitted, committedMemoryNeeded, LastErrorToText().data);
	}


	u32 Trace::AddString(const StringView& string)
	{
		if (!m_memoryBegin)
			return 0;

		SCOPED_FUTEX(m_stringsLock, lock);
		auto insres = m_strings.try_emplace(ToStringKeyNoCheck(string.data, string.count));
		if (insres.second)
		{
			insres.first->second = u32(m_strings.size() - 1);
			WriterScope writer(*this);
			if (!writer.IsValid())
				return 0;
			writer.WriteByte(TraceType_String);
			writer.WriteString(string);
		}
		return insres.first->second;
	}

	#define BEGIN_TRACE_ENTRY(x) \
		BEGIN_TRACE_ENTRY_IMPL(x, true)

	#define BEGIN_TRACE_ENTRY_NOTIME(x) \
		BEGIN_TRACE_ENTRY_IMPL(x, false)

	#define BEGIN_TRACE_ENTRY_IMPL(x, writeTime) \
		if (!m_memoryBegin) \
			return; \
		u64 time = GetTime() - m_startTime; \
		WriterScope writer(*this); \
		if (!writer.IsValid()) \
			return; \
		writer.WriteByte(TraceType_##x); \
		if (writeTime) \
			writer.Write7BitEncoded(time);

	void Trace::SessionAdded(u32 sessionId, u32 clientId, const StringView& name, const StringView& info)
	{
		BEGIN_TRACE_ENTRY(SessionAdded);
		writer.WriteString(name);
		writer.WriteString(info);
		writer.Write7BitEncoded(clientId);
		writer.WriteU32(sessionId);
	}

	void Trace::SessionUpdate(u32 sessionId, u32 connectionCount, u64 send, u64 recv, u64 lastPing, u64 memAvail, u64 memTotal, float cpuLoad)
	{
		BEGIN_TRACE_ENTRY(SessionUpdate);
		writer.Write7BitEncoded(sessionId);
		writer.Write7BitEncoded(connectionCount);
		writer.Write7BitEncoded(send);
		writer.Write7BitEncoded(recv);
		writer.Write7BitEncoded(lastPing);
		writer.Write7BitEncoded(memAvail);
		writer.Write7BitEncoded(memTotal);
		writer.WriteU32(*(u32*)&cpuLoad);
	}

	void Trace::SessionNotification(u32 sessionId, const tchar* text)
	{
		BEGIN_TRACE_ENTRY(SessionNotification);
		writer.WriteU32(sessionId);
		writer.WriteString(text);
	}

	void Trace::SessionSummary(u32 sessionId, const u8* data, u64 dataSize)
	{
		BEGIN_TRACE_ENTRY(SessionSummary);
		writer.WriteU32(sessionId);
		writer.WriteBytes(data, dataSize);
	}

	void Trace::SessionDisconnect(u32 sessionId)
	{
		BEGIN_TRACE_ENTRY(SessionDisconnect);
		writer.WriteU32(sessionId);
	}

	void Trace::ProcessAdded(u32 sessionId, u32 processId, const StringView& description, const StringView& breadcrumbs)
	{
		BEGIN_TRACE_ENTRY(ProcessAdded);
		writer.WriteU32(sessionId);
		writer.WriteU32(processId);
		writer.WriteString(description);
		writer.WriteLongString(breadcrumbs, 20);
	}

	void Trace::ProcessEnvironmentUpdated(u32 processId, const StringView& reason, const u8* data, u64 dataSize, const StringView& breadcrumbs)
	{
		BEGIN_TRACE_ENTRY(ProcessEnvironmentUpdated);
		writer.WriteU32(processId);
		writer.WriteString(reason);
		writer.WriteBytes(data, dataSize);
		writer.WriteLongString(breadcrumbs, 20);
	}

	void Trace::ProcessExited(u32 processId, u32 exitCode, const u8* data, u64 dataSize, const Vector<ProcessLogLine>& logLines)
	{
		BEGIN_TRACE_ENTRY(ProcessExited);
		writer.WriteU32(processId);
		writer.WriteU32(exitCode);
		writer.WriteBytes(data, dataSize);

		u32 lineCounter = 0;
		for (auto& line : logLines)
		{
			if (lineCounter++ == 100) // We don't want to write the entire error in the trace stream to blow the entire buffer
				break;
			if (!writer.EnsureMemory(2 + (line.text.size()+2)*sizeof(tchar)))
				return;
			writer.WriteByte(line.type);
			writer.WriteString(line.text);
		}
		writer.WriteByte(255);
	}

	void Trace::ProcessReturned(u32 processId, const StringView& reason)
	{
		BEGIN_TRACE_ENTRY(ProcessReturned);
		writer.WriteU32(processId);
		writer.WriteString(reason);
	}

	void Trace::ProcessAddBreadcrumbs(u32 processId, const StringView& breadcrumbs, bool deleteOld)
	{
		BEGIN_TRACE_ENTRY(ProcessBreadcrumbs);
		writer.WriteU32(processId);
		writer.WriteLongString(breadcrumbs, 20);
		writer.WriteBool(deleteOld);
	}

	void Trace::ProxyCreated(u32 clientId, const tchar* proxyName)
	{
		BEGIN_TRACE_ENTRY(ProxyCreated);
		writer.Write7BitEncoded(clientId);
		writer.WriteString(proxyName);
	}

	void Trace::ProxyUsed(u32 clientId, const tchar* proxyName)
	{
		BEGIN_TRACE_ENTRY(ProxyUsed);
		writer.Write7BitEncoded(clientId);
		writer.WriteString(proxyName);
	}

	void Trace::FileFetchLight(u32 clientId, const CasKey& key, u64 fileSize)
	{
			BEGIN_TRACE_ENTRY(FileFetchLight);
			writer.Write7BitEncoded(clientId);
			writer.Write7BitEncoded(fileSize);
	}

	void Trace::FileFetchBegin(u32 clientId, const CasKey& key, const StringView& hint)
	{
		u32 stringIndex = AddString(hint);
		BEGIN_TRACE_ENTRY(FileFetchBegin);
		writer.Write7BitEncoded(clientId);
		writer.WriteCasKey(key);
		writer.Write7BitEncoded(stringIndex);
	}

	void Trace::FileFetchSize(u32 clientId, const CasKey& key, u64 fileSize)
	{
		BEGIN_TRACE_ENTRY(FileFetchSize);
		writer.Write7BitEncoded(clientId);
		writer.WriteCasKey(key);
		writer.Write7BitEncoded(fileSize);
	}

	void Trace::FileFetchEnd(u32 clientId, const CasKey& key)
	{
		BEGIN_TRACE_ENTRY(FileFetchEnd);
		writer.Write7BitEncoded(clientId);
		writer.WriteCasKey(key);
	}

	void Trace::FileStoreBegin(u32 clientId, const CasKey& key, u64 size, const StringView& hint, bool detailed)
	{
		if (detailed)
		{
			u32 stringIndex = AddString(hint);
			BEGIN_TRACE_ENTRY(FileStoreBegin);
			writer.Write7BitEncoded(clientId);
			writer.WriteCasKey(key);
			writer.Write7BitEncoded(size);
			writer.Write7BitEncoded(stringIndex);
		}
		else
		{
			BEGIN_TRACE_ENTRY(FileStoreLight);
			writer.Write7BitEncoded(clientId);
			writer.Write7BitEncoded(size);
		}
	}

	void Trace::FileStoreEnd(u32 clientId, const CasKey& key)
	{
		BEGIN_TRACE_ENTRY(FileStoreEnd);
		writer.Write7BitEncoded(clientId);
		writer.WriteCasKey(key);
	}

	void Trace::WorkBegin(u32 workIndex, const StringView& desc, const Color& color)
	{
		u32 stringIndex = AddString(desc);
		BEGIN_TRACE_ENTRY(WorkBegin);
		writer.Write7BitEncoded(workIndex);
		writer.Write7BitEncoded(stringIndex);
		writer.WriteU32(color);
	}

	void Trace::WorkHint(u32 workIndex, const StringView& hint, u64 startTime)
	{
		u32 stringIndex = AddString(hint);
		BEGIN_TRACE_ENTRY(WorkHint);
		writer.Write7BitEncoded(workIndex);
		writer.Write7BitEncoded(stringIndex);
		writer.Write7BitEncoded(startTime ? (startTime - m_startTime) : 0);
	}

	void Trace::WorkEnd(u32 workIndex)
	{
		BEGIN_TRACE_ENTRY(WorkEnd);
		writer.Write7BitEncoded(workIndex);
	}

	void Trace::ProgressUpdate(u32 processesTotal, u32 processesDone, u32 errorCount)
	{
		BEGIN_TRACE_ENTRY(ProgressUpdate);
		writer.Write7BitEncoded(processesTotal);
		writer.Write7BitEncoded(processesDone);
		writer.Write7BitEncoded(errorCount);
	}

	void Trace::StatusUpdate(u32 statusRow, u32 statusColumn, const tchar* statusText, LogEntryType statusType, const tchar* statusLink)
	{
		BEGIN_TRACE_ENTRY(StatusUpdate);
		writer.Write7BitEncoded(statusRow);
		writer.Write7BitEncoded(statusColumn);
		writer.WriteString(statusText);
		writer.WriteByte(statusType);
		writer.WriteString(statusLink ? statusLink : TC(""));
	}

	void Trace::DriveUpdate(const tchar drive, u8 busyPercent, u32 readCount, u64 readBytes, u32 writeCount, u64 writeBytes)
	{
		BEGIN_TRACE_ENTRY_NOTIME(DriveUpdate);
		writer.WriteByte((u8)drive);
		writer.WriteByte(busyPercent);
		writer.Write7BitEncoded(readCount);
		writer.Write7BitEncoded(readBytes);
		writer.Write7BitEncoded(writeCount);
		writer.Write7BitEncoded(writeBytes);
	}

	void Trace::RemoteExecutionDisabled()
	{
		BEGIN_TRACE_ENTRY(RemoteExecutionDisabled);
	}

	void Trace::CacheBeginFetch(u32 fetchId, const tchar* description)
	{
		BEGIN_TRACE_ENTRY(CacheBeginFetch);
		writer.Write7BitEncoded(fetchId);
		writer.WriteString(description);
	}

	void Trace::CacheEndFetch(u32 fetchId, bool success, const u8* data, u64 dataSize)
	{
		BEGIN_TRACE_ENTRY(CacheEndFetch);
		writer.Write7BitEncoded(fetchId);
		writer.WriteBool(success);
		writer.WriteBytes(data, dataSize);
	}

	void Trace::CacheBeginWrite(u32 processId)
	{
		BEGIN_TRACE_ENTRY(CacheBeginWrite);
		writer.Write7BitEncoded(processId);
	}

	void Trace::CacheEndWrite(u32 processId, bool success, u64 bytesSent)
	{
		BEGIN_TRACE_ENTRY(CacheEndWrite);
		writer.Write7BitEncoded(processId);
		writer.WriteBool(success);
		writer.Write7BitEncoded(bytesSent);
	}

	TraceChannel::TraceChannel(Logger& logger) : m_logger(logger)
	{
	}

	bool TraceChannel::Init(const tchar* channelName)
	{
		#if PLATFORM_WINDOWS
		StringBuffer<245> channelMutex;
		channelMutex.Append(TCV("Uba")).Append(channelName).Append(TCV("Channel"));
		m_memHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE, 256, channelName, TC("TraceChannel"));
		if (!m_memHandle.IsValid())
		{
			m_logger.Error(TC("Failed to create file mapping %s for trace channel (%s)"), channelName, LastErrorToText().data);
			return false;
		}
		bool isCreator = GetLastError() != ERROR_ALREADY_EXISTS;

		auto mhg = MakeGuard([&]() { CloseFileMapping(m_logger, m_memHandle, TC("TraceChannel")); m_memHandle = {}; });

		m_mem = MapViewOfFile(m_logger, m_memHandle, FILE_MAP_WRITE, 0, 256);
		if (!m_mem)
		{
			m_logger.Error(TC("Failed to map file mapping for uba trace channel"));
			return false;
		}

		if (isCreator)
			*(tchar*)m_mem = 0;

		auto mg = MakeGuard([&]() { UnmapViewOfFile(m_logger, m_mem, 256, channelMutex.data); m_mem = nullptr; });

		channelMutex.Append(channelName).Append(TCV("Mutex"));
		m_mutex = CreateMutexW(false, channelMutex.data);
		if (!m_mutex)
			return false;

		mg.Cancel();
		mhg.Cancel();
		#endif
		return true;
	}

	TraceChannel::~TraceChannel()
	{
		#if PLATFORM_WINDOWS
		if (m_mem)
			UnmapViewOfFile(m_logger, m_mem, 256, TC("TraceChannel"));
		if (m_memHandle.IsValid())
			CloseFileMapping(m_logger, m_memHandle, TC("TraceChannel"));
		CloseMutex(m_mutex);
		#endif
	}

	bool TraceChannel::Write(const tchar* traceName, const tchar* ifMatching)
	{
		#if PLATFORM_WINDOWS
		WaitForSingleObject((HANDLE)m_mutex, INFINITE);
		auto g = MakeGuard([this]() { ReleaseMutex(m_mutex); });
		if (ifMatching)
			if (!Equals((tchar*)m_mem, ifMatching))
				return true;
		TStrcpy_s((tchar*)m_mem, 256, traceName);
		#endif
		return true;
	}

	bool TraceChannel::Read(StringBufferBase& outTraceName)
	{
		#if PLATFORM_WINDOWS
		WaitForSingleObject((HANDLE)m_mutex, INFINITE);
		outTraceName.Append((tchar*)m_mem);
		ReleaseMutex(m_mutex);
		#endif
		return true;
	}

	static OwnerInfo InternalGetOwnerInfo()
	{
		static tchar buffer[260];
		*buffer = 0;

		OwnerInfo info { buffer, 0 };

		StringBuffer<32> ownerPidStr;
		ownerPidStr.count = GetEnvironmentVariableW(TC("UBA_OWNER_PID"), ownerPidStr.data, ownerPidStr.capacity);
		if (ownerPidStr.count)
		{
			GetEnvironmentVariableW(TC("UBA_OWNER_ID"), buffer, sizeof_array(buffer));
			ownerPidStr.Parse(info.pid);
			return info;
		}

		#if PLATFORM_WINDOWS
		HANDLE snapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshotHandle == INVALID_HANDLE_VALUE)
			return info;

		PROCESSENTRY32 pe = { 0 };
		pe.dwSize = sizeof(PROCESSENTRY32);
		UnorderedMap<u32, u32> pidToParent;
		if (Process32First(snapshotHandle, &pe))
		{
			do
			{
				pidToParent[pe.th32ProcessID] = pe.th32ParentProcessID;
			}
			while (Process32Next(snapshotHandle, &pe));
		}
		CloseHandle(snapshotHandle);

		u32 pid = ::GetCurrentProcessId();
		while (true)
		{
			auto findIt = pidToParent.find(pid);
			if (findIt == pidToParent.end())
				break;
			pid = findIt->second;
			pidToParent.erase(findIt);

			HANDLE parentHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
			if (parentHandle == NULL)
				break;
			tchar moduleName[260];
			DWORD len = GetModuleFileNameExW(parentHandle, 0, moduleName, MAX_PATH);
			CloseHandle(parentHandle);
			if (!len)
				break;
			if (!Contains(moduleName, L"devenv.exe"))
				continue;
			TStrcpy_s(buffer, MAX_PATH, L"vs");
			info.pid = pid;
			break;
		}
		#endif

		return info;
	}

	const OwnerInfo& GetOwnerInfo()
	{
		static OwnerInfo info = InternalGetOwnerInfo();
		return info;
	}
}

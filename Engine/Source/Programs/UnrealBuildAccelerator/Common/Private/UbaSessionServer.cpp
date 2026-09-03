// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSessionServer.h"	
#include "UbaApplicationRules.h"
#include "UbaConfig.h"
#include "UbaNetworkServer.h"
#include "UbaProcess.h"
#include "UbaProcessStartInfoHolder.h"
#include "UbaScheduler.h"
#include "UbaStorage.h"

namespace uba
{
	class SessionServer::RemoteProcess final : public Process
	{
	public:
		RemoteProcess(SessionServer* server, const ProcessStartInfo& si, u32 processId, float weight_)
		:	m_server(server)
		,	m_startInfo(si)
		,	m_processId(processId)
		,	m_done(true)
		{
			m_startInfo.weight = weight_;
		}

		~RemoteProcess()
		{
			if (m_knownInputsDone.IsCreated())
				m_knownInputsDone.IsSet(50*1000);
			delete[] m_knownInputs;
		}

		virtual const ProcessStartInfo& GetStartInfo() const override { return m_startInfo; }
		virtual u32 GetId() override { return m_processId; }
		virtual u32 GetExitCode() override { UBA_ASSERT(m_done.IsSet(0)); return m_exitCode; }
		virtual bool HasExited() override { return m_done.IsSet(0); }
		virtual bool WaitForExit(u32 millisecondsTimeout) override { return m_done.IsSet(millisecondsTimeout); }
		virtual u64 GetTotalProcessorTime() const override { return m_processorTime; }
		virtual u64 GetTotalWallTime() const override { return m_wallTime; }
		virtual const Vector<ProcessLogLine>& GetLogLines() const override { return m_logLines; }
		virtual const Vector<u8>& GetTrackedInputs() const override { return m_trackedInputs; }
		virtual const Vector<u8>& GetTrackedOutputs() const override { return m_trackedOutputs; };
		virtual void Cancel(bool terminate) override
		{
			if (m_cancelled)
				return;
			m_cancelled = true;
			m_exitCode = ProcessCancelExitCode;
			if (auto s = m_server)
				s->OnCancelled(this);
			else
				m_done.Set();

			ProcessHandle h;
			h.m_process = this;
			CallProcessExit(h);
			h.m_process = nullptr;
		}

		virtual const tchar* GetExecutingHost() const override { return m_executingHost.c_str(); }
		virtual bool IsRemote() const override { return true; }
		virtual ProcessExecutionType GetExecutionType() const override { return ProcessExecutionType_Remote; }
		virtual bool IsChild() override { return false; }

		void CallProcessExit(ProcessHandle& h)
		{
			SCOPED_FUTEX(m_exitedLock, lock);
			if (!m_startInfo.exitedFunc)
				return;
			auto exitedFunc = m_startInfo.exitedFunc;
			auto userData = m_startInfo.userData;
			m_startInfo.exitedFunc = nullptr;
			m_startInfo.userData = nullptr;
			ProcessExitedResponse response = ProcessExitedResponse_None;
			exitedFunc(userData, h, response);
		}

		SessionServer* m_server;
		ProcessStartInfoHolder m_startInfo;
		Futex m_exitedLock;
		u32 m_processId;
		u32 m_exitCode = ~0u;
		u64 m_processorTime = 0;
		u64 m_wallTime = 0;
		Event m_done;
		Vector<ProcessLogLine> m_logLines;
		Vector<u8> m_trackedInputs;
		Vector<u8> m_trackedOutputs;
		bool m_cancelled = false;
		bool m_allowCrossArchitecture = false;
		u32 m_clientId = ~0u;
		u32 m_sessionId = 0;
		TString m_executingHost;

		struct KnownInput { CasKey key; u32 mappingAlignment = 0; bool allowProxy = true; };
		KnownInput* m_knownInputs = nullptr;
		u32 m_knownInputsCount = 0;
		Event m_knownInputsDone;
	};

	void SessionServerCreateInfo::Apply(const Config& config)
	{
		SessionCreateInfo::Apply(config);

		if (const ConfigTable* table = config.GetTable(TC("Session")))
		{
			table->GetValueAsBool(remoteLogEnabled, TC("RemoteLogEnabled"));
			table->GetValueAsBool(remoteTraceEnabled, TC("RemoteTraceEnabled"));
			table->GetValueAsBool(nameToHashTableEnabled, TC("NameToHashTableEnabled"));
			table->GetValueAsBool(traceIOEnabled, TC("TraceIOEnabled"));
		}
	}

	bool GetCrossArchitectureDir(Logger& logger, StringBufferBase& dir, bool reportError)
	{
		// UBT has the path win-x64/native or win-arm64/native
		bool isUbtPath = dir.EndsWith(TCV("native"));
		if (isUbtPath)
			dir.Resize(dir.count - 7); // Remove native and slash
		const tchar* archPath[2] = { TC("x64"), TC("arm64") };
		if (!dir.EndsWith(archPath[IsArmBinary]))
			return reportError ? logger.Error(TC("Module dir is not under supported folder (%s) to be able to run cross architecutres, can't figure out matching x64/arm64 folder"), dir.data) : false;
		dir.Resize(dir.count - TStrlen(archPath[IsArmBinary])).Append(archPath[!IsArmBinary]);
		if (isUbtPath)
			dir.Append(PathSeparator).Append(TCV("native"));
		return true;
	}

	SessionServer::SessionServer(const SessionServerCreateInfo& info, const u8* environment, u32 environmentSize)
	:	Session(info, TC("UbaSessionServer"), false, info.server)
	,	m_server(info.server)
	,	m_maxRemoteProcessCount(~0u)
	{
		m_server.RegisterOnClientDisconnected(ServiceId, [this](const Guid& clientUid, u32 clientId) { OnDisconnected(clientUid, clientId); });

		m_server.RegisterService(ServiceId,
			[this](const ConnectionInfo& connectionInfo, const WorkContext& workContext, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				switch (messageInfo.type)
				{
					#define UBA_SESSION_MESSAGE(x) case SessionMessageType_##x: return Handle##x(connectionInfo, workContext, reader, writer);
					UBA_SESSION_MESSAGES
					#undef UBA_SESSION_MESSAGE
				}

				UBA_ASSERT(false);
				return false;
			},
			[](u8 type)
			{
				switch (type)
				{
					#define UBA_SESSION_MESSAGE(x) case SessionMessageType_##x: return AsView(TC("")#x);
					UBA_SESSION_MESSAGES
					#undef UBA_SESSION_MESSAGE
				default:
					return ToView(TC("Unknown"));
				}
			}
		);

		if (environmentSize)
		{
			m_environmentMemory.resize(environmentSize);
			memcpy(m_environmentMemory.data(), environment, environmentSize);
		}

		m_uiLanguage = GetUserDefaultUILanguage();
		m_resetCas = info.resetCas;
		m_remoteExecutionEnabled = info.remoteExecutionEnabled;
		m_nameToHashTableEnabled = info.nameToHashTableEnabled;
		m_memKillLoadPercent = info.memKillLoadPercent;
		m_remoteLogEnabled = info.remoteLogEnabled;
		m_remoteTraceEnabled = info.remoteTraceEnabled;
		m_traceIOEnabled = info.traceIOEnabled;

		if (m_resetCas)
			m_storage.Reset();

		m_storage.SetTrace(&m_trace, m_detailedTrace);

		if (m_detailedTrace)
			m_server.SetWorkTracker(&m_trace);

		m_memoryThreadEvent.Create(true);
		if (info.checkMemory)
		{
			m_allowWaitOnMem = info.allowWaitOnMem;
			m_allowKillOnMem = info.allowKillOnMem;

			u64 memAvail;
			u64 memTotal;
			if (GetMemoryInfo(memAvail, memTotal))
			{
				m_memAvail = memAvail;
				m_memTotal = memTotal;
				m_memRequiredToSpawn = Min(u64(double(m_memTotal) * double(100 - info.memWaitLoadPercent) / 100.0), 35ull * 1024 * 1024 * 1024);
			}

			m_memoryThread.Start([this]() { ThreadMemoryCheckLoop();  return 0; }, TC("UbaMemTrackLoop"));
		}

		#if PLATFORM_WINDOWS
		m_localEnvironmentVariables.insert(TC("TMP"));
		m_localEnvironmentVariables.insert(TC("TEMP"));
		#else
		m_localEnvironmentVariables.insert(TC("TMPDIR"));
		#endif

		StringBuffer<> detoursFile;
		if (!GetDirectoryOfCurrentModule(m_logger, detoursFile))
		{
			UBA_ASSERT(false);
			return;
		}
		u32 dirLength = detoursFile.count;(void)dirLength;

		detoursFile.Append(PathSeparator).Append(UBA_DETOURS_LIBRARY);
		
		#if PLATFORM_WINDOWS
		char temp[1024];
		detoursFile.Parse(temp, sizeof_array(temp));
		m_detoursLibrary[IsArmBinary] = temp;
		if (GetCrossArchitectureDir(m_logger, detoursFile.Resize(dirLength), false))
		{
			detoursFile.Append(PathSeparator).Append(UBA_DETOURS_LIBRARY).Parse(temp, sizeof_array(temp));
			m_detoursLibrary[!IsArmBinary] = temp;
		}
		#else
		m_detoursLibrary[IsArmBinary] = detoursFile.data;
		#endif

		if (!Create(info))
		{
			UBA_ASSERT(false);
			return;
		}
	}

	SessionServer::~SessionServer()
	{
		m_memoryThreadEvent.Set();
		m_memoryThread.Wait();

		StopTraceThread();

		m_server.SetWorkTracker(nullptr);
		m_server.UnregisterOnClientDisconnected(ServiceId);
		m_server.UnregisterService(ServiceId);

		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		for (ProcessHandle& p : m_queuedRemoteProcesses)
		{
			((RemoteProcess*)p.m_process)->m_server = nullptr;
			p.Cancel(true);
		}
		m_queuedRemoteProcesses.clear();
		for (const ProcessHandle& p : m_activeRemoteProcesses)
		{
			((RemoteProcess*)p.m_process)->m_server = nullptr;
			p.Cancel(true);
		}
		m_activeRemoteProcesses.clear();

		if (m_trace.IsWriting())
		{
			StackBinaryWriter<SendMaxSize> writer;
			WriteSummary(writer, [&](Logger& logger)
				{
					PrintSummary(logger);
					m_storage.PrintSummary(logger);
					m_server.PrintSummary(logger);
					KernelStats::GetGlobal().Print(logger, true);
					PrintContentionSummary(logger);
				});
			m_trace.SessionSummary(0, writer.GetData(), writer.GetPosition());
		}

		for (auto s : m_clientSessions)
		{
			s->~ClientSession();
			aligned_free(s);
		}
		m_clientSessions.clear();

		#if 0
		for (auto& kv : m_directoryTable.m_lookup)
		{
			DirectoryTable::Directory& dir = kv.second;

			DirectoryTable::EntryLookup files(m_directoryTable.m_memoryBlock);
			m_directoryTable.PopulateDirectoryRecursive(StringKeyHasher(), dir.tableOffset, 0, files);
			for (auto& fileKv : files)
			{
				BinaryReader reader(m_directoryTable.m_memory, fileKv.second);
				StringBuffer<> filename;
				reader.ReadString(filename);
				m_logger.Info(filename.data);
			}
		}
		#endif
	}

	ProcessHandle SessionServer::RunProcessRacing(u32 raceAgainstRemoteProcessId)
	{
		// TODO: Implement
		return {};
	}

	ProcessHandle SessionServer::RunProcessRemote(const ProcessStartInfo& startInfo, float weight, const void* knownInputs, u32 knownInputsCount, bool allowCrossArchitecture)
	{
		//TrackWorkScope tws(m_trace, AsView(TC("RunProcessRemote")), ColorWork);

		UBA_ASSERT(!startInfo.startSuspended);

		FlushDeadProcesses();
		ValidateStartInfo(startInfo);
		u32 processId = CreateProcessId();
		RemoteProcess* remoteProcess = new RemoteProcess(this, startInfo, processId, weight);

		auto rules = GetRules(remoteProcess->m_startInfo);

		remoteProcess->m_startInfo.rules = rules;
		remoteProcess->m_allowCrossArchitecture = allowCrossArchitecture;

		if (knownInputsCount)
		{
			remoteProcess->m_knownInputsDone.Create(true);


			auto kiBegin = (const tchar*)knownInputs;
			auto kiEnd = kiBegin;
			for (u32 i=0;i!=knownInputsCount; ++i)
				kiEnd += TStrlen(kiEnd) + 1;

			u32 knownInputsBytes = u32((kiEnd - kiBegin)*sizeof(tchar));
			void* knownInputsCopy = malloc(knownInputsBytes);
			memcpy(knownInputsCopy, knownInputs, knownInputsBytes);

			#ifdef __clang_analyzer__
			free(knownInputsCopy); // doesn't seem to understand it is handed over
			#endif

			m_server.AddWork([remoteProcess, this, knownInputsCopy, knownInputsCount, rules](const WorkContext& context)
				{
					auto keys = remoteProcess->m_knownInputs = new RemoteProcess::KnownInput[knownInputsCount];

					struct Container
					{
						struct iterator
						{
							iterator() : ptr(nullptr), index(0) {}
							iterator(const tchar* p, u32 i) : ptr(p), index(i) {}
							iterator operator++(int) { auto prev = ptr; ptr += TStrlen(ptr) + 1; return iterator(prev, index++); }
							const tchar* operator*() { return ptr; }
							bool operator==(const iterator& o) const { return index == o.index; }
							const tchar* ptr;
							u32 index;
						};
						Container(const tchar* b, u32 c) : ptr(b), count(c) {}
						iterator begin() const { return iterator(ptr, 0); }
						iterator end() const { return iterator(ptr, count); }
						u32 size() { return count; }
						const tchar* ptr;
						u32 count;
					};

					Container container((const tchar*)knownInputsCopy, knownInputsCount);

					Atomic<u32> keysIndex;
					const TString& workingDir = remoteProcess->m_startInfo.workingDirStr;

					m_server.ParallelFor(knownInputsCount, container, [&](const WorkContext&, auto& it)
						{
							//TrackWorkScope tws2(m_trace, AsView(TC("KnownInputs")), ColorWork);
							StringBuffer<> fileName;
							FixPath(*it, workingDir.c_str(), u32(workingDir.size()), fileName);
							StringKey fileNameKey = CaseInsensitiveFs ? ToStringKeyLower(fileName) : ToStringKey(fileName);

							//tws2.AddHint(fileName);

							// Make sure cas entry exists and caskey is calculated (cas content creation is deferred in case client already has it)
							CasKey casKey;
							if (!StoreCasFile(casKey, fileNameKey, fileName.data) || casKey == CasKeyZero)
								return;

							UBA_ASSERT(keysIndex < knownInputsCount);
							auto& ki = keys[keysIndex++];
							ki.key = casKey;
							ki.mappingAlignment = GetMemoryMapAlignment(fileName, true);
							ki.allowProxy = rules->AllowStorageProxy(fileName);


							// Update name to hash table
							SCOPED_WRITE_LOCK(m_nameToHashLookupLock, lock);
							CasKey& lookupCasKey = m_nameToHashLookup[fileNameKey];
							if (lookupCasKey != casKey)
							{
								lookupCasKey = casKey;
								BinaryWriter w(m_nameToHashTableMem.memory, m_nameToHashTableMem.writtenSize, NameToHashMemSize);
								m_nameToHashTableMem.AllocateNoLock(sizeof(StringKey) + sizeof(CasKey), 1, TC("NameToHashTable"));
								w.WriteStringKey(fileNameKey);
								w.WriteCasKey(lookupCasKey);
							}
						}, AsView(TC("KnownInputsLoop")), true);

					remoteProcess->m_knownInputsCount = keysIndex;
					remoteProcess->m_knownInputsDone.Set();
					free(knownInputsCopy);

				}, 1, TC("KnownInputs"));
		}

		ProcessHandle h(remoteProcess); // Keep ref count up even if process is removed by callbacks etc.

		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		m_queuedRemoteProcesses.push_back(remoteProcess);

		SCOPED_READ_LOCK(m_remoteProcessReturnedEventLock, lock2);
		if (m_remoteProcessReturnedEvent)
		{
			if (!m_remoteExecutionEnabled)
			{
				m_logger.Info(TC("Process queued for remote but remote execution was disabled, returning process to queue"));
				m_remoteProcessReturnedEvent(*remoteProcess);
			}
			else if (!m_connectionCount)
			{
				m_logger.Info(TC("Process queued for remote but there are no active connections, returning process to queue"));
				m_remoteProcessReturnedEvent(*remoteProcess);
			}
		}
		return h;
	}

	void SessionServer::DisableRemoteExecution()
	{
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		if (m_remoteExecutionEnabled)
			m_logger.Info(TC("Disable remote execution (remote sessions will finish current processes)"));
		m_remoteExecutionEnabled = false;
		m_trace.RemoteExecutionDisabled();
	}

	bool SessionServer::IsRemoteExecutionDisabled()
	{
		return !m_remoteExecutionEnabled;
	}

	void SessionServer::ReenableRemoteExecution()
	{
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		if (m_remoteExecutionEnabled)
			return;
		m_logger.Info(TC("Reenabled remote execution"));
		m_remoteExecutionEnabled = true;
		//m_trace.RemoteExecutionDisabled();
	}

	void SessionServer::SetCustomCasKeyFromTrackedInputs(const tchar* fileName_, const tchar* workingDir_, const u8* trackedInputs, u32 trackedInputsBytes)
	{
		StringBuffer<> workingDir;
		FixFileName(workingDir, workingDir_, nullptr);
		if (workingDir[workingDir.count - 1] != '\\')
			workingDir.Append(TCV("\\"));
		StringBuffer<> fileName;
		FixFileName(fileName, fileName_, workingDir.data);
		StringKey fileNameKey = ToStringKey(fileName);
		
		SCOPED_FUTEX(m_customCasKeysLock, lock);
		auto insres = m_customCasKeys.try_emplace(fileNameKey);
		CustomCasKey& customKey = insres.first->second;
		customKey.casKey = CasKeyZero;
		customKey.workingDir = workingDir.data;
		customKey.trackedInputs.resize(trackedInputsBytes);
		memcpy(customKey.trackedInputs.data(), trackedInputs, trackedInputsBytes);

		//m_logger.Debug(TC("Registered file using custom cas %s (%s)"), fileName_, GuidToString(fileNameHash).str);
	}

	bool SessionServer::GetCasKeyFromTrackedInputs(CasKey& out, const tchar* fileName, const tchar* workingDir, const u8* data, u32 dataLen)
	{
		u64 workingDirLen = TStrlen(workingDir);

		BinaryReader reader(data);

		CasKeyHasher hasher;

		while (reader.GetPosition() < dataLen)
		{
			tchar str[512] = { 0 };
			reader.ReadString(str, sizeof_array(str));
			tchar* path = str;

			tchar temp[512];
			if (str[1] != ':' && (TStrstr(str, TC(".dll")) || TStrstr(str, TC(".exe"))))
			{
				bool res = SearchPathW(NULL, str, NULL, 512, temp, NULL);
				UBA_ASSERT(res);
				if (!res)
					return false;
				path = temp;
			}
			
			StringBuffer<> inputFileName;
			FixPath(path, workingDir, workingDirLen, inputFileName);

			if (inputFileName.StartsWith(m_tempPath.data))
				continue;
			if (inputFileName.Equals(fileName))
				continue;
			if (inputFileName.StartsWith(m_systemPath.data))
				continue;

			CasKey casKey;
			bool deferCreation = true;
			if (!m_storage.StoreCasFile(casKey, path, CasKeyZero, deferCreation))
				return false;
			UBA_ASSERTF(casKey != CasKeyZero, TC("Failed to store cas for %s when calculating key for tracked inputs on %s"), path, fileName);
			hasher.Update(&casKey, sizeof(CasKey));
		}

		out = ToCasKey(hasher, m_storage.StoreCompressed());
		return true;
	}

	void SessionServer::SetRemoteProcessSlotAvailableEvent(const Function<void(bool isCrossArchitecture)>& remoteProcessSlotAvailableEvent)
	{
		SCOPED_WRITE_LOCK(m_remoteProcessSlotAvailableEventLock, lock);
		m_remoteProcessSlotAvailableEvent = remoteProcessSlotAvailableEvent;
	}

	void SessionServer::SetRemoteProcessReturnedEvent(const Function<void(Process&)>& remoteProcessReturnedEvent)
	{
		SCOPED_WRITE_LOCK(m_remoteProcessReturnedEventLock, lock);
		m_remoteProcessReturnedEvent = remoteProcessReturnedEvent;
	}

	void SessionServer::WaitOnAllTasks()
	{
		while (true)
		{
			SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
			if (m_activeRemoteProcesses.empty() && m_queuedRemoteProcesses.empty())
				break;
			lock.Leave();
			Sleep(200);
		}

		bool isEmpty = false;
		while (!isEmpty)
		{
			Vector<ProcessHandle> processes;
			{
				SCOPED_FUTEX(m_processesLock, lock);
				isEmpty = m_processes.empty();
				processes.reserve(m_processes.size());
				for (auto& pair : m_processes)
					processes.push_back(pair.second);
			}

			for (auto& process : processes)
				process.WaitForExit(100000);
		}

		FlushDeadProcesses();
	}

	void SessionServer::SetMaxRemoteProcessCount(u32 count)
	{
		m_maxRemoteProcessCount.exchange(count);
	}

	u32 SessionServer::BeginExternalProcess(const tchar* description, const tchar* breadcrumbs)
	{
		u32 processId = CreateProcessId();
		m_trace.ProcessAdded(0, processId, ToView(description), ToView(breadcrumbs));
		return processId;
	}

	void SessionServer::EndExternalProcess(u32 id, u32 exitCode)
	{
		StackBinaryWriter<1024> statsWriter;
		ProcessStats processStats;
		processStats.Write(statsWriter);
		m_trace.ProcessExited(id, exitCode, statsWriter.GetData(), statsWriter.GetPosition(), Vector<ProcessLogLine>());
	}

	void SessionServer::UpdateProgress(u32 processesTotal, u32 processesDone, u32 errorCount)
	{
		m_trace.ProgressUpdate(processesTotal, processesDone, errorCount);
	}

	void SessionServer::UpdateStatus(u32 statusRow, u32 statusColumn, const tchar* statusText, LogEntryType statusType, const tchar* statusLink)
	{
		m_trace.StatusUpdate(statusRow, statusColumn, statusText, statusType, statusLink);
	}

	void SessionServer::AddProcessBreadcrumbs(u32 processId, const tchar* breadcrumbs, bool deleteOld)
	{
		m_trace.ProcessAddBreadcrumbs(processId, ToView(breadcrumbs), deleteOld);
	}

	NetworkServer& SessionServer::GetServer()
	{
		return m_server;
	}

	void SessionServer::RegisterNetworkTrafficProvider(const NetworkTrafficProvider& provider)
	{
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		m_provider = provider;
	}

	void SessionServer::RegisterCrossArchitectureMapping(const tchar* from, const tchar* to)
	{
		m_crossArchitectureMappings.emplace_back(CrossArchitectureMapping{from, to});
	}

	void SessionServer::SetOuterScheduler(Scheduler* scheduler)
	{
		UBA_ASSERT(!m_outerScheduler || !scheduler);
		m_outerScheduler = scheduler;
	}

	Scheduler* SessionServer::GetOuterScheduler()
	{
		return m_outerScheduler;
	}

	void SessionServer::OnDisconnected(const Guid& clientUid, u32 clientId)
	{
		u32 returnCount = 0;
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, queueLock);
		for (auto it=m_activeRemoteProcesses.begin(); it!=m_activeRemoteProcesses.end();)
		{
			RemoteProcess* remoteProcess = (RemoteProcess*)it->m_process;
			if (remoteProcess->m_clientId != clientId)
			{
				++it;
				continue;
			}
			m_queuedRemoteProcesses.push_front(*it);
			it = m_activeRemoteProcesses.erase(it);
			remoteProcess->m_executingHost.clear();

			m_trace.ProcessReturned(remoteProcess->m_processId, AsView(TC("Disconnected")));

			ProcessHandle h = ProcessRemoved(remoteProcess->m_processId);
			if (!h.m_process)
				m_logger.Warning(TC("Trying to remove process on client %u that does not exist in active list.. investigate me"), clientId);

			++returnCount;

			remoteProcess->m_clientId = ~0u;
			remoteProcess->m_sessionId = 0;

			if (m_remoteProcessReturnedEvent)
				m_remoteProcessReturnedEvent(*remoteProcess);
		}

		m_returnedRemoteProcessCount += returnCount;

		u32 sessionId = 0;
		StringBuffer<> sessionName;
		for (auto sptr : m_clientSessions)
		{
			++sessionId;
			auto& s = *sptr;
			if (s.clientId != clientId)
				continue;

			if (!returnCount && !s.hasNotification && !s.enabled)
				m_trace.SessionNotification(sessionId, TC("Done"));

			m_trace.SessionDisconnect(sessionId);

			sessionName.Append(s.name);
			UBA_ASSERTF(s.usedSlotCount == returnCount || m_logger.isMuted, TC("Used slot count different than return count (%u vs %u)"), s.usedSlotCount, returnCount);
			s.usedSlotCount -= returnCount;

			if (s.enabled)
				m_availableRemoteSlotCount -= s.processSlotCount - returnCount;
			s.enabled = false;
			s.connected = false;
			--m_connectionCount;
		}

		if (returnCount)
		{
			if (sessionName.IsEmpty())
				sessionName.Append(TCV("<can't find session>"));

			m_logger.Info(TC("Client session %s (%s) disconnected. Returned %u process(s) to queue"), sessionName.data, GuidToString(clientUid).str, returnCount);
		}

		if (m_connectionCount)
			return;

		if (!m_queuedRemoteProcesses.empty())
		{
			if (m_remoteProcessReturnedEvent)
			{
				m_logger.Info(TC("No client sessions connected and there are %llu processes left in the remote queue. Will return all queued remote processes"), m_queuedRemoteProcesses.size());
				List<ProcessHandle> temp(m_queuedRemoteProcesses);
				for (ProcessHandle& remoteProcess : temp)
					m_remoteProcessReturnedEvent(*remoteProcess.m_process);
			}
			else
			{
				m_logger.Info(TC("No client sessions connected and there are %llu processes left in the remote queue. processes will be picked up when remote connection is established"), m_queuedRemoteProcesses.size());
			}
		}

		if (!m_activeRemoteProcesses.empty())
		{
			// This path has been seen in the wild (Once over million of runs)....
			// And the theory is that 
			m_logger.Error(TC("No client sessions connected but there are %llu active remote processes. This should not happen, there is a bug in the code!!"), m_activeRemoteProcesses.size());
		}
	}

	bool SessionServer::HandleConnect(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		StringBuffer<128> name;
		reader.ReadString(name);
		u32 clientVersion = reader.ReadU32();
		bool isClientArm = false;
		if (clientVersion >= 36)
			isClientArm = reader.ReadBool();

		m_logger.Detail(TC("Client session %s connected (Id: %u, Uid: %s%s)"), name.data, connectionInfo.GetId(), GuidToString(connectionInfo.GetUid()).str, (isClientArm ? TC(", IsArm: true") : TC("")));

		CasKey clientAgentKey = reader.ReadCasKey();
		CasKey clientDetoursKey = reader.ReadCasKey();(void)clientDetoursKey;

		CasKey detoursBinaryKey[2];
		CasKey& agentBinaryKey = m_agentBinaryKey[isClientArm];

		bool binAsVersion = clientAgentKey != CasKeyZero;
		{
			SCOPED_FUTEX(m_binKeysLock, lock);

			detoursBinaryKey[0] = m_detoursBinaryKey[0];
			detoursBinaryKey[1] = m_detoursBinaryKey[1];

			StringBuffer<> detoursLib;
			bool deferCreation = true;

			// Handle x64 for both architectures and arm64 only for arm64 clients
			for (u32 i=0; i!=(isClientArm ? 2u : 1u); ++i)
			{
				if (detoursBinaryKey[i] != CasKeyZero)
					continue;
				detoursLib.Clear().Append(m_detoursLibrary[i].c_str());
				if (!m_storage.StoreCasFile(detoursBinaryKey[i], detoursLib.data, CasKeyZero, deferCreation) || detoursBinaryKey[i] == CasKeyZero)
					return m_logger.Error(TC("Failed to create cas for %s"), detoursLib.data);
				m_detoursBinaryKey[i] = detoursBinaryKey[i];
			}

			if (binAsVersion && agentBinaryKey == CasKeyZero)
			{
				StringBuffer<> agentDir;
				if (!GetDirectoryOfCurrentModule(m_logger, agentDir))
					return false;
				if (IsArmBinary != isClientArm)
					if (!GetCrossArchitectureDir(m_logger, agentDir, true))
						return false;
				UBA_ASSERT(IsWindows);
				agentDir.Append(PathSeparator).Append(UBA_AGENT_EXECUTABLE);
				if (!m_storage.StoreCasFile(agentBinaryKey, agentDir.data, CasKeyZero, deferCreation) || agentBinaryKey == CasKeyZero)
				{
					// This is hacky but uba binary is not copied by ubt anymore.
					StringBuffer<> dir2;
					if (!GetAlternativeUbaPath(m_logger, dir2, agentDir, IsWindows && isClientArm))
						return false;
					dir2.Append(UBA_AGENT_EXECUTABLE);
					if (!m_storage.StoreCasFile(agentBinaryKey, dir2.data, CasKeyZero, deferCreation) || agentBinaryKey == CasKeyZero)
						return m_logger.Error(TC("Failed to create cas for %s"), dir2.data);
				}
			}
		}

		StringBuffer<> tempBuffer;
		auto& disconnectResponse = tempBuffer;

		if (binAsVersion && clientAgentKey != agentBinaryKey)
		{
			m_logger.Warning(TC("UbaAgent binaries mismatch. Disconnecting %s"), name.data);
			disconnectResponse.Appendf(TC("UbaAgent binaries mismatch. Disconnecting..."));
		}
		else if (clientVersion != SessionNetworkVersion)
		{
			m_logger.Warning(TC("Version mismatch. Server is on version %u while client is on %u. Disconnecting %s"), SessionNetworkVersion, clientVersion, name.data);
			disconnectResponse.Appendf(TC("Version mismatch. Server is on version %u while client is on %u. Disconnecting..."), SessionNetworkVersion, clientVersion);
		}

		writer.WriteBool(disconnectResponse.IsEmpty());

		if (!disconnectResponse.IsEmpty())
		{
			writer.WriteString(disconnectResponse);
			writer.WriteCasKey(agentBinaryKey);
			writer.WriteCasKey(detoursBinaryKey[0]);
			if (isClientArm)
				writer.WriteCasKey(detoursBinaryKey[1]);
			return true;
		}

		u32 processSlotCount = reader.ReadU32();
		bool dedicated = reader.ReadBool();

		StringBuffer<256> info;
		reader.ReadString(info);

		u64 memAvail = reader.ReadU64();
		u64 memTotal = reader.ReadU64();
		u32 cpuLoadValue = reader.ReadU32();
		float cpuLoad = *(float*)&cpuLoadValue;


		// I have no explanation for this. On linux we get a shutdown crash when running through UBT if session is allocated with normal new
		// For now we will work around it by using aligned_alloc which seems to be working on all platforms
		auto& session = *new (aligned_alloc(alignof(ClientSession), sizeof(ClientSession))) ClientSession();
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		m_clientSessions.push_back(&session);
		u32 sessionId = u32(m_clientSessions.size());
		session.name = name.data;
		session.clientId = connectionInfo.GetId();
		session.processSlotCount = processSlotCount;
		session.dedicated = dedicated;
		session.isArm = isClientArm;
		session.memAvail = memAvail;
		session.memTotal = memTotal;
		session.cpuLoad = cpuLoad;
		m_availableRemoteSlotCount += processSlotCount;
		++m_connectionCount;

		if (!InitializeNameToHashTable())
			return false;

		writer.WriteCasKey(m_detoursBinaryKey[0]);
		if (isClientArm)
			writer.WriteCasKey(m_detoursBinaryKey[1]);
		writer.WriteBool(m_resetCas);
		writer.WriteU32(sessionId);
		writer.WriteU32(m_uiLanguage);
		writer.WriteBool(m_storeIntermediateFilesCompressed);
		writer.WriteBool(m_detailedTrace);
		writer.WriteBool(m_remoteLogEnabled);
		writer.WriteBool(m_remoteTraceEnabled);
		writer.WriteBool(m_readIntermediateFilesCompressed);

		auto& computerName = tempBuffer.Clear();
		GetComputerNameW(computerName);
		writer.WriteString(computerName);

		WriteRemoteEnvironmentVariables(writer);

		m_trace.SessionAdded(sessionId, connectionInfo.GetId(), name, info); // Must be inside lock for TraceSessionUpdate() to not include
		m_trace.SessionUpdate(sessionId, 1, 0, 0, 0, memAvail, memTotal, cpuLoad);

		lock.Leave();
		return true;
	}

	bool SessionServer::HandleEnsureBinaryFile(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		bool clientIsArm = reader.ReadBool();
		StringBuffer<> fileName;
		reader.ReadString(fileName);
		StringKey fileNameKey = reader.ReadStringKey();
		TString applicationDir = reader.ReadString();
		TString workingDir = reader.ReadString();

		StringBuffer<> lookupStr;
		lookupStr.Append(fileName).Append(applicationDir).Append(workingDir).Append('#');
		lookupStr.MakeLower();
		StringKey lookupKey = ToStringKeyNoCheck(lookupStr.data, lookupStr.count);

		SCOPED_FUTEX(m_applicationDataLock, lock);
		auto insres = m_applicationData.try_emplace(lookupKey);
		ApplicationData& data = insres.first->second;
		lock.Leave();

		SCOPED_FUTEX(data.lock, lock2);
		if (!data.bytes.empty())
		{
			writer.WriteBytes(data.bytes.data(), data.bytes.size());
			return true;
		}

		Vector<TString> loaderPaths;
		while (reader.GetLeft())
			loaderPaths.push_back(reader.ReadString());

		CasKey casKey = CasKeyZero;
		StringBuffer<> absoluteFile;

		auto FixCrossArchitecture = [this, clientIsArm](StringBuffer<>& absoluteFile)
			{
				if (clientIsArm == IsArmBinary)
					return;
				for (auto& mapping : m_crossArchitectureMappings)
					if (absoluteFile.StartsWith(mapping.from.c_str()))
					{
						StringBuffer<> temp;
						temp.Append(absoluteFile.data + mapping.from.size());
						absoluteFile.Clear().Append(mapping.to).Append(temp);
						break;
					}
			};


		if (!loaderPaths.empty())
		{
			for (auto& loaderPath : loaderPaths)
			{
				StringBuffer<> fullPath;

				#if PLATFORM_LINUX
				if (loaderPath[0] != '/') // TODO: Revisit this.. should be done in a less hacky way.
				#endif
					fullPath.Append(applicationDir).EnsureEndsWithSlash();
				fullPath.Append(loaderPath).EnsureEndsWithSlash().Append(fileName);
				if (GetFileAttributesW(fullPath.data) == INVALID_FILE_ATTRIBUTES)
					continue;
				FixPath(fullPath.data, nullptr, 0, absoluteFile);
				FixCrossArchitecture(absoluteFile);
				fileNameKey = ToStringKeyLower(absoluteFile);
				if (!StoreCasFile(casKey, fileNameKey, absoluteFile.data))
					return false;
				break;
			}

			#if 0
			if (casKey == CasKeyZero)
			{
				m_logger.Warning(TC("HandleEnsureBinaryFile - Failed to find file %s"), fileName.data);
				for (auto& loaderPath : loaderPaths)
				{
					m_logger.Warning(TC("   LoaderPath %s"), loaderPath.c_str());
				}
			}
			#endif
		}
		else if (SearchPathForFile(m_logger, absoluteFile, fileName.data, workingDir, applicationDir))
		{

			if (!absoluteFile.StartsWith(m_systemPath.data) || !IsKnownSystemFile(absoluteFile.data))
			{
				FixCrossArchitecture(absoluteFile);
				fileNameKey = ToStringKeyLower(absoluteFile);
				if (!StoreCasFile(casKey, fileNameKey, absoluteFile.data))
					return false;
			}
		}

		u64 startPos = writer.GetPosition();
		writer.WriteCasKey(casKey);
		writer.WriteString(absoluteFile);

		u64 bytesSize = writer.GetPosition() - startPos;
		data.bytes.resize(bytesSize);
		memcpy(data.bytes.data(), writer.GetData() + startPos, bytesSize);

		return true;
	}

	bool SessionServer::HandleGetApplication(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = reader.ReadU32(); (void)processId;
		StringBuffer<> applicationName;
		reader.ReadString(applicationName);
		StringKey applicationKey = ToStringKeyLower(applicationName);

		SCOPED_FUTEX(m_applicationDataLock, lock);
		auto insres = m_applicationData.try_emplace(applicationKey);
		ApplicationData& data = insres.first->second;
		lock.Leave();
				
		SCOPED_FUTEX(data.lock, lock2);
		if (!data.bytes.empty())
		{
			writer.WriteBytes(data.bytes.data(), data.bytes.size());
			return true;
		}

		u64 startPos = writer.GetPosition();
		Vector<BinaryModule> modules;
		if (!GetBinaryModules(modules, applicationName.data))
			return false;

		writer.WriteU32(m_systemPath.count);
		writer.WriteU32(u32(modules.size()));
		for (BinaryModule& m : modules)
		{
			CasKey casKey;
			if (!StoreCasFile(casKey, StringKeyZero, m.path.c_str()))
				return false;
			writer.WriteString(m.path);
			writer.WriteU32(m.fileAttributes);
			writer.WriteBool(m.isSystem);
			writer.WriteCasKey(casKey);
			#if PLATFORM_MAC
			writer.WriteU32(m.minOsVersion);
			#endif
		}

		u64 bytesSize = writer.GetPosition() - startPos;
		data.bytes.resize(bytesSize);
		memcpy(data.bytes.data(), writer.GetData() + startPos, bytesSize);

		return true;
	}

	bool SessionServer::HandleGetFileFromServer(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = reader.ReadU32(); (void)processId;
		StringBuffer<> fileName;
		reader.ReadString(fileName);
		StringKey fileNameKey = reader.ReadStringKey();

		workContext.tracker.AddHint(StringView(fileName).GetFileName());

		CasKey casKey;
		if (!StoreCasFile(casKey, fileNameKey, fileName.data))
			return false;
		if (casKey == CasKeyZero)
		{
			// TODO: Should this instead use DirectoryTable? (it is currently not properly populated for lookups)
			u32 attr = GetFileAttributesW(fileName.data);
			if (attr == INVALID_FILE_ATTRIBUTES || !IsDirectory(attr))
			{
				// Not finding a file is a valid path. Some applications try with a path and if fails try another path
				//m_logger.Error(TC("Failed to create cas for %s (not found)"), fileName.data);
				writer.WriteCasKey(casKey);
				return true;
			}

			casKey = CasKeyIsDirectory;
		}

		u64 serverTime;
		if (m_nameToHashInitialized && casKey != CasKeyIsDirectory)
		{
			SCOPED_WRITE_LOCK(m_nameToHashLookupLock, lock);
			serverTime = GetTime();
			CasKey& lookupCasKey = m_nameToHashLookup[fileNameKey];
			if (lookupCasKey != casKey)
			{
				lookupCasKey = casKey;
				BinaryWriter w(m_nameToHashTableMem.memory, m_nameToHashTableMem.writtenSize, NameToHashMemSize);
				m_nameToHashTableMem.AllocateNoLock(sizeof(StringKey) + sizeof(CasKey), 1, TC("NameToHashTable"));
				w.WriteStringKey(fileNameKey);
				w.WriteCasKey(casKey);
			}
		}
		else
			serverTime = GetTime();

		writer.WriteCasKey(casKey);
		writer.WriteU64(serverTime);
		return true;
	}

	bool SessionServer::HandleGetLongPathName(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		#if PLATFORM_WINDOWS
		StringBuffer<> shortPath;
		reader.ReadString(shortPath);
		StringBuffer<> longPath;
		longPath.count = ::GetLongPathNameW(shortPath.data, longPath.data, longPath.capacity);
		writer.WriteU32(GetLastError());
		writer.WriteString(longPath);
		return true;
		#else
		return false;
		#endif
	}

	bool SessionServer::HandleSendFileToServer(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 clientId = connectionInfo.GetId();
		u32 processId = reader.ReadU32();
		StringBuffer<> destination;
		reader.ReadString(destination);
		StringKey destinationKey = reader.ReadStringKey();
		u32 attributes = reader.ReadU32();
		UBA_ASSERT(attributes);
		CasKey casKey = reader.ReadCasKey();
		Storage::RetrieveResult res;
		bool success = m_storage.RetrieveCasFile(res, casKey, destination.data, nullptr, 1, true, clientId);
		casKey = res.casKey;
		if (!success)
		{
			auto logType = connectionInfo.ShouldDisconnect() ? LogEntryType_Info : LogEntryType_Warning;
			m_logger.Logf(logType, TC("Failed to retrieve cas for %s from client %u (Needed to write %s)"), CasKeyString(casKey).str, clientId, destination.data);
		}

		auto writeResponse = MakeGuard([&]() { writer.WriteBool(success); });

		bool shouldWriteToDisk = ShouldWriteToDisk(destination);
		if (success)
		{
			if (destination.StartsWith(TC("<log>")))
			{
				StringBuffer<> logPath;
				logPath.Append(m_sessionLogDir).Append(destination.data + 5);
				if (!m_storage.CopyOrLink(casKey, logPath.data, attributes))
					m_logger.Error(TC("Failed to copy cas from %s to %s"), CasKeyString(casKey).str, logPath.data);
				else if (!m_storage.DropCasFile(casKey, false, logPath.data))
					m_logger.Error(TC("Failed to drop cas %s"), CasKeyString(casKey).str);
				return true;
			}

			if (destination.StartsWith(TC("<uba>")))
			{
				StringBuffer<> ubaPath;
				ubaPath.Append(m_sessionLogDir);

				ClientSession* session = nullptr;
				for (auto& s : m_clientSessions)
					if (s->clientId == clientId)
						session = s;
				if (session)
					ubaPath.Append(session->name);
				else
					ubaPath.Append(TCV("Connection")).AppendValue(clientId);

				ubaPath.Append(TCV(".uba"));
				m_storage.CopyOrLink(casKey, ubaPath.data, attributes);
				m_storage.DropCasFile(casKey, false, ubaPath.data);
				return true;
			}

			if (shouldWriteToDisk)
			{
				bool writeCompressed = false;

				ProcessHandle h = GetProcess(processId);
				if (!h.IsValid())
				{
					success = false;
					m_logger.Info(TC("Failed to find process for id %u when receiving SendFileToServer message"), processId);
					return false;
				}

				RootsHandle rootsHandle = h.GetStartInfo().rootsHandle;

				auto& rules = *h.GetStartInfo().rules;

				Storage::FormattingFunc formattingFunc;
				bool escapeSpaces;
				if (HasVfs(rootsHandle) && rules.ShouldDevirtualizeFile(destination, escapeSpaces))
				{
					formattingFunc = [&](MemoryBlock& destData, const void* sourceData, u64 sourceSize, const tchar* hint)
						{
							return DevirtualizeDepsFile(rootsHandle, destData, sourceData, sourceSize, escapeSpaces, hint);
						};
				}
				else if (m_storeIntermediateFilesCompressed)
				{
					writeCompressed = g_globalRules.FileCanBeCompressed(destination);
				}
				success = m_storage.CopyOrLink(casKey, destination.data, attributes, writeCompressed, formattingFunc);
				if (!success)
					m_logger.Error(TC("Failed to copy cas from %s to %s (%s)"), CasKeyString(casKey).str, destination.data, GetProcessDescription(processId).c_str());
				else
					TraceWrittenFile(processId, destination);
			}
			else
			{
				success = m_storage.FakeCopy(casKey, destination.data, 0, 0, attributes);
				if (!success)
					m_logger.Error(TC("Failed to fake copy cas from %s to %s (%s)"), CasKeyString(casKey).str, destination.data, GetProcessDescription(processId).c_str());
				SCOPED_WRITE_LOCK(m_receivedFilesLock, lock);
				m_receivedFiles.try_emplace(destinationKey, casKey);
			}
		}

		if (success)
		{
			bool invalidateStorage = false; // No need, already handled in m_storage.CopyOrLink
			RegisterCreateFileForWrite(StringKeyZero, destination, shouldWriteToDisk, 0, 0, invalidateStorage);


			SCOPED_FUTEX_READ(m_processesLock, lock);
			auto findIt = m_processes.find(processId);
			if (findIt != m_processes.end())
			{
				ProcessHandle h(findIt->second);
				lock.Leave();
				auto& process = *(RemoteProcess*)h.m_process;
				if (process.m_startInfo.trackInputs)
				{
					u64 bytes = GetStringWriteSize(destination.data, destination.count);
					u64 prevSize = process.m_trackedOutputs.size();
					process.m_trackedOutputs.resize(prevSize + bytes);
					BinaryWriter w2(process.m_trackedOutputs.data(), prevSize, prevSize + bytes);
					w2.WriteString(destination);
				}
			}
		}
		return true;
	}

	bool SessionServer::HandleDeleteFile(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		StringKey fileNameKey = reader.ReadStringKey();
		StringBuffer<> fileName;
		reader.ReadString(fileName);
		bool result = uba::DeleteFileW(fileName.data);
		u32 errorCode = GetLastError();
		if (result)
			RegisterDeleteFile(fileNameKey, fileName);
		writer.WriteBool(result);
		writer.WriteU32(errorCode);
		return true;
	}

	bool SessionServer::HandleCopyFile(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		StringKey fromNameKey = reader.ReadStringKey(); (void)fromNameKey;
		StringBuffer<> fromName;
		reader.ReadString(fromName);
		StringKey toNameKey = reader.ReadStringKey();
		StringBuffer<> toName;
		reader.ReadString(toName);
		bool result = uba::CopyFileW(fromName.data, toName.data, false);
		u32 errorCode = GetLastError();
		if (result)
			RegisterCreateFileForWrite(toNameKey, toName, true);
		writer.WriteU32(errorCode);
		return true;
	}

	bool SessionServer::HandleCreateDirectory(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		CreateDirectoryMessage msg;
		reader.ReadString(msg.name);
		CreateDirectoryResponse response;
		if (!Session::CreateDirectory(response, msg))
			return false;
		writer.WriteBool(response.result);
		writer.WriteU32(response.errorCode);
		return true;
	}

	bool SessionServer::HandleRemoveDirectory(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		RemoveDirectoryMessage msg;
		reader.ReadString(msg.name);
		RemoveDirectoryResponse response;
		if (!Session::RemoveDirectory(response, msg))
			return false;
		writer.WriteBool(response.result);
		writer.WriteU32(response.errorCode);
		return true;
	}

	bool SessionServer::HandleListDirectory(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 sessionId = reader.ReadU32();
		u32 sessionIndex = sessionId - 1;
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		if (sessionIndex >= m_clientSessions.size())
			return m_logger.Error(TC("Got ListDirectory message from connection using bad sessionid (%u/%llu)"), sessionIndex, m_clientSessions.size());
		ClientSession& session = *m_clientSessions[sessionIndex];
		lock.Leave();

		StringBuffer<> dirName;
		reader.ReadString(dirName);
		StringKey dirKey = reader.ReadStringKey();
		ListDirectoryResponse out;
		GetListDirectoryInfo(out, dirName, dirKey);
		writer.WriteU32(out.tableOffset);
		WriteDirectoryTable(session, reader, writer);
		return true;
	}

	bool SessionServer::HandleGetDirectoriesFromServer(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 sessionId = reader.ReadU32();
		u32 sessionIndex = sessionId - 1;
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		if (sessionIndex >= m_clientSessions.size())
			return m_logger.Error(TC("Got GetDirectories message from connection using bad sessionid (%u/%llu)"), sessionIndex, m_clientSessions.size());
		ClientSession& session = *m_clientSessions[sessionIndex];
		lock.Leave();
		WriteDirectoryTable(session, reader, writer);
		return true;
	}

	bool SessionServer::HandleGetNameToHashFromServer(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 requestedSize = reader.ReadU32();

		SCOPED_READ_LOCK(m_nameToHashLookupLock, lock);
		if (requestedSize == ~0u)
		{
			requestedSize = u32(m_nameToHashTableMem.writtenSize);
			writer.WriteU32(requestedSize);
		}
		writer.WriteU64(GetTime());
		lock.Leave();

		WriteNameToHashTable(reader, writer, requestedSize);
		return true;
	}

	bool SessionServer::HandleProcessAvailable(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 sessionId = reader.ReadU32();
		u32 sessionIndex = sessionId - 1;

		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, sessionsLock);
		if (sessionIndex >= m_clientSessions.size())
			return m_logger.Error(TC("Got ProcessAvailable message from connection using bad sessionid (%u/%llu)"), sessionIndex, m_clientSessions.size());
		ClientSession& session = *m_clientSessions[sessionIndex];
		sessionsLock.Leave();

		bool isCrossArchitecture = IsArmBinary != session.isArm;

		u32 weight32 = reader.ReadU32();
		float availableWeight = *(float*)&weight32;

		Vector<RemoteProcess*> processesWithKnownInputsToSend;

		float weightLeft = availableWeight;
		u32 addCount = 0;
		SCOPED_FUTEX(m_fillUpOneAtTheTimeLock, fillLock); // This is a lock to group files better (all clients connect at the same time);
		while (weightLeft > 0)
		{
			RemoteProcess* process = DequeueProcess(session, sessionId, connectionInfo.GetId());
			if (process == nullptr)
				break;
			auto& startInfo = process->m_startInfo;

			StringBuffer<> applicationOverride;
			if (isCrossArchitecture)
			{
				auto returnProcess = [&]()
				{
					m_queuedRemoteProcesses.push_front({process});
					if (m_remoteProcessReturnedEvent)
						m_remoteProcessReturnedEvent(*process);
				};
			
				if (!process->m_allowCrossArchitecture)
				{
					// There is a risk cross architecture client dequeues another client's process that is not cross architecture
					// and in that case we just have to return it and try again later.. (break out)
					returnProcess();
					break;
				}

				for (auto& mapping : m_crossArchitectureMappings)
					if (StartsWith(startInfo.application, mapping.from.c_str()))
					{
						applicationOverride.Append(mapping.to).Append(startInfo.application + mapping.from.size());
						break;
					}

				if (applicationOverride.count)
				{
					//m_logger.Info(TC("Couldn't find cross architecture mapping for %s"), startInfo.application);
					//returnProcess();
					//break;
					if (!FileExists(m_logger, applicationOverride.data))
					{
						m_logger.Info(TC("Couldn't find cross architecture executable %s"), applicationOverride.data);
						returnProcess();
						break;
					}
				}

			}

			ProcessAdded(*process, sessionId);
			writer.WriteU32(process->m_processId);
			startInfo.Write(writer, applicationOverride);

			if (process->m_knownInputsDone.IsCreated())
				processesWithKnownInputsToSend.push_back(process);

			++addCount;

			if (writer.GetCapacityLeft() < 5000) // Arbitrary number to cover all parameters above
				break;

			weightLeft -= startInfo.weight;
		}
		fillLock.Leave();

		u32 neededDirectoryTableSize = GetDirectoryTableSize();
		u32 neededHashTableSize;
		{
			SCOPED_READ_LOCK(m_nameToHashLookupLock, l);
			neededHashTableSize = u32(m_nameToHashTableMem.writtenSize);
		}

		sessionsLock.Enter();
		//if (addCount)
		//	m_logger.Debug(TC("Gave %u processes to %s using up %.1f weight out of %.1f available"), addCount, session.name.c_str(), availableWeight - weightLeft, availableWeight);

		bool remoteExecutionEnabled = m_remoteExecutionEnabled || !m_queuedRemoteProcesses.empty();
		if (!remoteExecutionEnabled)
		{
			if (session.enabled)
				m_availableRemoteSlotCount -= session.processSlotCount - session.usedSlotCount;
			session.enabled = false;
			m_logger.Detail(TC("Disable remote execution on %s because remote execution has been disabled and queue is empty (will finish %u processes)"), session.name.c_str(), session.usedSlotCount);
		}

		// If this client session has 0 active processes and m_maxRemoteProcessCount < total available compute - client session, then we can disconnect this client
		if (remoteExecutionEnabled && !addCount && m_maxRemoteProcessCount != ~0u)
		{
			if (!session.dedicated && !session.usedSlotCount)
			{
				if (m_maxRemoteProcessCount < m_availableRemoteSlotCount - session.processSlotCount)
				{
					if (session.enabled)
						m_availableRemoteSlotCount -= session.processSlotCount - session.usedSlotCount;
					session.enabled = false;
					remoteExecutionEnabled = false;
					m_logger.Info(TC("Disable remote execution on %s because host session has enough help (%u left and %u remote slots)"), session.name.c_str(), m_maxRemoteProcessCount.load(), m_availableRemoteSlotCount);
				}
			}
		}
		sessionsLock.Leave();

		writer.WriteU32(remoteExecutionEnabled ? SessionProcessAvailableResponse_None : SessionProcessAvailableResponse_RemoteExecutionDisabled);

				
		// Write in the needed dir and hash table offset to be up-to-date (to potentially avoid additional messages from client
		writer.WriteU32(neededDirectoryTableSize);
		writer.WriteU32(neededHashTableSize);


		// Collect known inputs
		Vector<RemoteProcess::KnownInput*> knownInputsToSend;
		for (auto process : processesWithKnownInputsToSend)
			if (process->m_knownInputsDone.IsSet(50*1000))
				for (auto kiIt = process->m_knownInputs, kiEnd = kiIt + process->m_knownInputsCount; kiIt!=kiEnd; ++kiIt)
					if (session.sentKeys.insert(kiIt->key).second)
						knownInputsToSend.push_back(kiIt);

		// Send caskeys of known inputs so client can start retrieving them straight away
		u32 kiCapacity = u32(writer.GetCapacityLeft() - sizeof(u32)) / sizeof(RemoteProcess::KnownInput);
		u32 toSendCount = Min(kiCapacity, u32(knownInputsToSend.size()));
		writer.WriteU32(toSendCount);
		for (auto kv : knownInputsToSend)
		{
			if (!toSendCount--)
				break;
			writer.WriteCasKey(kv->key);
			writer.WriteU32(kv->mappingAlignment);
			writer.WriteBool(kv->allowProxy);
		}
		return true;
	}

	bool SessionServer::HandleProcessInputs(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = u32(reader.Read7BitEncoded());
		ProcessHandle h(GetProcess(processId));
		if (!h.IsValid())
		{
			m_logger.Info(TC("Failed to find process for id %u when receiving custom message"), processId);
			return false;
		}
		auto& process = *(RemoteProcess*)h.m_process;
		auto& inputs = process.m_trackedInputs;
		u64 size = inputs.size();
		if (u64 addCapacity = reader.Read7BitEncoded())
			inputs.reserve(size + addCapacity);
		u64 toRead = reader.GetLeft();
		inputs.resize(size + toRead);
		reader.ReadBytes(inputs.data() + size, toRead);
		return true;
	}

	bool SessionServer::HandleProcessFinished(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = reader.ReadU32();

		ProcessHandle h = ProcessRemoved(processId);
		if (!h.m_process)
		{
			m_logger.Info(TC("Client finished process with id %u that is not found on server"), processId);
			return false;
		}
		auto& process = *(RemoteProcess*)h.m_process;

		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, cs2);
		if (!m_activeRemoteProcesses.erase(&process))
		{
			cs2.Leave();
			m_logger.Info(TC("Got finished process but process was not in active remote processes. Was there a disconnect happening directly after but executed before?"));
			return false;
		}
		u32 sessionIndex = process.m_sessionId - 1;
		if (sessionIndex >= m_clientSessions.size())
			return m_logger.Error(TC("Got ProcessFinished message from connection using bad sessionid (%u/%llu)"), sessionIndex, m_clientSessions.size());
		auto& session = *m_clientSessions[sessionIndex];
		++m_finishedRemoteProcessCount;
		--session.usedSlotCount;
		if (session.enabled)
			++m_availableRemoteSlotCount;
		process.m_clientId = ~0u;
		cs2.Leave();

		u32 exitCode = reader.ReadU32();
		u32 logLineCount = reader.ReadU32();

		process.m_exitCode = exitCode;
		process.m_logLines.reserve(logLineCount);
		while (logLineCount-- != 0)
		{
			TString text = reader.ReadString();
			LogEntryType type = LogEntryType(reader.ReadByte());
			process.m_logLines.push_back({ std::move(text), type });
		}

		if (auto func = process.m_startInfo.logLineFunc)
			for (auto& line : process.m_logLines)
				func(process.m_startInfo.logLineUserData, line.text.c_str(), u32(line.text.size()), line.type);

		u32 id = process.m_processId;
		Vector<ProcessLogLine> emptyLines;
		auto& logLines = (exitCode != 0 || m_detailedTrace) ? process.m_logLines : emptyLines;
		m_trace.ProcessExited(id, exitCode, reader.GetPositionData(), reader.GetLeft(), logLines);

		ProcessStats processStats;
		processStats.Read(reader, ~0u);

		//SessionStats sessionStats;
		//sessionStats.Read(reader);
		//StorageStats storageStats;
		//storageStats.Read(reader);

		process.m_processorTime = processStats.cpuTime;
		process.m_wallTime = processStats.wallTime;
		process.m_server = nullptr;
		process.m_done.Set();
		process.CallProcessExit(h);
		return true;
	}

	bool SessionServer::HandleProcessReturned(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = reader.ReadU32();
		StringBuffer<> reason;
		reader.ReadString(reason);

		ProcessHandle h = ProcessRemoved(processId);
		RemoteProcess* process = (RemoteProcess*)h.m_process;
		if (!process)
		{
			m_logger.Warning(TC("Client %s returned process %u that is not found on server (%s)"), GuidToString(connectionInfo.GetUid()).str, processId, reason.data);
			return true;
		}

		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, cs2);
		if (!m_activeRemoteProcesses.erase(process))
		{
			cs2.Leave();
			m_logger.Warning(TC("Got returned process %u from client %s but process was not in active remote processes. Was there a disconnect happening directly after but executed before?"), processId, GuidToString(connectionInfo.GetUid()).str);
			return true;
		}
		u32 sessionIndex = process->m_sessionId - 1;
		if (sessionIndex >= m_clientSessions.size())
			return m_logger.Error(TC("Got ProcessReturned message from connection using bad sessionid (%u/%llu)"), sessionIndex, m_clientSessions.size());
		auto& session = *m_clientSessions[sessionIndex];
		--session.usedSlotCount;
		if (session.enabled)
			++m_availableRemoteSlotCount;

		m_logger.Detail(TC("Client %s returned process %u to queue (%s)"), session.name.c_str(), processId, reason.data);
		++m_returnedRemoteProcessCount;

		process->m_executingHost.clear();
		process->m_clientId = ~0u;
		process->m_sessionId = 0;

		m_trace.ProcessReturned(process->m_processId, reason);
		m_queuedRemoteProcesses.push_front(h);

		if (m_remoteProcessReturnedEvent)
			m_remoteProcessReturnedEvent(*process);
		return true;
	}

	bool SessionServer::HandleGetRoots(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		RootsHandle rootsHandle = reader.ReadU64();
		auto rootsEntry = GetRootsEntry(rootsHandle);
		if (!rootsEntry)
			return false;
		writer.WriteBytes(rootsEntry->memory.data(), rootsEntry->memory.size());
		return true;
	}

	bool SessionServer::HandleVirtualAllocFailed(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		m_logger.Error(TC("VIRTUAL ALLOC FAILING ON REMOTE MACHINE %s !"), GuidToString(connectionInfo.GetUid()).str);
		return true;
	}

	bool SessionServer::HandleGetTraceInformation(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 remotePos = reader.ReadU32();
		u32 localPos;
		{
			SCOPED_FUTEX_READ(m_trace.m_memoryLock, l);
			localPos = u32(m_trace.m_memoryPos);
		}

		writer.WriteU32(localPos);
		u32 toWrite = Min(localPos - remotePos, u32(writer.GetCapacityLeft()));
		writer.WriteBytes(m_trace.m_memoryBegin + remotePos, toWrite);
		return true;
	}

	bool SessionServer::HandlePing(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		LOG_STALL_SCOPE(m_logger, 5, TC("HandlePing took more than %s"));
		
		u32 sessionId = reader.ReadU32();
		u64 lastPing = reader.ReadU64();
		u64 memAvail = reader.ReadU64();
		u64 memTotal = reader.ReadU64();
		u32 cpuLoadValue = reader.ReadU32();

		u64 pingTime = GetTime();
		u32 sessionIndex = sessionId - 1;
		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		if (sessionIndex >= m_clientSessions.size())
			return m_logger.Error(TC("Got Pingmessage from connection using bad sessionid (%u/%llu)"), sessionIndex, m_clientSessions.size());
		auto& session = *m_clientSessions[sessionIndex];
		session.pingTime = pingTime;
		session.lastPing = lastPing;
		session.memAvail = memAvail;
		session.memTotal = memTotal;
		session.cpuLoad = *(float*)&cpuLoadValue;
		writer.WriteBool(session.abort);
		writer.WriteBool(session.crashdump);
		session.crashdump = false;

		return true;
	}

	bool SessionServer::HandleNotification(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 sessionId = reader.ReadU32();

		u32 sessionIndex = sessionId - 1;
		{
			SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
			if (sessionIndex < m_clientSessions.size())
				m_clientSessions[sessionIndex]->hasNotification = true;
		}

		StringBuffer<1024> str;
		reader.ReadString(str);
		m_trace.SessionNotification(sessionId, str.data);
		return true;
	}

	bool SessionServer::HandleGetNextProcess(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = reader.ReadU32();
		u32 prevExitCode = reader.ReadU32();
		ProcessHandle h(GetProcess(processId));
		if (!h.IsValid())
		{
			m_logger.Info(TC("Failed to find process for id %u when receiving GetNextProcess message"), processId);
			return false;
		}

		auto& remoteProcess = *(RemoteProcess*)h.m_process;
		SCOPED_FUTEX(remoteProcess.m_exitedLock, exitedLock);
		NextProcessInfo nextProcess;
		bool newProcess;
		remoteProcess.m_exitCode = prevExitCode;
		remoteProcess.m_done.Set();
		bool success = GetNextProcess(remoteProcess, newProcess, nextProcess, prevExitCode, reader);
		remoteProcess.m_exitCode = ~0u;
		remoteProcess.m_done.Reset();
		if (!success)
			return false;

		writer.WriteBool(newProcess);
		if (newProcess)
		{
			writer.WriteString(nextProcess.arguments);
			writer.WriteString(nextProcess.workingDir);
			writer.WriteString(nextProcess.description);
			writer.WriteString(nextProcess.logFile);
		}
		return true;
	}

	bool SessionServer::HandleCustom(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = reader.ReadU32();
		ProcessHandle h(GetProcess(processId));
		if (!h.IsValid())
		{
			m_logger.Info(TC("Failed to find process for id %u when receiving custom message"), processId);
			return false;
		}
		auto& remoteProcess = *(RemoteProcess*)h.m_process;
		SCOPED_FUTEX(remoteProcess.m_exitedLock, exitedLock);
		CustomMessage(remoteProcess, reader, writer);
		return true;
	}

	bool SessionServer::HandleUpdateEnvironment(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 processId = reader.ReadU32();
		ProcessHandle h(GetProcess(processId));
		if (!h.IsValid())
		{
			m_logger.Info(TC("Failed to find process for id %u when receiving update environment message"), processId);
			return false;
		}
		StringBuffer<> reason;
		reader.ReadString(reason);
		m_trace.ProcessEnvironmentUpdated(processId, reason, reader.GetPositionData(), reader.GetLeft(), ToView(h.GetStartInfo().breadcrumbs));
		return true;
	}

	bool SessionServer::HandleSummary(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		u32 sessionId = reader.ReadU32();
		m_trace.SessionSummary(sessionId, reader.GetPositionData(), reader.GetLeft());
		return true;
	}

	bool SessionServer::HandleCommand(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		StringBuffer<128> command;
		reader.ReadString(command);

		auto WriteString = [&](const tchar* str, LogEntryType type = LogEntryType_Info) { writer.WriteByte(type); writer.WriteString(str); };

		if (command.Equals(TCV("status")))
		{
			u32 totalUsed = 0;
			u32 totalSlots = 0;
			SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, queueLock);
			u64 time = GetTime();
			for (auto& s : m_clientSessions)
			{
				if (!s->enabled)
					continue;
				WriteString(StringBuffer<>().Appendf(TC("Session %u (%s)"), s->clientId, s->name.c_str()).data);
				WriteString(StringBuffer<>().Appendf(TC("   Process slots used %u/%u"), s->usedSlotCount, s->processSlotCount).data);
				if (s->pingTime)
					WriteString(StringBuffer<>().Appendf(TC("   Last ping %s ago"), TimeToText(time - s->pingTime).str).data);
				totalUsed += s->usedSlotCount;
				totalSlots += s->processSlotCount;
			}
			WriteString(StringBuffer<>().Appendf(TC("Total remote slots used %u/%u"), totalUsed, totalSlots).data);
		}
		if (command.Equals(TCV("crashdump")))
		{
			WriteString(TC("Requesting crashdumps from all remotes on next ping"));
			SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, queueLock);
			for (auto& s : m_clientSessions)
				s->crashdump = true;
		}
		else if (command.StartsWith(TC("abort")))
		{
			bool abortWithProxy = command.Equals(TCV("abortproxy"));
			bool abortUseProxy = command.Equals(TCV("abortnonproxy"));
			if (!abortWithProxy && !abortUseProxy)
			{
				abortWithProxy = true;
				abortUseProxy = true;
			}
			SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, queueLock);
			u32 abortCount = 0;
			for (auto& s : m_clientSessions)
			{
				if (!s->enabled || s->abort)
					continue;
				bool hasProxy = m_storage.HasProxy(s->clientId);
				if (abortWithProxy && hasProxy)
					s->abort = true;
				else if (abortUseProxy && !hasProxy)
					s->abort = true;
				if (s->abort)
					++abortCount;
			}
			WriteString(StringBuffer<>().Appendf(TC("Aborting: %u remote sessions"), abortCount).data);
		}
		else if (command.Equals(TCV("disableremote")))
		{
			DisableRemoteExecution();
			WriteString(StringBuffer<>().Appendf(TC("Remote execution is disabled")).data);
		}
		else
		{
			WriteString(StringBuffer<>().Appendf(TC("Unknown command: %s"), command.data).data, LogEntryType_Error);
		}
		writer.WriteByte(255);
		return true;
	}

	bool SessionServer::HandleSHGetKnownFolderPath(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
#if PLATFORM_WINDOWS
		GUID kfid;
		reader.ReadBytes(&kfid, sizeof(GUID));
		u32 flags = reader.ReadU32();
		PWSTR str;

		static HMODULE moduleHandle = LoadLibrary(L"Shell32.dll");
		using SHGetKnownFolderPathFunc = HRESULT(const GUID& rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);
		static SHGetKnownFolderPathFunc* SHGetKnownFolderPath = (SHGetKnownFolderPathFunc*)GetProcAddress(moduleHandle, "SHGetKnownFolderPath");;
		HRESULT res = SHGetKnownFolderPath(kfid, flags, NULL, &str);
		writer.WriteU32(res);
		if (res == S_OK)
		{
			writer.WriteString(str);
			CoTaskMemFree(str);
		}
#endif
		return true;
	}

	bool SessionServer::StoreCasFile(CasKey& out, const StringKey& fileNameKey, const tchar* fileName)
	{
		CasKey casKeyOverride = CasKeyZero;

		bool deferCreation = true;
		{
			SCOPED_FUTEX(m_customCasKeysLock, lock);
			auto findIt = m_customCasKeys.find(fileNameKey);
			if (findIt != m_customCasKeys.end())
			{
				CustomCasKey& customKey = findIt->second;
				if (customKey.casKey == CasKeyZero)
				{
					if (!GetCasKeyFromTrackedInputs(customKey.casKey, fileName, customKey.workingDir.c_str(), customKey.trackedInputs.data(), u32(customKey.trackedInputs.size())))
						return false;
					UBA_ASSERTF(customKey.casKey != CasKeyZero, TC("This should never happen!!"));
					//m_logger.Debug(TC("Calculated custom key: %s (%s)"), GuidToString(customKey.casKey).str, fileName);
				}
				casKeyOverride = customKey.casKey;
			}
		}

		if (!m_storage.StoreCasFile(out, fileName, casKeyOverride, deferCreation)) // We can defer the creation of the cas file since client might already have it
			return false;
		return true;//out != CasKeyZero;
	}

	bool SessionServer::WriteDirectoryTable(ClientSession& session, BinaryReader& reader, BinaryWriter& writer)
	{
		auto& dirTable = m_directoryTable;

		SCOPED_FUTEX(session.dirTablePosLock, lock2);

		//m_logger.Info(TC("WritePos: %llu"), session.dirTablePos);
		writer.WriteU32(session.dirTablePos); // We can figure out on the other side if everything was written based on if the message is full or not.

		u32 toSend = GetDirectoryTableSize() - session.dirTablePos;
		if (toSend == 0)
			return true;

		u32 capacityLeft = u32(writer.GetCapacityLeft());
		if (capacityLeft < toSend)
			toSend = capacityLeft;

		writer.WriteBytes(dirTable.m_memory + session.dirTablePos, toSend);

		session.dirTablePos += toSend;
		return true;
	}

	bool SessionServer::WriteNameToHashTable(BinaryReader& reader, BinaryWriter& writer, u32 requestedSize)
	{
		u32 remoteTableSize = reader.ReadU32();
				
		u32 toSend = requestedSize - remoteTableSize;
		if (toSend == 0)
			return true;

		u32 capacityLeft = u32(writer.GetCapacityLeft());
		if (capacityLeft < toSend)
			toSend = capacityLeft;

		writer.WriteBytes(m_nameToHashTableMem.memory + remoteTableSize, toSend);
		return true;
	}

	void SessionServer::ThreadMemoryCheckLoop()
	{
		u64 lastMessageTime = 0;

		while (true)
		{
			if (m_memoryThreadEvent.IsSet(1000))
				break;

			#if 0
			SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, queueLock);
			m_logger.Info(TC("RemoteQueue: %llu Active: %llu ConnectionCount: %u"), m_queuedRemoteProcesses.size(), m_activeRemoteProcesses.size(), m_connectionCount);

			if (!m_connectionCount && !m_activeRemoteProcesses.empty())
			{
				for (auto& i : m_activeRemoteProcesses)
				{
					m_logger.Info(TC("ACTIVE PROCESS: %s"), i.GetStartInfo().GetDescription());
				}
				break;
			}
			#endif

			u64 memAvail;
			u64 memTotal;
			if (!GetMemoryInfo(memAvail, memTotal))
				m_memRequiredToSpawn = 0;
			m_memAvail = memAvail;

			bool allGood = false;
			while (memAvail >= m_memRequiredToSpawn)
			{
				SCOPED_FUTEX(m_waitingProcessesLock, lock);
				WaitingProcess* wp = m_oldestWaitingProcess;
				if (!wp)
				{
					allGood = true;
					break;
				}
				m_oldestWaitingProcess = wp->next;
				if (m_newestWaitingProcess == wp)
					m_newestWaitingProcess = nullptr;
				wp->event.Set();
				memAvail -= m_memRequiredToSpawn;
			}

			if (allGood)
				continue;

			u64 time = GetTime();
			if (TimeToMs(time - lastMessageTime) > 5*1000)
			{
				lastMessageTime = time;
				u32 delayCount = 0;
				SCOPED_FUTEX(m_waitingProcessesLock, lock);
				for (auto it = m_oldestWaitingProcess; it; it = it->next)
					++delayCount;
				lock.Leave();
				if (delayCount)
				{
					m_logger.BeginScope();
					m_logger.Info(TC("Delaying %u processes from spawning due to memory pressure (Available: %s Total: %s)"), delayCount, BytesToText(m_memAvail).str, BytesToText(m_memTotal).str);

#if PLATFORM_WINDOWS
					static bool hasBeenRunOnce;
					if (!hasBeenRunOnce)
					{
						hasBeenRunOnce = true;
						m_logger.Info(TC("NOTE - To mitigate this spawn delay it is recommended to make page file larger until you don't see these messages again (Or reduce number of max parallel processes)"));
						m_logger.Info(TC("       Set max page file to a large number (like 128gb). It will not use disk space unless you actually start using that amount of committed memory"));
						m_logger.Info(TC("       Also note, this is \"committed\" memory. Not memory in use. So you necessarily don't need more physical memory"));
						MEMORYSTATUSEX memStatus = { sizeof(memStatus) };
						GlobalMemoryStatusEx(&memStatus);
						m_logger.Info(TC("  MaxPage:   %s"), BytesToText(m_maxPageSize));
						m_logger.Info(TC("  TotalPhys: %s"), BytesToText(memStatus.ullTotalPhys));
						m_logger.Info(TC("  AvailPhys: %s"), BytesToText(memStatus.ullAvailPhys));
						m_logger.Info(TC("  TotalPage: %s"), BytesToText(memStatus.ullTotalPageFile));
						m_logger.Info(TC("  AvailPage: %s"), BytesToText(memStatus.ullAvailPageFile));
					}
#endif
					m_logger.EndScope();
				}
			}

			if (!m_allowKillOnMem)
				continue;

			// TODO: This code path is not implemented yet... the cancel need to end up in a Requeue call.
			UBA_ASSERT(false);

			u64 memRequiredFree = u64(double(memTotal) * double(100 - m_memKillLoadPercent) / 100.0);
			if (m_memAvail < memRequiredFree)
			{
				u64 newestTime = 0;
				ProcessImpl* newestProcess = nullptr;
				SCOPED_FUTEX(m_processesLock, lock);
				for (auto& kv : m_processes)
				{
					ProcessHandle& h = kv.second;
					if (h.IsRemote())
						continue;
					auto& p = *(ProcessImpl*)h.m_process;
					if (p.m_startTime <= newestTime)
						continue;
					newestTime = p.m_startTime;
					newestProcess = &p;
				}

				if (newestProcess)
				{
					newestProcess->Cancel(true);
					newestProcess->WaitForExit(3000);
				}

				m_logger.Info(TC("Killed process due to memory pressure (Available: %s Total: %s)"), BytesToText(m_memAvail).str, BytesToText(m_memTotal).str);
			}
		}

		SCOPED_FUTEX(m_waitingProcessesLock, lock);
		for (auto it = m_oldestWaitingProcess; it; it = it->next)
			it->event.Set();
		m_oldestWaitingProcess = nullptr;
		m_newestWaitingProcess = nullptr;
	}

	SessionServer::RemoteProcess* SessionServer::DequeueProcess(ClientSession& session, u32 sessionId, u32 clientId)
	{
		//TrackWorkScope tws(m_trace, AsView(TC("DequeueProcess")), ColorWork);
		SCOPED_READ_LOCK(m_remoteProcessSlotAvailableEventLock, lock);
		bool hasCalledCallback = !m_remoteProcessSlotAvailableEvent;

		while (true)
		{
			SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, queueLock);

			if (!session.connected) // This should not be possible
			{
				m_logger.Warning(TC("Dequeing process to session that is not connected. This should never happen. Report to Epic (%u)"), clientId);
				return nullptr;
			}

			while (!m_queuedRemoteProcesses.empty())
			{
				auto processHandle = m_queuedRemoteProcesses.front();
				auto process = (RemoteProcess*)processHandle.m_process;
				m_queuedRemoteProcesses.pop_front();
				if (process->m_cancelled)
					continue;
				
				if (session.enabled)
					--m_availableRemoteSlotCount;
				++session.usedSlotCount;

				process->m_clientId = clientId;
				process->m_sessionId = sessionId;
				process->m_executingHost = session.name;
				UBA_ASSERT(!process->m_cancelled);
				m_activeRemoteProcesses.insert(process);
				return process;
			}
			queueLock.Leave();

			if (hasCalledCallback)
				return nullptr;

			m_remoteProcessSlotAvailableEvent(IsArmBinary != session.isArm);
			hasCalledCallback = true;
		}
		return nullptr;
	}

	void SessionServer::OnCancelled(RemoteProcess* process)
	{
		ProcessHandle h(process);

		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, queueLock);
		process->m_server = nullptr;

		if (process->m_clientId == ~0u)
		{
			for (auto it=m_queuedRemoteProcesses.begin(); it!=m_queuedRemoteProcesses.end(); ++it)
			{
				if (it->m_process != process)
					continue;
				m_queuedRemoteProcesses.erase(it);
				break;
			}
		}
		else
		{
			u32 sessionIndex = process->m_sessionId - 1;
			UBA_ASSERT(sessionIndex < m_clientSessions.size());
			ClientSession& session = *m_clientSessions[sessionIndex];
			--session.usedSlotCount;
			
			m_activeRemoteProcesses.erase(process);

			{
				SCOPED_FUTEX(m_processesLock, lock);
				m_processes.erase(process->m_processId);
			}

			queueLock.Leave();

			StackBinaryWriter<1024> writer;
			ProcessStats().Write(writer);
			SessionStats().Write(writer);
			StorageStats().Write(writer);
			KernelStats().Write(writer);
			m_trace.ProcessExited(process->m_processId, process->m_exitCode, writer.GetData(), writer.GetPosition(), Vector<ProcessLogLine>());
		}

		process->m_done.Set();
	}

	ProcessHandle SessionServer::ProcessRemoved(u32 processId)
	{
		SCOPED_FUTEX(m_processesLock, lock);
		auto findIt = m_processes.find(processId);
		if (findIt == m_processes.end())
			return {};
		ProcessHandle h(findIt->second);
		m_processes.erase(findIt);
		return h;
	}

	ProcessHandle SessionServer::GetProcess(u32 processId)
	{
		SCOPED_FUTEX_READ(m_processesLock, lock);
		auto findIt = m_processes.find(processId);
		if (findIt == m_processes.end())
			return {};
		return ProcessHandle(findIt->second);
	}

	TString SessionServer::GetProcessDescription(u32 processId)
	{
		StringBuffer<512> str;
		SCOPED_FUTEX_READ(m_processesLock, lock);
		auto findIt = m_processes.find(processId);
		if (findIt == m_processes.end())
			return str.Appendf(TC("<Process with id %u not found>"), processId).data;
		return str.Appendf(TC("%s"), findIt->second.GetStartInfo().GetDescription()).data;
	}

	bool SessionServer::PrepareProcess(ProcessImpl& process, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir)
	{
		if (!Session::PrepareProcess(process, isChild, outRealApplication, outRealWorkingDir))
			return false;

		if (!m_memTotal || !m_allowWaitOnMem || isChild)
			return true;

		if (m_memAvail >= m_memRequiredToSpawn)
			return true;

		u64 startWait = GetTime();

		WaitingProcess wp;
		wp.event.Create(true);

		SCOPED_FUTEX(m_waitingProcessesLock, lock);
		if (m_memoryThreadEvent.IsSet(0))
			return false;

		if (!m_oldestWaitingProcess)
			m_oldestWaitingProcess = &wp;
		else
			m_newestWaitingProcess->next = &wp;
		m_newestWaitingProcess = &wp;
		lock.Leave();

		wp.event.IsSet();

		u64 waitTime = GetTime() - startWait;
		m_logger.Info(TC("Waited %s for memory pressure to go down (Available: %s Total: %s)"), TimeToText(waitTime).str, BytesToText(m_memAvail).str, BytesToText(m_memTotal).str);

		return true;
	}

	bool SessionServer::CreateFile(CreateFileResponse& out, const CreateFileMessage& msg)
	{
		if (!m_shouldWriteToDisk && ((msg.access & FileAccess_Write) == 0))
		{
			SCOPED_READ_LOCK(m_receivedFilesLock, lock);
			auto findIt = m_receivedFiles.find(msg.fileNameKey);
			if (findIt != m_receivedFiles.end())
			{
				u64 memoryMapAlignment = GetMemoryMapAlignment(msg.fileName);
				if (!memoryMapAlignment)
					memoryMapAlignment = 4096;
				MemoryMap map;
				if (!CreateMemoryMapFromView(map, msg.fileNameKey, msg.fileName.data, findIt->second, memoryMapAlignment))
					return false;
				out.directoryTableSize = GetDirectoryTableSize();
				out.mappedFileTableSize = GetFileMappingSize();
				out.fileName.Append(map.name);
				out.size = map.size;
				return true;
			}
		}
		return Session::CreateFile(out, msg);
	}

	void SessionServer::FileEntryAdded(StringKey fileNameKey, u64 lastWritten, u64 size)
	{
		SCOPED_WRITE_LOCK(m_nameToHashLookupLock, lock);

		if (!m_nameToHashInitialized)
			return;
		
		Storage::CachedFileInfo cachedInfo;
		if (!m_storage.VerifyAndGetCachedFileInfo(cachedInfo, fileNameKey, lastWritten, size))
			if (m_nameToHashLookup.find(fileNameKey) == m_nameToHashLookup.end())
				return;
		CasKey& lookupCasKey = m_nameToHashLookup[fileNameKey];
		if (lookupCasKey == cachedInfo.casKey)
			return;

		#if 0
		//m_debugLogger->Info(TC("NAMETOHASHADD    %s %s\n"), KeyToString(fileNameKey).data, CasKeyString(lookupCasKey).str);
		#endif

		lookupCasKey = cachedInfo.casKey;
		BinaryWriter w(m_nameToHashTableMem.memory, m_nameToHashTableMem.writtenSize, NameToHashMemSize);
		m_nameToHashTableMem.AllocateNoLock(sizeof(StringKey) + sizeof(CasKey), 1, TC("NameToHashTable"));
		w.WriteStringKey(fileNameKey);
		w.WriteCasKey(lookupCasKey);
	}

	bool SessionServer::RunSpecialProgram(ProcessImpl& process, BinaryReader& reader, BinaryWriter& writer)
	{
		TString application = reader.ReadString();
		TString cmdLine = reader.ReadLongString();
		TString workingDir = reader.ReadString();
		UBA_ASSERT(StringView(application).Contains(TCV("UbaCli.exe")));

		StringBuffer<> jsonFile;
		ParseArguments(cmdLine.data(), cmdLine.size(), [&](const tchar* arg, u32 argLen)
			{
				StringView sv(arg, argLen);
				if (sv.Contains(TCV(".json")))
					jsonFile.Append(workingDir).EnsureEndsWithSlash().Append(sv);
			});

		if (jsonFile.IsEmpty())
			return false;

		ProcessImpl* rootProcess = &process;
		while (rootProcess->m_parentProcess)
			rootProcess = rootProcess->m_parentProcess;
		auto& startInfo = rootProcess->GetStartInfo();
		UBA_ASSERTF(m_outerScheduler, TC("No outer scheduler set"));
		return m_outerScheduler->EnqueueFromSpecialJson(jsonFile.data, workingDir.c_str(), TC("UbaDistributor"), startInfo.rootsHandle, startInfo.userData);
	}

	void SessionServer::PrintSessionStats(Logger& logger)
	{
		Session::PrintSessionStats(logger);

		if (m_nameToHashLookup.size())
			logger.Info(TC("  NameToHashLookup    %7u %9s"), u32(m_nameToHashLookup.size()), BytesToText(m_nameToHashTableMem.writtenSize).str);
		logger.Info(TC("  Remote processes finished    %8u"), m_finishedRemoteProcessCount);
		logger.Info(TC("  Remote processes returned    %8u"), m_returnedRemoteProcessCount);
		logger.Info(TC(""));
	}

	void SessionServer::TraceSessionUpdate()
	{
		u32 sessionIndex = 1;

		u64 serverSend = m_server.GetTotalSentBytes();
		u64 serverRecv = m_server.GetTotalRecvBytes();

		SCOPED_CRITICAL_SECTION(m_remoteProcessAndSessionLock, lock);
		for (auto sptr : m_clientSessions)
		{
			auto& s = *sptr;
			NetworkServer::ClientStats stats;
			m_server.GetClientStats(stats, s.clientId);
			if (stats.connectionCount && (stats.send || stats.recv))
				m_trace.SessionUpdate(sessionIndex, stats.connectionCount, stats.send, stats.recv, s.lastPing, s.memAvail, s.memTotal, s.cpuLoad);
			++sessionIndex;
		}
		if (m_provider)
		{
			u64 send;
			u64 recv;
			m_provider(send, recv);
			serverSend += send;
			serverRecv += recv;
		}
		lock.Leave();

		float cpuLoad = UpdateCpuLoad();
		u64 memAvail = m_memAvail;
		u64 memTotal = m_memTotal;


		if (m_traceIOEnabled)
		{
			for (auto& volume : m_volumeCache.volumes)
			{
				if (volume.drives.empty())
					continue;
				u8 busyPercent;
				u32 readCount;
				u64 readBytes;
				u32 writeCount;
				u64 writeBytes;
				if (!volume.UpdateStats(busyPercent, readCount, readBytes, writeCount, writeBytes))
					continue;
				if (!busyPercent && !readCount && !readBytes && !writeCount && !writeBytes)
					continue;
				m_trace.DriveUpdate(volume.drives[0], busyPercent, readCount, readBytes, writeCount, writeBytes);
			}
		}

		m_trace.SessionUpdate(0, 0, serverSend, serverRecv, 0, memAvail, memTotal, cpuLoad);
	}

	void SessionServer::WriteRemoteEnvironmentVariables(BinaryWriter& writer)
	{
		if (!m_remoteEnvironmentVariables.empty())
		{
			writer.WriteBytes(m_remoteEnvironmentVariables.data(), m_remoteEnvironmentVariables.size());
			return;
		}

		u64 startPos = writer.GetPosition();

		#if PLATFORM_WINDOWS
		auto strs = GetEnvironmentStringsW();
		auto freeStrs = MakeGuard([strs]() { FreeEnvironmentStringsW(strs); });
		#else
		auto strs = (const char*)GetProcessEnvironmentVariables();
		#endif

		for (auto it = strs; *it; it += TStrlen(it) + 1)
		{
			StringBuffer<> varName;
			varName.Append(it, TStrchr(it, '=') - it);
			if (!varName.IsEmpty() && !varName.Equals(TCV("CL")) && !varName.Equals(TCV("_CL_")))
				if (m_localEnvironmentVariables.find(varName.data) == m_localEnvironmentVariables.end())
					writer.WriteString(it);
		}

		writer.WriteString(TC(""));
		
		u64 size = writer.GetPosition() - startPos;
		m_remoteEnvironmentVariables.resize(size);
		memcpy(m_remoteEnvironmentVariables.data(), writer.GetData() + startPos, size);
	}

	bool SessionServer::InitializeNameToHashTable()
	{
		if (!m_nameToHashTableEnabled || m_nameToHashInitialized)
			return true;

		SCOPED_WRITE_LOCK(m_nameToHashLookupLock, lock);
		m_nameToHashTableMem.Init(NameToHashMemSize);
		m_nameToHashInitialized = true;
		lock.Leave();

		auto& dirTable = m_directoryTable;

		{
			Vector<DirectoryTable::Directory*> dirs;
			SCOPED_READ_LOCK(dirTable.m_lookupLock, dirsLock);
			dirs.reserve(dirTable.m_lookup.size());
			for (auto& kv : dirTable.m_lookup)
				dirs.push_back(&kv.second);
			dirsLock.Leave();

			for (auto dirPtr : dirs)
			{
				DirectoryTable::Directory& dir = *dirPtr;
				SCOPED_READ_LOCK(dir.lock, dirLock);
				for (auto& fileKv : dir.files)
				{
					StringKey fileNameKey = fileKv.first;

					BinaryReader reader(dirTable.m_memory, fileKv.second);

					u64 lastWritten = reader.ReadU64();
					u32 attr = reader.ReadU32();
					if (IsDirectory(attr))
						continue;
					reader.Skip(sizeof(u32) + sizeof(u64));
					u64 size = reader.ReadU64();
					FileEntryAdded(fileNameKey, lastWritten, size);
				}
			}
		}
		SCOPED_WRITE_LOCK(m_nameToHashLookupLock, lock2);
		u64 entryCount = m_nameToHashLookup.size();
		lock2.Leave();

		m_logger.Debug(TC("Prepopulated NameToHash table with %u entries"), entryCount);

		return true;
	}

	bool SessionServer::HandleDebugFileNotFoundError(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
#if PLATFORM_WINDOWS
		StringBuffer<> errorPath;
		reader.ReadString(errorPath);
		StringBuffer<> workDir;
		reader.ReadString(workDir);

		StringView searchString = errorPath;
		if (searchString.data[0] == '.' && searchString.data[1] == '.')
		{
			searchString.data += 3;
			searchString.count -= 3;
		}

		auto LogLine = [&](const StringView& text) { m_logger.Log(LogEntryType_Warning, text.data, text.count); };

		// Make a copy of dir table since we can't populate it
		MemoryBlock block(64*1024*1024);
		DirectoryTable dirTable(block);
		u8* dirMem;
		u32 dirMemSize;
		{
			SCOPED_READ_LOCK(m_directoryTable.m_memoryLock, lock);
			dirMem = m_directoryTableMem;
			dirMemSize = m_directoryTable.m_memorySize;
		}

		dirTable.Init(dirMem, 0, dirMemSize);

		u32 foundCount = 0;
		dirTable.TraverseAllFilesNoLock([&](const DirectoryTable::EntryInformation& info, const StringBufferBase& path, u32 dirOffset)
			{
				if (!path.EndsWith(searchString))
					return;
				if (path[path.count - searchString.count - 1] != PathSeparator)
					return;

				auto ToString = [](bool b) { return b ? TC("true") : TC("false"); };

				++foundCount;
				StringBuffer<> logStr;
				logStr.Appendf(TC("File %s found in directory table at offset %u of %u while searching for matches for %s (File size %llu attr %u)"), path.data, dirOffset, dirTable.m_memorySize, searchString.data, info.size, info.attributes);
				LogLine(logStr);

				StringKey fileNameKey = ToStringKey(path);
				{
					SCOPED_FUTEX_READ(m_fileMappingTableLookupLock, mlock);
					auto findIt = m_fileMappingTableLookup.find(fileNameKey);
					if (findIt != m_fileMappingTableLookup.end())
					{
						auto& entry = findIt->second;
						SCOPED_FUTEX_READ(entry.lock, entryCs);
						logStr.Clear().Appendf(TC("File %s found in mapping table table."), path.data);
						if (entry.handled)
						{
							StringBuffer<128> mappingName;
							if (entry.mapping.IsValid())
								Storage::GetMappingString(mappingName, entry.mapping, entry.mappingOffset);
							else
								mappingName.Append(TCV("Not valid"));
							logStr.Appendf(TC(" Success: %s Size: %u IsDir: %s Mapping name: %s Mapping offset: %u"), ToString(entry.success), entry.size, ToString(entry.isDir), mappingName.data, entry.mappingOffset);
						}
						else
						{
							logStr.Appendf(TC(" Entry not handled"));
						}
					}
					else
						logStr.Clear().Appendf(TC("File %s not found in mapping table table."), path.data);
					LogLine(logStr);
				}
				{
					SCOPED_READ_LOCK(m_nameToHashLookupLock, hlock);
					auto findIt = m_nameToHashLookup.find(fileNameKey);
					if (findIt != m_nameToHashLookup.end())
						logStr.Clear().Appendf(TC("File %s found in name-to-hash lookup. CasKey is %s"), path.data, CasKeyString(findIt->second).str);
					else
						logStr.Clear().Appendf(TC("File %s not found in name-to-hash lookup"), path.data);
					LogLine(logStr);
				}
			});

		if (!foundCount)
		{
			StringBuffer<> logStr;
			logStr.Appendf(TC("No matching entry found in directory table while searching for matches for %s. DirTable size: %u"), searchString.data, GetDirectoryTableSize());
			LogLine(logStr);
			/*
			if (errorPath.StartsWith(TC("..\\Intermediate")))
			{
				StringBuffer<> fullPath;
				FixPath(errorPath.data, workDir.data, workDir.count, fullPath);
				GetFileAttributes
			}
			*/
		}
#endif
		return true;
	}

	bool SessionServer::HandleHostRun(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		return HostRun(reader, writer);
	}

	bool SessionServer::HandleGetSymbols(const ConnectionInfo& connectionInfo, const WorkContext& workContext, BinaryReader& reader, BinaryWriter& writer)
	{
		TString application = reader.ReadString();
		bool isClientArm = reader.ReadBool();

		if (IsArmBinary != isClientArm)
		{
			writer.WriteString(TC("Can't resolve callstack on cross architectures"));
			return true;
		}

		GetSymbols(application.c_str(), isClientArm, reader, writer);

		if constexpr (DownloadDebugSymbols)
		{
			CasKey detoursSymbolsKey;
			StringBuffer<> dir;
			if (GetDirectoryOfCurrentModule(m_logger, dir))
			{
				bool deferCreation = true;
				auto ChangeToSymbolExtension = [](StringBufferBase& str) -> StringBufferBase& { IsWindows ? str.Resize(str.count - 3).Append("pdb") : str.Resize(str.count - 2).Append("debug"); return str; };
				if (!m_storage.StoreCasFile(detoursSymbolsKey, ChangeToSymbolExtension(dir).data, CasKeyZero, deferCreation) || detoursSymbolsKey == CasKeyZero)
				{
					StringBuffer<> dir2;
					if (GetAlternativeUbaPath(m_logger, dir2, dir, IsWindows && isClientArm))
						m_storage.StoreCasFile(detoursSymbolsKey, ChangeToSymbolExtension(dir2.Append(UBA_DETOURS_LIBRARY)).data, CasKeyZero, deferCreation);
				}
			}
			writer.WriteCasKey(detoursSymbolsKey);
		}

		return true;
	}
}

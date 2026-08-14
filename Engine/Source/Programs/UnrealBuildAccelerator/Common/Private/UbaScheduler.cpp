// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaScheduler.h"
#include "UbaApplicationRules.h"
#include "UbaCacheClient.h"
#include "UbaConfig.h"
#include "UbaNetworkServer.h"
#include "UbaProcess.h"
#include "UbaProcessStartInfoHolder.h"
#include "UbaRootPaths.h"
#include "UbaSessionServer.h"
#include "UbaStringBuffer.h"

namespace uba
{
	struct Scheduler::ProcessStartInfo2 : ProcessStartInfoHolder
	{
		ProcessStartInfo2(const ProcessStartInfo& si, const u8* ki, u32 kic)
		:	ProcessStartInfoHolder(si)
		, knownInputs(ki)
		, knownInputsCount(kic)
		{
		}

		~ProcessStartInfo2()
		{
			delete[] knownInputs;
		}

		const u8* knownInputs;
		u32 knownInputsCount;
		float weight = 1.0f;
		u32 cacheBucketId = 0;
	};


	struct Scheduler::ExitProcessInfo
	{
		Scheduler* scheduler = nullptr;
		ProcessStartInfo2* startInfo;
		u32 processIndex = ~0u;
		bool wasReturned = false;
		bool isLocal = true;
	};


	class SkippedProcess : public Process
	{
	public:
		SkippedProcess(const ProcessStartInfo& i) : startInfo(i) {}
		virtual u32 GetExitCode() override { return ProcessCancelExitCode; }
		virtual bool HasExited() override { return true; }
		virtual bool WaitForExit(u32 millisecondsTimeout) override{ return true; }
		virtual const ProcessStartInfo& GetStartInfo() const override { return startInfo; }
		virtual const Vector<ProcessLogLine>& GetLogLines() const override { static Vector<ProcessLogLine> v{ProcessLogLine{TC("Skipped"), LogEntryType_Warning}}; return v; }
		virtual const Vector<u8>& GetTrackedInputs() const override { static Vector<u8> v; return v;}
		virtual const Vector<u8>& GetTrackedOutputs() const override { static Vector<u8> v; return v;}
		virtual bool IsRemote() const override { return false; }
		virtual bool IsCache() const { return false; }
		virtual ProcessExecutionType GetExecutionType() const override { return ProcessExecutionType_Skipped; }
		ProcessStartInfoHolder startInfo;
	};

	class CachedProcess : public Process
	{
	public:
		CachedProcess(const ProcessStartInfo& i) : startInfo(i) {}
		virtual u32 GetExitCode() override { return 0; }
		virtual bool HasExited() override { return true; }
		virtual bool WaitForExit(u32 millisecondsTimeout) override{ return true; }
		virtual const ProcessStartInfo& GetStartInfo() const override { return startInfo; }
		virtual const Vector<ProcessLogLine>& GetLogLines() const override { return logLines; }
		virtual const Vector<u8>& GetTrackedInputs() const override { static Vector<u8> v; return v;}
		virtual const Vector<u8>& GetTrackedOutputs() const override { static Vector<u8> v; return v;}
		virtual bool IsRemote() const override { return false; }
		virtual bool IsCache() const { return false; }
		virtual ProcessExecutionType GetExecutionType() const override { return ProcessExecutionType_FromCache; }
		ProcessStartInfoHolder startInfo;
		Vector<ProcessLogLine> logLines;
	};

	void SchedulerCreateInfo::Apply(const Config& config)
	{
		if (const ConfigTable* table = config.GetTable(TC("Scheduler")))
		{
			table->GetValueAsBool(enableProcessReuse, TC("EnableProcessReuse"));
			table->GetValueAsBool(forceRemote, TC("ForceRemote"));
			table->GetValueAsBool(forceNative, TC("ForceNative"));
			table->GetValueAsU32(maxLocalProcessors, TC("MaxLocalProcessors"));
			table->GetValueAsU32(maxParallelCacheQueries, TC("MaxParallelCacheQueries"));
		}
	}


	Scheduler::Scheduler(const SchedulerCreateInfo& info)
	:	m_session(info.session)
	,	m_maxLocalProcessors(info.maxLocalProcessors != ~0u ? info.maxLocalProcessors : GetLogicalProcessorCount())
	,	m_maxParallelCacheQueries(info.maxParallelCacheQueries)
	,	m_updateThreadLoop(false)
	,	m_enableProcessReuse(info.enableProcessReuse)
	,	m_forceRemote(info.forceRemote)
	,	m_forceNative(info.forceNative)
	,	m_processConfigs(info.processConfigs)
	,	m_writeToCache(info.writeToCache && info.cacheClientCount)
	{
		m_cacheClients.insert(m_cacheClients.end(), info.cacheClients, info.cacheClients + info.cacheClientCount);
		m_session.RegisterGetNextProcess([this](Process& process, NextProcessInfo& outNextProcess, u32 prevExitCode)
			{
				return HandleReuseMessage(process, outNextProcess, prevExitCode);
			});

		m_session.SetOuterScheduler(this);
	}

	Scheduler::~Scheduler()
	{
		Stop();
		for (auto rt : m_rootPaths)
			delete rt;
	}

	void Scheduler::Start()
	{
		m_session.SetRemoteProcessReturnedEvent([this](Process& process) { RemoteProcessReturned(process); });
		m_session.SetRemoteProcessSlotAvailableEvent([this](bool isCrossArchitecture) { RemoteSlotAvailable(isCrossArchitecture); });

		m_loop = true;
		m_thread.Start([this]() { ThreadLoop(); return 0; }, TC("UbaSchedLoop"));
	}

	void Scheduler::Stop()
	{
		m_loop = false;
		m_updateThreadLoop.Set();
		m_thread.Wait();
		m_cacheDownloadThreads.clear();
		m_session.WaitOnAllTasks();
		SkipAllQueued();
		Cleanup();
	}

	void Scheduler::Cancel()
	{
		m_enableProcessReuse = false;
		SkipAllQueued();
		m_session.CancelAllProcesses();
	}

	void Scheduler::SkipAllQueued()
	{
		Vector<ProcessStartInfo2*> skipped;
		SCOPED_FUTEX(m_processEntriesLock, lock);
		for (auto& entry : m_processEntries)
			if (entry.status == ProcessStatus_QueuedForCache || entry.status == ProcessStatus_QueuedForRun)
			{
				entry.status = ProcessStatus_Skipped;
				skipped.push_back(entry.info);
			}
		lock.Leave();
		for (auto pi : skipped)
			SkipProcess(*pi);
	}

	void Scheduler::Cleanup()
	{
		SCOPED_FUTEX(m_processEntriesLock, lock);
		for (auto& entry : m_processEntries)
		{
			UBA_ASSERTF(entry.status > ProcessStatus_Running, TC("Found processes in queue/running state when stopping scheduler."));
			delete[] entry.dependencies;
			delete entry.info;
		}
		m_processEntries.clear();
		m_processEntriesStart = 0;
		m_session.SetOuterScheduler(nullptr);
	}

	void Scheduler::SetMaxLocalProcessors(u32 maxLocalProcessors)
	{
		m_maxLocalProcessors = maxLocalProcessors;
		m_updateThreadLoop.Set();
	}

	void Scheduler::SetAllowDisableRemoteExecution(bool allow)
	{
		m_allowDisableRemoteExecution = allow;
	}

	u32 Scheduler::EnqueueProcess(const EnqueueProcessInfo& info)
	{
		u8* ki = nullptr;
		if (info.knownInputsCount)
		{
			ki = new u8[info.knownInputsBytes];
			memcpy(ki, info.knownInputs, info.knownInputsBytes);
		}

		u32* dep = nullptr;
		if (info.dependencyCount)
		{
			dep = new u32[info.dependencyCount];
			memcpy(dep, info.dependencies, info.dependencyCount*sizeof(u32));
		}

		auto info2 = new ProcessStartInfo2(info.info, ki, info.knownInputsCount);
		info2->Expand();
		info2->weight = info.weight;
		info2->cacheBucketId = info.cacheBucketId;

		const ApplicationRules* rules = m_session.GetRules(*info2);
		info2->rules = rules;

		bool useCache = info.cacheBucketId && !m_cacheClients.empty() && rules->IsCacheable();
		bool canDetour = info.canDetour;
		bool canExecuteRemotely = info.canExecuteRemotely && info.canDetour;

		if (m_processConfigs)
		{
			auto name = info2->application;
			if (auto lastSeparator = TStrrchr(name, PathSeparator))
				name = lastSeparator + 1;
			StringBuffer<128> lower(name);
			lower.MakeLower();
			lower.Replace('.', '_');
			if (const ConfigTable* processConfig = m_processConfigs->GetTable(lower.data))
			{
				processConfig->GetValueAsBool(canExecuteRemotely, TC("CanExecuteRemotely"));
				processConfig->GetValueAsBool(canDetour, TC("CanDetour"));
			}
		}

		SCOPED_FUTEX(m_processEntriesLock, lock);
		u32 index = u32(m_processEntries.size());
		auto& entry = m_processEntries.emplace_back();
		entry.info = info2;
		entry.dependencies = dep;
		entry.dependencyCount = info.dependencyCount;
		entry.status = useCache ? ProcessStatus_QueuedForCache : ProcessStatus_QueuedForRun;
		entry.canDetour = canDetour;
		entry.canExecuteRemotely = canExecuteRemotely;
		lock.Leave();

		++m_totalProcesses;

		UpdateQueueCounter(1);

		m_updateThreadLoop.Set();
		return index;
	}

	void Scheduler::GetStats(u32& outQueued, u32& outActiveLocal, u32& outActiveRemote, u32& outFinished)
	{
		outActiveLocal = m_activeLocalProcesses;
		outActiveRemote = m_activeRemoteProcesses;
		outFinished = m_finishedProcesses;
		outQueued = m_queuedProcesses;
	}

	bool Scheduler::IsEmpty()
	{
		SCOPED_FUTEX_READ(m_processEntriesLock, lock);
		return m_processEntries.size() <= m_finishedProcesses;
	}

	void Scheduler::SetProcessFinishedCallback(const Function<void(const ProcessHandle&)>& processFinished)
	{
		m_processFinished = processFinished;
	}

	u32 Scheduler::GetProcessCountThatCanRunRemotelyNow()
	{
		if (m_session.IsRemoteExecutionDisabled())
			return 0;

		u32 count = 0;
		SCOPED_FUTEX_READ(m_processEntriesLock, lock);
		for (auto& entry : m_processEntries)
		{
			if (!entry.canExecuteRemotely)
				continue;
			if (entry.status != ProcessStatus_QueuedForRun)
				continue;
			++count;
		}

		count += m_activeRemoteProcesses;

		return count;
	}

	float Scheduler::GetProcessWeightThatCanRunRemotelyNow()
	{
		if (m_session.IsRemoteExecutionDisabled())
			return 0;

		float weight = 0;
		SCOPED_FUTEX_READ(m_processEntriesLock, lock);
		for (auto& entry : m_processEntries)
		{
			if (!entry.canExecuteRemotely)
				continue;
			if (entry.status != ProcessStatus_QueuedForRun)
				continue;
			bool canRun = true;
			for (u32 j=0, je=entry.dependencyCount; j!=je; ++j)
			{
				auto depIndex = entry.dependencies[j];
				if (m_processEntries[depIndex].status == ProcessStatus_Success)
					continue;
				canRun = false;
				break;
			}
			if (!canRun)
				continue;
			weight += entry.info->weight;
		}
		return weight;
	}

	void Scheduler::ThreadLoop()
	{
		while (m_loop)
		{
			if (!m_updateThreadLoop.IsSet())
				break;

			while (RunQueuedProcess(true))
				;
		}
	}

	void Scheduler::RemoteProcessReturned(Process& process)
	{
		auto& ei = *(ExitProcessInfo*)process.GetStartInfo().userData;

		ei.wasReturned = true;
		u32 processIndex = ei.processIndex;

		process.Cancel(true); // Cancel will call ProcessExited
		
		if (processIndex == ~0u)
			return;

		SCOPED_FUTEX(m_processEntriesLock, lock);
		if (m_processEntries[processIndex].status != ProcessStatus_Running)
			return;
		m_processEntries[processIndex].status = ProcessStatus_QueuedForRun;
		m_processEntriesStart = Min(m_processEntriesStart, processIndex);
		lock.Leave();

		UpdateQueueCounter(1);
		UpdateActiveProcessCounter(false, -1);
		m_updateThreadLoop.Set();
	}

	void Scheduler::HandleCacheMissed(u32 processIndex)
	{
		SCOPED_FUTEX(m_processEntriesLock, lock);
		UBA_ASSERT(m_processEntries[processIndex].status == ProcessStatus_DownloadingFromCache);
		m_processEntries[processIndex].status = ProcessStatus_QueuedForRun;
		m_processEntriesStart = Min(m_processEntriesStart, processIndex);
		--m_activeCacheQueries;
		lock.Leave();

		UpdateQueueCounter(1);
		UpdateActiveProcessCounter(false, -1);
		m_updateThreadLoop.Set();
	}

	void Scheduler::RemoteSlotAvailable(bool isCrossArchitecture)
	{
		UBA_ASSERTF(!isCrossArchitecture, TC("Cross architecture code path not implemented"));
		if (RunQueuedProcess(false))
			return;
		if (!m_allowDisableRemoteExecution)
			return;
		if (m_session.IsRemoteExecutionDisabled())
			return;
		u32 count = 0;
		SCOPED_FUTEX_READ(m_processEntriesLock, lock);
		for (auto& entry : m_processEntries)
			count += u32(entry.canExecuteRemotely && entry.status <= ProcessStatus_QueuedForRun);
		lock.Leave();
		if (count < m_maxLocalProcessors)
			m_session.DisableRemoteExecution();
		else
			m_session.SetMaxRemoteProcessCount(count);
	}

	void Scheduler::ProcessExited(ExitProcessInfo* info, const ProcessHandle& handle)
	{
		auto ig = MakeGuard([info]() { delete info; });

		if (info->wasReturned)
			return;

		auto si = info->startInfo;
		if (!si) // Can be a process that was reused but didn't get a new process
		{
			UBA_ASSERT(info->processIndex == ~0u);
			return;
		}

		ExitProcess(*info, *handle.m_process, handle.m_process->GetExitCode(), false);
	}

	u32 Scheduler::PopProcess(bool isLocal, ProcessStatus& outPrevStatus)
	{
		bool atMaxLocalWeight = m_activeLocalProcessWeight >= float(m_maxLocalProcessors);
		auto processEntries = m_processEntries.data();
		bool allFinished = true;

		for (u32 i=m_processEntriesStart, e=u32(m_processEntries.size()); i!=e; ++i)
		{
			auto& entry = processEntries[i];
			auto status = entry.status;

			// Check if we can move search starting point forward
			if (status != ProcessStatus_QueuedForCache && status != ProcessStatus_QueuedForRun)
			{
				if (allFinished)
				{
					if (status != ProcessStatus_Running && status != ProcessStatus_DownloadingFromCache)
						m_processEntriesStart = i;
					else
						allFinished = false;
				}
				continue;
			}
			allFinished = false;

			
			if (status == ProcessStatus_DownloadingFromCache)
				continue;

			if (status == ProcessStatus_QueuedForCache)
			{
				if (m_activeCacheQueries >= m_maxParallelCacheQueries)
					continue;
			}
			else if (isLocal)
			{
				if (m_forceRemote && entry.canExecuteRemotely)
					continue;
				if (status == ProcessStatus_QueuedForRun && atMaxLocalWeight)
					continue;
			}
			else
			{
				if (!entry.canExecuteRemotely)
					continue;
			}

			bool canRun = true;
			for (u32 j=0, je=entry.dependencyCount; j!=je; ++j)
			{
				auto depIndex = entry.dependencies[j];
				if (depIndex >= m_processEntries.size())
				{
					m_session.GetLogger().Error(TC("Found dependency on index %u but there are only %u processes registered"), depIndex, u32(m_processEntries.size()));
					return ~0u;
				}
				auto depStatus = processEntries[depIndex].status;
				if (depStatus == ProcessStatus_Failed || depStatus == ProcessStatus_Skipped)
				{
					entry.status = ProcessStatus_Skipped;
					return i;
				}
				if (depStatus != ProcessStatus_Success)
				{
					canRun = false;
					break;
				}
			}

			if (!canRun)
				continue;

			if (status != ProcessStatus_QueuedForCache)
			{
				if (isLocal)
					m_activeLocalProcessWeight += entry.info->weight;

				outPrevStatus = entry.status;
				entry.status = ProcessStatus_Running;
				return i;
			}

			// Clear out existing download threads if they're done
			FlushFinishedDownloadThreads();

			++m_activeCacheQueries;

			// Start download
			entry.status = ProcessStatus_DownloadingFromCache;

			m_cacheDownloadThreads.emplace_back([this, processIndex = i, info = entry.info]()
				{
					ProcessStartInfo2& si = *info;

					CacheResult cacheResult;
					bool isHit = false;
					for (auto& cacheClient : m_cacheClients)
					{
						if (!cacheClient->FetchFromCache(cacheResult, si.rootsHandle, si.cacheBucketId, si) || !cacheResult.hit)
							continue;

						isHit = true;
						auto process = new CachedProcess(si);
						process->logLines.swap(cacheResult.logLines);
						ProcessHandle ph(process);
						ExitProcessInfo exitInfo;
						exitInfo.scheduler = this;
						exitInfo.startInfo = info;
						exitInfo.isLocal = true;
						exitInfo.processIndex = processIndex;
						ExitProcess(exitInfo, *process, 0, true);
						++m_cacheHitCount;
						break;
					}

					if (!isHit)
					{
						HandleCacheMissed(processIndex);
						++m_cacheMissCount;
					}

					StringBuffer<> str;
					str.Appendf(TC("Hits %u Misses %u"), m_cacheHitCount.load(), m_cacheMissCount.load());
					m_session.GetTrace().StatusUpdate(1, 6, str.data, LogEntryType_Info, nullptr);
					return 0;

				}, TC("UbaScheDlCach"));
			continue;
		}

		FlushFinishedDownloadThreads();
		return ~0u;
	}


	bool Scheduler::RunQueuedProcess(bool isLocal)
	{
		while (true)
		{
			ProcessStatus prevStatus;
			SCOPED_FUTEX(m_processEntriesLock, lock);
			u32 indexToRun = PopProcess(isLocal, prevStatus);
			if (indexToRun == ~0u)
				return false;

			auto& processEntry = m_processEntries[indexToRun];
			auto info = processEntry.info;
			bool canDetour = processEntry.canDetour && !m_forceNative;
			bool wasSkipped = processEntry.status == ProcessStatus_Skipped;
			lock.Leave();

			UpdateQueueCounter(-1);

			if (wasSkipped)
			{
				SkipProcess(*info);
				continue;
			}

			UpdateActiveProcessCounter(isLocal, 1);
	
			auto exitInfo = new ExitProcessInfo();
			exitInfo->scheduler = this;
			exitInfo->startInfo = info;
			exitInfo->isLocal = isLocal;
			exitInfo->processIndex = indexToRun;

			ProcessStartInfo si = *info;
			si.userData = exitInfo;
			//si.trackInputs = m_writeToCache && info->cacheBucketId && !m_cacheClients.empty() && info->rules->IsCacheable();
			si.exitedFunc = [](void* userData, const ProcessHandle& handle, ProcessExitedResponse& response)
				{
					auto ei = (ExitProcessInfo*)userData;
					ei->scheduler->ProcessExited(ei, handle);
				};
			UBA_ASSERT(si.rules);

			if (isLocal)
				m_session.RunProcess(si, true, canDetour);
			else
				m_session.RunProcessRemote(si, info->weight, info->knownInputs, info->knownInputsCount);
			return true;
		}
	}

	bool Scheduler::HandleReuseMessage(Process& process, NextProcessInfo& outNextProcess, u32 prevExitCode)
	{
		if (!m_enableProcessReuse)
			return false;

		auto& currentStartInfo = process.GetStartInfo();
		auto ei = (ExitProcessInfo*)currentStartInfo.userData;
		if (!ei) // If null, process has already exited from some other thread
			return false;

		ExitProcess(*ei, process, prevExitCode, false);

		ei->startInfo = nullptr;
		ei->processIndex = ~0u;
		if (ei->wasReturned)
			return false;

		bool isLocal = !process.IsRemote();

		while (true)
		{
			ProcessStatus prevStatus;
			SCOPED_FUTEX(m_processEntriesLock, lock);
			u32 indexToRun = PopProcess(isLocal, prevStatus);
			if (indexToRun == ~0u)
				return false;
			UBA_ASSERT(prevStatus != ProcessStatus_QueuedForCache);
			auto& processEntry = m_processEntries[indexToRun];
			auto newInfo = processEntry.info;
			bool wasSkipped = processEntry.status == ProcessStatus_Skipped;
			lock.Leave();

			UpdateQueueCounter(-1);

			if (wasSkipped)
			{
				SkipProcess(*newInfo);
				continue;
			}

			UpdateActiveProcessCounter(isLocal, 1);

			ei->startInfo = newInfo;
			ei->processIndex = indexToRun;

			auto& si = *newInfo;
			outNextProcess.arguments = si.arguments;
			outNextProcess.workingDir = si.workingDir;
			outNextProcess.description = si.description;
			outNextProcess.logFile = si.logFile;
			outNextProcess.breadcrumbs = si.breadcrumbs;

			#if UBA_DEBUG
			auto PrepPath = [this](StringBufferBase& out, const ProcessStartInfo& psi)
				{
					if (IsAbsolutePath(psi.application))
						FixPath(psi.application, nullptr, 0, out);
					else
						SearchPathForFile(m_session.GetLogger(), out, psi.application, ToView(psi.workingDir), {});
				};
			StringBuffer<> temp1;
			StringBuffer<> temp2;
			PrepPath(temp1, currentStartInfo);
			PrepPath(temp2, si);
			UBA_ASSERTF(temp1.Equals(temp2.data), TC("%s vs %s"), temp1.data, temp2.data);
			#endif
			
			return true;
		}
	}

	void Scheduler::ExitProcess(ExitProcessInfo& info, Process& process, u32 exitCode, bool fromCache)
	{
		auto si = info.startInfo;
		if (!si)
			return;

		ProcessHandle ph;
		ph.m_process = &process;

		ProcessExitedResponse exitedResponse = ProcessExitedResponse_None;
		if (auto func = si->exitedFunc)
			func(si->userData, ph, exitedResponse);

		bool isDone = exitedResponse == ProcessExitedResponse_None;

		SCOPED_FUTEX(m_processEntriesLock, lock);
		auto& entry = m_processEntries[info.processIndex];

		u32* dependencies = entry.dependencies;

		if (isDone)
		{
			entry.status = exitCode == 0 ? ProcessStatus_Success : ProcessStatus_Failed;
			entry.info = nullptr;
			entry.dependencies = nullptr;
		}
		else
		{
			entry.canExecuteRemotely = false;
			entry.canDetour = exitedResponse != ProcessExitedResponse_RerunNative;
			entry.status = ProcessStatus_QueuedForRun;
			m_processEntriesStart = Min(m_processEntriesStart, info.processIndex);
		}

		if (info.isLocal)
		{
			if (fromCache)
				--m_activeCacheQueries;
			else
				m_activeLocalProcessWeight -= si->weight;
		}

		lock.Leave();

		UpdateActiveProcessCounter(info.isLocal, -1);
		m_updateThreadLoop.Set();

		if (isDone)
		{
			if (exitCode != 0)
				++m_errorCount;

			FinishProcess(ph);
			delete[] dependencies;
			delete si;
		}

		if (m_writeToCache && exitCode == 0)
		{
			// TODO: Read dep.json file
			UBA_ASSERTF(false, TC("Not implemented"));
		}

		ph.m_process = nullptr;
	}

	void Scheduler::SkipProcess(ProcessStartInfo2& info)
	{
		ProcessHandle ph(new SkippedProcess(info));
		ProcessExitedResponse exitedResponse = ProcessExitedResponse_None;
		if (auto func = info.exitedFunc)
			func(info.userData, ph, exitedResponse);
		UBA_ASSERT(exitedResponse == ProcessExitedResponse_None);
		FinishProcess(ph);
	}

	void Scheduler::UpdateQueueCounter(int offset)
	{
		m_queuedProcesses += u32(offset);
		m_session.UpdateProgress(m_totalProcesses, m_finishedProcesses, m_errorCount);
	}

	void Scheduler::UpdateActiveProcessCounter(bool isLocal, int offset)
	{
		if (isLocal)
			m_activeLocalProcesses += u32(offset);
		else
			m_activeRemoteProcesses += u32(offset);
	}

	void Scheduler::FinishProcess(const ProcessHandle& handle)
	{
		if (m_processFinished)
			m_processFinished(handle);
		++m_finishedProcesses;
		m_session.UpdateProgress(m_totalProcesses, m_finishedProcesses, m_errorCount);
	}

	void Scheduler::FlushFinishedDownloadThreads()
	{
		while (!m_cacheDownloadThreads.empty())
		{
			if (!m_cacheDownloadThreads.front().Wait(0))
				break;
			m_cacheDownloadThreads.pop_front();
		}
	}

	bool Scheduler::EnqueueFromFile(const tchar* yamlFilename, const Function<void(EnqueueProcessInfo&)>& enqueued)
	{
		auto& logger = m_session.GetLogger();

		TString app;
		TString arg;
		TString dir;
		TString desc;
		bool allowDetour = true;
		bool allowRemote = true;
		float weight = 1.0f;
		Vector<u32> deps;

		ProcessStartInfo si;

		auto enqueueProcess = [&]()
			{
				si.application = app.c_str();
				si.arguments = arg.c_str();
				si.workingDir = dir.c_str();
				si.description = desc.c_str();

				#if UBA_DEBUG
				StringBuffer<> logFile;
				if (true)
				{
					static u32 processId = 1; // TODO: This should be done in a better way.. or not at all?
					GenerateNameForProcess(logFile, si.arguments, ++processId);
					logFile.Append(TCV(".log"));
					si.logFile = logFile.data;
				};
				#endif

				EnqueueProcessInfo info { si };
				info.dependencies = deps.data();
				info.dependencyCount = u32(deps.size());
				info.canDetour = allowDetour;
				info.canExecuteRemotely = allowRemote;
				info.weight = weight;
				if (enqueued)
					enqueued(info);
				EnqueueProcess(info);
				app.clear();
				arg.clear();
				dir.clear();
				desc.clear();
				deps.clear();
				allowDetour = true;
				allowRemote = true;
				weight = 1.0f;
			};

		enum InsideArray
		{
			InsideArray_None,
			InsideArray_CacheRoots,
			InsideArray_Processes,
		};

		InsideArray insideArray = InsideArray_None;

		auto readLine = [&](const TString& line)
			{
				const tchar* keyStart = line.c_str();
				while (*keyStart && *keyStart == ' ')
					++keyStart;
				if (!*keyStart)
					return true;
				u32 indentation = u32(keyStart - line.c_str());

				if (insideArray != InsideArray_None && !indentation)
					insideArray = InsideArray_None;

				StringBuffer<32> key;
				const tchar* valueStart = nullptr;

				if (*keyStart == '-')
				{
					UBA_ASSERT(insideArray != InsideArray_None);
					valueStart = keyStart + 2;
				}
				else
				{
					const tchar* colon = TStrchr(keyStart, ':');
					if (!colon)
						return false;
					key.Append(keyStart, colon - keyStart);
					valueStart = colon + 1;
					while (*valueStart && *valueStart == ' ')
						++valueStart;
				}

				switch (insideArray)
				{
				case InsideArray_None:
				{
					if (key.Equals(TCV("environment")))
					{
						#if PLATFORM_WINDOWS
						SetEnvironmentVariable(TC("PATH"), valueStart);
						#endif
						return true;
					}
					if (key.Equals(TCV("cacheroots")))
					{
						insideArray = InsideArray_CacheRoots;
						return true;
					}
					if (key.Equals(TCV("processes")))
					{
						insideArray = InsideArray_Processes;
						return true;
					}
					return true;
				}
				case InsideArray_CacheRoots:
				{
					auto& rootPaths = *m_rootPaths.emplace_back(new RootPaths());
					if (Equals(valueStart, TC("SystemRoots")))
						rootPaths.RegisterSystemRoots(logger);
					else
						rootPaths.RegisterRoot(logger, valueStart);
					return true;
				}
				case InsideArray_Processes:
				{
					if (*keyStart == '-')
					{
						keyStart += 2;
						if (!app.empty())
							enqueueProcess();
					}

					if (key.Equals(TCV("app")))
						app = valueStart;
					else if (key.Equals(TCV("arg")))
						arg = valueStart;
					else if (key.Equals(TCV("dir")))
						dir = valueStart;
					else if (key.Equals(TCV("desc")))
						desc = valueStart;
					else if (key.Equals(TCV("detour")))
						allowDetour = !Equals(valueStart, TC("false"));
					else if (key.Equals(TCV("remote")))
						allowRemote = !Equals(valueStart, TC("false"));
					else if (key.Equals(TCV("weight")))
						StringBuffer<32>(valueStart).Parse(weight);
					else if (key.Equals(TCV("dep")))
					{
						const tchar* depStart = TStrchr(valueStart, '[');
						if (!depStart)
							return false;
						++depStart;
						StringBuffer<32> depStr;
						for (const tchar* it = depStart; *it; ++it)
						{
							if (*it != ']' && *it != ',')
							{
								if (*it != ' ')
									depStr.Append(*it);
								continue;
							}
							u32 depIndex;
							if (!depStr.Parse(depIndex))
								return false;
							depStr.Clear();
							deps.push_back(depIndex);

							if (!*it)
								break;
							depStart = it + 1;
						}
					}
					return true;
				}
				}
				return true;
			};

		if (!ReadLines(logger, yamlFilename, readLine))
			return false;

		if (!app.empty())
			enqueueProcess();

		return true;
	}

	bool Scheduler::EnqueueFromSpecialJson(const tchar* jsonFilename, const tchar* workingDir, const tchar* description, RootsHandle rootsHandle, void* userData)
	{
		Logger& logger = m_session.GetLogger();
		FileAccessor fa(logger, jsonFilename);
		if (!fa.OpenMemoryRead())
			return false;

		auto data = (const char*)fa.GetData();
		u64 dataLen = fa.GetSize();
		auto i = data;
		auto e = data + dataLen;
		u32 scope = 0;
		const char* stringStart = nullptr;
		std::string lastString;
		char lastChar = 0;

		struct Command { TString application; TString arguments; };
		Vector<Command> commands;

		while (i != e)
		{
			if (!stringStart)
			{
				if (*i == '{')
				{
					++scope;
				}
				else if (*i == '}')
				{
					--scope;
				}
				else if (*i == '\"' && lastChar != '\\')
				{
					stringStart = i+1;
				}
			}
			else
			{
				if (*i == '\"' && lastChar != '\\')
				{
					if (lastString == "command")
					{
						Command& command = commands.emplace_back();
						StringBuffer<2048> args;
						ParseArguments(stringStart, int(i - stringStart), [&](char* arg, u32 argLen)
						{
							// Strip out double backslash
							char* readIt = arg;
							char* writeIt = arg;
							char last = 0;
							while (true)
							{
								char c = *readIt;
								*writeIt = c;
								if (!(c == '\\' && last == '\\'))
									++writeIt;
								if (c == 0)
									break;
								++readIt;
								last = c;
							};
							argLen = u32(writeIt - arg);

							if (command.application.empty())
							{
								command.application.assign(arg, arg + argLen);
								return;
							}
							if (args.count)
								args.Append(' ');
							args.Append(arg, argLen);
						});
						command.arguments = args.ToString();
					}
					lastString.assign(stringStart, int(i - stringStart));
					stringStart = nullptr;
				}
			}
			lastChar = *i;
			++i;
		}
		UBA_ASSERT(scope == 0);

		float weight = 0;
		if (userData)
		{
			auto& ei = *(ExitProcessInfo*)userData;
			weight = ei.startInfo->weight;
		}

		// Return weight while running these tasks
		SCOPED_FUTEX(m_processEntriesLock, lock);
		m_activeLocalProcessWeight -= weight;
		lock.Leave();

		Event done(true);
		struct Context
		{
			Logger& logger;
			Event& done;
			Atomic<u32> counter;
		} context { logger, done };

		auto exitedFunc = [](void* userData, const ProcessHandle& ph, ProcessExitedResponse&)
			{
				auto& context = *(Context*)userData;
				if (ph.GetExitCode() != 0 && ph.GetExecutionType() != ProcessExecutionType_Skipped)
					for (auto& line : ph.GetLogLines())
						context.logger.Log(LogEntryType_Error, line.text);

				if (!--context.counter)
					context.done.Set();
			};

		for (auto& command : commands)
		{
			StringBuffer<> application(command.application);
			m_session.DevirtualizePath(application, rootsHandle);
			//StringBuffer<> logFile;
			//logFile.Appendf(L"%s_LOG_FILE_%u.log", description, context.counter.load());
			++context.counter;
			ProcessStartInfo si;
			si.application = application.data;
			si.workingDir = workingDir;
			si.arguments = command.arguments.c_str();
			si.description = description;
			si.exitedFunc = exitedFunc;
			si.userData = &context;
			si.rootsHandle = rootsHandle;
			//si.logFile = logFile.data;
			EnqueueProcess({si});
		}

		m_session.ReenableRemoteExecution();

		if (!done.IsSet(2*60*60*1000))
			logger.Error(TC("Something went wrong waiting for %s"), description);

		// Take back weight.. TODO: Should this wait for available weight before returning?
		lock.Enter();
		m_activeLocalProcessWeight += weight;
		lock.Leave();

		return true;
	}
}

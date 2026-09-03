// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSessionClient.h"
#include "UbaApplicationRules.h"
#include "UbaConfig.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkMessage.h"
#include "UbaProcess.h"
#include "UbaProcessStartInfoHolder.h"
#include "UbaProtocol.h"
#include "UbaStorage.h"

#if PLATFORM_WINDOWS
#include <winerror.h>
#include <tlhelp32.h>
#endif

namespace uba
{
	const tchar* ToString(MessageType type)
	{
		switch (type)
		{
			#define UBA_PROCESS_MESSAGE(x) case MessageType_##x: return TC(#x);
			UBA_PROCESS_MESSAGES
			#undef UBA_PROCESS_MESSAGE
		default:
			return TC("UnknownProcessMessage"); // Should never happen
		}
	}

	void SessionClientCreateInfo::Apply(const Config& config)
	{
		SessionCreateInfo::Apply(config);

		if (const ConfigTable* table = config.GetTable(TC("Session")))
		{
			table->GetValueAsBool(useDependencyCrawler, TC("UseDependencyCrawler"));
			table->GetValueAsU32(pingTimeoutSecondsPrintCallstacks, TC("PingTimeoutSecondsPrintCallstacks"));
		}
	}


	struct SessionClient::ModuleInfo
	{
		ModuleInfo(const tchar* n, const CasKey& c, u32 a) : name(n), casKey(c), attributes(a), done(true) {}
		TString name;
		CasKey casKey;
		u32 attributes;
		Event done;
	};

	SessionClient::SessionClient(const SessionClientCreateInfo& info)
	: Session(info, TC("UbaSessionClient"), true, info.client)
	,	m_client(info.client)
	,	m_name(info.name.data)
	,	m_terminationTime(~0ull)
	,	m_waitToSendEvent(false)
	,	m_loop(true)
	,	m_allowSpawn(true)
	{
		m_maxProcessCount = info.maxProcessCount;
		m_dedicated = info.dedicated;
		m_useStorage = info.useStorage;
		m_downloadDetoursLib = info.downloadDetoursLib;
		m_defaultPriorityClass = info.defaultPriorityClass;
		m_maxIdleSeconds = info.maxIdleSeconds;
		m_osVersion = info.osVersion;
		m_disableCustomAllocator = info.disableCustomAllocator;
		m_useBinariesAsVersion = info.useBinariesAsVersion;
		m_memWaitLoadPercent = info.memWaitLoadPercent;
		m_memKillLoadPercent = info.memKillLoadPercent;
		m_processFinished = info.processFinished;

		#if PLATFORM_WINDOWS || PLATFORM_MAC // Linux will crash with print all callstacks
		m_pingTimeoutSecondsPrintCallstacks = info.pingTimeoutSecondsPrintCallstacks;
		#endif

		m_useDependencyCrawler = info.useDependencyCrawler;

		m_processIdCounter = ~0u / 2; // We set this value to a very high value.. because it will be used by child processes and we don't want id from server and child process id to collide

		if (m_name.IsEmpty())
			GetComputerNameW(m_name);

		m_processWorkingDir.Append(m_rootDir).Append(TCV("empty"));// + TC("empty\\");
		m_storage.CreateDirectory(m_processWorkingDir.data);
		m_processWorkingDir.EnsureEndsWithSlash();

		if (info.killRandom)
		{
			Guid g;
			CreateGuid(g);
			m_killRandomIndex = 10 + g.data1 % 30;
		}

		m_nameToHashTableMem.Init(NameToHashMemSize);

		Create(info);



		if (m_useDependencyCrawler)
			m_dependencyCrawler.Init([this](const StringView& fileName, u32& outAttr) // FileExists
				{
					return Exists(fileName, outAttr);
				},
			[this](const StringView& path, const DependencyCrawler::FileFunc& fileFunc) // TraverseFilesFunc
				{
					u32 tableOffset = 0;
					if (!EntryExists(path, tableOffset))
						return;
					if ((tableOffset & 0x80000000) == 0)
						return;
					tableOffset &= ~0x80000000;

					// This is not entirely correct since files could be added while uba is running.. but since this is only used for includes that
					// we know are created before uba started we're good.. and there should be no new files added to those dirs causing table offset to be chained
					BinaryReader reader(m_directoryTableMem, tableOffset, m_directoryTableMemPos);
					while (true)
					{
						u32 prevTableOffset = u32(reader.Read7BitEncoded());
						if (!prevTableOffset)
							break;
						reader.SetPosition(prevTableOffset);
					}

					u32 dirAttr = reader.ReadFileAttributes();
					if (!dirAttr)
						return;
					reader.ReadVolumeSerial();
					reader.ReadFileIndex();
					u64 itemCount = reader.Read7BitEncoded();
					while (itemCount--)
					{
						StringBuffer<> fileName;
						reader.ReadString(fileName);
						if (CaseInsensitiveFs)
							fileName.MakeLower();
						u32 attr = reader.ReadFileAttributes();
						bool isDir = IsDirectory(attr);
						fileFunc(fileName, isDir);
						reader.ReadVolumeSerial();
						reader.ReadFileIndex();
						if (isDir)
							continue;
						reader.ReadFileTime();
						reader.ReadFileSize();
					}
				});
	}

	SessionClient::~SessionClient()
	{
		Stop();
	}

	bool SessionClient::Start()
	{
		m_client.RegisterOnDisconnected([this]()
			{
				m_loop = false;
			});

		m_client.RegisterOnConnected([this]()
			{
				Connect();
			});
		return true;
	}

	void SessionClient::Stop(bool wait)
	{
		m_loop = false;
		m_waitToSendEvent.Set();
		if (wait)
			m_loopThread.Wait();
	}

	bool SessionClient::Wait(u32 milliseconds, Event* wakeupEvent)
	{
		return m_loopThread.Wait(milliseconds, wakeupEvent);
	}

	void SessionClient::SetIsTerminating(const tchar* reason, u64 delayMs)
	{
		m_terminationTime = GetTime() + MsToTime(delayMs);
		m_terminationReason = reason;
		SendNotification(ToView(reason));
	}

	void SessionClient::SetMaxProcessCount(u32 count)
	{
		m_maxProcessCount = count;
	}

	void SessionClient::SetAllowSpawn(bool allow)
	{
		m_allowSpawn = allow;
	}

	u64 SessionClient::GetBestPing()
	{
		return m_bestPing;
	}

	u32 SessionClient::GetMaxProcessCount()
	{
		return m_maxProcessCount;
	}

	bool SessionClient::Exists(const StringView& path, u32& outAttributes)
	{
		u32 tableOffset = 0;
		if (!EntryExists(path, tableOffset))
			return false;
		outAttributes = m_directoryTable.GetAttributes(tableOffset);
		return outAttributes != 0;
	}

	bool SessionClient::RetrieveCasFile(CasKey& outNewKey, u64& outSize, const CasKey& casKey, const tchar* hint, bool storeUncompressed, bool allowProxy)
	{
		//StringBuffer<> workName;
		//u32 len = TStrlen(hint);
		//workName.Append(TCV("RC:")).Append(len > 30 ? hint + (len - 30) : hint);
		//TrackWorkScope tws(m_client, workName.data);

		TimerScope s(m_stats.storageRetrieve);
		CasKey tempKey = casKey;

		if (storeUncompressed)
			tempKey = AsCompressed(casKey, false);
		//else
		//	storeUncompressed = !IsCompressed(casKey);

		Storage::RetrieveResult result;
		bool res = m_storage.RetrieveCasFile(result, tempKey, hint, nullptr, 1, allowProxy);
		outNewKey = result.casKey;
		outSize = result.size;
		return res;
	}

	bool SessionClient::GetCasKeyForFile(CasKey& out, u32 processId, const StringView& fileName, const StringKey& fileNameKey)
	{
		TimerScope waitTimer(Stats().waitGetFileMsg);
		SCOPED_FUTEX(m_nameToHashLookupLock, lock);
		auto insres = m_nameToHashLookup.try_emplace(fileNameKey);
		HashRec& rec = insres.first->second;
		lock.Leave();
		SCOPED_FUTEX(rec.lock, lock2);
		if (rec.key == CasKeyZero)//!rec.serverTime)
		{
			waitTimer.Cancel();

			// These will never succeed
			if (fileName.StartsWith(m_sessionBinDir.data) || fileName.StartsWith(TC("c:\\noenvironment")) || fileName.StartsWith(m_processWorkingDir.data))
			{
				out = CasKeyZero;
				return true;
			}

			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetFileFromServer, writer);
			writer.WriteU32(processId);
			writer.WriteString(fileName);
			writer.WriteStringKey(fileNameKey);

			StackBinaryReader<128> reader;
			if (!msg.Send(reader, Stats().getFileMsg))
				return false;

			rec.key = reader.ReadCasKey();
			if (rec.key != CasKeyZero)
				rec.serverTime = reader.ReadU64();
		}
		out = rec.key;
		return true;
	}

	bool SessionClient::EnsureBinaryFile(StringBufferBase& out, StringBufferBase& outVirtual, u32 processId, StringView fileName, const StringKey& fileNameKey, StringView applicationDir, StringView workingDir, const u8* loaderPaths, u32 loaderPathsSize)
	{
		CasKey casKey;
		u32 fileAttributes = DefaultAttributes(); // TODO: This is wrong.. need to retrieve from server if this is executable or not

		bool isAbsolute = IsAbsolutePath(fileName.data);
		if (isAbsolute)
		{
			UBA_ASSERT(fileNameKey != StringKeyZero);
			if (!GetCasKeyForFile(casKey, processId, fileName, fileNameKey))
				return false;
			// This needs to be absolute virtual path (the path on the host).. don't remember why this was written like this. Changed to just use fileName (needed for shadercompilerworker)
			//const tchar* lastSlash = TStrrchr(fileName.data, PathSeparator);
			//outVirtual.Append(lastSlash + 1);
			outVirtual.Append(fileName);
		}
		else
		{
			UBA_ASSERT(fileNameKey == StringKeyZero);
			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_EnsureBinaryFile, writer);
			writer.WriteBool(IsRunningArm());
			writer.WriteString(fileName);
			writer.WriteStringKey(fileNameKey);
			writer.WriteString(applicationDir);
			writer.WriteString(workingDir);
			if (loaderPathsSize)
				writer.WriteBytes(loaderPaths, loaderPathsSize);

			StackBinaryReader<1024> reader;
			if (!msg.Send(reader, Stats().getBinaryMsg))
				return false;

			casKey = reader.ReadCasKey();
			reader.ReadString(outVirtual);
		}

		if (casKey == CasKeyZero)
		{
			out.Append(fileName);
			return true;
		}
		bool storeUncompressed = true;
		CasKey newKey;
		u64 fileSize;
		if (!RetrieveCasFile(newKey, fileSize, casKey, outVirtual.data, storeUncompressed))
			UBA_ASSERTF(false, TC("Casfile not found for %s using %s"), outVirtual.data, CasKeyString(casKey).str);
		StringBuffer<> destFile;
		if (isAbsolute || fileName.Contains(TC(".."))) // This is not beautiful, but we need to keep some dlls in the sub folder (for cl.exe etc)
			destFile.AppendFileName(fileName.data);
		else
			destFile.Append(fileName);

		StringBuffer<> applicationDirLower;
		applicationDirLower.Append(applicationDir).MakeLower();
		KeyToString keyStr(ToStringKey(applicationDirLower));

		return WriteBinFile(out, destFile, newKey, keyStr, fileAttributes);
	}

	bool SessionClient::PrepareProcess(ProcessImpl& process, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir)
	{
		ProcessStartInfoHolder& startInfo = process.m_startInfo;
		outRealWorkingDir = m_processWorkingDir.data;
		if (StartsWith(startInfo.application, TC("ubacopy")))
			return true;

		#if PLATFORM_LINUX
		//if (EndsWith(startInfo.application, TStrlen(startInfo.application), "/sh")) // For some reason the local shell application on the helpers hang (need to investigate aws shell application)
		//	return true;
		#elif PLATFORM_WINDOWS
		if (ToView(startInfo.application).EndsWith(TCV("system32\\cmd.exe"))) // We want to use local cmd.exe.. wine does not support kernel calls called by windows 11's cmd.exe for example
			return true;
		#endif

		outRealApplication.Clear();

		const tchar* application = startInfo.application;
		UBA_ASSERT(application && *application);
		bool isAbsolute = IsAbsolutePath(startInfo.application);

		SCOPED_FUTEX(m_handledApplicationEnvironmentsLock, environmentslock);
		auto insres = m_handledApplicationEnvironments.try_emplace(application);
		environmentslock.Leave();

		ApplicationEnvironment& appEnv = insres.first->second;
		SCOPED_FUTEX(appEnv.lock, lock);

		if (!appEnv.realApplication.empty())
		{
			outRealApplication.Append(appEnv.realApplication);
			if (!isAbsolute)
			{
				startInfo.applicationStr = appEnv.virtualApplication;
				startInfo.application = startInfo.applicationStr.c_str();
			}
			return true;
		}

		List<ModuleInfo> modules;
		if (!ReadModules(modules, 0, application))
			return false;

		StringBuffer<MaxPath> applicationDir;
		applicationDir.AppendDir(application);
		KeyToString keyStr(ToStringKeyLower(applicationDir));

		Atomic<bool> success = true;

		m_client.ParallelFor(u32(modules.size()), modules, [&](const WorkContext&, auto& it)
			{
				auto& m = *it;
				TrackWorkScope tws(m_client, AsView(TC("FetchModule")), ColorWork);
				tws.AddHint(m.name);

				auto g = MakeGuard([&]() { m.done.Set(); });
				CasKey newCasKey;
				bool storeUncompressed = true;
				u64 fileSize;
				const tchar* moduleName = m.name.c_str();
				if (!RetrieveCasFile(newCasKey, fileSize, m.casKey, moduleName, storeUncompressed))
				{
					m_logger.Error(TC("Casfile not found for %s (%s)"), moduleName, CasKeyString(m.casKey).str);
					success = false;
					return;
				}
				if (const tchar* lastSeparator = TStrrchr(moduleName, PathSeparator))
					moduleName = lastSeparator + 1;
				StringBuffer<MaxPath> temp;
				if (!WriteBinFile(temp, ToView(moduleName), newCasKey, keyStr, m.attributes))
					success = false;
			}, AsView(TC("FetchModule")), true);

		if (!success)
			return false;
		
		outRealApplication.Append(m_sessionBinDir).Append(keyStr).Append(PathSeparator).AppendFileName(application);
		appEnv.realApplication = outRealApplication.data;

		if (!isAbsolute)
		{
			appEnv.virtualApplication = modules.front().name;
			startInfo.applicationStr = appEnv.virtualApplication;
			startInfo.application = startInfo.applicationStr.c_str();
		}

		return true;
	}

	bool SessionClient::ReadModules(List<ModuleInfo>& outModules, u32 processId, const tchar* application)
	{
		TrackWorkScope tws(m_client, AsView(TC("ReadModules")), ColorWork);

		StackBinaryReader<16*1024> reader;
		{
			StackBinaryWriter<256> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetApplication, writer);
			writer.WriteU32(processId);
			writer.WriteString(application);
			if (!msg.Send(reader, m_stats.getApplicationMsg))
				return false;
		}

		u32 serverSystemPathLen = reader.ReadU32();
		u32 moduleCount = reader.ReadU32();
		if (moduleCount == 0)
			return m_logger.Error(TC("Application %s not found"), application);

		while (moduleCount--)
		{
			StringBuffer<> moduleFile;
			reader.ReadString(moduleFile);
			u32 fileAttributes = reader.ReadU32();
			bool isSystem = reader.ReadBool();

			CasKey casKey = reader.ReadCasKey();
			if (casKey == CasKeyZero)
				return m_logger.Error(TC("Bad CasKey for %s (%s)"), moduleFile.data, CasKeyString(casKey).str);

			#if PLATFORM_MAC
			u32 minOsVersion = reader.ReadU32();
			if (m_osVersion && m_osVersion < minOsVersion)
				return m_logger.Error(TC("%s has min os version %u but current os is %u"), moduleFile.data, minOsVersion, m_osVersion);
			#endif

			if (isSystem)
			{
				StringBuffer<> localSystemModule;
				localSystemModule.Append(m_systemPath).Append(moduleFile.data + serverSystemPathLen);
				if (FileExists(m_logger, localSystemModule.data) && !localSystemModule.EndsWith(TCV(".exe")))
					continue;
				moduleFile.Clear().Append(localSystemModule);
			}
			outModules.emplace_back(moduleFile.data, casKey, fileAttributes);
		}

		return true;
	}

	void* SessionClient::GetProcessEnvironmentVariables()
	{
		UBA_ASSERT(!m_environmentVariables.empty());
		return m_environmentVariables.data();
	}

	bool SessionClient::WriteBinFile(StringBufferBase& out, const StringView& binaryName, const CasKey& casKey, const KeyToString& applicationDir, u32 fileAttributes)
	{
		UBA_ASSERT(fileAttributes);

		out.Append(m_sessionBinDir);
		out.Append(applicationDir).Append(PathSeparator);
		
		StringBuffer<> lower;
		lower.Append(applicationDir).Append(PathSeparator).Append(binaryName);
		lower.MakeLower();
		SCOPED_FUTEX(m_binFileLock, lock);

		auto insres = m_writtenBinFiles.try_emplace(lower.data, casKey);
		if (!insres.second)
		{
			out.Append(binaryName);
			if (insres.first->second != casKey)
				return m_logger.Error(TC("Writing same binary file %s multiple times but with different data! (Current: %s Previous: %s)"), out.data, CasKeyString(casKey).str, CasKeyString(insres.first->second).str);
			return true;
		}

		m_storage.CreateDirectory(out.data);
		out.Append(binaryName);

		//if (GetFileAttributesW(out.data) != INVALID_FILE_ATTRIBUTES)
		//	return true;

		if (TStrchr(binaryName.data, PathSeparator))
		{
			StringBuffer<> binaryDir;
			binaryDir.AppendDir(out);
			if (!m_storage.CreateDirectory(binaryDir.data))
				return false;
		}

		// Special hack to prevent two identical dlls from pointing to the same file (dlls are deduplicated if they are the same file in the backend)
		bool allowHardlink = !binaryName.GetFileName().StartsWith(TC("c2"));

		constexpr bool writeCompressed = false;
		constexpr bool isTemp = true;
		return m_storage.CopyOrLink(casKey, out.data, fileAttributes, writeCompressed, {}, isTemp, allowHardlink);
	}

	bool SessionClient::ProcessThreadStart(ProcessImpl& process)
	{
		if (!Session::ProcessThreadStart(process))
			return false;

		#if PLATFORM_LINUX
		// TODO. Linux linker has lots of stuff going on. it is a shell script deleting, renaming, overwriting same file causing confusion with child processes
		// Since directory table is not updated from server between child processes we end up with not up-to-date table.. therefore we flush everything in between
		if (auto parent = process.m_parentProcess)
		{
			auto& si = parent->m_startInfo;
			if (ToView(si.application).EndsWith(TCV("/sh")))
			{
				FlushWrittenFiles(*parent);
				StackBinaryReader<SendMaxSize> reader;
				SendUpdateDirectoryTable(reader);
			}
		}
		#endif

		if (RootsHandle rootsHandle = process.GetStartInfo().rootsHandle)
			if (!SendRootsHandle(rootsHandle))
				return false;

		if (!process.m_parentProcess && m_useDependencyCrawler)
			RunDependencyCrawler(process);

		return true;
	}

	bool SessionClient::CreateFileForRead(CreateFileResponse& out, TrackWorkScope& tws, const StringView& fileName, const StringKey& fileNameKey, ProcessImpl& process, const ApplicationRules& rules)
	{
		CasKey casKey;
		if (!GetCasKeyForFile(casKey, process.GetId(), fileName, fileNameKey))
			return false;

		// Not finding a file is a valid path. Some applications try with a path and if fails try another path
		if (casKey == CasKeyZero)
		{
			//m_logger.Warning(TC("No casfile found for %s"), fileName.data);
			out.directoryTableSize = GetDirectoryTableSize();
			out.mappedFileTableSize = GetFileMappingSize();
			out.fileName.Append(fileName);
			return true;
		}

		// Code for doing retry if failing to decompress casfile. We've seen cases of corrupt cas files on clients
		bool shouldRetry = true;
		FileMappingEntry* retryEntry = nullptr;
		auto retryEntryGuard = MakeGuard([&]() { if (retryEntry) retryEntry->lock.Leave(); });

		while (true)
		{
			StringBuffer<> newName;
			bool isDir = casKey == CasKeyIsDirectory;
			u64 fileSize = InvalidValue;
			CasKey newCasKey;

			#ifdef __clang_analyzer__
			newCasKey = {};
			#endif

			u32 memoryMapAlignment = 0;
			if (m_allowMemoryMaps)
			{
				memoryMapAlignment = GetMemoryMapAlignment(fileName);
				if (!memoryMapAlignment && !m_useStorage)
					memoryMapAlignment = 64 * 1024;
			}

			if (isDir)
			{
				newName.Append(TCV("$d"));
			}
			else if (casKey != CasKeyZero)
			{
				if (m_useStorage || memoryMapAlignment == 0)
				{
					bool storeUncompressed = memoryMapAlignment == 0;
					bool allowProxy = rules.AllowStorageProxy(fileName);
					if (!RetrieveCasFile(newCasKey, fileSize, casKey, fileName.data, storeUncompressed, allowProxy))
						return m_logger.Error(TC("Error retrieving cas entry %s (%s)"), CasKeyString(casKey).str, fileName.data);

					if (!m_storage.GetCasFileName(newName, newCasKey))
						return false;
				}
				else
				{
					StorageStats& stats = m_storage.Stats();
					TimerScope ts(stats.ensureCas);

					SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
					auto insres = m_fileMappingTableLookup.try_emplace(fileNameKey);
					FileMappingEntry& entry = insres.first->second;
					lookupLock.Leave();

					SCOPED_FUTEX(entry.lock, entryCs);
					ts.Leave();

					if (entry.handled)
					{
						if (!entry.success)
							return false;
					}
					else
					{
						TimerScope s(m_stats.storageRetrieve);
						casKey = AsCompressed(casKey, false);
						entry.handled = true;
						Storage::RetrieveResult result;
						bool allowProxy = rules.AllowStorageProxy(fileName);
						if (!m_storage.RetrieveCasFile(result, casKey, fileName.data, &m_fileMappingBuffer, memoryMapAlignment, allowProxy))
							return m_logger.Error(TC("Error retrieving cas entry %s (%s)"), CasKeyString(casKey).str, fileName.data);
						entry.success = true;
						entry.contentSize = result.size;
						entry.mapping = result.view.handle;
						entry.mappingOffset = result.view.offset;
					}

					fileSize = entry.contentSize;
					if (entry.mapping.IsValid())
						Storage::GetMappingString(newName, entry.mapping, entry.mappingOffset);
					else
						newName.Append(entry.isDir ? TC("$d") : TC("$f"));
				}
			}

			UBA_ASSERTF(!newName.IsEmpty(), TC("No casfile available for %s using %s"), fileName.data, CasKeyString(casKey).str);

			if (newName[0] != '^')
			{
				TrackHintScope ths(tws, AsView(TC("CreateMemoryMap")));
				if (!isDir && memoryMapAlignment)
				{
					if (retryEntry)
						retryEntryGuard.Execute();

					MemoryMap map;
					if (!GetOrCreateMemoryMapFromFile(map, fileNameKey, newName.data, IsCompressed(newCasKey), memoryMapAlignment, fileName.data, nullptr, false))
					{
						if (!shouldRetry)
							return false;
						shouldRetry = false;

						// We need to take a lock around the file map entry since there might be another thread also wanting to map this
						{
							SCOPED_FUTEX(m_fileMappingTableLookupLock, lookupLock);
							retryEntry = &m_fileMappingTableLookup.try_emplace(fileNameKey).first->second;
							lookupLock.Leave();
							retryEntry->lock.Enter();
							retryEntry->handled = false;
						}

						if (!m_storage.ReportBadCasFile(newCasKey))
							return false;

						continue;
					}
					fileSize = map.size;
					newName.Clear().Append(map.name);
				}
				else if (!rules.IsRarelyRead(fileName))
				{
					AddFileMapping(fileNameKey, fileName.data, newName.data, fileSize);
				}
			}

			out.directoryTableSize = GetDirectoryTableSize();
			out.mappedFileTableSize = GetFileMappingSize();
			out.fileName.Append(newName);
			out.size = fileSize;
			return true;
		}
	}

	bool SessionClient::SendFiles(ProcessImpl& process, Timer& sendFiles)
	{
		auto& rules = *process.m_startInfo.rules;

		StorageStatsScope storageStatsScope(process.m_storageStats);
		for (auto& pair : process.m_shared.writtenFiles)
		{
			WrittenFile& file = pair.second;

			if (StringView(file.name).StartsWith(m_tempPath) && !rules.KeepTempOutputFile(StringView(file.name)))
			{
				CloseFileMapping(m_logger, file.mappingHandle, file.backedName.c_str());
				file.mappingHandle = {};
				#if UBA_DEBUG
				m_logger.Info(TC("Skipping sending back temp file %s"), file.name.c_str());
				#endif
				continue;
			}

			TimerScope timer(sendFiles);
#ifdef _DEBUG
			if (!pair.second.mappingHandle.IsValid())
				m_logger.Warning(TC("%s is not using file mapping"), file.name.c_str());
#endif
			bool keepMappingInMemory = IsWindows && !IsRarelyReadAfterWritten(process, file.name);
			bool compressed = rules.SendFileCompressedFromClient(file.name);
			if (!SendFile(file, process.GetId(), keepMappingInMemory, compressed))
				return false;
		}
		return true;
	}

	bool SessionClient::SendFile(WrittenFile& file, u32 processId, bool keepMappingInMemory, bool compressed)
	{
		CasKey casKey;
		{
			TimerScope ts(m_stats.storageSend);
			if (!m_storage.StoreCasFileClient(casKey, file.key, file.backedName.c_str(), file.mappingHandle, 0, file.mappingWritten, file.name.c_str(), keepMappingInMemory, compressed))
				return false;
		}
		if (casKey == CasKeyZero)
			return m_logger.Warning(TC("Failed to store cas on server for local file %s (size %llu destination %s)"), file.backedName.c_str(), file.mappingWritten, file.name.c_str());
	
		CloseFileMapping(m_logger, file.mappingHandle, file.backedName.c_str());
		file.mappingHandle = {};

		StackBinaryReader<128> reader;
		{
			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_SendFileToServer, writer);
			writer.WriteU32(processId);
			writer.WriteString(file.name);
			writer.WriteStringKey(file.key);
			writer.WriteU32(file.attributes);
			writer.WriteCasKey(casKey);
			if (!msg.Send(reader, Stats().sendFileMsg))
				return m_logger.Warning(TC("Failed to send file %s to server"), file.backedName.c_str());
		}

		#if UBA_DEBUG_LOGGER
		m_debugLogger->Info(TC("SENDFILE %s\n"), file.name.c_str());
		#endif

		if (!reader.ReadBool())
			return m_logger.Warning(TC("Server failed to copy cas %s to %s (local source %s)"), CasKeyString(casKey).str, file.name.c_str(), file.backedName.c_str());
		return true;
	}

	bool SessionClient::DeleteFile(DeleteFileResponse& out, const DeleteFileMessage& msg)
	{
		// TODO: Deleting output files should also delete them on disk (for now they will leak until process shutdown)
		RemoveWrittenFile(msg.process, msg.fileNameKey);

		bool sendDelete = true;
		if (msg.closeId != 0)
		{
			UBA_ASSERTF(false, TC("This has not been tested properly"));
			SCOPED_FUTEX(m_activeFilesLock, lock);
			sendDelete = m_activeFiles.erase(msg.closeId) == 0;
		}

		{
			SCOPED_FUTEX(m_outputFilesLock, lock);
			sendDelete = m_outputFiles.erase(msg.fileName.data) == 0 && sendDelete;
		}

		bool isTemp = StartsWith(msg.fileName.data, m_tempPath.data);
		if (isTemp)
			sendDelete = false;

		if (!sendDelete)
		{
			if (!m_allowMemoryMaps && isTemp)
			{
				out.result = uba::DeleteFileW(msg.fileName.data);
				out.errorCode = GetLastError();
				return true;
			}
			out.result = true;
			out.errorCode = ERROR_SUCCESS;
			return true;
		}

		// TODO: Cache this if it becomes noisy

		StackBinaryWriter<1024> writer;
		NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_DeleteFile, writer);
		writer.WriteStringKey(msg.fileNameKey);
		writer.WriteString(msg.fileName);
		StackBinaryReader<SendMaxSize> reader;
		if (!networkMsg.Send(reader, Stats().deleteFileMsg))
			return false;
		out.result = reader.ReadBool();
		out.errorCode = reader.ReadU32();
		if (out.result)
			if (!SendUpdateDirectoryTable(reader.Reset()))
				return false;
		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool SessionClient::CopyFile(CopyFileResponse& out, const CopyFileMessage& msg)
	{
		SCOPED_FUTEX(m_outputFilesLock, lock);
		auto findIt = m_outputFiles.find(msg.fromName.data);
		if (findIt == m_outputFiles.end())
		{
			lock.Leave();

			StackBinaryWriter<1024> writer;
			NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_CopyFile, writer);
			writer.WriteStringKey(msg.fromKey);
			writer.WriteString(msg.fromName);
			writer.WriteStringKey(msg.toKey);
			writer.WriteString(msg.toName);
			StackBinaryReader<SendMaxSize> reader;
			if (!networkMsg.Send(reader, Stats().copyFileMsg))
				return false;
			out.fromName.Append(msg.fromName);
			out.toName.Append(msg.toName);
			out.closeId = ~0u;
			out.errorCode = reader.ReadU32();
			if (!out.errorCode)
				if (!SendUpdateDirectoryTable(reader.Reset()))
					return false;
			out.directoryTableSize = GetDirectoryTableSize();
			return true;
		}
		lock.Leave();

		out.fromName.Append(findIt->second);

		CreateFileMessage writeMsg { msg.process };
		writeMsg.fileName.Append(msg.toName.data);
		writeMsg.fileNameKey = msg.toKey;
		writeMsg.access = FileAccess_Write;
		CreateFileResponse writeOut;
		if (!CreateFile(writeOut, writeMsg))
			return false;

		out.toName.Append(writeOut.fileName);
		out.closeId = writeOut.closeId;
		return true;
	}

	bool SessionClient::MoveFile(MoveFileResponse& out, const MoveFileMessage& msg)
	{
		const tchar* fromName = msg.fromName.data;
		const tchar* toName = msg.toName.data;
		auto& process = msg.process;

		{
			SCOPED_FUTEX(process.m_shared.writtenFilesLock, lock);
			auto& writtenFiles = process.m_shared.writtenFiles;
			auto findIt = writtenFiles.find(msg.fromKey);
			if (findIt != writtenFiles.end())
			{
				UBA_ASSERT(msg.toKey != StringKeyZero);
				auto insres = writtenFiles.try_emplace(msg.toKey);
				UBA_ASSERTF(insres.second, TC("Moving written file %s to other written file %s. (%s)"), fromName, toName, process.m_startInfo.description);
				insres.first->second = findIt->second;
				insres.first->second.key = msg.toKey;
				insres.first->second.name = toName;
				insres.first->second.owner = &process;
				writtenFiles.erase(findIt);
			}
			else
			{
				// TODO: Need to tell server 
			}
		}

		bool sendMove = true;
		{
			SCOPED_FUTEX(m_outputFilesLock, lock);
			auto findIt = m_outputFiles.find(fromName);
			if (findIt != m_outputFiles.end())
			{
				auto insres = m_outputFiles.try_emplace(toName);
				UBA_ASSERTF(insres.second, TC("Failed to add move destination file %s as output file because it is already added. (Moved from %s)"), toName, fromName);
				insres.first->second = findIt->second;
				m_outputFiles.erase(findIt);
				sendMove = false;
			}
		}

		if (!sendMove)
		{
			out.result = true;
			out.errorCode = ERROR_SUCCESS;
			return true;
		}

		// TODO: This should be done by server?

		out.result = uba::MoveFileExW(fromName, toName, 0);
		out.errorCode = GetLastError();

		return true;
	}

	bool SessionClient::Chmod(ChmodResponse& out, const ChmodMessage& msg)
	{
		{
			SCOPED_FUTEX(msg.process.m_shared.writtenFilesLock, lock);
			auto& writtenFiles = msg.process.m_shared.writtenFiles;
			auto findIt = writtenFiles.find(msg.fileNameKey);
			if (findIt != writtenFiles.end())
			{
				bool executable = false;
				#if !PLATFORM_WINDOWS
				if (msg.fileMode & S_IXUSR)
					executable = true;
				#endif
				findIt->second.attributes = DefaultAttributes(executable);
				out.errorCode = 0;
				return true;
			}
		}

		UBA_ASSERTF(false, TC("Code path not implemented.. should likely send message to server"));
		return true;
	}

	bool SessionClient::CreateDirectory(CreateDirectoryResponse& out, const CreateDirectoryMessage& msg)
	{
		// TODO: Cache this if it becomes noisy

		StackBinaryWriter<1024> writer;
		NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_CreateDirectory, writer);
		writer.WriteString(msg.name);
		StackBinaryReader<SendMaxSize> reader;
		if (!networkMsg.Send(reader, Stats().createDirMsg))
			return false;
		out.result = reader.ReadBool();
		out.errorCode = reader.ReadU32();

		if (out.result || out.errorCode == ERROR_ALREADY_EXISTS) // Even if it exists it might be that this client session is not aware
			if (!SendUpdateDirectoryTable(reader.Reset()))
				return false;

		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool SessionClient::RemoveDirectory(RemoveDirectoryResponse& out, const RemoveDirectoryMessage& msg)
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_RemoveDirectory, writer);
		writer.WriteString(msg.name);
		StackBinaryReader<SendMaxSize> reader;
		if (!networkMsg.Send(reader, Stats().deleteFileMsg)) // Wrong message
			return false;
		out.result = reader.ReadBool();
		out.errorCode = reader.ReadU32();

		if (out.result)
			if (!SendUpdateDirectoryTable(reader.Reset()))
				return false;

		out.directoryTableSize = GetDirectoryTableSize();
		return true;
	}

	bool SessionClient::GetFullFileName(GetFullFileNameResponse& out, const GetFullFileNameMessage& msg)
	{
		StringView workingDir(msg.process.m_startInfo.workingDirStr);

		StringKeyHasher hasher;
		hasher.UpdateNoCheck(msg.process.m_startInfo.applicationStr);
		hasher.UpdateNoCheck(msg.fileName);
		hasher.UpdateNoCheck(workingDir);
		StringKey nameKey = ToStringKey(hasher);

		SCOPED_FUTEX(m_nameToNameLookupLock, lock);
		auto insres = m_nameToNameLookup.try_emplace(nameKey);
		NameRec& rec = insres.first->second;
		lock.Leave();
		SCOPED_FUTEX(rec.lock, lock2);

		if (rec.handled)
		{
			out.fileName.Append(rec.name.c_str());
			out.virtualFileName.Append(rec.virtualName.c_str());
			return true;
		}
		rec.handled = true;

		StringBuffer<> appDir;
		appDir.AppendDir(msg.process.m_startInfo.application);
		if (!EnsureBinaryFile(out.fileName, out.virtualFileName, msg.process.m_id, msg.fileName, msg.fileNameKey, appDir, workingDir, msg.loaderPaths, msg.loaderPathsSize))
			return false;

		StringKey fileNameKey = msg.fileNameKey;
		if (fileNameKey == StringKeyZero)
			fileNameKey =  CaseInsensitiveFs ? ToStringKeyLower(out.virtualFileName) : ToStringKey(out.virtualFileName);

		rec.name = out.fileName.data;
		rec.virtualName = out.virtualFileName.data;
		out.mappedFileTableSize = AddFileMapping(fileNameKey, msg.fileName.data, out.fileName.data);
		return true;
	}

	bool SessionClient::GetLongPathName(GetLongPathNameResponse& out, const GetLongPathNameMessage& msg)
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage networkMsg(m_client, ServiceId, SessionMessageType_GetLongPathName, writer);
		writer.WriteString(msg.fileName);
		StackBinaryReader<1024> reader;
		if (!networkMsg.Send(reader, Stats().getLongNameMsg))
			return false;
		out.errorCode = reader.ReadU32();
		reader.ReadString(out.fileName);
		return true;
	}

	bool SessionClient::GetListDirectoryInfo(ListDirectoryResponse& out, const StringView& dirName, const StringKey& dirKey)
	{
		TrackWorkScope tws(m_client, AsView(TC("GetListDir")), ColorWork);
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_ListDirectory, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteString(dirName);
		writer.WriteStringKey(dirKey);
		
		StackBinaryReader<SendMaxSize> reader;

		if (!msg.Send(reader, Stats().listDirMsg))
			return false;

		u32 tableOffset = reader.ReadU32();

		u32 oldTableSize = GetDirectoryTableSize();
		if (!UpdateDirectoryTableFromServer(reader))
			return false;
		u32 newTableSize = GetDirectoryTableSize();

		// Ask for a refresh of hashes straight away since they will likely be asked for by the process doing this query
		if (oldTableSize != newTableSize)
			m_waitToSendEvent.Set();

		out.tableOffset = tableOffset;
		out.tableSize = newTableSize;

		return true;
	}

	bool SessionClient::WriteFilesToDisk(ProcessImpl& process, WrittenFile** files, u32 fileCount)
	{
		// Do nothing, we will send the data to the host when process is finished
		return true;
	}


	struct SessionClient::ActiveUpdateDirectoryEntry
	{
		Event done;
		u32 readPos = 0;
		ActiveUpdateDirectoryEntry* prev = nullptr;
		ActiveUpdateDirectoryEntry* next = nullptr;
		bool success = true;

		static bool Wait(SessionClient& client, ActiveUpdateDirectoryEntry*& first, ScopedFutex& lock, u32 readPos, const tchar* hint)
		{
			ActiveUpdateDirectoryEntry item;
			item.next = first;
			if (item.next)
				item.next->prev = &item;
			item.readPos = readPos;
			first = &item;
			item.done.Create(true);

			lock.Leave();
			bool res = item.done.IsSet(5*60*1000);
			lock.Enter();

			if (item.prev)
				item.prev->next = item.next;
			else
				first = item.next;
			if (item.next)
				item.next->prev = item.prev;

			if (res)
				return item.success;

			u32 activeCount = 0;
			for (auto i = first; i; i = i->next)
				++activeCount;
			return client.m_logger.Error(TC("Timed out after 5 minutes waiting for update directory message to reach read position %u  (%u active in %s wait)"), readPos, activeCount, hint);
		}

		static void UpdateReadPosMatching(ActiveUpdateDirectoryEntry*& first, u32 readPos)
		{
			for (auto i = first; i; i = i->next)
			{
				if (i->readPos != readPos)
					continue;
				i->done.Set();
				break;
			}
		}

		static void UpdateReadPosLessOrEqual(ActiveUpdateDirectoryEntry*& first, u32 readPos)
		{
			for (auto i = first; i; i = i->next)
				if (i->readPos <= readPos)
					i->done.Set();
		}

		static void UpdateError(ActiveUpdateDirectoryEntry*& first)
		{
			for (auto i = first; i; i = i->next)
			{
				i->success = false;
				i->done.Set();
			}
		}
	};

	bool SessionClient::UpdateDirectoryTableFromServer(StackBinaryReader<SendMaxSize>& reader)
	{
		auto& dirTable = m_directoryTable;

		auto updateMemorySizeAndSignal = [&]
			{
				SCOPED_WRITE_LOCK(dirTable.m_memoryLock, lock);
				dirTable.m_memorySize = m_directoryTableMemPos;
				lock.Leave();
				ActiveUpdateDirectoryEntry::UpdateReadPosLessOrEqual(m_firstEmptyWait, m_directoryTableMemPos);
				return true;
			};

		u32 lastWriteEnd = ~0u;

		while (true)
		{
			u32 readPos = reader.ReadU32();

			u8* pos = m_directoryTableMem + readPos;
			u32 toRead = u32(reader.GetLeft());

			SCOPED_FUTEX(m_directoryTableLock, lock);

			if (m_directoryTableError)
				return false;

			EnsureDirectoryTableMemory(readPos + toRead);

			if (toRead == 0)
			{
				// We wrote to lastWriteEnd and now we got an empty message where readPos is the same..
				// This means that it was a good cut-off and we can increase m_memorySize
				// If m_directoryTableMemPos is different it means that we have another thread going on that will update things a little bit later.
				if (lastWriteEnd == readPos && lastWriteEnd == m_directoryTableMemPos)
					return updateMemorySizeAndSignal();

				// We might share this position with others
				if (dirTable.m_memorySize < readPos)
					if (!ActiveUpdateDirectoryEntry::Wait(*this, m_firstEmptyWait, lock, readPos, TC("empty")))
						return false;
				return true;
			}

			reader.ReadBytes(pos, toRead);

			// Wait until all data before readPos has been read
			if (readPos != m_directoryTableMemPos)
				if (!ActiveUpdateDirectoryEntry::Wait(*this, m_firstReadWait, lock, readPos, TC("read")))
					return false;

			m_directoryTableMemPos += toRead;
			
			// Find potential waiter waiting for this exact size and wake it up
			ActiveUpdateDirectoryEntry::UpdateReadPosMatching(m_firstReadWait, m_directoryTableMemPos);


			// If there is space left in the message it means that we caught up with the directory table server side..
			// And we will stop asking for more data.
			// Note, we can only set m_memorySize when getting messages that reads less than capacity since we don't know if we reached a good position in the directory table
			if (reader.GetPosition() < m_client.GetMessageMaxSize() - m_client.GetMessageReceiveHeaderSize())
				return updateMemorySizeAndSignal();

			lastWriteEnd = m_directoryTableMemPos;

			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetDirectoriesFromServer, writer);
			writer.WriteU32(m_sessionId);

			if (msg.Send(reader.Reset(), Stats().getDirsMsg))
				continue;

			// Let's signal waiters to exit faster since we will not get out of this situation (most likely a disconnect)
			m_directoryTableError = true;
			ActiveUpdateDirectoryEntry::UpdateError(m_firstReadWait);
			ActiveUpdateDirectoryEntry::UpdateError(m_firstEmptyWait);
			return false;
		}
	}

	bool SessionClient::UpdateNameToHashTableFromServer(StackBinaryReader<SendMaxSize>& reader)
	{
		u32 serverTableSize = 0;
		bool isFirst = true;
		u32 readStartPos = u32(m_nameToHashTableMem.writtenSize);
		u32 localTableSize = readStartPos;
		u64 serverTime = 0;
		while (true)
		{
			if (isFirst)
			{
				serverTableSize = reader.ReadU32();
				isFirst = false;
			}
			else
			{
				StackBinaryWriter<1024> writer;
				NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetNameToHashFromServer, writer);
				writer.WriteU32(serverTableSize);
				writer.WriteU32(localTableSize);
				if (!msg.Send(reader.Reset(), Stats().getHashesMsg))
					return false;
			}
			serverTime = reader.ReadU64();

			u8* pos = m_nameToHashTableMem.memory + localTableSize;

			u32 left = u32(reader.GetLeft());
			u32 toRead = serverTableSize - localTableSize;

			bool needMore = left < toRead;
			if (needMore)
				toRead = left;

			m_nameToHashTableMem.AllocateNoLock(toRead, 1, TC("NameToHashTable"));
			reader.ReadBytes(pos, toRead);
			localTableSize += toRead;

			if (!needMore)
				break;
		}

		u32 addCount = 0;
		BinaryReader r(m_nameToHashTableMem.memory, readStartPos, NameToHashMemSize);
		SCOPED_FUTEX(m_nameToHashLookupLock, lock);
		while (r.GetPosition() < localTableSize)
		{
			StringKey name = r.ReadStringKey();
			CasKey hash = r.ReadCasKey();

			HashRec& rec = m_nameToHashLookup[name];
			SCOPED_FUTEX(rec.lock, lock2);
			if (serverTime < rec.serverTime)
				continue;
			rec.key = hash;
			rec.serverTime = serverTime;
			++addCount;
		}

		return true;
	}

	void SessionClient::Connect()
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Connect, writer);
		writer.WriteString(m_name.data);
		writer.WriteU32(SessionNetworkVersion);
		writer.WriteBool(IsRunningArm());

		CasKey keys[2];
		if (m_useBinariesAsVersion)
		{
			StringBuffer<> dir;
			GetDirectoryOfCurrentModule(m_logger, dir);
			u64 dirCount = dir.count;
			dir.Append(PathSeparator).Append(UBA_AGENT_EXECUTABLE);
			m_storage.CalculateCasKey(keys[0], dir.data);
			dir.Resize(dirCount).Append(PathSeparator).Append(UBA_DETOURS_LIBRARY);
			m_storage.CalculateCasKey(keys[1], dir.data);
		}

		writer.WriteCasKey(keys[0]);
		writer.WriteCasKey(keys[1]);

		writer.WriteU32(m_maxProcessCount);
		writer.WriteBool(m_dedicated);

		StringBuffer<> info;
		GetSystemInfo(info);
		writer.WriteString(info);

		u64 memAvail;
		u64 memTotal;
		GetMemoryInfo(memAvail, memTotal);
		float cpuLoad = UpdateCpuLoad();

		m_cpuUsage = cpuLoad;
		m_memAvail = memAvail;
		m_memTotal = memTotal;

		writer.WriteU64(memAvail);
		writer.WriteU64(memTotal);
		writer.WriteU32(*(u32*)&cpuLoad);


		StackBinaryReader<SendMaxSize> reader;

		if (!msg.Send(reader, m_stats.connectMsg))
			return;

		if (!reader.ReadBool())
		{
			StringBuffer<> str;
			reader.ReadString(str);
			m_logger.Error(str.data);

			CasKey exeKey = reader.ReadCasKey();
			CasKey dllKey = reader.ReadCasKey();
			m_client.InvokeVersionMismatch(exeKey, dllKey);
			return;
		}

		u32 isArm = IsRunningArm();

		CasKey detoursBinaryKey[2];
		detoursBinaryKey[0] = reader.ReadCasKey();
		if (isArm)
			detoursBinaryKey[1] = reader.ReadCasKey();

		StringBuffer<> detoursFile[2];

		if (m_downloadDetoursLib)
		{
			for (u32 i=0; i!=isArm+1; ++i)
			{
				{
					TimerScope s(m_stats.storageRetrieve);
					Storage::RetrieveResult result;
					if (!m_storage.RetrieveCasFile(result, AsCompressed(detoursBinaryKey[i], false), UBA_DETOURS_LIBRARY))
						return;
				}
				StringKey key;
				key.a = i;
				KeyToString dir(key);

				if (!WriteBinFile(detoursFile[i], AsView(UBA_DETOURS_LIBRARY), detoursBinaryKey[i], dir, DefaultAttributes()))
					return;
			}
		}
		else
		{
			GetDirectoryOfCurrentModule(m_logger, detoursFile[isArm]);
			detoursFile[isArm].EnsureEndsWithSlash().Append(UBA_DETOURS_LIBRARY);
		}

		for (u32 i=0; i!=isArm+1; ++i)
		{
			#if PLATFORM_WINDOWS
			char dll[1024];
			detoursFile[i].Parse(dll, sizeof_array(dll));
			m_detoursLibrary[i] = dll;
			#else
			m_detoursLibrary[i] = detoursFile[i].data;
			#endif
		}

		bool resetCas = reader.ReadBool();
		if (resetCas)
			m_storage.Reset();

		m_sessionId = reader.ReadU32();
		m_uiLanguage = reader.ReadU32();
		m_storeIntermediateFilesCompressed = reader.ReadBool();
		m_detailedTrace = reader.ReadBool();
		m_shouldSendLogToServer = reader.ReadBool();
		m_shouldSendTraceToServer = reader.ReadBool();
		m_readIntermediateFilesCompressed = reader.ReadBool();

		auto& serverName = detoursFile[0].Clear(); // reuse
		reader.ReadString(serverName);
		m_logger.Info(TC("Connected to server %s"), serverName.data);

		if (m_shouldSendTraceToServer)
		{
			//if (m_detailedTrace)
			{
				m_client.SetWorkTracker(&m_trace);
			}
			StartTrace(nullptr, 256);
		}
		else
		{
			StartTraceThread(); // We still use trace thread for ping messages
		}

		BuildEnvironmentVariables(reader);

		m_loopThread.Start([this]() { ThreadCreateProcessLoop(); return 0; }, TC("UbaCreateProc"));
	}

	void SessionClient::BuildEnvironmentVariables(BinaryReader& reader)
	{
		TString tempStr;
		while (true)
		{
			tempStr = reader.ReadString();
			if (tempStr.empty())
				break;
			m_environmentVariables.insert(m_environmentVariables.end(), tempStr.begin(), tempStr.end());
			m_environmentVariables.push_back(0);
		}

		#if PLATFORM_WINDOWS
		AddEnvironmentVariableNoLock(TC("TEMP"), m_tempPath.data);
		AddEnvironmentVariableNoLock(TC("TMP"), m_tempPath.data);
		#else
		AddEnvironmentVariableNoLock(TC("TMPDIR"), m_tempPath.data);
		#endif

		StringBuffer<> v;
		for (auto& var : m_localEnvironmentVariables)
			if (GetEnvironmentVariableW(var, v.data, v.capacity))
				AddEnvironmentVariableNoLock(var, v.data);

		m_environmentVariables.push_back(0);
	}

	struct SessionClient::InternalProcessStartInfo : ProcessStartInfoHolder
	{
		u32 processId = 0;
	};


	bool SessionClient::SendProcessAvailable(Vector<InternalProcessStartInfo>& out, float availableWeight)
	{
		StackBinaryReader<SendMaxSize> reader;

		{
			TrackWorkScope tws(m_client, AsView(TC("RequestProcesses")), ColorWork);
			StackBinaryWriter<32> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_ProcessAvailable, writer);
			writer.WriteU32(m_sessionId);
			writer.WriteU32(*(u32*)&availableWeight);

			if (!msg.Send(reader, m_stats.procAvailableMsg))
			{
				if (m_loop)
					m_logger.Error(TC("Failed to send ProcessAvailable message"));
				return false;
			}
		}

		while (true)
		{
			u32 processId = reader.ReadU32();
			if (processId == 0)
				break;
			if (processId == SessionProcessAvailableResponse_Disconnect)
			{
				m_logger.Info(TC("Got disconnect request from host"));
				return false;
			}
			if (processId == SessionProcessAvailableResponse_RemoteExecutionDisabled)
			{
				m_remoteExecutionEnabled = false;
				break;
			}
			out.push_back({});
			InternalProcessStartInfo& info = out.back();
			info.processId = processId;
			info.Read(reader);
		}

		u32 neededDirectoryTableSize = reader.ReadU32();
		u32 neededHashTableSize = reader.ReadU32();

		if (u32 knownInputsCount = reader.ReadU32())
		{
			while (knownInputsCount--)
			{
				CasKey knownInputKey = reader.ReadCasKey();
				u32 mappingAlignment = reader.ReadU32();
				bool allowProxy = reader.ReadBool();
				bool storeUncompressed = !m_allowMemoryMaps || mappingAlignment == 0;
				if (storeUncompressed)
					knownInputKey = AsCompressed(knownInputKey, false);

				m_client.AddWork([knownInputKey, allowProxy, this](const WorkContext&)
					{
						TrackWorkScope tws(m_client, StringBuffer<>().Appendf(TC("KnownInput")), ColorWork);
						Storage::RetrieveResult result;
						bool res = m_storage.RetrieveCasFile(result, knownInputKey, TC("KnownInput"), nullptr, 1, allowProxy);
						(void)res;
					}, 1, TC("KnownInput"));
			}
		}

		if (!out.empty())
			if (neededDirectoryTableSize > GetDirectoryTableSize())
				if (!SendUpdateDirectoryTable(reader.Reset()))
					return false;

		// Always nice to update name-to-hash table since it can reduce number of messages while building.
		u32 hashTableMemSize;
		{
			SCOPED_READ_LOCK(m_nameToHashMemLock, l);
			hashTableMemSize = u32(m_nameToHashTableMem.writtenSize);
		}
		if (neededHashTableSize > hashTableMemSize)
			if (!SendUpdateNameToHashTable(reader.Reset()))
				return false;

		return true;
	}

	void SessionClient::SendReturnProcess(u32 processId, const tchar* reason)
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_ProcessReturned, writer);
		writer.WriteU32(processId);
		writer.WriteString(reason);
		StackBinaryReader<32> reader;
		if (!msg.Send(reader, m_stats.procReturnedMsg))
			return;
	}

	bool SessionClient::SendProcessInputs(ProcessImpl& process)
	{
		auto inputs = process.GetTrackedInputs();
		u32 left = u32(inputs.size());
		u32 capacityToAdd = left;
		u8* readPos = inputs.data();
		while (left)
		{
			StackBinaryWriter<SendMaxSize> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_ProcessInputs, writer);
			writer.Write7BitEncoded(process.m_id);
			writer.Write7BitEncoded(capacityToAdd);
			capacityToAdd = 0;
			u32 toWrite = Min(left, u32(writer.GetCapacityLeft()));
			writer.WriteBytes(readPos, toWrite);
			StackBinaryReader<32> reader;
			if (!msg.Send(reader))
				return false;
			readPos += toWrite;
			left -= toWrite;
		}
		return true;
	}

	bool SessionClient::SendProcessFinished(ProcessImpl& process, u32 exitCode)
	{
		StackBinaryWriter<SendMaxSize> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_ProcessFinished, writer);
		writer.WriteU32(process.m_id);
		writer.WriteU32(exitCode);
		u32* lineCount = (u32*)writer.AllocWrite(sizeof(u32));
		*lineCount = WriteLogLines(writer, process);

		// This is normally set after callback so we need to calculate it here
		auto& exitTime = process.m_processStats.exitTime;
		auto oldExitTime = exitTime.load();
		if (exitTime)
			exitTime = GetTime() - exitTime;

		// Must be written last
		process.m_processStats.Write(writer);
		process.m_sessionStats.Write(writer);
		process.m_storageStats.Write(writer);
		process.m_kernelStats.Write(writer);

		exitTime = oldExitTime;

		StackBinaryReader<16> reader;
		if (!msg.Send(reader, m_stats.procFinishedMsg) && m_loop)
			return m_logger.Error(TC("Failed to send ProcessFinished message!"));
		return true;
	}

	bool SessionClient::SendUpdateDirectoryTable(StackBinaryReader<SendMaxSize>& reader)
	{
		TrackWorkScope tws(m_client, AsView(TC("UpdateDir")), ColorWork);
		UBA_ASSERT(reader.GetPosition() == 0);
		StackBinaryWriter<32> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetDirectoriesFromServer, writer);
		writer.WriteU32(m_sessionId);
		if (!msg.Send(reader, Stats().getDirsMsg))
			return false;
		return UpdateDirectoryTableFromServer(reader);
	}

	bool SessionClient::SendUpdateNameToHashTable(StackBinaryReader<SendMaxSize>& reader)
	{
		TrackWorkScope tws(m_client, AsView(TC("UpdateHashTable")), ColorWork);
		StackBinaryWriter<32> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetNameToHashFromServer, writer);
		writer.WriteU32(~u32(0));

		SCOPED_WRITE_LOCK(m_nameToHashMemLock, lock);
		writer.WriteU32(u32(m_nameToHashTableMem.writtenSize));

		if (!msg.Send(reader, Stats().getHashesMsg))
			return false;
		return UpdateNameToHashTableFromServer(reader);
	}

	void SessionClient::SendPing(u64 memAvail, u64 memTotal)
	{
		u64 time = GetTime();
		if (TimeToMs(time - m_lastPingSendTime) < 2000) // Ping every ~2 seconds... this is so server can disconnect a client quickly if no ping is coming
			return;

		float cpuLoad = UpdateCpuLoad();
		m_cpuUsage = cpuLoad;

		StackBinaryWriter<128> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Ping, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteU64(m_lastPing);
		writer.WriteU64(memAvail);
		writer.WriteU64(memTotal);
		writer.WriteU32(*(u32*)&cpuLoad);
		StackBinaryReader<32> reader;

		struct Response
		{
			Event done;
			u64 time;
		} response{true, 0};

		auto doneFunc = [](bool error, void* userData)
			{
				auto& response = *(Response*)userData;
				response.time = GetTime();
				response.done.Set();
			};

		time = GetTime();
		
		if (!msg.SendAsync(reader, doneFunc, &response))
		{
			m_loop = false;
			return;
		}

		bool reportPing = false;

		u32 timeoutSeconds = m_pingTimeoutSecondsPrintCallstacks ? m_pingTimeoutSecondsPrintCallstacks : 20;
		if (!response.done.IsSet(timeoutSeconds*1000))
		{
			reportPing = true;
			LoggerWithWriter logger(g_consoleLogWriter);
			logger.Info(TC("Took more than %u seconds to send/receive ping%s"), timeoutSeconds, (m_pingTimeoutSecondsPrintCallstacks ? TC(". Printing callstacks") : TC("")));
			if (m_pingTimeoutSecondsPrintCallstacks)
				PrintAllCallstacks(logger);
			m_client.ValidateNetwork(logger, true);

			while (!response.done.IsSet(2000))
				m_client.ValidateNetwork(logger, false);

		}
		
		response.done.IsSet(); // Wait forever (until message trigger error or finishes)

		if (reportPing)
		{
			LoggerWithWriter logger(g_consoleLogWriter);
			logger.Info(TC("Ping finished after %s"), TimeToText(GetTime() - time).str);
		}

		if (!msg.ProcessAsyncResults(reader) || msg.GetError())
		{
			m_loop = false;
			return;
		}

		u64 lastPing = response.time - time;
		m_lastPing = lastPing;
		m_lastPingSendTime = response.time;

		if (lastPing < m_bestPing || m_bestPing == 0)
			m_bestPing = lastPing;

		m_storage.Ping();

		if (reader.ReadBool()) // abort
		{
			LoggerWithWriter(g_consoleLogWriter).Info(TC("Got abort from server"));
			abort();
		}
		if (reader.ReadBool()) // crashdump
		{
			TraverseAllCallstacks([&](const CallstackInfo& cs)
				{
					BinaryReader stackReader(cs.data.data(), 0, cs.data.size());
					StackBinaryWriter<SendMaxSize> stackWriter;
					GetSymbols(UBA_AGENT_EXECUTABLE, IsArmBinary, stackReader, stackWriter);
					BinaryReader resultReader(stackWriter.GetData(), 0, stackWriter.GetPosition());
					TString infoString = resultReader.ReadString();
					m_logger.Info(TC("%s%s"), cs.desc.c_str(), infoString.c_str());
				},
				[&](const StringView& error)
				{
					m_logger.Info(error.data);
				});
		}
	}

	void SessionClient::SendNotification(const StringView& text)
	{
		StackBinaryWriter<1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Notification, writer);
		writer.WriteU32(m_sessionId);
		writer.WriteString(text);
		msg.Send();
	}

	bool SessionClient::SendRootsHandle(RootsHandle rootsHandle)
	{
		SCOPED_FUTEX(m_rootsLookupLock, rootsLock);
		auto insres = m_rootsLookup.try_emplace(WithVfs(rootsHandle, false));
		RootsEntry& entry = insres.first->second;
		rootsLock.Leave();

		SCOPED_FUTEX(entry.lock, entryLock);
		if (entry.handled)
			return true;
		entry.handled = true;

		StackBinaryWriter<128> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetRoots, writer);
		writer.WriteU64(rootsHandle);
		StackBinaryReader<8*1024> reader;
		if (!msg.Send(reader, m_stats.getApplicationMsg))
			return false;
		PopulateRootsEntry(entry, reader.GetPositionData(), reader.GetLeft());
		return true;
	}

	void SessionClient::SendSummary(const Function<void(Logger&)>& extraInfo)
	{
		StackBinaryWriter<SendMaxSize> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Summary, writer);

		writer.WriteU32(m_sessionId);

		WriteSummary(writer, [&](Logger& logger)
			{
				PrintSummary(logger);
				m_storage.PrintSummary(logger);
				m_client.PrintSummary(logger);
				KernelStats::GetGlobal().Print(logger, true);
				PrintContentionSummary(logger);
				if (extraInfo)
					extraInfo(logger);
			});

		msg.Send();
	}

	void SessionClient::SendLogFileToServer(ProcessImpl& pi)
	{
		auto logFile = pi.m_startInfo.logFile;
		if (!logFile || !*logFile)
			return;
		WrittenFile f;
		f.backedName = logFile;
		f.attributes = DefaultAttributes();
		StringBuffer<> dest;
		if (const tchar* lastSlash = TStrrchr(logFile, PathSeparator))
			logFile = lastSlash + 1;
		dest.Append(TCV("<log>")).Append(logFile);
		f.name = dest.data;
		f.key = ToStringKeyLower(dest);
		SendFile(f, pi.GetId(), false, true);
		for (auto& child : pi.m_childProcesses)
			SendLogFileToServer(*(ProcessImpl*)child.m_process);
	}

	void SessionClient::GetLogFileName(StringBufferBase& out, const tchar* logFile, const tchar* arguments, u32 processId)
	{
		out.Append(m_sessionLogDir.data);
		if (logFile && *logFile)
		{
			if (const tchar* lastSeparator = TStrrchr(logFile, PathSeparator))
				logFile = lastSeparator + 1;
			out.Append(logFile);
		}
		else
		{
			GenerateNameForProcess(out, arguments, processId);
			out.Append(TCV(".log"));
		}
	}

	void SessionClient::ThreadCreateProcessLoop()
	{
		m_sendPing = true;

		// Ask for dir and hash table straight away to minimize latency
		m_client.AddWork([this](const WorkContext&)
			{
				StackBinaryReader<SendMaxSize> reader;
				SendUpdateDirectoryTable(reader);
			}, 1, TC("InitGetDirTable"), ColorWork);
		m_client.AddWork([this](const WorkContext&)
			{
				StackBinaryReader<SendMaxSize> reader;
				SendUpdateNameToHashTable(reader);
			}, 1, TC("InitGetHashTable"), ColorWork);


		struct ProcessRec
		{
			ProcessRec(ProcessImpl* impl) : handle(impl) {}
			ProcessHandle handle;
			Futex lock;
			Atomic<bool> isKilled;
			Atomic<bool> isDone;
			float weight = 1.0f;
		};
		List<ProcessRec> activeProcesses;

		u64 lastWaitTime = 0;
		u64 waitForMemoryPressureStartTime = 0;

		constexpr u64 waitTimeToSpawnAfterKillMs = 5 * 1000;

		u64 memAvail = m_memAvail;
		u64 memTotal = m_memTotal;

		u64 memRequiredToSpawn = u64(double(memTotal) * double(100 - m_memWaitLoadPercent) / 100.0);
		u64 memRequiredFree = u64(double(memTotal) * double(100 - m_memKillLoadPercent) / 100.0);

		float activeWeight = 0;
		ReaderWriterLock activeWeightLock;

		u64 idleStartTime = GetTime();
		u32 processRequestCount = 0;

		auto RemoveInactiveProcesses = [&]()
		{
			for (auto it=activeProcesses.begin();it!=activeProcesses.end();)
			{
				ProcessRec& r = *it;
				if (!r.isDone)
				{
					++it;
					continue;
				}
				r.lock.Enter();
				r.lock.Leave();
				it = activeProcesses.erase(it);
			}

			if (m_remoteExecutionEnabled && m_terminationReason)
			{
				m_remoteExecutionEnabled = false;
				m_logger.Info(TC("%s. Will stop scheduling processes and send failing processes back for retry"), m_terminationReason.load());
			}

			if (!activeProcesses.empty() || !m_allowSpawn)
			{
				idleStartTime = GetTime();
				processRequestCount = 0;
			}
			else if (m_remoteExecutionEnabled)
			{
				u32 idleTime = u32(TimeToS(GetTime() - idleStartTime));
				if (idleTime > m_maxIdleSeconds)
				{
					m_logger.Info(TC("Session has been idle longer than max idle time (%u seconds). Disconnecting (Did %u process requests during idle)"), m_maxIdleSeconds, processRequestCount);
					SendNotification(AsView(TC("Idle time timeout")));

					m_waitToSendEvent.Set();
					m_remoteExecutionEnabled = false;
				}
			}
		};

		Vector<InternalProcessStartInfo> startInfos;

		while (m_loop)
		{
			float maxWeight = float(m_maxProcessCount);
			u32 waitTimeoutMs = 3000;

			FlushDeadProcesses();

			GetMemoryInfo(memAvail, memTotal);
			m_memAvail = memAvail;
			m_memTotal = memTotal;

			if (memAvail < memRequiredFree)
			{
				for (auto it = activeProcesses.rbegin(); it != activeProcesses.rend(); ++it)
				{
					ProcessRec& rec = *it;
					if (rec.isKilled || rec.isDone)
						continue;
					SCOPED_FUTEX(rec.lock, lock);
					if (rec.isDone)
						continue;
					rec.handle.Cancel(true);
					rec.isKilled = true;
					SendReturnProcess(rec.handle.GetId(), TC("Running out of memory"));
					++m_stats.killCount;
					m_logger.Warning(TC("Killed process due to memory pressure (Available: %s Total: %s)"), BytesToText(memAvail).str, BytesToText(memTotal).str);
					break;
				}
				lastWaitTime = GetTime();
			}

			bool canSpawn = TimeToMs(GetTime() - lastWaitTime) > waitTimeToSpawnAfterKillMs && m_allowSpawn;
			if (!canSpawn)
				waitTimeoutMs = 500;

			bool firstCall = true;

			while (m_remoteExecutionEnabled && canSpawn && m_loop)
			{
				float availableWeight;
				{
					SCOPED_READ_LOCK(activeWeightLock, lock);
					if (activeWeight >= maxWeight)
						break;
					availableWeight = maxWeight - activeWeight;
				}

				if (!firstCall)
				{
					GetMemoryInfo(memAvail, memTotal);
					m_memAvail = memAvail;
					m_memTotal = memTotal;
				}
				if (memAvail < memRequiredToSpawn)
				{
					if (waitForMemoryPressureStartTime == 0)
					{
						m_logger.Info(TC("Delaying spawn due to memory pressure (Available: %s Total: %s)"), BytesToText(memAvail).str, BytesToText(memTotal).str);
						waitForMemoryPressureStartTime = GetTime();
					}
					break;
				}

				if (waitForMemoryPressureStartTime)
				{
					u64 waitTime = GetTime() - waitForMemoryPressureStartTime;
					m_logger.Info(TC("Waited %s for memory pressure to go down (Available: %s Total: %s)"), TimeToText(waitTime).str, BytesToText(memAvail).str, BytesToText(memTotal).str);
					m_stats.waitMemPressure += waitTime;
					waitForMemoryPressureStartTime = 0;
					lastWaitTime = GetTime();
					waitTimeoutMs = 200;
					availableWeight = Min(availableWeight, 1.0f);
				}

				startInfos.clear();

				if (!SendProcessAvailable(startInfos, availableWeight))
				{
					m_loop = false;
					break;
				}
				++processRequestCount;

				if (!m_remoteExecutionEnabled)
				{
					m_logger.Info(TC("Got remote execution disabled response from host (will finish %llu active processes)"), startInfos.size() + activeProcesses.size());
				}

				if (startInfos.empty())
				{
					canSpawn = false;
					waitTimeoutMs = 200;
				}

				for (InternalProcessStartInfo& startInfo : startInfos)
				{
					startInfo.uiLanguage = int(m_uiLanguage);
					startInfo.priorityClass = m_defaultPriorityClass;
					startInfo.useCustomAllocator = !m_disableCustomAllocator;
					startInfo.rules = GetRules(startInfo);

					StringBuffer<> logFile;
					if (m_logToFile || (*startInfo.logFile && m_shouldSendLogToServer))
					{
						GetLogFileName(logFile, startInfo.logFile, startInfo.arguments, startInfo.processId);
						startInfo.logFile = logFile.data;
					}

					void* env = GetProcessEnvironmentVariables();

					auto process = new ProcessImpl(*this, startInfo.processId, nullptr, true);

					activeProcesses.emplace_back(process);
					ProcessRec* rec = &activeProcesses.back();

					rec->weight = startInfo.weight;

					{
						SCOPED_WRITE_LOCK(activeWeightLock, lock);
						activeWeight += rec->weight;
					}

					struct ExitedRec
					{
						ExitedRec(SessionClient& s, ReaderWriterLock& l, float& w, ProcessRec* r) : session(s), activeWeightLock(l), activeWeight(w), rec(r) {}
						SessionClient& session;
						ReaderWriterLock& activeWeightLock;
						float& activeWeight;
						ProcessRec* rec;
					};

					ExitedRec* exitedRec = new ExitedRec(*this, activeWeightLock, activeWeight, rec);
					startInfo.userData = exitedRec;
					startInfo.exitedFunc = [](void* userData, const ProcessHandle& h, ProcessExitedResponse& exitedResponse)
					{
						auto er = (ExitedRec*)userData;
						SessionClient& session = er->session;
						ReaderWriterLock& activeWeightLock = er->activeWeightLock;
						float& activeWeight = er->activeWeight;
						ProcessRec* rec = er->rec;
						delete er;

						auto& startInfo = h.GetStartInfo();
						if (session.m_shouldSendLogToServer)
							session.SendLogFileToServer(*(ProcessImpl*)h.m_process);

						float weight = rec->weight;
						auto decreaseWeight = MakeGuard([&]()
							{
								SCOPED_WRITE_LOCK(activeWeightLock, weightLock);
								activeWeight -= weight;
								session.m_waitToSendEvent.Set();
							});

						SCOPED_FUTEX(rec->lock, lock);
						auto doneGuard = MakeGuard([&]() { rec->isDone = true; session.m_waitToSendEvent.Set(); });

						if (rec->isKilled)
							return;

						auto& process = *(ProcessImpl*)h.m_process;

						if (session.m_killRandomIndex != ~0u && session.m_killRandomCounter++ == session.m_killRandomIndex)
						{
							session.m_loop = false;
							session.m_logger.Info(TC("Killed random process (%s)"), process.m_startInfo.GetDescription());
							return;
						}

						u32 exitCode = process.m_exitCode;

						if (exitCode != 0)
						{
							if (GetTime() >= session.m_terminationTime)
							{
								if (session.m_loop)
									session.SendReturnProcess(rec->handle.GetId(), session.m_terminationReason);
								return;
							}

							if (process.HasFailedMessage()) // If there are failure caused by failed messages we send back for retry
							{
								if (session.m_loop)
									session.SendReturnProcess(rec->handle.GetId(), StringBuffer<128>().Appendf(TC("Failed message (%s)"), ToString(process.m_firstMessageError)).data);
								return;
							}
						}

						if (exitCode == 0 || startInfo.writeOutputFilesOnFail)
						{
							// Should we decrease weight before or after sending files?
							//decreaseWeight.Execute();

							if (!session.SendFiles(process, process.m_processStats.sendFiles))
							{
								const tchar* desc = TC("Failed to send output files to host");
								session.m_logger.Error(desc);
								if (session.m_loop)
									session.SendReturnProcess(rec->handle.GetId(), desc);
								return;
							}
						}

						decreaseWeight.Execute();

						if (process.IsCancelled())
						{
							if (session.m_loop)
								session.SendReturnProcess(rec->handle.GetId(), TC("Cancelled"));
							return;
						}

						if (startInfo.trackInputs)
							session.SendProcessInputs(process);

						session.SendProcessFinished(process, exitCode);

						// TODO: These should be removed and instead added in TraceReader (so it will update over time)
						session.m_stats.stats.Add(process.m_sessionStats);
						session.m_storage.AddStats(process.m_storageStats);

						if (session.m_processFinished)
							session.m_processFinished(&process);
					};

					if (!process->Start(startInfo, true, env, true)) // If false, exitFunction is not called
					{
						SendReturnProcess(rec->handle.GetId(), TC("Failed to find executable"));
						activeProcesses.pop_back();
						delete exitedRec;
						m_remoteExecutionEnabled = false;
					}
				}

				RemoveInactiveProcesses();

				firstCall = false;
			}

			//SendUpdateNameToHashTable(); // It is always nice to populate this at certain cadence since it might speed up running processes queries

			m_waitToSendEvent.IsSet(waitTimeoutMs);

			RemoveInactiveProcesses();

			if (activeProcesses.empty() && !m_remoteExecutionEnabled)
			{
				// There can be processes that are done (isDone is true) but are still in m_processes list (since they are removed from that after). give them some time
				u64 counter = 300;
				while (true)
				{
					if (!counter--)
					{
						m_logger.Warning(TC("Took a long time for processes to be removed after being finished"));
						break;
					}

					SCOPED_FUTEX_READ(m_processesLock, processesLock);
					if (m_processes.empty())
						break;
					processesLock.Leave();
					Sleep(10);
				}
				break;
			}
		}

		CancelAllProcessesAndWait(); // If we got the exit from server there is no point sending anything more back.. cancel everything

		u32 retry = 0;
		while (true)
		{
			if (retry++ == 100)
			{
				m_logger.Error(TC("This should never happen!"));
				break;
			}
			RemoveInactiveProcesses();
			if (activeProcesses.empty())
				break;
			m_waitToSendEvent.IsSet(100);
		};


		m_client.FlushWork();


		StopTraceThread();

		if (m_shouldSendTraceToServer)
		{
			// Can't disable this, it can cause race conditions
			//m_client.SetWorkTracker(nullptr);

			StackBinaryWriter<SendMaxSize> writer;
			WriteSummary(writer, [&](Logger& logger)
				{
					PrintSummary(logger);
					m_storage.PrintSummary(logger);
					m_client.PrintSummary(logger);
					KernelStats::GetGlobal().Print(logger, true);
					PrintContentionSummary(logger);
				});
			m_trace.SessionSummary(0, writer.GetData(), writer.GetPosition());

			StringBuffer<> ubaFile(m_sessionLogDir);
			ubaFile.Append(TCV("Trace.uba"));
			if (StopTrace(ubaFile.data))
			{
				WrittenFile f;
				f.backedName = ubaFile.data;
				f.attributes = DefaultAttributes();
				StringBuffer<> dest(TC("<uba>"));
				f.name = dest.data;
				f.key = ToStringKeyLower(dest);

				SendFile(f, 0, false, true);
			}
		}
	}

	u32 SessionClient::WriteLogLines(BinaryWriter& writer, ProcessImpl& process)
	{
		u32 logLineCount = 0;
		for (auto& child : process.m_childProcesses)
			logLineCount += WriteLogLines(writer, *(ProcessImpl*)child.m_process);
		for (auto& line : process.m_logLines)
		{
			if (line.text.size()*sizeof(tchar) + 1000 >= writer.GetCapacityLeft())
				break;
			writer.WriteString(line.text);
			writer.WriteByte(line.type);
			++logLineCount;
		}
		return logLineCount;
	}

	bool SessionClient::ParseDirectoryTable()
	{
		SCOPED_WRITE_LOCK(m_directoryTable.m_lookupLock, lock);
		SCOPED_READ_LOCK(m_directoryTable.m_memoryLock, lock2);
		u32 newMemPosition = m_directoryTable.m_memorySize;
		lock2.Leave();
		if (newMemPosition == m_dirtableParsedPosition)
			return false;
		m_directoryTable.ParseDirectoryTableNoLock(m_dirtableParsedPosition, newMemPosition);
		m_dirtableParsedPosition = newMemPosition;
		return true;
	}

	bool SessionClient::EntryExists(const StringView& path, u32& outTableOffset)
	{
		StringKey key;
		if (path.data[path.count - 1] == PathSeparator)
			key = ToStringKey(path.data, path.count - 1);
		else
			key = ToStringKey(path);

		StringBuffer<> dirName;
		u32 tableOffset = 0;
		DirectoryTable::Exists exists = m_directoryTable.EntryExists(key, path, true, &tableOffset);
		if (exists == DirectoryTable::Exists_Maybe)
		{
			if (ParseDirectoryTable())
			{
				exists = m_directoryTable.EntryExists(key, path, true, &tableOffset);
			}

			if (exists == DirectoryTable::Exists_Maybe)
			{
				if (const tchar* lastSeparator = TStrrchr(path.data, PathSeparator))
					dirName.Append(path.data, lastSeparator - path.data);
				
				StringKey dirKey = ToStringKey(dirName);

				{	// Optimization to reduce number of messages
					SCOPED_FUTEX(m_dirVisitedLock, l);
					DirVisitedEntry& entry = m_dirVisited[dirKey];
					l.Leave();
				
					SCOPED_FUTEX(entry.lock, l2);
					if (!entry.handled)
					{
						ListDirectoryResponse out;
						if (!GetListDirectoryInfo(out, dirName, dirKey))
							return false;
						ParseDirectoryTable();
						entry.handled = true;
					}
				}

				exists = m_directoryTable.EntryExists(key, path, true, &tableOffset);
			}
		}

		UBA_ASSERTF(exists != DirectoryTable::Exists_Maybe, TC("This should not happen. Asking for directory %s"), dirName.data);

		if (exists != DirectoryTable::Exists_Yes)
			return false;

		outTableOffset = tableOffset;

		return true;
	}

	bool SessionClient::AllocFailed(Process& process, const tchar* allocType, u32 error)
	{
		//StackBinaryWriter<32> writer;
		//NetworkMessage msg(m_client, ServiceId, SessionMessageType_VirtualAllocFailed, writer);
		//if (!msg.Send())
		//	m_logger.Error(TC("Failed to send VirtualAllocFailed message!"));
		return Session::AllocFailed(process, allocType, error);
	}

	void SessionClient::PrintSessionStats(Logger& logger)
	{
		Session::PrintSessionStats(logger);
	}

	bool SessionClient::GetNextProcess(Process& process, bool& outNewProcess, NextProcessInfo& outNextProcess, u32 prevExitCode, BinaryReader& statsReader)
	{
		outNewProcess = false;

		if (!m_remoteExecutionEnabled)
			return true;

		auto& pi = (ProcessImpl&)process;

		if (!FlushWrittenFiles(pi))
			return false;

		ProcessStats processStats;
		processStats.Read(statsReader, TraceVersion);
		processStats.sendFiles = pi.m_processStats.sendFiles;

		StackBinaryReader<SendMaxSize> reader;
		StackBinaryWriter<16 * 1024> writer;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetNextProcess, writer);
		writer.WriteU32(pi.m_id);
		writer.WriteU32(prevExitCode);
		processStats.Write(writer);
		writer.WriteBytes(statsReader.GetPositionData(), statsReader.GetLeft());

		if (!msg.Send(reader, m_stats.customMsg))
			return false;

		outNewProcess = reader.ReadBool();
		if (outNewProcess)
		{
			if (m_shouldSendLogToServer)
				SendLogFileToServer(pi);

			pi.m_exitCode = prevExitCode;
			if (m_processFinished)
				m_processFinished(&process);

			outNextProcess.arguments = reader.ReadString();
			outNextProcess.workingDir = reader.ReadString();
			outNextProcess.description = reader.ReadString();
			outNextProcess.logFile = reader.ReadString();

			if (m_logToFile || (!outNextProcess.logFile.empty() && m_shouldSendLogToServer))
			{
				StringBuffer<512> logFile;
				GetLogFileName(logFile, outNextProcess.logFile.c_str(), outNextProcess.arguments.c_str(), process.GetId());
				outNextProcess.logFile = logFile.data;
			}
		}

		return SendUpdateDirectoryTable(reader.Reset());
	}

	bool SessionClient::CustomMessage(Process& process, BinaryReader& reader, BinaryWriter& writer)
	{
		StackBinaryWriter<SendMaxSize> msgWriter;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_Custom, msgWriter);

		u32 recvSize = reader.ReadU32();
		msgWriter.WriteU32(process.GetId());
		msgWriter.WriteU32(recvSize);
		msgWriter.WriteBytes(reader.GetPositionData(), recvSize);

		BinaryReader msgReader(writer.GetData(), 0);
		if (!msg.Send(msgReader, m_stats.customMsg))
			return false;

		u32 responseSize = msgReader.ReadU32();
		writer.AllocWrite(4ull + responseSize);
		return true;
	}

	bool SessionClient::SHGetKnownFolderPath(Process& process, BinaryReader& reader, BinaryWriter& writer)
	{
#if PLATFORM_WINDOWS
		StackBinaryWriter<SendMaxSize> msgWriter;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_SHGetKnownFolderPath, msgWriter);
		msgWriter.WriteBytes(reader.GetPositionData(), reader.GetLeft());
		BinaryReader msgReader(writer.GetData(), 0);
		if (!msg.Send(msgReader, m_stats.customMsg))
		{
			writer.WriteU32(u32(E_FAIL));
			return false;
		}
		writer.AllocWrite(msgReader.GetPosition());
#endif
		return true;
	}

	bool SessionClient::HostRun(BinaryReader& reader, BinaryWriter& writer)
	{
		const void* data = reader.GetPositionData();
		u64 size = reader.GetLeft();

		CasKey key = ToCasKey(CasKeyHasher().Update(data, size), false);

		SCOPED_FUTEX(m_hostRunCacheLock, l);
		auto insres = m_hostRunCache.try_emplace(key);
		auto& buffer = insres.first->second;
		if (!insres.second)
		{
			writer.WriteBytes(buffer.data(), buffer.size());
			return true;
		}

		StackBinaryWriter<SendMaxSize> msgWriter;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_HostRun, msgWriter);
		msgWriter.WriteBytes(data, size);
		BinaryReader msgReader(writer.GetData(), 0);
		if (!msg.Send(msgReader, m_stats.customMsg))
			return false;
		writer.AllocWrite(msgReader.GetLeft());

		buffer.resize(msgReader.GetLeft());
		memcpy(buffer.data(), msgReader.GetPositionData(), buffer.size());
		return true;
	}

	bool SessionClient::GetSymbols(const tchar* application, bool isArm, BinaryReader& reader, BinaryWriter& writer)
	{
		StackBinaryWriter<SendMaxSize> msgWriter;
		NetworkMessage msg(m_client, ServiceId, SessionMessageType_GetSymbols, msgWriter);
		msgWriter.WriteString(application);
		msgWriter.WriteBool(isArm);
		u32 size = reader.ReadU32();
		msgWriter.WriteU32(size);
		msgWriter.WriteBytes(reader.GetPositionData(), size);

		BinaryReader responseReader(writer.GetData(), 0, writer.GetCapacityLeft());
		if (!msg.Send(responseReader, m_stats.customMsg))
			return false;
		writer.AllocWrite(responseReader.GetLeft());

		if constexpr (DownloadDebugSymbols)
		{
			CasKey detoursSymbolsKey = responseReader.ReadCasKey();
			if (detoursSymbolsKey == CasKeyZero)
				return true;
			StringBuffer<128> symbolsFile(UBA_DETOURS_LIBRARY);
			#if PLATFORM_WINDOWS
			symbolsFile.Resize(symbolsFile.count - 3).Append("pdb");
			#else
			symbolsFile.Resize(symbolsFile.count - 2).Append("debug");
			#endif
			Storage::RetrieveResult result;
			StringBuffer<> throwaway;
			if (m_storage.RetrieveCasFile(result, AsCompressed(detoursSymbolsKey, false), symbolsFile.data))
				WriteBinFile(throwaway, symbolsFile, detoursSymbolsKey, KeyToString(StringKeyZero), DefaultAttributes());
		}
		return true;
	}


	bool SessionClient::FlushWrittenFiles(ProcessImpl& process)
	{
		SCOPED_FUTEX(process.m_shared.writtenFilesLock, lock);
		bool success = SendFiles(process, process.m_processStats.sendFiles);
		{
			SCOPED_FUTEX(m_outputFilesLock, lock2);
			for (auto& kv : process.m_shared.writtenFiles)
				m_outputFiles.erase(kv.second.name);
		}
		process.m_shared.writtenFiles.clear();

		return success;
	}

	bool SessionClient::UpdateEnvironment(ProcessImpl& process, const StringView& reason, bool resetStats)
	{
		StackBinaryReader<SendMaxSize> reader;

		if (resetStats)
		{
			StackBinaryWriter<16 * 1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_UpdateEnvironment, writer);
			writer.WriteU32(process.m_id);
			writer.WriteString(reason);
			process.m_processStats.Write(writer);
			process.m_sessionStats.Write(writer);
			process.m_storageStats.Write(writer);
			process.m_kernelStats.Write(writer);

			process.m_processStats = {};
			process.m_sessionStats = {};
			process.m_storageStats = {};
			process.m_kernelStats = {};

			if (!msg.Send(reader, m_stats.customMsg))
				return false;
			reader.Reset();
		}
		return SendUpdateDirectoryTable(reader);
	}

	bool SessionClient::LogLine(ProcessImpl& process, const tchar* line, LogEntryType logType)
	{
		// Remove this once we have figured out a bug that seems to exist for remote execution
		// Update: Bug has been found for macos... for windows we believe the bug is related to uninformed shutdown and having multiple tcp connections..
		// ... one tcp connection is disconnected, causing file not found while another connection manages to send "process finished"
#if 0 // PLATFORM_WINDOWS

		auto rules = process.m_startInfo.rules;
		if (!rules)
			return true;

		const tchar* errorPos = nullptr;
		if (rules->index == SpecialRulesIndex_ClExe)
		{
			if (!Contains(line, TC("C1083"), false, &errorPos))
				return true;
		}
		else
		{
			if (!Contains(line, TC("' file not found")))
				return true;
			if (!Contains(line, TC("fatal error: '"), false, &errorPos))
				return true;
		}

		const tchar* fileBegin = TStrchr(errorPos, '\'');
		if (!fileBegin)
			return true;
		++fileBegin;
		const tchar* fileEnd = TStrchr(fileBegin, '\'');
		if (!fileEnd)
			return true;

		MemoryBlock memoryBlock;
		DirectoryTable dirTable(&memoryBlock);
		{
			SCOPED_WRITE_LOCK(m_directoryTable.m_memoryLock, lock2);
			m_directoryTable.m_memorySize = m_directoryTableMemPos;
			dirTable.Init(m_directoryTable.m_memory, 0, m_directoryTable.m_memorySize);
		}

		StringBuffer<> errorPath;
		errorPath.Append(fileBegin, fileEnd - fileBegin).Replace('/', PathSeparator);

		{
			StackBinaryWriter<1024> writer;
			NetworkMessage msg(m_client, ServiceId, SessionMessageType_DebugFileNotFoundError, writer);
			writer.WriteString(errorPath);
			writer.WriteString(process.m_startInfo.workingDir);
			msg.Send();
		}

		StringView searchString = errorPath;
		if (searchString.data[0] == '.' && searchString.data[1] == '.')
		{
			searchString.data += 3;
			searchString.count -= 3;
		}

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
				process.LogLine(false, logStr.data, logType);

				StringKey fileNameKey = ToStringKey(path);
				SCOPED_READ_LOCK(m_fileMappingTableLookupLock, mlock);
				auto findIt = m_fileMappingTableLookup.find(fileNameKey);
				if (findIt != m_fileMappingTableLookup.end())
				{
					auto& entry = findIt->second;
					SCOPED_READ_LOCK(entry.lock, entryCs);
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
				process.LogLine(false, logStr.data, logType);

				CasKey key;
				if (GetCasKeyForFile(key, process.m_id, path, fileNameKey))
				{
					logStr.Clear().Appendf(TC("File %s caskey is %s."), path.data, CasKeyString(key).str);

					StringBuffer<512> casKeyFile;
					if (m_storage.GetCasFileName(casKeyFile, key))
					{
						logStr.Appendf(TC(" CasKeyFile: %s"), casKeyFile.data);
						u64 size = 0;
						u32 attributes = 0;
						bool exists = FileExists(m_logger, casKeyFile.data, &size, &attributes);
						logStr.Appendf(TC(" Exists: %s"), ToString(exists));
						if (exists)
						{
							logStr.Appendf(TC(" Size: %llu Attr: %u"), size, attributes);

							FileHandle fileHandle = uba::CreateFileW(casKeyFile.data, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, DefaultAttributes());
							if (fileHandle == InvalidFileHandle)
							{
								logStr.Appendf(TC(" Failed to open file %s (%s)"), casKeyFile.data, LastErrorToText().data);
							}
							else
							{
								logStr.Appendf(TC(" CreateFile for read successful"));
								uba::CloseFile(casKeyFile.data, fileHandle);
							}
						}
					}
					else
						logStr.Appendf(TC(" Failed to get cas filename for cas key"));
				}
				else
					logStr.Clear().Appendf(TC("File %s caskey not found"), path.data);
				process.LogLine(false, logStr.data, logType);
			});

		if (!foundCount)
		{
			StringBuffer<> logStr;
			logStr.Appendf(TC("No matching entry found in directory table while searching for matches for %s. DirTable size: %u"), searchString.data, GetDirectoryTableSize());
			process.LogLine(false, logStr.data, logType);
			if (errorPath.StartsWith(TC("..\\Intermediate")))
			{
				auto workDir = process.m_startInfo.workingDir;
				StringBuffer<> fullPath;
				FixPath(errorPath.data, workDir, TStrlen(workDir), fullPath);
			}
		}
#endif
		return true;
	}

	void SessionClient::TraceSessionUpdate()
	{
		if (m_loop && m_sendPing)
			SendPing(m_memAvail, m_memTotal);

		// TODO: There should be some sort of log entry if we have a process that has done no progress for 30 minutes

		if (!m_trace.IsWriting())
			return;

		u64 send;
		u64 recv;
		if (auto backend = m_client.GetFirstConnectionBackend())
		{
			backend->GetTotalSendAndRecv(send, recv);
		}
		else
		{
			recv = m_client.GetTotalRecvBytes();
			send = m_client.GetTotalSentBytes();
		}

		// send and recv are swapped on purpose because that is how visualizer is visualizing 
		m_trace.SessionUpdate(0, 0, send, recv, m_lastPing, m_memAvail, m_memTotal, m_cpuUsage);
	}
}

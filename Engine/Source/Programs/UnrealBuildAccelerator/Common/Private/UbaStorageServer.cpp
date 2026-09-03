// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaStorageServer.h"
#include "UbaConfig.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkServer.h"
#include "UbaTrace.h"

namespace uba
{
	void StorageServerCreateInfo::Apply(const Config& config)
	{
		StorageCreateInfo::Apply(config);
		const ConfigTable* tablePtr = config.GetTable(TC("Storage"));
		if (!tablePtr)
			return;
		const ConfigTable& table = *tablePtr;
		table.GetValueAsBool(writeReceivedCasFilesToDisk, TC("WriteReceivedCasFilesToDisk"));
		table.GetValueAsBool(allowHintAsFallback, TC("AllowHintAsFallback"));
		table.GetValueAsBool(createIndependentMappings, TC("CreateIndependentMappings"));
	}

	StorageServer::StorageServer(const StorageServerCreateInfo& info)
	:	StorageImpl(info, TC("UbaStorageServer"))
	,	m_server(info.server)
	{
		m_zone = info.zone;
		m_allowHintAsFallback = info.allowHintAsFallback;
		m_writeReceivedCasFilesToDisk = info.writeReceivedCasFilesToDisk;
		m_createIndependentMappings = info.createIndependentMappings;

		if (!CreateGuid(m_uid))
			UBA_ASSERT(false);

		m_server.RegisterService(ServiceId,
			[this](const ConnectionInfo& connectionInfo, const WorkContext& workContext, MessageInfo& messageInfo, BinaryReader& reader, BinaryWriter& writer)
			{
				return HandleMessage(connectionInfo, workContext, messageInfo.type, reader, writer);
			},
			[](u8 messageType)
			{
				return ToString(StorageMessageType(messageType));
			}
		);

		m_server.RegisterOnClientConnected(ServiceId, [this](const Guid& clientUid, u32 clientId)
			{
				SCOPED_FUTEX(m_loadCasTableLock, lock);
				if (!m_casTableLoaded)
					LoadCasTable(true);
			});

		m_server.RegisterOnClientDisconnected(ServiceId, [this](const Guid& clientUid, u32 clientId)
			{
				OnDisconnected(clientId);
			});
	}

	StorageServer::~StorageServer()
	{
		WaitForActiveWork();
		UBA_ASSERT(m_waitEntries.empty());
		UBA_ASSERT(m_proxies.empty());
		m_server.UnregisterOnClientDisconnected(ServiceId);
		m_server.UnregisterService(ServiceId);
	}

	bool StorageServer::RegisterDisallowedPath(const tchar* path)
	{
		m_disallowedPaths.push_back(path);
		return true;
	}

	void StorageServer::WaitForActiveWork()
	{
		while (m_activeUnmap)
			m_server.DoAdditionalWork();
	}

	bool StorageServer::GetZone(StringBufferBase& out)
	{
		if (m_zone.empty())
			return false;
		out.Append(m_zone);
		return true;
	}

	bool StorageServer::RetrieveCasFile(RetrieveResult& out, const CasKey& casKey, const tchar* hint, FileMappingBuffer* mappingBuffer, u64 memoryMapAlignment, bool allowProxy, u32 clientId)
	{
		UBA_ASSERT(!mappingBuffer);
		UBA_ASSERT(casKey != CasKeyZero);
		out.casKey = casKey;
		out.size = InvalidValue;

		CasKey actualKey = casKey;

		// This must be true because code handling storing files from remotes does not have decompression support
		// .. causing mix of compressed and non-compressed cas files with too many permutations
		UBA_ASSERT(m_storeCompressed);

		SCOPED_FUTEX(m_waitEntriesLock, waitLock);
		WaitEntry& waitEntry = m_waitEntries[actualKey];
		++waitEntry.refCount;
		waitLock.Leave();

		auto g = MakeGuard([&]()
			{
				SCOPED_FUTEX(m_waitEntriesLock, waitLock2);
				if (!--waitEntry.refCount)
					m_waitEntries.erase(actualKey);
			});

		if (HasCasFile(actualKey))
			return true;

		u64 startTime = GetTime();
		u32 timeout = 0;
		while (!waitEntry.done.IsSet(timeout)) // TODO WaitMultipleObjects (additional work and Done)
		{
			timeout = m_server.DoAdditionalWork() ? 0 : 50;

			u64 waited = GetTime() - startTime;
			u64 waitedMs = TimeToMs(waited);
			if (waitedMs > 500)
			{
				if (!m_server.IsConnectedSlow(clientId))
					return m_logger.Info(TC("Client with id %u disconnected while waiting for cas %s (%s)"), clientId, CasKeyString(casKey).str, hint).ToFalse();
				if (waitedMs > 4 * 60 * 1000) // 4 minutes timeout
					return m_logger.Info(TC("Timed out waiting %s for cas %s to be transferred from remote to storage (%s)"), TimeToText(waited).str, CasKeyString(casKey).str, hint).ToFalse();
			}
		}
		return waitEntry.Success;
	}

	void StorageServer::ActiveFetch::Release(StorageServer& server, const tchar* reason)
	{
		if (mappedView.handle.IsValid())
		{
			if (ownsMapping)
			{
				++server.m_activeUnmap;
				server.GetServer().AddWork([&server, mb = memoryBegin, mp = mappedView, rfh = readFileHandle, reason](const WorkContext& context)
					{
						if (!UnmapViewOfFile(server.m_logger, mb, mp.size, reason))
							server.m_logger.Error(TC("%s - Failed to unmap memory at 0x%llx with size %llu (%s)"), reason, mb, mp.size, LastErrorToText().data);
						if (!CloseFileMapping(server.m_logger, mp.handle, reason))
							server.m_logger.Error(TC("%s - Failed to close file mapping %llu (%s)"), reason, mp.handle.ToU64(), LastErrorToText().data);
						if (!CloseFile(nullptr, rfh))
							server.m_logger.Error(TC("%s - Failed to close file (%s)"), reason, LastErrorToText().data);

						--server.m_activeUnmap;
					}, 1, TC("ActiveFetchRelease"));
			}
			else
				server.m_casDataBuffer.UnmapView(mappedView, reason);
		}
		else
		{
			if (!memoryBegin)
				server.m_logger.Warning(TC("This should not happen. It means there is a race between a fetch and a disconnect. Report to honk (%s)"), reason);
			server.m_bufferSlots.Push((u8*)memoryBegin);
			memoryBegin = nullptr;
		}

		SCOPED_WRITE_LOCK(casEntry->lock, entryLock);
		--casEntry->readCount;
	}

	void StorageServer::OnDisconnected(u32 clientId)
	{
		{
			SCOPED_WRITE_LOCK(m_proxiesLock, lock);
			for (auto it=m_proxies.begin(); it!=m_proxies.end(); ++it)
			{
				ProxyEntry& e = it->second;
				if (e.clientId != clientId)
					continue;
				m_logger.Detail(TC("Proxy %s:%u for zone %s removed"), e.host.c_str(), e.port, e.zone.c_str());
				m_proxies.erase(it);
				break;
			}
		}
		{
			SCOPED_WRITE_LOCK(m_activeStoresLock, lock);
			for (auto it=m_activeStores.begin(); it!=m_activeStores.end();)
			{
				ActiveStore& store = it->second;
				if (store.clientId != clientId)
				{
					++it;
					continue;
				}

				{
					SCOPED_WRITE_LOCK(store.casEntry->lock, entryLock);
					store.casEntry->verified = false;
					store.casEntry->beingWritten = false;
					if (m_traceStore)
						m_trace->FileStoreEnd(clientId, store.casEntry->key);
				}

				if (auto fa = store.fileAccessor)
				{
					const tchar* filename = fa->GetFileName();
					delete store.fileAccessor;
					free((void*)filename);
				}

				m_casDataBuffer.UnmapView(store.mappedView, TC("OnDisconnected"));
				if (store.mappedView.isIndependent)
					CloseFileMapping(m_logger, store.mappedView.handle, TC("StoreDone"));
				it = m_activeStores.erase(it);
			}
		}
		{
			SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
			for (auto it=m_activeFetches.begin(); it!=m_activeFetches.end();)
			{
				ActiveFetch& fetch = it->second;
				if (fetch.clientId != clientId)
				{
					++it;
					continue;
				}

				m_logger.Detail(TC("Cancelled fetch id %u because of disconnect of client with id %u"), u32(it->first), clientId);

				fetch.Release(*this, TC("OnDisconnected"));

				if (m_traceFetch)
					m_trace->FileFetchEnd(clientId, AsCompressed(fetch.casEntry->key, m_storeCompressed));

				it = m_activeFetches.erase(it);
			}
		}
	}

	bool StorageServer::IsDisallowedPath(const tchar* fileName)
	{
		for (auto& path : m_disallowedPaths)
			if (StartsWith(fileName, path.c_str()))
				return true;
		return false;
	}

	void StorageServer::SetTrace(Trace* trace, bool detailed)
	{
		m_trace = trace;
		m_traceFetch = detailed;
		m_traceStore = detailed;
	}

	bool StorageServer::HasProxy(u32 clientId)
	{
		SCOPED_READ_LOCK(m_proxiesLock, l);
		for (auto& kv : m_proxies)
			if (kv.second.clientId == clientId)
				return true;
		return false;
	}

	bool StorageServer::WaitForWritten(CasEntry& casEntry, ScopedWriteLock& entryLock, const ConnectionInfo& connectionInfo, const tchar* hint)
	{
		int waitCount = 0;
		while (true)
		{
			if (!casEntry.beingWritten)
				return true;
			CasKey key = casEntry.key;
			entryLock.Leave();
			Sleep(100);
			entryLock.Enter();

			if (++waitCount < 12*60*10)
				continue;

			// Something went wrong.. should not take 12 minutes to write a file

			SCOPED_READ_LOCK(m_activeStoresLock, activeLock);
			for (auto& kv : m_activeStores)
			{
				if (kv.second.casEntry != &casEntry)
					continue;
				ActiveStore& as = kv.second;
				return m_logger.Error(TC("Client %u waited more than 12 minutes for file %s (%s) to be written by client %u (Written %llu/%llu)"), connectionInfo.GetId(), CasKeyString(key).str, hint, as.clientId, as.totalWritten.load(), as.fileSize);
			}
			return m_logger.Error(TC("Client %u waited more than 12 minutes for file %s (%s) to be written but there are no active writes. This should not be possible!"), connectionInfo.GetId(), CasKeyString(key).str, hint);
		}
	}

	bool StorageServer::VerifyExists(bool& outExists, CasEntry& casEntry, ScopedWriteLock& entryLock, const CasKey& casKey)
	{
		outExists = false;
		StringBuffer<> casFile;
		if (!GetCasFileName(casFile, casKey))
			return false;
		u64 outFileSize = 0;
		if (FileExists(m_logger, casFile.data, &outFileSize))
		{
			if (outFileSize == 0 && casKey != EmptyFileKey)
			{
				m_logger.Warning(TC("Found file %s with size 0 which did not have the zero-size-caskey. Deleting"), casFile.data);
				if (!uba::DeleteFileW(casFile.data))
					return m_logger.Error(TC("Failed to delete %s. Clean cas folder and restart"), casFile.data);
				casEntry.exists = false;
				casEntry.verified = true;
			}
			else
			{
				casEntry.verified = true;
				outExists = true;
				entryLock.Leave();
				CasEntryWritten(casEntry, outFileSize);
			}
		}
		else
		{
			casEntry.exists = false;
			casEntry.verified = true;
		}
		return true;
	}

	bool StorageServer::GetProxy(BinaryWriter& writer, u32 clientId, bool writeCasHeader)
	{
		SCOPED_READ_LOCK(m_connectionInfoLock, lock);
		auto findIt = m_connectionInfo.find(clientId);
		UBA_ASSERT(findIt != m_connectionInfo.end());
		Info& info = findIt->second;
		lock.Leave();

		if (info.zone.empty())
			return false;

		// Zone logic is a bit special to be able to handle AWS setups.
		// AWS has zones named a,b,c etc in the end. us-east-1a, us-east-1b etc.
		// If host is also in AWS, then we want different proxies per zone but if host is not in AWS, we want one proxy for all zones in a region

		StringBuffer<256> proxyName;

		if (!m_zone.empty() && info.zone.size() == m_zone.size() && Equals(m_zone.c_str(), info.zone.c_str(), u64(m_zone.size() - 1)))
		{
			// Host is inside AWS and same region.... if aws availability zone is different than host, then we use proxy
			if (m_zone != info.zone)
				proxyName.Append(info.zone.c_str());
		}
		else if (m_zone != info.zone)
		{
			// We remove last character from zone to make sure all AWS availability zones in the same region use same proxy if host zone is outside AWS
			proxyName.Append(info.zone.c_str(), info.zone.size() - 1);
		}

		if (proxyName.IsEmpty())
			return false;

		if (writeCasHeader)
		{
			writer.WriteU16(u16(~0));
			writer.Write7BitEncoded(0);
			writer.WriteByte(1 << 2);
		}

		auto proxyKey = ToStringKeyNoCheck(proxyName.data, proxyName.count);
		SCOPED_WRITE_LOCK(m_proxiesLock, proxiesLock);
		ProxyEntry& proxy = m_proxies[proxyKey];
		if (proxy.clientId == ~0u)
		{
			proxy.clientId = clientId;
			proxy.host = info.internalAddress;
			proxy.port = info.proxyPort;
			proxy.zone = proxyName.data;

			m_logger.Detail(TC("%s:%u (client %u) is assigned as proxy for zone %s"), proxy.host.c_str(), proxy.port, clientId, proxy.zone.c_str());

			writer.WriteBool(true);
			if (m_trace)
				m_trace->ProxyCreated(proxy.clientId, proxyName.data);
		}
		else
		{
			const tchar* proxyHost = proxy.host.c_str();
			if (clientId == proxy.clientId)
				proxyHost = TC("inprocess");

			writer.WriteBool(false);
			writer.WriteString(proxyHost);
			if (m_trace)
				m_trace->ProxyUsed(clientId, proxyName.data);
		}

		writer.WriteU16(proxy.port);
		writer.WriteU32(proxy.clientId);
		return true;
	}

	bool StorageServer::HandleMessage(const ConnectionInfo& connectionInfo, const WorkContext& workContext, u8 messageType, BinaryReader& reader, BinaryWriter& writer)
	{
		ActiveStore* firstStore = nullptr;
		ActiveStore tempStore;

		switch (messageType)
		{
			case StorageMessageType_Connect:
			{
				StringBuffer<> clientName;
				reader.ReadString(clientName);
				u32 clientVersion = reader.ReadU32();
				if (clientVersion != StorageNetworkVersion)
					return m_logger.Error(TC("Different network versions. Client: %u, Server: %u. Disconnecting"), clientVersion, StorageNetworkVersion);

				if (reader.ReadBool()) // is proxy
					return m_logger.Error(TC("Proxy is sending connect message. This path is not implemented"));
				u16 proxyPort = reader.ReadU16();
				SCOPED_WRITE_LOCK(m_connectionInfoLock, lock);
				Info& info = m_connectionInfo[connectionInfo.GetId()];
				info.zone = reader.ReadString();
				info.storageSize = reader.ReadU64();
				info.internalAddress = reader.ReadString();
				info.proxyPort = proxyPort;

				writer.WriteGuid(m_uid);
				writer.WriteByte(m_casCompressor);
				writer.WriteByte(m_casCompressionLevel);
				return true;
			}

			case StorageMessageType_FetchBegin:
			{
				// TODO: Remove this when we have tracked down the issue where clients time out
				u32 todoRemoveMe = 0;
				auto timeoutGuard = MakeGuard([&, timeoutStartTime = GetTime()]()
					{
						u64 timeSpentMs = TimeToMs(GetTime() - timeoutStartTime);
						if (timeSpentMs > 8 * 60 * 1000)
						{
							// Took more than 5 minutes to respond
							m_logger.Warning(TC("Took more than 8 minutes to respond to FetchBegin to client %u (Hang code %u).."), connectionInfo.GetId(), todoRemoveMe);
						}
					});

				u8 recvFlags = reader.ReadByte();
				bool wantsProxy = (recvFlags & 1) != 0;
				if (wantsProxy) // Wants proxy
				{
					todoRemoveMe = 1;
					if (GetProxy(writer, connectionInfo.GetId(), true))
						return true;
				}

				todoRemoveMe = 2;

				u64 start = GetTime();
				CasKey casKey = reader.ReadCasKey();
				StringBuffer<> hint;
				reader.ReadString(hint);

				workContext.tracker.AddHint(StringView(hint).GetFileName());

				casKey = AsCompressed(casKey, m_storeCompressed);

				bool detailedTrace = m_traceFetch;
				if (detailedTrace)
					m_trace->FileFetchBegin(connectionInfo.GetId(), casKey, hint);
				auto traceFetchGuard = MakeGuard([&](){ if (detailedTrace) m_trace->FileFetchEnd(connectionInfo.GetId(), casKey); });


				CasEntry* casEntry = nullptr;
				bool has = HasCasFile(casKey, &casEntry); // HasCasFile also writes deferred cas entries if in queue
				if (!has)
				{
					todoRemoveMe = 3;
					if (!EnsureCasFile(casKey, nullptr) && m_allowHintAsFallback)
					{
						// Last resort.. use hint to load file into cas (hint should be renamed since it is now a critical parameter)
						// We better check the caskey first to make sure it is matching on the server

						StringKey fileNameKey = CaseInsensitiveFs ? ToStringKeyLower(hint) : ToStringKey(hint);
						CasKey checkedCasKey;
						{
							SCOPED_READ_LOCK(m_fileTableLookupLock, lookupLock);
							auto findIt = m_fileTableLookup.find(fileNameKey);
							if (findIt != m_fileTableLookup.end())
							{
								FileEntry& fileEntry = findIt->second;
								lookupLock.Leave();
								SCOPED_FUTEX_READ(fileEntry.lock, entryLock);
								if (fileEntry.verified)
									checkedCasKey = fileEntry.casKey;
							}
						}
						if (checkedCasKey == CasKeyZero)
						{
							m_logger.Info(TC("Server did not find cas %s for %s in file table lookup. Recalculating cas key"), CasKeyString(casKey).str, hint.data);
							if (!CalculateCasKey(checkedCasKey, hint.data))
							{
								m_logger.Error(TC("FetchBegin failed for cas file %s (%s) requested by %s. Can't calculate cas key for file"), CasKeyString(casKey).str, hint.data, GuidToString(connectionInfo.GetUid()).str);
								writer.WriteU16(0);
								return false;
							}
						}

						if (AsCompressed(checkedCasKey, m_storeCompressed) != casKey)
						{
							m_logger.Error(TC("FetchBegin failed for cas file %s (%s). File on disk has different cas %s"), CasKeyString(casKey).str, hint.data, CasKeyString(checkedCasKey).str);
							writer.WriteU16(0);
							return false;
						}

						bool deferCreation = false;
						if (!AddCasFile(fileNameKey, hint.data, casKey, deferCreation))
						{
							m_logger.Error(TC("FetchBegin failed for cas file %s (%s). Can't add cas file to database"), CasKeyString(casKey).str, hint.data);
							writer.WriteU16(0);
							return true;
						}
					}
					SCOPED_WRITE_LOCK(m_casLookupLock, lookupLock);
					auto findIt = m_casLookup.find(casKey);
					if (findIt == m_casLookup.end())
					{
						writer.WriteU16(0);
						return true;
					}
					casEntry = &findIt->second;
				}

				SCOPED_WRITE_LOCK(casEntry->lock, entryLock);
				if (!casEntry->exists)
				{
					m_logger.Info(TC("FetchBegin failed for cas file %s (%s). Cas was deleted"), CasKeyString(casKey).str, hint.data);
					writer.WriteU16(0);
					return true;
				}
				++casEntry->readCount;
				entryLock.Leave();
				auto rlg = MakeGuard([&]() { entryLock.Enter(); --casEntry->readCount; });

				todoRemoveMe = 4;

				if (casEntry->disallowed)
				{
					writer.WriteU16(FetchCasIdDisallowed);
					m_logger.Warning(TC("Client %u is asking for cas content of file that is not allowed to be transferred (%s)"), connectionInfo.GetId(), hint.data);
					return true;
				}
				
				StringBuffer<512> casFile;
				FileHandle readFileHandle = InvalidFileHandle;
				auto rfg = MakeGuard([&]()
					{
						if (!CloseFile(nullptr, readFileHandle))
							m_logger.Error(TC("Failed to close file %s (%s)"), casFile.data, LastErrorToText().data);
					});
				u64 fileSize = 0;
				const u8* memoryBegin = nullptr;
				const u8* memoryPos = nullptr;
				bool ownsMapping = false;

				MappedView mappedView;
				auto mvg = MakeGuard([&]()
					{
						if (ownsMapping)
						{
							if (mappedView.memory)
								if (!UnmapViewOfFile(m_logger, mappedView.memory, fileSize, TC("FetchBegin")))
									m_logger.Error(TC("Failed to unmap memory %s at 0x%llx with size %llu (%s)"), hint.data, mappedView.memory, fileSize, LastErrorToText().data);
							if (mappedView.handle.IsValid())
								if (!CloseFileMapping(m_logger, mappedView.handle, hint.data))
									m_logger.Error(TC("Failed to close file mapping for %s with handle %llu (%s)"), hint.data, mappedView.handle.ToU64(), LastErrorToText().data);
						}
						else
							m_casDataBuffer.UnmapView(mappedView, TC("FetchBegin"));
					});

				todoRemoveMe = 5;

				bool useFileMapping = casEntry->mappingHandle.IsValid();
				if (useFileMapping)
				{
					mappedView = m_casDataBuffer.MapView(casEntry->mappingHandle, casEntry->mappingOffset, casEntry->mappingSize, CasKeyString(casKey).str, casEntry->mappingIsIndependent);
					memoryBegin = mappedView.memory;
					fileSize = casEntry->mappingSize;
					if (!memoryBegin)
						return m_logger.Error(TC("Failed to map memory map for %s. Will use file handle instead (%s)"), CasKeyString(casKey).str, LastErrorToText().data);
					memoryPos = memoryBegin;
				}
				else
				{
					GetCasFileName(casFile, casKey);
					if (!OpenFileSequentialRead(m_logger, casFile.data, readFileHandle))
					{
						writer.WriteU16(0);
						return true;
					}

					if (!uba::GetFileSizeEx(fileSize, readFileHandle))
						return m_logger.Error(TC("GetFileSizeEx failed on file %s (%s)"), casFile.data, LastErrorToText().data);

					if (fileSize > BufferSlotSize)
					{
						ownsMapping = true;
						mappedView.handle = CreateFileMappingW(m_logger, readFileHandle, PAGE_READONLY, fileSize, casFile.data);
						if (!mappedView.handle.IsValid())
							return m_logger.Error(TC("Failed to create file mapping of %s (%s)"), casFile.data, LastErrorToText().data);
						u64 offset = memoryPos - memoryBegin;
						mappedView.memory = MapViewOfFile(m_logger, mappedView.handle, FILE_MAP_READ, 0, fileSize);
						if (!mappedView.memory)
							return m_logger.Error(TC("Failed to map memory of %s with size %llu (%s)"), casFile.data, fileSize, LastErrorToText().data);
						memoryBegin = mappedView.memory;
						memoryPos = memoryBegin + offset;
						useFileMapping = true;
					}
				}

				todoRemoveMe = 6;

				if (detailedTrace)
					m_trace->FileFetchSize(connectionInfo.GetId(), casKey, fileSize);
				else if (m_trace)
					m_trace->FileFetchLight(connectionInfo.GetId(), casKey, fileSize);

				u64 left = fileSize;

				u16* fetchId = (u16*)writer.AllocWrite(sizeof(u16));
				*fetchId = 0;
				writer.Write7BitEncoded(fileSize);
				u8 flags = 0;
				flags |= u8(m_storeCompressed) << 0;
				flags |= u8(m_traceFetch)    << 1;
				writer.WriteByte(flags);

				u64 capacityLeft = writer.GetCapacityLeft();
				u32 toWrite = u32(Min(left, capacityLeft));
				void* writeBuffer = writer.AllocWrite(toWrite);

				todoRemoveMe = 7;

				if (useFileMapping)
				{
					memcpy(writeBuffer, memoryPos, toWrite);
					memoryPos += toWrite;
				}
				else if (toWrite == left)
				{
					if (!ReadFile(m_logger, casFile.data, readFileHandle, writeBuffer, toWrite))
					{
						UBA_ASSERT(false); // Implement
						return m_logger.Error(TC("Failed to read file %s (%s) (1)"), casFile.data, LastErrorToText().data);;
					}
				}
				else
				{
					u8* buffer = m_bufferSlots.Pop();;
					memoryBegin = buffer;
					memoryPos = memoryBegin;
					u32 toRead = u32(Min(left, BufferSlotSize));
 					if (!ReadFile(m_logger, casFile.data, readFileHandle, buffer, toRead))
					{
						UBA_ASSERT(false); // Implement
						return m_logger.Error(TC("Failed to read file %s (%s) (2)"), casFile.data, LastErrorToText().data);;
					}
					memcpy(writeBuffer, memoryPos, toWrite);
					memoryPos += toWrite;

					if (!CloseFile(casFile.data, readFileHandle))
						m_logger.Error(TC("Failed to close file %s (%s)"), casFile.data, LastErrorToText().data);;
					readFileHandle = InvalidFileHandle;
				}

				todoRemoveMe = 8;

				u64 actualSize = fileSize;
				if (m_storeCompressed)
					actualSize = *(u64*)writeBuffer;

				StorageStats& stats = Stats();
				stats.sendCasBytesComp += fileSize;
				stats.sendCasBytesRaw += actualSize;

				left -= toWrite;

				traceFetchGuard.Cancel();

				if (!left)
				{
					*fetchId = FetchCasIdDone;
					u64 sendCasTime = GetTime() - start;
					stats.sendCas += Timer{sendCasTime, 1};
					return true;
				}

				todoRemoveMe = 9;

				rlg.Cancel();
				mvg.Cancel();
				rfg.Cancel();

				*fetchId = PopId();

				SCOPED_WRITE_LOCK(m_activeFetchesLock, lock);
				auto insres = m_activeFetches.try_emplace(*fetchId);
				UBA_ASSERT(insres.second);
				ActiveFetch& fetch = insres.first->second;
				fetch.clientId = connectionInfo.GetId();
				lock.Leave();

				todoRemoveMe = 10;

				mappedView.size = fileSize;

				fetch.readFileHandle = readFileHandle;
				fetch.mappedView = mappedView;
				fetch.ownsMapping = ownsMapping;
				fetch.memoryBegin = memoryBegin;
				fetch.memoryPos = memoryPos;
				fetch.left = left;
				fetch.casEntry = casEntry;
				fetch.sendCasTime = GetTime() - start;

				return true;
			}
			case StorageMessageType_FetchSegment:
			{
				u64 start = GetTime();
				u16 fetchId = reader.ReadU16();
				u32 fetchIndex = reader.ReadU32();

				SCOPED_READ_LOCK(m_activeFetchesLock, lock);
				auto findIt = m_activeFetches.find(fetchId);
				if (findIt == m_activeFetches.end())
					return m_logger.Detail(TC("Can't find active fetch %u, disconnected client? (fetch index %u, client id %u uid %s)"), fetchId, fetchIndex, connectionInfo.GetId(), GuidToString(connectionInfo.GetUid()).str).ToFalse();
				ActiveFetch& fetch = findIt->second;
				UBA_ASSERT(fetch.clientId == connectionInfo.GetId());
				lock.Leave();

				UBA_ASSERT(fetchIndex);
				const u8* pos = fetch.memoryPos + (fetchIndex-1) * writer.GetCapacityLeft();
				u64 toWrite = writer.GetCapacityLeft();
				u64 readOffset = pos - fetch.memoryBegin;
				u64 viewSize = fetch.mappedView.size;
				if (readOffset + toWrite > viewSize)
				{
					if (readOffset >= viewSize)
						return m_logger.Detail(TC("Client is asking for more data than file contains (readoffset %llu, size %llu, fetch index %u, client id %u uid %s)"), readOffset, viewSize, fetchIndex, connectionInfo.GetId(), GuidToString(connectionInfo.GetUid()).str).ToFalse();
					toWrite = viewSize - readOffset;
				}
				memcpy(writer.AllocWrite(toWrite), pos, toWrite);

				bool isDone = fetch.left.fetch_sub(toWrite) == toWrite;
				if (!isDone)
				{
					fetch.sendCasTime += GetTime() - start;
					return true;
				}

				fetch.Release(*this, TC("FetchDone"));

				u64 sendCasTime = fetch.sendCasTime;
				SCOPED_WRITE_LOCK(m_activeFetchesLock, activeLock);
				m_activeFetches.erase(fetchId);
				activeLock.Leave();
				PushId(fetchId);

				sendCasTime += GetTime() - start;
				Stats().sendCas += Timer{sendCasTime, 1};
				return true;
			}

			case StorageMessageType_FetchEnd:
			{
				CasKey key = reader.ReadCasKey();
				if (m_traceFetch)
					m_trace->FileFetchEnd(connectionInfo.GetId(), AsCompressed(key, m_storeCompressed));
				return true;
			}

			case StorageMessageType_ExistsOnServer:
			{
				CasKey casKey = reader.ReadCasKey();
				//UBA_ASSERT(IsCompressed(casKey));
				SCOPED_WRITE_LOCK(m_casLookupLock, lookupLock);
				auto casInsres = m_casLookup.try_emplace(casKey, casKey);
				CasEntry& casEntry = casInsres.first->second;
				lookupLock.Leave();

				SCOPED_WRITE_LOCK(casEntry.lock, entryLock);
				
				if (!WaitForWritten(casEntry, entryLock, connectionInfo, TC("UNKNOWN")))
					return false;

				bool exists = casEntry.verified && casEntry.exists;

				if (!exists && casEntry.exists)
				{
					if (!VerifyExists(exists, casEntry, entryLock, casKey))
						return false;
				}
				writer.WriteBool(exists);
				return true;
			}

			case StorageMessageType_StoreBegin:
			{
				u64 start = GetTime();
				CasKey casKey = reader.ReadCasKey();
				u64 fileSize = reader.ReadU64();
				u64 actualSize = reader.ReadU64();
				//UBA_ASSERT(IsCompressed(casKey));
				StringBuffer<> hint;
				reader.ReadString(hint);

				SCOPED_WRITE_LOCK(m_casLookupLock, lookupLock);
				auto casInsres = m_casLookup.try_emplace(casKey, casKey);
				CasEntry& casEntry = casInsres.first->second;
				lookupLock.Leave();

				SCOPED_WRITE_LOCK(casEntry.lock, entryLock);

				{
					// Check if there is a deferred creation of this file, if there is we can just as well just use the local file
					SCOPED_FUTEX(m_deferredCasCreationLookupLock, deferredLock);
					auto findIt = m_deferredCasCreationLookup.find(casKey);
					if (findIt != m_deferredCasCreationLookup.end())
					{
						deferredLock.Leave();
						entryLock.Leave();
						if (HasCasFile(casKey))
						{
							writer.WriteU16(u16(~0));
							writer.WriteBool(m_traceStore);
							m_logger.Debug(TC("Client %u Store request of %s which already exists in casdb (was in deferred list) (%s)"), connectionInfo.GetId(), CasKeyString(casKey).str, hint.data);
							return true;
						}
						entryLock.Enter();
					}
				}


				if (!casEntry.verified)
				{
					if (bool exists = casEntry.exists)
					{
						if (!VerifyExists(exists, casEntry, entryLock, casKey))
							return false;
						if (exists)
						{
							// Unless writeReceivedCasFilesToDisk is set this is extremely unlikely to happen. "PerModuleInline.gen.cpp" seems to be the only one (they are supposed to all be the same)
							writer.WriteU16(u16(~0));
							writer.WriteBool(m_traceStore);
							m_logger.Debug(TC("Client %u Store request of %s which already exists in casdb (%s)"), connectionInfo.GetId(), CasKeyString(casKey).str, hint.data);
							return true;
						}
					}
					else
						casEntry.verified = true;
				}
				else
				{
					if (!WaitForWritten(casEntry, entryLock, connectionInfo, hint.data))
						return false;

					if (casEntry.exists)
					{
						entryLock.Leave();
						CasEntryAccessed(casEntry);
						writer.WriteU16(u16(~0));
						writer.WriteBool(m_traceStore);
						//m_logger.Debug(TC("Client %u Store request of %s which was just added to casdb (%s)"), connectionInfo.GetId(), CasKeyString(casKey).str, hint.data);
						return true;
					}
				}
				if (!fileSize)
				{
					m_logger.Error(TC("Client %u Store is of 0 size (%s)"), connectionInfo.GetId(), hint.data);
					casEntry.verified = false;
					return false;
				}

				MappedView mappedView;
				FileAccessor* fileAccessor = nullptr;
				
				if (m_writeReceivedCasFilesToDisk)
				{
					StringBuffer<> casKeyName;
					GetCasFileName(casKeyName, casKey);
					
					const tchar* filename = TStrdup(casKeyName.data);
					fileAccessor = new FileAccessor(m_logger, filename);
					if (!fileAccessor->CreateMemoryWrite(false, DefaultAttributes(), fileSize, m_tempPath.data))
					{
						delete fileAccessor;
						free((void*)filename);

						m_logger.Error(TC("Failed to create cas file %s (%s)"), casKeyName.data, hint.data);
						casEntry.verified = false;
						return false;
					}

					#ifdef __clang_analyzer__ // Seems clang analyzer gets lost
					free((void*)filename);
					#endif

					mappedView.memory = fileAccessor->GetData();
				}
				else
				{
					mappedView = m_casDataBuffer.AllocAndMapView(MappedView_Transient, fileSize, 1, CasKeyString(casKey).str, false, m_createIndependentMappings);
					if (!mappedView.memory)
					{
						casEntry.verified = false;
						return false;
					}
					casEntry.dropped = true; // Set to dropped because this will never hit disk
				}

				if (casEntry.beingWritten)
					return m_logger.Error(TC("Cas is being written while starting to write. %s (%s)"), CasKeyString(casKey).str, hint.data);

				casEntry.beingWritten = true;

				firstStore = &tempStore;

				*(u64*)mappedView.memory = fileSize;

				firstStore->casEntry  = &casEntry;
				firstStore->fileSize = fileSize;
				firstStore->actualSize = actualSize;
				firstStore->mappedView = mappedView;
				firstStore->fileAccessor = fileAccessor;
				firstStore->recvCasTime = GetTime() - start;

				if (m_trace)
					m_trace->FileStoreBegin(connectionInfo.GetId(), casKey, fileSize, hint, m_traceStore);

				[[fallthrough]];
			}
			case StorageMessageType_StoreSegment:
			{
				u64 start = GetTime();

				u16 storeId = 0;
				u64 memOffset = 0;
				ActiveStore* activeStoreTemp = firstStore;
				if (!firstStore) // If this is set we are a continuation from StorageMessageType_Begin
				{
					storeId = reader.ReadU16();
					memOffset = reader.ReadU64();
					SCOPED_READ_LOCK(m_activeStoresLock, activeLock);
					auto storeIt = m_activeStores.find(storeId);
					if (storeIt == m_activeStores.end())
						return m_logger.Error(TC("Can't find active store %u, disconnected client?"), storeId);
					activeStoreTemp = &storeIt->second;
					if (activeStoreTemp->clientId != connectionInfo.GetId())
						return m_logger.Error(TC("Client id not matching for active store %u, disconnected client? (%u vs %u)"), storeId, activeStoreTemp->clientId, connectionInfo.GetId());
				}
				ActiveStore& activeStore = *activeStoreTemp;

				u64 toRead = reader.GetLeft();
				u64 fileSize = activeStore.fileSize;

				if (fileSize < memOffset + toRead)
					return m_logger.Error(TC("Trying to write data to cas outside of size (Size %llu, writing %llu at offset %llu"), fileSize, memOffset, toRead);

				reader.ReadBytes(activeStore.mappedView.memory + memOffset, toRead);
				
				u64 time2 = GetTime();
				activeStore.recvCasTime += time2 - start;

				u64 totalWritten = activeStore.totalWritten.fetch_add(toRead) + toRead;
				if (totalWritten == fileSize)
				{
					m_casDataBuffer.UnmapView(activeStore.mappedView, TC("StoreDone"));

					if (activeStore.fileAccessor)
					{
						bool success = activeStore.fileAccessor->Close();
						const tchar* filename = activeStore.fileAccessor->GetFileName();
						delete activeStore.fileAccessor;
						free((void*)filename);
						if (!success)
							return m_logger.Error(TC("REVISIT THIS!"));
					}

					CasEntry& casEntry = *activeStore.casEntry;
					{
						SCOPED_WRITE_LOCK(casEntry.lock, entryLock);
						casEntry.mappingHandle = activeStore.mappedView.handle;
						casEntry.mappingOffset = activeStore.mappedView.offset;
						casEntry.mappingSize = totalWritten;
						casEntry.mappingIsIndependent = activeStore.mappedView.isIndependent;
						casEntry.exists = true;
						casEntry.beingWritten = false;
					}

					bool isPersistentStore = m_writeReceivedCasFilesToDisk;
					if (isPersistentStore)
						CasEntryWritten(casEntry, totalWritten);

					activeStore.recvCasTime += GetTime() - time2;

					StorageStats& stats = Stats();
					stats.recvCas += Timer{activeStore.recvCasTime, 1};
					stats.recvCasBytesComp += activeStore.fileSize;
					stats.recvCasBytesRaw += activeStore.actualSize;

					SCOPED_FUTEX(m_waitEntriesLock, waitLock);
					auto waitFindIt = m_waitEntries.find(casEntry.key);
					if (waitFindIt != m_waitEntries.end())
					{
						WaitEntry& waitEntry = waitFindIt->second;
						waitEntry.Success = true;
						waitEntry.done.Set();
					}
					waitLock.Leave();

					if (!firstStore)
					{
						SCOPED_WRITE_LOCK(m_activeStoresLock, activeLock);
						m_activeStores.erase(storeId);
						activeLock.Leave();
						PushId(storeId);
					}
					else
					{
						writer.WriteU16(0);
						writer.WriteBool(m_traceStore);
					}

					return true;
				}

				if (firstStore)
				{
					storeId = PopId();
					UBA_ASSERT(storeId != 0);
					writer.WriteU16(storeId);
					writer.WriteBool(m_traceStore);

					SCOPED_WRITE_LOCK(m_activeStoresLock, activeLock);
					auto insres = m_activeStores.try_emplace(storeId);
					UBA_ASSERT(insres.second);
					ActiveStore& s = insres.first->second;
					s.clientId = connectionInfo.GetId();
					activeLock.Leave();

					s.fileSize = firstStore->fileSize;
					s.mappedView = firstStore->mappedView;
					s.fileAccessor = firstStore->fileAccessor;
					s.casEntry = firstStore->casEntry;
					s.totalWritten = firstStore->totalWritten.load();
					s.recvCasTime = firstStore->recvCasTime.load();
				}
				return true;
			}
			case StorageMessageType_StoreEnd:
			{
				CasKey key = reader.ReadCasKey();
				if (m_traceStore)
					m_trace->FileStoreEnd(connectionInfo.GetId(), key);
				return true;
			}
			case StorageMessageType_ProxyFetchBegin:
			{
				CasKey casKey = reader.ReadCasKey();
				StringBuffer<> hint;
				reader.ReadString(hint);
				if (m_trace)
					m_trace->FileFetchBegin(connectionInfo.GetId(), casKey, hint);
				return true;
			}
			case StorageMessageType_ProxyFetchEnd:
			{
				CasKey casKey = reader.ReadCasKey();
				if (m_trace)
					m_trace->FileFetchEnd(connectionInfo.GetId(), casKey);
				return true;
			}
			case StorageMessageType_ReportBadProxy:
			{
				u32 proxyClientId = reader.ReadU32();

				SCOPED_WRITE_LOCK(m_proxiesLock, lock);
				for (auto it=m_proxies.begin(); it!=m_proxies.end(); ++it)
				{
					ProxyEntry& e = it->second;
					if (e.clientId != proxyClientId)
						continue;
					m_logger.Detail(TC("Proxy %u (%s:%u) for zone %s removed (bad proxy reported by client %u)"), proxyClientId, e.host.c_str(), e.port, e.zone.c_str(), connectionInfo.GetId());
					m_proxies.erase(it);
					break;
				}
				lock.Leave();

				GetProxy(writer, connectionInfo.GetId(), false);
				return true;
			}
		}
		UBA_ASSERT(false);
		return false;
	}

	u16 StorageServer::PopId()
	{
		SCOPED_FUTEX(m_availableIdsLock, lock);
		if (m_availableIds.empty())
		{
			if (m_availableIdsHigh == 65534)
			{
				m_logger.Error(TC("OUT OF AVAILABLE IDs.. SHOULD NEVER HAPPEN!"));
				UBA_ASSERT(false);
			}
			return m_availableIdsHigh++;
		}
		u16 storeId = m_availableIds.back();
		m_availableIds.pop_back();
		return storeId;
	}

	void StorageServer::PushId(u16 id)
	{
		SCOPED_FUTEX(m_availableIdsLock, lock);
		m_availableIds.push_back(id);
	}

}

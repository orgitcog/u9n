// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaDetoursFileMappingTable.h"
#include "UbaDirectoryTable.h"
#include "UbaPathUtils.h"

#if PLATFORM_WINDOWS
#include "Windows/UbaDetoursUtilsWin.h"
#endif

namespace uba
{
	MappedFileTable::MappedFileTable(MemoryBlock& memoryBlock) : m_memoryBlock(memoryBlock), m_lookup(memoryBlock)
	{
	}

	void MappedFileTable::Init(const u8* mem, u32 tableCount, u32 tableSize)
	{
		m_mem = mem;
		m_lookup.reserve(tableCount + 100);
		m_memoryBlock.CommitNoLock(tableCount*(sizeof(GrowingUnorderedMap<StringKey, FileInfo>::value_type)+16), TC(""));
		ParseNoLock(tableSize);
	}

	void MappedFileTable::ParseNoLock(u32 tableSize)
	{
		u32 startPosition = m_memPosition;
		if (tableSize <= startPosition)
			return;

		BinaryReader reader(m_mem, startPosition);
		while (reader.GetPosition() != tableSize)
		{
			UBA_ASSERT(reader.GetPosition() < tableSize);
			StringKey g = reader.ReadStringKey();
			StringBuffer<1024> mappedFileName;
			reader.ReadString(mappedFileName);
			u64 size = reader.Read7BitEncoded();
			auto insres = m_lookup.try_emplace(g);
			FileInfo& info = insres.first->second;
			if (!insres.second)
			{
				if (info.name && info.name[0] == '^' && !mappedFileName.Equals(info.name, false)) // Mapped file has been re-mapped.
				{
					UBA_ASSERTF(!info.memoryFile, TC("Mapped file %s has changed mapping (%s to %s) while being in use"), info.originalName, info.name, mappedFileName.data);
					info.name = m_memoryBlock.Strdup(mappedFileName).data;
				}
				continue;
			}
			info.fileNameKey = g;
			info.name = m_memoryBlock.Strdup(mappedFileName).data;
			info.size = size;
		}
		m_memPosition = tableSize;
	}

	void MappedFileTable::Parse(u32 tableSize)
	{
		SCOPED_WRITE_LOCK(m_lookupLock, lock);
		ParseNoLock(tableSize);
	}

	void MappedFileTable::SetDeleted(const StringKey& key, const tchar* name, bool deleted)
	{
		SCOPED_WRITE_LOCK(m_lookupLock, lock);
		auto it = m_lookup.find(key);
		if (it == m_lookup.end())
			return;
		FileInfo& sourceInfo = it->second;
		sourceInfo.deleted = deleted;
		sourceInfo.lastDesiredAccess = 0;
	}

	void Rpc_CreateFileW(const StringView& fileName, const StringKey& fileNameKey, u8 access, tchar* outNewName, u64 newNameCapacity, u64& outSize, u32& outCloseId, bool lock)
	{
		RPC_MESSAGE(CreateFile, createFile)
		writer.WriteString(fileName);
		writer.WriteStringKey(fileNameKey);
		writer.WriteByte(access);
		writer.Flush();
		BinaryReader reader;
		reader.ReadString(outNewName, newNameCapacity);
		outSize = reader.ReadU64();
		outCloseId = reader.ReadU32();
		u32 mappedFileTableSize = reader.ReadU32();
		u32 directoryTableSize = u32(reader.ReadU32());
		pcs.Leave();
		DEBUG_LOG_PIPE(L"CreateFile", L"%ls (%ls)", (access == 0 ? L"ATTRIB" : ((access & AccessFlag_Write) ? L"WRITE" : L"READ")), fileName.data);

		if (lock)
			g_mappedFileTable.Parse(mappedFileTableSize);
		else
			g_mappedFileTable.ParseNoLock(mappedFileTableSize);
		g_directoryTable.ParseDirectoryTable(directoryTableSize);
	}

	void Rpc_CheckRemapping(const StringView& fileName, const StringKey& fileNameKey)
	{
		RPC_MESSAGE(CheckRemapping, createFile)
		writer.WriteString(fileName);
		writer.WriteStringKey(fileNameKey);
		writer.Flush();
		BinaryReader reader;
		u32 mappedFileTableSize = reader.ReadU32();
		pcs.Leave();
		g_mappedFileTable.ParseNoLock(mappedFileTableSize);
	}

	u32 Rpc_UpdateDirectory(const StringKey& dirKey, const tchar* dirName, u64 dirNameLen, bool lockDirTable)
	{
		u32 directoryTableSize;
		u32 tableOffset;
		{
			RPC_MESSAGE(ListDirectory, listDirectory)
			writer.WriteString(dirName, dirNameLen);
			writer.WriteStringKey(dirKey);
			writer.Flush();
			BinaryReader reader;
			directoryTableSize = u32(reader.ReadU32());
			u64 to = reader.ReadU32();
			tableOffset = to == InvalidTableOffset ? ~u32(0) : (u32)to;
			pcs.Leave();
			DEBUG_LOG_PIPE(L"ListDirectory", L"(%ls)", dirName);
		}
		if (lockDirTable)
			g_directoryTable.ParseDirectoryTable(directoryTableSize);
		else
			g_directoryTable.ParseDirectoryTableNoLock(directoryTableSize);
		return tableOffset;
	}

	void Rpc_UpdateCloseHandle(const tchar* handleName, u32 closeId, bool deleteOnClose, const tchar* newName, const FileMappingHandle& mappingHandle, u64 mappingWritten, bool success)
	{
		u32 directoryTableSize;
		{
			RPC_MESSAGE(CloseFile, closeFile)
			writer.WriteString(handleName);
			writer.WriteU32(closeId);
			//writer.WriteU32(attributes); // TODO THIS
			writer.WriteBool(deleteOnClose);
			writer.WriteBool(success);
			writer.WriteU64(mappingHandle.ToU64());
			writer.WriteU64(mappingWritten);
			if (*newName)
			{
				StringBuffer<> fixedName;
				FixPath(fixedName, newName);
				StringBuffer<> forKey(fixedName);
				if (CaseInsensitiveFs)
					forKey.MakeLower();
				StringKey newNameKey = ToStringKey(forKey);
				writer.WriteStringKey(newNameKey);
				writer.WriteString(fixedName);
			}
			else
				writer.WriteStringKey(StringKeyZero);
			writer.Flush();
			BinaryReader reader;
			directoryTableSize = reader.ReadU32();
			pcs.Leave();
			DEBUG_LOG_PIPE(L"CloseFile", L"");
		}
		g_directoryTable.ParseDirectoryTable(directoryTableSize);
	}

	// Returns true if done (no overflow)
	bool UpdateWrittenFiles(BinaryReader& reader)
	{
		u32 count = reader.ReadU32();
		u8 overflow = reader.ReadByte();
		while (count--)
		{
			StringKey key = reader.ReadStringKey();
			auto insres = g_mappedFileTable.m_lookup.try_emplace(key);
			FileInfo& info = insres.first->second;

			bool isInTemp = reader.ReadBool();
			StringBuffer<> originalName;
			if (isInTemp)
				originalName.Append(g_systemTemp).Append(PathSeparator);
			reader.ReadString(originalName);
			if (!info.originalName || !originalName.Equals(info.originalName))
				info.originalName = g_mappedFileTable.m_memoryBlock.Strdup(originalName).data;

			StringBufferBase& backedName = originalName.Clear();
			reader.ReadString(backedName);

			FileMappingHandle mappingHandle = FileMappingHandle::FromU64(reader.Read7BitEncoded());
			u64 fileSize = reader.Read7BitEncoded();
			info.fileNameKey = key;
			info.size = fileSize;
			info.created = true;

			if (mappingHandle.IsValid())
				backedName.Clear().Append(':').AppendBase62(mappingHandle.ToU64());

			if (!info.name || !backedName.Equals(info.name))
				info.name = g_mappedFileTable.m_memoryBlock.Strdup(backedName).data;

			DEBUG_LOG(TC("GOT WRITTEN FILE: %s (BackedFile: %s Size: %llu)"), info.originalName, info.name, info.size);

			if (auto mf = info.memoryFile) // Memory file is now wrong.. we need to delete it (or should we create a new one with the new mapping?)
			{
				#if PLATFORM_WINDOWS
				if (info.refCount)
					DEBUG_LOG(TC("File has memory file and refcount %u. Will drop memory file and use received file. %s (BackedFile: %s Size: %llu)"), info.refCount, info.originalName, info.name, info.size);
				if (!mf->isLocalOnly)
				{
					DEBUG_LOG(TC("Unmapping old memoryfile for: %s"), info.originalName);
					UnmapViewOfFile(mf->baseAddress);
					CloseHandle(mf->mappingHandle.mh);
					CloseHandle(mf->mappingHandle.fh);
				}
				#endif
				// delete mf; // Let them leak
				info.memoryFile = nullptr;
			}
		}
		return overflow == 0;
	}

	void Rpc_GetWrittenFilesNoLock(bool isInit)
	{
		while (true)
		{
			RPC_MESSAGE_NO_LOCK(GetWrittenFiles, updateTables)
			writer.WriteBool(isInit);
			writer.Flush();
			BinaryReader reader;
			if (UpdateWrittenFiles(reader))
				break;
		}
	}

	void Rpc_UpdateTables()
	{
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		u32 directoryTableSize;
		u32 fileMappingTableSize;
		bool done;
		{
			RPC_MESSAGE_NO_LOCK(UpdateTables, updateTables)
			writer.WriteBool(false);
			writer.Flush();
			BinaryReader reader;
			directoryTableSize = reader.ReadU32();
			fileMappingTableSize = reader.ReadU32();
			done = UpdateWrittenFiles(reader);

			DEBUG_LOG_PIPE(L"UpdateTables", L"");
		}
		if (!done)
			Rpc_GetWrittenFilesNoLock(false);

		pcs.Leave();

		g_directoryTable.ParseDirectoryTable(directoryTableSize);
		g_mappedFileTable.Parse(fileMappingTableSize);
	}

	void Rpc_GetWrittenFiles()
	{
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		Rpc_GetWrittenFilesNoLock(true);
	}

	u32 Rpc_GetEntryOffset(const StringKey& entryNameKey, StringView entryName, bool checkIfDir)
	{
		u32 dirTableOffset = ~u32(0);
		StringBuffer<MaxPath> entryNameForKey;
		entryNameForKey.Append(entryName);
		if (CaseInsensitiveFs)
			entryNameForKey.MakeLower();
		else if (entryNameForKey.count == 1 && entryNameForKey[0] == '/')
			checkIfDir = true;

		CHECK_PATH(entryNameForKey);
		DirectoryTable::Exists exists = g_directoryTable.EntryExists(entryNameKey, entryNameForKey, checkIfDir, &dirTableOffset);
		if (exists != DirectoryTable::Exists_Maybe)
			return dirTableOffset;

		const tchar* lastPathSeparator = TStrrchr(entryName.data, PathSeparator);
		if (!lastPathSeparator)
		{
			UBA_ASSERTF(lastPathSeparator, TC("No path separator found in %s"), entryName.count > 0 ? entryName.data : TC("(NULL)"));
			return ~u32(0);
		}

		#if PLATFORM_WINDOWS
		UBA_ASSERT(wcsncmp(entryName.data, g_systemTemp.data, g_systemTemp.count) != 0);
		#endif

		u32 dirNameLen = u32(lastPathSeparator - entryName.data);
		DirHash hash(StringView(entryNameForKey.data, dirNameLen));

		if (Rpc_UpdateDirectory(hash.key, entryName.data, dirNameLen) == ~u32(0))
			return ~u32(0);

		SCOPED_WRITE_LOCK(g_directoryTable.m_lookupLock, lookLock);
		auto dirFindIt = g_directoryTable.m_lookup.find(hash.key);
		if (dirFindIt == g_directoryTable.m_lookup.end())
			return ~u32(0);
		auto& dir = dirFindIt->second;

		if (checkIfDir)
			return u32(dir.tableOffset) | 0x80000000; // Use significant bit to say that this is a dir

		g_directoryTable.PopulateDirectory(hash.open, dir);

		SCOPED_READ_LOCK(dir.lock, lock);
		auto findIt = dir.files.find(entryNameKey);
		if (findIt == dir.files.end())
			return ~u32(0);
		return findIt->second;
	}

	void Rpc_GetFullFileName(const tchar*& path, u64& pathLen, StringBufferBase& tempBuf, bool useVirtualName, const tchar* const* loaderPaths)
	{
		StringKey fileNameKey;
		StringBuffer<> temp2;
		if (IsAbsolutePath(path))
		{
			FixPath(tempBuf, path);
			temp2.Append(tempBuf);
			path = temp2.data;

			if (CaseInsensitiveFs)
				tempBuf.MakeLower();
			fileNameKey = ToStringKey(tempBuf);
			tempBuf.Clear();
		}

		u32 mappedFileTableSize;

		#if UBA_DEBUG
		StringBuffer<> virtualName;
		#endif

		{
			RPC_MESSAGE(GetFullFileName, getFullFileName)
			writer.WriteString(path);
			writer.WriteStringKey(fileNameKey);
			u16& bytes = *(u16*)writer.AllocWrite(2);
			auto pos = writer.GetPosition();
			if (loaderPaths)
				for (auto i=loaderPaths; *i; ++i)
					writer.WriteString(*i);
			bytes = u16(writer.GetPosition() - pos);
			writer.Flush();
			BinaryReader reader;
			reader.ReadString(tempBuf);
			if (useVirtualName)
			{
				reader.ReadString(tempBuf.Clear());
			}
			else
			{
				#if UBA_DEBUG
				reader.ReadString(virtualName);
				#else
				reader.SkipString();
				#endif
			}
			mappedFileTableSize = reader.ReadU32();
			DEBUG_LOG_PIPE(TC("GetFileName"), TC("(%ls)"), tempBuf);
		}

		#if UBA_DEBUG
		if (useVirtualName)
		{ DEBUG_LOG_DETOURED(TC("Rpc_GetFullFileName"), TC("%s -> %s"), path, tempBuf.data); }
		else
		{ DEBUG_LOG_DETOURED(TC("Rpc_GetFullFileName"), TC("%s -> %s (%s)"), path, tempBuf.data, virtualName.data); }
		#endif

		g_mappedFileTable.Parse(mappedFileTableSize);
		path = tempBuf.data;
		pathLen = tempBuf.count;
	}

	void Rpc_GetFullFileName2(const tchar* path, StringBufferBase& outReal, StringBufferBase& outVirtual, const tchar* const* loaderPaths)
	{
		StringKey fileNameKey;
		StringBuffer<> temp2;
		if (IsAbsolutePath(path))
		{
			FixPath(temp2, path);
			path = temp2.data;
			fileNameKey = CaseInsensitiveFs ? ToStringKeyLower(temp2) : ToStringKey(temp2);
		}

		u32 mappedFileTableSize;

		{
			RPC_MESSAGE(GetFullFileName, getFullFileName)
			writer.WriteString(path);
			writer.WriteStringKey(fileNameKey);
			u16& bytes = *(u16*)writer.AllocWrite(2);
			auto pos = writer.GetPosition();
			if (loaderPaths)
				for (auto i=loaderPaths; *i; ++i)
					writer.WriteString(*i);
			bytes = u16(writer.GetPosition() - pos);
			writer.Flush();
			BinaryReader reader;
			reader.ReadString(outReal);
			reader.ReadString(outVirtual);
			mappedFileTableSize = reader.ReadU32();
			DEBUG_LOG_PIPE(TC("GetFileName"), TC("(%ls)"), tempBuf);
		}

		#if UBA_DEBUG
		DEBUG_LOG_DETOURED(TC("Rpc_GetFullFileName"), TC("%s -> %s (%s)"), path, outReal.data, outVirtual.data);
		#endif
		g_mappedFileTable.Parse(mappedFileTableSize);
	}

	DirHash::DirHash(const StringView& str)
	{
		CHECK_PATH(str);
		open.Update(str);
		key = ToStringKey(open);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaPathUtils.h"
#include "UbaBinaryReaderWriter.h"

namespace uba
{
	enum : u64 { InvalidTableOffset = 0 };

	class DirectoryTable
	{
	public:
		using EntryLookup = GrowingUnorderedMap<StringKey, u32>;
		struct Directory { Directory(MemoryBlock& block) : files(block) {} u32 tableOffset = InvalidTableOffset; u32 parseOffset = InvalidTableOffset; EntryLookup files; ReaderWriterLock lock; };

		void Init(const u8* mem, u32 tableCount, u32 tableSize)
		{
			m_memory = mem;
			m_lookup.reserve(tableCount + 100);
			m_memoryBlock.CommitNoLock(tableCount*(sizeof(GrowingUnorderedMap<StringKey, Directory>::value_type)+16), TC(""));
			ParseDirectoryTable(tableSize);
		}

		void ParseDirectoryTable(u32 size)
		{
			SCOPED_WRITE_LOCK(m_lookupLock, lock);
			ParseDirectoryTableNoLock(size);
		}

		void ParseDirectoryTableNoLock(u32 size)
		{
			if (size <= m_memorySize)
				return;
			ParseDirectoryTableNoLock(m_memorySize, size);
			m_memorySize = size;
		}

		void ParseDirectoryTableNoLock(u32 from, u32 to)
		{
			BinaryReader reader(m_memory, from, to);
			while (true)
			{
				u64 pos = reader.GetPosition();
				if (pos == to)
					break;
				UBA_ASSERTF(pos < to, TC("Should never read past size (pos: %llu, size: %u)"), pos, to);
				u64 storageSize = reader.Read7BitEncoded();
				StringKey dirKey = reader.ReadStringKey();
				auto insres = m_lookup.try_emplace(dirKey, m_memoryBlock); // Note that this is allowed to overwrite
				insres.first->second.tableOffset = u32(reader.GetPosition());
				reader.Skip(storageSize - sizeof(dirKey));
			}
		}

		void PopulateDirectory(const StringKeyHasher& hasher, Directory& dir)
		{
			SCOPED_WRITE_LOCK(dir.lock, lock);
			PopulateDirectoryNoLock(hasher, dir);
		}

		void PopulateDirectoryNoLock(const StringKeyHasher& hasher, Directory& dir)
		{
			if (dir.parseOffset == dir.tableOffset)
				return;
			PopulateDirectoryRecursive(hasher, dir.tableOffset, dir.parseOffset, dir.files);
			dir.parseOffset = dir.tableOffset;
		}

		void PopulateDirectoryRecursive(const StringKeyHasher& hasher, u32 tableOffset, u32 parseOffset, EntryLookup& files)
		{
			BinaryReader reader(m_memory, tableOffset);
			u32 prevTableOffset = u32(reader.Read7BitEncoded());

			u32 buffer[48*1024];
			u32 count = 0;
			u32* readerOffsets = buffer;
			readerOffsets[count++] = u32(reader.GetPosition());
			bool firstIsRoot = true;
			while (true)
			{
				if (prevTableOffset == InvalidTableOffset || prevTableOffset == parseOffset)
				{
					firstIsRoot = prevTableOffset == InvalidTableOffset;
					break;
				}
				reader.SetPosition(prevTableOffset);
				prevTableOffset = u32(reader.Read7BitEncoded());
				readerOffsets[count++] = u32(reader.GetPosition());
				if (count == sizeof_array(buffer))
				{
					readerOffsets = new u32[1024*1024]; // This sucks, but somethings the directory is huuuge. Ideally these files should be spread out over multiple directories
					memcpy(readerOffsets, buffer, sizeof(buffer));
				}
			}

			for (u32 i=count; i>0; --i)
			{
				reader.SetPosition(readerOffsets[i-1]);
				if (firstIsRoot)
				{
					firstIsRoot = false;
					u32 attr = reader.ReadFileAttributes();
					if (!attr)
						continue;
					reader.ReadVolumeSerial(); // Directory volume serial
					reader.ReadFileIndex(); // Directory file index
				}

				PopulateDirectoryWithFiles(reader, hasher, files);
			}

			if (readerOffsets != buffer)
				delete[] readerOffsets;
		}

		void PopulateDirectoryWithFiles(BinaryReader& reader, const StringKeyHasher& hasher, EntryLookup& files)
		{
			u64 itemCount = reader.Read7BitEncoded();

			files.reserve(files.size() + itemCount);
	
			StringBuffer<> filename;
			filename.Append(PathSeparator);

			while (itemCount--)
			{
				u32 offset = u32(reader.GetPosition());
				filename.Resize(1);
				reader.ReadString(filename);
				if (CaseInsensitiveFs)
					filename.MakeLower();
				u32 attr = reader.ReadFileAttributes();
				reader.ReadVolumeSerial();
				reader.ReadFileIndex();
				if (!IsDirectory(attr))
				{
					reader.ReadFileTime();
					reader.ReadFileSize();
				}

				StringKey filenameKey = ToStringKey(hasher, filename.data, filename.count);
				files[filenameKey] = offset; // Always write, since same file might have been added with new info
			}
		}

		enum Exists
		{
			Exists_Yes,
			Exists_No,
			Exists_Maybe,
		};

		Exists EntryExists(StringKey entryKey, const StringView& entryName, bool checkIfDir = false, u32* tableOffset = nullptr)
		{
			SCOPED_READ_LOCK(m_lookupLock, lock);
			return EntryExistsNoLock(entryKey, entryName, checkIfDir, tableOffset);
		}

		Exists EntryExistsNoLock(StringKey entryKey, const StringView& entryName, bool checkIfDir = false, u32* tableOffset = nullptr)
		{
			u32 startSkip = 2;
			if (checkIfDir)
			{
				auto findIt = m_lookup.find(entryKey);
				if (findIt != m_lookup.end())
				{
					if (tableOffset)
						*tableOffset = u32(findIt->second.tableOffset) | 0x80000000; // Use significant bit to say that this is a dir
					return Exists_Yes;
				}
				startSkip = 1;
			}

			// Scan backwards
			const tchar* rend = entryName.data;
			const tchar* rit = rend + entryName.count - startSkip;

			bool inAncestor = false;
			while (rit > rend)
			{
				if (*rit != PathSeparator)
				{
					--rit;

					#if !PLATFORM_WINDOWS
					if (rit != rend) // We want to test empty for non-windows
					#endif
						continue;
				}

				u64 sublen = u64(rit - rend);

				StringKeyHasher ancestorHasher;
				ancestorHasher.Update(rend, sublen);
				StringKey ancestorKey = ToStringKey(ancestorHasher);
				auto dirIt = m_lookup.find(ancestorKey);
				if (dirIt != m_lookup.end())
				{
					DirectoryTable::Directory& parentDir = dirIt->second;
					if (parentDir.tableOffset == -1)
						return Exists_No;
					if (parentDir.parseOffset != parentDir.tableOffset)
					{
						SCOPED_WRITE_LOCK(parentDir.lock, lock);
						PopulateDirectoryRecursive(ancestorHasher, parentDir.tableOffset, parentDir.parseOffset, parentDir.files);
						parentDir.parseOffset = parentDir.tableOffset;
					}

					SCOPED_READ_LOCK(parentDir.lock, lock);
					auto entryIt = parentDir.files.find(entryKey);
					if (entryIt == parentDir.files.end())
						return Exists_No;
					if (inAncestor)
					{
						BinaryReader reader(m_memory, entryIt->second, m_memorySize);
						reader.SkipString();
						if (!IsDirectory(reader.ReadFileAttributes()))
							return Exists_No;
						return Exists_Maybe;
					}
					if (tableOffset)
						*tableOffset = entryIt->second;
					return Exists_Yes;
				}

				entryKey = ancestorKey;
				--rit;
				inAncestor = true;
			}
			return Exists_Maybe;
		}

		Exists EntryExists(const StringView& str, bool checkIfDir = false)
		{
			StringBuffer<> str2(str);
			if (str2[str2.count-1] == PathSeparator)
				str2.Resize(str2.count-1);
			if (CaseInsensitiveFs)
				str2.MakeLower();
			return EntryExists(ToStringKey(str2), str2, checkIfDir, nullptr);
		}

		struct EntryInformation
		{
			u32 attributes;
			u32 volumeSerial;
			u64 fileIndex;
			u64 size;
			u64 lastWrite;
		};

		u32 GetAttributes(u32 tableOffset)
		{
			if (tableOffset & 0x80000000)
			{
				tableOffset = tableOffset & ~0x80000000;
				BinaryReader reader(m_memory, tableOffset);
				u64 prevTableOffset = reader.Read7BitEncoded();
				while (prevTableOffset != InvalidTableOffset)
				{
					reader.SetPosition(prevTableOffset);
					prevTableOffset = reader.Read7BitEncoded();
				}
				return reader.ReadFileAttributes();
			}
			BinaryReader reader(m_memory, tableOffset);
			reader.SkipString();
			return reader.ReadFileAttributes();
		}

		u32 GetEntryInformation(EntryInformation& outInfo, u32 tableOffset, tchar* outFileName = nullptr, u32 fileNameCapacity = 0)
		{
			if (tableOffset & 0x80000000)
			{
				tableOffset = tableOffset & ~0x80000000;
				BinaryReader reader(m_memory, tableOffset);
				u64 prevTableOffset = reader.Read7BitEncoded();
				while (prevTableOffset != InvalidTableOffset)
				{
					reader.SetPosition(prevTableOffset);
					prevTableOffset = reader.Read7BitEncoded();
				}
				outInfo.attributes = reader.ReadFileAttributes();
				if (outInfo.attributes)
				{
					outInfo.volumeSerial = reader.ReadVolumeSerial();
					outInfo.fileIndex = reader.ReadFileIndex();
				}
				outInfo.size = 0;
				outInfo.lastWrite = 0;
				UBA_ASSERT(!outFileName);
				return ~u32(0);
			}

			BinaryReader reader(m_memory, tableOffset);
			if (outFileName)
				reader.ReadString(outFileName, fileNameCapacity);
			else
				reader.SkipString();
			outInfo.attributes = reader.ReadFileAttributes();
			outInfo.volumeSerial = reader.ReadVolumeSerial();
			outInfo.fileIndex = reader.ReadFileIndex();
			if (IsDirectory(outInfo.attributes))
			{
				outInfo.size = 0;
				outInfo.lastWrite = 0;
			}
			else
			{
				outInfo.lastWrite = reader.ReadFileTime();
				outInfo.size = reader.ReadFileSize();
			}
			return u32(reader.GetPosition());
		}

		void GetFinalPath(StringBufferBase& out, const tchar* path)
		{
			UBA_ASSERT(IsAbsolutePath(path));

			Directory* directory = nullptr;
			const tchar* prevSlash = TStrchr(path+3, PathSeparator);
			if (!prevSlash)
			{
				// return root directory as-is
				out.Append(path);
				return;
			}
			out.Append(path, u64(prevSlash - path));
			const tchar* end = path + TStrlen(path);

			StringBuffer<> forHash;
			forHash.Append(path, u64(prevSlash - path));
			if (CaseInsensitiveFs)
				forHash.MakeLower();

			StringKeyHasher hasher;
			hasher.Update(forHash.data, forHash.count);

			SCOPED_READ_LOCK(m_lookupLock, lock);
			while (true)
			{
				const tchar* slash = TStrchr(prevSlash + 1, PathSeparator);
				if (!slash)
					slash = end;

				forHash.Clear().Append(prevSlash, u64(slash - prevSlash));
				if (CaseInsensitiveFs)
					forHash.MakeLower();
				hasher.Update(forHash.data, forHash.count);
				StringKey fileNameKey = ToStringKey(hasher);

				if (directory)
				{
					SCOPED_READ_LOCK(directory->lock, lock2);
					auto fileIt = directory->files.find(fileNameKey);
					if (fileIt != directory->files.end())
					{
						UBA_ASSERT(fileIt->second != ~0u);
						BinaryReader reader(m_memory, fileIt->second);
						StringBuffer<> fileName;
						reader.ReadString(fileName);
						out.Append(PathSeparator).Append(fileName);
					}
					else
						out.Append(prevSlash, u64(slash - prevSlash));
				}
				else
					out.Append(prevSlash, u64(slash - prevSlash));

				if (slash == end)
					return;

				prevSlash = slash;

				auto findIt = m_lookup.find(fileNameKey);
				if (findIt == m_lookup.end())
				{
					directory = nullptr;
					continue;
				}
				directory = &findIt->second;
				PopulateDirectory(hasher, *directory); // This is needed to make sure files lookup is populated for query above
			}
		}

#if PLATFORM_WINDOWS
		template<typename Func>
		void TraverseFilesRecursiveNoLock(const StringBufferBase& path, const Func& func)
		{
			auto findIt = m_lookup.find(ToStringKey(path));
			if (findIt == m_lookup.end())
				return;
			StringKeyHasher hasher;
			hasher.Update(path.data, path.count);
			PopulateDirectory(hasher, findIt->second);
			for (auto& fileKv : findIt->second.files)
			{
				DirectoryTable::EntryInformation info;
				StringBuffer<> fileName(path);
				fileName.Append(PathSeparator);
				u32 fileOffset = fileKv.second;
				GetEntryInformation(info, fileOffset, fileName.data + fileName.count, fileName.capacity - fileName.count);
				fileName.count = TStrlen(fileName.data);
				if (CaseInsensitiveFs)
					fileName.MakeLower();
				func(info, fileName, fileOffset);
				TraverseFilesRecursiveNoLock(fileName, func);
			}
		}

		template<typename Func>
		void TraverseAllFilesNoLock(const Func& func)
		{
			for (tchar l='a';l!='z'; ++l)
			{
				StringBuffer<4> drive;
				drive.Append(l).Append(':');
				TraverseFilesRecursiveNoLock(drive, func);
			}
		}
#endif

		DirectoryTable(MemoryBlock& block) : m_memoryBlock(block), m_lookup(block) {}
		DirectoryTable(const DirectoryTable&) = delete;
		void operator=(const DirectoryTable&) = delete;

		MemoryBlock& m_memoryBlock;
		ReaderWriterLock m_lookupLock;
		GrowingUnorderedMap<StringKey, Directory> m_lookup;

		ReaderWriterLock m_memoryLock;
		const u8* m_memory = nullptr;
		u32 m_memorySize = 0;
	};

}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_LINUX

#include "UbaStringBuffer.h"
#include <elf.h>

#if !defined(UBA_IS_DETOURED_INCLUDE)
#define TRUE_WRAPPER(func) func
#endif

#define UBA_FINDIMPORTS_DEBUG 0

namespace uba
{
	struct BinaryInfo
	{
	};

	constexpr StringView g_systemFiles[] =
	{
		TCV("libstdc++.so"),
		TCV("libpthread.so"),
		TCV("ld-linux-x86-64.so"),
		TCV("librt.so"),
		TCV("libdl.so"),
		TCV("libc.so"),
		TCV("libdbus-1.so"),
		TCV("libgcc_s.so"),
		TCV("libm.so"),
		TCV("libdxil.so"),
		TCV("libX11.so"),
		TCV("libXext.so"),
		TCV("libXcursor.so"),
		TCV("libXi.so"),
		TCV("libXfixes.so"),
		TCV("libXrandr.so"),
		TCV("libXss.so"),
		TCV("libudev.so"),
	};

	inline bool IsKnownSystemFile(const tchar* fileName)
	{
		StringView file = ToView(fileName);
		bool isSystem = false;
		for (auto systemFile : g_systemFiles)
			if (file.StartsWith(systemFile.data))
				return true;
		return false;
	}

	inline bool ParseBinary(const StringView& filePath, const StringView& originalPath, BinaryInfo& outInfo, const Function<void(const tchar* import, bool isKnown, const char* const* loaderPaths)>& func, StringBufferBase& outError)
	{
		#if UBA_FINDIMPORTS_DEBUG
		printf("FINDIMPORTS: %s\n", filePath.data);
		#endif

		int fd = TRUE_WRAPPER(open)(filePath.data, O_RDONLY);
		if (fd == -1)
			return outError.Appendf("Open failed for file %s", filePath.data).ToFalse();

		auto closeFileHandle = MakeGuard([&]() { TRUE_WRAPPER(close)(fd); });
		struct stat sb;
		if (TRUE_WRAPPER(fstat)(fd, &sb) == -1)
			return outError.Appendf("Stat failed for file %s", filePath.data).ToFalse();

		u32 size = sb.st_size;

		auto mem = (u8*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (mem == MAP_FAILED)
			return outError.Appendf("Mmap failed for file %s", filePath.data).ToFalse();
		auto unmap = MakeGuard([&]() { munmap(mem, size); });

		auto ehdr = (Elf64_Ehdr*)mem;
		if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0 || ehdr->e_ident[EI_CLASS] != ELFCLASS64)
			return outError.Append("Not a valid 64-bit ELF file").ToFalse();

		Elf64_Phdr* phdrs = (Elf64_Phdr*)(mem + ehdr->e_phoff);
		Elf64_Shdr* shdrs = (Elf64_Shdr *)(mem + ehdr->e_shoff);
		Elf64_Dyn* dynamic = NULL;
		size_t dyn_count = 0;

		auto vaddr_to_offset = [](Elf64_Addr vaddr, Elf64_Phdr *phdrs, int phnum) -> Elf64_Off {
			for (int i = 0; i < phnum; i++) {
				if (phdrs[i].p_type != PT_LOAD) continue;
				Elf64_Addr start = phdrs[i].p_vaddr;
				Elf64_Addr end = start + phdrs[i].p_memsz;
				if (vaddr >= start && vaddr < end) {
					return phdrs[i].p_offset + (vaddr - start);
				}
			}
			return 0;
		};

		for (int i = 0; i < ehdr->e_phnum; ++i)
		{
			if (phdrs[i].p_type != PT_DYNAMIC)
				continue;
			Elf64_Off dyn_offset = vaddr_to_offset(phdrs[i].p_vaddr, phdrs, ehdr->e_phnum);
			if (dyn_offset >= sb.st_size || dyn_offset + sizeof(Elf64_Dyn) > sb.st_size)
				return outError.Appendf("Dynamic offset out of file bounds!").ToFalse();
			dynamic = (Elf64_Dyn*)(mem + dyn_offset);			
			dyn_count = phdrs[i].p_filesz / sizeof(Elf64_Dyn);
			break;
		}

		if (!dynamic)
			return outError.Appendf("No PT_DYNAMIC segment found.").ToFalse();
		if ((u8*)dynamic < mem || (u8*)dynamic >= mem + sb.st_size)
			return outError.Appendf("Dynamic segment is out of file bounds!").ToFalse();

		Elf64_Addr strtab_addr = 0;
		Elf64_Xword strsz = 0;
		Elf64_Xword needed[512];
		u32 needed_count = 0;

		for (size_t i = 0; i < dyn_count; ++i)
		{
			auto& dyn = dynamic[i];
			if (dyn.d_tag == DT_NULL)
				break;
			if (dyn.d_tag == DT_STRTAB)
				strtab_addr = dyn.d_un.d_ptr;
			else if (dyn.d_tag == DT_STRSZ)
				strsz = dyn.d_un.d_val;
			else if (dyn.d_tag == DT_NEEDED && needed_count < sizeof_array(needed))
				needed[needed_count++] = dyn.d_un.d_val;
		}

		char* strtab = NULL;

		// Step 3: If DT_STRTAB is missing, fallback to section header lookup
		if (!strtab_addr && ehdr->e_shoff != 0) {
			for (int i = 0; i < ehdr->e_shnum; i++) {
				if (shdrs[i].sh_type == SHT_STRTAB && shdrs[i].sh_flags == 0) { // Likely .dynstr
					strtab = (char *)mem + shdrs[i].sh_offset;
					strsz = shdrs[i].sh_size;
					break;
				}
			}
		}

		// If we have strtab_addr but not strtab, resolve via vaddr to file offset
		if (strtab_addr && !strtab)
		{
			Elf64_Off str_off = vaddr_to_offset(strtab_addr, phdrs, ehdr->e_phnum);
			strtab = (char *)mem + str_off;
		}

		if (!strtab)
			return outError.Appendf("Failed to find string table.").ToFalse();

		const char* loaderPaths[256];
		u32 loaderPathsCount = 0;

		List<TString> fixedLoaderPaths;

		#if UBA_FINDIMPORTS_DEBUG
		printf("ELF: %s\n", filePath.data);
		#endif

		#if UBA_FINDIMPORTS_DEBUG
		printf("RPATH / RUNPATH:\n");
		#endif

		
		//StringView origin(fileName, strrchr(fileName, '/') - fileName);

		for (Elf64_Dyn *dyn = dynamic; dyn->d_tag != DT_NULL; dyn++)
		{
			if (dyn->d_tag != DT_RPATH && dyn->d_tag != DT_RUNPATH)
				continue;

			#if UBA_FINDIMPORTS_DEBUG
			if (dyn->d_tag == DT_RPATH)
				printf("  DT_RPATH: %s\n", strtab + dyn->d_un.d_val);
			else if (dyn->d_tag == DT_RUNPATH)
				printf("  DT_RUNPATH: %s\n", strtab + dyn->d_un.d_val);
			#endif

			char* begin = strtab + dyn->d_un.d_val;
			char* end = begin + strlen(begin);
			char* pathBegin = begin;
			while (pathBegin < end)
			{
				char* pathEnd = strchr(pathBegin, ':');
				if (!pathEnd)
					pathEnd = end;
				StringView loaderPath(pathBegin, pathEnd - pathBegin);
				if (loaderPath.StartsWith("${ORIGIN}"))
					loaderPaths[loaderPathsCount++] = fixedLoaderPaths.emplace_back(originalPath.ToString() + loaderPath.Skip(9).ToString()).c_str();
				else
					loaderPaths[loaderPathsCount++] = fixedLoaderPaths.emplace_back(loaderPath.ToString()).c_str();

				#if UBA_FINDIMPORTS_DEBUG
				printf("  SEARCH_PATH: %s\n", loaderPaths[loaderPathsCount-1]);
				#endif

				pathBegin = pathEnd + 1;
			}
		}
		loaderPaths[loaderPathsCount] = nullptr;

		#if UBA_FINDIMPORTS_DEBUG
		printf("DT_NEEDED:\n");
		#endif
		
		#if 0
		for (Elf64_Dyn *dyn = dynamic; dyn->d_tag != DT_NULL; dyn++) {
			if (dyn->d_tag != DT_NEEDED)
				continue;
			printf("  %s\n", strtab + dyn->d_un.d_val);
			func(strtab + dyn->d_un.d_val, false, loaderPaths);
		}
		#endif

		for (int i = 0; i < needed_count; i++) {
			if (needed[i] < strsz || strsz == 0)
			{
				StringView file = ToView(strtab + needed[i]);
				bool isSystem = false;
				for (auto systemFile : g_systemFiles)
					isSystem |= file.StartsWith(systemFile.data);
				if (isSystem)
					continue;

				#if UBA_FINDIMPORTS_DEBUG
				printf("  %s\n", strtab + needed[i]);
				#endif

				func(file.data, false, loaderPaths);
			}
			else
			{
				#if UBA_FINDIMPORTS_DEBUG
				printf("  [offset %lu out of bounds]\n", needed[i]);
				#endif
			}
		}		
		return true;
	}
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_SKIP - Bad includes (ZipArchiveWriter.h)

#if WITH_ENGINE

#include "Compression/CompressedBuffer.h"
#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Serialization/MemoryReader.h"
#include "Templates/UniquePtr.h"

class FZipArchiveWriter;

class FShaderSymbolExport
{
public:
	FShaderSymbolExport() = delete;
	SHADERCOMPILERCOMMON_API FShaderSymbolExport(FName InShaderFormat);
	SHADERCOMPILERCOMMON_API ~FShaderSymbolExport();

	/** Should be called from IShaderFormat::NotifyShaderCompiled implementation.
	*   Template type is the platform specific symbol data structure.
	*/
	template<typename TPlatformShaderSymbolData>
	UE_DEPRECATED(5.6, "Use overload accepting an FCompressedBuffer for symbol data.")
	void NotifyShaderCompiled(const TConstArrayView<uint8>& PlatformSymbolData, const FString& DebugInfo = FString()) {}

	template<typename TPlatformShaderSymbolData>
	void NotifyShaderCompiled(const FCompressedBuffer& PlatformSymbolDataCompressed, const FString& DebugInfo = FString());

	/** Called at the end of a cook to free resources and finalize artifacts created during the cook. */
	SHADERCOMPILERCOMMON_API void NotifyShaderCompilersShutdown();

private:
	SHADERCOMPILERCOMMON_API void Initialize();
	SHADERCOMPILERCOMMON_API void WriteSymbolData(const FString& Filename, const FString& DebugInfo, TConstArrayView<uint8> Contents);

	const FName ShaderFormat;

	TUniquePtr<FZipArchiveWriter> ZipWriter;
	TSet<FString> ExportedShaders;
	struct FSymbolFileInfo
	{
		uint64 Hash;
		int32 Size;
	};
	TMap<FString, FSymbolFileInfo> ExportedSymbolInfo;
	FString ExportPath;
	FString InfoFilePath;
	FCriticalSection Cs;
	uint64 TotalSymbolDataBytes{ 0 };
	uint64 TotalSymbolData{ 0 };
	bool bInitialized{ false };
	bool bExportShaderSymbols{ false };

	TMap<FString, FString> ShaderInfos;

	uint32 DuplicateSymbols{ 0 };

	/**
	 * If true, the current process is the first process in a multiprocess group, or is not in a group,
	 * and should combine artifacts produced by the other processes. Will also be false if no combination
	 * is necessary for given settings.
	 */
	bool bMultiprocessOwner{ false };
};

template<typename TPlatformShaderSymbolData>
inline void FShaderSymbolExport::NotifyShaderCompiled(const FCompressedBuffer& PlatformSymbolDataCompressed, const FString& DebugInfo)
{
	FScopeLock Lock(&Cs);
	if (!bInitialized)
	{
		// If we get called, we know we're compiling. Do one time initialization
		// which will create the output directory / open the open file stream.
		Initialize();
		bInitialized = true;
	}

	if (bExportShaderSymbols)
	{
		// Deserialize the platform symbol data
		TPlatformShaderSymbolData FullSymbolData;
		FSharedBuffer PlatformSymbolData = PlatformSymbolDataCompressed.Decompress();
		FMemoryReaderView Ar(PlatformSymbolData.GetView());
		Ar << FullSymbolData;

		for (const auto& SymbolData : FullSymbolData.GetAllSymbolData())
		{
			const FString FileName = SymbolData.GetFilename();
			TConstArrayView<uint8> Contents = SymbolData.GetContents();

			WriteSymbolData(FileName, DebugInfo, Contents);
		}
	}
}

#endif // WITH_ENGINE

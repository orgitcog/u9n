// Copyright Epic Games, Inc. All Rights Reserved.

#include "SerializeTextureSynthesisModelDataCommandlet.h"
#include "MetaHumanTextureSynthesisModelData.h"
#include "MetaHumanCoreTechLibGlobals.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Logging/StructuredLog.h"


int32 USerializeTextureSynthesisModelDataCommandlet::Main(const FString& Params)
{
	FString ModelDataFolderPath;
	FParse::Value(*Params, TEXT("FolderPath="), ModelDataFolderPath);
	if (!FPaths::DirectoryExists(ModelDataFolderPath))
	{
		UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Input folder does not exist {ModelDataFolderPath}", ModelDataFolderPath);
		return -1;
	}

	FString ArchiveFileName;
	if (!FParse::Value(*Params, TEXT("ArchiveFileName="), ArchiveFileName))
	{
		ArchiveFileName = TEXT("compressed.ar");
	}

	bool bLoadHFAnimatedMaps;
	if (!FParse::Bool(*Params, TEXT("LoadHFMaps="), bLoadHFAnimatedMaps))
	{
		bLoadHFAnimatedMaps = false;
	}

	FMetaHumanTextureSynthesizerModelData ModelData;
	ModelData.LoadMapsFromFolder(ModelDataFolderPath, bLoadHFAnimatedMaps);

	if (!ModelData.IsValidForSynthesis())
	{
		UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Model data not valid for synthesis");
		return -1;
	}

	const FString ArchiveFilePath = ModelDataFolderPath / ArchiveFileName;
	if (FPaths::FileExists(ArchiveFilePath))
	{
		UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "File {ArchiveFilePath} for writing already exists", ArchiveFilePath);
		return -1;
	}

	TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileWriter(*ArchiveFilePath));
	if (FileAr)
	{
		ModelData.Serialize(*FileAr);
		UE_LOGFMT(LogMetaHumanCoreTechLib, Display, "Written model data archive to {ArchiveFilePath}", ArchiveFilePath);
	}
	else
	{
		UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Cannot open file {ArchiveFilePath} for writing", ArchiveFilePath);
	}

	return 0;
}

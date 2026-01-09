// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/NotNull.h"

#include "MetaHumanCharacterDCCExport.generated.h"

/**
 * Information about the exported MetaHuman.
 */
USTRUCT()
struct FMetaHumanExportDCCManifest
{
	GENERATED_BODY()
	UPROPERTY()
	FString MetaHumanName;
	UPROPERTY()
	FString ExportPluginVersion;
	UPROPERTY()
	FString ExportEngineVersion;
	UPROPERTY()
	FDateTime ExportedAt;
};

struct FMetaHumanCharacterEditorDCCExportParameters
{
	/**
	 * Output folder on disk to store the DCC data. It should not be empty. 
	 */
	FString OutputFolderPath;
	
	/**
	 * Whether or not to bake makeup in the face textures
	 */
	bool bBakeFaceMakeup = true;

	/**
	 * Whether or not to export files in ZIP archive
	 */
	bool bExportZipFile = false;

	/**
	 * File name on disk to store the archive with the DCC data. If empty, character name will be used.
	 */
	FString ArchiveName;
};

struct FMetaHumanCharacterEditorDCCExport
{
	/**
	 * Generate an archive containing the MetaHuman assets for consumption in DCC tools. 
	 */
	static void ExportCharacterForDCC(
		TNotNull<class UMetaHumanCharacter*> InMetaHumanCharacter,
		const FMetaHumanCharacterEditorDCCExportParameters& InExportParams);
};

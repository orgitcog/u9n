// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/SortedMap.h"
#include "ts/ts_types.h"


/**
 * Helper for loading and storing the data needed to use the titan texture synthesis model
 */
struct FMetaHumanTextureSynthesizerModelData
{
	/**
	 * Buffer with the data for a specific model map
	 * Reflects the storage passed to the texture model via the Model_Data type, providing UE lifetime management
	 */
	struct FModelDataContainer
	{
		FModelDataContainer() :
			NumRows(0),
			NumColumns(0),
			WordSize(0)
		{
		}

		FModelDataContainer(int32 InNumRows, int32 InNumColumns, int32 InWordSize) :
			NumRows(InNumRows),
			NumColumns(InNumColumns),
			WordSize(InWordSize)
		{
			DataBuffer.SetNum(NumRows * NumColumns * InWordSize);
		}

		int32 NumRows;
		int32 NumColumns;
		int32 WordSize;
		TArray<uint8> DataBuffer;

		/**
		 * Serialize the data into an archive
		 * DataBuffer will be compressed, empty buffers are handled gracefully
		 */
		void Serialize(FArchive& InAr);
	};

	/** Resolution of the loaded HF model data */
	int32 HFResolution = 0;

	/** Resolution of the loaded LF model data */
	int32 LFResolution = 0;

	/** Model data used to do the synthesis part of the model */
	TSortedMap<TITAN_NAMESPACE::ts::Data_Type, FModelDataContainer> SynthesisModelData;

	/** 
	 * Maps that complement the synthesis to get the final result 
	 * Correspond to the types of supported maps: neutral and animated 1-3
	 */
	FModelDataContainer AlbedoHFMaps[4];
	FModelDataContainer AlbedoLFMaps[4];
	FModelDataContainer NormalHFMaps[4];
	FModelDataContainer CavityHFMap;

	/**
	 * Load the model data from the input folder
	 * First checks if there is a compressed file with the data in the folder, tries to load the raw loose files
	 * if no archive is available
	 */
	void Load(const FString& InModelDataFolderPath, const FString& InCompressedFileName = TEXT("compressed.ar"));

	/**
	 * Load the model data from the loose files in the input folder
	 * @param InModelDataFolderPath, folder with the npy files storing the model data for the texture synthesis model
	 * @param bLoadAnimatedMaps, set to false to not load the HF animated map from the data
	 */
	void LoadMapsFromFolder(const FString& InModelDataFolderPath, bool bLoadHFAnimatedMaps = true);

	/**
	 * Returns true if the loaded model data can be used to do synthesis
	 */
	bool IsValidForSynthesis() const;

	/**
	 * Serialize to an archive, all internal data containers will be compressed
	 */
	void Serialize(FArchive& InAr);
};

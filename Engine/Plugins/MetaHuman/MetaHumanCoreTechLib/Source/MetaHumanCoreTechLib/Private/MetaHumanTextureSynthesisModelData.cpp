// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanTextureSynthesisModelData.h"
#include "MetaHumanCoreTechLibGlobals.h"

#include "Containers/AnsiString.h"
#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Misc/StringBuilder.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Logging/StructuredLog.h"
#include "Misc/Paths.h"
#include "UObject/ObjectMacros.h"

#include <carbon/io/NpyFileFormat.h>


namespace UE::MetaHuman
{
	static FMetaHumanTextureSynthesizerModelData::FModelDataContainer LoadFromNPY(const FString& ModelDataPath, int64 DataTypeSize)
	{
		FMetaHumanTextureSynthesizerModelData::FModelDataContainer ResultModelData;

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (IFileHandle* FileHandle = PlatformFile.OpenRead(*ModelDataPath))
		{
			ON_SCOPE_EXIT
			{
				delete FileHandle;
			};

			// Load npy file header, re-purposed TITAN_NAMESPACE::npy::LoadNPYRawHeader() to use IFileHandle
			CARBON_NPY_NAMESPACE::NPYHeader header;
			{
				char preHeader[10];
				if (!FileHandle->Read(reinterpret_cast<uint8*>(preHeader), 10))
				{
					UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Failed to read NumPy preheader");
					return ResultModelData;
				}

				const char* magicString = "\x93NUMPY";
				preHeader[9] = '\0';
				if (FCStringAnsi::Strncmp(preHeader, magicString, 6) != 0)
				{
					UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Not a NumPy file");
					return ResultModelData;
				}
				if ((preHeader[6] != '\x01') || (preHeader[7] != '\x00'))
				{
					UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Unsupported NPY version");
					return ResultModelData;
				}

				int headerLen = (unsigned char)preHeader[8] + ((unsigned char)preHeader[9] << 8);
				if ((headerLen + 10) % 64 != 0)
				{
					// old numpy version save using an unaligned header
					UE_LOGFMT(LogMetaHumanCoreTechLib, Warning, "Unaligned NPY header: {headerLen}", headerLen);
				}

				std::vector<char> htxt(headerLen);
				if (!FileHandle->Read(reinterpret_cast<uint8*>(htxt.data()), headerLen))
				{
					UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Failed to read NumPy header");
					return ResultModelData;
				}

				CARBON_NPY_NAMESPACE::LoadNPYRawHeader(htxt, header);
			}

			if (header.m_shape.size() > 2)
			{
				UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Only 1D and 2D arrays are supported.");
				return ResultModelData;
			}

			int n_rows = static_cast<int>(header.m_shape[0]);
			int n_cols = 1;
			if (header.m_shape.size() == 2)
			{
				n_cols = static_cast<int>(header.m_shape[1]);
			}

			if (DataTypeSize != header.DataTypeSize())
			{
				UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Mismatching type.");
				return ResultModelData;
			}

			// Read the data to a TArray 
			ResultModelData = FMetaHumanTextureSynthesizerModelData::FModelDataContainer(n_rows, n_cols, static_cast<int32>(DataTypeSize));

			if (!FileHandle->Read(ResultModelData.DataBuffer.GetData(), ResultModelData.DataBuffer.Num()))
			{
				UE_LOGFMT(LogMetaHumanCoreTechLib, Warning, "Failed to read data from numpy file.");
			}
		}

		return ResultModelData;
	}

	static void LoadModelMap(const FString & InModelDataFolderPath,
		TITAN_NAMESPACE::ts::TextureType texture_type,
		TITAN_NAMESPACE::ts::Frequency frequency,
		int map_id,
		FMetaHumanTextureSynthesizerModelData::FModelDataContainer & OutBufferedModelData)
	{
		const FString ModelDataPath = InModelDataFolderPath + FString::Printf(TEXT("/%s_%s_%i.npy"),
			UTF8_TO_TCHAR(TITAN_NAMESPACE::ts::TextureModelParams::TextureTypeToStr(texture_type)),
			UTF8_TO_TCHAR(TITAN_NAMESPACE::ts::TextureModelParams::FrequencyToStr(frequency)),
			map_id);

		if (FPaths::FileExists(ModelDataPath))
		{
			if (frequency == TITAN_NAMESPACE::ts::Frequency::LF)
			{
				OutBufferedModelData = LoadFromNPY(ModelDataPath, sizeof(uint16));
			}
			else
			{
				OutBufferedModelData = LoadFromNPY(ModelDataPath, sizeof(uint8));
			}
		}
	}

	static int32 GetResolutionFromModelData(const FMetaHumanTextureSynthesizerModelData::FModelDataContainer & InModelData)
	{
		// Assume that each row in the loaded HF Maps is an image with a flattened layout of (res, res, 3)
		if (!InModelData.DataBuffer.IsEmpty())
		{
			const int32 NumValues = InModelData.NumColumns;
			return static_cast<int32>(FMath::Sqrt(NumValues / 3.0f));
		}

		UE_LOGFMT(LogMetaHumanCoreTechLib, Error, "Empty model data for resolution");

		return 0;
	}
}



void FMetaHumanTextureSynthesizerModelData::FModelDataContainer::Serialize(FArchive& InAr)
{
	InAr << NumRows;
	InAr << NumColumns;
	InAr << WordSize;

	int32 UncompressedBufferSize = 0;
	if (InAr.IsLoading())
	{
		// Read uncompressed buffer size
		InAr << UncompressedBufferSize;
		DataBuffer.SetNumUninitialized(UncompressedBufferSize);
	}
	else
	{
		// Store uncompressed buffer size
		UncompressedBufferSize = DataBuffer.Num();
		InAr << UncompressedBufferSize;
	}

	if (UncompressedBufferSize > 0)
	{
		InAr.SerializeCompressedNew(DataBuffer.GetData(), DataBuffer.Num(), NAME_Oodle, NAME_Oodle, COMPRESS_BiasMemory);
	}
}

void FMetaHumanTextureSynthesizerModelData::Load(const FString& InModelDataFolderPath, const FString& InCompressedFileName /*= TEXT("compressed.ar")*/)
{
	const FString CompressedFilePath = InModelDataFolderPath / InCompressedFileName;

	// If there is a an archive with the model data, load from it
	if (FPaths::FileExists(CompressedFilePath))
	{
		UE_LOGFMT(LogMetaHumanCoreTechLib, Display, "Loading texture synthesis model data from file {CompressedFilePath}", CompressedFilePath);

		TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileReader(*CompressedFilePath));
		if (FileAr)
		{
			Serialize(*FileAr);
		}
	}
	else
	{
		// If there is not archive file, try to load the "raw" npy files
		LoadMapsFromFolder(InModelDataFolderPath);
	}

	if (IsValidForSynthesis())
	{
		HFResolution = UE::MetaHuman::GetResolutionFromModelData(AlbedoHFMaps[0]);
		LFResolution = UE::MetaHuman::GetResolutionFromModelData(AlbedoLFMaps[0]);
	}
}

void FMetaHumanTextureSynthesizerModelData::LoadMapsFromFolder(const FString& InModelDataFolderPath, bool bLoadHFAnimatedMaps)
{
	// TODO: more checks

	const int32 MaxHFMapIndex = bLoadHFAnimatedMaps ? 4 : 1;

	// Load the HF maps
	for (int32 i = 0; i < MaxHFMapIndex; ++i)
	{
		UE::MetaHuman::LoadModelMap(InModelDataFolderPath, TITAN_NAMESPACE::ts::TextureType::ALBEDO, TITAN_NAMESPACE::ts::Frequency::HF, i, AlbedoHFMaps[i]);
		UE::MetaHuman::LoadModelMap(InModelDataFolderPath, TITAN_NAMESPACE::ts::TextureType::NORMAL, TITAN_NAMESPACE::ts::Frequency::HF, i, NormalHFMaps[i]);
	}

	for (int32 i = 0; i < 4; ++i)
	{
		UE::MetaHuman::LoadModelMap(InModelDataFolderPath, TITAN_NAMESPACE::ts::TextureType::ALBEDO, TITAN_NAMESPACE::ts::Frequency::LF, i, AlbedoLFMaps[i]);
	}

	UE::MetaHuman::LoadModelMap(InModelDataFolderPath, TITAN_NAMESPACE::ts::TextureType::CAVITY, TITAN_NAMESPACE::ts::Frequency::HF, 0, CavityHFMap);

	SynthesisModelData.Reset();

	// Load LF map
	{
		const FString FullModelPath = InModelDataFolderPath + TEXT("/") + UTF8_TO_TCHAR(TITAN_NAMESPACE::ts::TextureModelParams::TextureTypeToStr(TITAN_NAMESPACE::ts::TextureType::ALBEDO)) + TEXT("_LF_model.npy");
		SynthesisModelData.Add(TITAN_NAMESPACE::ts::Data_Type::LF_model, UE::MetaHuman::LoadFromNPY(FullModelPath, sizeof(float)));
	}

	SynthesisModelData.Add(TITAN_NAMESPACE::ts::Data_Type::pca_mu, UE::MetaHuman::LoadFromNPY(InModelDataFolderPath + TEXT("/skin_tones_pca_mu.npy"), sizeof(float)));
	SynthesisModelData.Add(TITAN_NAMESPACE::ts::Data_Type::pca_S, UE::MetaHuman::LoadFromNPY(InModelDataFolderPath + TEXT("/skin_tones_pca_S.npy"), sizeof(float)));
	SynthesisModelData.Add(TITAN_NAMESPACE::ts::Data_Type::pca_T, UE::MetaHuman::LoadFromNPY(InModelDataFolderPath + TEXT("/skin_tones_pca_T.npy"), sizeof(float)));
	SynthesisModelData.Add(TITAN_NAMESPACE::ts::Data_Type::v1_ranges, UE::MetaHuman::LoadFromNPY(InModelDataFolderPath + TEXT("/v1_ranges.npy"), sizeof(float)));	
	SynthesisModelData.Add(TITAN_NAMESPACE::ts::Data_Type::yellow_mask, UE::MetaHuman::LoadFromNPY(InModelDataFolderPath + TEXT("/yellow_mask.npy"), sizeof(float)));
}

bool FMetaHumanTextureSynthesizerModelData::IsValidForSynthesis() const
{
	// LF albedo maps should always be available
	for (int32 i = 0; i < 4; ++i)
	{
		if (AlbedoLFMaps[i].DataBuffer.IsEmpty())
		{
			return false;
		}
	}

	// Neutral HF albedo, normal & cavity should always be available for synthesis
	return	!AlbedoHFMaps[0].DataBuffer.IsEmpty() && 
			!NormalHFMaps[0].DataBuffer.IsEmpty() &&
			!CavityHFMap.DataBuffer.IsEmpty();
}

void FMetaHumanTextureSynthesizerModelData::Serialize(FArchive& InAr)
{
	static constexpr int32 DataTypeIndexCount = static_cast<int32>(TITAN_NAMESPACE::ts::Data_Type::Count);
	for (int32 DataTypeIndex = 0; DataTypeIndex < DataTypeIndexCount; ++DataTypeIndex)
	{
		FModelDataContainer& ModelDataMap = SynthesisModelData.FindOrAdd(static_cast<TITAN_NAMESPACE::ts::Data_Type>(DataTypeIndex));

		InAr << DataTypeIndex;
		ModelDataMap.Serialize(InAr);
	}

	for (int32 i = 0; i < 4; ++i)
	{
		AlbedoHFMaps[i].Serialize(InAr);
		AlbedoLFMaps[i].Serialize(InAr);
		NormalHFMaps[i].Serialize(InAr);
	}

	CavityHFMap.Serialize(InAr);
}

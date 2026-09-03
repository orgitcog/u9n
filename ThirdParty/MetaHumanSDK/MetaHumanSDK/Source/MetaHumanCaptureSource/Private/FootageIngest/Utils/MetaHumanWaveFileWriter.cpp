// Copyright Epic Games, Inc. All Rights Reserved.

#include "FootageIngest/Utils/MetaHumanWaveFileWriter.h"
#include "IMediaAudioSample.h"
#include "AudioWaveFormatParser.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanWaveFileWriter, Log, All);

class FMetaHumanWaveFileWriter : public IMetaHumanWaveFileWriter
{
public:
	FMetaHumanWaveFileWriter();
	virtual ~FMetaHumanWaveFileWriter() override;

	virtual bool Open(const FString& InWavFilename, int32 InSampleRate, int32 InNumChannels, int32 InBitsPerSample) override;
	virtual bool Append(IMediaAudioSample* InSample) override;
	virtual bool Close() override;

private:
	int32 SampleRate;
	int32 NumChannels;
	int32 BitsPerSample;
	int32 BlockSize;	// NumChannels * BitsPerSample / 8

	uint32 TotalDataBytesWritten;
	IFileHandle* FileHandle;
};


static constexpr uint32 WaveFileHeaderSize = 44;
static constexpr uint32 PCMFormatChunkSize = 16;

//	From AudioWaveFormatParser.cpp
#define CHUNK_ID_RIFF			(0x46464952)	// "RIFF"
#define CHUNK_TYPE_WAVE			(0x45564157)	// "WAVE"
#define CHUNK_ID_FMT			(0x20746D66)	// "fmt "
#define CHUNK_ID_DATA			(0x61746164)	// "data"

FMetaHumanWaveFileWriter::FMetaHumanWaveFileWriter() : SampleRate(44100), NumChannels(1),
BitsPerSample(16), BlockSize(2),
TotalDataBytesWritten(0),
FileHandle(nullptr)
{
}

FMetaHumanWaveFileWriter::~FMetaHumanWaveFileWriter()
{
	if (FileHandle)
	{
		delete FileHandle;
		FileHandle = nullptr;
	}
}

bool FMetaHumanWaveFileWriter::Open(const FString& InWavFilename, const int32 InSampleRate,
											const int32 InNumChannels,
											const int32 InBitsPerSample)
{
	SampleRate = InSampleRate;
	NumChannels = InNumChannels;
	BitsPerSample = InBitsPerSample;
	BlockSize = InNumChannels * InBitsPerSample / 8;

	FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*InWavFilename);

	if (!FileHandle)
	{
		UE_LOG(LogMetaHumanWaveFileWriter, Error, TEXT("Failed to create the wave file: %s"), *InWavFilename);
		return false;
	}

	if (!FileHandle->Seek(WaveFileHeaderSize))
	{
		UE_LOG(LogMetaHumanWaveFileWriter, Error, TEXT("Failed to access the wave file: %s"), *InWavFilename);
		return false;
	}
	return true;
}

bool FMetaHumanWaveFileWriter::Append(IMediaAudioSample* InSample)
{
	// Ticks are in 100ns units
	const int64 Time = InSample->GetTime().Time.GetTicks();
	const int64 Duration = InSample->GetDuration().GetTicks();
	const uint8* Data = static_cast<const uint8*>(InSample->GetBuffer());
	const int32 Frames = InSample->GetFrames();
	const uint32 Size = Frames * BlockSize;
	uint32 BytesToSkip = 0;

	if (Time + Duration < 0)
	{
		return true;
	}

	if (TotalDataBytesWritten <= 0)
	{
		//	The audio samples can kick in before or after the first video frame arrives.
		//	Add zero paddings or skip some samples to match the start of the video.
		if (Time < 0)
		{
			BytesToSkip = FMath::RoundToInt(Time * SampleRate * -1.0e-7) * BlockSize;
		}
		else
		{
			const int32 SamplesToPad = FMath::RoundToInt(Time * SampleRate * 1e-7);
			TArray<uint8> ZeroBlock;
			ZeroBlock.SetNumZeroed(BlockSize);

			for (int32 i = 0; i < SamplesToPad; ++i)
			{
				FileHandle->Write(ZeroBlock.GetData(), ZeroBlock.Num());
				TotalDataBytesWritten += BlockSize;
			}
		}
	}

	if (BytesToSkip >= Size)
	{
		return true;
	}

	if (!FileHandle->Write(Data + BytesToSkip, Size - BytesToSkip))
	{
		UE_LOG(LogMetaHumanWaveFileWriter, Error, TEXT("Failed to write onto the wave file"));
		return false;
	}
	TotalDataBytesWritten += Size - BytesToSkip;

	return true;
}

bool FMetaHumanWaveFileWriter::Close()
{
	if (!FileHandle)
	{
		UE_LOG(LogMetaHumanWaveFileWriter, Error, TEXT("Failed to close the wave file"));
		return false;
	}

	uint32 UInt32Buffer;
	FFormatChunk PCMFormatChunk;
	FileHandle->Seek(0);

	UInt32Buffer = CHUNK_ID_RIFF;
	FileHandle->Write(reinterpret_cast<const uint8*>(&UInt32Buffer), sizeof(uint32));

	UInt32Buffer = TotalDataBytesWritten + WaveFileHeaderSize - 8;
	FileHandle->Write(reinterpret_cast<const uint8*>(&UInt32Buffer), sizeof(uint32));

	UInt32Buffer = CHUNK_TYPE_WAVE;
	FileHandle->Write(reinterpret_cast<const uint8*>(&UInt32Buffer), sizeof(uint32));

	UInt32Buffer = CHUNK_ID_FMT;
	FileHandle->Write(reinterpret_cast<const uint8*>(&UInt32Buffer), sizeof(uint32));

	UInt32Buffer = PCMFormatChunkSize;
	FileHandle->Write(reinterpret_cast<const uint8*>(&UInt32Buffer), sizeof(uint32));

	PCMFormatChunk.FormatTag = 1;	// Linear PCM
	PCMFormatChunk.NumChannels = NumChannels;
	PCMFormatChunk.SamplesPerSec = SampleRate;
	PCMFormatChunk.AverageBytesPerSec = SampleRate * NumChannels * BitsPerSample / 8;
	PCMFormatChunk.BlockAlign = NumChannels * BitsPerSample / 8;
	PCMFormatChunk.BitsPerSample = BitsPerSample;

	FileHandle->Write(reinterpret_cast<const uint8*>(&PCMFormatChunk), PCMFormatChunkSize);

	UInt32Buffer = CHUNK_ID_DATA;
	FileHandle->Write(reinterpret_cast<const uint8*>(&UInt32Buffer), sizeof(uint32));

	UInt32Buffer = TotalDataBytesWritten;
	FileHandle->Write(reinterpret_cast<const uint8*>(&UInt32Buffer), sizeof(uint32));

	FileHandle->Flush();
	delete FileHandle;
	FileHandle = nullptr;

	return true;
}

TSharedPtr<IMetaHumanWaveFileWriter> IMetaHumanWaveFileWriter::Create()
{
	return MakeShared<FMetaHumanWaveFileWriter>();
}

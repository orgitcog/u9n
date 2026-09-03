// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/UtilitiesMP4.h"
#include "Utilities/MP4Boxes/MP4Boxes.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/Archive.h"
#include "Serialization/ArrayReader.h"


namespace Electra
{
namespace UtilitiesMP4
{


FMP4AtomReaderHelper::FMP4AtomReaderHelper(const TConstArrayView<uint8>& InData)
	: DataPtr(InData.GetData()), DataSize(InData.Num()), CurrentOffset(0)
{
}

int64 FMP4AtomReaderHelper::GetCurrentOffset() const
{
	return CurrentOffset;
}

int64 FMP4AtomReaderHelper::GetNumBytesRemaining() const
{
	return DataSize - GetCurrentOffset();
}

const uint8* FMP4AtomReaderHelper::GetCurrentDataPointer() const
{
	return GetNumBytesRemaining() ? DataPtr + GetCurrentOffset() : nullptr;
}

void FMP4AtomReaderHelper::SetCurrentOffset(int64 InNewOffset)
{
	check(InNewOffset >= 0 && InNewOffset <= DataSize);
	if (InNewOffset >= 0 && InNewOffset <= DataSize)
	{
		CurrentOffset = InNewOffset;
	}
}

bool FMP4AtomReaderHelper::ReadVersionAndFlags(uint8& OutVersion, uint32& OutFlags)
{
	uint32 VersionAndFlags = 0;
	if (!Read(VersionAndFlags))
	{
		return false;
	}
	OutVersion = (uint8)(VersionAndFlags >> 24);
	OutFlags = VersionAndFlags & 0x00ffffff;
	return true;
}

bool FMP4AtomReaderHelper::ReadString(FString& OutString, uint16 InNumBytes)
{
	OutString.Empty();
	if (InNumBytes == 0)
	{
		return true;
	}
	TArray<uint8> Buf;
	Buf.AddUninitialized(InNumBytes);
	if (ReadBytes(Buf.GetData(), InNumBytes))
	{
		// Check for UTF16 BOM
		if (InNumBytes >= 2 && ((Buf[0] == 0xff && Buf[1] == 0xfe) || (Buf[0] == 0xfe && Buf[1] == 0xff)))
		{
			// String uses UTF16, which is not supported
			return false;
		}
		FUTF8ToTCHAR cnv((const ANSICHAR*)Buf.GetData(), InNumBytes);
		OutString = FString::ConstructFromPtrSize(cnv.Get(), cnv.Length());
		return true;
	}
	return false;
}

bool FMP4AtomReaderHelper::ReadStringUTF8(FString& OutString, int32 InNumBytes)
{
	OutString.Empty();
	if (InNumBytes == 0)
	{
		return true;
	}
	else if (InNumBytes < 0)
	{
		InNumBytes = GetNumBytesRemaining();
		check(InNumBytes >= 0);
		if (InNumBytes < 0)
		{
			return false;
		}
	}
	TArray<uint8> Buf;
	Buf.AddUninitialized(InNumBytes);
	if (ReadBytes(Buf.GetData(), InNumBytes))
	{
		FUTF8ToTCHAR cnv((const ANSICHAR*)Buf.GetData(), InNumBytes);
		OutString = FString::ConstructFromPtrSize(cnv.Get(), cnv.Length());
		return true;
	}
	return false;
}

bool FMP4AtomReaderHelper::ReadStringUTF16(FString& OutString, int32 InNumBytes)
{
	OutString.Empty();
	if (InNumBytes == 0)
	{
		return true;
	}
	else if (InNumBytes < 0)
	{
		InNumBytes = GetNumBytesRemaining();
		check(InNumBytes >= 0);
		if (InNumBytes < 0)
		{
			return false;
		}
	}
	TArray<uint8> Buf;
	Buf.AddUninitialized(InNumBytes);
	if (ReadBytes(Buf.GetData(), InNumBytes))
	{
		check(!"TODO");
/*
		FUTF8ToTCHAR cnv((const ANSICHAR*)Buf.GetData(), InNumBytes);
		OutString = FString(cnv.Length(), cnv.Get());
		return true;
*/
	}
	return false;
}

bool FMP4AtomReaderHelper::ReadAsNumber(uint64& OutValue, int32 InNumBytes)
{
	OutValue = 0;
	if (InNumBytes < 0 || InNumBytes > 8)
	{
		return false;
	}
	for(int32 i=0; i<InNumBytes; ++i)
	{
		uint8 d;
		if (!Read(d))
		{
			return false;
		}
		OutValue = (OutValue << 8) | d;
	}
	return true;
}
bool FMP4AtomReaderHelper::ReadAsNumber(int64& OutValue, int32 InNumBytes)
{
	OutValue = 0;
	if (InNumBytes < 0 || InNumBytes > 8)
	{
		return false;
	}
	for(int32 i=0; i<InNumBytes; ++i)
	{
		uint8 d;
		if (!Read(d))
		{
			return false;
		}
		if (i==0 && d>127)
		{
			OutValue = -1;
		}
		OutValue = (OutValue << 8) | d;
	}
	return true;
}
bool FMP4AtomReaderHelper::ReadAsNumber(float& OutValue)
{
	uint32 Flt;
	if (Read(Flt))
	{
		OutValue = *reinterpret_cast<float*>(&Flt);
		return true;
	}
	return false;
}
bool FMP4AtomReaderHelper::ReadAsNumber(double& OutValue)
{
	uint64 Dbl;
	if (Read(Dbl))
	{
		OutValue = *reinterpret_cast<double*>(&Dbl);
		return true;
	}
	return false;
}

bool FMP4AtomReaderHelper::ReadBytes(void* Buffer, int32 InNumBytes)
{
	return ReadData(Buffer, InNumBytes) == InNumBytes;
}

int32 FMP4AtomReaderHelper::ReadData(void* IntoBuffer, int32 NumBytesToRead)
{
	if (NumBytesToRead <= 0)
	{
		return 0;
	}
	int64 NumAvail = DataSize - CurrentOffset;
	if (NumAvail >= NumBytesToRead)
	{
		if (IntoBuffer)
		{
			FMemory::Memcpy(IntoBuffer, DataPtr + CurrentOffset, NumBytesToRead);
		}
		CurrentOffset += NumBytesToRead;
		return NumBytesToRead;
	}
	return -1;
}

bool FMP4AtomReaderHelper::ParseIntoBoxInfo(FMP4BoxInfo& OutBoxInfo, int64 InAtFileOffset)
{
	// Clear output with default values before continuing.
	OutBoxInfo = FMP4BoxInfo();
	uint32 BoxSize, BoxType;
	if (!Read(BoxSize) || !Read(BoxType))
	{
		return false;
	}
	OutBoxInfo.Offset = InAtFileOffset;
	OutBoxInfo.Size = (int64) BoxSize;
	OutBoxInfo.Type = BoxType;
#if !UE_BUILD_SHIPPING
	OutBoxInfo.Name[0] = (char) ((BoxType >> 24) & 255);
	OutBoxInfo.Name[1] = (char) ((BoxType >> 16) & 255);
	OutBoxInfo.Name[2] = (char) ((BoxType >>  8) & 255);
	OutBoxInfo.Name[3] = (char) ((BoxType >>  0) & 255);
	OutBoxInfo.Name[4] = 0;
#endif
	OutBoxInfo.DataOffset = 8;
	// Check the box size value.
	if (OutBoxInfo.Size == 1)
	{
		uint64 BoxSize64;
		if (!Read(BoxSize64))
		{
			return false;
		}
		OutBoxInfo.DataOffset += 8;
		OutBoxInfo.Size = (int64) BoxSize64;
	}
	// Is the box type a UUID ?
	if (OutBoxInfo.Type == MakeBoxAtom('u','u','i','d'))
	{
		// Read additional 16 bytes for the UUID
		if (ReadData(OutBoxInfo.UUID, 16) != 16)
		{
			return false;
		}
		OutBoxInfo.DataOffset += 16;
	}
	OutBoxInfo.Data = MakeConstArrayView(GetCurrentDataPointer(), OutBoxInfo.Size ? OutBoxInfo.Size - OutBoxInfo.DataOffset : GetNumBytesRemaining());
	return true;
}



/**********************************************************************************************************************/
/**********************************************************************************************************************/
/**********************************************************************************************************************/



FMP4BoxLocatorReader::~FMP4BoxLocatorReader()
{
}

bool FMP4BoxLocatorReader::LocateAndReadRootBoxes(TArray<TSharedPtr<FMP4BoxData, ESPMode::ThreadSafe>>& OutBoxInfos, const TSharedPtr<IBaseDataReader, ESPMode::ThreadSafe>& InDataReader, const TArray<uint32>& InFirstBoxes, const TArray<uint32>& InStopAfterBoxes, const TArray<uint32>& InReadDataOfBoxes, IBaseDataReader::FCancellationCheckDelegate InCheckCancellationDelegate)
{
	// We NEVER want to read the `mdat` box here!
	check(!InReadDataOfBoxes.Contains(MakeBoxAtom('m','d','a','t')));

	check(InDataReader.IsValid());
	if (!InDataReader.IsValid())
	{
		return false;
	}
	CurrentOffset = InDataReader->GetCurrentFileOffset();

	#define CHECK_READ(NumReq) \
		if (NumRead == IBaseDataReader::EResult::Canceled) \
		{ return false;	} \
		else if (NumRead == IBaseDataReader::EResult::ReadError) \
		{ LastError = InDataReader->GetLastError(); return false; } \
		else if (/*NumRead == IBaseDataReader::EResult::ReachedEOF ||*/ NumRead != NumReq) \
		{ LastError = FString::Printf(TEXT("File truncated. Cannot read %lld bytes from offset %lld"), (long long int)NumReq, (long long int)CurrentOffset+BoxInternalOffset); return false; }

	int64 TotalFileSize = -1;
	for(int32 BoxNum=0; ;++BoxNum)
	{
		union UBuf
		{
			uint64 As64[2];
			uint32 As32[4];
			uint8 As8[16];
		};
		UBuf BoxSizeAndType;

		// Read 8 bytes
		uint32 BoxInternalOffset = 0;
		int64 NumRead = InDataReader->ReadData(BoxSizeAndType.As64, 8, CurrentOffset, InCheckCancellationDelegate);
		CHECK_READ(8);

		BoxInternalOffset = 8;
		TSharedPtr<FMP4BoxData, ESPMode::ThreadSafe> bi = MakeShared<FMP4BoxData, ESPMode::ThreadSafe>();
		bi->Size = (int64) GetFromBigEndian(BoxSizeAndType.As32[0]);
		bi->Offset = CurrentOffset;
		bi->Type = GetFromBigEndian(BoxSizeAndType.As32[1]);
#if !UE_BUILD_SHIPPING
		bi->Name[0] = BoxSizeAndType.As8[4]; bi->Name[1] = BoxSizeAndType.As8[5]; bi->Name[2] = BoxSizeAndType.As8[6]; bi->Name[3] = BoxSizeAndType.As8[7]; bi->Name[4] = 0;
#endif

		// After having read the first few bytes we should now know the overall filesize.
		if (BoxNum == 0)
		{
			TotalFileSize = InDataReader->GetTotalFileSize();
			if (InFirstBoxes.Num() && !InFirstBoxes.Contains(bi->Type))
			{
				LastError = TEXT("Invalid mp4 file: First box is not of expected type");
				return false;
			}
		}

		// Check the box size value.
		if (bi->Size == 0)
		{
			// Zero size means "until the end of the file".
			bi->Size = TotalFileSize > 0 ? TotalFileSize - CurrentOffset : -1;
		}
		else if (bi->Size == 1)
		{
			// A size of 1 indicates that the size is expressed as a 64 bit value following the box type.
			// Read additional 8 bytes for the 64 bit length
			NumRead = InDataReader->ReadData(BoxSizeAndType.As64, 8, CurrentOffset+BoxInternalOffset, InCheckCancellationDelegate);
			CHECK_READ(8);
			BoxInternalOffset += 8;
			bi->Size = (int64) GetFromBigEndian(BoxSizeAndType.As64[0]);
		}

		// Is the box type a UUID ?
		if (bi->Type == MakeBoxAtom('u','u','i','d'))
		{
			// Read additional 16 bytes for the UUID
			NumRead = InDataReader->ReadData(bi->UUID, 16, CurrentOffset+BoxInternalOffset, InCheckCancellationDelegate);
			CHECK_READ(16);
			BoxInternalOffset += 16;
		}
		// Shall we read this box?
		if ((bi->Type != MakeBoxAtom('m','d','a','t')) && (InReadDataOfBoxes.IsEmpty() || InReadDataOfBoxes.Contains(bi->Type)))
		{
			bi->DataBuffer.SetNumUninitialized(bi->Size-BoxInternalOffset);
			bi->Data = MakeConstArrayView(bi->DataBuffer);
			NumRead = InDataReader->ReadData(bi->DataBuffer.GetData(), bi->Size-BoxInternalOffset, CurrentOffset+BoxInternalOffset, InCheckCancellationDelegate);
		}
		else
		{
			NumRead = InDataReader->ReadData(nullptr, bi->Size-BoxInternalOffset, CurrentOffset+BoxInternalOffset, InCheckCancellationDelegate);
		}
		CHECK_READ(bi->Size - BoxInternalOffset);
		bi->DataOffset = BoxInternalOffset;

		// Advance the current offset, whether we have read the box or not.
		CurrentOffset += bi->Size;
		check(CurrentOffset == InDataReader->GetCurrentFileOffset());
		bool bStopNow = InStopAfterBoxes.Contains(bi->Type);
		OutBoxInfos.Emplace(MoveTemp(bi));
		if (bStopNow || InDataReader->HasReachedEOF())
		{
			return true;
		}
	}
}





bool FMP4BoxTreeParser::ParseBoxTreeInternal(const TWeakPtr<FMP4BoxBase>& InParent, const FMP4BoxInfo& InBox)
{
	BoxTree = FMP4BoxFactory::Get().Create(InParent, InBox);
	// Parse the enclosed box recursively unless it contains
	// a list of entries that only the box knows how to parse.
	if (BoxTree.IsValid() && !BoxTree->IsLeafBox() && !BoxTree->IsListOfEntries())
	{
		// The data of this container box represents one or several other boxes.
		// We need to parse them one by one until there is no more data here.
		const TConstArrayView<uint8>& bd(BoxTree->GetBoxData());
		const uint8* BoxData = bd.GetData();
		int64 BoxBytesRemaining = bd.Num();
		int64 NextBoxOffset = BoxTree->GetBoxFileOffset() + BoxTree->GetBoxDataOffset();
		while(BoxBytesRemaining > 0)
		{
			FMP4AtomReaderHelper bh(MakeConstArrayView(BoxData, BoxBytesRemaining));
			FMP4BoxInfo bi;
			if (!bh.ParseIntoBoxInfo(bi, NextBoxOffset))
			{
				return false;
			}
			FMP4BoxTreeParser bp;
			if (!bp.ParseBoxTreeInternal(BoxTree, bi))
			{
				return false;
			}
			BoxTree->AddChildBox(MoveTemp(bp.BoxTree));
			BoxData += bi.Size;
			BoxBytesRemaining -= bi.Size;
			NextBoxOffset = bi.Offset + bi.Size;
		}
	}
	return true;
}

bool FMP4BoxTreeParser::ParseBoxTree(const TSharedPtr<FMP4BoxInfo, ESPMode::ThreadSafe>& InRootBox)
{
	check(InRootBox.IsValid());
	bool bOk = ParseBoxTreeInternal(nullptr, *InRootBox);
	if (bOk && BoxTree.IsValid())
	{
		BoxTree->SetRootBoxData(InRootBox);
	}
	return bOk;
}



} // namespace UtilitiesMP4



/**********************************************************************************************************************/
/**********************************************************************************************************************/
/**********************************************************************************************************************/

class FFileDataReader : public IFileDataReader
{
public:
	~FFileDataReader();

	bool Open(const FString& InFilename) override;

	//-------------------------------------------------------------------------
	// Methods from IBaseDataReader
	//
	int64 ReadData(void* InOutBuffer, int64 InNumBytes, int64 InFromOffset, FCancellationCheckDelegate InCheckCancellationDelegate) override;
	int64 GetTotalFileSize() override;
	int64 GetCurrentFileOffset() override;
	bool HasReachedEOF() override;
	FString GetLastError() override;
private:
	void Close();
	FString LastError;
	FArchive* Archive = nullptr;
	int64 TotalFileSize = -1;
	int64 CurrentOffset = 0;
};

TSharedPtr<IFileDataReader, ESPMode::ThreadSafe> IFileDataReader::Create()
{
	return MakeShared<FFileDataReader, ESPMode::ThreadSafe>();
}

FFileDataReader::~FFileDataReader()
{
	Close();
}

void FFileDataReader::Close()
{
	if (Archive)
	{
		Archive->Close();
		delete Archive;
		Archive = nullptr;
	}
}

bool FFileDataReader::Open(const FString& InFilename)
{
	Archive = IFileManager::Get().CreateFileReader(*InFilename, 0);
	if (!Archive)
	{
		LastError = FString::Printf(TEXT("Failed to open file \"%s\""), *InFilename);
		return false;
	}
	TotalFileSize = Archive->TotalSize();
	return true;
}

int64 FFileDataReader::ReadData(void* InOutBuffer, int64 InNumBytes, int64 InFromOffset, FCancellationCheckDelegate InCheckCancellationDelegate)
{
	check(InNumBytes >= 0);
	if (!Archive)
	{
		LastError = FString::Printf(TEXT("File reader has not been openend"));
		return EResult::ReadError;
	}
	if (InNumBytes <= 0)
	{
		return 0;
	}
	if (CurrentOffset != InFromOffset)
	{
		check(InFromOffset >= 0 && InFromOffset <= TotalFileSize);
		CurrentOffset = InFromOffset >= 0 ? InFromOffset <= TotalFileSize ? InFromOffset : TotalFileSize : 0;
		Archive->Seek(CurrentOffset);
	}
	if (InNumBytes + CurrentOffset > TotalFileSize)
	{
		InNumBytes = TotalFileSize - CurrentOffset;
	}
	if (InOutBuffer)
	{
		Archive->Serialize(InOutBuffer, InNumBytes);
	}
	else
	{
		Archive->Seek(CurrentOffset + InNumBytes);
	}
	CurrentOffset += InNumBytes;
	return InNumBytes;
}

int64 FFileDataReader::GetTotalFileSize()
{
	return TotalFileSize;
}

int64 FFileDataReader::GetCurrentFileOffset()
{
	return CurrentOffset;
}

bool FFileDataReader::HasReachedEOF()
{
	return CurrentOffset >= TotalFileSize;
}

FString FFileDataReader::GetLastError()
{
	return LastError;
}



} // namespace Electra

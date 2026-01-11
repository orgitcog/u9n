// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Templates/PimplPtr.h"
#include "Math/NumericLimits.h"
#include "IElectraBaseDataReader.h"

#define UE_API ELECTRABASE_API

namespace Electra
{
#if !PLATFORM_LITTLE_ENDIAN
	static constexpr uint8 GetFromBigEndian(uint8 value)		{ return value; }
	static constexpr int8 GetFromBigEndian(int8 value)			{ return value; }
	static constexpr uint16 GetFromBigEndian(uint16 value)		{ return value; }
	static constexpr int16 GetFromBigEndian(int16 value)		{ return value; }
	static constexpr int32 GetFromBigEndian(int32 value)		{ return value; }
	static constexpr uint32 GetFromBigEndian(uint32 value)		{ return value; }
	static constexpr int64 GetFromBigEndian(int64 value)		{ return value; }
	static constexpr uint64 GetFromBigEndian(uint64 value)		{ return value; }
#else
	static constexpr uint16 EndianSwap(uint16 value)			{ return (value >> 8) | (value << 8); }
	static constexpr int16 EndianSwap(int16 value)				{ return int16(EndianSwap(uint16(value))); }
	static constexpr uint32 EndianSwap(uint32 value)			{ return (value << 24) | ((value & 0xff00) << 8) | ((value >> 8) & 0xff00) | (value >> 24); }
	static constexpr int32 EndianSwap(int32 value)				{ return int32(EndianSwap(uint32(value))); }
	static constexpr uint64 EndianSwap(uint64 value)			{ return (uint64(EndianSwap(uint32(value & 0xffffffffU))) << 32) | uint64(EndianSwap(uint32(value >> 32))); }
	static constexpr int64 EndianSwap(int64 value)				{ return int64(EndianSwap(uint64(value)));}
	static constexpr uint8 GetFromBigEndian(uint8 value)		{ return value; }
	static constexpr int8 GetFromBigEndian(int8 value)			{ return value; }
	static constexpr uint16 GetFromBigEndian(uint16 value)		{ return EndianSwap(value); }
	static constexpr int16 GetFromBigEndian(int16 value)		{ return EndianSwap(value); }
	static constexpr int32 GetFromBigEndian(int32 value)		{ return EndianSwap(value); }
	static constexpr uint32 GetFromBigEndian(uint32 value)		{ return EndianSwap(value); }
	static constexpr int64 GetFromBigEndian(int64 value)		{ return EndianSwap(value); }
	static constexpr uint64 GetFromBigEndian(uint64 value)		{ return EndianSwap(value); }
#endif

	namespace UtilitiesMP4
	{
		class FMP4BoxBase;

		static constexpr uint32 MakeBoxAtom(const uint8 A, const uint8 B, const uint8 C, const uint8 D)
		{
			return (static_cast<uint32>(A) << 24) | (static_cast<uint32>(B) << 16) | (static_cast<uint32>(C) << 8) | static_cast<uint32>(D);
		}

		static constexpr uint32 MakeBoxAtom(const char A, const char B, const char C, const char D)
		{
			return (static_cast<uint32>(A) << 24) | (static_cast<uint32>(B) << 16) | (static_cast<uint32>(C) << 8) | static_cast<uint32>(D);
		}

		static FString GetPrintableBoxAtom(uint32 InAtom)
		{
			TCHAR tc[4];
			tc[0] = (TCHAR) ((InAtom >> 24) & 255);
			tc[1] = (TCHAR) ((InAtom >> 16) & 255);
			tc[2] = (TCHAR) ((InAtom >>  8) & 255);
			tc[3] = (TCHAR) ((InAtom >>  0) & 255);
			for(int32 i=0;i<4; ++i)
			{
				tc[i] = tc[i] >= 32 && tc[i] <= 127 ? tc[i] : TCHAR(' ');
			}
			return FString::ConstructFromPtrSize(tc, 4);
		}

		static FString Printable4CC(const uint32 In4CC)
		{
			FString Out;
			// Not so much just printable as alphanumeric.
			for(uint32 i=0, Atom=In4CC; i<4; ++i, Atom<<=8)
			{
				int32 v = Atom >> 24;
				if ((v >= 'A' && v <= 'Z') || (v >= 'a' && v <= 'z') || (v >= '0' && v <= '9') || v == '_' || v == '.')
				{
					Out.AppendChar(v);
				}
				else
				{
					// Not alphanumeric, return it as a hex string.
					return FString::Printf(TEXT("%08x"), In4CC);
				}
			}
			return Out;
		}

		struct FMP4BoxInfo
		{
			TConstArrayView<uint8> Data;
			uint8 UUID[16] {};
			int64 Size = 0;
			int64 Offset = 0;
			uint32 Type = 0;
			uint32 DataOffset = 0;
		#if !UE_BUILD_SHIPPING
			char Name[5]{0};
		#endif
		};
		struct FMP4BoxData : public FMP4BoxInfo
		{
			TArray<uint8> DataBuffer;
		};


		class FMP4AtomReaderHelper
		{
		public:
			UE_API FMP4AtomReaderHelper(const TConstArrayView<uint8>& InData);

			UE_API bool ParseIntoBoxInfo(FMP4BoxInfo& OutBoxInfo, int64 InAtFileOffset);

			UE_API int64 GetCurrentOffset() const;
			UE_API int64 GetNumBytesRemaining() const;
			UE_API const uint8* GetCurrentDataPointer() const;
			UE_API void SetCurrentOffset(int64 InNewOffset);

			template <typename T>
			bool Read(T& OutValue)
			{
				T Temp = 0;
				int64 NumRead = ReadData(&Temp, sizeof(T));
				if (NumRead == sizeof(T))
				{
					OutValue = ValueFromBigEndian(Temp);
					return true;
				}
				return false;
			}

			UE_API bool ReadVersionAndFlags(uint8& OutVersion, uint32& OutFlags);
			UE_API bool ReadString(FString& OutString, uint16 InNumBytes);
			UE_API bool ReadStringUTF8(FString& OutString, int32 InNumBytes);
			UE_API bool ReadStringUTF16(FString& OutString, int32 InNumBytes);
			UE_API bool ReadBytes(void* OutBuffer, int32 InNumBytes);
			UE_API bool ReadAsNumber(int64& OutValue, int32 InNumBytes);
			UE_API bool ReadAsNumber(uint64& OutValue, int32 InNumBytes);
			UE_API bool ReadAsNumber(float& OutValue);
			UE_API bool ReadAsNumber(double& OutValue);
			bool SkipBytes(int32 InNumBytes)
			{
				return ReadData(nullptr, InNumBytes) == InNumBytes;
			}
		private:
			template <typename T>
			T ValueFromBigEndian(const T value)
			{ return GetFromBigEndian(value); }
			UE_API int32 ReadData(void* IntoBuffer, int32 NumBytesToRead);

			const uint8* DataPtr = nullptr;
			int64 DataSize = 0;
			int64 CurrentOffset = 0;
		};


		class FMP4BoxLocatorReader
		{
		public:
			FMP4BoxLocatorReader() = default;
			UE_API ~FMP4BoxLocatorReader();

			UE_API bool LocateAndReadRootBoxes(TArray<TSharedPtr<FMP4BoxData, ESPMode::ThreadSafe>>& OutBoxInfos, const TSharedPtr<IBaseDataReader, ESPMode::ThreadSafe>& InDataReader, const TArray<uint32>& InFirstBoxes, const TArray<uint32>& InStopAfterBoxes, const TArray<uint32>& InReadDataOfBoxes, IBaseDataReader::FCancellationCheckDelegate InCheckCancellationDelegate);
			FString GetLastError() const
			{ return LastError; }
		private:
			FString LastError;
			int64 CurrentOffset = 0;
		};


		class FMP4BoxTreeParser
		{
		public:
			UE_API bool ParseBoxTree(const TSharedPtr<FMP4BoxInfo, ESPMode::ThreadSafe>& InRootBox);
			TSharedPtr<FMP4BoxBase> GetBoxTree() const
			{ return BoxTree; }
		private:
			UE_API bool ParseBoxTreeInternal(const TWeakPtr<FMP4BoxBase>& InParent, const FMP4BoxInfo& InBox);
			TSharedPtr<FMP4BoxBase> BoxTree;
		};

	} // namespace UtilitiesMP4


	class IFileDataReader : public IBaseDataReader
	{
	public:
		static ELECTRABASE_API TSharedPtr<IFileDataReader, ESPMode::ThreadSafe> Create();
		virtual ~IFileDataReader() = default;
		virtual bool Open(const FString& InFilename) = 0;
	};

} // namespace Electra

#undef UE_API

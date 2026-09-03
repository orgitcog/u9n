// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanZlib.h"
#include "Containers/Utf8String.h"

THIRD_PARTY_INCLUDES_START
#include "ThirdParty/zlib/1.3/include/zlib.h"
THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(LogMetaHumanZlib, Log, All);
DEFINE_LOG_CATEGORY(LogMetaHumanZlib);

namespace UE::MetaHuman
{
	// 256K is a good size if you can afford it, according to the zlib documentation
	constexpr int32 TempBufferMaxSize = 256 * 1024;

	bool FZlib::Deflate(TArray<uint8>& OutDeflated, TConstArrayView<uint8> InRaw, Format DeflatedFormat)
	{
		TArray<uint8> TempBuffer;
		const int32 TempBufferSize = FMath::Min(TempBufferMaxSize, InRaw.Num());
		TempBuffer.SetNumUninitialized(TempBufferSize);
		OutDeflated.Empty(TempBufferSize);

		z_stream ZStream;
		FMemory::Memset(&ZStream, 0, sizeof(z_stream));
		ZStream.next_in = const_cast<uint8*>(InRaw.GetData());
		ZStream.avail_in = InRaw.Num();
		ZStream.next_out = TempBuffer.GetData();
		ZStream.avail_out = TempBufferSize;

		switch (DeflatedFormat)
		{
		case Format::WithZlibHeader:
		{
			// default settings and with headers/footers for identification in streams etc
			deflateInit(&ZStream, Z_DEFAULT_COMPRESSION);
		}
		break;
		case Format::Raw:
		{
			// Raw deflate
			constexpr int Level = 8; //< default compression
			constexpr int WindowBits = -15; //< no zlib header or footer generated
			constexpr int MemLevel = 8; //< default speed vs memory usage
			deflateInit2(&ZStream, Level, Z_DEFLATED, WindowBits, MemLevel, Z_DEFAULT_STRATEGY);
		}
		break;
		default:;
		}

		const auto Flush = [&ZStream, &OutDeflated, &TempBuffer, TempBufferSize]()
			{
				if (ZStream.avail_out == 0)
				{
					// flush
					OutDeflated.Append(TempBuffer.GetData(), TempBufferSize);
					ZStream.next_out = TempBuffer.GetData();
					ZStream.avail_out = TempBufferSize;
				}
			};

		while (ZStream.avail_in != 0)
		{
			int32 Result = deflate(&ZStream, Z_NO_FLUSH);
			if (Result != Z_OK)
			{
				UE_LOG(LogMetaHumanZlib, Warning, TEXT("Deflate failed"));
				return false;
			}
			Flush();
		}
		// complete the deflation (Z_FINISH) and output
		int32 Result = Z_OK;
		do
		{
			Flush();
		} while ((Result = deflate(&ZStream, Z_FINISH)) == Z_OK);

		if (Result == Z_STREAM_END)
		{
			// remainder
			OutDeflated.Append(TempBuffer.GetData(), TempBufferSize - ZStream.avail_out);
			Result = deflateEnd(&ZStream);
		}

		return Result == Z_OK;
	}

	bool FZlib::Inflate(TArray<uint8>& OutInflated, TConstArrayView<uint8> InDeflated, Format DeflatedFormat)
	{
		TArray<uint8> TempBuffer;
		int32 TempBufferSize = FMath::Min(TempBufferMaxSize, InDeflated.Num());
		TempBuffer.SetNumUninitialized(TempBufferSize);

		z_stream ZStream;
		FMemory::Memset(&ZStream, 0, sizeof(z_stream));
		switch (DeflatedFormat)
		{
		case Format::Raw:
		{
			// Raw deflate, no headers/footers
			constexpr int WindowBits = -15;
			inflateInit2(&ZStream, WindowBits);
		}
		break;
		case Format::WithZlibHeader:
		{
			// expects zlib headers/footers
			inflateInit(&ZStream);
		}
		break;
		default:;
		}

		int32 DeflatedOffset = 0;

		int32 Result;
		do {
			const int32 AvailIn = FMath::Min(TempBufferSize, InDeflated.Num() - DeflatedOffset);
			ZStream.avail_in = AvailIn;
			ZStream.next_in = const_cast<uint8*>(InDeflated.GetData()) + DeflatedOffset;

			do {
				ZStream.next_out = TempBuffer.GetData();
				ZStream.avail_out = TempBufferSize;
				Result = inflate(&ZStream, Z_NO_FLUSH);
				switch (Result)
				{
				case Z_NEED_DICT:
					Result = Z_DATA_ERROR;
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&ZStream);
					UE_LOG(LogMetaHumanZlib, Error, TEXT("Failed to inflate stream"));
					return false;
				}
				OutInflated.Append(TempBuffer.GetData(), TempBufferSize - ZStream.avail_out);
			} while (ZStream.avail_out == 0);

			DeflatedOffset += AvailIn;

		} while (Result != Z_STREAM_END);

		(void)inflateEnd(&ZStream);
		return true;
	}
}

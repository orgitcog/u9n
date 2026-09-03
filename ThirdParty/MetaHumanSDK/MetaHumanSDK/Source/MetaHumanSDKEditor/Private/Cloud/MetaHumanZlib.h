// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

namespace UE::MetaHuman
{
	/*
	* Convenience wrapper around Engine/Thirdparty/zlib to provide standard deflate and inflate methods 
	*/
	struct FZlib
	{
		enum class Format
		{
			// simply deflate; use this when creating/reading ZIP archives for example
			Raw,
			// deflate and wrap with zlib header and trailer; use for deflated HTTP request content for example
			WithZlibHeader,
		};
		// Deflate the InRaw array using ZLib according to the given option
		static bool Deflate(TArray<uint8>& OutDeflated, TConstArrayView<uint8> InRaw, Format DeflatedFormat);
		// Inflate deflated data
		// NOTE: The option must match the option used to deflate the data originally
		static bool Inflate(TArray<uint8>& OutInflated, TConstArrayView<uint8> InDeflated, Format DeflatedFormat);
	};
}

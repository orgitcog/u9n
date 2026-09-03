// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Array.h"
#include "Serialization/Archive.h"
#include "Templates/SharedPointer.h"

namespace Chaos
{
	/** Wrapper around a data buffer used for serialization of chaos data */
	struct FSerializedDataBuffer
	{
		FSerializedDataBuffer(const TArray<uint8>& InByteArray) : DataBuffer(InByteArray)
		{	
		}

		FSerializedDataBuffer()
		{
		}

		TArray<uint8>& GetDataAsByteArrayRef()
		{
			return DataBuffer;
		}

		int32 GetSize() const
		{
			return DataBuffer.Num();
		}

		void Serialize(FArchive& Ar)
		{
			Ar << DataBuffer;
		}

	private:
		TArray<uint8> DataBuffer;
	};

	inline FArchive& operator<<(FArchive& Ar, FSerializedDataBuffer& Data)
	{
		Data.Serialize(Ar);
		return Ar;
	}
}

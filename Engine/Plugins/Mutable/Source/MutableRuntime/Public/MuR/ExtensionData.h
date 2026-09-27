// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/Types.h"

#define UE_API MUTABLERUNTIME_API

namespace mu
{
	/** ExtensionData represents data types that Mutable doesn't support natively.
	* Extensions can provide data, and functionality to operate on that data, without Mutable
	* needing to know what the data refers to.
	*/
	class FExtensionData : public FResource
	{
	public:
		static UE_API void Serialise(const FExtensionData* Data, FOutputArchive& Archive);
		static UE_API TSharedPtr<FExtensionData> StaticUnserialise(FInputArchive& Archive);

		// Resource interface
		UE_API int32 GetDataSize() const override;

		//! A stable hash of the contents
		UE_API uint32 Hash() const;

		UE_API void Serialise(FOutputArchive& Archive) const;
		UE_API void Unserialise(FInputArchive& Archive);

		inline bool operator==(const FExtensionData& Other) const
		{
			const bool bResult =
				Other.Index == Index
				&& Other.Origin == Origin;

			return bResult;
		}

		enum class EOrigin : uint8
		{
			//! An invalid value used to indicate that this ExtensionData hasn't been initialized
			Invalid,

			//! This ExtensionData is a compile-time constant that's always loaded into memory
			ConstantAlwaysLoaded,

			//! This ExtensionData is a compile-time constant that's streamed in from disk when needed
			ConstantStreamed,

			//! This ExtensionData was generated at runtime
			Runtime
		};

		int16 Index = -1;

		EOrigin Origin = EOrigin::Invalid;
	};
}

#undef UE_API

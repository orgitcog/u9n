// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "IAudioProxyInitializer.h"
#include "Sound/SoundWave.h"
#include "DSP/InterpolatedLinearPitchShifter.h"

#define UE_API METASOUNDENGINE_API


namespace Metasound
{
	// Forward declare ReadRef
	class FWaveAsset;
	typedef TDataReadReference<FWaveAsset> FWaveAssetReadRef;

	// Helper utility to test if exact types are required for a datatype.
	template <>
	struct TIsExplicit<FWaveAsset>
	{
		static constexpr bool Value = true;
	};

	// Metasound data type that holds onto a weak ptr. Mostly used as a placeholder until we have a proper proxy type.
	class FWaveAsset
	{
		FSoundWaveProxyPtr SoundWaveProxy;
	public:

		FWaveAsset() = default;
		FWaveAsset(const FWaveAsset&) = default;
		FWaveAsset& operator=(const FWaveAsset& Other) = default;

		UE_API FWaveAsset(const TSharedPtr<Audio::IProxyData>& InInitData);

		UE_API bool IsSoundWaveValid() const;

		const FSoundWaveProxyPtr& GetSoundWaveProxy() const
		{
			return SoundWaveProxy;
		}

		const FSoundWaveProxy* operator->() const
		{
			return SoundWaveProxy.Get();
		}

		FSoundWaveProxy* operator->()
		{
			return SoundWaveProxy.Get();
		}

		friend FORCEINLINE uint32 GetTypeHash(const Metasound::FWaveAsset& InWaveAsset)
		{
			if (InWaveAsset.IsSoundWaveValid())
			{
				return GetTypeHash(*InWaveAsset.GetSoundWaveProxy());
			}
			return INDEX_NONE;
		}
	};

	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FWaveAsset, METASOUNDENGINE_API, FWaveAssetTypeInfo, FWaveAssetReadRef, FWaveAssetWriteRef)
}
 

#undef UE_API

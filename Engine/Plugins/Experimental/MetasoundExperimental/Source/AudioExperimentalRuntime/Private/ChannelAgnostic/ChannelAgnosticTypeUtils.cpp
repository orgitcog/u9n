// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChannelAgnostic/ChannelAgnosticTypeUtils.h"
#include "ChannelAgnostic/ChannelAgnosticType.h"

namespace Audio
{
	// TODO: MOVE to DSP.
	static void Interleave(const TArrayView<const float> InMultiMono, const int32 InNumChannels, const TArrayView<float> OutInterleaved)
	{
		checkSlow(InNumChannels * InMultiMono.Num() <= OutInterleaved.Num());

		float* Dst = OutInterleaved.GetData();
		const float* Src = InMultiMono.GetData();
		const int32 NumFrames = InMultiMono.Num() / InNumChannels;
					
		for(int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			for (int32 Channel = 0; Channel < InNumChannels; ++Channel)
			{
				*Dst++ = Src[NumFrames * Channel + Frame];
			}
		}
	}

	void FCatUtils::Interleave(const FChannelAgnosticType& In, const TArrayView<float> Out)
	{
		Audio::Interleave(In.Buffer.GetView(), In.NumChannels(), Out);
	}

}
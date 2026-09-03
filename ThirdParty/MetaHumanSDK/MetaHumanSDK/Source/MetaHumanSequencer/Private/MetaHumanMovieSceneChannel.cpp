// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMovieSceneChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Misc/FrameRate.h"


// This function is called when new key is created via sequencer. We want the value of bool to be false, unless there's one already created at that frame
bool FMetaHumanMovieSceneChannel::Evaluate(FFrameTime InTime, bool& OutValue) const
{
	if (Times.Num() && Times.Contains(InTime.FrameNumber))
	{
		const int32 Index = FMath::Max(0, Algo::UpperBound(Times, InTime.FrameNumber)-1);
		OutValue = Values[Index];
		return true;
	}
	return false;
}

void FMetaHumanMovieSceneChannel::GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
{
	GetData().GetKeys(WithinRange, OutKeyTimes, OutKeyHandles);
}

void FMetaHumanMovieSceneChannel::GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
{
	GetData().GetKeyTimes(InHandles, OutKeyTimes);
}

void FMetaHumanMovieSceneChannel::SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
{
	GetData().SetKeyTimes(InHandles, InKeyTimes);
}

void FMetaHumanMovieSceneChannel::DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
{
	GetData().DuplicateKeys(InHandles, OutNewHandles);
}

void FMetaHumanMovieSceneChannel::DeleteKeys(TArrayView<const FKeyHandle> InHandles)
{
	GetData().DeleteKeys(InHandles);
}

void FMetaHumanMovieSceneChannel::DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore)
{
	// Insert a key at the current time to maintain evaluation
	if (GetData().GetTimes().Num() > 0)
	{
		bool Value = false;
		if (Evaluate(InTime, Value))
		{
			GetData().UpdateOrAddKey(InTime, Value);
		}
	}

	GetData().DeleteKeysFrom(InTime, bDeleteKeysBefore);
}

void FMetaHumanMovieSceneChannel::RemapTimes(const UE::MovieScene::IRetimingInterface& Retimer)
{
	GetData().RemapTimes(Retimer);
}

TRange<FFrameNumber> FMetaHumanMovieSceneChannel::ComputeEffectiveRange() const
{
	return GetData().GetTotalRange();
}

int32 FMetaHumanMovieSceneChannel::GetNumKeys() const
{
	return Times.Num();
}

void FMetaHumanMovieSceneChannel::Reset()
{
	Times.Reset();
	Values.Reset();
	KeyHandles.Reset();
	bHasDefaultValue = false;
}

void FMetaHumanMovieSceneChannel::Optimize(const FKeyDataOptimizationParams& InParameters)
{
	UE::MovieScene::Optimize(this, InParameters);
}

void FMetaHumanMovieSceneChannel::Offset(FFrameNumber DeltaPosition)
{
	GetData().Offset(DeltaPosition);
}

void FMetaHumanMovieSceneChannel::ClearDefault()
{
	bHasDefaultValue = false;
}

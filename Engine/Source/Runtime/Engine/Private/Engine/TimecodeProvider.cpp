// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/TimecodeProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TimecodeProvider)


FQualifiedFrameTime UTimecodeProvider::GetDelayedQualifiedFrameTime() const
{
	FQualifiedFrameTime NewFrameTime = GetQualifiedFrameTime();
	NewFrameTime.Time -= FFrameTime::FromDecimal(FrameDelay);
	return NewFrameTime;
}


FTimecode UTimecodeProvider::GetTimecode() const
{
	return GetQualifiedFrameTime().ToTimecode();
}


FTimecode UTimecodeProvider::GetDelayedTimecode() const
{
	return GetDelayedQualifiedFrameTime().ToTimecode();
}


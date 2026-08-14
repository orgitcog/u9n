// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// ELocalAxesMode

namespace ELocalAxesMode
{
enum Type
{
	None,
	Selected,
	All,
	NumAxesModes
};
}; // namespace ELocalAxesMode

//////////////////////////////////////////////////////////////////////////
// ELocalAxesMode

namespace EDisplayInfoMode
{
enum Type
{
	None,
	Basic,
	Detailed,
	SkeletalControls,
	NumInfoModes
};
}; // namespace EDisplayInfoMode

namespace EAnimationPlaybackSpeeds
{
enum Type
{
	OneTenth = 0,
	Quarter,
	Half,
	ThreeQuarters,
	Normal,
	Double,
	FiveTimes,
	TenTimes,
	Custom,
	NumPlaybackSpeeds
};

extern float Values[NumPlaybackSpeeds];
}; // namespace EAnimationPlaybackSpeeds

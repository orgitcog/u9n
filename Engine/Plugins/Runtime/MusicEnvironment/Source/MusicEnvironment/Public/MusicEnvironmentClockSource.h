// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FrameBasedMusicMap.h"
#include "Misc/MusicalTime.h"
#include "UObject/Interface.h"

#include "MusicEnvironmentClockSource.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UMusicEnvironmentClockSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IMusicEnvironmentClockSource
{
	GENERATED_BODY()

public:
	virtual FMusicalTime GetMusicalTime() const = 0;
	virtual int32 GetAbsoluteTickPosition() const = 0;
	virtual FMusicalTime GetMusicalTimeWithSourceSpaceOffset(const FMusicalTime& Offset) const = 0;
	virtual int32 GetAbsoluteTickPositionWithSourceSpaceOffset(const FMusicalTime& SourceOffset) const = 0;
	virtual FMusicalTime Quantize(const FMusicalTime& MusicalTime, int32 QuantizationInterval, UFrameBasedMusicMap::EQuantizeDirection Direction = UFrameBasedMusicMap::EQuantizeDirection::Nearest) const = 0;
	virtual bool CanAuditionInEditor() const = 0;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "SequencerToolMenuContext.generated.h"

class ISequencer;
class SSequencerPlayRateCombo;

UCLASS(MinimalAPI)
class USequencerToolMenuContext : public UObject
{
	GENERATED_BODY()
public:
	TWeakPtr<ISequencer> WeakSequencer;
};

UCLASS(MinimalAPI)
class USequencerClockSourceMenuContext : public USequencerToolMenuContext
{
	GENERATED_BODY()
public:
	TWeakPtr<SSequencerPlayRateCombo> WeakSequencerCombo;
};

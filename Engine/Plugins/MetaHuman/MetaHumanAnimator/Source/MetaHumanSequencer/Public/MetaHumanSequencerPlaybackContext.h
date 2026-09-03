// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class METAHUMANSEQUENCER_API FMetaHumanSequencerPlaybackContext
	: public TSharedFromThis<FMetaHumanSequencerPlaybackContext>
{
public:
	UObject* GetPlaybackContext() const;

private:
	class UWorld* ComputePlaybackContext() const;

	mutable TWeakObjectPtr<class UWorld> WeakCurrentContext;
};

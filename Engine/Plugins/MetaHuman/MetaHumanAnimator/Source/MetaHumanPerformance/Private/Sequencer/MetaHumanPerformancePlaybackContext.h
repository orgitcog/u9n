// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class FMetaHumanPerformancePlaybackContext
	: public TSharedFromThis<FMetaHumanPerformancePlaybackContext>
{
public:
	UObject* GetPlaybackContext() const;

private:
	class UWorld* ComputePlaybackContext() const;

	mutable TWeakObjectPtr<class UWorld> WeakCurrentContext;
};

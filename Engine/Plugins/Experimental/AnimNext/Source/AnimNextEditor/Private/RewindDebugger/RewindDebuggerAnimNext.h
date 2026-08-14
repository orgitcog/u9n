// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebugger.h"
#include "IRewindDebuggerExtension.h"

// Rewind debugger extension for Chooser support

class FRewindDebuggerAnimNext : public IRewindDebuggerExtension
{
public:
	FRewindDebuggerAnimNext();
	virtual ~FRewindDebuggerAnimNext() {};

	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;
};

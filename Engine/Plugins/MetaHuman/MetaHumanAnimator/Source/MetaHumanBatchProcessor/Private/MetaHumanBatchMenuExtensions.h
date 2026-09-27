// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "AssetRegistry/AssetData.h"

class FMetaHumanBatchMenuExtensions
{
public:
	FMetaHumanBatchMenuExtensions();
	~FMetaHumanBatchMenuExtensions();

	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();

private:
	void AddMenuExtensions();
	void AddSoundWaveMenuExtensions();
	void AddPerformanceMenuExtensions();
};

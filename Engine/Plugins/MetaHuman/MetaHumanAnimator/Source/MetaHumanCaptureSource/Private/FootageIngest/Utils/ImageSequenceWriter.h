// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IMediaTextureSample;

class IImageSequenceWriter
{
public:
	virtual bool Open(const FString& InDirPath) = 0;
	virtual bool Append(IMediaTextureSample* InDirPath) = 0;
	virtual void Close() = 0;
	virtual ~IImageSequenceWriter() {}

	static TSharedPtr<IImageSequenceWriter, ESPMode::ThreadSafe> Create();
};

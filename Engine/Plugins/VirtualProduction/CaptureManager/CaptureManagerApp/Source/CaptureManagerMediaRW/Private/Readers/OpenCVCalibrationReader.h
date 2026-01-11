// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaReader.h"
#include "IMediaRWFactory.h"

#include "Dom/JsonObject.h"

// TODO: The format is specific to the HMC so it might make sense to call it differently
class FOpenCvCalibrationReaderHelpers
{
public:

	static void RegisterReaders(class FMediaRWManager* InManager);
};

class FOpenCvCalibrationReaderFactory : public ICalibrationReaderFactory
{
public:

	virtual TUniquePtr<ICalibrationReader> CreateCalibrationReader() override;
};

class FOpenCvCalibrationReader : public ICalibrationReader
{
public:

	virtual TOptional<FText> Open(const FString& InFileName) override;
	virtual TOptional<FText> Close() override;

	virtual TValueOrError<TUniquePtr<UE::CaptureManager::FMediaCalibrationSample>, FText> Next() override;

private:

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	std::atomic_int ArrayIndex = 0;
};
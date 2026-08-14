// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaReader.h"
#include "IMediaWriter.h"

class CAPTUREMANAGERMEDIARW_API IAudioReaderFactory
{
public:

	virtual ~IAudioReaderFactory() = default;

	virtual TUniquePtr<IAudioReader> CreateAudioReader() = 0;
};

class CAPTUREMANAGERMEDIARW_API IVideoReaderFactory
{
public:

	virtual ~IVideoReaderFactory() = default;

	virtual TUniquePtr<IVideoReader> CreateVideoReader() = 0;
};

class CAPTUREMANAGERMEDIARW_API ICalibrationReaderFactory
{
public:

	virtual ~ICalibrationReaderFactory() = default;

	virtual TUniquePtr<ICalibrationReader> CreateCalibrationReader() = 0;
};

class CAPTUREMANAGERMEDIARW_API IAudioWriterFactory
{
public:

	virtual ~IAudioWriterFactory() = default;

	virtual TUniquePtr<IAudioWriter> CreateAudioWriter() = 0;
};

class CAPTUREMANAGERMEDIARW_API IImageWriterFactory
{
public:

	virtual ~IImageWriterFactory() = default;

	virtual TUniquePtr<IImageWriter> CreateImageWriter() = 0;
};

class CAPTUREMANAGERMEDIARW_API ICalibrationWriterFactory
{
public:

	virtual ~ICalibrationWriterFactory() = default;

	virtual TUniquePtr<ICalibrationWriter> CreateCalibrationWriter() = 0;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaRWFactory.h"

#include "Containers/Map.h"

class CAPTUREMANAGERMEDIARW_API FMediaRWManager
{
public:

	FMediaRWManager();

	FMediaRWManager(const FMediaRWManager& InCopy) = delete;
	FMediaRWManager& operator=(const FMediaRWManager& InCopy) = delete;

	void RegisterAudioReader(const TArray<FString>& InFormats, TUniquePtr<IAudioReaderFactory> InReader);
	void RegisterVideoReader(const TArray<FString>& InFormats, TUniquePtr<IVideoReaderFactory> InReader);
	void RegisterCalibrationReader(const TArray<FString>& InFormats, TUniquePtr<ICalibrationReaderFactory> InReader);

	void RegisterAudioWriter(const TArray<FString>& InFormats, TUniquePtr<IAudioWriterFactory> InWriter);
	void RegisterImageWriter(const TArray<FString>& InFormats, TUniquePtr<IImageWriterFactory> InWriter);
	void RegisterCalibrationWriter(const TArray<FString>& InFormats, TUniquePtr<ICalibrationWriterFactory> InWriter);

	TUniquePtr<IAudioReader> CreateAudioReaderByFormat(const FString& InFormat, int32 InIndex = 0);
	TUniquePtr<IVideoReader> CreateVideoReaderByFormat(const FString& InFormat, int32 InIndex = 0);
	TUniquePtr<ICalibrationReader> CreateCalibrationReaderByFormat(const FString& InFormat, int32 InIndex = 0);

	TValueOrError<TUniquePtr<IAudioReader>, FText> CreateAudioReader(const FString& InPath, int32 InIndex = 0);
	TValueOrError<TUniquePtr<IVideoReader>, FText> CreateVideoReader(const FString& InPath, int32 InIndex = 0);
	TValueOrError<TUniquePtr<ICalibrationReader>, FText> CreateCalibrationReader(const FString& InPath, int32 InIndex = 0);

	TUniquePtr<IAudioWriter> CreateAudioWriterByFormat(const FString& InFormat, int32 InIndex = 0);
	TUniquePtr<IImageWriter> CreateImageWriterByFormat(const FString& InFormat, int32 InIndex = 0);
	TUniquePtr<ICalibrationWriter> CreateCalibrationWriterByFormat(const FString& InFormat, int32 InIndex = 0);

	TValueOrError<TUniquePtr<IAudioWriter>, FText> CreateAudioWriter(const FString& InDirectory, const FString& InFileName, const FString& InFormat, int32 InIndex = 0);
	TValueOrError<TUniquePtr<IImageWriter>, FText> CreateImageWriter(const FString& InDirectory, const FString& InFileName, const FString& InFormat, int32 InIndex = 0);
	TValueOrError<TUniquePtr<ICalibrationWriter>, FText> CreateCalibrationWriter(const FString& InDirectory, const FString& InFileName, const FString& InFormat, int32 InIndex = 0);

private:

	TMap<FString, IAudioReaderFactory*> AudioReadersPerFormat;
	TArray<TUniquePtr<IAudioReaderFactory>> AudioReaders;

	TMap<FString, IVideoReaderFactory*> VideoReadersPerFormat;
	TArray<TUniquePtr<IVideoReaderFactory>> VideoReaders;

	TMap<FString, ICalibrationReaderFactory*> CalibrationReadersPerFormat;
	TArray<TUniquePtr<ICalibrationReaderFactory>> CalibrationReaders;

	TMap<FString, IAudioWriterFactory*> AudioWritersPerFormat;
	TArray<TUniquePtr<IAudioWriterFactory>> AudioWriters;

	TMap<FString, IImageWriterFactory*> ImageWritersPerFormat;
	TArray<TUniquePtr<IImageWriterFactory>> ImageWriters;

	TMap<FString, ICalibrationWriterFactory*> CalibrationWritersPerFormat;
	TArray<TUniquePtr<ICalibrationWriterFactory>> CalibrationWriters;
};
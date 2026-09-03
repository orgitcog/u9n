// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerMediaRWModule.h"

#if PLATFORM_WINDOWS && !UE_SERVER

#include "Utils/WindowsRWHelpers.h"

#endif
#include "Readers/MHADepthVideoReader.h"
#include "Readers/MHAICalibrationReader.h"
#include "Readers/OpenCVCalibrationReader.h"

#include "Writers/AudioWaveMediaWriter.h"
#include "Writers/DepthImageWriter.h"
#include "Writers/UnrealCalibrationWriter.h"

void FCaptureManagerMediaRWModule::StartupModule()
{
	MediaRWManager = MakeUnique<FMediaRWManager>();

	// Readers
#if PLATFORM_WINDOWS && !UE_SERVER
	FWindowsRWHelpers::Init();
	FWindowsRWHelpers::RegisterReaders(MediaRWManager.Get());
	FWindowsRWHelpers::RegisterWriters(MediaRWManager.Get());
#endif

	FMHADepthVideoReaderHelpers::RegisterReaders(MediaRWManager.Get());
	FMHAICalibrationReaderHelpers::RegisterReaders(MediaRWManager.Get());
	FOpenCvCalibrationReaderHelpers::RegisterReaders(MediaRWManager.Get());

	// Writers
	FAudioWaveWriterHelpers::RegisterWriters(MediaRWManager.Get());
	FDepthExrImageWriterHelpers::RegisterWriters(MediaRWManager.Get());
	FUnrealCalibrationWriterHelpers::RegisterWriters(MediaRWManager.Get());
}

void FCaptureManagerMediaRWModule::ShutdownModule()
{
#if PLATFORM_WINDOWS && !UE_SERVER
	FWindowsRWHelpers::Deinit();
#endif

	MediaRWManager = nullptr;
}

FMediaRWManager& FCaptureManagerMediaRWModule::Get()
{
	return *MediaRWManager;
}

IMPLEMENT_MODULE(FCaptureManagerMediaRWModule, CaptureManagerMediaRW);
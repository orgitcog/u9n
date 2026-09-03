// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsRWHelpers.h"

#include "MediaRWManager.h"

#if PLATFORM_WINDOWS && !UE_SERVER

DEFINE_LOG_CATEGORY_STATIC(LogWindowsRWHelper, Log, All);

bool FWindowsRWHelpers::Init()
{
	HRESULT Result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	if (!SUCCEEDED(Result))
	{
		CoUninitialize();

		FString Message = FormatWindowsMessage(Result);
		UE_LOG(LogWindowsRWHelper, Error, TEXT("Failed to initialize Windows Media Foundation %s"), *Message);

		return false;
	}

	Result = MFStartup(MF_VERSION);

	if (!SUCCEEDED(Result))
	{
		CoUninitialize();

		FString Message = FormatWindowsMessage(Result);
		UE_LOG(LogWindowsRWHelper, Error, TEXT("Failed to start Windows Media Foundation %s"), *Message);

		return false;
	}

	return true;
}

void FWindowsRWHelpers::Deinit()
{
	MFShutdown();
	CoUninitialize();
}

void FWindowsRWHelpers::RegisterReaders(FMediaRWManager* InManager)
{
	TArray<FString> SupportedExtensions = { TEXT("mov"), TEXT("mp4") };
	InManager->RegisterAudioReader(SupportedExtensions, MakeUnique<FWindowsReadersFactory>());
	InManager->RegisterVideoReader(SupportedExtensions, MakeUnique<FWindowsReadersFactory>());
}

void FWindowsRWHelpers::RegisterWriters(FMediaRWManager* InManager)
{
	TArray<FString> SupportedExtensions = { TEXT("png"), TEXT("jpg"), TEXT("jpeg") };
	InManager->RegisterImageWriter(SupportedExtensions, MakeUnique<FWindowsImageWriterFactory>());
}

FText FWindowsRWHelpers::CreateErrorMessage(HRESULT InResult, FText InMessage)
{
	FText WindowsErrorMessage = FText::FromString(FWindowsRWHelpers::FormatWindowsMessage(InResult));
	FText ErrorMessage = FText::Format(FText::FromString(TEXT("{0}: {1}")), MoveTemp(InMessage), WindowsErrorMessage);

	return ErrorMessage;
}

FString FWindowsRWHelpers::FormatWindowsMessage(HRESULT InResult)
{
	constexpr uint32 BufSize = 1024;
	WIDECHAR Buffer[BufSize];

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, InResult, 0, Buffer, BufSize, nullptr);

	return Buffer;
}

#endif // PLATFORM_WINDOWS && !UE_SERVER
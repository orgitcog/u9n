// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Experimental/UnifiedError/UnifiedError.h"
#include "UObject/UObjectGlobals.h"


DECLARE_ERROR_MODULE(StreamableManager, 0x10);


DECLARE_ERROR_ONEPARAM(PackageLoadFailed, 1, StreamableManager, NSLOCTEXT("StreamableManager", "PackageLoadError", "Failed to load package {PackageName}"), FString, PackageName, TEXT("Unknown"));
DECLARE_ERROR_ONEPARAM(PackageLoadCanceled, 2, StreamableManager, NSLOCTEXT("StreamableManager", "PackageLoadCancelled", "Async load canceled {PackageName}"), FString, PackageName, TEXT("Unknown"));
DECLARE_ERROR(DownloadError, 3, StreamableManager, NSLOCTEXT("StreamableManager", "DownloadError", "Failed to download"));
DECLARE_ERROR_ONEPARAM(PackageNameInvalid, 4, StreamableManager, NSLOCTEXT("StreamableManager", "PackageNameInvalid", "Found invalid package name {InvalidPackageName}"), FString, InvalidPackageName, TEXT("Unknown"));

DECLARE_ERROR(IoStoreNotFound, 6, StreamableManager, NSLOCTEXT("StreamableManager", "IoStoreNotFound", "IoStore did not load correctly."));
DECLARE_ERROR_ONEPARAM(SyncLoadIncomplete, 7, StreamableManager, NSLOCTEXT("StreamableManager", "SyncLoadIncomplete", "Sync load did not complete correctly for {DebugName}."), FString, DebugName, TEXT("Unknown"));

DECLARE_ERROR(AsyncLoadFailed, 8, StreamableManager, NSLOCTEXT("StreamableManager", "AsyncLoadFailed", "Async load failed"));
DECLARE_ERROR(AsyncLoadCancelled, 9, StreamableManager, NSLOCTEXT("StreamableManager", "AsyncLoadCancelled", "Async load cancelled"));
DECLARE_ERROR_ONEPARAM(AsyncLoadUnknownError, 10, StreamableManager, NSLOCTEXT("StreamableManager", "AsyncLoadUnknownError", "Unknown async loading error {AsyncLoadingErrorId}."), int32, AsyncLoadingErrorId, -1);


DECLARE_ERROR(UnknownError, 11, StreamableManager, NSLOCTEXT("StreamableManager", "UnknownError", "Unknown error occured while streaming asset"));
DECLARE_ERROR(AsyncLoadNotInstalled, 12, StreamableManager, NSLOCTEXT("StreamableManager", "AsyncLoadNotInstalled", "Async load failed because the package is not installed."));

namespace UE::UnifiedError::StreamableManager
{
	class FStreamableManagerErrorDetails : public FDynamicErrorDetails
	{
		DECLARE_FERROR_DETAILS(StreamableManager, FStreamableManagerErrorDetails);
	private:
		FString SoftObjectPath;
		FText ErrorFormatString;
		FStreamableManagerErrorDetails() = default;
	public:

		FStreamableManagerErrorDetails(const FString& InSoftObjectPath) 
			: SoftObjectPath(InSoftObjectPath)
			, ErrorFormatString(NSLOCTEXT("StreamableManager", "StreamableManagerErrorDetails", "{InnerErrorMessage} Target Path: {SoftObjectPath}"))
		{
		}

		virtual void GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const override
		{
			OutProperties.AddProperty(UTF8TEXTVIEW("SoftObjectPath"), SoftObjectPath);
			FFormatNamedArguments ErrorMessageArgs;
			UE::UnifiedError::FTextFormatArgsPropertyExtractor ExtractToFTextArgs(ErrorMessageArgs);
			GetInnerErrorDetails()->GetErrorProperties(Error, ExtractToFTextArgs);
			const FText& FormatString = GetInnerErrorDetails()->GetErrorFormatString(Error);
			FText Result =FText::Format(FormatString, ErrorMessageArgs);
			OutProperties.AddProperty(UTF8TEXTVIEW("InnerErrorMessage"), Result);
			FDynamicErrorDetails::GetErrorProperties(Error, OutProperties);
		}

		virtual const FText GetErrorFormatString(const FError& Error) const override
		{
			const FText InnerFormatString = GetInnerErrorDetails()->GetErrorFormatString(Error);

			FText MyFormatString = NSLOCTEXT("StreamableManager", "DetailsFormatString", "(SoftObjectPath:{SoftObjectPath})");

			return FText::Join(NSLOCTEXT("StreamableManager", "DetailsFormatStringDelimiter", " "), InnerFormatString, MyFormatString);
		}

		const FString& GetSoftObjectPath() const { return SoftObjectPath; }

		
	};
	UE::UnifiedError::FError GetStreamableError(EAsyncLoadingResult::Type Result);



	class FStreamableManagerAdditionalContext
	{
	public:
		/*friend void SerializeForLog(FCbWriter& Writer, const UE::UnifiedError::StreamableManager::FStreamableManagerAdditionalContext& Context);
	private:*/
		FString ExtraSoftObjectPath;
	};


	inline void GatherPropertiesForError(const UE::UnifiedError::FError& Error, const UE::UnifiedError::StreamableManager::FStreamableManagerAdditionalContext& Context, UE::UnifiedError::IErrorPropertyExtractor& PropertyExtractor)
	{
		PropertyExtractor.AddProperty(ANSITEXTVIEW("ExtraSoftObjectPath"), Context.ExtraSoftObjectPath);
	}
	
}

inline void SerializeForLog(FCbWriter& Writer, const UE::UnifiedError::StreamableManager::FStreamableManagerAdditionalContext& Context)
{
	Writer.BeginObject();
	Writer.AddString(ANSITEXTVIEW("$type"), ANSITEXTVIEW("FStreamableManagerAdditionalContext"));
	FStringFormatNamedArguments NamedArguments;
	NamedArguments.Add(TTuple<FString, FStringFormatArg>(TEXT("SoftObjectPath"), Context.ExtraSoftObjectPath));
	Writer.AddString(ANSITEXTVIEW("$text"), FString::Format(TEXT("(SoftObjectPath: {SoftObjectPath})"), NamedArguments));
	Writer.AddString(ANSITEXTVIEW("SoftObjectPath"), Context.ExtraSoftObjectPath);
	Writer.EndObject();
}
DECLARE_ERRORSTRUCT_FEATURES(StreamableManager::FStreamableManagerAdditionalContext);


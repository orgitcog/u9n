// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackingPathUtils.h"

#include "ImageSequenceUtils.h"
#include "ImgMediaSource.h"
#include "Internationalization/Regex.h"

bool FTrackingPathUtils::GetTrackingFilePathAndInfo(const class UImgMediaSource* InImgSequence, FString& OutTrackingFilePath, int32& OutFrameOffset, int32& OutNumFrames)
{
	return GetTrackingFilePathAndInfo(InImgSequence->GetFullPath(), OutTrackingFilePath, OutFrameOffset, OutNumFrames);
}

bool FTrackingPathUtils::GetTrackingFilePathAndInfo(const FString& InFullSequencePath, FString& OutTrackingFilePath, int32& OutFrameOffset, int32& OutNumFrames)
{
	TArray<FString> ImageFiles;
	bool bFoundImages = FImageSequenceUtils::GetImageSequenceFilesFromPath(InFullSequencePath, ImageFiles);

	if (ImageFiles.Num() == 0)
	{
		return false;
	}

	ImageFiles.Sort();

	// find an image filename which can be some optional alphabetic or underscore/space/hyphen characters followed by some digits 
	// followed by some optional alphabetic or underscore/space/hyphen characters with any extension
	const FRegexPattern ImageFilenamePattern(TEXT("^(([a-zA-Z0-9]*[_\\s-])*)([0-9]+)[a-zA-Z_\\s-]*\\.[a-zA-Z]+$"));
	FRegexMatcher ImageFilenameMatcher(ImageFilenamePattern, ImageFiles[0]);

	if (ImageFilenameMatcher.FindNext())
	{
		const FString Digits = ImageFilenameMatcher.GetCaptureGroup(3);
		const int32 DigitsStart = ImageFilenameMatcher.GetCaptureGroupBeginning(3);
		OutFrameOffset = FCString::Atoi(*Digits);
		const FString InitialChars = ImageFiles[0].Left(DigitsStart);
		const FString EndChars = ImageFiles[0].Right(ImageFiles[0].Len() - Digits.Len() - DigitsStart);
		FString DigitsSpecifier = TEXT("%0") + FString::FromInt(Digits.Len()) + TEXT("d");
		OutTrackingFilePath = InFullSequencePath / InitialChars + DigitsSpecifier + EndChars;
		OutNumFrames = ImageFiles.Num();
		return true;
	}

	return false;
}

FString FTrackingPathUtils::ExpandFilePathFormat(const FString& InFilePathFormat, int32 InFrameNumber)
{
	// The function will expand a limited set of format specifiers, specifically %d or %i with
	// optional 0 character padding, eg %i, %04i, %d, %5d. The function used to use sprintf
	// but that caused cross platform compile issues. While only a very limited set of sprintf
	// format specifiers are supported, they are whats needed for the use case here of
	// specifying an image sequence.

	const int32 Percent = InFilePathFormat.Find("%");
	if (Percent == INDEX_NONE)
	{
		return InFilePathFormat;
	}

	int32 Decimal = 0;
	const int32 DecimalD = InFilePathFormat.Find("d", ESearchCase::IgnoreCase, ESearchDir::FromStart, Percent);
	const int32 DecimalI = InFilePathFormat.Find("i", ESearchCase::IgnoreCase, ESearchDir::FromStart, Percent);

	if (DecimalD == INDEX_NONE && DecimalI == INDEX_NONE)
	{
		return InFilePathFormat;
	}
	else if (DecimalD == INDEX_NONE)
	{
		Decimal = DecimalI;
	}
	else if (DecimalI == INDEX_NONE)
	{
		Decimal = DecimalD;
	}
	else
	{
		Decimal = FMath::Min(DecimalD, DecimalI);
	}

	int32 Padding = 0;
	if (Decimal - Percent > 1)
	{
		const FString PaddingString = InFilePathFormat.Mid(Percent + 1, Decimal - (Percent + 1));

		if (!PaddingString.IsNumeric())
		{
			return InFilePathFormat;
		}

		Padding = FCString::Atoi(*PaddingString);

		if (Padding < 0 || Padding > 50) // apply sensible limits for padding
		{
			return InFilePathFormat;
		}
	}

	FString FrameNumber = FString::Printf(TEXT("%d"), InFrameNumber);
	while (FrameNumber.Len() < Padding)
	{
		FrameNumber = TEXT("0") + FrameNumber;
	}

	FString Filename = InFilePathFormat.Left(Percent);
	Filename += FrameNumber;
	Filename += InFilePathFormat.Mid(Decimal + 1);

	return Filename;
}

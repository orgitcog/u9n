// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFacePacket.h"

bool FLiveLinkFacePacket::Read(const FArrayReaderPtr& InPayloadPtr)
{
	FArrayReader& Payload = *InPayloadPtr;
	
	Payload << Version;

	// Subject ID
	uint16 SubjectIdLength;
	Payload << SubjectIdLength;
	SubjectId = ReadString(Payload, SubjectIdLength);

	// Qualified Frame Time (Time code)
	QualifiedFrameTime = ReadQualifiedFrameTime(Payload);

	// Control Values
	ReadControlValues(Payload, ControlValues);

	// Head Pose
	ReadHeadPose(Payload, HeadPose);
	
	return !Payload.IsError();
}

uint16 FLiveLinkFacePacket::GetVersion() const
{
	return Version;
}

FString FLiveLinkFacePacket::GetSubjectId() const
{
	return SubjectId;
}

FQualifiedFrameTime FLiveLinkFacePacket::GetQualifiedFrameTime() const
{
	return QualifiedFrameTime;
}

const TArray<uint16>& FLiveLinkFacePacket::GetControlValues() const
{
	return ControlValues;
}

const TArray<float>& FLiveLinkFacePacket::GetHeadPose() const
{
	return HeadPose;
}

TArray<uint8> FLiveLinkFacePacket::ReadData(FArrayReader& InArrayReader, const uint32 InLen)
{
	TArray<uint8> DataArray;
	DataArray.SetNumUninitialized(InLen);
	InArrayReader.Serialize(DataArray.GetData(), InLen);
	return DataArray;
}

FString FLiveLinkFacePacket::ReadString(FArrayReader& InArrayReader, const uint32 InLen)
{
	TArray<uint8> Data = ReadData(InArrayReader, InLen);
	return FString(StringCast<TCHAR>((const UTF8CHAR*)Data.GetData(), Data.Num()));
}

FName FLiveLinkFacePacket::ReadName(FArrayReader& InArrayReader, const uint32 InLen)
{
	FString String = ReadString(InArrayReader, InLen);
	return FName(String);
}

FQualifiedFrameTime FLiveLinkFacePacket::ReadQualifiedFrameTime(FArrayReader& InArrayReader)
{
	int32 FrameNumber;
	float SubFrame;
	int32 FrameRateNumerator;
	int32 FrameRateDenominator;

	InArrayReader << FrameNumber;
	InArrayReader << SubFrame;
	InArrayReader << FrameRateNumerator;
	InArrayReader << FrameRateDenominator;

	const FFrameTime FrameTime(FrameNumber, SubFrame);
	const FFrameRate FrameRate(FrameRateNumerator, FrameRateDenominator);
	return FQualifiedFrameTime(FrameTime, FrameRate);
}

void FLiveLinkFacePacket::ReadControlValues(FArrayReader& InArrayReader, TArray<uint16>& OutControlValues)
{
	uint16 ControlValuesCount;
	InArrayReader << ControlValuesCount;

	OutControlValues.SetNumUninitialized(ControlValuesCount);
	for (uint16 ControlValueIndex = 0; ControlValueIndex < ControlValuesCount; ++ControlValueIndex)
	{
		InArrayReader << OutControlValues[ControlValueIndex];
	}
}

void FLiveLinkFacePacket::ReadHeadPose(FArrayReader& InArrayReader, TArray<float>& OutHeadPose)
{
	OutHeadPose.SetNumUninitialized(HeadPoseValueCount);
	for (int32 Index = 0; Index < HeadPoseValueCount; ++Index)
	{
		InArrayReader << OutHeadPose[Index];
	}
}

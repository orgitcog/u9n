// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/UdpSocketReceiver.h"
#include "Misc/QualifiedFrameTime.h"

class FLiveLinkFacePacket
{
public:
	static constexpr int32 HeadPoseValueCount = 6;
	
	bool Read(const FArrayReaderPtr& InPayloadPtr);

	uint16 GetVersion() const;
	FString GetSubjectId() const;
	FQualifiedFrameTime GetQualifiedFrameTime() const;
	const TArray<uint16>& GetControlValues() const;
	const TArray<float>& GetHeadPose() const;
	
private:

	uint16 Version = 0;
	FString SubjectId;
	FQualifiedFrameTime QualifiedFrameTime;
	TArray<uint16> ControlValues;
	TArray<float> HeadPose;
	
	TArray<uint8> ReadData(FArrayReader& InArrayReader, const uint32 InLen);
	FString ReadString(FArrayReader& InArrayReader, const uint32 InLen);
	FName ReadName(FArrayReader& InArrayReader, const uint32 InLen);
	FQualifiedFrameTime ReadQualifiedFrameTime(FArrayReader& InArrayReader);
	void ReadControlValues(FArrayReader& InArrayReader, TArray<uint16>& OutControlValues);
	void ReadHeadPose(FArrayReader& InArrayReader, TArray<float>& OutHeadPose);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlResponse.h"
#include "Control/Messages/Constants.h"

#include "Control/Messages/ControlJsonUtilities.h"

namespace UE::CaptureManager
{

FControlResponse::FControlResponse(FString InAddressPath)
	: AddressPath(MoveTemp(InAddressPath))
{
}

TProtocolResult<void> FControlResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	if (InBody.IsValid() && !InBody->Values.IsEmpty())
	{
		return FCaptureProtocolError("Response must NOT have a body");
	}

	return ResultOk;
}

const FString& FControlResponse::GetAddressPath() const
{
	return AddressPath;
}

FKeepAliveResponse::FKeepAliveResponse()
	: FControlResponse(CPS::AddressPaths::GKeepAlive)
{
}

FStartSessionResponse::FStartSessionResponse()
	: FControlResponse(CPS::AddressPaths::GStartSession)
{
}

TProtocolResult<void> FStartSessionResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GSessionId, SessionId));

	return ResultOk;
}

const FString& FStartSessionResponse::GetSessionId() const
{
	return SessionId;
}

FStopSessionResponse::FStopSessionResponse()
	: FControlResponse(CPS::AddressPaths::GStopSession)
{
}

FGetServerInformationResponse::FGetServerInformationResponse()
	: FControlResponse(CPS::AddressPaths::GGetServerInformation)
{
}

TProtocolResult<void> FGetServerInformationResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GId, Id));
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GName, Name));
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GModel, Model));
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GPlatformName, PlatformName));
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GPlatformVersion, PlatformVersion));
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GSoftwareName, SoftwareName));
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GSoftwareVersion, SoftwareVersion));
	CHECK_PARSE(FJsonUtility::ParseNumber(InBody, CPS::Properties::GExportPort, ExportPort));

	return ResultOk;
}

const FString& FGetServerInformationResponse::GetId() const
{
	return Id;
}

const FString& FGetServerInformationResponse::GetName() const
{
	return Name;
}

const FString& FGetServerInformationResponse::GetModel() const
{
	return Model;
}

const FString& FGetServerInformationResponse::GetPlatformName() const
{
	return PlatformName;
}

const FString& FGetServerInformationResponse::GetPlatformVersion() const
{
	return PlatformVersion;
}

const FString& FGetServerInformationResponse::GetSoftwareName() const
{
	return SoftwareName;
}

const FString& FGetServerInformationResponse::GetSoftwareVersion() const
{
	return SoftwareVersion;
}

uint16 FGetServerInformationResponse::GetExportPort() const
{
	return ExportPort;
}

FSubscribeResponse::FSubscribeResponse()
	: FControlResponse(CPS::AddressPaths::GSubscribe)
{
}

FUnsubscribeResponse::FUnsubscribeResponse()
	: FControlResponse(CPS::AddressPaths::GUnsubscribe)
{
}

FGetStateResponse::FGetStateResponse()
	: FControlResponse(CPS::AddressPaths::GGetState)
{
}

TProtocolResult<void> FGetStateResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	CHECK_PARSE(FJsonUtility::ParseBool(InBody, CPS::Properties::GIsRecording, bIsRecording));

	// Optional
	const TSharedPtr<FJsonObject>* PlatformStatePtr;
	TProtocolResult<void> Result = FJsonUtility::ParseObject(InBody, CPS::Properties::GPlatformState, PlatformStatePtr);
	if (Result.HasValue())
	{
		PlatformState = *PlatformStatePtr;
	}

	return ResultOk;
}

bool FGetStateResponse::IsRecording() const
{
	return bIsRecording;
}

const TSharedPtr<FJsonObject>& FGetStateResponse::GetPlatformState() const
{
	return PlatformState;
}

FStartRecordingTakeResponse::FStartRecordingTakeResponse()
	: FControlResponse(CPS::AddressPaths::GStartRecordingTake)
{
}

FStopRecordingTakeResponse::FStopRecordingTakeResponse()
	: FControlResponse(CPS::AddressPaths::GStopRecordingTake)
{
}

FAbortRecordingTakeResponse::FAbortRecordingTakeResponse()
	: FControlResponse(CPS::AddressPaths::GAbortRecordingTake)
{
}

TProtocolResult<void> FStopRecordingTakeResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	CHECK_PARSE(FJsonUtility::ParseString(InBody, CPS::Properties::GName, TakeName));

	return ResultOk;
}

const FString& FStopRecordingTakeResponse::GetTakeName() const
{
	return TakeName;
}

FGetTakeListResponse::FGetTakeListResponse()
	: FControlResponse(CPS::AddressPaths::GGetTakeList)
{
}

TProtocolResult<void> FGetTakeListResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	const TArray<TSharedPtr<FJsonValue>>* NamesJson;
	CHECK_PARSE(FJsonUtility::ParseArray(InBody, CPS::Properties::GNames, NamesJson));

	for (const TSharedPtr<FJsonValue>& NameJson : *NamesJson)
	{
		Names.Add(NameJson->AsString());
	}

	return ResultOk;
}

const TArray<FString>& FGetTakeListResponse::GetNames() const
{
	return Names;
}

FGetTakeMetadataResponse::FGetTakeMetadataResponse()
	: FControlResponse(CPS::AddressPaths::GGetTakeMetadata)
{
}

TProtocolResult<void> FGetTakeMetadataResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	const TArray<TSharedPtr<FJsonValue>>* TakesJson;
	CHECK_PARSE(FJsonUtility::ParseArray(InBody, CPS::Properties::GTakes, TakesJson));

	for (const TSharedPtr<FJsonValue>& TakeJson : *TakesJson)
	{
		const TSharedPtr<FJsonObject>& TakeJsonObject = TakeJson->AsObject();

		FTakeObject Take;
		CHECK_PARSE(CreateTakeObject(TakeJsonObject, Take));
		Takes.Add(MoveTemp(Take));
	}

	return ResultOk;
}

const TArray<FGetTakeMetadataResponse::FTakeObject>& FGetTakeMetadataResponse::GetTakes() const
{
	return Takes;
}

TProtocolResult<void> FGetTakeMetadataResponse::CreateTakeObject(const TSharedPtr<FJsonObject>& InTakeObject, FTakeObject& OutTake) const
{
	CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, CPS::Properties::GName, OutTake.Name));
	CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, CPS::Properties::GSlateName, OutTake.Slate));
	CHECK_PARSE(FJsonUtility::ParseNumber(InTakeObject, CPS::Properties::GTakeNumber, OutTake.TakeNumber));
	CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, CPS::Properties::GDateTime, OutTake.DateTime));
	CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, CPS::Properties::GAppVersion, OutTake.AppVersion));
	CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, CPS::Properties::GModel, OutTake.Model));

	// Optional
	FJsonUtility::ParseString(InTakeObject, CPS::Properties::GSubject, OutTake.Subject);
	FJsonUtility::ParseString(InTakeObject, CPS::Properties::GScenario, OutTake.Scenario);

	const TArray<TSharedPtr<FJsonValue>>* TagsJson;
	TProtocolResult<void> ParseArrayResult = FJsonUtility::ParseArray(InTakeObject, CPS::Properties::GTags, TagsJson);

	if (ParseArrayResult.HasValue())
	{
		for (const TSharedPtr<FJsonValue>& TagJson : *TagsJson)
		{
			OutTake.Tags.Add(TagJson->AsString());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* FilesJson;
	CHECK_PARSE(FJsonUtility::ParseArray(InTakeObject, CPS::Properties::GFiles, FilesJson));

	for (const TSharedPtr<FJsonValue>& FileJson : *FilesJson)
	{
		const TSharedPtr<FJsonObject>& FileJsonObject = FileJson->AsObject();
		FFileObject File;
		CHECK_PARSE(CreateFileObject(FileJsonObject, File));
		OutTake.Files.Add(MoveTemp(File));
	}

	const TSharedPtr<FJsonObject>* VideoMetadata;
	TProtocolResult<void> ParseVideoObject = FJsonUtility::ParseObject(InTakeObject, CPS::Properties::GVideo, VideoMetadata);
	if (ParseVideoObject.HasValue())
	{
		CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, CPS::Properties::GFrames, OutTake.Video.Frames));
		CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, CPS::Properties::GFrameRate, OutTake.Video.FrameRate));
		CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, CPS::Properties::GHeight, OutTake.Video.Height));
		CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, CPS::Properties::GWidth, OutTake.Video.Width));
	}

	const TSharedPtr<FJsonObject>* AudioMetadata;
	TProtocolResult<void> ParseAudioObject = FJsonUtility::ParseObject(InTakeObject, CPS::Properties::GAudio, AudioMetadata);
	if (ParseAudioObject.HasValue())
	{
		CHECK_PARSE(FJsonUtility::ParseNumber(*AudioMetadata, CPS::Properties::GChannels, OutTake.Audio.Channels));
		CHECK_PARSE(FJsonUtility::ParseNumber(*AudioMetadata, CPS::Properties::GSampleRate, OutTake.Audio.SampleRate));
		CHECK_PARSE(FJsonUtility::ParseNumber(*AudioMetadata, CPS::Properties::GBitsPerChannel, OutTake.Audio.BitsPerChannel));
	}

	return ResultOk;
}

TProtocolResult<void> FGetTakeMetadataResponse::CreateFileObject(const TSharedPtr<FJsonObject>& InFileObject, FFileObject& OutFile) const
{
	CHECK_PARSE(FJsonUtility::ParseString(InFileObject, CPS::Properties::GName, OutFile.Name));
	CHECK_PARSE(FJsonUtility::ParseNumber(InFileObject, CPS::Properties::GLength, OutFile.Length));

	return ResultOk;
}

FGetStreamingSubjectsResponse::FGetStreamingSubjectsResponse()
	: FControlResponse(CPS::AddressPaths::GGetStreamingSubjects)
{
	
}

TProtocolResult<void> FGetStreamingSubjectsResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	const TArray<TSharedPtr<FJsonValue>>* SubjectsJson;
	CHECK_PARSE(FJsonUtility::ParseArray(InBody, CPS::Properties::GSubjects, SubjectsJson));

	for (const TSharedPtr<FJsonValue>& SubjectJson : *SubjectsJson)
	{
		const TSharedPtr<FJsonObject>& SubjectJsonObject = SubjectJson->AsObject();

		FSubject Subject;
		CHECK_PARSE(CreateSubject(SubjectJsonObject, Subject));
		Subjects.Add(MoveTemp(Subject));
	}

	return ResultOk;
}

const TArray<FGetStreamingSubjectsResponse::FSubject>& FGetStreamingSubjectsResponse::GetSubjects() const
{
	return Subjects;
}

TProtocolResult<void> FGetStreamingSubjectsResponse::CreateSubject(const TSharedPtr<FJsonObject>& InSubjectObject, FSubject& OutSubject) const
{
	CHECK_PARSE(FJsonUtility::ParseString(InSubjectObject, CPS::Properties::GId, OutSubject.Id))
	CHECK_PARSE(FJsonUtility::ParseString(InSubjectObject, CPS::Properties::GName, OutSubject.Name))

	const TSharedPtr<FJsonObject>* AnimationMetadataObject;
	CHECK_PARSE(FJsonUtility::ParseObject(InSubjectObject, CPS::Properties::GAnimationMetadata, AnimationMetadataObject));
	
	FAnimationMetadata AnimationMetadata;
	CHECK_PARSE(CreateAnimationMetadata(*AnimationMetadataObject, AnimationMetadata))
	OutSubject.AnimationMetadata = MoveTemp(AnimationMetadata);

	return ResultOk;
}

TProtocolResult<void> FGetStreamingSubjectsResponse::CreateAnimationMetadata(const TSharedPtr<FJsonObject>& InAnimationObject, FAnimationMetadata& OutAnimation) const
{
	CHECK_PARSE(FJsonUtility::ParseString(InAnimationObject, CPS::Properties::GType, OutAnimation.Type))
	CHECK_PARSE(FJsonUtility::ParseNumber(InAnimationObject, CPS::Properties::GVersion, OutAnimation.Version))
	
	const TArray<TSharedPtr<FJsonValue>>* ControlsJson;
	CHECK_PARSE(FJsonUtility::ParseArray(InAnimationObject, CPS::Properties::GControls, ControlsJson))
	for (const TSharedPtr<FJsonValue>& ControlJson : *ControlsJson)
	{
		OutAnimation.Controls.Add(ControlJson->AsString());
	}

	return ResultOk;
}

FStartStreamingResponse::FStartStreamingResponse()
	: FControlResponse(CPS::AddressPaths::GStartStreaming)
{
}

FStopStreamingResponse::FStopStreamingResponse()
	: FControlResponse(CPS::AddressPaths::GStopStreaming)
{
}

}
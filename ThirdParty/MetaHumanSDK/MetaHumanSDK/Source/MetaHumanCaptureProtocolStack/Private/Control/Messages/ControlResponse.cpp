// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlResponse.h"
#include "Control/Messages/Constants.h"

#include "Control/Messages/ControlJsonUtilities.h"

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
    : FControlResponse(UE::CPS::AddressPaths::GKeepAlive)
{
}

FStartSessionResponse::FStartSessionResponse()
    : FControlResponse(UE::CPS::AddressPaths::GStartSession)
{
}

TProtocolResult<void> FStartSessionResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GSessionId, SessionId));

    return ResultOk;
}

const FString& FStartSessionResponse::GetSessionId() const
{
    return SessionId;
}

FStopSessionResponse::FStopSessionResponse()
    : FControlResponse(UE::CPS::AddressPaths::GStopSession)
{
}

FGetServerInformationResponse::FGetServerInformationResponse()
    : FControlResponse(UE::CPS::AddressPaths::GGetServerInformation)
{
}

TProtocolResult<void> FGetServerInformationResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GId, Id));
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GName, Name));
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GModel, Model));
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GPlatformName, PlatformName));
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GPlatformVersion, PlatformVersion));
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GSoftwareName, SoftwareName));
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GSoftwareVersion, SoftwareVersion));
	CHECK_PARSE(FJsonUtility::ParseNumber(InBody, UE::CPS::Properties::GExportPort, ExportPort));

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
    : FControlResponse(UE::CPS::AddressPaths::GSubscribe)
{
}

FUnsubscribeResponse::FUnsubscribeResponse()
    : FControlResponse(UE::CPS::AddressPaths::GUnsubscribe)
{
}

FGetStateResponse::FGetStateResponse()
    : FControlResponse(UE::CPS::AddressPaths::GGetState)
{
}

TProtocolResult<void> FGetStateResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
    CHECK_PARSE(FJsonUtility::ParseBool(InBody, UE::CPS::Properties::GIsRecording, bIsRecording));

    // Optional
    const TSharedPtr<FJsonObject>* PlatformStatePtr;
    TProtocolResult<void> Result = FJsonUtility::ParseObject(InBody, UE::CPS::Properties::GPlatformState, PlatformStatePtr);
	if (Result.IsValid())
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
    : FControlResponse(UE::CPS::AddressPaths::GStartRecordingTake)
{
}

FStopRecordingTakeResponse::FStopRecordingTakeResponse()
    : FControlResponse(UE::CPS::AddressPaths::GStopRecordingTake)
{
}

FAbortRecordingTakeResponse::FAbortRecordingTakeResponse()
	: FControlResponse(UE::CPS::AddressPaths::GAbortRecordingTake)
{
}

TProtocolResult<void> FStopRecordingTakeResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
	CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GName, TakeName));

	return ResultOk;
}

const FString& FStopRecordingTakeResponse::GetTakeName() const
{
	return TakeName;
}

FGetTakeListResponse::FGetTakeListResponse()
    : FControlResponse(UE::CPS::AddressPaths::GGetTakeList)
{
}

TProtocolResult<void> FGetTakeListResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
    const TArray<TSharedPtr<FJsonValue>>* NamesJson;
    CHECK_PARSE(FJsonUtility::ParseArray(InBody, UE::CPS::Properties::GNames, NamesJson));

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
    : FControlResponse(UE::CPS::AddressPaths::GGetTakeMetadata)
{
}

TProtocolResult<void> FGetTakeMetadataResponse::Parse(TSharedPtr<FJsonObject> InBody)
{
    const TArray<TSharedPtr<FJsonValue>>* TakesJson;
    CHECK_PARSE(FJsonUtility::ParseArray(InBody, UE::CPS::Properties::GTakes, TakesJson));

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
    CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, UE::CPS::Properties::GName, OutTake.Name));
    CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, UE::CPS::Properties::GSlateName, OutTake.Slate));
    CHECK_PARSE(FJsonUtility::ParseNumber(InTakeObject, UE::CPS::Properties::GTakeNumber, OutTake.TakeNumber));
    CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, UE::CPS::Properties::GDateTime, OutTake.DateTime));
    CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, UE::CPS::Properties::GAppVersion, OutTake.AppVersion));
    CHECK_PARSE(FJsonUtility::ParseString(InTakeObject, UE::CPS::Properties::GModel, OutTake.Model));

    // Optional
    FJsonUtility::ParseString(InTakeObject, UE::CPS::Properties::GSubject, OutTake.Subject);
    FJsonUtility::ParseString(InTakeObject, UE::CPS::Properties::GScenario, OutTake.Scenario);

    const TArray<TSharedPtr<FJsonValue>>* TagsJson;
    TProtocolResult<void> ParseArrayResult = FJsonUtility::ParseArray(InTakeObject, UE::CPS::Properties::GTags, TagsJson);

    if (ParseArrayResult.IsValid())
    {
        for (const TSharedPtr<FJsonValue>& TagJson : *TagsJson)
        {
            OutTake.Tags.Add(TagJson->AsString());
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* FilesJson;
    CHECK_PARSE(FJsonUtility::ParseArray(InTakeObject, UE::CPS::Properties::GFiles, FilesJson));

    for (const TSharedPtr<FJsonValue>& FileJson : *FilesJson)
    {
        const TSharedPtr<FJsonObject>& FileJsonObject = FileJson->AsObject();
        FFileObject File;
        CHECK_PARSE(CreateFileObject(FileJsonObject, File));
        OutTake.Files.Add(MoveTemp(File));
    }

    const TSharedPtr<FJsonObject>* VideoMetadata;
    TProtocolResult<void> ParseVideoObject = FJsonUtility::ParseObject(InTakeObject, UE::CPS::Properties::GVideo, VideoMetadata);
    if (ParseVideoObject.IsValid())
    {
		CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, UE::CPS::Properties::GFrames, OutTake.Video.Frames));
        CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, UE::CPS::Properties::GFrameRate, OutTake.Video.FrameRate));
        CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, UE::CPS::Properties::GHeight, OutTake.Video.Height));
        CHECK_PARSE(FJsonUtility::ParseNumber(*VideoMetadata, UE::CPS::Properties::GWidth, OutTake.Video.Width));
    }

    const TSharedPtr<FJsonObject>* AudioMetadata;
    TProtocolResult<void> ParseAudioObject = FJsonUtility::ParseObject(InTakeObject, UE::CPS::Properties::GAudio, AudioMetadata);
    if (ParseAudioObject.IsValid())
    {
        CHECK_PARSE(FJsonUtility::ParseNumber(*AudioMetadata, UE::CPS::Properties::GChannels, OutTake.Audio.Channels));
        CHECK_PARSE(FJsonUtility::ParseNumber(*AudioMetadata, UE::CPS::Properties::GSampleRate, OutTake.Audio.SampleRate));
        CHECK_PARSE(FJsonUtility::ParseNumber(*AudioMetadata, UE::CPS::Properties::GBitsPerChannel, OutTake.Audio.BitsPerChannel));
    }

    return ResultOk;
}

TProtocolResult<void> FGetTakeMetadataResponse::CreateFileObject(const TSharedPtr<FJsonObject>& InFileObject, FFileObject& OutFile) const
{
    CHECK_PARSE(FJsonUtility::ParseString(InFileObject, UE::CPS::Properties::GName, OutFile.Name));
    CHECK_PARSE(FJsonUtility::ParseNumber(InFileObject, UE::CPS::Properties::GLength, OutFile.Length));

    return ResultOk;
}

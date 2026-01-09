// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlRequest.h"
#include "Control/Messages/Constants.h"

FControlRequest::FControlRequest(FString InAddressPath)
    : AddressPath(MoveTemp(InAddressPath))
{
}

const FString& FControlRequest::GetAddressPath() const
{
    return AddressPath;
}

TSharedPtr<FJsonObject> FControlRequest::GetBody() const
{
    return nullptr;
}

FKeepAliveRequest::FKeepAliveRequest()
    : FControlRequest(UE::CPS::AddressPaths::GKeepAlive)
{
}

FStartSessionRequest::FStartSessionRequest()
    : FControlRequest(UE::CPS::AddressPaths::GStartSession)
{
}

FStopSessionRequest::FStopSessionRequest()
    : FControlRequest(UE::CPS::AddressPaths::GStopSession)
{
}

FGetServerInformationRequest::FGetServerInformationRequest()
    : FControlRequest(UE::CPS::AddressPaths::GGetServerInformation)
{
}

FSubscribeRequest::FSubscribeRequest()
    : FControlRequest(UE::CPS::AddressPaths::GSubscribe)
{
}

FUnsubscribeRequest::FUnsubscribeRequest()
    : FControlRequest(UE::CPS::AddressPaths::GUnsubscribe)
{
}

FGetStateRequest::FGetStateRequest()
    : FControlRequest(UE::CPS::AddressPaths::GGetState)
{
}

FStartRecordingTakeRequest::FStartRecordingTakeRequest(FString InSlateName, 
                                                       uint16 InTakeNumber, 
													   TOptional<FString> InSubject,
													   TOptional<FString> InScenario,
													   TOptional<TArray<FString>> InTags)
    : FControlRequest(UE::CPS::AddressPaths::GStartRecordingTake)
    , SlateName(MoveTemp(InSlateName))
    , TakeNumber(InTakeNumber)
    , Subject(MoveTemp(InSubject))
    , Scenario(MoveTemp(InScenario))
    , Tags(MoveTemp(InTags))
{
}

TSharedPtr<FJsonObject> FStartRecordingTakeRequest::GetBody() const
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();

    Body->SetStringField(UE::CPS::Properties::GSlateName, SlateName);
    Body->SetNumberField(UE::CPS::Properties::GTakeNumber, TakeNumber);
	if (Subject.IsSet())
	{
		Body->SetStringField(UE::CPS::Properties::GSubject, Subject.GetValue());
	}
    
	if (Scenario.IsSet())
	{
		Body->SetStringField(UE::CPS::Properties::GScenario, Scenario.GetValue());
	}
    
	if (Tags.IsSet())
	{
		TArray<TSharedPtr<FJsonValue>> TagsJson;
		for (const FString& Tag : Tags.GetValue())
		{
			TagsJson.Add(MakeShared<FJsonValueString>(Tag));
		}

		Body->SetArrayField(UE::CPS::Properties::GTags, TagsJson);
	}
    
    return Body;
}

FStopRecordingTakeRequest::FStopRecordingTakeRequest()
    : FControlRequest(UE::CPS::AddressPaths::GStopRecordingTake)
{
}

FAbortRecordingTakeRequest::FAbortRecordingTakeRequest()
	: FControlRequest(UE::CPS::AddressPaths::GAbortRecordingTake)
{
}

FGetTakeListRequest::FGetTakeListRequest()
    : FControlRequest(UE::CPS::AddressPaths::GGetTakeList)
{
}

FGetTakeMetadataRequest::FGetTakeMetadataRequest(TArray<FString> InNames)
    : FControlRequest(UE::CPS::AddressPaths::GGetTakeMetadata)
    , Names(MoveTemp(InNames))
{
}

TSharedPtr<FJsonObject> FGetTakeMetadataRequest::GetBody() const
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();

    TArray<TSharedPtr<FJsonValue>> NamesJson;
    for (const FString& Name : Names)
    {
        NamesJson.Add(MakeShared<FJsonValueString>(Name));
    }

    Body->SetArrayField(UE::CPS::Properties::GNames, NamesJson);

    return Body;
}

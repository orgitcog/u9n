// Copyright Epic Games, Inc. All Rights Reserved.

#include "Control/Messages/ControlUpdate.h"
#include "Control/Messages/Constants.h"

#include "Control/Messages/ControlJsonUtilities.h"

TProtocolResult<TSharedRef<FControlUpdate>> FControlUpdateCreator::Create(const FString& InAddressPath)
{
    if (InAddressPath == UE::CPS::AddressPaths::GSessionStopped)
    {
        return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FSessionStopped>());
    }
	else if (InAddressPath == UE::CPS::AddressPaths::GTakeAdded)
	{
		return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FTakeAddedUpdate>());
	}
	else if (InAddressPath == UE::CPS::AddressPaths::GTakeRemoved)
	{
		return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FTakeRemovedUpdate>());
	}
	else if (InAddressPath == UE::CPS::AddressPaths::GTakeUpdated)
	{
		return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FTakeUpdatedUpdate>());
	}
    else if (InAddressPath == UE::CPS::AddressPaths::GRecordingStatus)
    {
        return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FRecordingStatusUpdate>());
    }
    else if(InAddressPath == UE::CPS::AddressPaths::GDiskCapacity)
    {
        return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FDiskCapacityUpdate>());
    }
    else if (InAddressPath == UE::CPS::AddressPaths::GBattery)
    {
        return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FBatteryPercentageUpdate>());
    }
    else if (InAddressPath == UE::CPS::AddressPaths::GThermalState)
    {
        return TProtocolResult<TSharedRef<FControlUpdate>>(MakeShared<FThermalStateUpdate>());
    }
    else
    {
        return FCaptureProtocolError(TEXT("Unknown update arrived"));
    }
}

FControlUpdate::FControlUpdate(FString InAddressPath)
    : AddressPath(MoveTemp(InAddressPath))
{
}

const FString& FControlUpdate::GetAddressPath() const
{
    return AddressPath;
}

TProtocolResult<void> FControlUpdate::Parse(TSharedPtr<FJsonObject> InBody)
{
    if (InBody.IsValid() && !InBody->Values.IsEmpty())
    {
        return FCaptureProtocolError("Update must NOT have a body");
    }

    return ResultOk;
}

FSessionStopped::FSessionStopped()
    : FControlUpdate(UE::CPS::AddressPaths::GSessionStopped)
{
}

FRecordingStatusUpdate::FRecordingStatusUpdate()
    : FControlUpdate(UE::CPS::AddressPaths::GRecordingStatus)
{
}

TProtocolResult<void> FRecordingStatusUpdate::Parse(TSharedPtr<FJsonObject> InBody)
{
    CHECK_PARSE(FJsonUtility::ParseBool(InBody, UE::CPS::Properties::GIsRecording, bIsRecording));

    return ResultOk;
}

bool FRecordingStatusUpdate::IsRecording() const
{
    return bIsRecording;
}

TProtocolResult<void> FBaseTakeUpdate::Parse(TSharedPtr<FJsonObject> InBody)
{
	CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GName, TakeName));

	return ResultOk;
}

const FString& FBaseTakeUpdate::GetTakeName() const
{
	return TakeName;
}

FTakeAddedUpdate::FTakeAddedUpdate()
	: FBaseTakeUpdate(UE::CPS::AddressPaths::GTakeAdded)
{
}

FTakeRemovedUpdate::FTakeRemovedUpdate()
	: FBaseTakeUpdate(UE::CPS::AddressPaths::GTakeRemoved)
{
}

FTakeUpdatedUpdate::FTakeUpdatedUpdate()
	: FBaseTakeUpdate(UE::CPS::AddressPaths::GTakeUpdated)
{
}

FDiskCapacityUpdate::FDiskCapacityUpdate()
    : FControlUpdate(UE::CPS::AddressPaths::GDiskCapacity)
{
}

TProtocolResult<void> FDiskCapacityUpdate::Parse(TSharedPtr<FJsonObject> InBody)
{
    CHECK_PARSE(FJsonUtility::ParseNumber(InBody, UE::CPS::Properties::GTotal, Total));
    CHECK_PARSE(FJsonUtility::ParseNumber(InBody, UE::CPS::Properties::GRemaining, Remaining));

    return ResultOk;
}

uint64 FDiskCapacityUpdate::GetTotal() const
{
    return Total;
}

uint64 FDiskCapacityUpdate::GetRemaining() const
{
    return Remaining;
}

FBatteryPercentageUpdate::FBatteryPercentageUpdate()
    : FControlUpdate(UE::CPS::AddressPaths::GBattery)
{
}

TProtocolResult<void> FBatteryPercentageUpdate::Parse(TSharedPtr<FJsonObject> InBody)
{
    CHECK_PARSE(FJsonUtility::ParseNumber(InBody, UE::CPS::Properties::GLevel, Level));

    return ResultOk;
}

float FBatteryPercentageUpdate::GetLevel() const
{
    return Level;
}

FThermalStateUpdate::FThermalStateUpdate()
    : FControlUpdate(UE::CPS::AddressPaths::GThermalState)
{
}

TProtocolResult<void> FThermalStateUpdate::Parse(TSharedPtr<FJsonObject> InBody)
{
    FString StateStr;
    CHECK_PARSE(FJsonUtility::ParseString(InBody, UE::CPS::Properties::GState, StateStr));

    State = ConvertState(StateStr);
    if (State == EState::Invalid)
    {
        return FCaptureProtocolError("Invalid thermal state provided: " + StateStr);
    }

    return ResultOk;
}

FThermalStateUpdate::EState FThermalStateUpdate::GetState() const
{
    return State;
}

FThermalStateUpdate::EState FThermalStateUpdate::ConvertState(const FString& InStateString)
{
    if (InStateString == UE::CPS::Properties::GNominal)
    {
        return EState::Nominal;
    }
    else if (InStateString == UE::CPS::Properties::GFair)
    {
        return EState::Fair;
    }
    else if (InStateString == UE::CPS::Properties::GSerious)
    {
        return EState::Serious;
    }
    else if (InStateString == UE::CPS::Properties::GCritical)
    {
        return EState::Critical;
    }
    else
    {
        return EState::Invalid;
    }
}

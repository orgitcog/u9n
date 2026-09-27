// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlJsonUtilities.h"

#include "Utility/Error.h"

#include "Delegates/Delegate.h"

class FControlUpdate;
class FControlUpdateCreator final
{
public:
    static TProtocolResult<TSharedRef<FControlUpdate>> Create(const FString& InAddressPath);
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FControlUpdate
{
public:

    DECLARE_DELEGATE_OneParam(FOnUpdateMessage, TSharedPtr<FControlUpdate> InUpdateMessage);

    FControlUpdate(FString InAddressPath);
    virtual ~FControlUpdate() = default;

    const FString& GetAddressPath() const;

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody);

private:

    FString AddressPath;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FSessionStopped final : public FControlUpdate
{
public:

    FSessionStopped();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FRecordingStatusUpdate final : public FControlUpdate
{
public:

    FRecordingStatusUpdate();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    bool IsRecording() const;

private:

    bool bIsRecording;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FBaseTakeUpdate : public FControlUpdate
{
public:

	using FControlUpdate::FControlUpdate;

	virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

	const FString& GetTakeName() const;

private:

	FString TakeName;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FTakeAddedUpdate final : public FBaseTakeUpdate
{
public:

	FTakeAddedUpdate();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FTakeRemovedUpdate final : public FBaseTakeUpdate
{
public:

	FTakeRemovedUpdate();
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FTakeUpdatedUpdate final : public FBaseTakeUpdate
{
public:

	FTakeUpdatedUpdate();
};

// IOS

class METAHUMANCAPTUREPROTOCOLSTACK_API FDiskCapacityUpdate final : public FControlUpdate
{
public:

    FDiskCapacityUpdate();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    uint64 GetTotal() const;
    uint64 GetRemaining() const;

private:

    uint64 Total;
    uint64 Remaining;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FBatteryPercentageUpdate final : public FControlUpdate
{
public:

    FBatteryPercentageUpdate();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    float GetLevel() const;

private:

    float Level;
};

class METAHUMANCAPTUREPROTOCOLSTACK_API FThermalStateUpdate final : public FControlUpdate
{
public:

    enum class EState
    {
        Nominal = 0,
        Fair,
        Serious,
        Critical,

        Invalid
    };

    FThermalStateUpdate();

    virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

    EState GetState() const;

private:

    static EState ConvertState(const FString& InStateString);

    EState State;
};
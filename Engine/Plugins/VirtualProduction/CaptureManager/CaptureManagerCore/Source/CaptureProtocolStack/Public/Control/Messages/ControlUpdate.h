// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlJsonUtilities.h"

#include "Utility/Error.h"

#include "Delegates/Delegate.h"

namespace UE::CaptureManager
{

class FControlUpdate;
class FControlUpdateCreator final
{
public:
	static TProtocolResult<TSharedRef<FControlUpdate>> Create(const FString& InAddressPath);
};

class CAPTUREPROTOCOLSTACK_API FControlUpdate
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

class CAPTUREPROTOCOLSTACK_API FSessionStopped final : public FControlUpdate
{
public:

	FSessionStopped();
};

class CAPTUREPROTOCOLSTACK_API FRecordingStatusUpdate final : public FControlUpdate
{
public:

	FRecordingStatusUpdate();

	virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

	bool IsRecording() const;

private:

	bool bIsRecording;
};

class CAPTUREPROTOCOLSTACK_API FBaseTakeUpdate : public FControlUpdate
{
public:

	using FControlUpdate::FControlUpdate;

	virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

	const FString& GetTakeName() const;

private:

	FString TakeName;
};

class CAPTUREPROTOCOLSTACK_API FTakeAddedUpdate final : public FBaseTakeUpdate
{
public:

	FTakeAddedUpdate();
};

class CAPTUREPROTOCOLSTACK_API FTakeRemovedUpdate final : public FBaseTakeUpdate
{
public:

	FTakeRemovedUpdate();
};

class CAPTUREPROTOCOLSTACK_API FTakeUpdatedUpdate final : public FBaseTakeUpdate
{
public:

	FTakeUpdatedUpdate();
};

// IOS

class CAPTUREPROTOCOLSTACK_API FDiskCapacityUpdate final : public FControlUpdate
{
public:

	FDiskCapacityUpdate();

	virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

	uint64 GetTotal() const;
	uint64 GetRemaining() const;

private:

	uint64 Total = 0;
	uint64 Remaining = 0;
};

class CAPTUREPROTOCOLSTACK_API FBatteryPercentageUpdate final : public FControlUpdate
{
public:

	FBatteryPercentageUpdate();

	virtual TProtocolResult<void> Parse(TSharedPtr<FJsonObject> InBody) override;

	float GetLevel() const;

private:

	float Level = 0.0f;
};

class CAPTUREPROTOCOLSTACK_API FThermalStateUpdate final : public FControlUpdate
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

}
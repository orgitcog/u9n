// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/DataStream/DataStream.h"
#include "Iris/DataStream/DataStreamManager.h"

UDataStream::~UDataStream()
{
}

UDataStream::EWriteResult UDataStream::BeginWrite(const FBeginWriteParameters& Params)
{
	return EWriteResult::HasMoreData;
}

void UDataStream::EndWrite()
{
}

void UDataStream::Init(const FInitParameters& Params)
{
	DataStreamInitParameters = Params;
}

void UDataStream::Deinit()
{
}

void UDataStream::Update(const FUpdateParameters& Params)
{
}

const TCHAR* LexToString(const UDataStream::EDataStreamState State)
{
	static const TCHAR* Names[] = {
		TEXT("Invalid"),
		TEXT("PendingCreate"),
		TEXT("WaitOnCreateConfirmation"),
		TEXT("Open"),
		TEXT("PendingClose"),
		TEXT("WaitOnCloseConfirmation"),
	};
	static_assert(UE_ARRAY_COUNT(Names) == uint32(UDataStream::EDataStreamState::Count), "Missing names for one or more values of EDataStreamState.");

	return State < UDataStream::EDataStreamState::Count ? Names[(uint32)State] : TEXT("");
}

const UDataStream::EDataStreamState UDataStream::GetState() const
{
	if (DataStreamInitParameters.DataStreamManager)
	{
		return DataStreamInitParameters.DataStreamManager->GetStreamState(GetDataStreamName());
	}
	else
	{
		return EDataStreamState::Invalid;
	}
}

void UDataStream::RequestClose()
{
	if (DataStreamInitParameters.DataStreamManager)
	{
		DataStreamInitParameters.DataStreamManager->CloseStream(GetDataStreamName());
	}
}

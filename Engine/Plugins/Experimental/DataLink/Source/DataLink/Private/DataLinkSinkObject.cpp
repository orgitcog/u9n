// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkSinkObject.h"
#include "DataLinkSink.h"

UDataLinkSinkObject::UDataLinkSinkObject()
{
	Sink = MakeShared<FDataLinkSink>();
}

void UDataLinkSinkObject::ResetSink()
{
	Sink = MakeShared<FDataLinkSink>();
}

TSharedPtr<FDataLinkSink> UDataLinkSinkObject::GetSink() const
{
	return Sink;
}

const UDataLinkSinkObject* UDataLinkSinkObject::GetSinkObject_Implementation() const
{
	return this;
}

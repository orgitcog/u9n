// Copyright Epic Games, Inc. All Rights Reserved.

#include "Messenger.h"

void FFeatureBase::SetEndpoint(TSharedPtr<FMessageEndpoint> InEndpoint)
{
	Endpoint = InEndpoint;
}

void FFeatureBase::SetAddress(const FMessageAddress& InAddress)
{
	Address = InAddress;
}
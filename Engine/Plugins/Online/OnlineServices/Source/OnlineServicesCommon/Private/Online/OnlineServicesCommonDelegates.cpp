// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesCommonDelegates.h"
#include "Online/OnlineServicesCommon.h"

namespace UE::Online {

FOnOnlineAsyncOpCompletedParams::FOnOnlineAsyncOpCompletedParams(FOnlineServicesCommon& InOnlineServicesCommon, const TOptional<FOnlineError>& InOnlineError)
	: OnlineServicesCommon(InOnlineServicesCommon.AsShared())
	, OnlineError(InOnlineError)
{
}

FOnOnlineAsyncOpCompletedV2 OnOnlineAsyncOpCompletedV2;

/* UE::Online */ }

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackChannel/Types.h"

#define UE_API BACKCHANNEL_API



class FBackChannelDispatchMap
{

public:

	UE_API FBackChannelDispatchMap();

	virtual ~FBackChannelDispatchMap() {}

	UE_API FDelegateHandle AddRoute(FStringView Path, FBackChannelRouteDelegate::FDelegate Delegate);
	
	UE_API void RemoveRoute(FStringView Path, FDelegateHandle DelegateHandle);

	UE_API bool	DispatchMessage(IBackChannelPacket& Message);

protected:

	TMap<FString, FBackChannelRouteDelegate> DispatchMap;

};

#undef UE_API

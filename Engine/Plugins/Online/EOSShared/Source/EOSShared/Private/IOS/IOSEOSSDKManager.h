// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EOS_SDK

#include "EOSSDKManager.h"

class FIOSEOSSDKManager : public FEOSSDKManager
{
public:
	FIOSEOSSDKManager();
	~FIOSEOSSDKManager();

	virtual FString GetCacheDirBase() const override;
private:
	void OnAudioInterruptedNotification(bool bInterrupted);
};

using FPlatformEOSSDKManager = FIOSEOSSDKManager;

#endif // WITH_EOS_SDK

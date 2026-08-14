// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "OnlineSubsystemCatchHelper.h"
#include "Helpers/AutoRestoreConfig.h"

class FOnlineSubsystemEOSLegacyTestFixture : public FOnlineSubsystemTestBaseFixture
{ 
public:
	FOnlineSubsystemEOSLegacyTestFixture()
		: ConfigInstanceName(TEXT("/Script/OnlineSubsystemEOS.EOSSettings"), TEXT("bUseNewLoginFlow"), GEngineIni)
	{
		bool bUseNewLoginFlow = false;
		ConfigInstanceName.SetValue(bUseNewLoginFlow);
	}

	~FOnlineSubsystemEOSLegacyTestFixture()
	{
		DestroyCurrentOnlineSubsystemModule();
	}

	void DestroyCurrentOnlineSubsystemModule() const override 
	{
		ConfigInstanceName.Reset();
		REQUIRE(!FOnlineSubsystemTestBaseFixture::GetSubsystem().IsEmpty());
		FOnlineSubsystemTestBaseFixture::DestroyCurrentOnlineSubsystemModule();
	}

private:	
	mutable TAutoRestoreGConfig<bool> ConfigInstanceName;
};

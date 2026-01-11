// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EOSSharedTypes.h"
#include "Online/OnlineServicesEOSGSTypes.h"
#include "Online/PresenceCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_presence_types.h"

namespace UE::Online {

class FOnlineServicesEpicCommon;
	
class FPresenceEOS : public FPresenceCommon
{
public:
	using Super = FPresenceCommon;

	ONLINESERVICESEOS_API FPresenceEOS(FOnlineServicesEpicCommon& InServices);

	ONLINESERVICESEOS_API virtual void Initialize() override;
	ONLINESERVICESEOS_API virtual void PreShutdown() override;

	ONLINESERVICESEOS_API virtual TOnlineAsyncOpHandle<FQueryPresence> QueryPresence(FQueryPresence::Params&& Params) override;
	ONLINESERVICESEOS_API virtual TOnlineResult<FGetCachedPresence> GetCachedPresence(FGetCachedPresence::Params&& Params) override;
	ONLINESERVICESEOS_API virtual TOnlineAsyncOpHandle<FUpdatePresence> UpdatePresence(FUpdatePresence::Params&& Params) override;
	ONLINESERVICESEOS_API virtual TOnlineAsyncOpHandle<FPartialUpdatePresence> PartialUpdatePresence(FPartialUpdatePresence::Params&& Params) override;

protected:
	/** Get a user's presence, creating entries if missing */
	ONLINESERVICESEOS_API TSharedRef<FUserPresence> FindOrCreatePresence(FAccountId LocalAccountId, FAccountId PresenceAccountId);
	/** Update a user's presence from EOS's current value */
	ONLINESERVICESEOS_API void UpdateUserPresence(FAccountId LocalAccountId, FAccountId PresenceAccountId);
	/** Performs queued presence updates after a user's login completes */
	ONLINESERVICESEOS_API void HandleAuthLoginStatusChanged(const FAuthLoginStatusChanged& EventParameters);

	ONLINESERVICESEOS_API void HandlePresenceChanged(const EOS_Presence_PresenceChangedCallbackInfo* Data);

	ONLINESERVICESEOS_API FAccountId FindAccountId(const EOS_EpicAccountId EpicAccountId);

	EOS_HPresence PresenceHandle = nullptr;

	/** Login status changed event handle */
	FOnlineEventDelegateHandle LoginStatusChangedHandle;

	TMap<EOS_EpicAccountId, TArray<EOS_EpicAccountId>> PendingPresenceUpdates;
	TMap<FAccountId, TMap<FAccountId, TSharedRef<FUserPresence>>> PresenceLists;
	
	FEOSEventRegistrationPtr OnPresenceChanged;
};

/* UE::Online */ }

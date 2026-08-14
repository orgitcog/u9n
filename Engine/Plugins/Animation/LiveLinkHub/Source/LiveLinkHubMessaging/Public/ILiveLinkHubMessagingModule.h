// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Misc/Guid.h"
#include "Misc/TVariant.h"
#include "Modules/ModuleInterface.h"

#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointer.h"

#define UE_API LIVELINKHUBMESSAGING_API

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHubConnectionEstablished, FGuid SourceId);


DECLARE_LOG_CATEGORY_EXTERN(LogLiveLinkHubMessaging, Display, All);


class FLiveLinkHubInstanceId
{
public:
	UE_API explicit FLiveLinkHubInstanceId(FGuid Guid);
	UE_API explicit FLiveLinkHubInstanceId(FStringView NamedId);

	UE_API FString ToString() const;

private:
	/** Variant holding the actual instance ID. */
	TVariant<FGuid, FString> Id;
};

enum class ELiveLinkTopologyMode : uint8;

class ILiveLinkHubMessagingModule : public IModuleInterface
{
public:
	/** Delegate called when a connection is established to a livelink hub. */
	virtual FOnHubConnectionEstablished& OnConnectionEstablished() = 0;

	/** Set the topology mode for this host. This will dictate to what apps it can connect to. */
	virtual void SetHostTopologyMode(ELiveLinkTopologyMode InMode) = 0;

	/** Get the topology mode for this host. */
	virtual ELiveLinkTopologyMode GetHostTopologyMode() const = 0;

	/** Set the instance ID for this connection manager, used to detect if it's trying to connect to itself. */
	virtual void SetInstanceId(const FLiveLinkHubInstanceId& Id) = 0;

	/** Get the ID for this running instance. Returns an empty guid if this isn't running as part of LiveLinkHub. */
	virtual FLiveLinkHubInstanceId GetInstanceId() const = 0;
};

#undef UE_API

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

#include "Discovery/DiscoveryMessenger.h"

/**
 * Periodically sends out multicast discovery requests and listens for
 * any responses from devices running the Live Link Face application.
 *
 * In addition, if a device sends a discovery notification the internal
 * list of discovered servers will be updated.
 *
 * Each time the internal server list is updated the delegate will be triggered
 * on the game thread with a set of discovered servers.
 */
class LIVELINKFACEDISCOVERY_API FLiveLinkFaceDiscovery : public TSharedFromThis<FLiveLinkFaceDiscovery>
{
public:
	/**
	 * A representation of a discovered device running the Live Link Face application
	 */
	struct FServer
	{
		/** A unique identifier for the device */
		const FGuid Id;

		/** The user defined device name */
		const FString Name;

		/** The IPV4 address of the discovered device */
		const FString Address;

		/** The port to use when establishing a CPS control TCP connection */
		const uint16 ControlPort;

		/** The PlatformTime when the device last provided a discovery response or notification */
		const double LastSeen;

		FServer(const FGuid InId, FString InName, FString InAddress, const uint16 InControlPort, const double InLastSeen)
			: Id(InId)
			, Name(MoveTemp(InName))
			, Address(MoveTemp(InAddress))
			, ControlPort(InControlPort)
			, LastSeen(InLastSeen)
			{
			}

		bool operator==(const FServer& InOther) const
		{
			return Id == InOther.Id;
		}
		
		friend uint32 GetTypeHash(const FServer& Server)
		{
			return GetTypeHash(Server.Id);
		}
	};

	/**
	 * @param InRefreshDelay The period of time in seconds to wait before sending discovery requests.
	 * @param InServerExpiry The period of time after which we consider a device stale and remove it from the set.
	 * Note that this will only be evaluated every InRefreshDelay seconds and should be larger than the refresh delay.
	 */
	FLiveLinkFaceDiscovery(const double InRefreshDelay = 3.0f, const double InServerExpiry = 6.0f);
	~FLiveLinkFaceDiscovery();

	/**
	 * Start discovery. Bind to the OnServersUpdated delegate before starting to receive every update. 
	 */
	void Start();
	
	void Stop();

	DECLARE_DELEGATE_OneParam(FOnServersUpdated, const TSet<FServer>& Servers);

	/**
	 * Bind to this delegate before starting discovery in order to receive every update
	 */
	FOnServersUpdated OnServersUpdated;

private:
	const float RefreshDelay;
	const float ServerExpiry;
	
	TUniquePtr<UE::CaptureManager::FDiscoveryMessenger> DiscoveryMessenger;
	FTSTicker::FDelegateHandle RefreshTickerHandle;
	TSet<FServer> Servers;
	
	static uint32 Pack(const uint8 InA, const uint8 InB, const uint8 InC, const uint8 InD);
	
	FServer CreateServer(const FString& InServerAddress, const TStaticArray<uint8, 16>& InServerId, const FString& InServerName, const uint16 InControlPort);
	bool Refresh(float InDeltaTime);
	void SendRequestBurst() const;
	void UpdateDelegate();
};
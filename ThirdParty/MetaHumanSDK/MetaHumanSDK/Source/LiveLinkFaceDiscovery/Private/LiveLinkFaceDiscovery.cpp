// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceDiscovery.h"

#include "Async/Async.h"

using namespace UE::CaptureManager;


FLiveLinkFaceDiscovery::FLiveLinkFaceDiscovery(const double InRefreshDelay, const double InServerExpiry)
	: RefreshDelay(InRefreshDelay)
	, ServerExpiry(InServerExpiry)
{
}

FLiveLinkFaceDiscovery::~FLiveLinkFaceDiscovery()
{
	Stop();
}

void FLiveLinkFaceDiscovery::Start()
{
	if (DiscoveryMessenger.IsValid())
	{
		return;
	}
	
	DiscoveryMessenger = MakeUnique<FDiscoveryMessenger>();
	FDiscoveryMessenger::FOnResponseArrived OnResponseArrived = FDiscoveryMessenger::FOnResponseArrived::CreateLambda([this](const FString& InServerAddress, const FDiscoveryResponse& InResponse)
	{
		AsyncTask(ENamedThreads::GameThread, [This = SharedThis(this), InServerAddress, InResponse]
		{
			const FServer Server = This->CreateServer(InServerAddress, InResponse.GetServerId(), InResponse.GetServerName(), InResponse.GetControlPort());
			This->Servers.Add(Server);
			This->UpdateDelegate();
		});
	});
	DiscoveryMessenger->SetResponseHandler(MoveTemp(OnResponseArrived));
	
	FDiscoveryMessenger::FOnNotifyArrived OnNotifyArrived = FDiscoveryMessenger::FOnNotifyArrived::CreateLambda([this](const FString& InServerAddress, const FDiscoveryNotify& InNotification)
	{
		AsyncTask(ENamedThreads::GameThread, [This = SharedThis(this), InServerAddress, InNotification]
		{
			const FServer Server = This->CreateServer(InServerAddress, InNotification.GetServerId(), InNotification.GetServerName(), InNotification.GetControlPort());
		
			if (InNotification.GetConnectionState() == FDiscoveryNotify::EConnectionState::Online)
			{
				// A new server came online whilst discovery was running
				This->Servers.Add(Server);
			}
			else if (InNotification.GetConnectionState() == FDiscoveryNotify::EConnectionState::Offline)
			{
				// A server went offline, remove it from our set.
				This->Servers.Remove(Server);
			}
			This->UpdateDelegate();
		});
	});
	DiscoveryMessenger->SetNotifyHandler(MoveTemp(OnNotifyArrived));
	
	DiscoveryMessenger->Start();
	SendRequestBurst();

	const FTickerDelegate Delegate = FTickerDelegate::CreateRaw(this, &FLiveLinkFaceDiscovery::Refresh);
	RefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(Delegate, RefreshDelay);
}

void FLiveLinkFaceDiscovery::Stop()
{
	if (DiscoveryMessenger.IsValid())
	{
		DiscoveryMessenger->SetResponseHandler(nullptr);
		DiscoveryMessenger->SetNotifyHandler(nullptr);
		DiscoveryMessenger->Stop();
		DiscoveryMessenger.Reset();
	}
	
	Servers.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(MoveTemp(RefreshTickerHandle));
}

uint32 FLiveLinkFaceDiscovery::Pack(const uint8 InA, const uint8 InB, const uint8 InC, const uint8 InD)
{
	return InA << 24 | InB << 16 | InC << 8 | InD;
}

FLiveLinkFaceDiscovery::FServer FLiveLinkFaceDiscovery::CreateServer(const FString& InServerAddress, const TStaticArray<uint8, 16>& InServerId, const FString& InServerName, const uint16 InControlPort)
{
	uint32 A = Pack(InServerId[0], InServerId[1], InServerId[2], InServerId[3]);
	uint32 B = Pack(InServerId[4], InServerId[5], InServerId[6], InServerId[7]);
	uint32 C = Pack(InServerId[8], InServerId[9], InServerId[10], InServerId[11]);
	uint32 D = Pack(InServerId[12], InServerId[13], InServerId[14], InServerId[15]);
	FGuid ServerGuid(A, B, C, D);
	return FServer(ServerGuid, InServerName, InServerAddress, InControlPort, FPlatformTime::Seconds());
}

bool FLiveLinkFaceDiscovery::Refresh(float InDeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	
	DiscoveryMessenger->SendRequest();

	bool ServerRemoved = false;
	// Prune servers that we haven't heard from for ServerExpiry seconds.
	for (TSet<FServer>::TIterator It(Servers); It; ++It)
	{
		if (Now - It->LastSeen > ServerExpiry)
		{
			It.RemoveCurrent();
			ServerRemoved = true;
		}
	}

	if (ServerRemoved)
	{
		UpdateDelegate();
	}
	
	return true;
}

void FLiveLinkFaceDiscovery::SendRequestBurst() const
{
	constexpr uint16 Count = 3;
	for (int32 i = 0; i < Count; ++i)
	{
		DiscoveryMessenger->SendRequest();
	}
}

void FLiveLinkFaceDiscovery::UpdateDelegate()
{
	const TSet<FServer> ServersCopy = Servers;
	OnServersUpdated.ExecuteIfBound(ServersCopy);
}

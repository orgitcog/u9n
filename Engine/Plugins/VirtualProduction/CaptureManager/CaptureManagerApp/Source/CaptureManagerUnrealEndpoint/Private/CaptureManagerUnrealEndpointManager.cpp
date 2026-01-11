// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerUnrealEndpointManager.h"
#include "CaptureManagerUnrealEndpointLog.h"

#include "DiscoveryRequester.h"

#include <condition_variable>

namespace UE::CaptureManager
{

struct FUnrealEndpointManager::FImpl
{
	FImpl();
	~FImpl();

	void Start();
	void Stop();

	TSharedPtr<FUnrealEndpoint> WaitForEndpoint(TFunction<bool(const FUnrealEndpoint&)> InPredicate, int32 InTimeoutMS);
	TSharedPtr<FUnrealEndpoint> FindEndpointByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate);
	TArray<TSharedRef<FUnrealEndpoint>> GetEndpoints();
	TArray<TSharedRef<FUnrealEndpoint>> FindEndpointsByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate);
	int32 GetNumEndpoints() const;

	TArray<TSharedRef<FUnrealEndpoint>> Endpoints;
	TUniquePtr<FDiscoveryRequester> DiscoveryRequester;
	FDelegateHandle EndpointFoundDelegateHandle;
	FDelegateHandle EndpointLostDelegateHandle;
	std::atomic<bool> bIsRunning;
	FEndpointsChanged EndpointsChangedDelegate;

	// We protect class state with a standard mutex rather than a critical section, just because we're using 
	// a condition variable and one mutex is better than two.
	mutable std::mutex Mutex;
	mutable std::condition_variable CondVar;
};

FUnrealEndpointManager::FUnrealEndpointManager() :
	Impl(MakePimpl<FImpl>())
{
}

FUnrealEndpointManager::~FUnrealEndpointManager() = default;

void FUnrealEndpointManager::Start()
{
	Impl->Start();
}

void FUnrealEndpointManager::Stop()
{
	Impl->Stop();
}

TSharedPtr<FUnrealEndpoint> FUnrealEndpointManager::WaitForEndpoint(TFunction<bool(const FUnrealEndpoint&)> InPredicate, int32 InTimeoutMS)
{
	return Impl->WaitForEndpoint(MoveTemp(InPredicate), InTimeoutMS);
}

TSharedPtr<FUnrealEndpoint> FUnrealEndpointManager::FindEndpointByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate)
{
	return Impl->FindEndpointByPredicate(MoveTemp(InPredicate));
}

TArray<TSharedRef<FUnrealEndpoint>> FUnrealEndpointManager::FindEndpointsByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate)
{
	return Impl->FindEndpointsByPredicate(MoveTemp(InPredicate));
}

TArray<TSharedRef<FUnrealEndpoint>> FUnrealEndpointManager::GetEndpoints()
{
	return Impl->GetEndpoints();
}

int32 FUnrealEndpointManager::GetNumEndpoints() const
{
	return Impl->GetNumEndpoints();
}

FUnrealEndpointManager::FEndpointsChanged& FUnrealEndpointManager::EndpointsChanged()
{
	return Impl->EndpointsChangedDelegate;
}

FUnrealEndpointManager::FImpl::FImpl() :
	bIsRunning(false)
{
}

FUnrealEndpointManager::FImpl::~FImpl()
{
	// Stop should have been called before destruction
	check(!bIsRunning);
}

void FUnrealEndpointManager::FImpl::Start()
{
	if (bIsRunning)
	{
		return;
	}

	TUniquePtr<FDiscoveryRequester> NewRequester = FDiscoveryRequester::Create();

	std::lock_guard<std::mutex> LockGuard(Mutex);

	DiscoveryRequester = MoveTemp(NewRequester);

	if (!DiscoveryRequester)
	{
		UE_LOG(LogCaptureManagerUnrealEndpoint, Warning, TEXT("Endpoint manager failed to start (Discovery is disabled)"));
		return;
	}

	bIsRunning = true;

	EndpointFoundDelegateHandle = DiscoveryRequester->ClientFound().AddLambda(
		[this](const FDiscoveredClient& InDiscoveredClient)
		{
			FUnrealEndpointInfo EndpointInfo =
			{
				.EndpointID = InDiscoveredClient.GetClientID(),
				.MessageAddress = InDiscoveredClient.GetMessageAddress(),
				.IPAddress = InDiscoveredClient.GetIPAddress(),
				.HostName = InDiscoveredClient.GetHostName(),
				.ImportServicePort = InDiscoveredClient.GetExportPort()
			};

			TSharedRef<FUnrealEndpoint> Endpoint = MakeShared<FUnrealEndpoint>(EndpointInfo);

			{
				std::lock_guard<std::mutex> LockGuard(Mutex);
				Endpoints.Emplace(MoveTemp(Endpoint));
			}
			CondVar.notify_one();

			EndpointsChangedDelegate.Broadcast();
		}
	);

	EndpointLostDelegateHandle = DiscoveryRequester->ClientLost().AddLambda(
		[this](const FGuid& InEndpointId)
		{
			std::unique_lock<std::mutex> LockGuard(Mutex);

			const int32 IndexToRemove = Endpoints.IndexOfByPredicate(
				[&InEndpointId](const TSharedRef<FUnrealEndpoint>& InEndpoint)
				{
					return InEndpoint->GetInfo().EndpointID == InEndpointId;
				}
			);

			if (IndexToRemove != INDEX_NONE)
			{
				Endpoints.RemoveAt(IndexToRemove);
			}

			LockGuard.unlock();
			EndpointsChangedDelegate.Broadcast();

		}
	);

	DiscoveryRequester->Start();
}

void FUnrealEndpointManager::FImpl::Stop()
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	if (DiscoveryRequester)
	{
		DiscoveryRequester->ClientFound().Remove(EndpointFoundDelegateHandle);
		DiscoveryRequester->ClientLost().Remove(EndpointLostDelegateHandle);
		DiscoveryRequester.Reset();
	}

	Endpoints.Empty();
	bIsRunning = false;
}

TSharedPtr<FUnrealEndpoint> FUnrealEndpointManager::FImpl::WaitForEndpoint(TFunction<bool(const FUnrealEndpoint&)> InPredicate, const int32 InTimeoutMS)
{
	TSharedPtr<FUnrealEndpoint> Endpoint;

	std::unique_lock<std::mutex> Lock(Mutex);

	[[maybe_unused]] bool bWaitSuccess = CondVar.wait_for(
		Lock,
		std::chrono::milliseconds(InTimeoutMS),
		[this, &InPredicate, &Endpoint]() -> bool
		{
			TSharedRef<FUnrealEndpoint>* FoundEndpoint = Endpoints.FindByPredicate(
				[&InPredicate](const TSharedRef<FUnrealEndpoint>& InEndpoint)
				{
					// We convert the internal shared ref into a simple reference, just to make the caller's life a bit easier
					return InPredicate(*InEndpoint);
				}
			);

			if (FoundEndpoint)
			{
				Endpoint = *FoundEndpoint;
			}

			return Endpoint != nullptr;
		}
	);

	return Endpoint;
}

TSharedPtr<FUnrealEndpoint> FUnrealEndpointManager::FImpl::FindEndpointByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	TSharedRef<FUnrealEndpoint>* Endpoint = Endpoints.FindByPredicate(
		[&InPredicate](const TSharedRef<FUnrealEndpoint>& InEndpoint)
		{
			return InPredicate(*InEndpoint);
		}
	);

	if (Endpoint)
	{
		return (*Endpoint).ToSharedPtr();
	}

	return nullptr;
}

TArray<TSharedRef<FUnrealEndpoint>> FUnrealEndpointManager::FImpl::GetEndpoints()
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	return Endpoints;
}

TArray<TSharedRef<FUnrealEndpoint>> FUnrealEndpointManager::FImpl::FindEndpointsByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);

	TArray<TSharedRef<FUnrealEndpoint>> FilteredEndpoints;

	for (const TSharedRef<FUnrealEndpoint>& Endpoint : Endpoints)
	{
		if (InPredicate(*Endpoint))
		{
			FilteredEndpoints.Emplace(Endpoint);
		}
	}

	return FilteredEndpoints;
}

int32 FUnrealEndpointManager::FImpl::GetNumEndpoints() const
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	return Endpoints.Num();
}

}

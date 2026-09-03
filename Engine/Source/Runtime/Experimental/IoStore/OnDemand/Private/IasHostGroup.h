// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Mutex.h"
#include "IO/IoStatus.h"
#include "OnDemandBackendStatus.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

namespace UE::IoStore
{

class FOnDemandHostGroup;

class FIASHostGroup final
{
	struct FImpl;
	using FSharedImpl = TSharedPtr<FImpl, ESPMode::ThreadSafe>;

public:
	static TIoStatusOr<FIASHostGroup> Create(FName Name, TConstArrayView<FAnsiString> HostUrls);
	static TIoStatusOr<FIASHostGroup> Create(FName Name, TConstArrayView<FString> HostUrls);

	FIASHostGroup();
	FIASHostGroup(FName Name, FAnsiStringView TestPath);
	FIASHostGroup(FName Name, FOnDemandHostGroup&& HostGroup);
	~FIASHostGroup() = default;

	FName GetName() const;
	const FAnsiString& GetTestPath() const;

	bool IsResolved() const;
	bool IsConnected() const;

	FIoStatus Resolve(TConstArrayView<FAnsiString> HostUrls);
	FIoStatus Resolve(TConstArrayView<FString> HostUrls);
	
	void Connect(int32 HostIndex);
	void Disconnect();

	enum class EReconnectionResult : uint8
	{
		AlreadyConnected = 0,
		Reconnected,
		FailedToConnect,
	};

	EReconnectionResult AttemptReconnection(uint32 TimeoutMs, std::atomic_bool& CancellationToken);

	void OnSuccessfulResponse();
	bool OnFailedResponse();

	FAnsiStringView GetPrimaryHostUrl() const;
	int32 GetPrimaryHostIndex() const;

	const FOnDemandHostGroup& GetUnderlyingHostGroup() const;

	TConstArrayView<FAnsiString> GetHostUrls() const;
private:
	FIASHostGroup(FSharedImpl&& Impl);

	void SetPrimaryHost(int32 Index) const;

	FSharedImpl Impl;
	
};

class FHostGroupManager final
{
	FHostGroupManager() =default;
	~FHostGroupManager() = default;

public:

	static FHostGroupManager& Get(); 

	// TODO: Note that currently we have no way to remove FIASHostGroups

	TIoStatusOr<FIASHostGroup> Register(FName Name, FAnsiStringView TestPath); // <- Unresolved
	TIoStatusOr<FIASHostGroup> Register(FName Name, TConstArrayView<FAnsiString> HostUrls); // <- Resolved

	FIASHostGroup Find(FName Name);

	void ForEachHostGroup(TFunctionRef<void (const FIASHostGroup&)> Callback) const;

	void Tick(uint32 TimeoutMs, std::atomic_bool& CancellationToken);

	void DisconnectAll(); 
	uint32 GetNumDisconnctedHosts() const;

private:

	TArray<FIASHostGroup> HostGroups;

	mutable UE::FMutex Mutex;
};

} //namespace UE::IoStore

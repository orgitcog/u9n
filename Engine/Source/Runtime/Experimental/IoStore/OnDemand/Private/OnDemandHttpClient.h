// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/AnsiString.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "IO/Http/Client.h"
#include "IO/IoBuffer.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "IO/OnDemandHostGroup.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

// When NO_LOGGING is enabled, then log categories will change type to 'FNoLoggingCategory' which does
// not inherit from 'FLogCategoryBase' so in order to keep code compiling we need to be able to switch
// types as well.
#if !NO_LOGGING
	#define UE_LOG_CATEGORY_TYPE FLogCategoryBase
#else
	#define UE_LOG_CATEGORY_TYPE FNoLoggingCategory
#endif //!NO_LOGGING

namespace UE::IoStore
{

FIoStatus LoadDefaultHttpCertificates(bool& bWasLoaded); 
inline bool IsHttpStatusOk(uint32 StatusCode)
{
	return StatusCode > 199 && StatusCode < 300;
}

enum class EHttpRedirects
{
	/** Redirects will be rejected and handled as failed requests. */
	Disabled,
	/** Follow redirects automatically. */
	Follow
};

struct FMultiEndpointHttpClientConfig
{
	int32						MaxConnectionCount = 4;
	int32						ReceiveBufferSize = -1;
	int32						MaxRetryCount = -1;	// Positive: The number of times to retry a failed request
													// Zero: Failed requests will not be retried
													// Negative: A failed request will retry once per provided host url
	int32						TimeoutMs = 0;
	EHttpRedirects				Redirects = EHttpRedirects::Follow;
	bool						bEnableThreadSafetyChecks = false;
	bool						bAllowChunkedTransfer = true;
	/** Logging will be disabled if this is set to a nullptr, it is up to the calling system to assign a LogCategory */
	const UE_LOG_CATEGORY_TYPE*	LogCategory = nullptr;
	ELogVerbosity::Type			LogVerbosity = ELogVerbosity::Log;
};

struct FMultiEndpointHttpClientResponse
{
	const bool	IsOk() const
	{
		return IsHttpStatusOk(StatusCode);
	}

	FIoBuffer					Body;
	FString						Reason;
	HTTP::FTicketPerf::FSample	Sample;
	uint64						DurationMilliseconds = 0;
	uint32						StatusCode = 0;
	uint32						RetryCount = 0;
	int32						HostIndex = INDEX_NONE;
	int8						CDNCacheStatus = -1;
};

/**
 * Todo - More documentation on the client behavior.
 * 
 * Retry policy:
 * If a request fails then the client will retry it up to FMultiEndpointHttpClientConfig::MaxRetryCount
 * times. The first retry attempt will use the primary host with each subsequent attempt cycling to the
 * next host in the FOnDemandHostGroup. If the end of the group is reached with retries remaining then
 * the cycle will begin again at the start of the group.
 * @see FOnDemandHostGroup for more info on how the host cycling works.
 */
class FMultiEndpointHttpClient
{
public:
	UE_NONCOPYABLE(FMultiEndpointHttpClient);

	using FOnHttpResponse = TFunction<void(FMultiEndpointHttpClientResponse&&)>;

	[[nodiscard]] static TUniquePtr<FMultiEndpointHttpClient> Create(const FMultiEndpointHttpClientConfig& Config);

	/** Blocking method */
	[[nodiscard]] static TIoStatusOr<FMultiEndpointHttpClientResponse> Get(FAnsiStringView Url, const FMultiEndpointHttpClientConfig& Config);

	void Get(const FOnDemandHostGroup& HostGroup, FAnsiStringView RelativeUrl, FOnHttpResponse&& OnResponse);
	void Get(const FOnDemandHostGroup& HostGroup, FAnsiStringView RelativeUrl, const FIoOffsetAndLength& ChunkRange, FOnHttpResponse&& OnResponse);

	bool Tick(int32 WaitTimeMs, uint32 MaxKiBPerSecond);
	bool Tick() { return Tick(-1, 0); }

	void UpdateConnections();

private:
	using FHttpConnectionPools = TArray<TUniquePtr<HTTP::FConnectionPool>, TInlineAllocator<4>>;

	struct FConnection
	{
		FOnDemandHostGroup		HostGroup;
		FHttpConnectionPools	Pools;
		int32					CurrentHost = INDEX_NONE;
	};

	struct FRequest
	{
		FOnHttpResponse		OnResponse;
		FAnsiString			RelativeUrl;
		FIoOffsetAndLength	Range;
		FConnection&		Connection;
		FIoBuffer			Body;
		uint64				StartTime = 0;
		uint32				RetryCount = 0;
		uint32				StatusCode = 0;
		int32				Host = INDEX_NONE;
		int8				CDNCacheStatus = -1;
	};

										FMultiEndpointHttpClient(const FMultiEndpointHttpClientConfig& Config);
	void								IssueRequest(FRequest&& Request);
	void								RetryRequest(FRequest&& Request);

	TUniquePtr<HTTP::FConnectionPool>	CreateConnection(FAnsiStringView HostUrl) const;
	FConnection&						GetConnection(const FOnDemandHostGroup& HostGroup);
	FConnection*						FindConnection(const FOnDemandHostGroup& HostGroup);

	uint32								GetRetryLimitForRequest(const FRequest& Request) const;

	void								Log(const FRequest& Request) const;

	FMultiEndpointHttpClientConfig		Config;
	TArray<TUniquePtr<FConnection>>		Connections;
	HTTP::FEventLoop					EventLoop;
	TArray<FRequest>					Retries;

#if UE_HTTPCLIENT_THREADSAFE_CHECKS
	uint32							OwningThread = 0;
#endif //UE_HTTPCLIENT_THREADSAFE_CHECKS
};

} // namespace UE::IoStore

#undef UE_LOG_CATEGORY_TYPE

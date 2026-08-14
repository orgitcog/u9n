// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandHttpClient.h"

#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "IO/IoStoreOnDemand.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#if UE_HTTPCLIENT_THREADSAFE_CHECKS
	#define UE_CHECK_THREADSAFETY(MethodName) checkf(OwningThread == 0 || OwningThread == FPlatformTLS::GetCurrentThreadId() || !FGenericPlatformProcess::SupportsMultithreading(), #MethodName TEXT("was called on the on the wrong thread!"))
#else
	#define UE_CHECK_THREADSAFETY(MethodName)
#endif //UE_HTTPCLIENT_THREADSAFE_CHECKS

namespace UE::IoStore
{

static int8 TrackCdnCacheStats(const HTTP::FResponse& Response)
{
#if !UE_BUILD_TEST
	for (FAnsiStringView Header : { "X-Cache", "CF-Cache-Status" })
	{
		FAnsiStringView Value = Response.GetHeader(Header);
		if (Value.IsEmpty())
		{
			continue;
		}

		return Value.Find("HIT", 0, ESearchCase::IgnoreCase) >= 0 ? 1 : 0;
	}
#endif

	return -1;
}

static const TCHAR* CDNCacheStatusToString(int8 Status)
{
	return Status > 0 ? TEXT("HIT") : Status == 0 ? TEXT("MISS") : TEXT("???");
}

FIoStatus LoadDefaultHttpCertificates(bool& bWasLoaded)
{
	using namespace UE::IoStore::HTTP;
	
	bWasLoaded = false;

	static struct FDefaultCerts
	{
		FDefaultCerts(bool& bWasLoadedd)
		{
			bWasLoadedd = true;
			Status = FIoStatus::Ok;

			// The following config option is used when staging to copy root certs PEM
			const TCHAR* CertSection = TEXT("/Script/Engine.NetworkSettings");
			const TCHAR* CertKey = TEXT("n.VerifyPeer");

			bool bVerifyPeer = false;
			if (GConfig != nullptr)
			{
				GConfig->GetBool(CertSection, CertKey, bVerifyPeer, GEngineIni);
			}

			// Open the certs file
			IFileManager& Ifm = IFileManager::Get();
			const FString PemPath = FPaths::EngineContentDir() / TEXT("Certificates/ThirdParty/cacert.pem");
			TUniquePtr<FArchive> Reader(Ifm.CreateFileReader(*PemPath));

			if (Reader.IsValid())
			{
				// Buffer certificate data
				const uint64 Size = Reader->TotalSize();
				FIoBuffer PemData(Size);
				FMutableMemoryView PemView = PemData.GetMutableView();
				Reader->Serialize(PemView.GetData(), Size);

				// Load the certs
				FCertRoots CaRoots(PemData.GetView());

				const uint32 NumCerts = CaRoots.Num();
				FCertRoots::SetDefault(MoveTemp(CaRoots));

				UE_LOG(LogIoStoreOnDemand, Display, TEXT("Loaded %u certificates from '%s'"), NumCerts, *PemPath);
			}
			else if (bVerifyPeer)
			{
				Status = FIoStatusBuilder(EIoErrorCode::FileNotOpen) << TEXT("Failed to open certificates file '") << PemPath << TEXT("'");
			}
		}

		FIoStatus Status;
	} DefaultCerts(bWasLoaded);

	return DefaultCerts.Status;
}

FMultiEndpointHttpClient::FMultiEndpointHttpClient(const FMultiEndpointHttpClientConfig& InConfig)
	: Config(InConfig)
{
	EventLoop.SetFailTimeout(Config.TimeoutMs);

#if UE_HTTPCLIENT_THREADSAFE_CHECKS
	if (Config.bEnableThreadSafetyChecks)
	{
		OwningThread = FPlatformTLS::GetCurrentThreadId();
	}
#endif //UE_HTTPCLIENT_THREADSAFE_CHECKS
}

TUniquePtr<FMultiEndpointHttpClient> FMultiEndpointHttpClient::Create(const FMultiEndpointHttpClientConfig& Config)
{
	return TUniquePtr<FMultiEndpointHttpClient>(new FMultiEndpointHttpClient(Config));
}

TIoStatusOr<FMultiEndpointHttpClientResponse> FMultiEndpointHttpClient::Get(FAnsiStringView Url, const FMultiEndpointHttpClientConfig& Config)
{
	using namespace UE::IoStore::HTTP;

	FEventLoop::FRequestParams Params = FEventLoop::FRequestParams
	{
		.bAutoRedirect = Config.Redirects == EHttpRedirects::Follow
	};

	FEventLoop Loop;
	FIoBuffer Body;
	TStringBuilder<128> Reason;
	uint32 StatusCode = 0;

	const uint32 MaxAttempts = Config.MaxRetryCount == -1 ? 3u : static_cast<uint32>(Config.MaxRetryCount);

	const uint64 StartTime = FPlatformTime::Cycles64();

	for (uint32 Attempt = 0; Attempt <= MaxAttempts; ++Attempt)
	{
		Loop.Send(Loop.Request("GET", Url, &Params), [&Body, &Reason, &StatusCode](const FTicketStatus& Status)
			{
				if (Status.GetId() == FTicketStatus::EId::Response)
				{
					Status.GetResponse().SetDestination(&Body);
					StatusCode = Status.GetResponse().GetStatusCode();
					return;
				}

				if (Status.GetId() == FTicketStatus::EId::Error)
				{
					Reason << Status.GetError().Reason;
				}
			});

		while (Loop.Tick(-1))
		{
			// Busy loop
		}

		if (IsHttpStatusOk(StatusCode))
		{
			FMultiEndpointHttpClientResponse Response
			{
				.Body = MoveTemp(Body),
				.DurationMilliseconds = uint64(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime)),
				.StatusCode = StatusCode,
				.RetryCount = Attempt,
			};

			return Response;
		}
	}

	if (Reason.Len() == 0)
	{
		Reason << TEXT("StatusCode: ") << StatusCode;
	}

	return FIoStatus(EIoErrorCode::ReadError, Reason.ToView());
}

void FMultiEndpointHttpClient::Get(const FOnDemandHostGroup& Endpoint, FAnsiStringView RelativeUrl, FOnHttpResponse&& OnResponse)
{
	FIoOffsetAndLength DefaultChunkRange(0,0);
	Get(Endpoint, RelativeUrl, DefaultChunkRange, MoveTemp(OnResponse));
}

void FMultiEndpointHttpClient::Get(const FOnDemandHostGroup& Endpoint, FAnsiStringView RelativeUrl, const FIoOffsetAndLength& ChunkRange, FOnHttpResponse&& OnResponse)
{
	UE_CHECK_THREADSAFETY(FMultiEndpointHttpClient::Get);

	FConnection& Connection = GetConnection(Endpoint);

	IssueRequest(FRequest
	{
		.OnResponse		= MoveTemp(OnResponse),
		.RelativeUrl	= FAnsiString(RelativeUrl),
		.Range			= ChunkRange,
		.Connection		= Connection,
		.StartTime		= FPlatformTime::Cycles64(),
		.Host			= Connection.CurrentHost
	});
}

bool FMultiEndpointHttpClient::Tick(int32 WaitTimeMs, uint32 MaxKiBPerSecond)
{
	UE_CHECK_THREADSAFETY(FMultiEndpointHttpClient::Tick);

	EventLoop.Throttle(MaxKiBPerSecond);
	const uint32 TicketCount = EventLoop.Tick(WaitTimeMs);

	if (Retries.IsEmpty() == false)
	{
		const int32 RequestCount = FMath::Min(Retries.Num(), int32(HTTP::FEventLoop::MaxActiveTickets - TicketCount));
		for (int32 Idx = 0; Idx < RequestCount; Idx++)
		{
			IssueRequest(MoveTemp(Retries[Idx]));
		}
		Retries.RemoveAtSwap(0, RequestCount);
	}

	const bool bIsIdle = EventLoop.IsIdle();
	if (bIsIdle)
	{
		// Destroy all non active connection pool(s)
		for (TUniquePtr<FConnection>& Connection : Connections)
		{
			for (int32 Idx = 0, Count = Connection->Pools.Num(); Idx < Count; ++Idx)
			{
				if (Idx != Connection->CurrentHost)
				{
					Connection->Pools[Idx].Reset();
				}
			}
		}
	}

	return bIsIdle == false;
}

void FMultiEndpointHttpClient::UpdateConnections()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMultiEndpointHttpClient::UpdateConnections);

	UE_CHECK_THREADSAFETY(FMultiEndpointHttpClient::UpdateConnections);

	for (TUniquePtr<FConnection>& Connection : Connections)
	{
		check(Connection.IsValid());

		Connection->CurrentHost = Connection->HostGroup.PrimaryHostIndex();

		if (Connection->CurrentHost != INDEX_NONE)
		{
			if (Connection->Pools[Connection->CurrentHost].IsValid() == false)
			{
				Connection->Pools[Connection->CurrentHost] = CreateConnection(Connection->HostGroup.PrimaryHost());
			}
		}
	}
}

void FMultiEndpointHttpClient::IssueRequest(FRequest&& Request)
{
	UE_CHECK_THREADSAFETY(FMultiEndpointHttpClient::IssueRequest);

	using namespace UE::IoStore::HTTP;

	check(Request.Connection.HostGroup.IsEmpty() == false);
	check(Request.Connection.Pools[Request.Connection.CurrentHost].IsValid());

	FAnsiStringView Url				= Request.RelativeUrl;
	FConnectionPool& ConnectionPool = *Request.Connection.Pools[Request.Connection.CurrentHost];

	auto HttpSink = [this, Request = MoveTemp(Request)](const FTicketStatus& TicketStatus) mutable
	{
		const uint64 DurationMs = uint64(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Request.StartTime));

		switch (TicketStatus.GetId())
		{
			case FTicketStatus::EId::Response:
			{
				FResponse& HttpResponse = TicketStatus.GetResponse();
				Request.StatusCode		= HttpResponse.GetStatusCode();
				HttpResponse.SetDestination(&Request.Body);
				Request.CDNCacheStatus = TrackCdnCacheStats(HttpResponse);
				break;
			}
			case FTicketStatus::EId::Content:
			case FTicketStatus::EId::Error:
			case FTicketStatus::EId::Cancelled:
			{
				Log(Request);

				const bool bError		= TicketStatus.GetId() == FTicketStatus::EId::Error;
				const bool bServerError = Request.StatusCode > 499 && Request.StatusCode < 600;

				if ((bError || bServerError) && Request.RetryCount < GetRetryLimitForRequest(Request))
				{
					RetryRequest(MoveTemp(Request));
				}
				else
				{
					
					FMultiEndpointHttpClientResponse Response
					{
						.Body					= MoveTemp(Request.Body),
						.DurationMilliseconds	= DurationMs,
						.StatusCode				= Request.StatusCode,
						.RetryCount				= Request.RetryCount,
						.HostIndex				= Request.Host,
						.CDNCacheStatus			= Request.CDNCacheStatus
					};

					if (TicketStatus.GetId() == FTicketStatus::EId::Content)
					{
						Response.Sample = TicketStatus.GetPerf().GetSample();
					}

					if (bError)
					{
						Response.Reason = TicketStatus.GetError().Reason;
					}
					else if (TicketStatus.GetId() == FTicketStatus::EId::Cancelled)
					{
						Response.Reason = TEXT("Cancelled");
					}

					FOnHttpResponse OnResponse = MoveTemp(Request.OnResponse);
					OnResponse(MoveTemp(Response));
				}
				break;
			}
		}
	};

	FEventLoop::FRequestParams RequestParams = 
		{
			.ContentSizeEst = uint32(Request.Range.GetLength()),
			.bAutoRedirect = Config.Redirects == EHttpRedirects::Follow,
			.bAllowChunked = Config.bAllowChunkedTransfer
		};

	UE::IoStore::HTTP::FRequest HttpRequest = EventLoop.Get(Url, ConnectionPool, &RequestParams);

#if !UE_BUILD_TEST
	HttpRequest.Header("Pragma", "akamai-x-cache-on");
#endif

	if (Request.Range.GetOffset() > 0 || Request.Range.GetLength() > 0)
	{
		HttpRequest.Header(ANSITEXTVIEW("Range"),
			WriteToAnsiString<64>(ANSITEXTVIEW("bytes="), Request.Range.GetOffset(), ANSITEXTVIEW("-"), Request.Range.GetOffset() + Request.Range.GetLength() - 1));
	}

	EventLoop.Send(MoveTemp(HttpRequest), MoveTemp(HttpSink));
}

void FMultiEndpointHttpClient::RetryRequest(FRequest&& Request)
{
	UE_CHECK_THREADSAFETY(FMultiEndpointHttpClient::RetryRequest);

	FConnection& Connection = Request.Connection;

	// Try a different host URL after the first retry
	if (Request.RetryCount > 0 && Request.Host == Request.Connection.CurrentHost)
	{
		FAnsiStringView HostUrl = Request.Connection.HostGroup.CycleHost(Connection.CurrentHost);
		if (Connection.Pools[Connection.CurrentHost].IsValid() == false)
		{
			Connection.Pools[Connection.CurrentHost] = CreateConnection(HostUrl);
		}
	}

	Request.StatusCode = 0;
	Request.RetryCount++;
	Request.Host = Connection.CurrentHost;
	Retries.Emplace(MoveTemp(Request));
}

TUniquePtr<HTTP::FConnectionPool> FMultiEndpointHttpClient::CreateConnection(FAnsiStringView HostUrl) const
{
	HTTP::FConnectionPool::FParams Params;
	ensure(Params.SetHostFromUrl(HostUrl) >= 0);
	if (Config.ReceiveBufferSize >= 0)
	{
		Params.RecvBufSize = Config.ReceiveBufferSize;
	}
	Params.ConnectionCount = uint16(Config.MaxConnectionCount);

	return MakeUnique<HTTP::FConnectionPool>(Params);
}

FMultiEndpointHttpClient::FConnection& FMultiEndpointHttpClient::GetConnection(const FOnDemandHostGroup& HostGroup)
{
	UE_CHECK_THREADSAFETY(FMultiEndpointHttpClient::GetConnection);

	for (TUniquePtr<FConnection>& Conn : Connections)
	{
		check(Conn.IsValid());
		if (Conn->HostGroup == HostGroup)
		{
			return *Conn;
		}
	}

	FConnection& Conn = *Connections.Emplace_GetRef(new FConnection
	{
		.HostGroup	= HostGroup,
	});

	Conn.Pools.SetNum(HostGroup.Hosts().Num());
	Conn.CurrentHost = HostGroup.PrimaryHostIndex();

	Conn.Pools[Conn.CurrentHost] = CreateConnection(HostGroup.PrimaryHost());
	return Conn;
}

FMultiEndpointHttpClient::FConnection* FMultiEndpointHttpClient::FindConnection(const FOnDemandHostGroup& HostGroup)
{
	UE_CHECK_THREADSAFETY(FMultiEndpointHttpClient::FindConnection);

	for (TUniquePtr<FConnection>& Conn : Connections)
	{
		check(Conn.IsValid());
		if (Conn->HostGroup == HostGroup)
		{
			return Conn.Get();
		}
	}

	return nullptr;
}

uint32 FMultiEndpointHttpClient::GetRetryLimitForRequest(const FRequest& Request) const
{
	return Config.MaxRetryCount == -1 ? Request.Connection.HostGroup.Hosts().Num() : Config.MaxRetryCount;
}

void FMultiEndpointHttpClient::Log(const FRequest& Request) const
{
#if !NO_LOGGING
	if (Config.LogCategory == nullptr || Config.LogCategory->IsSuppressed(Config.LogVerbosity))
	{
		return;
	}

	const uint64 DurationMs = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Request.StartTime);
	const uint64 Size = Request.Body.GetSize() >> 10;

	FMsg::Logf(__FILE__, __LINE__, Config.LogCategory->GetCategoryName(), Config.LogVerbosity,
		TEXT("http-%3u: %5" UINT64_FMT "ms %5" UINT64_FMT "KiB [%4s] %s (Attempt %u/%u)"),
		Request.StatusCode,
		DurationMs,
		Size,
		CDNCacheStatusToString(Request.CDNCacheStatus),
		WriteToString<512>(Request.Connection.HostGroup.Host(Request.Host), Request.RelativeUrl).ToString(),
		Request.RetryCount,
		GetRetryLimitForRequest(Request)
	);

#endif //!NO_LOGGING
}

} // namespace UE::IoStore

#undef UE_CHECK_THREADSAFETY

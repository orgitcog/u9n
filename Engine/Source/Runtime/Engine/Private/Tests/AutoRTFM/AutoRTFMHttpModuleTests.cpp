// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Misc/AutomationTest.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "AutoRTFM.h"
#include "Containers/Array.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/MemoryReader.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The HTTP test module is a simple wrapper which provides delegates to track the number of active requests,
 * and allows us to block until all requests complete. The native API is asynchronous, and these helpers make
 * it easier to construct simple test cases.
 */

class FHttpTestModule : public FHttpModule
{
public:
	FHttpTestModule()
	{
		StartupModule();

		GetHttpManager().SetRequestAddedDelegate(FHttpManagerRequestAddedDelegate::CreateLambda([this](const FHttpRequestRef&) -> void
		{
			++ActiveRequests;
		}));

		GetHttpManager().SetRequestCompletedDelegate(FHttpManagerRequestCompletedDelegate::CreateLambda([this](const FHttpRequestRef&) -> void
		{
			check(ActiveRequests > 0);
			--ActiveRequests;
		}));
	}

	~FHttpTestModule()
	{
		ShutdownModule();
	}

	int NumActiveRequests()
	{
		return ActiveRequests;
	}

	bool BlockOnActiveRequests()
	{
		// Tick the HTTP manager until all requests have been processed.
		// Returns true if active requests all drain, or false if a timeout occurs.
		constexpr float TicksPerSecond = 60.0f;
		constexpr float SecondsBeforeTimeout = 10.0f;
		constexpr float MaximumTicksToWait = SecondsBeforeTimeout * TicksPerSecond;
		for (float Tick = 0.0f; Tick < MaximumTicksToWait; Tick += 1.0f)
		{
			FPlatformProcess::Sleep(1.0f / TicksPerSecond);
			GetHttpManager().Tick(1.0f / TicksPerSecond);
			if (ActiveRequests == 0)
			{
				return true;
			}
		}

		return false;
	}

private:
	int ActiveRequests = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMHttpModuleNetworking, "AutoRTFM + HttpModuleNetworking", \
								 EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | \
								 EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMHttpModuleNetworking::RunTest(const FString& Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMHttpModuleTests' test. AutoRTFM disabled.")));
		return true;
	}

	// This helper function requests a small file that exists on Horde and should be reachable from any Epic developer or build machine.
	// If this file no longer exists in the future, we can update the test to point to a new file.
	// If this ends up being flaky, we can run a local FHttpServer inside AutoRTFMEngineTests to respond to our test requests instead.
	auto IssueHttpRequest = [&](FHttpModule& Module) -> void
	{
		TSharedRef<IHttpRequest> HttpRequest = Module.CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->SetURL(TEXT("https://www.epicgames.com/favicon.ico"));
		HttpRequest->OnProcessRequestComplete().BindLambda([&](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			TestTrue("HTTP request was processed successfully", bSucceeded);
			TestTrueExpr(HttpResponse != nullptr);
			TestTrueExpr(HttpResponse->GetResponseCode() == 200 ||
						 HttpResponse->GetResponseCode() == 301 ||
						 HttpResponse->GetResponseCode() == 307 ||
						 HttpResponse->GetResponseCode() == 403 ||
						 HttpResponse->GetResponseCode() == 404);
		});
		HttpRequest->ProcessRequest();
	};

	// Verify that HttpModule can create a request normally.
	FHttpTestModule Module;
	IssueHttpRequest(Module);
	TestTrue("HTTP requests are issued immediately outside of a transaction", Module.NumActiveRequests() == 1);

	// Verify that it is safe for an HTTP request to go out of scope within an AutoRTFM transaction
	// without ever being issued.
	AutoRTFM::Transact([&]
	{
		TSharedRef<IHttpRequest> HttpRequest = Module.CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->SetURL(TEXT("https://www.unrealengine.com/en-US"));
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Commit([&]
	{
		TSharedRef<IHttpRequest> HttpRequest = Module.CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->SetURL(TEXT("https://www.epicgames.com/unrealtournament"));
	});

	// Verify that HttpModule can create a request from within AutoRTFM transaction that is aborted.
	AutoRTFM::Transact([&]
	{
		IssueHttpRequest(Module);
		TestTrue("HTTP requests are deferred inside a transaction", Module.NumActiveRequests() == 1);
		AutoRTFM::AbortTransaction();
	});

	TestTrue("HTTP requests are abandoned on abort", Module.NumActiveRequests() == 1);

	// Verify that HttpModule can issue a request from within AutoRTFM transaction that is committed.
	AutoRTFM::Commit([&]
	{
		IssueHttpRequest(Module);
		TestTrue("HTTP requests are deferred inside a transaction", Module.NumActiveRequests() == 1);
	});

	// After a transaction is committed, our pending request should be materialized.
	TestTrue("HTTP requests are issued when transaction commits", Module.NumActiveRequests() == 2);

	// Allow the requests to complete.
	TestTrue("HTTP requests complete normally without timing out", Module.BlockOnActiveRequests());
	TestTrue("HTTP requests fully complete", Module.NumActiveRequests() == 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMHttpRequestPreservesSettings, "AutoRTFM + HttpRequestPreservesSettings", \
								 EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | \
								 EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMHttpRequestPreservesSettings::RunTest(const FString& Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMHttpModuleTests' test. AutoRTFM disabled.")));
		return true;
	}

	FHttpTestModule Module;

	// Verify each setting that is handled by a transactionally-safe HTTP request.
	// It's important to test that data is preserved both inside the transaction, and after the transaction commits.
	// While inside the transaction, the data is held in a FClosedHttpRequest.
	// After the transaction commits, the data is held in a platform-specific FHttpRequest.
	auto DoCheck = [&](auto SetterFn, auto GetterFn) -> void
	{
		// Test that changes are reflected both before and after a transactional commit.
		{
			TSharedPtr<IHttpRequest> Request;
			AutoRTFM::Commit([&]
			{
				Request = Module.CreateRequest();
				SetterFn(*Request);  // Sets data on a FClosedHttpRequest
				GetterFn(*Request);  // Gets data on a FClosedHttpRequest
			});
			GetterFn(*Request);      // Gets data on the platform-specific FHttpRequest
		}
	};

	// URL and URL parameters
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetURL(TEXT("https://www.fortnite.com/?lang=en-US"));
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetURL() == FString("https://www.fortnite.com/?lang=en-US"));
			TestTrueExpr(Request.GetEffectiveURL() == FString());
			TestTrueExpr(Request.GetURLParameter(TEXT("lang")) == FString("en-US"));
		});

	// Verb
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetVerb(TEXT("POST"));
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetVerb() == FString("POST"));
		});

	// Headers
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetHeader(TEXT("Cat"), TEXT("Meow"));
			Request.SetHeader(TEXT("Dog"), TEXT("Woof"));
			Request.SetHeader(TEXT("Cow"), TEXT("Moo"));
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetHeader(TEXT("Dog")) == FString("Woof"));
			TestTrueExpr(Request.GetHeader(TEXT("Cow")) == FString("Moo"));
			TestTrueExpr(Request.GetHeader(TEXT("Cat")) == FString("Meow"));
			TestTrueExpr(Request.GetHeader(TEXT("Fox")) == FString());
		});

	// Options
	DoCheck(
		[&](IHttpRequest& Request)
		{
#if UE_HTTP_SUPPORT_UNIX_SOCKET
			Request.SetOption(HttpRequestOptions::UnixSocketPath, "MyUnixSocketPath");
#endif
		},
		[&](IHttpRequest& Request)
		{
#if UE_HTTP_SUPPORT_UNIX_SOCKET
			TestTrueExpr(Request.GetOption(HttpRequestOptions::UnixSocketPath) == FString("MyUnixSocketPath"));
#endif
			TestTrueExpr(Request.GetOption(FName()) == FString());
		});

	// SetContent(TArray<uint8>)
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetContent(TArray<uint8>{'A', 'B', 'C'});
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetContent() == TArray<uint8>{'A', 'B', 'C'});
		});

	// SetContentAsString
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetContentAsString(TEXT("Strings!"));
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetContent() == TArray<uint8>{'S', 't', 'r', 'i', 'n', 'g', 's', '!'});
		});

	// SetContentAsStreamedFile
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetContentAsStreamedFile(TEXT("C:\\HttpRequestTest.txt"));
		},
		[&](IHttpRequest& Request)
		{
			// (No matching getter exists here. A thorough test would need to issue a POST request and verify the contents match.)
		});

	// SetContentFromStream
	DoCheck(
		[&](IHttpRequest& Request)
		{
			TArray<uint8> StreamData{'A', 'B', 'C'};
			Request.SetContentFromStream(MakeShared<FMemoryReader>(StreamData));
		},
		[&](IHttpRequest& Request)
		{
			// (No matching getter exists here. A thorough test would need to issue a POST request and verify the contents match.)
		});

	// SetTimeout
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetTimeout(2.5f);
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetTimeout() == 2.5f);
		});

	// ClearTimeout
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetTimeout(2.5f);
			Request.ClearTimeout();
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(!Request.GetTimeout().IsSet());
		});

	// SetActivityTimeout
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetActivityTimeout(2.5f);
		},
		[&](IHttpRequest& Request)
		{
			// (No matching getter exists here.)
		});

	// SetDelegateThreadPolicy
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetDelegateThreadPolicy() == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		});

	// OnProcessRequestComplete
	bool bCalledDelegate;
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.OnProcessRequestComplete().BindLambda([&](FHttpRequestPtr, FHttpResponsePtr, bool) { bCalledDelegate = true; });
		},
		[&](IHttpRequest& Request)
		{
			bCalledDelegate = false;
			Request.OnProcessRequestComplete().Execute(nullptr, nullptr, true);
			TestTrueExpr(bCalledDelegate);
		});

	// OnRequestProgress64
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.OnRequestProgress64().BindLambda([&](FHttpRequestPtr, uint64, uint64) { bCalledDelegate = true; });
		},
		[&](IHttpRequest& Request)
		{
			bCalledDelegate = false;
			Request.OnRequestProgress64().Execute(nullptr, 0, 0);
			TestTrueExpr(bCalledDelegate);
		});

	// OnRequestWillRetry
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.OnRequestWillRetry().BindLambda([&](FHttpRequestPtr, FHttpResponsePtr, float) { bCalledDelegate = true; });
		},
		[&](IHttpRequest& Request)
		{
			bCalledDelegate = false;
			Request.OnRequestWillRetry().Execute(nullptr, nullptr, 0.0f);
			TestTrueExpr(bCalledDelegate);
		});

	// OnHeaderReceived
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.OnHeaderReceived().BindLambda([&](FHttpRequestPtr, const FString&, const FString&) { bCalledDelegate = true; });
		},
		[&](IHttpRequest& Request)
		{
			bCalledDelegate = false;
			Request.OnHeaderReceived().Execute(nullptr, FString(), FString());
			TestTrueExpr(bCalledDelegate);
		});

	// OnStatusCodeReceived
	DoCheck(
		[&](IHttpRequest& Request)
		{
			Request.OnStatusCodeReceived().BindLambda([&](FHttpRequestPtr, int32) { bCalledDelegate = true; });
		},
		[&](IHttpRequest& Request)
		{
			bCalledDelegate = false;
			Request.OnStatusCodeReceived().Execute(nullptr, 0);
			TestTrueExpr(bCalledDelegate);
		});

	// Ensure that default values match between FClosedHttpRequests and platform-specific FHttpRequests.
	DoCheck(
		[&](IHttpRequest& Request)
		{
		},
		[&](IHttpRequest& Request)
		{
			TestTrueExpr(Request.GetURL() == FString());
			TestTrueExpr(Request.GetEffectiveURL() == FString());
			TestTrueExpr(Request.GetVerb() == FString(TEXT("GET")));
			TestTrueExpr(Request.GetHeader(TEXT("Mystery")) == FString());
#if UE_HTTP_SUPPORT_UNIX_SOCKET
			TestTrueExpr(Request.GetOption(HttpRequestOptions::UnixSocketPath) == FString());
#endif
			TestTrueExpr(Request.GetContent() == TArray<uint8>{});
			TestTrueExpr(Request.GetStatus() == EHttpRequestStatus::NotStarted);
			TestTrueExpr(!Request.GetTimeout().IsSet());
			TestTrueExpr(Request.GetResponse() == nullptr);
			TestTrueExpr(Request.GetDelegateThreadPolicy() == EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
			TestTrueExpr(!Request.OnProcessRequestComplete().IsBound());
			TestTrueExpr(!Request.OnRequestProgress64().IsBound());
			TestTrueExpr(!Request.OnRequestWillRetry().IsBound());
			TestTrueExpr(!Request.OnHeaderReceived().IsBound());
			TestTrueExpr(!Request.OnStatusCodeReceived().IsBound());
		});

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMHttpRequestSafeToDelete, "AutoRTFM + HttpRequestSafeToDelete", \
								 EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | \
								 EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMHttpRequestSafeToDelete::RunTest(const FString& Parameters)
{
	FHttpTestModule Module;

	// Verify that it is safe for an HTTP request to be deleted inside a transaction before it is committed.
	AutoRTFM::Commit([&]
	{
		TSharedPtr<IHttpRequest> HttpRequest = Module.CreateRequest();
		HttpRequest->SetURL(TEXT("https://www.unrealengine.com/"));
		HttpRequest = nullptr;
	});

	// Verify that it is safe for an HTTP request to be deleted inside a transaction that is aborted.
	AutoRTFM::Transact([&]
	{
		TSharedPtr<IHttpRequest> HttpRequest = Module.CreateRequest();
		HttpRequest->SetURL(TEXT("https://www.unrealengine.com/"));
		HttpRequest = nullptr;
		AutoRTFM::AbortTransaction();
	});

	// Verify that it is safe for an HTTP request to be created inside a successful transaction, and then deleted
	// in the open.
	TSharedPtr<IHttpRequest> OuterRequest;
	AutoRTFM::Commit([&]
	{
		OuterRequest = Module.CreateRequest();
		OuterRequest->SetURL(TEXT("https://www.unrealengine.com/"));
	});
	OuterRequest = nullptr;

	return true;
}

#endif // #if WITH_DEV_AUTOMATION_TESTS

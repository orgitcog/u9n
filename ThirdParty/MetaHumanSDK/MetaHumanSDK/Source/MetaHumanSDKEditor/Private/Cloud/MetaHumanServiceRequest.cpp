// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloud/MetaHumanServiceRequest.h"
#include "Cloud/MetaHumanCloudServicesSettings.h"
#include "MetaHumanCloudAuthenticationInternal.h"
#include "MetaHumanDdcUtils.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpRetrySystem.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"

#include "HAL/PlatformFileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

#include "Logging/StructuredLog.h"
#include "Async/Async.h"
#include "Misc/Optional.h"

#include "JsonDomBuilder.h"
#include "Editor.h"


#define LOCTEXT_NAMESPACE "MetaHumanServiceClient"
DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanServiceClient, Log, All)

namespace UE::MetaHuman
{
	struct FRequestContextBase::FImpl
	{
		FString PollUri;		
		TSharedPtr<FRequestContextBase> Outer;
		TSharedPtr<FMetaHumanServiceRequestBase> RequestOwner;

		FImpl() = default;
		~FImpl() = default;
		void CheckRequestStatus();
	};

	FRequestContextBase::FRequestContextBase()
	{
		BaseImpl = MakePimpl<FImpl>();
	}

	namespace
	{		
		// we send all requests through the engine retry manager
		TSharedPtr<FHttpRetrySystem::FManager> RetryManager;
		TSet<int32> RetryCodes;	
	}

	namespace ServiceAuthentication
	{
		std::atomic<bool> bInitialised = false;
		std::atomic<bool> bLoginStatusChecked = false;
		std::atomic<bool> bLoggedIn;
		std::atomic<bool> bAuthBusy = false;
		std::atomic<bool> bShuttingDown = false;
		TSharedPtr<Authentication::FClient> AuthClient;
		TSharedPtr<TArray<uint8>> InternalNonProdData;
		
		void CompleteLogin(bool bLoginStatus)
		{
			bLoginStatusChecked = true;
			bLoggedIn = bLoginStatus;
			bAuthBusy = false;
		}

		void CheckCreateClient()
		{
			check(bInitialised);
			if (!AuthClient.IsValid())
			{
				const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
				const Authentication::EEosEnvironmentType EosEnvironmentType = (Settings->ServiceEnvironment == EMetaHumanCloudServiceEnvironment::GameDev ? Authentication::EEosEnvironmentType::GameDev : Authentication::EEosEnvironmentType::Prod);
				AuthClient = Authentication::FClient::CreateClient(EosEnvironmentType, InternalNonProdData ? InternalNonProdData->GetData() : nullptr);
			}
		}

		void InitialiseAuthEnvironment(TSharedPtr<TArray<uint8>> NonProdData)
		{
			check(!bShuttingDown);
			if (bInitialised)
			{
				// ok to call multiple times for now
				return;
			}

			InternalNonProdData = NonProdData;
			bLoginStatusChecked = false;
			bLoggedIn = false;
			bShuttingDown = false;
			bInitialised = true;
		}

		void ShutdownAuthEnvironment()
		{
			if (!bInitialised)
			{
				return;
			}
			bool bNotShuttingDown = false;
			if (bShuttingDown.compare_exchange_weak(bNotShuttingDown, true))
			{
				const bool bAuthWasBusy = bAuthBusy.exchange(true);
				if (bAuthWasBusy)
				{
					UE_LOGFMT(LogMetaHumanServiceClient, Warning, "Shutting down auth environment while authentication operation is in progress");
				}
				AuthClient.Reset();
				bInitialised = false;
				bShuttingDown = false;
			}
		}

		void CheckHasLoggedInUserAsync(FOnCheckHasLoggedInUserCompleteDelegate&& OnCheckHasLoggedInUserCompleteDelegate)
		{
			CheckCreateClient();

			if (bLoginStatusChecked)
			{
				OnCheckHasLoggedInUserCompleteDelegate.ExecuteIfBound(bLoggedIn);
			}
			else
			{
				bool bNotBusy = false;
				if (bAuthBusy.compare_exchange_strong(bNotBusy, true))
				{
					AuthClient->HasLoggedInUser(FOnCheckHasLoggedInUserCompleteDelegate::CreateLambda([OnCheckHasLoggedInUserCompleteDelegate = MoveTemp(OnCheckHasLoggedInUserCompleteDelegate)](bool bUserLoggedIn)
						{
							CompleteLogin(bUserLoggedIn);
							bAuthBusy = false;
							OnCheckHasLoggedInUserCompleteDelegate.ExecuteIfBound(bLoggedIn);
						}));
				}
				else
				{
					AsyncTask(ENamedThreads::Type::AnyBackgroundThreadNormalTask, [OnCheckHasLoggedInUserCompleteDelegate = Forward<FOnCheckHasLoggedInUserCompleteDelegate>(OnCheckHasLoggedInUserCompleteDelegate)] () mutable
					{
						while (!bShuttingDown && bAuthBusy)
						{
							FPlatformProcess::Sleep(0.75f);
						}
						if (!bShuttingDown)
						{
							// move to the game task since most of the delegates for this deal with UI or Rendering in some form
							AsyncTask(ENamedThreads::Type::GameThread, [OnCheckHasLoggedInUserCompleteDelegate = MoveTemp(OnCheckHasLoggedInUserCompleteDelegate)]() {
								OnCheckHasLoggedInUserCompleteDelegate.ExecuteIfBound(bLoggedIn);
							});
						}
					});
				}
			}
		}

		void LogoutFromAuthEnvironment(FOnLogoutCompleteDelegate&& OnLogoutCompleteDelegate)
		{
			CheckCreateClient();

			bool bNotBusy = false;
			if (bAuthBusy.compare_exchange_strong(bNotBusy, true))
			{				
				AuthClient->LogoutAsync(Authentication::FOnLogoutCompleteDelegate::CreateLambda([LogoutCompleteDelegate = MoveTemp(OnLogoutCompleteDelegate)](TSharedRef<Authentication::FClient>)
					{
						CompleteLogin(false);
						LogoutCompleteDelegate.ExecuteIfBound();
					}));
			}
		}

		void LoginToAuthEnvironment(FOnLoginCompleteDelegate&& OnLoginCompleteDelegate, FOnLoginFailedDelegate&& OnLoginFailedDelegate)
		{
			CheckCreateClient();

			bool bNotBusy = false;
			if (!bLoggedIn && bAuthBusy.compare_exchange_strong(bNotBusy, true))
			{
				// user is not logged in, we need to get a token before we can issue a request to whatever service we're dealing with
				AuthClient->LoginAsync(Authentication::FOnLoginCompleteDelegate::CreateLambda([LoginCompleteDelegate = MoveTemp(OnLoginCompleteDelegate)](TSharedRef<Authentication::FClient> EosAuthClient)
					{
						CompleteLogin(true);
						LoginCompleteDelegate.ExecuteIfBound();
					}),
				Authentication::FOnLoginFailedDelegate::CreateLambda([LoginFailedDelegate = MoveTemp(OnLoginFailedDelegate)]()
					{
						CompleteLogin(false);
						LoginFailedDelegate.ExecuteIfBound();
					}));
			}
		}

		void SetAuthHeader(FHttpRequestPtr Request)
		{
			bool bWasSet = false;
			if (AuthClient.IsValid())
			{
				bWasSet = AuthClient->SetAuthHeaderForUserBlocking(Request.ToSharedRef());
			}
			if (!bWasSet)
			{
				// force authentication by sending an invalid token
				Request->SetHeader(TEXT("Authorization"), TEXT("Bearer TOKEN"));
			}
		}
	}
	using namespace ServiceAuthentication;

	TSharedRef<IHttpRequest> CreateHttpRequest()
	{
		const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
		if (!RetryManager.IsValid())
		{
			RetryManager = MakeShared<FHttpRetrySystem::FManager>(Settings->RetryCount, Settings->Timeout);
			RetryCodes.Add(500);
			RetryCodes.Add(502);
			RetryCodes.Add(503);
			RetryCodes.Add(504);
		}
		const TSharedRef<IHttpRequest> HttpRequest = RetryManager->CreateRequest(Settings->RetryCount, Settings->Timeout, RetryCodes);
		HttpRequest->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
		return HttpRequest;
	}

	EMetaHumanServiceRequestResult HttpErrorReporterHelper(const int32 HttpErrorCode)
	{
		EMetaHumanServiceRequestResult Result = EMetaHumanServiceRequestResult::ServerError;
		switch (const int32 ErrorClass = HttpErrorCode / 100)
		{
		case 4:
			switch (HttpErrorCode)
			{
			case EHttpResponseCodes::Denied:
				Result = EMetaHumanServiceRequestResult::Unauthorized;
				break;
			case EHttpResponseCodes::Forbidden:
				Result = EMetaHumanServiceRequestResult::EulaNotAccepted;
				break;
			case EHttpResponseCodes::TooManyRequests:
				Result = EMetaHumanServiceRequestResult::Busy;
				break;
			default:
				Result = EMetaHumanServiceRequestResult::InvalidArguments;
				break;
			}
			break;
		case 5:
		{
			switch (HttpErrorCode)
			{
			case 502:
			case 504:
				Result = EMetaHumanServiceRequestResult::GatewayError;
				break;
			case 503:
				Result = EMetaHumanServiceRequestResult::Busy;
				break;
			default:
				break;
			}
		}
		break;
		case 2:
			Result = EMetaHumanServiceRequestResult::Ok;
			break;
		default:
			break;
		}
		return Result;
	}

	
	void FRequestContextBase::FImpl::CheckRequestStatus()
	{
		const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
		const TSharedRef<IHttpRequest> ProgressRequest = CreateHttpRequest();
		ProgressRequest->SetURL(PollUri + TEXT("?waitSeconds=") + FString::FromInt(Settings->LongPollTimeout));	
		ProgressRequest->SetVerb("GET");
		ProgressRequest->SetHeader("Content-Type", TEXT("application/json"));
		SetAuthHeader(ProgressRequest);

		ProgressRequest->OnProcessRequestComplete().BindLambda([this, Settings](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bCompletedOk)
			{
				int32 ResponseCode = bCompletedOk && Response ? Response->GetResponseCode() : -1;

				// while the service is processing the request it will keep returning OK (or fail)
				if (EHttpResponseCodes::IsOk(ResponseCode))
				{
					const FString ContentType = Response->GetHeader(TEXT("Content-Type"));
					if (ContentType.Equals(TEXT("application/octet-stream")))
					{
						// the work is complete
						RequestOwner->MetaHumanServiceRequestProgressDelegate.ExecuteIfBound(1.0f);
						RequestOwner->OnRequestCompleted(Response->GetContent(), Outer);
					}
					else
					{
						TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Response->GetContentAsString());
						TSharedPtr<FJsonObject> ResponseJson;
						if (FJsonSerializer::Deserialize(JsonReader, ResponseJson))
						{
							const FString Status = ResponseJson->GetStringField(TEXT("status"));
							if (Status.Equals(TEXT("QUEUED")))
							{
								RequestOwner->MetaHumanServiceRequestProgressDelegate.ExecuteIfBound(0.5f);
							}
							else if (Status.Equals(TEXT("RUNNING")))
							{
								RequestOwner->MetaHumanServiceRequestProgressDelegate.ExecuteIfBound(0.75f);
							}
							else if (Status.Equals(TEXT("FAILED")))
							{ 
								ResponseCode = 500;
							}

							if (EHttpResponseCodes::IsOk(ResponseCode))
							{
								// keep checking
								CheckRequestStatus();
							}
						}
						else
						{
							ResponseCode = 500;
						}
					}
				}

				if (EHttpResponseCodes::IsOk(ResponseCode) == false)
				{
					RequestOwner->OnRequestFailed(HttpErrorReporterHelper(ResponseCode), Outer);
				}
			});
		ProgressRequest->ProcessRequest();
	}

	
	void FMetaHumanServiceRequestBase::ExecuteRequestAsync(FRequestContextBasePtr Context)
	{
		// situations might occur where the request object is being destroyed when we get here through a lambda, so guard against it
		if (!DoesSharedInstanceExist())
		{
			return;
		}
		if (TSharedPtr<IHttpRequest> HttpRequest = CreateRequest(Context))
		{
			OnMetaHumanServiceRequestBeginDelegate.ExecuteIfBound();
			SetAuthHeader(HttpRequest);
			HttpRequest->ProcessRequest();
		}
	}

	TSharedPtr<IHttpRequest> FMetaHumanServiceRequestBase::CreateRequest(FRequestContextBasePtr Context)
	{
		const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
		const TSharedRef<IHttpRequest> HttpRequest = CreateHttpRequest();

		{
			// Set up the context (or create one) with the information we need for polling etc.
			if (!Context.IsValid())
			{
				// we always need a context even if the caller doesn't
				Context = MakeShared<FRequestContextBase>();
			}
			Context->BaseImpl->Outer = Context;
			Context->BaseImpl->RequestOwner = AsShared();
		}

		// subclass builds payload (including URL) - if there are issues with the input it may abort
		if (!DoBuildRequest(HttpRequest, Context))
		{
			return {};
		}

		//NOTE: this has to be strongly bound
		TSharedPtr<FMetaHumanServiceRequestBase> Owner = AsShared();
		HttpRequest->OnProcessRequestComplete().BindLambda([Owner, Settings, Context](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bCompletedOk)
			{
				int32 ResponseCode = bCompletedOk && Response ? Response->GetResponseCode() : -1;
				if (ResponseCode == EHttpResponseCodes::Ok)
				{
					// subclass handles unpacking payload and forwarding to user delegates
					Owner->OnRequestCompleted(Response->GetContent(), Context);
				}
				else if (ResponseCode == EHttpResponseCodes::Accepted)
				{
					// the service request has been accepted but is still in progress, response can be polled for progress until it returns 302 and download link
					Context->BaseImpl->PollUri = Response->GetHeader(TEXT("Location"));
					if (!Context->BaseImpl->PollUri.IsEmpty())
					{
						float Timeout = FCString::Atof(*Response->GetHeader(TEXT("Retry-After")));
						if (Timeout == 0.0f)
						{
							UE_LOGFMT(LogMetaHumanServiceClient, Warning, "Service returned invalid timeout, using default");
							Timeout = Settings->Timeout;
						}
						Context->BaseImpl->RequestOwner = Owner;
						Owner->MetaHumanServiceRequestProgressDelegate.ExecuteIfBound(0.25f);
						Context->BaseImpl->CheckRequestStatus();
					}
					else
					{
						Owner->OnRequestFailed(EMetaHumanServiceRequestResult::ServerError, Context);
					}
				}
				else if (ResponseCode == EHttpResponseCodes::Denied)
				{
					/////////////////////////////////////////////////////////////////////////////////////////////
					// authenticate or wait for another task to do it

					CheckCreateClient();

					bool bNotBusy = false;
					if (bAuthBusy.compare_exchange_strong(bNotBusy, true))
					{
						// user is not logged in, we need to get a token before we can issue a request to whatever service we're dealing with
						
						AuthClient->LoginAsync(Authentication::FOnLoginCompleteDelegate::CreateLambda([Owner, Request](TSharedRef<Authentication::FClient> EosAuthClient)
							{
								CompleteLogin(true);
								// re-issue the request with the auth token
								SetAuthHeader(Request);
								Request->ProcessRequest();
							}),
							Authentication::FOnLoginFailedDelegate::CreateLambda([Owner, Context]() {
								AsyncTask(ENamedThreads::GameThread, [Owner, Context]()
									{
										CompleteLogin(false);
										Owner->OnRequestFailed(EMetaHumanServiceRequestResult::LoginFailed, Context);
									});
							}));
					}
					else
					{
						// some other task is handling authentication so in the meantime we'll just hang around waiting until it's done
						const auto StartWaitTime = FPlatformTime::Seconds();
						AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Owner, Request, Context, EndWaitTime = StartWaitTime + Settings->AuthTimeout, AuthPollInterval = Settings->AuthPollInterval]()
							{
								while (!bShuttingDown && bAuthBusy && FPlatformTime::Seconds() < EndWaitTime)
								{
									// authentication is measured in seconds, so 1/2 second is a reasonable interval to wait between checks
									FPlatformProcess::Sleep(AuthPollInterval);
								}
								if (!bShuttingDown)
								{
									if (!bAuthBusy)
									{
										// if we've not timed out we can try again and re-issue the request (the token is probably valid)
										SetAuthHeader(Request);
										Request->ProcessRequest();
									}
									else
									{
										// timed out
										AsyncTask(ENamedThreads::GameThread, [Owner, Context]()
											{
												// make sure the caller can take some action
												Owner->OnRequestFailed(EMetaHumanServiceRequestResult::Timeout, Context);
											});
									}
								}
								// else shutting down
							});
					}
				}
				else
				{
					EMetaHumanServiceRequestResult Result = HttpErrorReporterHelper(ResponseCode);
					if (Result == EMetaHumanServiceRequestResult::GatewayError || Result == EMetaHumanServiceRequestResult::Busy)
					{
						UE_LOGFMT(LogMetaHumanServiceClient, Warning, "Got retriable error; Retry manager failed to intercept");
					}
					Owner->OnRequestFailed(Result, Context);
				}
			}
		);

		return HttpRequest;
	}
	
}

#undef LOCTEXT_NAMESPACE 

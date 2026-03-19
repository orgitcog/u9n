// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "TimerManager.h"
#include "Templates/SharedPointer.h"
#include "Interfaces/IHttpRequest.h"
#include "Templates/PimplPtr.h"
#include "Cloud/MetaHumanCloudAuthentication.h"

#include "MetaHumanServiceRequest.generated.h"

class FSharedBuffer;

// service results that can be used by all services
UENUM()
enum class EMetaHumanServiceRequestResult
{
	Ok,
	Busy,
	Unauthorized,
	EulaNotAccepted,
	InvalidArguments,
	ServerError,
	LoginFailed,
	Timeout,
	GatewayError,
};

namespace UE::MetaHuman
{
	// These are relatively generic and can be used across all services

	// service request failed
	DECLARE_DELEGATE_OneParam(FMetaHumanServiceRequestFailedDelegate, EMetaHumanServiceRequestResult RequestResult);
	// service request in progress (percentage might not be accurate)
	DECLARE_DELEGATE_OneParam(FMetaHumanServiceRequestProgressDelegate, float Percentage);
	// service request succeeded, payload is available
	DECLARE_DELEGATE_OneParam(FMetaHumanServiceRequestFinishedDelegate, const TArray<uint8>& ServiceResponse);

	DECLARE_DELEGATE_OneParam(FMetaHumanServiceUserEulaAcceptedCheck, bool Accepted);

	// Subclass contexts must derive from this class for appropriate lifetimes
	class FRequestContextBase : public TSharedFromThis<FRequestContextBase, ESPMode::ThreadSafe>
	{
	public:
		FRequestContextBase();
		virtual ~FRequestContextBase() {}
		struct FImpl;
		TPimplPtr<FImpl> BaseImpl;
	};
	using FRequestContextBasePtr = TSharedPtr<FRequestContextBase>;

	namespace ServiceAuthentication
	{
		DECLARE_DELEGATE(FOnLoginCompleteDelegate);
		DECLARE_DELEGATE_OneParam(FOnCheckHasLoggedInUserCompleteDelegate, bool);
		DECLARE_DELEGATE(FOnLoginFailedDelegate);
		DECLARE_DELEGATE(FOnLogoutCompleteDelegate);

		// initialise auth services environment and (if needed) extra data
		void METAHUMANSDKEDITOR_API InitialiseAuthEnvironment(TSharedPtr<TArray<uint8>> EnvData);
		// shut down the auth environment cleanly (can only be called *once*)
		void METAHUMANSDKEDITOR_API ShutdownAuthEnvironment();
		// checks if a user is logged in
		void METAHUMANSDKEDITOR_API CheckHasLoggedInUserAsync(FOnCheckHasLoggedInUserCompleteDelegate&& OnCheckHasLoggedInUserCompleteDelegate);
		// log in to the active service auth environment - this function is intended for test purposes
		void METAHUMANSDKEDITOR_API LoginToAuthEnvironment(FOnLoginCompleteDelegate&& OnLoginCompleteDelegate, FOnLoginFailedDelegate&& OnLoginFailedDelegate);
		// log out of the active service auth environment - this function is intended for test purposes
		void METAHUMANSDKEDITOR_API LogoutFromAuthEnvironment(FOnLogoutCompleteDelegate&& OnLogoutCompleteDelegate);
	}

	/*
		Base class for MH Service Clients
		Implements the core "message loop" and delegates to specific handlers as needed.
		Handles:
		 1. Request building (delegates to subclass for payload details)
		 2. Success and Error response delegation

		NOTE: This class is not intended to be used directly (and indeed it can't)
	*/
	class METAHUMANSDKEDITOR_API FMetaHumanServiceRequestBase : public TSharedFromThis<FMetaHumanServiceRequestBase, ESPMode::ThreadSafe>
	{
		friend class FRequestContextBase;
	public:		
		virtual ~FMetaHumanServiceRequestBase() = default;

		FSimpleDelegate OnMetaHumanServiceRequestBeginDelegate;
		FMetaHumanServiceRequestFailedDelegate OnMetaHumanServiceRequestFailedDelegate;
		FMetaHumanServiceRequestProgressDelegate MetaHumanServiceRequestProgressDelegate;

	protected:
		FMetaHumanServiceRequestBase() = default;
		// build a request packet for a particular MH service 
		// Note that this can be called multiple times during an ExecuteRequest (for the same request), 
		// if for example the user first has to accept a EULA.
		// MaybeContext can be used to pass context specific information to the subclass' delegates
		virtual bool DoBuildRequest(TSharedRef<IHttpRequest> Request, FRequestContextBasePtr MaybeContext) = 0;
		// invoked when the request is complete and a response payload is available
		// the subclass is expected to package this into a adapter and forward to a service specific user delegate
		virtual void OnRequestCompleted(const TArray<uint8>& Response, FRequestContextBasePtr MaybeContext) = 0;
		// create a request, but don't start executing it
		TSharedPtr<IHttpRequest> CreateRequest(FRequestContextBasePtr Context);
		// Create and execute a request directly
		void ExecuteRequestAsync(FRequestContextBasePtr Context);
		// can be overriden in the subclass if a context is needed
		// should always be super:: invoked
		virtual void OnRequestFailed(EMetaHumanServiceRequestResult Result, FRequestContextBasePtr MaybeContext)
		{
			(void)MaybeContext;
			OnMetaHumanServiceRequestFailedDelegate.ExecuteIfBound(Result);
		}
	};
}
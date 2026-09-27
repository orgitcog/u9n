// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Cloud/MetaHumanCloudAuthentication.h"
#include "HttpModule.h"
#include "Templates/PimplPtr.h"

namespace UE::MetaHuman::Authentication
{
	struct FClientState;

	DECLARE_DELEGATE_OneParam(FOnCheckLoggedInCompletedDelegate, bool);
	DECLARE_DELEGATE_OneParam(FOnLoginCompleteDelegate, TSharedRef<class FClient>);
	DECLARE_DELEGATE(FOnLoginFailedDelegate);
	DECLARE_DELEGATE_OneParam(FOnLogoutCompleteDelegate, TSharedRef<class FClient>);

	class METAHUMANSDKEDITOR_API FClient final
		: public TSharedFromThis<FClient>
	{
		struct FPrivateToken
		{
			explicit FPrivateToken() = default;
		};
	public:
		explicit FClient(FPrivateToken PrivateToken);
		// create an instance of a MetaHuman cloud client for the given environment
		// NOTE: if GameDev the correct reserved data needs to be passed in
		static TSharedRef<FClient> CreateClient(EEosEnvironmentType EnvironmentType, void* InReserved = nullptr);
		// returns true if there's at least one logged in user for this client (will check auth token status)
		void HasLoggedInUser(FOnCheckLoggedInCompletedDelegate&& OnCheckLoggedInCompletedDelegate);
		// Log in asynchronously and invoke delegate when done
		void LoginAsync(FOnLoginCompleteDelegate&& OnLoginCompleteDelegate, FOnLoginFailedDelegate&& OnLoginFailedDelegate);
		// Log out asynchronously and invoke delegate when done
		void LogoutAsync(FOnLogoutCompleteDelegate&& OnLogoutCompleteDelegate);
		// Set the authorization header for this user in the given request
		// NOTE: this might block waiting for an in-progress authentication process
		bool SetAuthHeaderForUserBlocking(TSharedRef<IHttpRequest> Request);
	private:
		TPimplPtr<FClientState> ClientState;
	};
}
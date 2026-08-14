// Copyright Epic Games, Inc. All Rights Reserved.
#include "Cloud/MetaHumanCloudAuthentication.h"
#include "MetaHumanCloudAuthenticationInternal.h"
#include "Cloud/MetaHumanCloudServicesSettings.h"
#include "Logging/StructuredLog.h"
#include "Misc/ScopeLock.h"

#include "EOSShared.h"
#include "eos_auth.h"
#include "eos_sdk.h"
#include "IEOSSDKManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanAuth, Log, All)

namespace UE::MetaHuman::Authentication
{
	using IEOSPlatformHandlePtr = TSharedPtr<class IEOSPlatformHandle>;

	struct FCallbackContext;
	struct FClientState
	{
		FCriticalSection StateLock;
		EOS_HAuth AuthHandle = nullptr;
		EOS_EpicAccountId EpicAccountId = nullptr;
		IEOSPlatformHandlePtr PlatformHandle = nullptr;
		EEosEnvironmentType EnvironmentType = EEosEnvironmentType::Prod;

		TSharedPtr<FClient> OuterClient;
		uint64_t LoginFlags = 0;
		
		void Init(TSharedRef<FClient> Outer, EEosEnvironmentType EosEnvironmentType, void* InReserved);
		void LoginUsingPersist(FCallbackContext* CallbackContext);
		void LoginUsingAccountPortal(FCallbackContext* CallbackContext);
		void Login(FOnLoginCompleteDelegate&& OnLoginCompleteDelegate, FOnLoginFailedDelegate&& OnLoginFailedDelegate);
		void Logout(FOnLogoutCompleteDelegate&& OnLogoutCompleteDelegate);
		bool SetAuthHeaderForUser(TSharedRef<IHttpRequest> Request);
		void CheckIfLoggedInAsync(FOnCheckLoggedInCompletedDelegate&& OnCheckLoggedInCompletedDelegate);
		bool CheckedGetAuthHandle()
		{
			if (!PlatformHandle.IsValid())
			{
				return false;
			}
			AuthHandle = EOS_Platform_GetAuthInterface(*PlatformHandle);
			return true;
		}
	};

	struct FCallbackContext
	{
		FClientState* ClientState;
		FOnLoginCompleteDelegate OnLoginCompleteDelegate;
		FOnLoginFailedDelegate OnLoginFailedDelegate;
		FOnLogoutCompleteDelegate OnLogoutCompleteDelegate;

		static FCallbackContext* Create()
		{
			return new FCallbackContext;
		}

		template<typename TEOSCallbackInfo>
		static FCallbackContext* Get(const TEOSCallbackInfo* Data)
		{
			return reinterpret_cast<FCallbackContext*>(Data->ClientData);
		}
	};

	void EOS_CALL LoginCompleteCallbackFn(const EOS_Auth_LoginCallbackInfo* Data)
	{
		// ensure cleanup on leaving this function 
		TUniquePtr<FCallbackContext> CallbackContext; 
		CallbackContext.Reset(FCallbackContext::Get(Data));

		FClientState* ClientState = CallbackContext->ClientState;
		check(ClientState);
		if (ClientState->OuterClient.IsValid())
		{
			const EOS_EResult ResultCode = Data->ResultCode;
			switch (ResultCode)
			{
			case EOS_EResult::EOS_Success:
			{
				{
					FScopeLock Lock(&ClientState->StateLock);
					const int32_t AccountsCount = EOS_Auth_GetLoggedInAccountsCount(ClientState->AuthHandle);
					for (int32_t AccountIdx = 0; AccountIdx < AccountsCount; ++AccountIdx)
					{
						const EOS_EpicAccountId AccountId = EOS_Auth_GetLoggedInAccountByIndex(ClientState->AuthHandle, AccountIdx);
						EOS_ELoginStatus LoginStatus = EOS_Auth_GetLoginStatus(ClientState->AuthHandle, Data->LocalUserId);
						ClientState->EpicAccountId = AccountId;
					}
				}

				CallbackContext->OnLoginCompleteDelegate.ExecuteIfBound(ClientState->OuterClient.ToSharedRef());
			}
			break;
			case EOS_EResult::EOS_Auth_PinGrantCode:
			{
				UE_LOGFMT(LogMetaHumanAuth, Warning, "Login pin grant code");
			}
			break;
			case EOS_EResult::EOS_Auth_MFARequired:
			{
				UE_LOGFMT(LogMetaHumanAuth, Display, "Login MFA required");
			}
			break;
			case EOS_EResult::EOS_InvalidUser:
			{
				UE_LOGFMT(LogMetaHumanAuth, Display, "Invalid user");
			}
			break;
			case EOS_EResult::EOS_Auth_AccountFeatureRestricted:
			{
				UE_LOGFMT(LogMetaHumanAuth, Display, "Login failed, account is restricted");
			}
			break;
			default:
			{
				const FString Code = EOS_EResult_ToString(Data->ResultCode);
				UE_LOGFMT(LogMetaHumanAuth, Display, "Login failed - error code: {ResultCode}", *Code);
			}
			break;
			}

			if (Data->ResultCode != EOS_EResult::EOS_Success)
			{
				CallbackContext->OnLoginFailedDelegate.ExecuteIfBound();
			}
		}
	}

	void EOS_CALL LoginPersistCompleteCallbackFn(const EOS_Auth_LoginCallbackInfo* Data)
	{
		FCallbackContext* CallbackContext = FCallbackContext::Get(Data);
		if (Data->ResultCode != EOS_EResult::EOS_Success && ((CallbackContext->ClientState->LoginFlags & EOS_LF_NO_USER_INTERFACE) != EOS_LF_NO_USER_INTERFACE))
		{			
			CallbackContext->ClientState->LoginUsingAccountPortal(CallbackContext);
		}
		else
		{
			LoginCompleteCallbackFn(Data);
		}
	}

	void EOS_CALL LogoutCompletedCallbackFn(const EOS_Auth_LogoutCallbackInfo* Data)
	{
		// ensure cleanup on leaving this function 
		TUniquePtr<FCallbackContext> CallbackContext;
		CallbackContext.Reset(FCallbackContext::Get(Data));

		FClientState* ClientState = CallbackContext->ClientState;
		if (ClientState->OuterClient.IsValid())
		{
			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				{
					FScopeLock Lock(&ClientState->StateLock);
					// use this to signal that we're no longer logged in
					ClientState->EpicAccountId = nullptr;
				}
				CallbackContext->OnLogoutCompleteDelegate.ExecuteIfBound(ClientState->OuterClient.ToSharedRef());
			}
		}
	}

	void EOS_CALL DeletePersistentAuthCompletedCallbackFn(const EOS_Auth_DeletePersistentAuthCallbackInfo*)
	{
		/* NOP but we need the callback for the EOS function to succeed */
	}

	void FClientState::LoginUsingPersist(FCallbackContext* CallbackContext)
	{
		if (!CheckedGetAuthHandle())
		{
			CallbackContext->OnLoginFailedDelegate.ExecuteIfBound();
			return;
		}
		
		EOS_Auth_Credentials Credentials = { 0 };
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

		Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;

		EOS_Auth_LoginOptions LoginOptions = { 0 };
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
		LoginOptions.Credentials = &Credentials;
		LoginFlags = 0;

		EOS_Auth_Login(AuthHandle, &LoginOptions, CallbackContext, LoginPersistCompleteCallbackFn);
	}

	void FClientState::LoginUsingAccountPortal(FCallbackContext* CallbackContext)
	{
		if (!CheckedGetAuthHandle())
		{
			CallbackContext->OnLoginFailedDelegate.ExecuteIfBound();
			return;
		}

		EOS_Auth_Credentials Credentials = { 0 };
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

		Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;

		EOS_Auth_LoginOptions LoginOptions = { 0 };
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
		LoginOptions.Credentials = &Credentials;
		LoginFlags = 0;

		EOS_Auth_Login(AuthHandle, &LoginOptions, CallbackContext, LoginCompleteCallbackFn);
	}

	void FClientState::Init(TSharedRef<FClient> Outer, EEosEnvironmentType EosEnvironmentType, void* InReservedData)
	{
		EnvironmentType = EosEnvironmentType;
		OuterClient = Outer;

		IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
		if (SDKManager && SDKManager->IsInitialized())
		{
			EOS_Platform_Options PlatformOptions = {};
			PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;

			const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
			FString ProductId;
			FString SandboxId;
			FString ClientId;
			FString ClientSecret;
			FString DeploymentId;
			switch (EnvironmentType)
			{
			case EEosEnvironmentType::Prod:
			{
				ProductId = Settings->ProdEosConstants.ProductId;
				SandboxId = Settings->ProdEosConstants.SandboxId;
				ClientId = Settings->ProdEosConstants.ClientCredentialsId;
				ClientSecret = Settings->ProdEosConstants.ClientCredentialsSecret;
				DeploymentId = Settings->ProdEosConstants.DeploymentId;
				PlatformOptions.Reserved = nullptr;
			}
			break;
			case EEosEnvironmentType::GameDev:
			{
				ProductId = Settings->GameDevEosConstants.ProductId;
				SandboxId = Settings->GameDevEosConstants.SandboxId;
				ClientId = Settings->GameDevEosConstants.ClientCredentialsId;
				ClientSecret = Settings->GameDevEosConstants.ClientCredentialsSecret;
				DeploymentId = Settings->GameDevEosConstants.DeploymentId;
				PlatformOptions.Reserved = InReservedData;
			}
			break;
			default:;
			}

			const FTCHARToUTF8 Utf8ProductId(*ProductId);
			const FTCHARToUTF8 Utf8SandboxId(*SandboxId);
			const FTCHARToUTF8 Utf8ClientId(*ClientId);
			const FTCHARToUTF8 Utf8ClientSecret(*ClientSecret);
			const FTCHARToUTF8 Utf8DeploymentId(*DeploymentId);

			PlatformOptions.ClientCredentials.ClientId = Utf8ClientId.Get();
			PlatformOptions.ClientCredentials.ClientSecret = Utf8ClientSecret.Get();
			PlatformOptions.ProductId = Utf8ProductId.Get();
			PlatformOptions.SandboxId = Utf8SandboxId.Get();
			PlatformOptions.DeploymentId = Utf8DeploymentId.Get();
			PlatformOptions.bIsServer = EOS_FALSE;
			PlatformOptions.Flags = EOS_PF_DISABLE_OVERLAY;
			PlatformOptions.TickBudgetInMilliseconds = 0;
			PlatformOptions.IntegratedPlatformOptionsContainerHandle = nullptr;

			PlatformHandle = SDKManager->CreatePlatform(PlatformOptions);
		}
	}

	void FClientState::Login(FOnLoginCompleteDelegate&& InOnLoginCompleteDelegate, FOnLoginFailedDelegate&& InOnLoginFailedDelegate)
	{		
		if (PlatformHandle.IsValid())
		{
			FCallbackContext* CallbackContext = FCallbackContext::Create();
			CallbackContext->ClientState = this;
			CallbackContext->OnLoginCompleteDelegate = MoveTemp(InOnLoginCompleteDelegate);
			CallbackContext->OnLoginFailedDelegate = MoveTemp(InOnLoginFailedDelegate);
			// always do this first, if it fails we chain to the portal
			LoginUsingPersist(CallbackContext);
		}
	}

	void FClientState::Logout(FOnLogoutCompleteDelegate&& InOnLogoutCompleteDelegate)
	{
		if (EpicAccountId != nullptr && PlatformHandle.IsValid())
		{
			// If we've just logged in with Persistent Auth
			EOS_Auth_LogoutOptions LogoutOptions = {};
			LogoutOptions.ApiVersion = EOS_AUTH_LOGOUT_API_LATEST;
			LogoutOptions.LocalUserId = EpicAccountId;

			FCallbackContext* CallbackContext = FCallbackContext::Create();
			CallbackContext->ClientState = this;
			CallbackContext->OnLogoutCompleteDelegate = MoveTemp(InOnLogoutCompleteDelegate);
			EOS_Auth_Logout(AuthHandle, &LogoutOptions, CallbackContext, LogoutCompletedCallbackFn);

			// And if we've also logged in with account portal (we need both to properly clean things up)
			EOS_Auth_DeletePersistentAuthOptions LogoutPersistentOptions = {};
			LogoutPersistentOptions.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
			EOS_Auth_DeletePersistentAuth(AuthHandle, &LogoutPersistentOptions, this, DeletePersistentAuthCompletedCallbackFn);
		}
	}

	bool FClientState::SetAuthHeaderForUser(TSharedRef<IHttpRequest> Request)
	{
		bool bWasSet = false;
		// we need to lock this entire call so that we don't clash with logouts
		FScopeLock Lock(&StateLock);
		if (EpicAccountId != nullptr)
		{
			EOS_Auth_Token* UserAuthTokenPtr = nullptr;
			EOS_Auth_CopyUserAuthTokenOptions CopyTokenOptions = { 0 };
			CopyTokenOptions.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;

			FString UserAuthToken;
			if (EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyTokenOptions, EpicAccountId, &UserAuthTokenPtr) == EOS_EResult::EOS_Success)
			{
				UserAuthToken = UserAuthTokenPtr->AccessToken;
				EOS_Auth_Token_Release(UserAuthTokenPtr);
			}
			
			if (!UserAuthToken.IsEmpty())
			{
				Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + UserAuthToken);
				bWasSet = true;
			}
		}
		return bWasSet;
	}

	void FClientState::CheckIfLoggedInAsync(FOnCheckLoggedInCompletedDelegate&& OnCheckLoggedInCompletedDelegate)
	{
		OnCheckLoggedInCompletedDelegate.ExecuteIfBound(CheckedGetAuthHandle() && EOS_Auth_GetLoggedInAccountsCount(AuthHandle) > 0);

#if USE_EOS_LOGGED_IN_CHECK
		if (EOS_Auth_GetLoggedInAccountsCount(AuthHandle) == 0)
		{
			EOS_Auth_Credentials Credentials = { 0 };
			Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;

			EOS_Auth_LoginOptions LoginOptions = { 0 };
			LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
			LoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
			// don't trigger a UI flow if persistent log-in doesn't work
			LoginOptions.LoginFlags = LoginFlags = EOS_LF_NO_USER_INTERFACE;
			LoginOptions.Credentials = &Credentials;

			FCallbackContext* CallbackContext = new FCallbackContext;
			CallbackContext->ClientState = this;
			CallbackContext->OnLoginCompleteDelegate = FOnLoginCompleteDelegate::CreateLambda([OnCheckLoggedInCompletedDelegate](TSharedRef<FClient>)
				{
					OnCheckLoggedInCompletedDelegate.ExecuteIfBound(true);
				});
			CallbackContext->OnLoginFailedDelegate = FOnLoginFailedDelegate::CreateLambda([OnCheckLoggedInCompletedDelegate]()
				{
					OnCheckLoggedInCompletedDelegate.ExecuteIfBound(false);
				});
			EOS_Auth_Login(AuthHandle, &LoginOptions, CallbackContext, LoginPersistCompleteCallbackFn);
		}
#endif // USE_EOS_LOGGED_IN_CHECK
	}

	FClient::FClient(FPrivateToken)
	{
		ClientState = MakePimpl<FClientState>();
	}

	TSharedRef<FClient> FClient::CreateClient(EEosEnvironmentType EnvironmentType, void* InReserved)
	{
		TSharedRef<FClient> Client = MakeShared<FClient>(FClient::FPrivateToken());
		Client->ClientState->Init(Client, EnvironmentType, InReserved);
		return Client;
	}

	void FClient::HasLoggedInUser(FOnCheckLoggedInCompletedDelegate&& OnCheckLoggedInCompletedDelegate)
	{
		return ClientState->CheckIfLoggedInAsync(Forward<FOnCheckLoggedInCompletedDelegate>(OnCheckLoggedInCompletedDelegate));
	}

	void FClient::LoginAsync(FOnLoginCompleteDelegate&& OnLoginCompleteDelegate, FOnLoginFailedDelegate&& OnLoginFailedDelegate)
	{
		ClientState->Login(Forward<FOnLoginCompleteDelegate>(OnLoginCompleteDelegate), Forward<FOnLoginFailedDelegate>(OnLoginFailedDelegate));
	}

	void FClient::LogoutAsync(FOnLogoutCompleteDelegate &&OnLogoutCompleteDelegate)
	{
		ClientState->Logout(Forward<FOnLogoutCompleteDelegate>(OnLogoutCompleteDelegate));
	}

	bool FClient::SetAuthHeaderForUserBlocking(TSharedRef<IHttpRequest> Request)
	{
		return ClientState->SetAuthHeaderForUser(Request);
	}
}

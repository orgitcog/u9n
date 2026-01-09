// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineServices.h"

#include "Online/OnlineComponentRegistry.h"
#include "Online/OnlineAsyncOpCache.h"
#include "Online/OnlineAsyncOpQueue.h"
#include "Online/OnlineConfig.h"

#include "Containers/Ticker.h"

#define UE_API ONLINESERVICESCOMMON_API

namespace UE::Online { class IOnlineExecHandler; }

namespace UE::Online {

class FOnlineServicesCommon
	: public IOnlineServices
	, public TSharedFromThis<FOnlineServicesCommon>
	, public FTSTickerObjectBase
	, public FSelfRegisteringExec
{
public:
	using Super = IOnlineServices;
	
	UE_DEPRECATED(5.5, "Please call the new method taking an FName InstanceConfigName")
	FOnlineServicesCommon(const FString& InServiceConfigName, FName InInstanceName)
		: FOnlineServicesCommon(InServiceConfigName, InInstanceName, NAME_None)
	{
	}

	UE_API FOnlineServicesCommon(const FString& InServiceConfigName, FName InInstanceName, FName InInstanceConfigName);
	FOnlineServicesCommon(const FOnlineServicesCommon&) = delete;
	FOnlineServicesCommon(FOnlineServicesCommon&&) = delete;
	UE_API virtual ~FOnlineServicesCommon();

	// IOnlineServices
	UE_API virtual void Init() override;
	UE_API virtual void Destroy() override;
	UE_API virtual IAchievementsPtr GetAchievementsInterface() override;
	UE_API virtual IAuthPtr GetAuthInterface() override;
	UE_API virtual IUserInfoPtr GetUserInfoInterface() override;
	UE_API virtual ICommercePtr GetCommerceInterface() override;
	UE_API virtual ISocialPtr GetSocialInterface() override;
	UE_API virtual IPresencePtr GetPresenceInterface() override;
	UE_API virtual IExternalUIPtr GetExternalUIInterface() override;
	UE_API virtual ILeaderboardsPtr GetLeaderboardsInterface() override;
	UE_API virtual ILobbiesPtr GetLobbiesInterface() override;
	UE_API virtual ISessionsPtr GetSessionsInterface() override;
	UE_API virtual IStatsPtr GetStatsInterface() override;
	UE_API virtual IConnectivityPtr GetConnectivityInterface() override;
	UE_API virtual IPrivilegesPtr GetPrivilegesInterface() override;
	UE_API virtual ITitleFilePtr GetTitleFileInterface() override;
	UE_API virtual IUserFilePtr GetUserFileInterface() override;
	UE_API virtual TOnlineResult<FGetResolvedConnectString> GetResolvedConnectString(FGetResolvedConnectString::Params&& Params) override;
	UE_API virtual FName GetInstanceName() const override;
	UE_API virtual FName GetInstanceConfigName() const override;
	UE_API virtual void AssignBaseInterfaceSharedPtr(const FOnlineTypeName& TypeName, void* OutBaseInterfaceSP) override final;

	// FOnlineServicesCommon

	/**
	 * Retrieve any of the Interface IOnlineComponents
	 */
	template <typename ComponentType>
	ComponentType* Get()
	{
		return Components.Get<ComponentType>();
	}

	/**
	 * Called to register all the IOnlineComponents with the IOnlineService, called after this is constructed
	 */
	UE_API virtual void RegisterComponents();

	/**
	 * Calls Initialize on all the components, called after RegisterComponents
	 */
	UE_API virtual void Initialize();

	/**
	 * Calls PostInitialize on all the components, called after Initialize
	 */
	UE_API virtual void PostInitialize();

	/**
	 * Calls UpdateConfig on all the components
	 */
	UE_API virtual void UpdateConfig();

	/**
	 * Calls Tick on all the components
	 */
	UE_API virtual bool Tick(float DeltaSeconds) override;

	/**
	 * Calls PreShutdown on all the components, called prior to Shutdown
	 */
	UE_API virtual void PreShutdown();

	/**
	 * Calls Shutdown on all the components, called before this is destructed
	 */
	UE_API virtual void Shutdown();

	/**
	 * Call a callable according to a specified execution policy
	 */
	template <typename CallableType>
	void Execute(FOnlineAsyncExecutionPolicy ExecutionPolicy, CallableType&& Callable)
	{
		switch (ExecutionPolicy.GetExecutionPolicy())
		{
			case EOnlineAsyncExecutionPolicy::RunOnGameThread:
				ExecuteOnGameThread(MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunOnNextTick:
				Async(EAsyncExecution::TaskGraphMainThread, MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunOnThreadPool:
				Async(EAsyncExecution::ThreadPool, MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunOnTaskGraph:
				Async(EAsyncExecution::TaskGraph, MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunImmediately:
				Callable();
				break;
		}
	}

	/**
	 * Call a callable on the game thread
	 */
	template <typename CallableType>
	void ExecuteOnGameThread(CallableType&& Callable)
	{
		if (IsInGameThread())
		{
			Callable();
		}
		else
		{
			Async(EAsyncExecution::TaskGraphMainThread, MoveTemp(Callable));
		}
	}

	/**
	 * Override the default config provider (FOnlineConfigProviderGConfig(GEngineini))
	 */
	void SetConfigProvider(TUniquePtr<IOnlineConfigProvider>&& InConfigProvider)
	{
		ConfigProvider = MoveTemp(InConfigProvider);
	}

	/**
	 * Clear the list of config overrides
	 */
	void ResetConfigSectionOverrides()
	{
		ConfigSectionOverrides.Reset();
	}

	/**
	 * Add a config section override. These will be used in the order they are added
	 */
	void AddConfigSectionOverride(const FString& Override)
	{
		ConfigSectionOverrides.Add(Override);
	}

	/**
	 * Get the ini config name for the Subsystem
	 */
	UE_DEPRECATED(5.5, "GetConfigName has been renamed GetServiceConfigName")
	const FString& GetConfigName() const { return ServiceConfigName; }
	const FString& GetServiceConfigName() const { return ServiceConfigName; }

	UE_DEPRECATED(5.6, "GetConfigSectionHeiarchy has been renamed GetConfigSectionHeirarchy")
	TArray<FString> GetConfigSectionHeiarchy(const FString& OperationName = FString()) const
	{
		return GetConfigSectionHeirarchy(OperationName);
	}
	
	TArray<FString> GetConfigSectionHeirarchy(const FString& OperationName = FString()) const
	{
		TArray<FString> SectionHeirarchy;
		FString SectionName = TEXT("OnlineServices");
		SectionHeirarchy.Add(SectionName);
		SectionName += TEXT(".") + GetServiceConfigName();
		SectionHeirarchy.Add(SectionName);
		if (!OperationName.IsEmpty())
		{
			SectionName += TEXT(".") + OperationName;
			SectionHeirarchy.Add(SectionName);
		}
		return SectionHeirarchy;
	}

	/**
	 * Load a config struct for an interface + operation
	 * Will load values from the following sections:
	 *   OnlineServices
	 *   OnlineServices.<InterfaceName> (if InterfaceName is set)
	 *   OnlineServices.<ServiceProvider>
	 *   OnlineServices.<ServiceProvider>.<InterfaceName> (if InterfaceName is set)
	 *   OnlineServices.<ServiceProvider>.<InterfaceName>.<OperationName> (if OperationName is set)
	 * 
	 * @param Struct Struct to populate with values from config
	 * @param InterfaceName Optional interface name to append to the config section name
	 * @param OperationName Optional operation name to append to the config section name
	 * 
	 * @return true if a value was loaded
	 */
	template <typename StructType>
	bool LoadConfig(StructType& Struct, const FString& InterfaceName = FString(), const FString& OperationName = FString()) const
	{
		TArray<FString> SectionHeirarchy;
		FString SectionName = TEXT("OnlineServices");
		SectionHeirarchy.Add(SectionName);
		if (!InterfaceName.IsEmpty())
		{
			SectionHeirarchy.Add(SectionName + TEXT(".") + InterfaceName);
		}
		SectionName += TEXT(".") + GetServiceConfigName();
		SectionHeirarchy.Add(SectionName);
		if (!InterfaceName.IsEmpty())
		{
			SectionName += TEXT(".") + InterfaceName;
			SectionHeirarchy.Add(SectionName);
			if (!OperationName.IsEmpty())
			{
				SectionName += TEXT(".") + OperationName;
				SectionHeirarchy.Add(SectionName);
			}
		}
		return LoadConfig(Struct, SectionHeirarchy);
	}
	
	UE_DEPRECATED(5.6, "GetConfigSectionHeirachWithOverrides has been renamed GetConfigSectionHeirarchyWithOverrides")
	TArray<FString> GetConfigSectionHeirachWithOverrides(const TArray<FString>& SectionHeirarchy) const
	{
		return GetConfigSectionHeirarchyWithOverrides(SectionHeirarchy);
	}

	/**
	 * Get an array of a config section with the overrides added in
	 * 
	 * @param SectionHeirarchy Array of config sections to load values from
	 * 
	 * @return Array of the sections with overrides for values to be loaded from
	 */
	TArray<FString> GetConfigSectionHeirarchyWithOverrides(const TArray<FString>& SectionHeirarchy) const
	{
		TArray<FString> AllConfigSectionOverrides = ConfigSectionOverrides;

		if (GIsEditor)
		{
			AllConfigSectionOverrides.Add("Editor");
		}

		TArray<FString> SectionHeirarchyWithOverrides;
		for (const FString& Section : SectionHeirarchy)
		{
			SectionHeirarchyWithOverrides.Add(Section);

			for (const FString& Override : AllConfigSectionOverrides)
			{
				// Using of space in online config section is deprecated
				FString OverrideSection = Section + TEXT(" ") + Override;
				SectionHeirarchyWithOverrides.Add(OverrideSection);

				OverrideSection = Section + TEXT(":") + Override;
				SectionHeirarchyWithOverrides.Add(OverrideSection);
			}
		}

		return SectionHeirarchyWithOverrides;
	}

	/**
	 * Load a config struct for a section heirarchy, also using the ConfigSectionOverrides
	 *
	 * @param Struct Struct to populate with values from config
	 * @param SectionHeirarchy Array of config sections to load values from
	 *
	 * @return true if a value was loaded
	 */
	template <typename StructType>
	bool LoadConfig(StructType& Struct, const TArray<FString>& SectionHeirarchy) const
	{
		return ::UE::Online::LoadConfig(*ConfigProvider, GetConfigSectionHeirarchyWithOverrides(SectionHeirarchy), Struct);
	}

	/* Get op (OnlineServices) */
	template <typename OpType>
	TOnlineAsyncOpRef<OpType> GetOp(typename OpType::Params&& Params)
	{
		LogErrorIfOnlineServicesHasShutdown<OpType>();
		return OpCache.GetOp<OpType>(MoveTemp(Params), GetConfigSectionHeirarchy());
	}

	template <typename OpType, typename ParamsFuncsType = TJoinableOpParamsFuncs<OpType>>
	TOnlineAsyncOpRef<OpType> GetJoinableOp(typename OpType::Params&& Params)
	{
		LogErrorIfOnlineServicesHasShutdown<OpType>();
		return OpCache.GetJoinableOp<OpType, ParamsFuncsType>(MoveTemp(Params), GetConfigSectionHeirarchy());
	}

	template <typename OpType, typename ParamsFuncsType = TMergeableOpParamsFuncs<OpType>>
	TOnlineAsyncOpRef<OpType> GetMergeableOp(typename OpType::Params&& Params)
	{
		LogErrorIfOnlineServicesHasShutdown<OpType>();
		return OpCache.GetMergeableOp<OpType, ParamsFuncsType>(MoveTemp(Params), GetConfigSectionHeirarchy());
	}

	/* Get op (Interface) */
	template <typename OpType>
	TOnlineAsyncOpRef<OpType> GetOp(typename OpType::Params&& Params, const TArray<FString>& ConfigSectionHeirarchy)
	{
		LogErrorIfOnlineServicesHasShutdown<OpType>();
		return OpCache.GetOp<OpType>(MoveTemp(Params), ConfigSectionHeirarchy);
	}

	template <typename OpType, typename ParamsFuncsType = TJoinableOpParamsFuncs<OpType>>
	TOnlineAsyncOpRef<OpType> GetJoinableOp(typename OpType::Params&& Params, const TArray<FString>& ConfigSectionHeirarchy)
	{
		LogErrorIfOnlineServicesHasShutdown<OpType>();
		return OpCache.GetJoinableOp<OpType, ParamsFuncsType>(MoveTemp(Params), ConfigSectionHeirarchy);
	}

	template <typename OpType, typename ParamsFuncsType = TMergeableOpParamsFuncs<OpType>>
	TOnlineAsyncOpRef<OpType> GetMergeableOp(typename OpType::Params&& Params, const TArray<FString>& ConfigSectionHeirarchy)
	{
		LogErrorIfOnlineServicesHasShutdown<OpType>();
		return OpCache.GetMergeableOp<OpType, ParamsFuncsType>(MoveTemp(Params), ConfigSectionHeirarchy);
	}

	/* Queue for executing tasks in parallel. Serial queues feed into this */
	UE_API FOnlineAsyncOpQueueParallel& GetParallelQueue();

	/* Queue for executing tasks in serial */
	UE_API FOnlineAsyncOpQueue& GetSerialQueue();

	/* Queues for executing per-user tasks in serial */
	UE_API FOnlineAsyncOpQueue& GetSerialQueue(const FAccountId& AccountId);
	
	UE_API void RegisterExecHandler(const FString& Name, TUniquePtr<IOnlineExecHandler>&& Handler);

#if UE_ALLOW_EXEC_COMMANDS
	UE_API virtual bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override;
#endif

	FOnlineAsyncOpCache OpCache;

protected:
	enum class EAsyncOpFlushReason : uint8
	{
		Default,
		Shutdown
	};

	template <typename OpType>
	void LogErrorIfOnlineServicesHasShutdown()
	{
		UE_CLOG(bPreShutdownComplete, LogOnlineServices, Error, TEXT("Can't add op %s anymore after shutdown in %p %s online services!"), OpType::Name, this, LexToString(GetServicesProvider()));
	}

	UE_API void Flush(EAsyncOpFlushReason FlushReason);
	UE_API virtual void FlushTick(float DeltaSeconds);
	UE_API void LoadCommonConfig();

	TMap<FString, TUniquePtr<IOnlineExecHandler>> ExecCommands;

	static UE_API uint32 NextInstanceIndex;
	uint32 InstanceIndex;
	FName InstanceName;
	FName InstanceConfigName;

	FOnlineComponentRegistry Components;
	TUniquePtr<IOnlineConfigProvider> ConfigProvider;

	/* Config section overrides */
	TArray<FString> ConfigSectionOverrides;
	FString ServiceConfigName;

	FOnlineAsyncOpQueueParallel ParallelQueue;
	FOnlineAsyncOpQueueSerial SerialQueue;
	TMap<FAccountId, TUniquePtr<FOnlineAsyncOpQueueSerial>> PerUserSerialQueue;
	bool bPreShutdownComplete;
};

/* UE::Online */ }

#undef UE_API

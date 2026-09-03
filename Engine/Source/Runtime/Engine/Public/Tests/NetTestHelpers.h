// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Engine/NetDriver.h"

#include "GameFramework/PlayerController.h"

class IConsoleVariable;
class UGameInstance;
class UWorld;
class UReplicationSystem;
class UObjectReplicationBridge;

struct FGameInstancePIEParameters;

#if WITH_EDITOR

namespace UE::Net
{

/*
 * Used to set a cvar and restore it to it's original value when destroyed
 */
class [[nodiscard]] FScopedCVarOverrideInt
{
public:
	ENGINE_API FScopedCVarOverrideInt(const TCHAR* VariableName, int32 Value);
	ENGINE_API ~FScopedCVarOverrideInt();

	FScopedCVarOverrideInt(FScopedCVarOverrideInt&&) = delete;
	FScopedCVarOverrideInt(const FScopedCVarOverrideInt&) = delete;
	FScopedCVarOverrideInt& operator=(FScopedCVarOverrideInt&&) = delete;
	FScopedCVarOverrideInt& operator=(const FScopedCVarOverrideInt&) = delete;

private:
	IConsoleVariable* Variable = nullptr;
	int32 SavedValue = 0;
};

class [[nodiscard]] FScopedCVarOverrideFloat
{
public:
	ENGINE_API FScopedCVarOverrideFloat(const TCHAR* VariableName, float Value);
	ENGINE_API ~FScopedCVarOverrideFloat();

	FScopedCVarOverrideFloat(FScopedCVarOverrideFloat&&) = delete;
	FScopedCVarOverrideFloat(const FScopedCVarOverrideFloat&) = delete;
	FScopedCVarOverrideFloat& operator=(FScopedCVarOverrideFloat&&) = delete;
	FScopedCVarOverrideFloat& operator=(const FScopedCVarOverrideFloat&) = delete;

private:
	IConsoleVariable* Variable = nullptr;
	float SavedValue = 0;
};

/**
* Sets and restores  cvars needed to use FNetTestWorldInstances within a scope.
* Meant to be used within a single function.
*/
class FScopedTestSettings
{
public:
	ENGINE_API FScopedTestSettings();
	ENGINE_API ~FScopedTestSettings();

	FScopedTestSettings(FScopedTestSettings&&) = delete;
	FScopedTestSettings(const FScopedTestSettings&) = delete;
	FScopedTestSettings& operator=(FScopedTestSettings&&) = delete;
	FScopedTestSettings& operator=(const FScopedTestSettings&) = delete;

private:
	FScopedCVarOverrideInt AddressResolutionDisabled;
	FScopedCVarOverrideInt BandwidthThrottlingDisabled;
	FScopedCVarOverrideInt RepGraphBandwidthThrottlingDisabled;
	FScopedCVarOverrideInt RandomNetUpdateDelayDisabled;
	FScopedCVarOverrideInt GameplayDebuggerDisabled;
};

/**
* Stores and restore GWorld and PIE settings modified by the creation of the temporary test worlds
*/
class FScopedNetTestPIERestoration
{
public:

	ENGINE_API FScopedNetTestPIERestoration();
	ENGINE_API ~FScopedNetTestPIERestoration();

	FScopedNetTestPIERestoration(FScopedNetTestPIERestoration&&) = delete;
	FScopedNetTestPIERestoration(const FScopedNetTestPIERestoration&) = delete;
	FScopedNetTestPIERestoration& operator=(FScopedNetTestPIERestoration&&) = delete;
	FScopedNetTestPIERestoration& operator=(const FScopedNetTestPIERestoration&) = delete;

private:

	// Restores PIE world context
	UWorld* OldGWorld = nullptr;
	int32 OldPIEID = 0;
	bool OldGIsPlayInEditorWorld = false;
};

/**
 * Properly scoped/RAII wrapper around a GameInstance/WorldContext/World that makes it easier to write tests
 * involving full UWorld functionality within the scope of one function.
 */
struct FTestWorldInstance
{
	struct FContext;

public:
	ENGINE_API static FTestWorldInstance CreateServer(const TCHAR* InURL, const EReplicationSystem ReplicationSystem = EReplicationSystem::Default);
	ENGINE_API static FTestWorldInstance CreateClient(int32 ServerPort);
	ENGINE_API static FTestWorldInstance CreateProxy();

	ENGINE_API ~FTestWorldInstance();

	FTestWorldInstance(const FTestWorldInstance&) = delete;
	FTestWorldInstance& operator=(const FTestWorldInstance&) = delete;

	explicit FTestWorldInstance(bool bDelayedInit) : GameInstance(nullptr) {}
	
	ENGINE_API FTestWorldInstance(FTestWorldInstance&& Other);
	ENGINE_API FTestWorldInstance& operator=(FTestWorldInstance&& Other);

	ENGINE_API FWorldContext* GetWorldContext() const;	

	ENGINE_API FContext GetTestContext() const;

	ENGINE_API UWorld* GetWorld() const;
	ENGINE_API UNetDriver* GetNetDriver() const;

	ENGINE_API int32 GetPort();

	ENGINE_API void Tick(float DeltaSeconds = 0.0166f);

	ENGINE_API void LoadStreamingLevel(FName LevelName);
	ENGINE_API void UnloadStreamingLevel(FName LevelName);

public:

	struct FContext
	{
		UWorld* World = nullptr;
		UNetDriver* NetDriver = nullptr;
#if UE_WITH_IRIS
		UReplicationSystem* IrisRepSystem = nullptr;
		UObjectReplicationBridge* IrisRepBridge = nullptr;
#endif
	};

public:

	UGameInstance* GameInstance = nullptr;

private:
	explicit FTestWorldInstance(const FGameInstancePIEParameters& InstanceParams);

	void Shutdown();

	static int32 FindUnusedPIEInstance();

	int32 LevelStreamRequestUUID = 0;
};

/**
 * Stores FTestWorldInstances for a server and clients and allows synchronously ticking them.
 * Can be used within a single function to make automated tests that use the whole world & net driver flow.
 */
struct FTestWorlds
{
	/** Creates a server world using the default map (Entry) and default GameMode class (GameMode) */
	ENGINE_API FTestWorlds(const EReplicationSystem ReplicationSystem = EReplicationSystem::Default);

	/** 
	 * Creates a server world using a specific map name and GameMode class
	 * @param MapName Full path name of the map. ex: /Engine/Maps/Entry
	 * @param GameModeName Full path name of the game mode. ex: /Script/EngineTest.MyGameMode
	 */
	ENGINE_API FTestWorlds(const FString& MapName, const FString& GameModeName, const EReplicationSystem ReplicationSystem = EReplicationSystem::Default);

	/** Creates a server world using the given URL. */
	ENGINE_API explicit FTestWorlds(const TCHAR* ServerURL, const EReplicationSystem ReplicationSystem = EReplicationSystem::Default);

	ENGINE_API ~FTestWorlds();

	ENGINE_API void SetTickInSeconds(float TickInSeconds);
	float GetTickDeltaSeconds() const { return TickDeltaSeconds; }
	
	/** Create a client and fully connect it to the server before returning */
	ENGINE_API bool CreateAndConnectClient();

	/** Ticks all server & client worlds NumTick times synchronously. */
	ENGINE_API void TickAll(int32 NumTicks=1);
	ENGINE_API void TickServer();
	ENGINE_API void TickClients();

	/** Tick the world and drop all outgoing packets */
	ENGINE_API void TickServerAndDrop();
	ENGINE_API void TickClientsAndDrop();

	/** Tick the world but delay the packets that would be sent */
	ENGINE_API void TickServerAndDelay(uint32 NumFramesToDelay = 1);
	ENGINE_API void TickClientsAndDelay(uint32 NumFramesToDelay = 1);

	/**
	 * Ticks all server & client worlds until Predicate returns true, or MaxTicks is reached.
	 * Returns true if Predicate did, false if it didn't within MaxTicks.
	 */
	template<class PredicateT>
	bool TickAllUntil(const PredicateT& Predicate, float DeltaSeconds = 0.0166f, int32 MaxTicks = 60);

	/** Ticks all server & client worlds until the passed in client world has a valid client PlayerController. */
	ENGINE_API bool WaitForClientConnect(FTestWorldInstance& Client);

	/** Return the Server's player state corresponding to a specific client */
	ENGINE_API APlayerController* GetServerPlayerControllerOfClient(uint32 ClientIndex);

	/**
	 * Find the remote instance of a replicated object and ensures if it can't find it
	 * @param ServerObject The server object that you want the remote version of.
	 * @param ClientIndex The client you want a remote instance of
	 * @return Return the remote (client) instance of the same object if it exists.
	 */
	ENGINE_API UObject* FindReplicatedObjectOnClient(UObject* ServerObject, uint32 ClientIndex) const;

	template<typename T>
	T* FindReplicatedObjectOnClient(UObject* ServerObject, uint32 ClientIndex) const
	{
		return Cast<T>(FindReplicatedObjectOnClient(ServerObject, ClientIndex));
	}

	/**
	 * Returns if the remote instance of a replicated object exists on the client or not.
	 * @param ServerObject The server object you want to see if it is replicated
	 * @param ClientIndex The client you want to test on
	 */
	ENGINE_API bool DoesReplicatedObjectExistOnClient(UObject* ServerObject, uint32 ClientIndex) const;

	/**
	 * Returns if the object is registered for replication on the server.
	 * @param ServerObject The server object you want to see if it is replicated
	 */
	ENGINE_API bool IsServerObjectReplicated(const UObject* ServerObject) const;

public:

	/** Delegate called before a TickAll is executed */
	DECLARE_MULTICAST_DELEGATE(FPreTickAll);
	FPreTickAll PreTickAllDelegate;

	/** Delegate called after a client finished connecting to the server */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientConnected, const FTestWorldInstance& Client);
	FOnClientConnected OnClientConnected;

public:

	/** Server and Client Worlds */
	FTestWorldInstance Server;
	TArray<FTestWorldInstance> Clients;

private:

	void InitDelegates();

	void OnNetDriverCreated(UWorld* InWorld, UNetDriver* InNetDriver);
	FDelegateHandle NetDriverCreatedHandle;

private:

	float TickDeltaSeconds = 0.0166f;

	// Sets up important settings for the networking system to run optimally
	FScopedTestSettings TestSettings;

	// Restore GWorld and other PIE settings
	FScopedNetTestPIERestoration PIERestoration;
};

//------------------------------------------------------------------------
// Inline functions
//------------------------------------------------------------------------

template<class PredicateT>
inline bool FTestWorlds::TickAllUntil(const PredicateT& Predicate, float DeltaSeconds, int32 MaxTicks)
{
	PreTickAllDelegate.Broadcast();

	int32 TickCount = 0;
	bool bPredicateResult = Predicate();
		
	while (!bPredicateResult && TickCount < MaxTicks)
	{
		Server.Tick(DeltaSeconds);
		for (FTestWorldInstance& Client : Clients)
		{
			Client.Tick(DeltaSeconds);
		}
		TickCount++;
		GFrameCounter++;
		bPredicateResult = Predicate();
	}

	return bPredicateResult;
}

} // end namespace UE::Net

#endif //WITH_EDITOR
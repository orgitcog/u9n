// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/PendingNetGame.h"
#include "Engine/ChildConnection.h"
#include "GameFramework/OnlineReplStructs.h"
#include "IpNetDriver.h"
#include "IpConnection.h"
#include "MultiServerProxy.generated.h"

enum class EProxyConnectionState : uint8
{
	Disconnected,
	ConnectingPrimary,
	ConnectedPrimary
};

FString LexToString(EProxyConnectionState State);

USTRUCT()
struct FGameServerSplitJoinRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<class ULocalPlayer> Player;

	UE::Net::EJoinFlags Flags;

	uint32 ClientHandshakeId;
};

/**
 * All of the state associated with a proxy server connection to a backend game server.
 */ 
USTRUCT()
struct FGameServerConnectionState
{
	GENERATED_BODY()

	int32 Port;

	UPROPERTY()
	TObjectPtr<class UWorld> World;

	UPROPERTY()
	TObjectPtr<class UNetDriver> NetDriver;
	
	FName NetDriverName;

	UPROPERTY()
	TArray<TObjectPtr<class ULocalPlayer>> Players;

	UPROPERTY()
	TObjectPtr<UGameServerNotify> GameServerNotify;

	UPROPERTY()
	TArray<FGameServerSplitJoinRequest> PendingSplitJoinRequests;
};

USTRUCT()
struct FMultiServerProxyInternalConnectionRoute
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNetConnection> ProxyConnection;

	UPROPERTY()
	TObjectPtr<UNetConnection> ParentGameServerConnection;

	UPROPERTY()
	TObjectPtr<class ULocalPlayer> Player;
};

/** 
 * Intercept outgoing connection requests to game servers from the proxy server.
 *
 * Ensure that NMT_Join is sent after receiving NMT_Welcome from a game server. Normally
 * NMT_Join will be sent after a level is loaded but the proxy doesn't currently handle 
 * loading levels when connecting to a server.
 *
 * Defaults to the behavior in UPendingNetGame which normally handles all outgoing 
 * connections to a game server.
 */
UCLASS()
class UGameServerNotify : public UPendingNetGame
{
public:

	GENERATED_BODY()

	virtual void 					NotifyAcceptedConnection(UNetConnection* Connection) override;
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;
	virtual bool 					NotifyAcceptingChannel(class UChannel* Channel) override;
	virtual void 					NotifyControlMessage(UNetConnection* GameServerConnection, uint8 MessageType, class FInBunch& Bunch) override;
	virtual class ULocalPlayer* 	GetFirstGamePlayer();

	void SetProxyNetDriver(TObjectPtr<class UProxyNetDriver> InProxyNetDriver);
	void SetFlags(UE::Net::EJoinFlags InFlags) { Flags = InFlags; };

	/** Set the value to be returned by GetFirstGamePlayer(). */
	void SetFirstPlayer(TObjectPtr<class ULocalPlayer> Player);

private:

	UE::Net::EJoinFlags Flags;

	UPROPERTY()
	TObjectPtr<class ULocalPlayer> FirstPlayer;
	
	UPROPERTY()
	TObjectPtr<class UProxyNetDriver> ProxyNetDriver;
};

/** 
 * Intecept incoming from clients to the proxy server.
 *
 * Intercept NMT_Join when a client connects to the proxy, establishes a connection to
 * a game server and performs the logic required to associate these two connections and
 * forward state replicated from the game server to the client.
 * 
 * Defaults to the behavior in UWorld which normally handles all incoming game server 
 * connections.
 */
UCLASS()
class UProxyListenerNotify : public UObject, 
							 public FNetworkNotify
{
public:

	GENERATED_BODY()

	void SetProxyNetDriver(TObjectPtr<class UProxyNetDriver> InProxyNetDriver);

	virtual void 					NotifyAcceptedConnection(UNetConnection* Connection) override;
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;
	virtual bool 					NotifyAcceptingChannel(class UChannel* Channel) override;
	virtual void 					NotifyControlMessage(UNetConnection* ProxyConnection, uint8 MessageType, class FInBunch& Bunch) override;

private:

	/** Start connecting an incoming proxy connection to a game server. */
	void ConnectToGameServer(UNetConnection* ProxyConnection, 
							 int32 GameServerConnectionStateIndex, 
							 FGameServerConnectionState* GameServerConnectionState, 
							 UE::Net::EJoinFlags Flags);

	UPROPERTY()
	TObjectPtr<class UProxyNetDriver> ProxyNetDriver;
};

/** 
* A network connection used by UProxyBackendNetDriver.
 */
UCLASS()
class MULTISERVERREPLICATION_API UProxyBackendNetConnection : public UIpConnection
{
	GENERATED_BODY()

public:

	virtual void HandleClientPlayer(APlayerController* PC, UNetConnection* NetConnection);
};

/**
 * A child network connection used by UProxyBackendNetDriver.
 */
UCLASS()
class MULTISERVERREPLICATION_API UProxyBackendChildNetConnection : public UChildConnection
{
	GENERATED_BODY()

public:

	virtual void HandleClientPlayer(APlayerController* PC, UNetConnection* NetConnection);
};

/** 
 * A driver that is used by UProxyNetDriver to connect to backend game servers.
 */
UCLASS()
class MULTISERVERREPLICATION_API UProxyBackendNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:

	void SetProxyNetDriver(TObjectPtr<UProxyNetDriver> InParentNetDriver);
	TObjectPtr<UProxyNetDriver> GetProxyNetDriver();

	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error);
	virtual void ForwardRemoteFunction(UObject* RootObject, UObject* SubObject, UFunction* Function, void* Parms) override;
	virtual bool ShouldSkipRepNotifies() const override;
	virtual UChildConnection* CreateChild(UNetConnection* Parent) override;
	virtual void InternalProcessRemoteFunction(class AActor* Actor, class UObject* SubObject, class UNetConnection* Connection, class UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack, bool bIsServer) override;

private:

	UPROPERTY()
	TObjectPtr<UProxyNetDriver> ProxyNetDriver;
};

/**
* A network proxy that intercepts and forwards UE game network connections to backend game servers.
*
* The proxy externally behaves the same as a normal game server when game clients connect and
* as a normal client when connecting to game servers. This means that there is no need for the
* clients and game servers that the proxy is connected to have any special proxy-aware configuration.
*
* Internally, the proxy is made up of an instance of UProxyNetDriver that listens for incoming 
* connections, known as proxy connections, and an instance of UProxyBackendNetDriver for each 
* connection to a backend game server. State from the backend servers is replicated into a single,
* shared UWorld and the listening UProxyNetDriver replicates that state out to proxy connections.
*
* All actors replicated to the proxy from remote game servers will have the same role as a client
* (ROLE_SimulatedProxy or ROLE_AutonomousProxy) and will be replicated as-is to the proxy client.
*
* When a proxy connection (UNetConnection) is opened in UProxyNetDriver it opens a game server 
* connection (UNetConnection) to each registered backend server. Each proxy connection and game 
* server connection is associated with it's own instance of APlayerController. There will always 
* be an equal number of connections and player controllers on a proxy and can be calculated with
* this equation: i+(j*i), where i is the number of clients connected to the proxy and j is the 
* number of registered game servers.
*
* The proxy and game server player controllers are related through an instance of ULocalPlayer
* for each proxy connection to a registered game server: the equation for the number of players
* on the proxy is (i*j), where i is the number of clients connected to the proxy and j in the 
* number of registered game servers.
*
* The relationship between proxy connections and game server connections is done through pointers
* in the respective classes:
*
* ProxyConnection <-> ProxyPlayerController <-> LocalPlayer <- GameServerPlayerController <-> GameServerConnection
*
* From this representation of the relationship we can see that ProxyConnection has a pointer to 
* ProxyPlayerController, and ProxyPlayerController has a pointer to ProxyConnection. Using these
* existing pointers enables the use of existing UE code and avoids the need for additional data
* structures to hold the relationship betweeen the clients connected to the proxy
* and registered game servers.
*
* IMPORTANT: There isn't a relationship from LocalPlayer to GameServerPlayerController because
* the exiting pointer (ULocalPlayer::PlayerController) that is being used can only point to one
* controller.
*
* For each proxy connection one of the game servers are considered the primary game server. This
* is the game server that spawns the proxy client's pawn, player controller, receive RPCs from
* the proxy connection, and send RPCs to the proxy connection. The other game servers are considered
* non-primary game servers and only replicate state relevant to that connection to the proxy.
*
* When connecting to non-primary game servers the game server will spawn a ANoPawnPlayerController 
* player controller, and not spawn a pawn. These connections will replicate state from the game 
* server but not maintain a player presence.
*/
UCLASS()
class MULTISERVERREPLICATION_API UProxyNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:
	
	/** Register a game server. */
	void RegisterGameServer(int32 Port);

	/** Return true if all registered servers are connected. */
	bool IsConnectedToAllGameServers() const;

	/** Enumerate through all outgoing connections to game servers. */
	int32 GetGameServerConnectionCount() const;
	FGameServerConnectionState* GetGameServerConnection(int32 Index);

	/** UNetDriver interface function. */
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(class FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual void ForwardRemoteFunction(UObject* RootObject, UObject* SubObject, UFunction* Function, void* Parms) override;
	virtual bool ShouldReplicateFunction(AActor* Actor, UFunction* Function) const override;
	virtual void NotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor) override;
	virtual void AddNetworkActor(AActor* Actor) override;
	virtual bool ShouldCallRemoteFunction(UObject* Object, UFunction* Function, const FReplicationFlags& RepFlags) const;
	virtual void InternalProcessRemoteFunction(class AActor* Actor, class UObject* SubObject, class UNetConnection* Connection, class UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack, bool bIsServer) override;
	virtual int32 ServerReplicateActors(float DeltaSeconds) override;
	virtual bool CanDowngradeActorRole(UNetConnection* ProxyConnection, AActor* Actor) const override;
	virtual void Shutdown() override;
	
	/** Get the next identifier for outgoing connections to game servers. */
	int32 GetNextGameServerClientId();

	/** Get the next connection handshake id. */
	uint32 GetNextClientHandshakeId();
	
	/** Called when the player controller associated with a connection is changed (either at the end of initial connection handshake, or if changed after successfully connected). */
	void GameServerAssignPlayerController(UNetConnection* ChildGameServerConnection, UNetConnection* ParentGameServerConnection, APlayerController* PlayerController);

private:

	friend class UProxyListenerNotify;

	// Classes in EngineTest that test the behavior of UMultiServerProxy.
	friend class FNetTestProxyRPCRouting;
	friend class FNetTestProxyConnectionRouting;
	friend class FNetTestProxyMovePlayerController;
	
	friend class UProxyBackendNetDriver;

	/** Set all of the configuration options that disable executing actor functionality or game specific code. */
	void DisableActorLogicAndGameCode();

	/** Return true if an actor is explicitly spawned by the proxy. */
	bool IsProxySpawned(AActor* Actor) const;
	
	/** Send any split join requests that have been queued up to the game server. */
	void FlushSplitJoinRequests(FGameServerConnectionState* GameServerConnectionState);

	/** Return a game server index that a player is connected to. */
	int32 GetGameServerWithPlayer(class ULocalPlayer* Player);

	/** Disassociate a proxy connection with a game server as a primary game server. */
	void DeregisterPrimaryGameServer(UNetConnection* ProxyConnection);

	/** Associate a proxy connection with a primary game server. */
	void RegisterPrimaryGameServer(UNetConnection* ProxyConnection, class ULocalPlayer* PrimaryPlayer, int32 GameServerConnectionStateIndex);

	/** Attach a new player, player controller and connection. */
	void AttachPlayerController(APlayerController* PlayerController, UNetConnection* Connection, ULocalPlayer* Player);

	/** Detach an existing player, player controller and connection. */
	void DetachPlayerController(APlayerController* PlayerController);
	
	/** Detach an existing player, player controller and connection and destroy the controller. */
	void DetachAndDestroyPlayerController(APlayerController* PlayerController);

	/** Create and destroy a proxy player controller. Returns null if failed to spawn the controller. */
	APlayerController* SpawnPlayerController();

	/** Destroy a player controller spawned by the proxy. */
	void DestroyPlayerController(APlayerController* PlayerController);
	
	/** Return a proxy connection from a primary player (a player on a primary game server). */
	static UNetConnection* GetProxyConnectionFromPrimaryPlayer(UPlayer* Player);

	/** Return a proxy player controller given a primary game server controller. */
	APlayerController* GetProxyControllerFromPrimaryGameServerController(APlayerController* GameServerPlayerController);
	
	/** Associate a request to connect to a game server with a route from a proxy connection to game server. */
	UPROPERTY()
	TMap<uint32, FMultiServerProxyInternalConnectionRoute> ClientHandshakeIdToInternalRoute;

	/** Map a player to it's primary game server index into GameServerConnections. */
	UPROPERTY()
	TMap<TObjectPtr<class ULocalPlayer>, int32> ProxyPlayerToPrimaryGameServer;

	/** Map a player to it's primary game server player controller. */
	UPROPERTY()
	TMap<TObjectPtr<class ULocalPlayer>, TObjectPtr<APlayerController>> ProxyPlayerToPrimaryGameServerPlayerController;

	/** The state of each incoming proxy connection. */
	TMap<TObjectPtr<UNetConnection>, EProxyConnectionState> ProxyConnectionState;

	/** Net drivers and associated state used to connect to backend game servers. */
	UPROPERTY()
	TArray<FGameServerConnectionState> GameServerConnections;

	/** Proxy listener handshake logic. */
	UPROPERTY()
	TObjectPtr<UProxyListenerNotify> ProxyListenerNotify;

	/** A set of actors that have been spawned by the proxy. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> ProxySpawnedActors;
	
	int32 NextGameServerClientId = 0;
	
	uint32 NextClientHandshakeId = 123;
	
	// The primary game server to use for the next primary game client.
	int32 PrimaryGameServerForNextClient = 0;

	// After a client connects to the proxy increment the value of PrimaryGameServerForNextClient.
	bool bCyclePrimaryGameServer = false;
};

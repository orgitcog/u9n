// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiServerProxy.h"
#include "UnrealEngine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/CommandLine.h"
#include "Net/DataChannel.h"

DEFINE_LOG_CATEGORY_STATIC(LogNetProxy, Log, All);

FString LexToString(EProxyConnectionState State)
{
	switch (State)
	{
		case EProxyConnectionState::Disconnected:
			return FString(TEXT("Disconnected"));
		case EProxyConnectionState::ConnectingPrimary:
			return FString(TEXT("ConnectingPrimary"));
		case EProxyConnectionState::ConnectedPrimary:
			return FString(TEXT("ConnectedPrimary"));
		default:
			return FString(TEXT("Unknown"));
	}
}

void UGameServerNotify::NotifyAcceptedConnection(class UNetConnection* Connection)
{

}

EAcceptConnection::Type UGameServerNotify::NotifyAcceptingConnection()
{
	return EAcceptConnection::Accept;
}

bool UGameServerNotify::NotifyAcceptingChannel(class UChannel* Channel)
{
	return true;
}

void UGameServerNotify::NotifyControlMessage(UNetConnection* GameServerConnection, uint8 MessageType, class FInBunch& Bunch)
{
	Super::NotifyControlMessage(GameServerConnection, MessageType, Bunch);

	if (MessageType == NMT_Welcome)
	{
		// The default implementation of UPendingNetGame will only send the join request to the server when the level
		// has loaded. Since the proxy is not currently dependent on loading levels we just send the join request 
		// when receiving the welcome message to shortcut this logic.
		SendJoinWithFlags(Flags);
	}
}

void UGameServerNotify::SetProxyNetDriver(TObjectPtr<class UProxyNetDriver> InProxyNetDriver)
{
	ProxyNetDriver = InProxyNetDriver;
}

void UGameServerNotify::SetFirstPlayer(TObjectPtr<ULocalPlayer> Player)
{
	FirstPlayer = Player;
}

ULocalPlayer* UGameServerNotify::GetFirstGamePlayer()
{
	return FirstPlayer;
}

void UProxyListenerNotify::SetProxyNetDriver(TObjectPtr<class UProxyNetDriver> InProxyNetDriver)
{
	ProxyNetDriver = InProxyNetDriver;
}

void UProxyListenerNotify::NotifyAcceptedConnection(class UNetConnection* Connection)
{

}

EAcceptConnection::Type UProxyListenerNotify::NotifyAcceptingConnection()
{
	return EAcceptConnection::Accept;
}

bool UProxyListenerNotify::NotifyAcceptingChannel(class UChannel* Channel)
{
	return true;
}

void UProxyListenerNotify::NotifyControlMessage(UNetConnection* ProxyConnection, uint8 MessageType, class FInBunch& Bunch)
{
	check(ProxyNetDriver);

	// The NMT_Join message received by a proxy connection should trigger a connection to the backend game servers.
	if (MessageType == NMT_Join)
	{
		// For now, the primary game server is always the first registered game server.
		for (int32 Index = 0; Index < ProxyNetDriver->GetGameServerConnectionCount(); Index++)
		{
			UE::Net::EJoinFlags Flags = UE::Net::EJoinFlags::NoPawn;
			if (Index == ProxyNetDriver->PrimaryGameServerForNextClient)
			{
				EnumRemoveFlags(Flags, UE::Net::EJoinFlags::NoPawn);
			}
			ConnectToGameServer(ProxyConnection, ProxyNetDriver->PrimaryGameServerForNextClient, ProxyNetDriver->GetGameServerConnection(Index), Flags);
		}

		if (ProxyNetDriver->bCyclePrimaryGameServer)
		{
			ProxyNetDriver->PrimaryGameServerForNextClient = (ProxyNetDriver->PrimaryGameServerForNextClient + 1) % ProxyNetDriver->GetGameServerConnectionCount();
		}
	}
	else
	{
		// Forward all other connection messages onto the existing handshake logic.
		ProxyNetDriver->GetWorld()->NotifyControlMessage(ProxyConnection, MessageType, Bunch);
	}
}

void UProxyListenerNotify::ConnectToGameServer(UNetConnection* ProxyConnection, 
											   int32 GameServerConnectionStateIndex,
											   FGameServerConnectionState* GameServerConnectionState,
											   UE::Net::EJoinFlags Flags)
{
	ProxyNetDriver->ProxyConnectionState.Add(ProxyConnection, EProxyConnectionState::Disconnected);

	// If this is the first connection to the game server, instantiate the backend network driver that will manage all
	// connections from proxy connections to that server.
	if (GameServerConnectionState->NetDriver == nullptr)
	{
		// Acts as a unique identifier for dependency NetDrivers.
		static int32 GameServerDriverId = 0;

		GameServerConnectionState->World = ProxyNetDriver->GetWorld();
		GameServerConnectionState->NetDriverName = FName(*FString::Printf(TEXT("ProxyToGameServer-%d"), GameServerDriverId++));

		GEngine->CreateNamedNetDriver(ProxyNetDriver->GetWorld(), GameServerConnectionState->NetDriverName, "ProxyBackendNetDriver");
		GameServerConnectionState->NetDriver = GEngine->FindNamedNetDriver(GameServerConnectionState->World, GameServerConnectionState->NetDriverName);
		
		GameServerConnectionState->NetDriver->SetWorld(ProxyNetDriver->GetWorld());
		Cast<UProxyBackendNetDriver>(GameServerConnectionState->NetDriver)->SetProxyNetDriver(ProxyNetDriver);

		UE_LOG(LogNetProxy, Log, TEXT("Created a game server NetDriver (name=%s, port=%d)"), *GameServerConnectionState->NetDriver->GetName(), GameServerConnectionState->Port);
	}

	// Add a player to use on the game server.
	const FPlatformUserId GameServerClientId = FPlatformUserId::CreateFromInternalId(ProxyNetDriver->GetNextGameServerClientId());
	ULocalPlayer* NewPlayer = NewObject<ULocalPlayer>(GEngine, ULocalPlayer::StaticClass());
	ProxyNetDriver->GetWorld()->GetGameInstance()->AddLocalPlayer(NewPlayer, GameServerClientId);

	// The new player will use the same unique identifier as the incoming proxy connection so that it will be propogated up to 
	// the game servers through UNetConnection::PlayerId. This way each game server's incoming connection will have a PlayerId 
	// that corresponds to a client connected to the proxy.
	NewPlayer->SetCachedUniqueNetId(ProxyConnection->PlayerId);

	const uint32 ClientHandshakeId = ProxyNetDriver->GetNextClientHandshakeId();

	UNetConnection* GameServerConnection = GameServerConnectionState->NetDriver->ServerConnection;
	const bool bIsFirstGameServerConnection = (GameServerConnection == nullptr);
	if (bIsFirstGameServerConnection)
	{
		const int32 GameServerPort = GameServerConnectionState->Port;
		FString URLStr = FString::Format(TEXT("127.0.0.1:{0}"), { GameServerPort });
		FURL URL(nullptr, *URLStr, TRAVEL_Absolute);
		
		URL.AddOption(*FString::Printf(TEXT("HandshakeId=%u"), ClientHandshakeId));

		// Maybe this should be a control message since it changes the server setting (it's global driver setting).
		URL.AddOption(TEXT("AutonomousAsSimulated"));

		// Start the connection flow to the game server.
		GameServerConnectionState->GameServerNotify = NewObject<UGameServerNotify>();
		GameServerConnectionState->GameServerNotify->Initialize(URL);
		GameServerConnectionState->GameServerNotify->InitNetDriver(GameServerConnectionState->NetDriver);
		GameServerConnectionState->GameServerNotify->SetFirstPlayer(NewPlayer);
		GameServerConnectionState->GameServerNotify->SetProxyNetDriver(ProxyNetDriver);
		GameServerConnectionState->GameServerNotify->SetFlags(Flags);
		GameServerConnection = GameServerConnectionState->NetDriver->ServerConnection;

		// UNetDriver::Notify will be reset in UPendingNetGame above so it's important that we override
		// it here again to point to the proxy.
		GameServerConnectionState->NetDriver->Notify = GameServerConnectionState->GameServerNotify;

		UE_LOG(LogNetProxy, Log, TEXT("Connecting to game server: %s:%s -> %s (player=%s client_handshake_id=%u)"), 
			   *ProxyConnection->GetDriver()->GetName(), 
			   *ProxyConnection->GetName(),
			   *GameServerConnection->GetDriver()->GetName(),
			   *NewPlayer->GetName(),
			   ClientHandshakeId);
	}
	else
	{
		FGameServerSplitJoinRequest Request;
		Request.Player = NewPlayer;
		Request.Flags = Flags;
		Request.ClientHandshakeId = ClientHandshakeId;

		// The NMT_JoinSplit message can only be sent when the parent connection is open.
		if (GameServerConnectionState->NetDriver->ServerConnection->GetConnectionState() == USOCK_Open)
		{
			GameServerConnectionState->PendingSplitJoinRequests.Add(Request);
			ProxyNetDriver->FlushSplitJoinRequests(GameServerConnectionState);

			UE_LOG(LogNetProxy, Log, TEXT("Connecting to game server (multiplexed): %s:%s -> %s (player=%s client_handshake_id=%u"), 
				   *ProxyConnection->GetDriver()->GetName(), 
				   *ProxyConnection->GetName(),
				   *GameServerConnection->GetDriver()->GetName(),
				   *NewPlayer->GetName(),
				   Request.ClientHandshakeId);
		}
		else
		{
			GameServerConnectionState->PendingSplitJoinRequests.Add(Request);
	
			UE_LOG(LogNetProxy, Log, TEXT("Connecting to game server (queued multiplexed): %s:%s -> %s (player=%s client_handshake_id=%u)"), 
				   *ProxyConnection->GetDriver()->GetName(),
				   *ProxyConnection->GetName(),
				   *GameServerConnection->GetDriver()->GetName(),
				   *NewPlayer->GetName(),
				   Request.ClientHandshakeId);
		}
	}

	GameServerConnectionState->Players.Add(NewPlayer);
		
	// Associate this proxy connection with the parent game server connection when beginning the handshake because
	// the child connection hasn't been created yet. Once the handshake is complete, it's expected that this entry will
	// be updated with the new child connection.
	FMultiServerProxyInternalConnectionRoute Route;
	Route.ProxyConnection = ProxyConnection;
	Route.Player = NewPlayer;
	Route.ParentGameServerConnection = GameServerConnection;

	ProxyNetDriver->ClientHandshakeIdToInternalRoute.Add(ClientHandshakeId, Route);

	if (!EnumHasAllFlags(Flags, UE::Net::EJoinFlags::NoPawn))
	{
		ProxyNetDriver->RegisterPrimaryGameServer(ProxyConnection, NewPlayer, GameServerConnectionStateIndex);
	}

	ProxyNetDriver->ProxyConnectionState[ProxyConnection] = EProxyConnectionState::ConnectingPrimary;
}

void UProxyBackendNetConnection::HandleClientPlayer(APlayerController* NewPlayerController, UNetConnection* GameServerConnection)
{
	// This function is called when a PlayerController is replicated to the proxy from a game server and represents the finalization
	// of a connection to a primary or non-primary game server.

	ensure(GameServerConnection == this);

	UProxyBackendNetDriver* BackendNetDriver = Cast<UProxyBackendNetDriver>(Driver);
	if (ensure(BackendNetDriver))
	{
		BackendNetDriver->GetProxyNetDriver()->GameServerAssignPlayerController(this, GameServerConnection, NewPlayerController);
	}
}

void UProxyBackendChildNetConnection::HandleClientPlayer(APlayerController* NewPlayerController, UNetConnection* GameServerConnection)
{
	// This function is called when a PlayerController is replicated to the proxy from a game server and represents the finalization
	// of a connection to a primary or non-primary game server.

	ensure(GameServerConnection != this);

	UProxyBackendNetDriver* BackendNetDriver = Cast<UProxyBackendNetDriver>(Driver);
	if (ensure(BackendNetDriver))
	{
		BackendNetDriver->GetProxyNetDriver()->GameServerAssignPlayerController(this, GameServerConnection, NewPlayerController);
	}
}

void UProxyBackendNetDriver::SetProxyNetDriver(TObjectPtr<UProxyNetDriver> InProxyNetDriver)
{
	ProxyNetDriver = InProxyNetDriver;
}

TObjectPtr<UProxyNetDriver> UProxyBackendNetDriver::GetProxyNetDriver()
{
	return ProxyNetDriver;
}

bool UProxyBackendNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	bool bSuccess = Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);

	if (bSuccess)
	{
		NetConnectionClass = UProxyBackendNetConnection::StaticClass();

		// Don't allow any RPCs received game servers to be executed on the proxy.
		EnableExecuteRPCFunctions(false);
	}

	SetReplicateTransactionally(false);

	return bSuccess;
}

void UProxyBackendNetDriver::ForwardRemoteFunction(UObject* RootObject, UObject* SubObject, UFunction* Function, void* Parms)
{
	check(ProxyNetDriver);

	// This function is called when the proxy receives an RPC from a game server and will only forward the function on
	// to the frontend net driver if it is owned by a player that considers that game server the primary game server.

	AActor* OwningActor = Cast<AActor>(RootObject);
	if (!ensure(OwningActor != nullptr))
	{
		UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Ignoring %s on %s from game server %s because it has no owner."), *Function->GetName(), *RootObject->GetName(), *GetName());
		return;
	}

	// If the owner is a PlayerController it is one that represents a connection to a game server and must be mapped
	// to a PlayerController for an incoming proxy connection. If no such mapping exists it means that the RPC comes 
	// from a game server that isn't the primary game server.
	if (APlayerController* GameServerController = Cast<APlayerController>(OwningActor))
	{
		APlayerController* ProxyController = ProxyNetDriver->GetProxyControllerFromPrimaryGameServerController(GameServerController);
		if (ProxyController == nullptr)
		{
			UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Ignoring %s on %s from game server %s because it's owning game server controller %s cannot map to a proxy controller."), *Function->GetName(), *RootObject->GetName(), *GetName(), *GameServerController->GetName());
			return;
		}
		else
		{
			UE_LOG(LogNetProxy, VeryVerbose, TEXT("RPC: Remapping game server controller %s to proxy controller %s when forwarding RPC %s on %s."), *GameServerController->GetName(), *ProxyController->GetName(), *Function->GetName(), *RootObject->GetName());
		}

		OwningActor = ProxyController;
	}

	ULocalPlayer* OwningPlayer = Cast<ULocalPlayer>(OwningActor->GetNetOwningPlayerAnyRole());
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Ignoring %s on %s from game server %s because it's owning actor %s doesn't have an owning player."), *Function->GetName(), *RootObject->GetName(), *GetName(), *OwningActor->GetName());
		return;
	}

	UE_LOG(LogNetProxy, VeryVerbose, TEXT("RPC: Pre-forwarding %s on %s from game server %s to proxy connection."), *Function->GetName(), *OwningActor->GetName(), *GetName());

	// The OwningActor actor will be associated with a connection (the return value of AActor::GetNetConnection()) to the backend game server, but
	// when UProxyNetDriver::InternalProcessRemoteFunction() is called the owning player will be used to lookup the actual proxy connection to
	// forward the RPC.
	ProxyNetDriver->ProcessRemoteFunction(OwningActor, Function, Parms, static_cast<FOutParmRec*>(nullptr), static_cast<FFrame*>(nullptr), SubObject);
}

bool UProxyBackendNetDriver::ShouldSkipRepNotifies() const
{
	return true;
}

UChildConnection* UProxyBackendNetDriver::CreateChild(UNetConnection* Parent)
{
	UChildConnection* Child = NewObject<UProxyBackendChildNetConnection>();
	Child->InitChildConnection(this, Parent);
	Parent->Children.Add(Child);
	return Child;
}


void UProxyBackendNetDriver::InternalProcessRemoteFunction(
	class AActor* Actor,
	class UObject* SubObject,
	class UNetConnection* Connection,
	class UFunction* Function,
	void* Parms,
	FOutParmRec* OutParms,
	FFrame* Stack,
	bool bIsServer)
{
	bool bShouldForwardRPC = true;

	// The majority of the actors on the proxy are replicated from the game servers so their owners will be game server player
	// controllers. The exception are proxy player controllers which are spawned by the proxy and will have an owner that is
	// the proxy player controller. In this case we must forward the RPC to the primary game server player controller.
	if (ProxyNetDriver->IsProxySpawned(Actor))
	{
		AActor* MappedActor = Actor;

		if (APlayerController* ProxyPlayerController = Cast<APlayerController>(MappedActor))
		{
			if (ULocalPlayer* Player = Cast<ULocalPlayer>(ProxyPlayerController->Player))
			{
				if (TObjectPtr<APlayerController>* GameServerPlayerControllerPtr = ProxyNetDriver->ProxyPlayerToPrimaryGameServerPlayerController.Find(Player))
				{
					UNetConnection* GameServerConnection = GameServerPlayerControllerPtr->Get()->NetConnection;
					if (ensure(GameServerConnection->PlayerController))
					{
						ensure(GameServerConnection->PlayerController->Player == Player);
						MappedActor = GameServerConnection->PlayerController;

						UE_LOG(LogNetProxy, VeryVerbose, TEXT("RPC: Remapping proxy player controller %s to game server player controller %s when forwarding RPC %s on %s"),
							   *ProxyPlayerController->GetName(),
							   *GameServerConnection->PlayerController->GetName(),
							   *Function->GetName(),
							   *Actor->GetName());
					}
				}
			}
		}
		
		if (ProxyNetDriver->IsProxySpawned(MappedActor))
		{
			bShouldForwardRPC = false;

			UE_LOG(LogNetProxy, Warning, TEXT("RPC: Unable to map proxy owned actor %s to game server %s owned actor."), *Actor->GetName(), *Connection->GetName());
		}
		else
		{
			Actor = MappedActor;
		}
	}

	// If the sub-object is not owned by the actor, attempt to find an component in that actor that matches the same type.
	// This logic assumes that an actor only has one component of a given type and will fail if that assumption is incorrect.
	if (UActorComponent* SubObjectAsActorComponent = Cast<UActorComponent>(SubObject))
	{
		if (SubObjectAsActorComponent->GetOwner() != Actor)
		{
			int32 MatchingComponents = 0;
			UActorComponent* MappedActorComponent = SubObjectAsActorComponent;

			for (UActorComponent* ActorComponent : Actor->GetComponents())
			{
				if (ActorComponent->GetClass() == SubObjectAsActorComponent->GetClass())
				{
					MappedActorComponent = ActorComponent;
					MatchingComponents++;

					UE_LOG(LogNetProxy, VeryVerbose, TEXT("RPC: Remapping sub-object %s to sub-object %s in actor %s"),
						*SubObjectAsActorComponent->GetName(),
						*MappedActorComponent ->GetName(),
						*Actor->GetName());
				}
			}

			if (MappedActorComponent->GetOwner() != Actor)
			{
				bShouldForwardRPC = false;

				UE_LOG(LogNetProxy, Warning, TEXT("RPC: Unable map sub-object %s to actor %s"),
					   *SubObjectAsActorComponent->GetName(),
					   *Actor->GetName());
			}

			// Detect an actor with two components of the same type.
			else if (MatchingComponents > 1)
			{
				bShouldForwardRPC = false;

				UE_LOG(LogNetProxy, Warning, TEXT("RPC: Found an actor %s with more than one component %s."), *Actor->GetName(), *SubObject->GetName());
			}
			else
			{
				SubObject = MappedActorComponent;
			}
		}
	}

	if (bShouldForwardRPC)
	{
		UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Forwarding %s on %s (owner:%s sub-object:%s) to game server connection %s:%s"), 
			   *Function->GetName(), 
			   *Actor->GetName(), 
			   *GetNameSafe(Actor->GetOwner()),
			   *GetNameSafe(SubObject),
			   *Connection->GetDriver()->GetName(),
			   *Connection->GetName());

		Super::InternalProcessRemoteFunction(Actor, SubObject, Connection, Function, Parms, OutParms, Stack, bIsServer);
	}
	else
	{
		UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Ignoring %s on %s (sub-object:%s) to game server connection %s."), 
			   *Function->GetName(), 
			   *Actor->GetName(), 
			   *GetNameSafe(SubObject),
			   *Connection->GetName());
	}
}

void UProxyNetDriver::RegisterGameServer(int32 Port)
{
	UE_LOG(LogNetProxy, Log, TEXT("Registering proxy game server on port %d"), Port);

	FGameServerConnectionState GameServerConnectionState;
	GameServerConnectionState.Port = Port;
	GameServerConnections.Add(GameServerConnectionState);
}

bool UProxyNetDriver::IsConnectedToAllGameServers() const
{
	for (const FGameServerConnectionState& GameServerConnectionState : GameServerConnections)
	{
		if (GameServerConnectionState.NetDriver == nullptr ||
			GameServerConnectionState.NetDriver->ServerConnection == nullptr ||
			GameServerConnectionState.NetDriver->ServerConnection->GetConnectionState() != EConnectionState::USOCK_Open)
		{
			return false;
		}
	}

	return true;
}

int32 UProxyNetDriver::GetGameServerConnectionCount() const
{
	return GameServerConnections.Num();
}

FGameServerConnectionState* UProxyNetDriver::GetGameServerConnection(int32 Index)
{
	if (ensure(Index < GameServerConnections.Num()))
	{
		return &GameServerConnections[Index];
	}

	return nullptr;
}

bool UProxyNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	check(!bInitAsClient);

	UE_LOG(LogNetProxy, Log, TEXT("Initializing ProxyNetDriver with URL %s"), *URL.ToString());

	ProxyListenerNotify = NewObject<UProxyListenerNotify>(GEngine, UProxyListenerNotify::StaticClass());
	ProxyListenerNotify->SetProxyNetDriver(this);

	const bool bSuccess = Super::InitBase(bInitAsClient, ProxyListenerNotify, URL, bReuseAddressAndPort, Error);

	FString GameServerAddresses;
	if (FParse::Value(FCommandLine::Get(), TEXT("-ProxyGameServers="), GameServerAddresses, false))
	{
		TArray<FString> Addresses;
		GameServerAddresses.ParseIntoArray(Addresses, TEXT(","), true);

		for (const FString& Address : Addresses)
		{
			FString IPAddressStr, PortStr;
			Address.Split(TEXT(":"), &IPAddressStr, &PortStr);

			int32 Port = FCString::Atoi(ToCStr(PortStr));
			RegisterGameServer(Port);
		}
	}

	FString ClientPrimaryGameServer;
	if (FParse::Value(FCommandLine::Get(), TEXT("ProxyClientPrimaryGameServer="), ClientPrimaryGameServer))
	{
		PrimaryGameServerForNextClient = FCString::Atoi(ToCStr(ClientPrimaryGameServer));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ProxyCyclePrimaryGameServer")))
	{
		bCyclePrimaryGameServer = true;
	}

	SetReplicateTransactionally(false);
	
	DisableActorLogicAndGameCode();

	return bSuccess;
}

bool UProxyNetDriver::InitConnect(class FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	checkf(0, TEXT("UProxyNetDriver is only intended to be used to receive connections and not establish outgoing connections."));
	return false;
}

void UProxyNetDriver::ForwardRemoteFunction(UObject* RootObject, UObject* SubObject, UFunction* Function, void* Parms)
{
	// This function is called when the proxy receives an RPC from a game client and will forward the function on
	// to the owning player's primary game server.

	// If the owner is a PlayerController it will be for a proxy connection. There is no need to map it to the PlayerController
	// on the game server because UNetDriver::ProcessRemoteFunction() will automatically send the RPC to the server connection
	// and ignores the value of AActor::GetNetConnection().
	AActor* OwningActor = Cast<AActor>(RootObject);
	if (!ensure(OwningActor != nullptr))
	{
		UE_LOG(LogNetProxy, Warning, TEXT("RPC: Ignoring %s on %s from proxy connection because it doesn't have an owner."), *Function->GetName(), *RootObject->GetName());
		return;
	}

	ULocalPlayer* OwningPlayer = Cast<ULocalPlayer>(OwningActor->GetNetOwningPlayerAnyRole());
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogNetProxy, Warning, TEXT("RPC: Ignoring %s on %s from proxy connection because it owner %s doesn't have an owning player"), *Function->GetName(), *RootObject->GetName(), *OwningActor->GetName());
		return;
	}

	UE_LOG(LogNetProxy, VeryVerbose, TEXT("RPC: Pre-forwarding %s on %s from proxy connection to game server."), *Function->GetName(), *OwningActor->GetName());

	if (TObjectPtr<APlayerController>* GamePlayerControllerPtr = ProxyPlayerToPrimaryGameServerPlayerController.Find(OwningPlayer))
	{
		UNetConnection* GameConnection = GamePlayerControllerPtr->Get()->NetConnection;
		if (GameConnection)
		{
			GameConnection->Driver->ProcessRemoteFunction(OwningActor, Function, Parms, static_cast<FOutParmRec*>(nullptr), static_cast<FFrame*>(nullptr), SubObject);
		}
	}
	else
	{
		UE_LOG(LogNetProxy, Warning, TEXT("RPC: Unable to forward %s on %s because player %s isn't mapped to a primary game server."), *Function->GetName(), *OwningActor->GetName(), *OwningPlayer->GetName());
	}
}

bool UProxyNetDriver::ShouldReplicateFunction(AActor* Actor, UFunction* Function) const
{
	// If any game code in the proxy world attempts to send an RPC it should not be called.
	return false;
}

void UProxyNetDriver::NotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor)
{
	Super::NotifyActorChannelOpen(Channel, Actor);

	// The actor roles in the proxy must be replicated to the client as-is (i.e. the role on the client
	// must be the same as the role in the proxy). Since the client will always swap roles when receiving
	// replicated objects and the proxy is transparent to the client, the role is swapped on the proxy 
	// before replicating.
	SetRoleSwapOnReplicate(Actor, true);
}

void UProxyNetDriver::AddNetworkActor(AActor* Actor)
{
	// Ideally the proxy shouldn't spawn any actors since it's just used as a cache to pass state
	// between game clients and game servers. For now though, actors that have the role ROLE_Authority
	// will have replication disabled and the role set to ROLE_None. This stops them replicating to
	// clients and disable any game actor code that only performs when the role is ROLE_Authority.
	//
	// It's important to note this function is called for all actors spawned on the client, both ones
	// loaded by the proxy and those replicated from the connected game servers. It's assumed that 
	// the actors replicated from the game servers will not have a role of ROLE_Authority and will
	// therefore be unaffacted by this code and replicate as normal.
	//
	// Actors that are explicitly spawned as part of the proxy functionality are allowed to be replicated.
	if (Actor->GetIsReplicated() && !IsProxySpawned(Actor))
	{
		if (Actor->GetLocalRole() == ROLE_Authority)
		{
			Actor->SetReplicates(false);
			Actor->SetRole(ROLE_None);
		}
	}

	Super::AddNetworkActor(Actor);
}

bool UProxyNetDriver::ShouldCallRemoteFunction(UObject* Object, UFunction* Function, const FReplicationFlags& RepFlags) const
{
	return !RepFlags.bIgnoreRPCs;
}

void UProxyNetDriver::InternalProcessRemoteFunction(
	class AActor* Actor,
	class UObject* SubObject,
	class UNetConnection* Connection,
	class UFunction* Function,
	void* Parms,
	FOutParmRec* OutParms,
	FFrame* Stack,
	bool bIsServer)
{
	if (ULocalPlayer* Player = Cast<ULocalPlayer>(Actor->GetNetOwningPlayerAnyRole()))
	{
		// RPCs from game servers will be routed to proxy connections. Only RPCs from a connections primary game server will
		// be routed to the game client; other RPCs will be ignored.
		if (UNetConnection* ProxyConnection = GetProxyConnectionFromPrimaryPlayer(Player))
		{
			UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Forwarding %s on %s to proxy connection %s for player %s"), *Function->GetName(), *Actor->GetName(), *ProxyConnection->GetName(), *Player->GetName());
			Super::InternalProcessRemoteFunction(Actor, SubObject, ProxyConnection, Function, Parms, OutParms, Stack, bIsServer);
		}
		else
		{
			UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Ignoring %s on %s because player %s isn't a primary player."), *Function->GetName(), *Actor->GetName(), *Player->GetName());
		}
	}
	else
	{
		UE_LOG(LogNetProxy, Verbose, TEXT("RPC: Ignoring %s on %s because there is no actor owning player."), *Function->GetName(), *Actor->GetName());
	}
}

int32 UProxyNetDriver::ServerReplicateActors(float DeltaSeconds)
{
	// The owners are actors in the world are going to be game server player controlers. Since these controllers 
	// aren't replicated ot the client, and the client has it's own proxy player controller, we need to map an
	// acor's game server player controller owner to the corresponding proxy player controller during replication.
	TMap<AActor*, AActor*> OriginalActorOwners;
	for (const TSharedPtr<FNetworkObjectInfo>& ObjectInfo : GetNetworkObjectList().GetAllObjects())
	{
		AActor* Owner = ObjectInfo->Actor->Owner;
		if (APlayerController* GameServerController = Cast<APlayerController>(Owner))
		{
			OriginalActorOwners.Add(ObjectInfo->Actor, GameServerController);

			APlayerController* ProxyController = GetProxyControllerFromPrimaryGameServerController(GameServerController);
			AActor* Actor = const_cast<TSharedPtr<FNetworkObjectInfo>*>(&ObjectInfo)->Get()->Actor;
			Actor->Owner = ProxyController;
		}
	}

	int32 ActorCount = Super::ServerReplicateActors(DeltaSeconds);

	// Restore actor owners to game server player controllers.
	for (TPair<AActor*, AActor*>& Pair : OriginalActorOwners)
	{
		AActor* OriginalActor = Pair.Key;
		AActor* OriginalOwner = Pair.Value;
		if (const TSharedPtr<FNetworkObjectInfo>* ObjectInfo = GetNetworkObjectList().GetAllObjects().Find(OriginalActor))
		{
			AActor* Actor = const_cast<TSharedPtr<FNetworkObjectInfo>*>(ObjectInfo)->Get()->Actor;
			Actor->Owner = OriginalOwner;
		}
	}

	return ActorCount;
}

bool UProxyNetDriver::CanDowngradeActorRole(UNetConnection* ProxyConnection, AActor* Actor) const
{
	if (ULocalPlayer* Player = Cast<ULocalPlayer>(Actor->GetNetOwningPlayerAnyRole()))
	{
		// If this autonomous actor is owned by a player that is bound to the same proxy connection
		// as the attached proxy player controller, don't downgrade from ROLE_AutonomousProxy to ROLE_SimulatedProxy.
		if (Actor->GetRemoteRole() == ROLE_AutonomousProxy)
		{
			if (UNetConnection* PlayerProxyConnection = GetProxyConnectionFromPrimaryPlayer(Player))
			{
				if (PlayerProxyConnection == ProxyConnection)
				{
					return false;
				}
			}

			return true;
		}
	}

	return false;
}

void UProxyNetDriver::Shutdown()
{
	for (FGameServerConnectionState& ConnectionState : GameServerConnections)
	{
		GEngine->DestroyNamedNetDriver(ConnectionState.World, ConnectionState.NetDriverName);
		ConnectionState.GameServerNotify->NetDriver = nullptr;
	}

	GameServerConnections.Reset();

	Super::Shutdown();
}

int32 UProxyNetDriver::GetNextGameServerClientId()
{
	return NextGameServerClientId++;
}

uint32 UProxyNetDriver::GetNextClientHandshakeId()
{
	return NextClientHandshakeId++;
}

void UProxyNetDriver::DisableActorLogicAndGameCode()
{
	// The proxy should only be replicating the exact state from the servers and passing it on
	// to clients and not tick actors or call any user actor callbacks.

	EnableExecuteRPCFunctions(false);
	EnablePreReplication(false);

#if UE_SUPPORT_FOR_ACTOR_TICK_DISABLE
	GetWorld()->EnableActorTickAndUserCallbacks(false);
#endif
}

bool UProxyNetDriver::IsProxySpawned(AActor* Actor) const
{
	return ProxySpawnedActors.Contains(Actor);
}

void UProxyNetDriver::GameServerAssignPlayerController(UNetConnection* ChildGameServerConnection, UNetConnection* NotUsedParentGameServerConnection, APlayerController* GameServerPlayerController)
{
	UE_LOG(LogNetProxy, Log, TEXT("Received a new player controller %s:%s for connection %s:%s (client_handshake_id=%u)."), 
		   *GameServerPlayerController->GetName(), *GameServerPlayerController->GetClass()->GetName(),
		   *ChildGameServerConnection->GetDriver()->GetName(), *ChildGameServerConnection->GetName(),
		   GameServerPlayerController->GetClientHandshakeId());
	
	// The game server player controller is used as a client connection to the game server so must be ROLE_AutonomousProxy.
	// NOTE: A proxy player controller will still be ROLE_Authority since it's spawned by the proxy.
	GameServerPlayerController->SetRole(ROLE_AutonomousProxy);

	const uint32 ClientHandshakeId = GameServerPlayerController->GetClientHandshakeId();
	FMultiServerProxyInternalConnectionRoute* RoutePtr = ClientHandshakeIdToInternalRoute.Find(ClientHandshakeId);
	if (RoutePtr == nullptr)
	{
		UE_LOG(LogNetProxy, Error, TEXT("Failed to find a mapping for game server connection request (client_handshake_id=%u)"), ClientHandshakeId);
		return;
	}

	// Cache this value locally incase ClientHandshakeIdToInternalRoute changes and invalidates the pointer.
	FMultiServerProxyInternalConnectionRoute Route = *RoutePtr;
	
	UE_LOG(LogNetProxy, Log, TEXT("Found internal route (client_handshake_id=%u player=%s proxy_connection=%s:%s parent_game_server_connection=%s:%s)"), 
		   ClientHandshakeId,
		   *GetNameSafe(Route.Player),
			Route.ProxyConnection ? *GetNameSafe(Route.ProxyConnection->GetDriver()) : TEXT("None"),
		   *GetNameSafe(Route.ProxyConnection),
			Route.ParentGameServerConnection ? *GetNameSafe(Route.ParentGameServerConnection->GetDriver()) : TEXT("None"),
		   *GetNameSafe(Route.ParentGameServerConnection));

	ULocalPlayer* Player = Route.Player;
		
	// The player should only reference a proxy player controller if this game server connection
	// is the primary game server for the proxy connection (decided later in this function).
	Player->PlayerController = nullptr;
	
	// If a new player controller is spawned on the proxy from a game server it's assumed it will replace any
	// existing game server player controllers used by connections and players on the proxy. For this reason
	// always detach an existing game server player controllers from the connection.
	DetachPlayerController(ChildGameServerConnection->PlayerController);
	AttachPlayerController(GameServerPlayerController, ChildGameServerConnection, Player);

	// Find the proxy connection that established this connection to the game server.
	TObjectPtr<UNetConnection> ProxyConnection = Route.ProxyConnection;

	if (ensure(ProxyConnectionState.Contains(ProxyConnection)))
	{
		EProxyConnectionState State = ProxyConnectionState[ProxyConnection];

		UE_LOG(LogNetProxy, Log, TEXT("Attempting to assign game server player controller %s to proxy connection %s:%s (state=%s)"),
			*GetNameSafe(GameServerPlayerController),
			ProxyConnection ? *GetNameSafe(ProxyConnection->GetDriver()) : TEXT("None"),
			*GetNameSafe(ProxyConnection),
			*LexToString(State));

		switch (State)
		{
			case EProxyConnectionState::Disconnected:
				break;

			case EProxyConnectionState::ConnectingPrimary:
				{
					if (int32* PlayerPrimaryGameServerIdxPtr = ProxyPlayerToPrimaryGameServer.Find(Player))
					{
						int32 PlayerPrimaryGameServerIdx = *PlayerPrimaryGameServerIdxPtr;

						ensure(ProxyConnection->PlayerController == nullptr);

						if (ensure(PlayerPrimaryGameServerIdx < GameServerConnections.Num()))
						{
							FGameServerConnectionState& PlayerPrimaryGameServer = GameServerConnections[PlayerPrimaryGameServerIdx];
							if (PlayerPrimaryGameServer.NetDriver->ServerConnection == Route.ParentGameServerConnection)
							{
								UPlayer* PreviousPlayer = GameServerPlayerController->Player;
			
								APlayerController* ProxyPlayerController = SpawnPlayerController();
								if (ensure(ProxyPlayerController))
								{
									DetachAndDestroyPlayerController(ProxyConnection->PlayerController);
									AttachPlayerController(ProxyPlayerController, ProxyConnection, Player);

									// The player always points to the proxy player controller if that player is
									// considered the primary player for the proxy connection.
									Player->PlayerController = ProxyPlayerController;

									ProxyPlayerToPrimaryGameServerPlayerController.Add(Player, GameServerPlayerController);

									UE_LOG(LogNetProxy, Log, TEXT("Successfully connected proxy connection %s:%s to primary game server connection %s:%s (game_server_controller=%s:%s proxy_controller=%s:%s player=%s client_handshake_id=%u)"),
										*ProxyConnection->GetDriver()->GetName(),
										*ProxyConnection->GetName(),
										*ChildGameServerConnection->GetDriver()->GetName(),
										*ChildGameServerConnection->GetName(),
										*GameServerPlayerController->GetName(),
										*GameServerPlayerController->GetClass()->GetName(),
										*ProxyPlayerController->GetName(),
										*ProxyPlayerController->GetClass()->GetName(),
										*Player->GetName(),
										ClientHandshakeId);

									ProxyConnectionState[ProxyConnection] = EProxyConnectionState::ConnectedPrimary;
								}
							}
						}
					}
					else
					{
						UE_LOG(LogNetProxy, Log, TEXT("Game server player controller %s not from proxy connection %s:%s primary game server"),
							*GameServerPlayerController->GetName(),
							*ProxyConnection->GetDriver()->GetName(),
							*ProxyConnection->GetName());
					}
				}
				break;

			case EProxyConnectionState::ConnectedPrimary:
				{
					if (!GameServerPlayerController->IsA(ANoPawnPlayerController::StaticClass()))
					{
						if (ULocalPlayer* ExistingPrimaryPlayer = Cast<ULocalPlayer>(ProxyConnection->PlayerController->Player))
						{
							if (ProxyPlayerToPrimaryGameServerPlayerController.Contains(ExistingPrimaryPlayer))
							{
								int32* PreviousPrimaryGameServerIdxPtr = ProxyPlayerToPrimaryGameServer.Find(ExistingPrimaryPlayer);
								if (ensure(PreviousPrimaryGameServerIdxPtr))
								{
									int32 PreviousPrimaryGameServerIdx = *PreviousPrimaryGameServerIdxPtr;

									if (Player->GetCachedUniqueNetId() == ExistingPrimaryPlayer->GetCachedUniqueNetId())
									{
										int32 NewPrimaryGameServerIdx = GetGameServerWithPlayer(Player);

										UE_LOG(LogNetProxy, Log, TEXT("Changing primary game server for proxy connection %s:%s from player %s to %s (prev_game_server_idx=%d new_game_server_idx=%d)."),
											*ProxyConnection->Driver->GetName(),
											*ProxyConnection->GetName(),
											*ExistingPrimaryPlayer->GetName(),
											*Player->GetName(),
											PreviousPrimaryGameServerIdx,
											NewPrimaryGameServerIdx);
					
										ProxyPlayerToPrimaryGameServerPlayerController.Remove(ExistingPrimaryPlayer);

										DeregisterPrimaryGameServer(ProxyConnection);
										RegisterPrimaryGameServer(ProxyConnection, Player, NewPrimaryGameServerIdx);
				
										ExistingPrimaryPlayer->PlayerController = nullptr;
										Player->PlayerController = ProxyConnection->PlayerController;
										ProxyConnection->PlayerController->Player = Player;

										ProxyPlayerToPrimaryGameServerPlayerController.Add(Player, GameServerPlayerController);
									}
								}
							}
						}
					}
				}
				break;

		}
	}
			
	// Update the internal connection mapping to use the new child game server connection (if it's different from the parent connection).
	ClientHandshakeIdToInternalRoute.Remove(NextClientHandshakeId);
	ClientHandshakeIdToInternalRoute.Add(NextClientHandshakeId, Route);

	// If this was a parent connection send any join requests for any pending multiplexed connections.
	if (ChildGameServerConnection == Route.ParentGameServerConnection)
	{
		FGameServerConnectionState* CurrGameServerConnectionState = nullptr;
		for (FGameServerConnectionState& GameServerConnectionState : GameServerConnections)
		{
			if (GameServerConnectionState.NetDriver->ServerConnection == Route.ParentGameServerConnection)
			{
				CurrGameServerConnectionState = &GameServerConnectionState;
				break;
			}
		}

		if (ensure(CurrGameServerConnectionState))
		{
			FlushSplitJoinRequests(CurrGameServerConnectionState);
		}
	}
}

void UProxyNetDriver::FlushSplitJoinRequests(FGameServerConnectionState* GameServerConnectionState)
{
	EConnectionState ParentConnectionState = GameServerConnectionState->NetDriver->ServerConnection->GetConnectionState();
	if (!(ParentConnectionState == EConnectionState::USOCK_Open))
	{
		UE_LOG(LogNetProxy, Error, TEXT("Flushing split join requests on %s without the parent connection being opened."), 
			   *GameServerConnectionState->NetDriver->GetName());

		return;
	}

	UE_LOG(LogNetProxy, Log, TEXT("Flushing %d split join connection requests for connection %s:%s."), 
		   GameServerConnectionState->PendingSplitJoinRequests.Num(), *GameServerConnectionState->NetDriver->GetName(), *GameServerConnectionState->NetDriver->ServerConnection->GetName());

	for (FGameServerSplitJoinRequest& Request : GameServerConnectionState->PendingSplitJoinRequests)
	{
		UE_LOG(LogNetProxy, Log, TEXT("Sending queued connection (multiplexed) request to game server: %s (player=%s flags=%d client_handshake_id=%u)"),
			   *GameServerConnectionState->NetDriver->GetName(),
			   *Request.Player->GetName(),
			   Request.Flags,
			   Request.ClientHandshakeId);

		TArray<FString> Options;
		Options.Add(FString::Printf(TEXT("HandshakeId=%u"), Request.ClientHandshakeId));
		Request.Player->SendSplitJoin(Options, GameServerConnectionState->NetDriver, Request.Flags);
	}

	GameServerConnectionState->PendingSplitJoinRequests.Reset();
}

int32 UProxyNetDriver::GetGameServerWithPlayer(ULocalPlayer* Player)
{
	for (int32 GameServerIdx = 0; GameServerIdx < GameServerConnections.Num(); GameServerIdx++)
	{
		if (GameServerConnections[GameServerIdx].Players.Find(Player) != INDEX_NONE)
		{
			return GameServerIdx;
		}
	}

	return -1;
}

void UProxyNetDriver::DeregisterPrimaryGameServer(UNetConnection* ProxyConnection)
{
	if (ensure(ProxyConnection->PlayerController))
	{
		ULocalPlayer* OldPlayer = Cast<ULocalPlayer>(ProxyConnection->PlayerController->Player);
		if (OldPlayer)
		{
			FUniqueNetIdRepl& PlayerId = ProxyConnection->PlayerId;

			UE_LOG(LogNetProxy, Log, TEXT("Clearing player %s primary game server (proxy_connection=%s:%s)"),
				   *OldPlayer->GetName(),
				   *ProxyConnection->GetDriver()->GetName(),
				   *ProxyConnection->GetName());

			ensure(ProxyPlayerToPrimaryGameServer.Contains(OldPlayer));
		
			ProxyPlayerToPrimaryGameServer.Remove(OldPlayer);
		}
	}
}

void UProxyNetDriver::RegisterPrimaryGameServer(UNetConnection* ProxyConnection, class ULocalPlayer* PrimaryPlayer, int32 GameServerConnectionStateIndex)
{
	if (ensure(GameServerConnectionStateIndex < GameServerConnections.Num()))
	{
		FUniqueNetIdRepl& PlayerId = ProxyConnection->PlayerId;

		UE_LOG(LogNetProxy, Log, TEXT("Configuring primary game server %d for player (player=%s proxy_connection=%s:%s)"),
			GameServerConnectionStateIndex,
			*PrimaryPlayer->GetName(),
			*ProxyConnection->GetDriver()->GetName(),
			*ProxyConnection->GetName());

		ensure(!ProxyPlayerToPrimaryGameServer.Contains(PrimaryPlayer));

		ProxyPlayerToPrimaryGameServer.Add(PrimaryPlayer, GameServerConnectionStateIndex);
	}
}

void UProxyNetDriver::AttachPlayerController(APlayerController* PlayerController, UNetConnection* Connection, ULocalPlayer* Player)
{
	// The presumed relationships between connection, player controller and player:
	//	
	//	ProxyConnection <-> ProxyPlayerController -> LocalPlayer
	//	LocalPlayer <- GameServerPlayerController <-> Game Server Connection
	//
	// This function doesn't associate the player with a player controller since it differs between
	// proxy and game server player controllers.

	UE_LOG(LogNetProxy, Log, TEXT("Attaching controller %s:%s on connection %s and player %s."),
		   *PlayerController->GetName(),
		   *PlayerController->GetClass()->GetName(),
		   *Connection->GetName(),
		   *Player->GetName());

	Connection->SetConnectionState(EConnectionState::USOCK_Open);
	Connection->SetClientHandshakeId(PlayerController->GetClientHandshakeId());

	// It's assumed the connection is not associated with a player controller.	
	ensure(Connection->PlayerController == nullptr);
	ensure(Connection->OwningActor == nullptr);

	Connection->PlayerController = PlayerController;
	Connection->OwningActor = PlayerController;
	
	PlayerController->NetConnection = Connection;
	PlayerController->Player = Player;
	
	Connection->LastReceiveTime = GetElapsedTime();
}

void UProxyNetDriver::DetachPlayerController(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		UPlayer* Player = PlayerController->Player;

		UE_LOG(LogNetProxy, Log, TEXT("Detaching old controller %s:%s on %s attached to player %s."),
			*PlayerController->GetName(),
			*PlayerController->GetClass()->GetName(),
			*GetName(),
			*Player->GetName());

		if (UNetConnection* Connection = PlayerController->NetConnection)
		{
			Connection->PlayerController = nullptr;
			Connection->OwningActor = nullptr;
		}

		PlayerController->NetConnection = nullptr;
		PlayerController->Player = nullptr;

		// Only detach the player controller from the player if it's the one referenced by the player.
		if (Player->PlayerController == PlayerController)
		{
			Player->PlayerController = nullptr;
		}
	}
}

void UProxyNetDriver::DetachAndDestroyPlayerController(APlayerController* PlayerController)
{
	DetachPlayerController(PlayerController);
	DestroyPlayerController(PlayerController);
}

APlayerController* UProxyNetDriver::SpawnPlayerController()
{
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();

	if (ensure(GameMode))
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		SpawnInfo.bDeferConstruction = true;
	
		APlayerController* PlayerController = GetWorld()->SpawnActor<APlayerController>(GameMode->PlayerControllerClass, SpawnInfo);

		if (PlayerController)
		{
			UE_LOG(LogNetProxy, Log, TEXT("Spawning player controller %s of type %s."), *PlayerController->GetName(), *GameMode->PlayerControllerClass->GetName());

			ProxySpawnedActors.Add(PlayerController);
	
			GetWorld()->AddController(PlayerController);

			PlayerController->SetRole(ROLE_Authority);
			PlayerController->SetReplicates(true);
			PlayerController->SetAutonomousProxy(true); // Sets AActor::RemoteRole.
			PlayerController->FinishSpawning(FTransform());
		}

		return PlayerController;
	}

	return nullptr;
}

void UProxyNetDriver::DestroyPlayerController(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		// Only player controllers spawned by the proxy should be destroyed.
		if (ensure(ProxySpawnedActors.Contains(PlayerController)))
		{
			if (ensure(PlayerController->GetLocalRole() == ROLE_Authority))
			{
				ProxySpawnedActors.Remove(PlayerController);

				PlayerController->GetWorld()->RemoveController(PlayerController);
				PlayerController->GetWorld()->DestroyActor(PlayerController);
			}
		}
	}
}

UNetConnection* UProxyNetDriver::GetProxyConnectionFromPrimaryPlayer(UPlayer* Player)
{
	if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
	{
		if (APlayerController* ProxyPlayerController = LocalPlayer->PlayerController)
		{
			return ProxyPlayerController->NetConnection;
		}
	}

	return nullptr;
}

APlayerController* UProxyNetDriver::GetProxyControllerFromPrimaryGameServerController(APlayerController* GameServerController)
{
	if (ULocalPlayer* Player = Cast<ULocalPlayer>(GameServerController->Player))
	{
		// The player's controller will always point to the proxy player controller.
		return Player->PlayerController;
	}

	return nullptr;
}
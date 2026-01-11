// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/DisplayClusterClusterManager.h"

#include "Cluster/DisplayClusterClusterEventHandler.h"
#include "Cluster/DisplayClusterGenericBarrierAPI.h"

#include "Cluster/IDisplayClusterClusterSyncObject.h"
#include "Cluster/IDisplayClusterClusterEventListener.h"

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlDisabled.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlEditor.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlMain.h"

#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlDisabled.h"
#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlEditor.h"
#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlMain.h"

#include "Cluster/NetAPI/DisplayClusterNetApiFacade.h"

#include "Config/IPDisplayClusterConfigManager.h"

#include "Dom/JsonObject.h"

#include "Misc/App.h"
#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"
#include "Misc/DisplayClusterTypesConverter.h"
#include "Misc/ScopeLock.h"

#include "UObject/Interface.h"

#include "DisplayClusterConfigurationTypes.h"
#include "IDisplayClusterCallbacks.h"


FDisplayClusterClusterManager::FDisplayClusterClusterManager()
	: NodeCtrl(MakeShared<FDisplayClusterClusterNodeCtrlDisabled>())
	, FailoverCtrl(MakeShared<FDisplayClusterFailoverNodeCtrlDisabled>(NodeCtrl))
	, NetApi(new FDisplayClusterNetApiFacade(FailoverCtrl))
{
	// Sync objects
	ObjectsToSync.Emplace(EDisplayClusterSyncGroup::PreTick).Reserve(64);
	ObjectsToSync.Emplace(EDisplayClusterSyncGroup::Tick).Reserve(64);
	ObjectsToSync.Emplace(EDisplayClusterSyncGroup::PostTick).Reserve(64);

	// Sync objects - replication
	ObjectsToSyncCache.Emplace(EDisplayClusterSyncGroup::PreTick);
	ObjectsToSyncCache.Emplace(EDisplayClusterSyncGroup::Tick);
	ObjectsToSyncCache.Emplace(EDisplayClusterSyncGroup::PostTick);

	ObjectsToSyncCacheReadySignals.Emplace(EDisplayClusterSyncGroup::PreTick,  FPlatformProcess::GetSynchEventFromPool(true));
	ObjectsToSyncCacheReadySignals.Emplace(EDisplayClusterSyncGroup::Tick,     FPlatformProcess::GetSynchEventFromPool(true));
	ObjectsToSyncCacheReadySignals.Emplace(EDisplayClusterSyncGroup::PostTick, FPlatformProcess::GetSynchEventFromPool(true));

	// Set cluster event handlers. These are the entry points for any incoming cluster events.
	OnClusterEventJson.AddRaw(this,   &FDisplayClusterClusterManager::OnClusterEventJsonHandler);
	OnClusterEventBinary.AddRaw(this, &FDisplayClusterClusterManager::OnClusterEventBinaryHandler);

	// Set internal system events handler
	OnClusterEventJson.Add(FDisplayClusterClusterEventHandler::Get().GetJsonListenerDelegate());
}

FDisplayClusterClusterManager::~FDisplayClusterClusterManager()
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Releasing cluster manager..."));

	// Trigger all data cache availability events to prevent client session threads to be deadlocked.
	SetInternalSyncObjectsReleaseState(true);

	// Stop networking in case it hasn't been stopped yet
	ReleaseNetworking();

	// Return sync event objects to the pool
	for (TPair<EDisplayClusterSyncGroup, FEvent*>& GroupEventIt : ObjectsToSyncCacheReadySignals)
	{
		FPlatformProcess::ReturnSynchEventToPool(GroupEventIt.Value);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterManager::Init(EDisplayClusterOperationMode OperationMode)
{
	CurrentOperationMode = OperationMode;
	return true;
}

void FDisplayClusterClusterManager::Release()
{
	CurrentOperationMode = EDisplayClusterOperationMode::Disabled;
}

bool FDisplayClusterClusterManager::StartSession(UDisplayClusterConfigurationData* InConfigData, const FString& InNodeId)
{
	ClusterNodeId = InNodeId;

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Node ID: %s"), *ClusterNodeId);

	// Node name must be valid
	if (ClusterNodeId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Node ID was not specified"));
		return false;
	}

	// Get configuration data
	const UDisplayClusterConfigurationData* const ConfigData = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	if (!ConfigData)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't get configuration data"));
		return false;
	}

	// Does it exist in the cluster configuration?
	if (!ConfigData->Cluster->Nodes.Contains(ClusterNodeId))
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Node '%s' not found in the configuration data"), *ClusterNodeId);
		return false;
	}

	// Subscribe for events
	GDisplayCluster->GetCallbacks().OnDisplayClusterFailoverPrimaryNodeChanged().AddRaw(this, &FDisplayClusterClusterManager::OnPrimaryNodeChangedHandler);
	GDisplayCluster->GetCallbacks().OnDisplayClusterFailoverNodeDown().AddRaw(this, &FDisplayClusterClusterManager::OnClusterNodeFailed);

	// Reset all internal sync objects
	SetInternalSyncObjectsReleaseState(false);

	// Save initial list of cluster nodes
	ConfigData->Cluster->Nodes.GetKeys(InitialClusterNodeIds);

	// Also, initialize the active nodes list
	{
		FScopeLock Lock(&ActiveClusterNodeIdsCS);
		ActiveClusterNodeIds = InitialClusterNodeIds;
	}

	// Determine cluster role for this instance
	InitializeClusterRole(InNodeId, ConfigData);

	// Set primary node
	SetPrimaryNode(ConfigData->Cluster->PrimaryNode.Id);

	// Initialize networking internals
	const bool bNetworkingInternalsInitialized = InitializeNetworking(ConfigData);
	if (!bNetworkingInternalsInitialized)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Node '%s' could not initialize networking subsystems"), *ClusterNodeId);
		return false;
	}

	return true;
}

void FDisplayClusterClusterManager::EndSession()
{
	// Unsubscribe from the session events
	GDisplayCluster->GetCallbacks().OnDisplayClusterFailoverPrimaryNodeChanged().RemoveAll(this);
	GDisplayCluster->GetCallbacks().OnDisplayClusterFailoverNodeDown().RemoveAll(this);

	// Trigger all data cache availability events to prevent
	// client session threads to be deadlocked.
	SetInternalSyncObjectsReleaseState(true);

	// Stop networking
	ReleaseNetworking();

	{
		FScopeLock Lock(&ActiveClusterNodeIdsCS);
		ActiveClusterNodeIds.Reset();
	}

	InitialClusterNodeIds.Reset();
	ClusterNodeId.Empty();
}

bool FDisplayClusterClusterManager::StartScene(UWorld* InWorld)
{
	check(InWorld);
	CurrentWorld = InWorld;

	return true;
}

void FDisplayClusterClusterManager::EndScene()
{
	{
		FScopeLock Lock(&ObjectsToSyncCS);
		for (auto& SyncGroupPair : ObjectsToSync)
		{
			SyncGroupPair.Value.Reset();
		}
	}

	{
		FScopeLock Lock(&ClusterEventListenersCS);
		ClusterEventListeners.Reset();
	}

	NativeInputCache.Reset();
	CurrentWorld = nullptr;
}

void FDisplayClusterClusterManager::StartFrame(uint64 FrameNum)
{
	// Even though this signal gets reset on EndFrame, it's still possible a client
	// will try to synchronize time data before the primary node finishes EndFrame
	// processing. Since time data replication step and EndFrame call don't have
	// any barriers between each other, it's theoretically possible a client will
	// get outdated time information which will break determinism. As a simple
	// solution that requires minimum resources, we do safe signal reset right
	// after WaitForFrameStart barrier, which is called after time data
	// synchronization. As a result, we're 100% sure the clients will always get
	// actual time data.
	TimeDataCacheReadySignal->Reset();
}

void FDisplayClusterClusterManager::EndFrame(uint64 FrameNum)
{
	// Reset all the synchronization objects
	SetInternalSyncObjectsReleaseState(false);

	// Reset cache containers
	JsonEventsCache.Reset();
	BinaryEventsCache.Reset();
	NativeInputCache.Reset();

	// Reset objects sync cache for all sync groups
	for (TPair<EDisplayClusterSyncGroup, TMap<FString, FString>>& It : ObjectsToSyncCache)
	{
		It.Value.Reset();
	}
}

void FDisplayClusterClusterManager::PreTick(float DeltaSeconds)
{
	// Sync cluster objects (PreTick)
	SyncObjects(EDisplayClusterSyncGroup::PreTick);

	// Sync cluster events
	SyncEvents();
}

void FDisplayClusterClusterManager::Tick(float DeltaSeconds)
{
	// Sync cluster objects (Tick)
	SyncObjects(EDisplayClusterSyncGroup::Tick);
}

void FDisplayClusterClusterManager::PostTick(float DeltaSeconds)
{
	// Sync cluster objects (PostTick)
	SyncObjects(EDisplayClusterSyncGroup::PostTick);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterManager::IsPrimary() const
{
	return HasClusterRole(EDisplayClusterNodeRole::Primary);
}

bool FDisplayClusterClusterManager::IsSecondary() const
{
	return HasClusterRole(EDisplayClusterNodeRole::Secondary);
}

bool FDisplayClusterClusterManager::IsBackup() const
{
	return HasClusterRole(EDisplayClusterNodeRole::Backup);
}

bool FDisplayClusterClusterManager::HasClusterRole(EDisplayClusterNodeRole Role) const
{
	return GetClusterRole() == Role;
}

EDisplayClusterNodeRole FDisplayClusterClusterManager::GetClusterRole() const
{
	FScopeLock Lock(&CurrentNodeRoleCS);
	return CurrentNodeRole;
}

FString FDisplayClusterClusterManager::GetPrimaryNodeId() const
{
	FScopeLock Lock(&PrimaryNodeIdCS);
	return PrimaryNodeId;
}

FString FDisplayClusterClusterManager::GetNodeId() const
{
	return ClusterNodeId;
}

uint32 FDisplayClusterClusterManager::GetNodesAmount() const
{
	FScopeLock Lock(&ActiveClusterNodeIdsCS);
	const int32 NodesNum = ActiveClusterNodeIds.Num();
	return static_cast<uint32>(NodesNum <= 0 ? 0 : NodesNum);
}

void FDisplayClusterClusterManager::GetNodeIds(TArray<FString>& OutNodeIds) const
{
	FScopeLock Lock(&ActiveClusterNodeIdsCS);
	OutNodeIds = ActiveClusterNodeIds.Array();
}

void FDisplayClusterClusterManager::GetNodeIds(TSet<FString>& OutNodeIds) const
{
	FScopeLock Lock(&ActiveClusterNodeIdsCS);
	OutNodeIds = ActiveClusterNodeIds;
}

bool FDisplayClusterClusterManager::DropClusterNode(const FString& NodeId)
{
	if (!IsPrimary())
	{
		UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Node drop is allowed on P-nodes only"));
		return false;
	}

	return DropNode(NodeId, IPDisplayClusterClusterManager::ENodeDropReason::UserRequest);
}

void FDisplayClusterClusterManager::RegisterSyncObject(IDisplayClusterClusterSyncObject* SyncObj, EDisplayClusterSyncGroup SyncGroup)
{
	if (SyncObj)
	{
		FScopeLock Lock(&ObjectsToSyncCS);
		ObjectsToSync[SyncGroup].Add(SyncObj);
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Registered sync object: %s"), *SyncObj->GetSyncId());
	}
}

void FDisplayClusterClusterManager::UnregisterSyncObject(IDisplayClusterClusterSyncObject* SyncObj)
{
	if (SyncObj)
	{
		FScopeLock Lock(&ObjectsToSyncCS);

		for (auto& GroupPair : ObjectsToSync)
		{
			GroupPair.Value.Remove(SyncObj);
		}

		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Unregistered sync object: %s"), *SyncObj->GetSyncId());
	}
}

TSharedRef<IDisplayClusterGenericBarriersClient> FDisplayClusterClusterManager::CreateGenericBarriersClient()
{
	return MakeShared<FDisplayClusterGenericBarrierAPI>();
}

void FDisplayClusterClusterManager::AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	if (Listener.GetObject() && IsValidChecked(Listener.GetObject()) && !Listener.GetObject()->IsUnreachable())
	{
		ClusterEventListeners.Add(Listener);
	}
}

void FDisplayClusterClusterManager::RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	if (ClusterEventListeners.Contains(Listener))
	{
		ClusterEventListeners.Remove(Listener);
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Cluster event listeners left: %d"), ClusterEventListeners.Num());
	}
}

void FDisplayClusterClusterManager::AddClusterEventJsonListener(const FOnClusterEventJsonListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventJson.Add(Listener);
}

void FDisplayClusterClusterManager::RemoveClusterEventJsonListener(const FOnClusterEventJsonListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventJson.Remove(Listener.GetHandle());
}

void FDisplayClusterClusterManager::AddClusterEventBinaryListener(const FOnClusterEventBinaryListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventBinary.Add(Listener);
}

void FDisplayClusterClusterManager::RemoveClusterEventBinaryListener(const FOnClusterEventBinaryListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventBinary.Remove(Listener.GetHandle());
}

void FDisplayClusterClusterManager::EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
{
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("JSON event emission request: %s"), *Event.ToString());

	FScopeLock Lock(&ClusterEventsJsonCS);

	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		// [Primary] Since we receive cluster events asynchronously, we push it to a primary events pool
		if (IsPrimary())
		{
			// Generate event ID
			const FString EventId = FString::Printf(TEXT("%s-%s-%s"), *Event.Category, *Event.Type, *Event.Name);
			// Make it shared ptr
			TSharedPtr<FDisplayClusterClusterEventJson> EventPtr = MakeShared<FDisplayClusterClusterEventJson>(Event);
			// Store event object
			if (EventPtr->bShouldDiscardOnRepeat)
			{
				ClusterEventsJson.FindOrAdd(EventPtr->bIsSystemEvent).Emplace(EventId, EventPtr);
			}
			else
			{
				ClusterEventsJsonNonDiscarded.Add(EventPtr);
			}
		}
		// [Secondary] Send event to the primary node
		else
		{
			// An event will be emitted from a secondary node if it's explicitly specified by bPrimaryOnly=false
			if (!bPrimaryOnly)
			{
				FailoverCtrl->EmitClusterEventJson(Event);
			}
		}
	}
}

void FDisplayClusterClusterManager::EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
{
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("BIN event emission request: %d"), Event.EventId);

	FScopeLock Lock(&ClusterEventsBinaryCS);

	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		// [Primary] Since we receive cluster events asynchronously, we push it to a primary events pool
		if (IsPrimary())
		{
			// Make it shared ptr
			TSharedPtr<FDisplayClusterClusterEventBinary> EventPtr = MakeShared<FDisplayClusterClusterEventBinary>(Event);

			if (EventPtr->bShouldDiscardOnRepeat)
			{
				ClusterEventsBinary.FindOrAdd(EventPtr->bIsSystemEvent).Emplace(EventPtr->EventId, EventPtr);
			}
			else
			{
				ClusterEventsBinaryNonDiscarded.Add(EventPtr);
			}
		}
		// [Secondary] Send event to the primary node
		else
		{
			// An event will be emitted from a secondary node if it's explicitly specified by bPrimaryOnly=false
			if (!bPrimaryOnly)
			{
				FailoverCtrl->EmitClusterEventBinary(Event);
			}
		}
	}
}

void FDisplayClusterClusterManager::SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
{
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		if (IsPrimary() || !bPrimaryOnly)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("JSON event emission request: recipient=%s:%u, event=%s:%s:%s"), *Address, Port, *Event.Category, *Event.Type, *Event.Name);
			NodeCtrl->SendClusterEventTo(Address, Port, Event, bPrimaryOnly);
		}
	}
}

void FDisplayClusterClusterManager::SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
{
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		if (IsPrimary() || !bPrimaryOnly)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("BIN event emission request: recipient=%s:%u, event=%d"), *Address, Port, Event.EventId);
			NodeCtrl->SendClusterEventTo(Address, Port, Event, bPrimaryOnly);
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterNetApiFacade& FDisplayClusterClusterManager::GetNetApi()
{
	return *NetApi.Get();
}

TSharedRef<IDisplayClusterClusterNodeController> FDisplayClusterClusterManager::GetNodeController()
{
	return NodeCtrl;
}

TSharedRef<FDisplayClusterCommDataCache> FDisplayClusterClusterManager::GetDataCache()
{
	return FailoverCtrl->GetDataCache();
}

TWeakPtr<FDisplayClusterService> FDisplayClusterClusterManager::GetNodeService(const FName& ServiceName)
{
	return NodeCtrl->GetService(ServiceName);
}

bool FDisplayClusterClusterManager::DropNode(const FString& NodeId, IPDisplayClusterClusterManager::ENodeDropReason DropReason)
{
	// Ignore invalid requests
	{
		FScopeLock Lock(&ActiveClusterNodeIdsCS);

		if (!ActiveClusterNodeIds.Contains(NodeId))
		{
			return false;
		}
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Requested node '%s' drop, reason=%u"), *NodeId, DropReason);

	// User requests are sent to the desired nodes as "exit" like commands
	if (DropReason == IPDisplayClusterClusterManager::ENodeDropReason::UserRequest)
	{
		FailoverCtrl->RequestNodeDrop(NodeId, static_cast<uint8>(DropReason));
	}
	// Other requests should go though failover pipeline
	else if (DropReason == IPDisplayClusterClusterManager::ENodeDropReason::Failed)
	{
		HandleNodeDrop(NodeId);
	}

	return true;
}

void FDisplayClusterClusterManager::CacheTimeData()
{
	// Cache data so it will be the same for all requests within current frame
	DeltaTimeCache = FApp::GetDeltaTime();
	GameTimeCache  = FApp::GetGameTime();
	FrameTimeCache = FApp::GetCurrentFrameTime();

	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Time data cache: Delta=%lf, Game=%lf, Frame=%lf"),
		DeltaTimeCache, GameTimeCache, FrameTimeCache.IsSet() ? FrameTimeCache.GetValue().AsSeconds() : 0.f);

	TimeDataCacheReadySignal->Trigger();
}

void FDisplayClusterClusterManager::SyncTimeData()
{
	double DeltaTime = 0.0f;
	double GameTime  = 0.0f;
	TOptional<FQualifiedFrameTime> FrameTime;

	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (time)..."));

	GetNetApi().GetClusterSyncAPI()->GetTimeData(DeltaTime, GameTime, FrameTime);

	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Delta=%lf, Game=%lf, Frame=%lf"),
		DeltaTime, GameTime, FrameTime.IsSet() ? FrameTime.GetValue().AsSeconds() : 0.f);

	// Apply new time data (including primary node)
	ImportTimeData(DeltaTime, GameTime, FrameTime);
}

void FDisplayClusterClusterManager::ExportTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime)
{
	// Wait until data is available
	TimeDataCacheReadySignal->Wait();

	// Return cached values
	OutDeltaTime = DeltaTimeCache;
	OutGameTime  = GameTimeCache;
	OutFrameTime = FrameTimeCache;
}

void FDisplayClusterClusterManager::ImportTimeData(const double& InDeltaTime, const double& InGameTime, const TOptional<FQualifiedFrameTime>& InFrameTime)
{
	// Compute new 'current' and 'last' time on the local platform timeline
	const double NewCurrentTime = FPlatformTime::Seconds();
	const double NewLastTime  = NewCurrentTime - InDeltaTime;

	// Store new data
	FApp::SetCurrentTime(NewLastTime);
	FApp::UpdateLastTime();
	FApp::SetCurrentTime(NewCurrentTime);
	FApp::SetDeltaTime(InDeltaTime);
	FApp::SetGameTime(InGameTime);
	FApp::SetIdleTime(0);
	FApp::SetIdleTimeOvershoot(0);

	if (InFrameTime.IsSet())
	{
		FApp::SetCurrentFrameTime(InFrameTime.GetValue());
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("DisplayCluster timecode: %s | %s"), *FTimecode::FromFrameNumber(InFrameTime->Time.GetFrame(), InFrameTime->Rate).ToString(), *InFrameTime->Rate.ToPrettyText().ToString());
	}
	else
	{
		FApp::InvalidateCurrentFrameTime();
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("DisplayCluster timecode: Invalid"));
	}
}

void FDisplayClusterClusterManager::SyncObjects(EDisplayClusterSyncGroup InSyncGroup)
{
	TMap<FString, FString> ObjectsData;

	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (objects)..."));
	GetNetApi().GetClusterSyncAPI()->GetObjectsData(InSyncGroup, ObjectsData);
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available %d records (objects)."), ObjectsData.Num());

	// Perform data load (objects state update)
	ImportObjectsData(InSyncGroup, ObjectsData);
}

void FDisplayClusterClusterManager::CacheObjects(EDisplayClusterSyncGroup SyncGroup)
{
	FScopeLock Lock(&ObjectsToSyncCS);

	// Cache data for requested sync group
	if (TMap<FString, FString>* GroupCache = ObjectsToSyncCache.Find(SyncGroup))
	{
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Exporting sync data for sync group: %u, items to sync: %d"), (uint8)SyncGroup, GroupCache->Num());

		for (IDisplayClusterClusterSyncObject* SyncObj : ObjectsToSync[SyncGroup])
		{
			if (SyncObj && SyncObj->IsActive() && SyncObj->IsDirty())
			{
				UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Adding object to sync: %s"), *SyncObj->GetSyncId());

				const FString SyncId   = SyncObj->GetSyncId();
				const FString SyncData = SyncObj->SerializeToString();

				UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Sync object: %s - %s"), *SyncId, *SyncData);

				// Cache the object
				GroupCache->Emplace(SyncId, SyncData);

				SyncObj->ClearDirty();
			}
		}
	}

	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Objects data cache contains %d records"), ObjectsToSyncCache[SyncGroup].Num());

	// Notify data is available
	ObjectsToSyncCacheReadySignals[SyncGroup]->Trigger();
}

void FDisplayClusterClusterManager::ExportObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData)
{
	// Wait until primary node provides data
	ObjectsToSyncCacheReadySignals[InSyncGroup]->Wait();
	// Return cached value
	OutObjectsData = ObjectsToSyncCache[InSyncGroup];
}

void FDisplayClusterClusterManager::ImportObjectsData(const EDisplayClusterSyncGroup InSyncGroup, const TMap<FString, FString>& InObjectsData)
{
	if (InObjectsData.Num() > 0)
	{
		for (auto It = InObjectsData.CreateConstIterator(); It; ++It)
		{
			UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("sync-data: %s=%s"), *It->Key, *It->Value);
		}

		FScopeLock Lock(&ObjectsToSyncCS);

		for (IDisplayClusterClusterSyncObject* SyncObj : ObjectsToSync[InSyncGroup])
		{
			if (SyncObj && SyncObj->IsActive())
			{
				const FString SyncId = SyncObj->GetSyncId();
				if (!InObjectsData.Contains(SyncId))
				{
					UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("%s has nothing to update"), *SyncId);
					continue;
				}

				if (SyncObj->DeserializeFromString(InObjectsData[SyncId]))
				{
					UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Synchronized object: %s"), *SyncId);
				}
				else
				{
					UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't apply sync data for sync object %s"), *SyncId);
				}
			}
		}
	}
}

void FDisplayClusterClusterManager::SyncEvents()
{
	TArray<TSharedPtr<FDisplayClusterClusterEventJson>>   JsonEvents;
	TArray<TSharedPtr<FDisplayClusterClusterEventBinary>> BinaryEvents;

	// Get events data from a provider
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (events)..."));
	GetNetApi().GetClusterSyncAPI()->GetEventsData(JsonEvents, BinaryEvents);
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available events: json=%d binary=%d"), JsonEvents.Num(), BinaryEvents.Num());

	// Import and process them
	ImportEventsData(JsonEvents, BinaryEvents);
}

void FDisplayClusterClusterManager::CacheEvents()
{
	// Export JSON events
	{
		FScopeLock Lock(&ClusterEventsJsonCS);

		// Export all system and non-system json events that have 'discard on repeat' flag
		for (const TPair<bool, TMap<FString, TSharedPtr<FDisplayClusterClusterEventJson>>>& It : ClusterEventsJson)
		{
			TArray<TSharedPtr<FDisplayClusterClusterEventJson>> JsonEventsToExport;
			It.Value.GenerateValueArray(JsonEventsToExport);
			JsonEventsCache.Append(MoveTemp(JsonEventsToExport));
		}

		// Clear original containers
		ClusterEventsJson.Reset();

		// Export all json events that don't have 'discard on repeat' flag
		JsonEventsCache.Append(MoveTemp(ClusterEventsJsonNonDiscarded));
	}

	// Export binary events
	{
		FScopeLock Lock(&ClusterEventsBinaryCS);

		// Export all binary events that have 'discard on repeat' flag
		for (const TPair<bool, TMap<int32, TSharedPtr<FDisplayClusterClusterEventBinary>>>& It : ClusterEventsBinary)
		{
			TArray<TSharedPtr<FDisplayClusterClusterEventBinary>> BinaryEventsToExport;
			It.Value.GenerateValueArray(BinaryEventsToExport);
			BinaryEventsCache.Append(MoveTemp(BinaryEventsToExport));
		}

		// Clear original containers
		ClusterEventsBinary.Reset();

		// Export all binary events that don't have 'discard on repeat' flag
		BinaryEventsCache.Append(MoveTemp(ClusterEventsBinaryNonDiscarded));
	}

	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Cluster events data cache contains: json=%d, binary=%d"), JsonEventsCache.Num(), BinaryEventsCache.Num());

	// Notify data is available
	CachedEventsDataSignal->Trigger();
}

void FDisplayClusterClusterManager::ExportEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary>>& OutBinaryEvents)
{
	// Wait until data is available
	CachedEventsDataSignal->Wait();

	// Return cached value
	OutJsonEvents   = JsonEventsCache;
	OutBinaryEvents = BinaryEventsCache;
}

void FDisplayClusterClusterManager::ImportEventsData(const TArray<TSharedPtr<FDisplayClusterClusterEventJson>>& InJsonEvents, const TArray<TSharedPtr<FDisplayClusterClusterEventBinary>>& InBinaryEvents)
{
	// Process and fire all JSON events
	if (InJsonEvents.Num() > 0)
	{
		FScopeLock LockListeners(&ClusterEventListenersCS);

		for (const TSharedPtr<FDisplayClusterClusterEventJson>& Event : InJsonEvents)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Processing json event %s|%s|%s|s%d|d%d..."), *Event->Category, *Event->Type, *Event->Name, Event->bIsSystemEvent ? 1 : 0, Event->bShouldDiscardOnRepeat ? 1 : 0);
			// Fire event
			OnClusterEventJson.Broadcast(*Event);
		}
	}

	// Process and fire all binary events
	if (InBinaryEvents.Num() > 0)
	{
		FScopeLock LockListeners(&ClusterEventListenersCS);

		for (const TSharedPtr<FDisplayClusterClusterEventBinary>& Event : InBinaryEvents)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Processing binary event %d..."), Event->EventId);
			// Fire event
			OnClusterEventBinary.Broadcast(*Event);
		}
	}
}

void FDisplayClusterClusterManager::ImportNativeInputData(TMap<FString, FString>& InNativeInputData)
{
	// Cache input data
	NativeInputCache = MoveTemp(InNativeInputData);

	UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Native input data cache: %d items"), NativeInputCache.Num());

	// Notify the data is available
	NativeInputCacheReadySignal->Trigger();
}

void FDisplayClusterClusterManager::ExportNativeInputData(TMap<FString, FString>& OutNativeInputData)
{
	// Wait for data cache to be ready
	NativeInputCacheReadySignal->Wait();
	// Export data from cache
	OutNativeInputData = NativeInputCache;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterManager::InitializeNetworking(const UDisplayClusterConfigurationData* ConfigData)
{
	// Instantiate cluster node controller
	NodeCtrl = CreateClusterNodeController();

	// Initialize the controller
	if (!NodeCtrl->Initialize())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't initialize the networking controller."));
		return false;
	}

	// Instantiate failover controller
	FailoverCtrl = CreateFailoverController(NodeCtrl);

	// Initialize the controller
	if (!FailoverCtrl->Initialize(ConfigData))
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't initialize the failover controller."));
		return false;
	}

	// Finally, setup API
	NetApi = MakeUnique<FDisplayClusterNetApiFacade>(FailoverCtrl);

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Networking internals have been successfully initialized."));

	return true;
}

void FDisplayClusterClusterManager::ReleaseNetworking()
{
	// Stop local clients/servers
	NodeCtrl->Shutdown();

	// Reset controllers to their 'Disabled' state
	NodeCtrl = MakeShared<FDisplayClusterClusterNodeCtrlDisabled>();
	FailoverCtrl = MakeShared<FDisplayClusterFailoverNodeCtrlDisabled>(NodeCtrl);
	NetApi = MakeUnique<FDisplayClusterNetApiFacade>(FailoverCtrl);
}

TSharedRef<IDisplayClusterClusterNodeController> FDisplayClusterClusterManager::CreateClusterNodeController() const
{
	// Instantiate appropriate controller depending on the operation mode
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating 'Main' node controller..."));
		return MakeShared<FDisplayClusterClusterNodeCtrlMain>(ClusterNodeId);
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating 'Editor' node controller..."));
		return MakeShared<FDisplayClusterClusterNodeCtrlEditor>();
	}

	// Otherwise 'Disabled'
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating 'Disabled' node controller..."));
	return MakeShared<FDisplayClusterClusterNodeCtrlDisabled>();
}

TSharedRef<IDisplayClusterFailoverNodeController> FDisplayClusterClusterManager::CreateFailoverController(TSharedRef<IDisplayClusterClusterNodeController>& ClusterCtrl) const
{
	// Instantiate appropriate controller depending on the operation mode
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating 'Main' failover controller..."));
		return MakeShared<FDisplayClusterFailoverNodeCtrlMain>(ClusterCtrl);
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating 'Editor' failover controller..."));
		return MakeShared<FDisplayClusterFailoverNodeCtrlEditor>(ClusterCtrl);
	}

	// Otherwise 'Disabled'
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating 'Disabled' failover controller..."));
	return MakeShared<FDisplayClusterFailoverNodeCtrlDisabled>(ClusterCtrl);
}

void FDisplayClusterClusterManager::InitializeClusterRole(const FString& NodeId, const UDisplayClusterConfigurationData* ConfigData)
{
	checkSlow(ConfigData);

	const bool bIsPrimary = NodeId.Equals(ConfigData->Cluster->PrimaryNode.Id, ESearchCase::IgnoreCase);
	if (bIsPrimary)
	{
		SetClusterRole(EDisplayClusterNodeRole::Primary);
		return;
	}
	
	// Currently we don't completely support the backup nodes concept. So this
	// part remains @todo. If it was supported, we would need to determine
	// either it's 'secondary' or 'backup'.
	SetClusterRole(EDisplayClusterNodeRole::Secondary);
}

void FDisplayClusterClusterManager::SetPrimaryNode(const FString& NewPrimaryNodeId)
{
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Requested new primary node: '%s'"), *NewPrimaryNodeId);

	{
		FScopeLock LockPrimary(&PrimaryNodeIdCS);

		// Nothing to do if already set
		if (PrimaryNodeId.Equals(NewPrimaryNodeId, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("'%s' is primary already"), *NewPrimaryNodeId);
			return;
		}

		// Check if new node is valid
		{
			FScopeLock LockActive(&ActiveClusterNodeIdsCS);
			if (!ActiveClusterNodeIds.Contains(NewPrimaryNodeId))
			{
				UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("'%s' was not found in the list of active nodes"), *NewPrimaryNodeId);
				return;
			}
		}

		// Update current primary
		PrimaryNodeId = NewPrimaryNodeId;

		UE_LOG(LogDisplayClusterCluster, Log, TEXT("New primary node (P-node): '%s'"), *NewPrimaryNodeId);

		// Update the role if we're the new primary.
		const bool bThisNodeIsNowPrimary = NewPrimaryNodeId.Equals(ClusterNodeId, ESearchCase::IgnoreCase);
		if (bThisNodeIsNowPrimary)
		{
			SetClusterRole(EDisplayClusterNodeRole::Primary);
		}
	}
}

void FDisplayClusterClusterManager::SetClusterRole(EDisplayClusterNodeRole NewRole)
{
	FScopeLock Lock(&CurrentNodeRoleCS);
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("New cluster role: '%u'"), static_cast<uint8>(NewRole));
	CurrentNodeRole = NewRole;
}

void FDisplayClusterClusterManager::HandleNodeDrop(const FString& NodeId)
{
	// Remove this node from the list of active nodes
	{
		FScopeLock Lock(&ActiveClusterNodeIdsCS);
		if (ActiveClusterNodeIds.Remove(NodeId) <= 0)
		{
			// This node has been processed already so nothing to do
			return;
		}
	}

	// Just exit if this node has failed
	if (NodeId.Equals(GetNodeId(), ESearchCase::IgnoreCase))
	{
		FDisplayClusterAppExit::ExitApplication(TEXT("This node has failed. Requesting exit."));
		return;
	}

	// Let the node controller drop it
	NodeCtrl->DropClusterNode(NodeId);

	// Let the failover controller process this
	if (!FailoverCtrl->HandleFailure(NodeId))
	{
		FDisplayClusterAppExit::ExitApplication(TEXT("Failover controller was unable to handle a failure. Requesting exit."));
	}

	// Finally, broadcast node failed event
	GDisplayCluster->GetCallbacks().OnDisplayClusterFailoverNodeDown().Broadcast(NodeId);
}

void FDisplayClusterClusterManager::OnClusterEventJsonHandler(const FDisplayClusterClusterEventJson& Event)
{
	FScopeLock Lock(&ClusterEventListenersCS);

	decltype(ClusterEventListeners) InvalidListeners;

	for (auto Listener : ClusterEventListeners)
	{
		if (!Listener.GetObject() || !IsValidChecked(Listener.GetObject()) || Listener.GetObject()->IsUnreachable()) // Note: .GetInterface() is always returning null when intefrace is added to class in the Blueprint.
		{
			UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Will remove invalid cluster event listener"));
			InvalidListeners.Add(Listener);
			continue;
		}
		Listener->Execute_OnClusterEventJson(Listener.GetObject(), Event);
	}

	for (auto& InvalidListener : InvalidListeners)
	{
		ClusterEventListeners.Remove(InvalidListener);
	}
}

void FDisplayClusterClusterManager::OnClusterEventBinaryHandler(const FDisplayClusterClusterEventBinary& Event)
{
	FScopeLock Lock(&ClusterEventListenersCS);

	decltype(ClusterEventListeners) InvalidListeners;

	for (auto Listener : ClusterEventListeners)
	{
		if (!Listener.GetObject() || !IsValidChecked(Listener.GetObject()) || Listener.GetObject()->IsUnreachable()) // Note: .GetInterface() is always returning null when intefrace is added to class in the Blueprint.
		{
			UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Will remove invalid cluster event listener"));
			InvalidListeners.Add(Listener);
			continue;
		}

		Listener->Execute_OnClusterEventBinary(Listener.GetObject(), Event);
	}

	for (auto& InvalidListener : InvalidListeners)
	{
		ClusterEventListeners.Remove(InvalidListener);
	}
}

void FDisplayClusterClusterManager::SetInternalSyncObjectsReleaseState(bool bRelease)
{
	if (bRelease)
	{
		// Set all events signaled
		TimeDataCacheReadySignal->Trigger();
		CachedEventsDataSignal->Trigger();
		NativeInputCacheReadySignal->Trigger();

		for (TPair<EDisplayClusterSyncGroup, FEvent*>& It : ObjectsToSyncCacheReadySignals)
		{
			It.Value->Trigger();
		}
	}
	else
	{
		// Reset all cache events
		TimeDataCacheReadySignal->Reset();
		CachedEventsDataSignal->Reset();
		NativeInputCacheReadySignal->Reset();

		// Reset events for all sync groups
		for (TPair<EDisplayClusterSyncGroup, FEvent*>& It : ObjectsToSyncCacheReadySignals)
		{
			It.Value->Reset();
		}
	}
}

void FDisplayClusterClusterManager::OnPrimaryNodeChangedHandler(const FString& NewPrimaryId)
{
	SetPrimaryNode(NewPrimaryId);
}

void FDisplayClusterClusterManager::OnClusterNodeFailed(const FString& FailedNodeId)
{
	// Remove it from the active nodes list
	{
		FScopeLock Lock(&ActiveClusterNodeIdsCS);
		ActiveClusterNodeIds.Remove(FailedNodeId);
	}
}

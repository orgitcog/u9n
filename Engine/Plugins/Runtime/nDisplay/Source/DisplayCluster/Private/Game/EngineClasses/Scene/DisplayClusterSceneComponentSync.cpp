// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterSceneComponentSync.h"

#include "DisplayClusterEnums.h"

#include "GameFramework/Actor.h"

#include "Cluster/IDisplayClusterClusterManager.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterTypesConverter.h"


UDisplayClusterSceneComponentSync::UDisplayClusterSceneComponentSync(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDisplayClusterSceneComponentSync::BeginPlay()
{
	Super::BeginPlay();

	const EDisplayClusterOperationMode OpMode = GDisplayCluster->GetOperationMode();
	if (OpMode == EDisplayClusterOperationMode::Cluster)
	{
		// Generate unique sync id
		SyncId = GenerateSyncId();

		// Register sync object
		if (IDisplayClusterClusterManager* ClusterMgr = GDisplayCluster->GetClusterMgr())
		{
			UE_LOG(LogDisplayClusterGame, Log, TEXT("Registering sync object %s..."), *SyncId);
			ClusterMgr->RegisterSyncObject(this, EDisplayClusterSyncGroup::Tick);
		}
		else
		{
			UE_LOG(LogDisplayClusterGame, Warning, TEXT("Couldn't register %s scene component sync."), *SyncId);
		}
	}
}

void UDisplayClusterSceneComponentSync::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const EDisplayClusterOperationMode OpMode = GDisplayCluster->GetOperationMode();
	if (OpMode == EDisplayClusterOperationMode::Cluster)
	{
		// Unregister sync object
		if (IDisplayClusterClusterManager* ClusterMgr = GDisplayCluster->GetClusterMgr())
		{
			UE_LOG(LogDisplayClusterGame, Log, TEXT("Unregistering sync object %s..."), *SyncId);
			ClusterMgr->UnregisterSyncObject(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterSyncObject
//////////////////////////////////////////////////////////////////////////////////////////////
bool UDisplayClusterSceneComponentSync::IsActive() const
{
	return IsValidChecked(this);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStringSerializable
//////////////////////////////////////////////////////////////////////////////////////////////
FString UDisplayClusterSceneComponentSync::GenerateSyncId()
{
	return FString::Printf(TEXT("S_%s"), *GetFullName());
}

FString UDisplayClusterSceneComponentSync::SerializeToString() const
{
	return DisplayClusterTypesConverter::template ToHexString(GetSyncTransform());
}

bool UDisplayClusterSceneComponentSync::DeserializeFromString(const FString& data)
{
	FTransform NewTransform = DisplayClusterTypesConverter::template FromHexString<FTransform>(data);
	UE_LOG(LogDisplayClusterGame, Verbose, TEXT("%s: applying transform data <%s>"), *SyncId, *NewTransform.ToHumanReadableString());
	SetSyncTransform(NewTransform);

	return true;
}

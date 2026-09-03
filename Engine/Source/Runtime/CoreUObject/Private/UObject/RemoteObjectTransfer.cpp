// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/RemoteObjectTransfer.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "UObject/RemoteObjectPrivate.h"
#include "UObject/RemoteObjectSerialization.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Class.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/RemoteObject.h"
#include "UObject/RemoteObjectPrivate.h"
#include "UObject/ObjectHandlePrivate.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectMigrationContext.h"
#include "HAL/CriticalSection.h"
#include "AutoRTFM.h"
#include "Templates/Atomic.h"


namespace UE::RemoteObject::Transfer
{
const FRemoteServerId DatabaseId(ERemoteServerIdConstants::Database);
static TAutoConsoleVariable<bool> CVarRemoveMissingMigratedComponents(TEXT("DSTM.RemoveMissingMigratedComponents"), true, TEXT("When we receive less SubObjects (Components) than we expect, remove the extras that we have locally (assume they were removed on purpose)"));

UObject* MigrateObjectFromObjectData(FRemoteObjectData& ObjectData, const FUObjectMigrationContext& MigrationContext);

// Prevent us from having to match this definition to the declartion in the header just to initialize the delegate
#define DEFINE_MIGRATION_DELEGATE(x) decltype(x) x
	DEFINE_MIGRATION_DELEGATE(RemoteObjectTransferDelegate);
	DEFINE_MIGRATION_DELEGATE(RemoteObjectDeniedTransferDelegate);
	DEFINE_MIGRATION_DELEGATE(RequestRemoteObjectDelegate);
	DEFINE_MIGRATION_DELEGATE(OnObjectDataReceivedDelegate);
	DEFINE_MIGRATION_DELEGATE(OnObjectsReceivedDelegate);
	DEFINE_MIGRATION_DELEGATE(OnObjectDataSentDelegate);
	DEFINE_MIGRATION_DELEGATE(OnObjectsSentDelegate);
	DEFINE_MIGRATION_DELEGATE(OnObjectTouchedDelegate);
	DEFINE_MIGRATION_DELEGATE(StoreRemoteObjectDataDelegate);
	DEFINE_MIGRATION_DELEGATE(RestoreRemoteObjectDataDelegate);
#undef DEFINE_MIGRATION_DELEGATE

/**
 * Remote object transfer queue. 
 * Queued requests (both send and receive) are processed on the game thread since some systems (like RPCs) that are used for transfering object data are GT-only
 */

class FRemoteObjectRequest
{
public:
	FRemoteTransactionId RequestId;
	FRemoteWorkPriority Priority;

	// this array fills up with object ids that we have ever touched
	// while running the transaction
	TArray<FRemoteObjectId> RequiredObjects;
	TArray<FRemoteObjectId> RequiredObjectsCanonicalRoot;

	// this array gets cleared before running the 
	// transaction and only tracks the objects touched 
	// during the most recent run
	TArray<FRemoteObjectId> UsedObjects;

	FRemoteTransactionId GetRequestId() const { return RequestId; }
	FRemoteWorkPriority GetPriority() const { return Priority; }
};

class FObjectMigrationRequest
{
public:
	FRemoteObjectId ObjectId;
	FRemoteServerId DestinationServerId;
	FRemoteWorkPriority RequestPriority;

	FString ToString() const
	{
		return FString::Format(TEXT("[obj %s dest %s %s]"),
			{*ObjectId.ToString(),
			*DestinationServerId.ToString(),
			*RequestPriority.ToString()});
	}
};

class FObjectMigrationRequests
{
public:
	FRemoteObjectId ObjectId;

	// IndividualRequests is sorted by priority (highest first)
	TArray<FObjectMigrationRequest> IndividualRequests;
};

class FPendingObjectRequest
{
public:
	FRemoteObjectId ObjectId;
	FRemoteWorkPriority RequestPriority;
};

class FRemoteObjectTransferQueue : public FRemoteSubsystem<FRemoteObjectRequest>
{
	// these are objects that we have outstanding requests for (across all requests)
	TArray<FPendingObjectRequest> PendingObjectRequests;

	TSet<UObject*> MultiServerCommitObjectsToReturn;
	TSet<UObject*> MultiServerCommitSentObjects;
	TSet<UObject*> MultiServerCommitReferencedObjects;

	// this is a list of objects that we have locally that 
	// other servers are asking for
	TArray<FObjectMigrationRequests> PendingObjectMigrationRequests;

public:
	bool bInMultiServerCommit = false;

	const TCHAR* NameForDebug() final { return TEXT("RemoteObjectTransferQueue"); }

	void BeginRequest() final
	{
	}

	void TickSubsystem() final
	{
		using namespace UE::RemoteObject::Private;
		using namespace UE::RemoteObject::Handle;
		using namespace UE::RemoteObject::Serialization;

		// go through the list of pending object migration requests and see if we can satisfy any of them
		for (int32 MigrationRequestsIndex = 0; MigrationRequestsIndex < PendingObjectMigrationRequests.Num(); )
		{
			FObjectMigrationRequests& MigrationRequests = PendingObjectMigrationRequests[MigrationRequestsIndex];

			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations processing (%d) requests for obj %s"),
					MigrationRequests.IndividualRequests.Num(),
					*MigrationRequests.ObjectId.ToString());

			UObject* Object = StaticFindObjectFastInternal(MigrationRequests.ObjectId);

			if (Object && !Handle::IsRemote(Object))
			{
				UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations obj %s (is local)"),
					*MigrationRequests.ObjectId.ToString());

				// look at the list of requests for this object and pick the one with the highest priority for arbitration
				// (the list is sorted, so the first element is always the highest priority)
				FObjectMigrationRequest* HighestPriorityRequest = &MigrationRequests.IndividualRequests[0];

				// verify the list is sorted
				for (int32 MigrationRequestIndex = 1; MigrationRequestIndex < MigrationRequests.IndividualRequests.Num(); MigrationRequestIndex++)
				{
					FObjectMigrationRequest* Request = &MigrationRequests.IndividualRequests[MigrationRequestIndex];
					check(!IsHigherPriority(Request->RequestPriority, HighestPriorityRequest->RequestPriority));
				}

				UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations obj %s highest priority request is %s"),
					*MigrationRequests.ObjectId.ToString(),
					*HighestPriorityRequest->RequestPriority.ToString());

				// find the canonical root object id to use for arbitration
				UObject* const RootObject = FindCanonicalRootObjectForSerialization(Object);
				const FRemoteObjectId CanonicalRootObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(RootObject);

				// now look over all active transactions and see if this request is higher priority than all of them
				bool bObjectLocked = false;

				for (int32 RequestIndex = 0; RequestIndex < GetRequestCount(); RequestIndex++)
				{
					FRemoteObjectRequest* ExistingRequest = GetRequestByIndex(RequestIndex);

					if (IsHigherPriority(ExistingRequest->GetPriority(), HighestPriorityRequest->RequestPriority))
					{
						// ExistingRequest is higher priority, now see if it actually needs this object
						for (int32 RequiredObjectIndex = 0; RequiredObjectIndex < ExistingRequest->RequiredObjectsCanonicalRoot.Num(); RequiredObjectIndex++)
						{
							FRemoteObjectId RequiredObjectId = ExistingRequest->RequiredObjectsCanonicalRoot[RequiredObjectIndex];

							if (RequiredObjectId == CanonicalRootObjectId)
							{
								// we found an existing request that is higher priority than the highest priority request,
								// so we have to give it up
								bObjectLocked = true;
								break;
							}
						}
					}

					if (bObjectLocked)
					{
						break;
					}
				}

				if (!bObjectLocked)
				{
					// send the object
					UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations sending obj %s to %s %s"),
						*HighestPriorityRequest->ObjectId.ToString(),
						*HighestPriorityRequest->DestinationServerId.ToString(),
						*HighestPriorityRequest->RequestPriority.ToString());

					FRemoteServerId OwnerServerId = Handle::GetOwnerServerId(RootObject);
					FRemoteServerId PhysicsServerId = Handle::GetPhysicsServerId(RootObject);
					FUObjectMigrationContext MigrationContext {
						.ObjectId = HighestPriorityRequest->ObjectId,
						.RemoteServerId = HighestPriorityRequest->DestinationServerId,
						.OwnerServerId = OwnerServerId,
						.PhysicsServerId = PhysicsServerId,
						.MigrationSide = EObjectMigrationSide::Send
					};
					FScopedObjectMigrationContext ScopedMigrationContext(MigrationContext);

					TSet<UObject*> SentObjects;
					TSet<UObject*> ReferencedObjects;
					FRemoteObjectData ObjectData = SerializeObjectData(Object, SentObjects, ReferencedObjects, &MigrationContext);

					checkf(SentObjects.Num() > 0, TEXT("unable to migrate object %s to %s"), *HighestPriorityRequest->ObjectId.ToString(), *HighestPriorityRequest->DestinationServerId.ToString());

					for (UObject* RO : ReferencedObjects)
					{
						RO->SetInternalFlags(EInternalObjectFlags::RemoteReference);
					}

					for (UObject* SO : SentObjects)
					{
						FRemoteObjectId SubObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(SO);
						UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations sending obj %s sent subobject : %s [%s]"),
							*HighestPriorityRequest->ObjectId.ToString(),
							*SubObjectId.ToString(),
							*GetNameSafe(SO));
					}

					// Notify that object data has been migrated (actually will be but SendObjectData does a MoveTemp on the data)
					OnObjectDataSentDelegate.Broadcast(ObjectData, MigrationContext);

					FMigrateSendParams SendRemoteObjectParams = { .MigrationContext = MigrationContext, .ObjectData = ObjectData };
					SendRemoteObject(SendRemoteObjectParams);

					{
						// PostMigrate callbacks may be manipulating migrated objects and may try to migrate them back so prevent that from happening since
						// they all will be destroyed anyway
						FUnsafeToMigrateScope UnsafeToMigrateScope;

						// Notify that objects have been migrated to a remote server
						OnObjectsSentDelegate.Broadcast(SentObjects, MigrationContext);

						for (UObject* SentObject : SentObjects)
						{
#if UE_WITH_REMOTE_OBJECT_HANDLE
							SentObject->PostMigrate(MigrationContext);
#endif
							MarkAsRemote(SentObject, HighestPriorityRequest->DestinationServerId);
							UE::RemoteObject::Handle::ChangeOwnerServerId(SentObject, OwnerServerId);
						}
					}

					// for every other request, forward the request on to the new server
					for (int32 MigrationRequestIndex = 0; MigrationRequestIndex < MigrationRequests.IndividualRequests.Num(); MigrationRequestIndex++)
					{
						FObjectMigrationRequest* MigrationRequest = &MigrationRequests.IndividualRequests[MigrationRequestIndex];

						if (MigrationRequest != HighestPriorityRequest)
						{
							if (MigrationRequest->DestinationServerId != HighestPriorityRequest->DestinationServerId)
							{
								UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations obj %s forwarding request for server %s"),
									*MigrationRequests.ObjectId.ToString(),
									*MigrationRequest->DestinationServerId.ToString());

								SendRemoteObjectRequest(MigrationRequest->RequestPriority, MigrationRequest->ObjectId, HighestPriorityRequest->DestinationServerId, MigrationRequest->DestinationServerId);
							}
							else
							{
								UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations obj %s denying request to server %s"),
									*MigrationRequests.ObjectId.ToString(),
									*MigrationRequest->DestinationServerId.ToString());

								RemoteObjectDeniedTransferDelegate.Execute(MigrationRequest->ObjectId, MigrationRequest->DestinationServerId);
							}
						}
					}
					
					// all done, delete all requests for this object
					PendingObjectMigrationRequests.RemoveAt(MigrationRequestsIndex, EAllowShrinking::No);
					// do not increment MigrationRequestsIndex
				}
				else
				{
					UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations obj %s is locked, continuing"),
						*MigrationRequests.ObjectId.ToString());

					// object is locked, do we need to report it?
					MigrationRequestsIndex++;
				}
			}
			else
			{
				UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TickObjectMigrations obj %s is remote, forwarding requests"),
					*MigrationRequests.ObjectId.ToString());

				// we found a request for an object we don't have, we need to forward all of these
				// requests to whatever server we think has the object
				FRemoteObjectStub* RemoteObjectStub = FindRemoteObjectStub(MigrationRequests.ObjectId);

				FRemoteServerId CurrentResidentServerId = RemoteObjectStub ? RemoteObjectStub->ResidentServerId : MigrationRequests.ObjectId.GetServerId();

				for (int32 MigrationRequestIndex = 0; MigrationRequestIndex < MigrationRequests.IndividualRequests.Num(); MigrationRequestIndex++)
				{
					FObjectMigrationRequest* MigrationRequest = &MigrationRequests.IndividualRequests[MigrationRequestIndex];
				
					if (CurrentResidentServerId != MigrationRequest->DestinationServerId)
					{
						SendRemoteObjectRequest(MigrationRequest->RequestPriority, MigrationRequest->ObjectId, CurrentResidentServerId, MigrationRequest->DestinationServerId);
					}
					else
					{
						RemoteObjectDeniedTransferDelegate.Execute(MigrationRequest->ObjectId, MigrationRequest->DestinationServerId);
					}
				}

				// all done, delete all requests for this object
				PendingObjectMigrationRequests.RemoveAt(MigrationRequestsIndex, EAllowShrinking::No);
				// do not increment MigrationRequestsIndex
			}
		}
	}

	void TickRequest() final
	{
		using namespace UE::RemoteObject::Private;
		using namespace UE::RemoteObject::Handle;
		using namespace UE::RemoteObject::Serialization;

		check(ActiveRequest->RequiredObjects.Num() == ActiveRequest->RequiredObjectsCanonicalRoot.Num());

		// reset the list of used objects each time before we run
		ActiveRequest->UsedObjects.SetNum(0, EAllowShrinking::No);

		UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: TickRequest(%s) %d required objs (%d pending requests):"), *ActiveRequest->GetRequestId().ToString(), ActiveRequest->RequiredObjects.Num(), PendingObjectRequests.Num());
		for (int32 RequiredObjectIndex = 0; RequiredObjectIndex < ActiveRequest->RequiredObjects.Num(); RequiredObjectIndex++)
		{
			FRemoteObjectId RequiredObjectId = ActiveRequest->RequiredObjects[RequiredObjectIndex];
			FRemoteObjectId CanonicalObjectId = ActiveRequest->RequiredObjectsCanonicalRoot[RequiredObjectIndex];
			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: TickRequest(%s) %s [root %s] %s"), *ActiveRequest->GetRequestId().ToString(), *RequiredObjectId.ToString(), *CanonicalObjectId.ToString(), Handle::IsRemote(RequiredObjectId) ? TEXT("<remote>") : TEXT("<local>"));
		}

		// send the requests
		for (int32 RequiredObjectIndex = 0; RequiredObjectIndex < ActiveRequest->RequiredObjects.Num(); RequiredObjectIndex++)
		{
			FRemoteObjectId RequiredObjectId = ActiveRequest->RequiredObjects[RequiredObjectIndex];
			FRemoteObjectId RequiredCanonicalRootObjectId = ActiveRequest->RequiredObjectsCanonicalRoot[RequiredObjectIndex];
			
			FRemoteObjectId ObjectIdToRequest = RequiredCanonicalRootObjectId.IsValid() ? RequiredCanonicalRootObjectId : RequiredObjectId;

			if (Handle::IsRemote(ObjectIdToRequest))
			{
				// find the highest priority request that needs this object
				FRemoteObjectRequest* HighestPriorityRequest = ActiveRequest;

				for (int32 RequestIndex = 0; RequestIndex < GetRequestCount(); RequestIndex++)
				{
					FRemoteObjectRequest* ExistingRequest = GetRequestByIndex(RequestIndex);

					bool bFoundObject =
						(ExistingRequest->RequiredObjectsCanonicalRoot.Find(ObjectIdToRequest) != INDEX_NONE) ||
						(ExistingRequest->RequiredObjects.Find(ObjectIdToRequest) != INDEX_NONE);

					if (bFoundObject)
					{
						if (IsHigherPriority(ExistingRequest->Priority, HighestPriorityRequest->Priority))
						{
							HighestPriorityRequest = ExistingRequest;
						}
					}
				}

				// does this object id already exist in PendingObjectRequests?
				// if it does, but the current HighestPriorityRequest is of
				// a different priority, then we need to re-send the request
				// to update the priority at which we require this object

				FPendingObjectRequest* ExistingObjectRequest = nullptr;

				for (FPendingObjectRequest& PendingObjectRequest : PendingObjectRequests)
				{
					if (PendingObjectRequest.ObjectId == ObjectIdToRequest)
					{
						ExistingObjectRequest = &PendingObjectRequest;
						break;
					}
				}

				const bool bUpdatingPriority = (ExistingObjectRequest != nullptr);

				if (ExistingObjectRequest == nullptr)
				{
					ExistingObjectRequest = &PendingObjectRequests.Emplace_GetRef();
					ExistingObjectRequest->ObjectId = ObjectIdToRequest;
				}

				if (ExistingObjectRequest->RequestPriority != HighestPriorityRequest->GetPriority())
				{
					// send this request
					ExistingObjectRequest->RequestPriority = HighestPriorityRequest->GetPriority();

					FRemoteObjectStub* RemoteObjectStub = FindRemoteObjectStub(ObjectIdToRequest);
					FRemoteServerId CurrentResidentServerId = RemoteObjectStub ? RemoteObjectStub->ResidentServerId : ObjectIdToRequest.GetServerId();

					SendRemoteObjectRequest(
						HighestPriorityRequest->GetPriority(),
						ObjectIdToRequest,
						CurrentResidentServerId,
						GetGlobalServerId());
			
					if (bUpdatingPriority)
					{
						UE_LOG(LogRemoteObject, Verbose, TEXT("FRemoteObjectTransferQueue: TickRequest(%s) sent updated priority for %s %s to server %s (%d pending requests)"), *ActiveRequest->GetRequestId().ToString(), *RequiredObjectId.ToString(), *HighestPriorityRequest->Priority.ToString(), *CurrentResidentServerId.ToString(), PendingObjectRequests.Num());
					}
					else
					{
						UE_LOG(LogRemoteObject, Verbose, TEXT("FRemoteObjectTransferQueue: TickRequest(%s) sent request for %s %s to server %s (%d pending requests)"), *ActiveRequest->GetRequestId().ToString(), *RequiredObjectId.ToString(), *HighestPriorityRequest->Priority.ToString(), *CurrentResidentServerId.ToString(), PendingObjectRequests.Num());
					}
				}
			}
			else if (!RequiredCanonicalRootObjectId.IsValid())
			{
				if (UObject* Object = StaticFindObjectFastInternal(RequiredObjectId))
				{
					UObject* const RootObject = FindCanonicalRootObjectForSerialization(Object);
					const FRemoteObjectId CanonicalRootObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(RootObject);
					ActiveRequest->RequiredObjectsCanonicalRoot[RequiredObjectIndex] = CanonicalRootObjectId;
				}
			}
		}
	}

	bool AreDependenciesSatisfied() const final
	{
		for (int32 RequiredObjectIndex = 0; RequiredObjectIndex < ActiveRequest->RequiredObjects.Num(); RequiredObjectIndex++)
		{
			FRemoteObjectId RequiredObjectId = ActiveRequest->RequiredObjects[RequiredObjectIndex];

			if (Handle::IsRemote(RequiredObjectId))
			{
				return false;
			}
		}

		UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: TickRequest(%s) all %d required objects are local"), *ActiveRequest->GetRequestId().ToString(), ActiveRequest->RequiredObjects.Num());

		return true;
	}

	void BeginMultiServerCommit(TArray<FRemoteServerId>& OutMultiServerCommitRemoteServers) final
	{
		using namespace UE::RemoteObject::Serialization;

		check(ActiveRequest);
		check(!bInMultiServerCommit);
		bInMultiServerCommit = true;

		check(MultiServerCommitObjectsToReturn.Num() == 0);
		check(MultiServerCommitSentObjects.Num() == 0);
		check(MultiServerCommitReferencedObjects.Num() == 0);

		UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: BeginMultiServerCommit has %d used objects"), ActiveRequest->UsedObjects.Num());

		// are any of our used objects borrowed?
		for (FRemoteObjectId UsedObjectId : ActiveRequest->UsedObjects)
		{
			if (UsedObjectId.IsAsset())
			{
				continue;
			}

			// if we used it this run and we're about to commit, we expect that the object is local
			check(!Handle::IsRemote(UsedObjectId));

			UObject* UsedObject = StaticFindObjectFastInternal(UsedObjectId);
			check(UsedObject);
			FRemoteServerId OwnerServerId = Handle::GetOwnerServerId(UsedObject);

			UObject* RootObject = FindCanonicalRootObjectForSerialization(UsedObject);
			check(RootObject);
			FRemoteObjectId RootObjectId = FRemoteObjectId(RootObject);
			FRemoteServerId RootOwnerServerId = Handle::GetOwnerServerId(RootObject);

			if (RootObjectId.IsAsset())
			{
				// if the used object is not an asset, but the root of it is, then we
				// expect that the used object owner is the local server
				check(OwnerServerId == GetGlobalServerId());

				continue;
			}

			if (RootOwnerServerId != GetGlobalServerId() && !RootOwnerServerId.IsAsset())
			{
				UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: borrowed obj: %s (%s) owner server: %s (root: %s (s) root owner: %s)"),
					*UsedObjectId.ToString(),
					*UsedObject->GetName(),
					*OwnerServerId.ToString(),
					*RootObjectId.ToString(),
					*RootObject->GetName(),
					*RootOwnerServerId.ToString());

				MultiServerCommitObjectsToReturn.Add(RootObject);

				if (OutMultiServerCommitRemoteServers.Find(RootOwnerServerId) == INDEX_NONE)
				{
					OutMultiServerCommitRemoteServers.Add(RootOwnerServerId);
				}
			}
		}
	}

	void ExecuteMultiServerCommit() final
	{
		using namespace UE::RemoteObject::Serialization;
		
		check(MultiServerCommitSentObjects.Num() == 0);
		check(MultiServerCommitReferencedObjects.Num() == 0);
		check(bInMultiServerCommit);

		for (UObject* Object : MultiServerCommitObjectsToReturn)
		{
			FRemoteObjectId ObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object);
			FRemoteServerId OwnerServerId = Handle::GetOwnerServerId(Object);

			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: ExecuteMultiServerCommit sending root object %s back to %s"), *ObjectId.ToString(), *OwnerServerId.ToString());

			// NOTE: we need to run SerializeObjectData in the CLOSED because it turns out some of the
			// serialization code ends up mutating the object, so we need to be able to undo those
			// mutations if we need to abort past this point
			FUObjectMigrationContext MigrationContext {
				.ObjectId = ObjectId,
				.RemoteServerId = OwnerServerId,
				.OwnerServerId = OwnerServerId,
				.PhysicsServerId = OwnerServerId,
				.MigrationSide = EObjectMigrationSide::Send,
				.MultiServerCommitRequestId = ActiveRequest->RequestId,
			};
			FScopedObjectMigrationContext ScopedMigrationContext(MigrationContext);

			TSet<UObject*> SentObjects;
			FRemoteObjectData ObjectData = SerializeObjectData(Object, SentObjects, MultiServerCommitReferencedObjects, &MigrationContext);
			FMigrateSendParams SendRemoteObjectParams = { .MigrationContext = MigrationContext, .ObjectData = ObjectData};

			UE_AUTORTFM_OPEN
			{
				SendRemoteObject(SendRemoteObjectParams);

				for (UObject* SO : SentObjects)
				{
					FRemoteObjectId SOId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(SO);
					UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: ExecuteMultiServerCommit sent object %s (subobject %s %s)"), *ObjectId.ToString(), *SOId.ToString(), *SO->GetName());
				}

				MultiServerCommitSentObjects.Append(SentObjects);
			};
		}
	}

	void AbortMultiServerCommit() final
	{
		UE_AUTORTFM_OPEN
		{
			MultiServerCommitObjectsToReturn.Reset();
			MultiServerCommitSentObjects.Reset();
			MultiServerCommitReferencedObjects.Reset();
		};

		bInMultiServerCommit = false;
	}

	void CommitMultiServerCommit() final
	{
		using namespace UE::RemoteObject::Serialization;
		using namespace UE::RemoteObject::Private;

		UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: CommitMultiServerCommit"));

		for (UObject* ReferencedObject : MultiServerCommitReferencedObjects)
		{
			FRemoteObjectId ReferencedObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(ReferencedObject);
			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: CommitMultiServerCommit setting RemoteReference %s"), *ReferencedObjectId.ToString());

			ReferencedObject->SetInternalFlags(EInternalObjectFlags::RemoteReference);
		}

		UE_AUTORTFM_OPEN
		{
			MultiServerCommitReferencedObjects.Reset();
		};

		FUnsafeToMigrateScope UnsafeToMigrateScope;
		for (UObject* SentObject : MultiServerCommitSentObjects)
		{
			FRemoteServerId OwnerServerId = Handle::GetOwnerServerId(SentObject);
			FRemoteServerId PhysicsServerId = Handle::GetPhysicsServerId(SentObject);
			FRemoteObjectId SentObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(SentObject);

			FUObjectMigrationContext MigrationContext {
				.ObjectId = SentObjectId,
				.RemoteServerId = OwnerServerId,
				.OwnerServerId = OwnerServerId,
				.PhysicsServerId = PhysicsServerId,
				.MigrationSide = EObjectMigrationSide::Send
			};
			FScopedObjectMigrationContext ScopedMigrationContext(MigrationContext);

			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("FRemoteObjectTransferQueue: CommitMultiServerCommit postmigrate %s"), *SentObjectId.ToString());

#if UE_WITH_REMOTE_OBJECT_HANDLE
			SentObject->PostMigrate(MigrationContext);
#endif
			MarkAsRemote(SentObject, OwnerServerId);
		}

		UE_AUTORTFM_OPEN
		{
			MultiServerCommitObjectsToReturn.Reset();
			MultiServerCommitSentObjects.Reset();
		};

		bInMultiServerCommit = false;
	}

	void EndRequest(bool bTransactionCommitted) final
	{
	}

	void RequestObjectMigration(FRemoteWorkPriority RequestPriority, FRemoteObjectId ObjectId, FRemoteServerId DestinationServerId)
	{
		check(DestinationServerId != GetGlobalServerId());

		// enqueue this request
		FObjectMigrationRequests* ObjectRequests = nullptr;

		for (int32 RequestIndex = 0; RequestIndex < PendingObjectMigrationRequests.Num(); RequestIndex++)
		{
			FObjectMigrationRequests* ExistingObjectRequests = &PendingObjectMigrationRequests[RequestIndex];

			if (ExistingObjectRequests->ObjectId == ObjectId)
			{
				ObjectRequests = ExistingObjectRequests;
				break;
			}
		}
		
		if (ObjectRequests == nullptr)
		{
			ObjectRequests = &PendingObjectMigrationRequests.Emplace_GetRef();
			ObjectRequests->ObjectId = ObjectId;
		}

		// first search the list to see if we have a request with the same DestinationServerId - if so, overwrite the priority
		// otherwise, insert the request in priority sorted order
		FObjectMigrationRequest* Request = nullptr;

		for (FObjectMigrationRequest& ExistingRequest : ObjectRequests->IndividualRequests)
		{
			if (ExistingRequest.DestinationServerId == DestinationServerId)
			{
				Request = &ExistingRequest;
				break;
			}
		}

		if (!Request)
		{
			Request = &ObjectRequests->IndividualRequests.Emplace_GetRef();
			Request->ObjectId = ObjectId;
			Request->DestinationServerId = DestinationServerId;
		}

		Request->RequestPriority = RequestPriority;

		// re-sort the list by priority
		ObjectRequests->IndividualRequests.Sort([](const FObjectMigrationRequest& Lhs, const FObjectMigrationRequest& Rhs) -> bool
			{
				return IsHigherPriority(Lhs.RequestPriority, Rhs.RequestPriority);
			});
	}

	void SendRemoteObjectRequest(FRemoteWorkPriority RequestPriority, FRemoteObjectId ObjectId, FRemoteServerId LastKnownResidentServerId, FRemoteServerId DestinationServerId)
	{
		check(!AutoRTFM::IsClosed());

		// Request an object from remote server
		if (LastKnownResidentServerId != DatabaseId)
		{
			// We are forwarding a request for pull-migrate from the resident server id
			// This isn't technically a migration, it is actually a request for migration...
			RequestRemoteObjectDelegate.Execute(RequestPriority, ObjectId, LastKnownResidentServerId, DestinationServerId);
		}
		else
		{
			UE_LOG(LogRemoteObject, Log, TEXT("%hs: Restoring ObjectId %s from Database and Forwarding to %s"), __func__, *ObjectId.ToString(), *DestinationServerId.ToString());
			// We are intercepting a request to pull-migrate since we believe the object resides in our database, migrate it "from" the database to us
			// Inside the database call, it will fix-up these migration context parameters to make us the owner
			FUObjectMigrationContext MigrationContext {
				.ObjectId = ObjectId,
				.RemoteServerId = DatabaseId,
				.OwnerServerId = DatabaseId,
				.PhysicsServerId = DatabaseId,
				.MigrationSide = EObjectMigrationSide::Receive
			};
			FScopedObjectMigrationContext ScopedMigrationContext(MigrationContext);
			RestoreRemoteObjectDataDelegate.Execute(MigrationContext);

#if UE_WITH_REMOTE_OBJECT_HANDLE
			// We assume we have it restored, now forward it
			TWeakObjectPtr WeakObjPtr { ObjectId };
			ensureMsgf(!WeakObjPtr.IsExplicitlyNull() && WeakObjPtr.IsValid(true) && !WeakObjPtr.IsRemote(), TEXT("Expected RemoteObjectId %s to be restored from Database and thus locally owned"), *ObjectId.ToString());
#endif

			// If we didn't expect it locally, send it on to the final destination
			if (DestinationServerId != GetGlobalServerId())
			{
				MigrateObjectToRemoteServer(ObjectId, DestinationServerId);
			}
		}
	}

	/**
	*/
	void SendRemoteObject(const FMigrateSendParams& Params)
	{
		check(!AutoRTFM::IsClosed());
		RemoteObjectTransferDelegate.Execute(Params);
	}

	/**
	* Fullfils receive request
	* @param ObjectId Id of the object that was requested
	* @param Data Object data. Data ownership is transferred to the receive request (if it exists)
	*/
	void FulfillReceiveRequest(FRemoteObjectData& ObjectData, const FUObjectMigrationContext& MigrationContext)
	{
		using namespace UE::RemoteObject::Serialization;

		check(!AutoRTFM::IsClosed());

		int32 FoundPendingRequestIndex = INDEX_NONE;

		for (int32 PendingRequestIndex = 0; PendingRequestIndex < PendingObjectRequests.Num(); PendingRequestIndex++)
		{
			const FPendingObjectRequest& PendingObjectRequest = PendingObjectRequests[PendingRequestIndex];
			if (PendingObjectRequest.ObjectId == MigrationContext.ObjectId)
			{
				FoundPendingRequestIndex = PendingRequestIndex;
				break;
			}
		}

		const bool bFoundRequest = (FoundPendingRequestIndex != INDEX_NONE);

		UE_LOG(LogRemoteObject, Verbose, TEXT("FRemoteObjectTransferQueue: FulfillReceiveRequest %s (was requested: %d, owner: %s)"), *MigrationContext.ObjectId.ToString(), bFoundRequest, *MigrationContext.OwnerServerId.ToString());

		UObject* Result = MigrateObjectFromObjectData(ObjectData, MigrationContext);
		checkf(UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Result) == MigrationContext.ObjectId, TEXT("PeerId:%s requested object %s migration from PeerId:%s but received %s"),
			   *GetGlobalServerId().ToString(), *MigrationContext.ObjectId.ToString(), *MigrationContext.RemoteServerId.ToString(), *UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Result).ToString());
		checkf(MigrationContext.OwnerServerId == Handle::GetOwnerServerId(Result), TEXT("Expected owner id %s doesn't match post-deserialize owner: %s"), *MigrationContext.OwnerServerId.ToString(), *Handle::GetOwnerServerId(Result).ToString());

		if (bFoundRequest)
		{
			PendingObjectRequests.RemoveAt(FoundPendingRequestIndex);
		}
	}

	void DenyReceiveRequest(FRemoteObjectId ObjectId)
	{
		check(!AutoRTFM::IsClosed());

		int32 FoundPendingRequestIndex = INDEX_NONE;

		for (int32 PendingRequestIndex = 0; PendingRequestIndex < PendingObjectRequests.Num(); PendingRequestIndex++)
		{
			const FPendingObjectRequest& PendingObjectRequest = PendingObjectRequests[PendingRequestIndex];
			if (PendingObjectRequest.ObjectId == ObjectId)
			{
				FoundPendingRequestIndex = PendingRequestIndex;
				break;
			}
		}

		const bool bFoundRequest = (FoundPendingRequestIndex != INDEX_NONE);

		UE_LOG(LogRemoteObject, Verbose, TEXT("FRemoteObjectTransferQueue: DenyReceiveRequest %s, (was requested: %d)"), *ObjectId.ToString(), bFoundRequest);

		if (bFoundRequest)
		{
			PendingObjectRequests.RemoveAt(FoundPendingRequestIndex);
		}
	}
};

FRemoteObjectTransferQueue GTransferQueue;

UObject* MigrateObjectFromObjectData(FRemoteObjectData& ObjectData, const FUObjectMigrationContext& MigrationContext)
{
	using namespace UE::RemoteObject::Serialization;
	check(!AutoRTFM::IsClosed());

	// Notify that object data has been migrated from a remote server
	OnObjectDataReceivedDelegate.Broadcast(ObjectData, MigrationContext);

	TArray<FRemoteObjectId> ReceivedObjectRemoteIds;
	TArray<UObject*> ReceivedObjects;
	int32 RequestedObjectIndex = DeserializeObjectData(ObjectData, &MigrationContext, ReceivedObjectRemoteIds, ReceivedObjects);

	// Debug section
	checkf(ReceivedObjects.Num() > 0, TEXT("PeerId:%s unable to deserialize object data (%d bytes)"), *GetGlobalServerId().ToString(), ObjectData.GetNumBytes());
	if (ReceivedObjects.Num() > 0)
	{
		if (UE_LOG_ACTIVE(LogRemoteObject, VeryVerbose))
		{
			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("PeerId:%s deserialized object data (%d bytes)"), *GetGlobalServerId().ToString(), ObjectData.GetNumBytes());
			for(int32 SubObjectIndex = 0; SubObjectIndex < ReceivedObjects.Num(); SubObjectIndex++)
			{
				FRemoteObjectId SubObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(ReceivedObjects[SubObjectIndex]);
				check(ReceivedObjectRemoteIds[SubObjectIndex] == SubObjectId);

				UE_LOG(LogRemoteObject, VeryVerbose, TEXT("         subobject[%2d] : %s %s"),
					SubObjectIndex,
					*SubObjectId.ToString(),
					(SubObjectIndex == RequestedObjectIndex) ? TEXT("<requested>") : TEXT(""));
			}

			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("         <done listing subobjects>"));
		}

		// Let's make sure we received data for all of the objects
		const UObject* RootObject = ReceivedObjects[0];
		TArray<UObject*> AllSubObjects;
		GetObjectsWithOuter(RootObject, AllSubObjects);

		const bool bSameNumberOfObjects = (1 + AllSubObjects.Num() == ReceivedObjects.Num());
		if (!bSameNumberOfObjects)
		{
			FRemoteServerId LocalServerId = UE::RemoteObject::GetGlobalServerId();

			UE_LOG(LogRemoteObject, Warning, TEXT("While migrating '%s' [%s -> %s] we received %d Objects whereas %d Objects got instantiated after deserialization.  Differences:"), *GetNameSafe(RootObject), *MigrationContext.RemoteServerId.ToString(), *LocalServerId.ToString(), ReceivedObjects.Num(), 1 + AllSubObjects.Num());
			TArray<UObject*> ReceivedButNotInstanced;
			for (UObject* Obj : ReceivedObjects)
			{
				if (AllSubObjects.Remove(Obj) < 1)
				{
					ReceivedButNotInstanced.Add(Obj);
				}
			}
			// We'll always have the root object in this list due to AllSubObjects not containing the root object
			ReceivedButNotInstanced.RemoveAtSwap(0);

			auto WrappedGetNameSafe = [](const UObject* O) { return FString::Printf(TEXT("%s (%s)"), *GetNameSafe(O), *(FObjectPtr{ const_cast<UObject*>(O) }.GetRemoteId().ToString())); };
			UE_CLOG(!ReceivedButNotInstanced.IsEmpty(), LogRemoteObject, Warning, TEXT(" Received objects but they weren't instanced: %s"), *FString::JoinBy(ReceivedButNotInstanced, TEXT(", "), WrappedGetNameSafe));
			UE_CLOG(!AllSubObjects.IsEmpty(), LogRemoteObject, Warning, TEXT(" New (or Existing) instances not received: %s"), *FString::JoinBy(AllSubObjects, TEXT(", "), WrappedGetNameSafe));

			if (CVarRemoveMissingMigratedComponents.GetValueOnGameThread())
			{
				// Let's remove all of those instances that we shouldn't have
				for (UObject* Obj : AllSubObjects)
				{
					UE_LOG(LogRemoteObject, Warning, TEXT(" Removing: %s"), *GetNameSafe(Obj));
					Obj->MarkAsGarbage();
				}
			}
		}
	}

#if UE_WITH_REMOTE_OBJECT_HANDLE
	for (UObject* ReceivedObject : ReceivedObjects)
	{
		ReceivedObject->PostMigrate(MigrationContext);
	}
#endif

	// Set the ownership to the correct server id
	for (UObject* ReceivedObject : ReceivedObjects)
	{
		UE::RemoteObject::Handle::ChangeOwnerServerId(ReceivedObject, MigrationContext.OwnerServerId);
		UE::RemoteObject::Handle::ChangePhysicsServerId(ReceivedObject, MigrationContext.PhysicsServerId);
	}

	// Notify that objects have been migrated from a remote server
	OnObjectsReceivedDelegate.Broadcast(ReceivedObjects, MigrationContext);

	// do any existing requests require any of the objects in this hierarchy?
	// if so, we can now update the canonical root id
	FRemoteObjectId RootObjectRemoteId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(ReceivedObjects[0]);

	for (int32 RequestIndex = 0; RequestIndex < GTransferQueue.GetRequestCount(); RequestIndex++)
	{
		FRemoteObjectRequest* ExistingRequest = GTransferQueue.GetRequestByIndex(RequestIndex);

		for (FRemoteObjectId SubObjectId : ReceivedObjectRemoteIds)
		{
			const int32 RequiredObjectIndex = ExistingRequest->RequiredObjects.Find(SubObjectId);
			if (RequiredObjectIndex != INDEX_NONE)
			{
				ExistingRequest->RequiredObjectsCanonicalRoot[RequiredObjectIndex] = RootObjectRemoteId;
			}
		}
	}

	return ReceivedObjects[RequestedObjectIndex];
} 

void MigrateObjectFromRemoteServer(FRemoteObjectId ObjectId, FRemoteServerId CurrentOwnerServerId, UObject* DestinationOuter)
{
	UE_LOG(LogRemoteObject, Verbose, TEXT("Aborting transaction, server %s needs obj id %s from server %s"),
		*UE::RemoteObject::GetGlobalServerId().ToString(),
		*ObjectId.ToString(),
		*CurrentOwnerServerId.ToString());

	checkf(GTransferQueue.ActiveRequest, TEXT("Attempting to access remote object %s but we are outside of a transaction"), *ObjectId.ToString());

	// ensure the request is added in the Open so after we abort it is preserved
	UE_AUTORTFM_OPEN
	{
		// check bInMultiServerCommit here to ensure the UsedObjects list doesn't 
		// accidentally mutate during the actual multi-server commit process
		if (!GTransferQueue.bInMultiServerCommit && GTransferQueue.ActiveRequest->UsedObjects.Find(ObjectId) == INDEX_NONE)
		{
			GTransferQueue.ActiveRequest->UsedObjects.Add(ObjectId);

			OnObjectTouchedDelegate.Broadcast(GTransferQueue.ActiveRequest->RequestId, ObjectId);
		}

		// add it to the active request's list of required objects
		if (GTransferQueue.ActiveRequest->RequiredObjects.Find(ObjectId) == INDEX_NONE)
		{
			// does any other existing request also require this object? if so, try to
			// grab the cached canonical id from them
			FRemoteObjectId CanonicalRootObjectId;

			for (int32 RequestIndex = 0; RequestIndex < GTransferQueue.GetRequestCount(); RequestIndex++)
			{
				FRemoteObjectRequest* ExistingRequest = GTransferQueue.GetRequestByIndex(RequestIndex);

				for (int32 RequiredObjectIndex = 0; RequiredObjectIndex < ExistingRequest->RequiredObjects.Num(); RequiredObjectIndex++)
				{
					FRemoteObjectId RequiredObjectId = ExistingRequest->RequiredObjects[RequiredObjectIndex];

					if (ObjectId == RequiredObjectId)
					{
						CanonicalRootObjectId = ExistingRequest->RequiredObjectsCanonicalRoot[RequiredObjectIndex];
						break;
					}
				}
			}

			GTransferQueue.ActiveRequest->RequiredObjects.Add(ObjectId);
			GTransferQueue.ActiveRequest->RequiredObjectsCanonicalRoot.Add(CanonicalRootObjectId);
		}
		
		if (UE_LOG_ACTIVE(LogRemoteObject, VeryVerbose))
		{
			ANSICHAR HumanReadableString[8192] = { 0 };
			constexpr int32 SkipNumCalls = 5;
			FGenericPlatformStackWalk::StackWalkAndDump(HumanReadableString, sizeof(HumanReadableString), SkipNumCalls);
			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("Callstack: %hs"), HumanReadableString);
		}
	};

	// TODO: DO we want to log when we're pulling over an object we've NEVER known about?!
	// 
	// abort so the outer handler can renegotiate object transfers and retry
	UE::RemoteExecutor::AbortTransactionRequiresDependencies(TEXT("object migration"));
}

void TouchResidentObject(UObject* Object)
{
	using namespace UE::RemoteObject::Serialization;

	if (!Object)
	{
		return;
	}

	// check bInMultiServerCommit here to ensure the UsedObjects list doesn't 
	// accidentally mutate during the actual multi-server commit process
	if (GTransferQueue.bInMultiServerCommit)
	{
		return;
	}

	if (AutoRTFM::IsClosed() && GTransferQueue.ActiveRequest)
	{
		UE_AUTORTFM_OPEN
		{
			const FRemoteObjectId ObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object);

			if (GTransferQueue.ActiveRequest->UsedObjects.Find(ObjectId) == INDEX_NONE)
			{
				GTransferQueue.ActiveRequest->UsedObjects.Add(ObjectId);

				OnObjectTouchedDelegate.Broadcast(GTransferQueue.ActiveRequest->RequestId, ObjectId);
			}

			// add it to the active request's list of required objects
			if (GTransferQueue.ActiveRequest->RequiredObjects.Find(ObjectId) == INDEX_NONE)
			{
				GTransferQueue.ActiveRequest->RequiredObjects.Add(ObjectId);
				
				// NOTE - even though this object is resident, we can't fetch the canonical root
				// object because it could trigger endless recursion into this function which is
				// called when object pointers get resolved
				FRemoteObjectId CanonicalRootObjectId;
				GTransferQueue.ActiveRequest->RequiredObjectsCanonicalRoot.Add(CanonicalRootObjectId);
			}
		};
	}
}

void TransferObjectOwnershipToRemoteServer(UObject* Object, FRemoteServerId DestinationServerId)
{
	using namespace UE::RemoteObject::Serialization;
	using namespace UE::RemoteObject::Private;

	check(!AutoRTFM::IsClosed());

	FRemoteObjectId ObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object);

	// find the canonical root object id to use for arbitration
	UObject* const RootObject = FindCanonicalRootObjectForSerialization(Object);
	const FRemoteObjectId CanonicalRootObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(RootObject);

	// Figure out the ownership semantics
	FRemoteServerId PhysicsServerId = Handle::GetPhysicsServerId(RootObject);
	FUObjectMigrationContext MigrationContext {
		.ObjectId = ObjectId,
		.RemoteServerId = DestinationServerId,
		.OwnerServerId = DestinationServerId,
		.PhysicsServerId = PhysicsServerId,
		.MigrationSide = EObjectMigrationSide::Send
	};
    FScopedObjectMigrationContext ScopedMigrationContext(MigrationContext);
	
	UE_LOG(LogRemoteObject, Verbose, TEXT("TransferObjectOwnershipToRemoteServer %s (%s) root %s (%s) to %s"),
		*ObjectId.ToString(),
		*Object->GetName(),
		*CanonicalRootObjectId.ToString(),
		*RootObject->GetName(),
		*DestinationServerId.ToString());

	// send the object
	TSet<UObject*> SentObjects;
	TSet<UObject*> ReferencedObjects;
	FRemoteObjectData ObjectData = SerializeObjectData(Object, SentObjects, ReferencedObjects, &MigrationContext);

	UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TransferObjectOwnershipToRemoteServer %s serialization complete (%d bytes)"),
		*ObjectId.ToString(),
		ObjectData.GetNumBytes());

	for (UObject* RO : ReferencedObjects)
	{
		RO->SetInternalFlags(EInternalObjectFlags::RemoteReference);
	}

	for (UObject* SO : SentObjects)
	{
		FRemoteObjectId SubObjectId = UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(SO);
		UE_LOG(LogRemoteObject, VeryVerbose, TEXT("TransferObjectOwnershipToRemoteServer %s sent subobject : %s (%s)"),
			*ObjectId.ToString(),
			*SubObjectId.ToString(),
			*GetNameSafe(SO));
	}

	// Notify that object data has been migrated (actually will be but SendObjectData does a MoveTemp on the data)
	OnObjectDataSentDelegate.Broadcast(ObjectData, MigrationContext);

	FMigrateSendParams SendRemoteObjectParams = { .MigrationContext = MigrationContext, .ObjectData = ObjectData };
	GTransferQueue.SendRemoteObject(SendRemoteObjectParams);

	checkf(SentObjects.Num() > 0, TEXT("PeerId:%s unable to migrate object %s to PeerId:%s"), *GetGlobalServerId().ToString(), *UE::CoreUObject::Private::FObjectHandleUtils::GetRemoteId(Object).ToString(), *DestinationServerId.ToString());

	{
		// PostMigrate callbacks may be manipulating migrated objects and may try to migrate them back so prevent that from happening since
		// they all will be destroyed anyway
		FUnsafeToMigrateScope UnsafeToMigrateScope;

		// Notify that objects have been migrated to a remote server
		OnObjectsSentDelegate.Broadcast(SentObjects, MigrationContext);

		for (UObject* SentObject : SentObjects)
		{
#if UE_WITH_REMOTE_OBJECT_HANDLE
			SentObject->PostMigrate(MigrationContext);
#endif
			MarkAsRemote(SentObject, DestinationServerId);
			UE::RemoteObject::Handle::ChangeOwnerServerId(SentObject, DestinationServerId);
		}
	}
}

void RegisterSharedObject(UObject* Object)
{
	UE::RemoteObject::Private::RegisterSharedObject(Object);
}

void RegisterRemoteObjectId(FRemoteObjectId Id, FRemoteServerId ResidentServerId)
{
	UE::RemoteObject::Private::RegisterRemoteObjectId(Id, ResidentServerId);
}

void GetAllBorrowedObjects(TArray<FRemoteObjectId>& OutBorrowedObjectIds)
{
	check(GTransferQueue.ActiveRequest);

	for (const FRemoteObjectId& Id : GTransferQueue.ActiveRequest->UsedObjects)
	{
		if (!UE::RemoteObject::Handle::IsOwned(Id))
		{
			OutBorrowedObjectIds.Add(Id);
		}
	}
}

void MigrateObjectToRemoteServer(FRemoteObjectId ObjectId, FRemoteServerId DestinationServerId)
{
	FRemoteWorkPriority RootWorkPriority = UE::RemoteExecutor::CreateRootWorkPriority();
	MigrateObjectToRemoteServerWithExplicitPriority(RootWorkPriority, ObjectId, DestinationServerId);
}

void MigrateObjectToRemoteServerWithExplicitPriority(FRemoteWorkPriority RequestPriority, FRemoteObjectId ObjectId, FRemoteServerId DestinationServerId)
{
	GTransferQueue.RequestObjectMigration(RequestPriority, ObjectId, DestinationServerId);
}

void OnObjectDataReceived(FRemoteServerId OwnerServerId, FRemoteServerId PhysicsId, FRemoteObjectId ObjectId, FRemoteServerId RemoteServerId, FRemoteObjectData& Data)
{
	check(!AutoRTFM::IsClosed());

	FUObjectMigrationContext MigrationContext {
		.ObjectId = ObjectId,
		.RemoteServerId = RemoteServerId,
		.OwnerServerId = OwnerServerId,
		.PhysicsServerId = PhysicsId,
		.MigrationSide = EObjectMigrationSide::Receive
	};
	FScopedObjectMigrationContext ScopedMigrationContext(MigrationContext);
	
	GTransferQueue.FulfillReceiveRequest(Data, MigrationContext);
}

void OnObjectDataDenied(FRemoteObjectId ObjectId, FRemoteServerId RemoteServerId)
{
	check(!AutoRTFM::IsClosed());
	GTransferQueue.DenyReceiveRequest(ObjectId);
}

void PrefetchRemoteObject(FRemoteObjectId ObjectId, UObject* DestinationOuter)
{
	if (!AutoRTFM::IsClosed())
	{
		// if we are outside of a transaction (dev/test workflow)
		// then prefetch is a no-op
		return;
	}

	bool bAdded = false;
	
	UE_AUTORTFM_OPEN
	{
		// check bInMultiServerCommit here to ensure the UsedObjects list doesn't 
		// accidentally mutate during the actual multi-server commit process
		if (!GTransferQueue.bInMultiServerCommit && GTransferQueue.ActiveRequest->UsedObjects.Find(ObjectId) == INDEX_NONE)
		{
			GTransferQueue.ActiveRequest->UsedObjects.Add(ObjectId);

			OnObjectTouchedDelegate.Broadcast(GTransferQueue.ActiveRequest->RequestId, ObjectId);
		}

		// it's possible a previous prefetch or strong fetch already added this to the RequiredObjects list
		bAdded = (GTransferQueue.ActiveRequest->RequiredObjects.Find(ObjectId) == INDEX_NONE);

		if (bAdded)
		{
			GTransferQueue.ActiveRequest->RequiredObjects.Add(ObjectId);
			GTransferQueue.ActiveRequest->RequiredObjectsCanonicalRoot.Add(ObjectId);
			UE_LOG(LogRemoteObject, VeryVerbose, TEXT("PrefetchRemoteObject(%s) : obj %s"), *GTransferQueue.ActiveRequest->GetRequestId().ToString(), *ObjectId.ToString());
		}
	};
}

void InitRemoteObjectTransfer()
{
	UE::RemoteExecutor::RegisterRemoteSubsystem(&GTransferQueue);
}

} // namespace UE::RemoteObject::Transfer

namespace UE::RemoteObject::Transfer::Private
{

UObject* GetOutermostUnreachableRemoteReference(UObject* Object)
{
	UObject* OutermostUnreachableRemoteReference = Object;
	for (UObject* Outer = Object->GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		FUObjectItem* OuterItem = GUObjectArray.ObjectToObjectItem(Outer);
		if (OuterItem->HasAllFlags(EInternalObjectFlags::RemoteReference | EInternalObjectFlags::Unreachable))
		{
			OutermostUnreachableRemoteReference = Outer;
		}
		else
		{
			break;
		}
	}
	return OutermostUnreachableRemoteReference;
}

void StoreObjectToDatabase(UObject* Object)
{
	using namespace UE::RemoteObject::Serialization;
	using namespace UE::RemoteObject::Private;
	using namespace UE::RemoteObject::Transfer;

	FRemoteObjectId ObjectId(Object);
	if (ObjectId.IsAsset())
	{
		// We don't need to store assets to database since they already exist on disk so we just store their pathname so that we can reload them if needed
		StoreAssetPath(Object);
		return;
	}

	FRemoteServerId LocalServerId = GetGlobalServerId();
	FRemoteServerId OwnerServerId = UE::RemoteObject::Handle::GetOwnerServerId(Object);
	if (LocalServerId != OwnerServerId)
	{
		// We don't own this, and it's definitely remotely owned, just don't save it to the database
		if (UE::RemoteObject::Handle::IsRemote(Object))
		{
			return;
		}

		// This ensure should never fire.  We shouldn't be able to store a non-locally-owned Object into the Database (that is the job of the Owning Server).  A scenario where I've seen this:
		// We borrowed an Object that was Marked As Garbage and didn't return it to the Owning Server before a GC.
		ensureMsgf(false, TEXT("%hs was about to store non-locally-owned Object %s to Server %s's Database; making it remote instead"), __func__, *GetNameSafe(Object), *LocalServerId.ToString());
		MarkAsRemote(Object, OwnerServerId);
		return;
	}

	do
	{
		// Object could be a subobject of its outer that is going to be destroyed later (and have this function invoked on)
		// so to make storing to database (disk) consistent with how objects are migrated (root object with its subobjects in one transfer), 
		// find this object's outermost object that is also unreachable and marked as remote reference.
		// If we re-entered this loop because Object has not been stored to disk with its OutermostUnreachableRemoteReference
		// we still want to try and get an Outer that might also have not been stored. If such outer does not exist GetOutermostUnreachableRemoteReference(Object) will return Object itself.
		UObject* OutermostUnreachableRemoteReference = GetOutermostUnreachableRemoteReference(Object);

		TSet<UObject*> SentObjects;
		TSet<UObject*> ReferencedObjects;
		{
			// Let's setup a fake migration context for storing this
			FRemoteObjectId RootObjectId = FRemoteObjectId { OutermostUnreachableRemoteReference };


			// Setup a migration context for "push migrating to the new owner: the Database Server"
			FUObjectMigrationContext StoreToDatabaseContext {
				.ObjectId = RootObjectId,
				.RemoteServerId = DatabaseId,
				.OwnerServerId = DatabaseId,
				.PhysicsServerId = DatabaseId,
				.MigrationSide = EObjectMigrationSide::Send
			};
			FScopedObjectMigrationContext ScopedMigrationContext(StoreToDatabaseContext);

			// Do the actual serialization
			FRemoteObjectData ObjectData = SerializeObjectData(OutermostUnreachableRemoteReference, SentObjects, ReferencedObjects, &StoreToDatabaseContext);

			const FMigrateSendParams Params { .MigrationContext = StoreToDatabaseContext, .ObjectData = ObjectData };
			StoreRemoteObjectDataDelegate.Execute(Params);
		}

		checkf(SentObjects.Num() > 0, TEXT("PeerId:%s unable to store object %s to DatabaseId:%s"), *GetGlobalServerId().ToString(), *FRemoteObjectId(Object).ToString(), *DatabaseId.ToString());

		for (UObject* ReferencedObject : ReferencedObjects)
		{
			ReferencedObject->SetInternalFlags(EInternalObjectFlags::RemoteReference);
		}

		// MarkAsRemote clears EInternalObjectFlags::RemoteReference flag which will prevent re-enetring this loop with the same outermost object but 
		// we still want to separately process its subobjects (so that they're stored as separate entries in the database) so only mark the root object as remote
		MarkAsRemote(OutermostUnreachableRemoteReference, DatabaseId);

		// It's possible that OutermostUnreachableRemoteReference no longer referenced this Object and Object hasn't been stored to disk
		// in which case we need to repeat this loop until Object is marked as remote (and is no longer marked as RemoteReference).
		
		// Assert if we're about to enter an infinite loop.
		checkf(!Object->HasAnyInternalFlags(EInternalObjectFlags::RemoteReference) || OutermostUnreachableRemoteReference != Object,
			TEXT("PeerId:%s failed to store %s to database"), *GetGlobalServerId().ToString(), *Object->GetPathName());

	} while (Object->HasAnyInternalFlags(EInternalObjectFlags::RemoteReference));
}

} // namespace UE::RemoteObject::Transfer::Private


FRemoteObjectReference::FRemoteObjectReference(const FObjectPtr& Ptr)
{
	ObjectId = Ptr.GetRemoteId();
	if (ObjectId.IsValid())
	{
		if (Ptr.IsResolved())
		{
			ServerId = UE::RemoteObject::GetGlobalServerId();
		}
		else if (UE::RemoteObject::Handle::FRemoteObjectStub* Stub = UE::RemoteObject::Private::FindRemoteObjectStub(ObjectId))
		{
			ServerId = Stub->ResidentServerId;
		}
	}
}

FRemoteObjectReference::FRemoteObjectReference(const FWeakObjectPtr& WeakPtr)
#if UE_WITH_REMOTE_OBJECT_HANDLE
	: FRemoteObjectReference(FObjectPtr(WeakPtr.GetRemoteId()))
#endif
{
}

FObjectPtr FRemoteObjectReference::ToObjectPtr() const
{
	return FObjectPtr(ObjectId);
}

FWeakObjectPtr FRemoteObjectReference::ToWeakPtr() const
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	return FWeakObjectPtr(ObjectId);
#else
	return FWeakObjectPtr();
#endif
}

UObject* FRemoteObjectReference::Resolve() const
{
	return ToObjectPtr().Get();
}

bool FRemoteObjectReference::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	using namespace UE::RemoteObject::Serialization;

	// Serialize remote object id
	Ar << *this;

	// Serialize the pathname of the object if possible which will then be used on the receiving end to find assets loaded by both servers in memory	
	FRemoteObjectPathName PathName;
	if (Ar.IsSaving() && ObjectId.IsValid())
	{		
		if (UObject* ExistingObject = StaticFindObjectFastInternal(ObjectId))
		{
			UE::RemoteObject::Private::RegisterSharedObject(ExistingObject);
			PathName = FRemoteObjectPathName(ExistingObject);
		}
	}

	Ar << PathName;

	if (Ar.IsLoading() && PathName.Num())
	{
		// We currently can't differentiate between assets that exist on both servers which we don't want to migrate
		// so try to find the object in memory first by its id and if it doesn't exist try to find the object by pathname
		// and if such object does exist use its remote id to resolve this reference to avoid migrating the asset from another server
		UObject* ExistingObject = StaticFindObjectFastInternal(ObjectId);
		if (!ExistingObject)
		{
			ExistingObject = PathName.Resolve();
			if (ExistingObject)
			{
				ObjectId = FRemoteObjectId(ExistingObject);
			}
		}
	}
	bOutSuccess = true;
	return bOutSuccess;
}

void FRemoteObjectReference::NetDequantize(FRemoteObjectId InObjectId, FRemoteServerId InServerId, const FRemoteObjectPathName& InPath)
{
	ObjectId = InObjectId;
	ServerId = InServerId;

	UE::RemoteObject::Private::RegisterRemoteObjectId(ObjectId, ServerId);

	if (InPath.Num())
	{
		// We currently can't differentiate between assets that exist on both servers which we don't want to migrate
		// so try to find the object in memory first by its id and if it doesn't exist try to find the object by pathname
		// and if such object does exist use its remote id to resolve this reference to avoid migrating the asset from another server
		UObject* ExistingObject = StaticFindObjectFastInternal(ObjectId);
		if (!ExistingObject)
		{
			ExistingObject = InPath.Resolve();
			if (ExistingObject)
			{
				ObjectId = FRemoteObjectId(ExistingObject);
			}
		}
	}
}

FArchive& operator<<(FArchive& Ar, FRemoteObjectReference& Ref)
{
	Ar << Ref.ObjectId;
	Ar << Ref.ServerId;

	if (Ar.IsLoading())
	{
		UE::RemoteObject::Private::RegisterRemoteObjectId(Ref.ObjectId, Ref.ServerId);
	}

	return Ar;
}

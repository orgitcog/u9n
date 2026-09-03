// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/UObjectMigrationContext.h"

#include "Serialization/Archive.h"
#include "UObject/RemoteObjectTransfer.h"
#include "UObject/UObjectThreadContext.h"

bool FUObjectMigrationContext::IsValid() const
{
	return ObjectId.IsValid() && RemoteServerId.IsValid() && OwnerServerId.IsValid();
}

EObjectMigrationRecvType FUObjectMigrationContext::GetObjectMigrationRecvType(const UObjectBase* Object) const
{
	return GetObjectMigrationRecvType(FRemoteObjectId { Object });
}

EObjectMigrationRecvType FUObjectMigrationContext::GetObjectMigrationRecvType(FRemoteObjectId InObjectId) const
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	if (!ensureMsgf(IsValid(), TEXT("%hs called on an invalid MigrationContext"), __func__))
	{
		return EObjectMigrationRecvType::Invalid;
	}

	const FRemoteServerId LocalServerId = UE::RemoteObject::GetGlobalServerId();
	const bool bAlreadyOwns = UE::RemoteObject::Handle::IsOwned(InObjectId);

	const bool bAssignOwnership = (OwnerServerId == LocalServerId) && !bAlreadyOwns;
	const bool bReturnLoan = (OwnerServerId == LocalServerId) && bAlreadyOwns;

	if (bAssignOwnership)
	{
		// We were pushed an object that we do not already own, so we must assume ownership of it
		return EObjectMigrationRecvType::AssignedOwnership;
	}

	if (bReturnLoan)
	{
		// We were given an object back that we already own
		return EObjectMigrationRecvType::ReturnedLoan;
	}

	// We received an object and ownership is not involved, so we are just borrowing it
	return EObjectMigrationRecvType::Borrowed;
#else
	return EObjectMigrationRecvType::Invalid;
#endif
}

EObjectMigrationSendType FUObjectMigrationContext::GetObjectMigrationSendType(const UObjectBase* Object) const
{
	return GetObjectMigrationSendType(FRemoteObjectId { Object });
}

EObjectMigrationSendType FUObjectMigrationContext::GetObjectMigrationSendType(FRemoteObjectId InObjectId) const
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	if (!ensureMsgf(IsValid(), TEXT("%hs passed an invalid MigrationContext"), __func__))
	{
		return EObjectMigrationSendType::Invalid;
	}

	const FRemoteServerId LocalServerId = UE::RemoteObject::GetGlobalServerId();
	const bool bCurrentlyOwns = UE::RemoteObject::Handle::IsOwned(InObjectId);

	const bool bOwnershipTransfer = (OwnerServerId == RemoteServerId) && bCurrentlyOwns;
	const bool bReturnBorrowed = (OwnerServerId == RemoteServerId) && !bCurrentlyOwns;

	if (bOwnershipTransfer)
	{
		// We have ownership of this object and are pushing it to the destination (reassigning the ownership)
		return EObjectMigrationSendType::ReassignOwnership;
	}

	if (bReturnBorrowed)
	{
		// Send an object we don't own back to the owning server (it requested the object back).
		return EObjectMigrationSendType::ReturnBorrowed;
	}

	// By default we loan the object to the destination server but ownership does not change
	return EObjectMigrationSendType::Loan;
#else
	return EObjectMigrationSendType::Invalid;
#endif
}

const TCHAR* ToString(EObjectMigrationRecvType Value)
{
	switch (Value)
	{
		case EObjectMigrationRecvType::Borrowed:
			return TEXT("Borrowed");

		case EObjectMigrationRecvType::ReturnedLoan:
			return TEXT("ReturnedLoan");

		case EObjectMigrationRecvType::AssignedOwnership:
			return TEXT("AssignedOwnership");

		default:
			return TEXT("Invalid");
	}
}

const TCHAR* ToString(EObjectMigrationSendType Value)
{
	switch (Value)
	{
		case EObjectMigrationSendType::Loan:
			return TEXT("Loan");

		case EObjectMigrationSendType::ReturnBorrowed:
			return TEXT("ReturnBorrowed");

		case EObjectMigrationSendType::ReassignOwnership:
			return TEXT("ReassignOwnership");

		default:
			return TEXT("Invalid");
	}
}

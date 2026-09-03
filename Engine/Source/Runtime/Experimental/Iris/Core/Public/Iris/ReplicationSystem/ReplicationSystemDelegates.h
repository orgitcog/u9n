// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Delegates/Delegate.h"
#include "Net/Core/Connection/ConnectionHandle.h"

namespace UE::Net::Private
{
	class FReplicationSystemImpl;
}

namespace UE::Net
{

class FReplicationSystemDelegates
{
public:
	using FConnectionAddedDelegate = TMulticastDelegate<void(FConnectionHandle ConnectionHandle)>;
	using FConnectionRemovedDelegate = TMulticastDelegate<void(FConnectionHandle ConnectionHandle)>;

	/** 
	 * Returns a delegate registration instance allowing the caller to register their FConnectionAddedDelegate. 
	 * The delegate will be called when a valid and not previously added connection is registered via a FReplicationSystem::AddConnection call.
	 * Currently only parent connections will call the delegates.
	 */
	FConnectionAddedDelegate::RegistrationType& OnConnectionAdded(); 

	/**
	 * Returns a delegate registration instance allowing the caller to register their FConnectionRemovedDelegate.
	 * The delegate will be called when a previously successfully added connection is removed via a FReplicationSystem::RemoveConnection call. 
	 * Currently only parent connections will call the delegates.
	 */
	FConnectionAddedDelegate::RegistrationType& OnConnectionRemoved(); 

private:
	friend UE::Net::Private::FReplicationSystemImpl;

	FConnectionAddedDelegate ConnectionAddedDelegate;
	FConnectionRemovedDelegate ConnectionRemovedDelegate;
};

inline FReplicationSystemDelegates::FConnectionAddedDelegate::RegistrationType& FReplicationSystemDelegates::OnConnectionAdded()
{
	return ConnectionAddedDelegate;
}

inline FReplicationSystemDelegates::FConnectionRemovedDelegate::RegistrationType& FReplicationSystemDelegates::OnConnectionRemoved()
{
	return ConnectionRemovedDelegate;
}

}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Iris/ReplicationSystem/NetEngineFactories.h"

#if UE_WITH_IRIS
#include "Net/Iris/ReplicationSystem/NetActorFactory.h"
#include "Net/Iris/ReplicationSystem/NetSubObjectFactory.h"

#include "Iris/ReplicationSystem/NetObjectFactoryRegistry.h"
#endif 

#if UE_WITH_IRIS

namespace UE::Net
{

namespace Private
{
	static bool bAreFactoriesRegistered = false;
}

void InitEngineNetObjectFactories()
{
	using namespace UE::Net::Private;

	if (ensure(bAreFactoriesRegistered == false))
	{
		FNetObjectFactoryRegistry::RegisterFactory(UNetActorFactory::StaticClass(), UNetActorFactory::GetFactoryName());
		FNetObjectFactoryRegistry::RegisterFactory(UNetSubObjectFactory::StaticClass(), UNetSubObjectFactory::GetFactoryName());
		bAreFactoriesRegistered = true;
	}
}

void ShutdownEngineNetObjectFactories()
{
	using namespace UE::Net::Private;

	if (bAreFactoriesRegistered)
	{
		FNetObjectFactoryRegistry::UnregisterFactory(UNetActorFactory::GetFactoryName());
		FNetObjectFactoryRegistry::UnregisterFactory(UNetSubObjectFactory::GetFactoryName());
		bAreFactoriesRegistered = false;
	}
}

} // end namespace UE::Net

#endif // UE_WITH_IRIS
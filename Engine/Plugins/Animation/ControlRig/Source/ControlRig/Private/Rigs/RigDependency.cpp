// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigDependency.h"

#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"

const TMap<int32, TArray<int32>>& FNoDependenciesProvider::GetDependencies() const
{
	static const TMap<int32, TArray<int32>> Empty;
	return Empty;
}

FRigVMDependenciesProvider::FRigVMDependenciesProvider(const URigHierarchy* InHierarchy, URigVM* InVM, FName InEventName)
	: WeakHierarchy(InHierarchy)
	, WeakRigVM(InVM)
	, EventName(InEventName)
{}
	
const TMap<int32, TArray<int32>>& FRigVMDependenciesProvider::GetDependencies() const
{
	// NOTE: read URigHierarchy::GetDependenciesForVM about using this function with modular rigs.

#if WITH_EDITOR
	const URigHierarchy* Hierarchy = WeakHierarchy.Get();
	if (ensure(Hierarchy))
	{
		uint32 NewHash = 0; 
		auto LazyGetTopologyHash = [Hierarchy, &NewHash]()
		{
			if (NewHash == 0)
			{
				// NOTE: GetTopologyVersion() is used to track simple topology changes (GetTopologyHash() is probably more complete but also slower)
				NewHash = Hierarchy->GetTopologyVersion();
			}
			return NewHash;
		};
		
		if (CachedDependencies.IsEmpty() || TopologyHash != LazyGetTopologyHash())
		{
			URigVM* RigVM = WeakRigVM.Get();
			if (ensure(RigVM))
			{
				CachedDependencies = Hierarchy->GetDependenciesForVM(RigVM, EventName);
				TopologyHash = NewHash;
			}
		}
	}
#endif

	return CachedDependencies;
}
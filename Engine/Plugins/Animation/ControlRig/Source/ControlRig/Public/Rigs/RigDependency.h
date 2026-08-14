// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#define UE_API CONTROLRIG_API

class URigHierarchy;
class URigVM;

/**
 * IDependenciesProvider provides an interface for constructing and passing elements dependencies to functions that need to perform dependency tests.
 * This is currently mainly used to ensure that certain rig elements are not dependent on each other (parent switching, constraints ordering, etc.) 
 */

struct IDependenciesProvider
{
	virtual ~IDependenciesProvider() = default;
	virtual const TMap<int32, TArray<int32>>& GetDependencies() const = 0;
};

/**
 * FNoDependenciesProvider is the default dependency provider (no dependency at all) 
 */

struct FNoDependenciesProvider : public IDependenciesProvider
{
	virtual ~FNoDependenciesProvider() override = default;
	UE_API virtual const TMap<int32, TArray<int32>>& GetDependencies() const override;
};

/**
 * FRigVMDependenciesProvider builds a dependency map based on the provided RigVM's instructions
 */

struct FRigVMDependenciesProvider : public IDependenciesProvider
{
	FRigVMDependenciesProvider() = default;
	UE_API FRigVMDependenciesProvider(const URigHierarchy* InHierarchy, URigVM* InVM, FName InEventName = NAME_None);
	virtual ~FRigVMDependenciesProvider() override = default;
	
	UE_API virtual const TMap<int32, TArray<int32>>& GetDependencies() const override;
	
private:
		
	TWeakObjectPtr<const URigHierarchy> WeakHierarchy = nullptr;
	TWeakObjectPtr<URigVM> WeakRigVM = nullptr;
	FName EventName = NAME_None;
	
	mutable TMap<int32, TArray<int32>> CachedDependencies;
	mutable uint32 TopologyHash = 0;
};

#undef UE_API

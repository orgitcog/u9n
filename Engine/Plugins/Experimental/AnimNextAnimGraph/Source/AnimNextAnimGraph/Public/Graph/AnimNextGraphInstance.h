// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "HAL/CriticalSection.h"
#include "TraitCore/TraitEvent.h"
#include "TraitCore/TraitEventList.h"
#include "TraitCore/TraitPtr.h"
#include "RigVMCore/RigVMExecuteContext.h"

#include "AnimNextGraphInstance.generated.h"

#define UE_API ANIMNEXTANIMGRAPH_API

class FReferenceCollector;

struct FRigUnit_AnimNextGraphEvaluator;
class UAnimNextAnimationGraph;
class UAnimNextModule;
struct FAnimNextModuleInstance;
class FRigVMTraitScope;
struct FRigVMExtendedExecuteContext;
struct FAnimNextExecuteContext;
struct FAnimNextModuleInjectionComponent;
struct FRigUnit_AnimNextRunAnimationGraph_v1;
struct FRigUnit_AnimNextRunAnimationGraph_v2;
struct FAnimNextStateTreeRigVMConditionBase;
struct FAnimNextStateTreeRigVMTaskBase;

namespace UE::AnimNext
{
	class IDataInterfaceHost;
	struct FExecutionContext;
	struct FGraphInstanceComponent;
	struct FLatentPropertyHandle;
	struct FTraitStackBinding;
	struct FPlayAnimSlotTrait;
	struct FBlendStackCoreTrait;
	struct FInjectionRequest;
	struct FInjectionSiteTrait;
	struct FStateTreeTrait;
	struct FCachedBindingInfo;
	struct FInjectionUtils;
	struct FBlendSpacePlayerTrait;
}

using GraphInstanceComponentMapType = TMap<FName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>>;

// Represents an instance of an AnimNext graph
// This struct uses UE reflection because we wish for the GC to keep the graph
// alive while we own a reference to it. It is not intended to be serialized on disk with a live instance.
USTRUCT()
struct FAnimNextGraphInstance : public FAnimNextDataInterfaceInstance
{
	GENERATED_BODY()

	// Creates an empty graph instance that doesn't reference anything
	UE_API FAnimNextGraphInstance();

	// No copying, no moving
	FAnimNextGraphInstance(const FAnimNextGraphInstance&) = delete;
	FAnimNextGraphInstance& operator=(const FAnimNextGraphInstance&) = delete;

	// If the graph instance is allocated, we release it during destruction
	UE_API ~FAnimNextGraphInstance();

	// Releases the graph instance and frees all corresponding memory
	UE_API void Release();

	// Returns true if we have a live graph instance, false otherwise
	UE_API bool IsValid() const;

	// Returns the animation graph used by this instance or nullptr if the instance is invalid
	UE_API const UAnimNextAnimationGraph* GetAnimationGraph() const;

	// Returns the entry point in Graph that this instance corresponds to 
	UE_API FName GetEntryPoint() const;
	
	// Returns a weak handle to the root trait instance
	UE_API UE::AnimNext::FWeakTraitPtr GetGraphRootPtr() const;

	// Returns the module instance that owns us or nullptr if we are invalid
	UE_API FAnimNextModuleInstance* GetModuleInstance() const;

	// Returns the parent graph instance that owns us or nullptr for the root graph instance or if we are invalid
	UE_API FAnimNextGraphInstance* GetParentGraphInstance() const;

	// Returns the root graph instance that owns us and the components or nullptr if we are invalid
	UE_API FAnimNextGraphInstance* GetRootGraphInstance() const;

	// Check to see if this instance data matches the provided animation graph
	UE_API bool UsesAnimationGraph(const UAnimNextAnimationGraph* InAnimationGraph) const;

	// Check to see if this instance data matches the provided graph entry point
	UE_API bool UsesEntryPoint(FName InEntryPoint) const;
	
	// Returns whether or not this graph instance is the root graph instance or false otherwise
	UE_API bool IsRoot() const;

	// Adds strong/hard object references during GC
	UE_API void AddStructReferencedObjects(class FReferenceCollector& Collector);

	// Returns a typed graph instance component, creating it lazily the first time it is queried
	template<class ComponentType>
	ComponentType& GetComponent();

	// Returns a typed graph instance component pointer if found or nullptr otherwise
	template<class ComponentType>
	ComponentType* TryGetComponent();

	// Returns a typed graph instance component pointer if found or nullptr otherwise
	template<class ComponentType>
	const ComponentType* TryGetComponent() const;

	// Returns const iterators to the graph instance component container
	UE_API GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const;

	// Returns whether or not this graph instance has updated at least once
	UE_API bool HasUpdated() const;

	// Called each time the graph updates to mark the instance as updated
	UE_API void MarkAsUpdated();

	// Get a reference to the internal public variables
	const FInstancedPropertyBag& GetVariables() const { return Variables; }

private:
	// Returns a pointer to the specified component, or nullptr if not found
	UE_API UE::AnimNext::FGraphInstanceComponent* TryGetComponent(int32 ComponentNameHash, FName ComponentName) const;

	// Adds the specified component and returns a reference to it
	UE_API UE::AnimNext::FGraphInstanceComponent& AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>&& Component);

	// Executes a list of latent RigVM pins and writes the result into the destination pointer (latent handle offsets are using the destination as base)
	// When frozen, latent handles that can freeze are skipped, all others will execute
	UE_API void ExecuteLatentPins(const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& LatentHandles, void* DestinationBasePtr, bool bIsFrozen);

#if WITH_EDITORONLY_DATA
	// During graph compilation, if we have existing graph instances, we freeze them by releasing their memory before thawing them
	// Freezing is a partial release of resources that retains the necessary information to re-create things safely
	UE_API void Freeze();

	// During graph compilation, once compilation is done we thaw existing graph instances to reallocate their memory
	UE_API void Thaw();

	// Hook into module compilation
	UE_API void OnModuleCompiled(UAnimNextModule* InModule);
#endif

	// Check if variable binding is required
	bool RequiresPublicVariableBinding() const
	{
		return PublicVariablesState == EPublicVariablesState::Unbound;
	}

	// Helper function used to bind to cached bindings after they are built
	UE_API bool BindToCachedBindings();

	// Bind the variables in the supplied traits in scope to their respective public variables, represented by structs held on a host
	// Note: struct views are mutable here as bindings allow writes!
	UE_API void BindPublicVariables(TArrayView<FStructView> InHostStructs, TConstArrayView<UE::AnimNext::IDataInterfaceHost*> InHosts = TConstArrayView<UE::AnimNext::IDataInterfaceHost*>());

	// Bind the variables in the supplied traits in scope to their respective public variables, so they point at host memory
	UE_API void BindPublicVariables(TConstArrayView<UE::AnimNext::IDataInterfaceHost*> InHosts);

	// Unbind any public variables that were pointing at host memory and re-point them at the internal defaults
	UE_API void UnbindPublicVariables();

	// Get a reference to the internal public variables
	FInstancedPropertyBag& GetMutableVariables() { return Variables; }

	// The entry point in Graph that this instance corresponds to 
	FName EntryPoint;

	// Hard reference to the graph instance data, we own it
	UE::AnimNext::FTraitPtr GraphInstancePtr;

	// The module instance that owns the root, us and the components
	FAnimNextModuleInstance* ModuleInstance = nullptr;

	// The root graph instance that owns us and the components
	FAnimNextGraphInstance* RootGraphInstance = nullptr;

	struct FCachedDataInterfaceBinding
	{
		struct FVariable
		{
			FVariable() = default;

			FVariable(const FName& InVariableName, int32 InInterfaceVariableIndex, const FProperty* InProperty, uint8* InMemory)
				: VariableName(InVariableName)
				, InterfaceVariableIndex(InInterfaceVariableIndex)
				, Property(InProperty)
				, Memory(InMemory)
			{
			}

			// Name of the variable
			FName VariableName;

			// Index of the variable in the public variables property bag for DataInterface
			int32 InterfaceVariableIndex = INDEX_NONE;

			// Cached property of the variable, used for type validation
			const FProperty* Property = nullptr;

			// Memory of the variable, bound to the 'outermost' container's property bag that implements DataInterface
			uint8* Memory = nullptr;
		};

		const UAnimNextDataInterface* DataInterface = nullptr;
		TArray<FVariable> CachedBindings;
	};

	UE_API void UpdateCachedBindingsForHost();
	UE_API void UpdateCachedBindingsForHost(const FAnimNextModuleInstance& InHost);
	UE_API void UpdateCachedBindingsForHost(const UE::AnimNext::IDataInterfaceHost& InHost);

	// Helper function used to cache a host's bindings, overriding any of the already cached bindings that are valid.
	// Template to allow binding to both FAnimNextDataInterfaceInstance and IDataInterfaceHost without an abstraction between the two.
	template<typename HostType>
	void UpdateCachedBindingsForHostHelper(const HostType& InHost);

	// Bindings we have to our outer host(s), per data interface
	TArray<FCachedDataInterfaceBinding> CachedBindings;

	// Graph instance components that persist from update to update
	GraphInstanceComponentMapType Components;

	// The current state of public variable bindings to the host
	enum class EPublicVariablesState : uint8
	{
		None,		// No public variables present
		Unbound,	// Present, but currently unbound
		Bound		// Present and bound
	};

	EPublicVariablesState PublicVariablesState;

	// Whether or not this graph has updated once
	bool bHasUpdatedOnce : 1 = false;

	friend UAnimNextAnimationGraph;			// The graph is the one that allocates instances
	friend FRigUnit_AnimNextGraphEvaluator;	// We evaluate the instance
	friend UE::AnimNext::FExecutionContext;
	friend UE::AnimNext::FTraitStackBinding;
	friend UE::AnimNext::FPlayAnimSlotTrait;
	friend UE::AnimNext::FBlendStackCoreTrait;
	friend UE::AnimNext::FInjectionRequest;
	friend UE::AnimNext::FInjectionSiteTrait;
	friend UE::AnimNext::FStateTreeTrait;	// Temp - remove: Needs to access mutable variables to copy-in data OnBecomeRelevant
	friend FAnimNextModuleInjectionComponent;
	friend FRigUnit_AnimNextRunAnimationGraph_v1;
	friend FRigUnit_AnimNextRunAnimationGraph_v2;
	friend UE::AnimNext::FInjectionUtils;
	friend FAnimNextStateTreeRigVMConditionBase;	// Temp - remove: Needs to access mutable variables to copy-in data for function shim
	friend FAnimNextStateTreeRigVMTaskBase;	// Temp - remove: Needs to access mutable variables to copy-in data for function shim
	friend UE::AnimNext::FBlendSpacePlayerTrait;
};

template<>
struct TStructOpsTypeTraits<FAnimNextGraphInstance> : public TStructOpsTypeTraitsBase2<FAnimNextGraphInstance>
{
	enum
	{
		WithAddStructReferencedObjects = true,
		WithCopy = false,
	};
};

//////////////////////////////////////////////////////////////////////////

template<class ComponentType>
ComponentType& FAnimNextGraphInstance::GetComponent()
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	if (UE::AnimNext::FGraphInstanceComponent* Component = TryGetComponent(ComponentNameHash, ComponentName))
	{
		return *static_cast<ComponentType*>(Component);
	}

	return static_cast<ComponentType&>(AddComponent(ComponentNameHash, ComponentName, MakeShared<ComponentType>(*this)));
}

template<class ComponentType>
ComponentType* FAnimNextGraphInstance::TryGetComponent()
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}

template<class ComponentType>
const ComponentType* FAnimNextGraphInstance::TryGetComponent() const
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}

#undef UE_API

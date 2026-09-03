// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphInstance.h"

#include "AnimNextAnimGraphStats.h"
#include "DataInterface/AnimNextDataInterfaceHost.h"
#include "DataInterface/DataInterfaceStructAdapter.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "Graph/AnimNextGraphContextData.h"
#include "Graph/AnimNextGraphLatentPropertiesContextData.h"
#include "TraitCore/ExecutionContext.h"
#include "Graph/GC_GraphInstanceComponent.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"
#include "Graph/RigVMTrait_AnimNextPublicVariables.h"
#include "Logging/StructuredLog.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/ModuleGuard.h"

DEFINE_STAT(STAT_AnimNext_Graph_RigVM);

FAnimNextGraphInstance::FAnimNextGraphInstance()
{
#if WITH_EDITORONLY_DATA
	UAnimNextModule::OnModuleCompiled().AddRaw(this, &FAnimNextGraphInstance::OnModuleCompiled);
#endif
}

FAnimNextGraphInstance::~FAnimNextGraphInstance()
{
	Release();
}

void FAnimNextGraphInstance::Release()
{
#if WITH_EDITORONLY_DATA
	UAnimNextModule::OnModuleCompiled().RemoveAll(this);

	if(const UAnimNextAnimationGraph* Graph = GetAnimationGraph())
	{
		FScopeLock Lock(&Graph->GraphInstancesLock);
		Graph->GraphInstances.Remove(this);
	}
#endif

	if (!GraphInstancePtr.IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	ModuleInstance = nullptr;
	RootGraphInstance = nullptr;
	ExtendedExecuteContext.Reset();
	Components.Empty();
	DataInterface = nullptr;
}

bool FAnimNextGraphInstance::IsValid() const
{
	return GraphInstancePtr.IsValid();
}

const UAnimNextAnimationGraph* FAnimNextGraphInstance::GetAnimationGraph() const
{
	return CastChecked<UAnimNextAnimationGraph>(DataInterface);
}

FName FAnimNextGraphInstance::GetEntryPoint() const
{
	return EntryPoint;
}

UE::AnimNext::FWeakTraitPtr FAnimNextGraphInstance::GetGraphRootPtr() const
{
	return GraphInstancePtr;
}

FAnimNextModuleInstance* FAnimNextGraphInstance::GetModuleInstance() const
{
	return ModuleInstance;
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetParentGraphInstance() const
{
	if(IsRoot())
	{
		return nullptr;
	}
	else
	{
		return static_cast<FAnimNextGraphInstance*>(HostInstance);
	}
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetRootGraphInstance() const
{
	return RootGraphInstance;
}

bool FAnimNextGraphInstance::UsesAnimationGraph(const UAnimNextAnimationGraph* InAnimationGraph) const
{
	return GetAnimationGraph() == InAnimationGraph;
}

bool FAnimNextGraphInstance::UsesEntryPoint(FName InEntryPoint) const
{
	if(const UAnimNextAnimationGraph* AnimationGraph = GetAnimationGraph())
	{
		if(InEntryPoint == NAME_None)
		{
			return EntryPoint == AnimationGraph->DefaultEntryPoint;
		}

		return InEntryPoint == EntryPoint;
	}
	return false;
}

bool FAnimNextGraphInstance::IsRoot() const
{	
	return this == RootGraphInstance;
}

void FAnimNextGraphInstance::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	if (!IsRoot())
	{
		return;	// If we aren't the root graph instance, we don't own the components
	}

	if (const UE::AnimNext::FGCGraphInstanceComponent* Component = TryGetComponent<UE::AnimNext::FGCGraphInstanceComponent>())
	{
		Component->AddReferencedObjects(Collector);
	}
}

UE::AnimNext::FGraphInstanceComponent* FAnimNextGraphInstance::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
{
	if (const TSharedPtr<UE::AnimNext::FGraphInstanceComponent>* Component = RootGraphInstance->Components.FindByHash(ComponentNameHash, ComponentName))
	{
		return Component->Get();
	}

	return nullptr;
}

UE::AnimNext::FGraphInstanceComponent& FAnimNextGraphInstance::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>&& Component)
{
	return *RootGraphInstance->Components.AddByHash(ComponentNameHash, ComponentName, MoveTemp(Component)).Get();
}

GraphInstanceComponentMapType::TConstIterator FAnimNextGraphInstance::GetComponentIterator() const
{
	return RootGraphInstance->Components.CreateConstIterator();
}

bool FAnimNextGraphInstance::HasUpdated() const
{
	return bHasUpdatedOnce;
}

void FAnimNextGraphInstance::MarkAsUpdated()
{
	bHasUpdatedOnce = true;
}

void FAnimNextGraphInstance::ExecuteLatentPins(const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& LatentHandles, void* DestinationBasePtr, bool bIsFrozen)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_RigVM);

	if (!IsValid())
	{
		return;
	}

	if (URigVM* VM = GetAnimationGraph()->RigVM)
	{
		FAnimNextExecuteContext& AnimNextContext = ExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();
		if (ModuleInstance)
		{
			AnimNextContext.SetOwningObject(ModuleInstance->GetObject());
		}

		// Insert our context data for the scope of execution
		FAnimNextGraphLatentPropertiesContextData ContextData(ModuleInstance, this, LatentHandles, DestinationBasePtr, bIsFrozen);
		UE::AnimNext::FScopedExecuteContextData ContextDataScope(AnimNextContext, ContextData);

		VM->ExecuteVM(ExtendedExecuteContext, FRigUnit_AnimNextShimRoot::EventName);
	}
}

#if WITH_EDITORONLY_DATA
void FAnimNextGraphInstance::Freeze()
{
	if (!IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	ExtendedExecuteContext.Reset();
	Components.Empty();
	CachedBindings.Reset();
	PublicVariablesState = PublicVariablesState == EPublicVariablesState::Bound ? EPublicVariablesState::Unbound : EPublicVariablesState::None;
	bHasUpdatedOnce = false;
}

void FAnimNextGraphInstance::Thaw()
{
	if (const UAnimNextAnimationGraph* AnimationGraph = GetAnimationGraph())
	{
		Variables.MigrateToNewBagInstance(AnimationGraph->VariableDefaults);

		ExtendedExecuteContext = AnimationGraph->ExtendedExecuteContext;

		{
			UE::AnimNext::FExecutionContext Context(*this);
			if(const FAnimNextTraitHandle* FoundHandle = AnimationGraph->ResolvedRootTraitHandles.Find(EntryPoint))
			{
				GraphInstancePtr = Context.AllocateNodeInstance(*this, *FoundHandle);
			}
		}

		if (!IsValid())
		{
			// We failed to allocate our instance, clear everything
			Release();
		}
	}
}

void FAnimNextGraphInstance::OnModuleCompiled(UAnimNextModule* InModule)
{
	auto DependentModuleCompiled = [this, InModule]()
	{
		bool bFound = false;
		ModuleInstance->ForEachPrerequisite([InModule, &bFound](FAnimNextModuleInstance& InPrerequisite)
		{
			if(InPrerequisite.GetModule() == InModule)
			{
				bFound = true;
			}
		});
		return bFound;
	};

	// If we are hosted transitively by the compiled module, or if we could be bound to the compiled module,
	// invalidate and mark our bindings as needing update. They will be lazily re-bound the next time we run.
	if(ModuleInstance)
	{
		UE::AnimNext::FModuleWriteGuard Guard(ModuleInstance);
		if(InModule == ModuleInstance->GetModule() || DependentModuleCompiled())
		{
			UnbindPublicVariables();
		}
	}
}

#endif

bool FAnimNextGraphInstance::BindToCachedBindings()
{
	bool bPublicVariablesBound = false;

	auto FindCachedInterfaceBindings = [this](const UAnimNextDataInterface* InDataInterface) -> const FCachedDataInterfaceBinding*
	{
		for(const FCachedDataInterfaceBinding& CachedBinding : CachedBindings)
		{
			if(CachedBinding.DataInterface == InDataInterface)
			{
				return &CachedBinding;
			}
		}

		return nullptr;
	};

	const UAnimNextAnimationGraph* AnimationGraph = GetAnimationGraph();
	const UPropertyBag* PropertyBag = Variables.GetPropertyBagStruct();
	for(const FAnimNextImplementedDataInterface& ImplementedInterface : AnimationGraph->GetImplementedInterfaces())
	{
		if(!ImplementedInterface.bAutoBindToHost)
		{
			continue;
		}

		const FCachedDataInterfaceBinding* CachedBinding = FindCachedInterfaceBindings(ImplementedInterface.DataInterface);
		if(CachedBinding == nullptr)
		{
			// Did not cache this binding, so skip
			continue;
		}

		if (CachedBinding->CachedBindings.Num() != ImplementedInterface.NumVariables)
		{
			UE_LOGFMT(LogAnimation, Error, "Interface size mismatch: {CachedInterfaceName} {CachedSize} vs {ImplementedInterfaceName} {ImplementedSize}. Interface's values will not be updated at runtime.", CachedBinding->DataInterface->GetFName(), CachedBinding->CachedBindings.Num(), ImplementedInterface.DataInterface->GetFName(), ImplementedInterface.NumVariables);
			continue;
		}

		// Index into local property bag
		int32 VariableIndex = ImplementedInterface.VariableIndex;
		const int32 EndVariableIndex = ImplementedInterface.VariableIndex + ImplementedInterface.NumVariables;

		// Index into interface
		int32 InterfaceVariableIndex = 0;

		for(; VariableIndex < EndVariableIndex; ++VariableIndex, ++InterfaceVariableIndex)
		{
			const FPropertyBagPropertyDesc& Desc = PropertyBag->GetPropertyDescs()[VariableIndex];
			const FCachedDataInterfaceBinding::FVariable& CachedBindingVariable = CachedBinding->CachedBindings[InterfaceVariableIndex];
			if(CachedBindingVariable.Memory == nullptr)
			{
				UE_LOGFMT(LogAnimation, Error, "FAnimNextGraphInstance::BindToCachedBindings: Did not find cached binding for {VariableName} - variable's values will not be updated at runtime.", Desc.Name);
				continue;
			}

			// Validate that interface layout is the same
			if( Desc.Name == CachedBindingVariable.VariableName &&
				InterfaceVariableIndex == CachedBindingVariable.InterfaceVariableIndex &&
				Desc.CachedProperty->GetClass() == CachedBindingVariable.Property->GetClass())
			{
				ExtendedExecuteContext.ExternalVariableRuntimeData[VariableIndex].Memory = CachedBindingVariable.Memory;
				bPublicVariablesBound = true;
			}
			else
			{
				UE_LOGFMT(LogAnimation, Error, "Interface layout mismatch for {GraphName}. Interface's values will not be updated at runtime. (Have:Need): {HaveName}:{NeedName}, {HaveIndex}:{NeedIndex}, {HaveType}:{NeedType}", AnimationGraph->GetFName(), Desc.Name, CachedBindingVariable.VariableName, InterfaceVariableIndex, CachedBindingVariable.InterfaceVariableIndex, Desc.CachedProperty->GetClass()->GetFName(), CachedBindingVariable.Property->GetClass()->GetFName());
				continue;
			}
		}
	}

	return bPublicVariablesBound;
}

void FAnimNextGraphInstance::UpdateCachedBindingsForHost()
{
	if(IsRoot())
	{
		if(FAnimNextModuleInstance* ModuleHost = GetModuleInstance())
		{
			UpdateCachedBindingsForHost(*ModuleHost);
		}
	}
	else if(FAnimNextGraphInstance* ParentGraphInstance = GetParentGraphInstance())
	{
		// Just copy parent cached bindings
		CachedBindings = ParentGraphInstance->CachedBindings;
	}
}

void FAnimNextGraphInstance::UpdateCachedBindingsForHost(const FAnimNextModuleInstance& InHost)
{
	UpdateCachedBindingsForHostHelper(InHost);
}

void FAnimNextGraphInstance::UpdateCachedBindingsForHost(const UE::AnimNext::IDataInterfaceHost& InHost)
{
	UpdateCachedBindingsForHostHelper(InHost);
}

template<typename HostType>
void FAnimNextGraphInstance::UpdateCachedBindingsForHostHelper(const HostType& InHost)
{
	auto FindOrAddCachedInterfaceBindings = [this](const UAnimNextDataInterface* InDataInterface, int32 InNumVariables) -> FCachedDataInterfaceBinding&
	{
		for(FCachedDataInterfaceBinding& CachedBinding : CachedBindings)
		{
			if(CachedBinding.DataInterface == InDataInterface)
			{
				return CachedBinding;
			}
		}

		FCachedDataInterfaceBinding& NewCachedBinding = CachedBindings.AddDefaulted_GetRef();
		NewCachedBinding.DataInterface = InDataInterface;
		NewCachedBinding.CachedBindings.SetNum(InNumVariables);
		return NewCachedBinding;
	};

	const UAnimNextDataInterface* HostDataInterface = InHost.GetDataInterface();
	if(HostDataInterface == nullptr)
	{
		return;
	}

	const UPropertyBag* PropertyBag = HostDataInterface->GetPublicVariableDefaults().GetPropertyBagStruct();
	if(PropertyBag == nullptr)
	{
		return;
	}

	for(const FAnimNextImplementedDataInterface& ImplementedInterface : HostDataInterface->GetImplementedInterfaces())
	{
		FCachedDataInterfaceBinding& CachedBinding = FindOrAddCachedInterfaceBindings(ImplementedInterface.DataInterface, ImplementedInterface.NumVariables);

		if (CachedBinding.CachedBindings.Num() != ImplementedInterface.NumVariables)
		{
			UE_LOGFMT(LogAnimation, Error, "UpdateCachedBindingsForHostHelper: Interface size mismatch: {CachedInterfaceName} {CachedSize} vs {ImplementedInterfaceName} {ImplementedSize}. Interface's values will not be updated at runtime.", CachedBinding.DataInterface->GetFName(), CachedBinding.CachedBindings.Num(), ImplementedInterface.DataInterface->GetFName(), ImplementedInterface.NumVariables);
			continue;
		}

		int32 VariableIndex = ImplementedInterface.VariableIndex;
		const int32 EndVariableIndex = ImplementedInterface.VariableIndex + ImplementedInterface.NumVariables;
		for(int32 InterfaceVariableIndex = 0; VariableIndex < EndVariableIndex && InterfaceVariableIndex < ImplementedInterface.NumVariables; ++VariableIndex, ++InterfaceVariableIndex)
		{
			const FPropertyBagPropertyDesc& Desc = PropertyBag->GetPropertyDescs()[VariableIndex];
			uint8* HostMemory = InHost.GetMemoryForVariable(VariableIndex, Desc.Name, Desc.CachedProperty);
			uint8* PrevMemory = CachedBinding.CachedBindings.IsValidIndex(InterfaceVariableIndex) 
				? CachedBinding.CachedBindings[InterfaceVariableIndex].Memory 
				: nullptr;
			HostMemory = HostMemory ? HostMemory : PrevMemory;

			CachedBinding.CachedBindings[InterfaceVariableIndex] = FCachedDataInterfaceBinding::FVariable(Desc.Name, InterfaceVariableIndex, Desc.CachedProperty, HostMemory);
		}
	}
}

void FAnimNextGraphInstance::BindPublicVariables(TArrayView<FStructView> InHostStructs, TConstArrayView<UE::AnimNext::IDataInterfaceHost*> InHosts)
{
	using namespace UE::AnimNext;

	const UAnimNextAnimationGraph* AnimationGraph = GetAnimationGraph();
	if (AnimationGraph == nullptr)
	{
		return;
	}

	ensure(CachedBindings.Num() == 0);

	{
		const int32 NumStructs = InHostStructs.Num();
		const int32 NumHosts = InHosts.Num();

		// stack allocating uninitialized arrays to contain FDataInterfaceStructAdapter(s)
		// Note this means we can end up calling FMemory_Alloca(0), which is somewhat implementation specific.
		// This is OK as we dont every iterate or use the memory in that case
		TArrayView<FDataInterfaceStructAdapter> Adapters((FDataInterfaceStructAdapter*)FMemory_Alloca(NumStructs * sizeof(FDataInterfaceStructAdapter)), NumStructs);
		TArrayView<IDataInterfaceHost*> Hosts((IDataInterfaceHost**)FMemory_Alloca((NumStructs + NumHosts) * sizeof(IDataInterfaceHost*)), NumStructs + NumHosts);

		// initializing the FDataInterfaceStructAdapter(s) with their respective HostStruct
		for (int32 StructIndex = 0; StructIndex < NumStructs; ++StructIndex)
		{
			FStructView& StructView = InHostStructs[StructIndex];
			Hosts[StructIndex] = new (&Adapters[StructIndex]) FDataInterfaceStructAdapter(AnimationGraph, StructView);
		}

		// Copy incoming hosts to provide a combined set to BindPublicVariables
		for (int32 HostIndex = 0; HostIndex < NumHosts; ++HostIndex)
		{
			Hosts[NumStructs + HostIndex] = InHosts[HostIndex];
		}

		// Inner binding 
		BindPublicVariables(Hosts);

		// uninitializing the FDataInterfaceStructAdapter(s)
		for (int32 StructIndex = 0; StructIndex < NumStructs; ++StructIndex)
		{
			Adapters[StructIndex].~FDataInterfaceStructAdapter();
		}

		// abandoning no longer initialized stack allocated arrays to their demise
	}
}

void FAnimNextGraphInstance::BindPublicVariables(TConstArrayView<UE::AnimNext::IDataInterfaceHost*> InHosts)
{
	using namespace UE::AnimNext;

	const UAnimNextAnimationGraph* AnimationGraph = GetAnimationGraph();
	if (AnimationGraph == nullptr)
	{
		return;
	}

	if(PublicVariablesState == EPublicVariablesState::Bound)
	{
		return;
	}

	const UPropertyBag* PropertyBag = Variables.GetPropertyBagStruct();
	if(PropertyBag == nullptr)
	{
		// Nothing to bind
		PublicVariablesState = EPublicVariablesState::None;
		return;
	}

	ensure(CachedBindings.Num() == 0);

	// First cache any bindings to this instance's host
	UpdateCachedBindingsForHost();

	// Next cache any supplied host interfaces 
	for(const IDataInterfaceHost* HostInterface : InHosts)
	{
		check(HostInterface);
		UpdateCachedBindingsForHost(*HostInterface);
	}

	// Bind to cached bindings we built above
	if(BindToCachedBindings())
	{
		// Re-initialize memory handles
		AnimationGraph->RigVM->InitializeInstance(ExtendedExecuteContext, /* bCopyMemory = */false);
	}

	PublicVariablesState = EPublicVariablesState::Bound;
}

void FAnimNextGraphInstance::UnbindPublicVariables()
{
	const UAnimNextAnimationGraph* AnimationGraph = GetAnimationGraph();
	if (AnimationGraph == nullptr)
	{
		return;
	}

	if(PublicVariablesState != EPublicVariablesState::Bound)
	{
		return;
	}

	// Reset external variable ptrs to point to internal public vars
	const int32 NumVariables = Variables.GetNumPropertiesInBag();
	TConstArrayView<FPropertyBagPropertyDesc> Descs = Variables.GetPropertyBagStruct()->GetPropertyDescs();
	uint8* BasePtr = Variables.GetMutableValue().GetMemory();
	for(int32 VariableIndex = 0; VariableIndex < NumVariables; ++VariableIndex)
	{
		ExtendedExecuteContext.ExternalVariableRuntimeData[VariableIndex].Memory = Descs[VariableIndex].CachedProperty->ContainerPtrToValuePtr<uint8>(BasePtr);
	}

	// Re-initialize memory handles
	AnimationGraph->RigVM->InitializeInstance(ExtendedExecuteContext, /* bCopyMemory = */false);

	CachedBindings.Reset();

	PublicVariablesState = EPublicVariablesState::Unbound;
}

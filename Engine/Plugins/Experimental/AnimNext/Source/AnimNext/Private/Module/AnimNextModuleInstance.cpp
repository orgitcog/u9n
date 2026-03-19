// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModuleInstance.h"

#if UE_ENABLE_DEBUG_DRAWING
#include "AnimNextDebugDraw.h"
#endif
#include "AnimNextPool.h"
#include "Engine/World.h"
#include "AnimNextStats.h"
#include "Async/TaskGraphInterfaces.h"
#include "SceneInterface.h"
#include "Algo/TopologicalSort.h"
#include "Logging/StructuredLog.h"
#include "Misc/EnumerateRange.h"
#include "Module/ModuleGuard.h"
#include "Module/RigUnit_AnimNextModuleEvents.h"
#include "Module/ProxyVariablesContext.h"
#include "UObject/UObjectIterator.h"
#include "Variables/IAnimNextVariableProxyHost.h"
#include "RewindDebugger/AnimNextTrace.h"

DEFINE_STAT(STAT_AnimNext_InitializeInstance);

FAnimNextModuleInstance::FAnimNextModuleInstance() = default;

FAnimNextModuleInstance::FAnimNextModuleInstance(
		UAnimNextModule* InModule,
		UObject* InObject,
		UE::AnimNext::TPool<FAnimNextModuleInstance>* InPool,
		IAnimNextVariableProxyHost* InProxyHost,
		EAnimNextModuleInitMethod InInitMethod)
	: Object(InObject)
	, Pool(InPool)
	, ProxyHost(InProxyHost)
	, InitState(EInitState::NotInitialized)
	, RunState(ERunState::NotInitialized)
	, InitMethod(InInitMethod)
{
	check(InModule);
	check(InObject);

	DataInterface = InModule;

#if UE_ENABLE_DEBUG_DRAWING
	if(Object && Object->GetWorld())
	{
		DebugDraw = MakeUnique<UE::AnimNext::Debug::FDebugDraw>(Object);
	}
#endif
}

FAnimNextModuleInstance::~FAnimNextModuleInstance()
{
	ResetBindingsAndInstanceData();

#if UE_ENABLE_DEBUG_DRAWING
	DebugDraw.Reset();
#endif

	Object = nullptr;
	DataInterface = nullptr;
	Handle.Reset();
}

namespace UE::AnimNext::Private
{

struct FImplementedModuleEvent
{
	UScriptStruct* Struct = nullptr;
	FModuleEventBindingFunction Binding;
	FName EventName;
	EModuleEventPhase Phase = EModuleEventPhase::Execute;
	ETickingGroup TickGroup = ETickingGroup::TG_PrePhysics;
	int32 SortOrder = 0;
	bool bUserEvent = false;
	bool bIsTask = false;
	bool bIsGameThreadTask = false;
};

static TArray<FImplementedModuleEvent> GImplementedModuleEvents;

// Gets information about the module events that are implemented by the supplied VM, sorted by execution order in the frame
static TConstArrayView<FImplementedModuleEvent> GetImplementedModuleEvents(const URigVM* VM)
{
	check(IsInGameThread());	// This function cannot be run concurrently because of static usage
	GImplementedModuleEvents.Reset();

	const FRigVMByteCode& ByteCode = VM->GetByteCode();
	const TArray<const FRigVMFunction*>& Functions = VM->GetFunctions();
	FRigVMInstructionArray Instructions = ByteCode.GetInstructions();
	for (int32 EntryIndex = 0; EntryIndex < ByteCode.NumEntries(); EntryIndex++)
	{
		const FRigVMByteCodeEntry& Entry = ByteCode.GetEntry(EntryIndex);
		const FRigVMInstruction& Instruction = Instructions[Entry.InstructionIndex];
		const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
		const FRigVMFunction* Function = Functions[Op.FunctionIndex];
		check(Function != nullptr);

		if (Function->Struct->IsChildOf(FRigUnit_AnimNextModuleEventBase::StaticStruct()))
		{
			TInstancedStruct<FRigUnit_AnimNextModuleEventBase> StructInstance;
			StructInstance.InitializeAsScriptStruct(Function->Struct);
			const FRigUnit_AnimNextModuleEventBase& Event = StructInstance.Get();
			FImplementedModuleEvent& NewEvent = GImplementedModuleEvents.AddDefaulted_GetRef();
			NewEvent.Struct = Function->Struct;
			NewEvent.Binding = Event.GetBindingFunction();
			NewEvent.EventName = Event.GetEventName();
			NewEvent.Phase = Event.GetEventPhase();
			NewEvent.TickGroup = Event.GetTickGroup();
			NewEvent.SortOrder = Event.GetSortOrder();
			NewEvent.bUserEvent = Event.IsUserEvent();
			NewEvent.bIsTask = Event.IsTask();
			NewEvent.bIsGameThreadTask = Event.IsGameThreadTask();

			// User events can override their event name etc. via parameters 
			if (Function->Struct->IsChildOf(FRigUnit_AnimNextUserEvent::StaticStruct()))
			{
				// Pull the values out of the literal memory
				FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Instruction);
				check(Function->ArgumentNames.Num() == Operands.Num());
				int32 NumOperands = Operands.Num();
				URigVM* NonConstVM = const_cast<URigVM*>(VM);
				for (int32 OperandIndex = 0; OperandIndex < NumOperands; ++OperandIndex)
				{
					const FRigVMOperand& Operand = Operands[OperandIndex];
					FName OperandName = Function->ArgumentNames[OperandIndex];
					if (OperandName == GET_MEMBER_NAME_CHECKED(FRigUnit_AnimNextUserEvent, Name))
					{
						check(Operand.GetMemoryType() == ERigVMMemoryType::Literal);
						NewEvent.EventName = *NonConstVM->LiteralMemoryStorage.GetData<FName>(Operand.GetRegisterIndex());
					}
					else if (OperandName == GET_MEMBER_NAME_CHECKED(FRigUnit_AnimNextUserEvent, SortOrder))
					{
						check(Operand.GetMemoryType() == ERigVMMemoryType::Literal);
						NewEvent.SortOrder = *NonConstVM->LiteralMemoryStorage.GetData<int32>(Operand.GetRegisterIndex());
					}
				}
			}
		}
	}

	Algo::Sort(GImplementedModuleEvents, [](const FImplementedModuleEvent& InA, const FImplementedModuleEvent& InB)
	{
		if (InA.Phase != InB.Phase)
		{
			return InA.Phase < InB.Phase;
		}
		else if (InA.TickGroup != InB.TickGroup)
		{
			return InA.TickGroup < InB.TickGroup;
		}
		else if (InA.SortOrder != InB.SortOrder)
		{
			return InA.SortOrder < InB.SortOrder;
		}

		// Tie-break sorting on event name for determinism
		return InA.EventName.Compare(InB.EventName) < 0;
	});
	
	return GImplementedModuleEvents;
}

}

void FAnimNextModuleInstance::Initialize()
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_InitializeInstance);
	
	using namespace UE::AnimNext;

	check(IsInGameThread());

	FModuleWriteGuard Guard(this);

	check(Object)
	const UAnimNextModule* Module = GetModule();
	check(Module);
	check(Handle.IsValid());

	UWorld* World = Object->GetWorld();
	if(World)
	{
		WorldType = World->WorldType;
	}

	// Get all the module events from the VM entry points, sorted by ordering in the frame
	URigVM* VM = Module->RigVM;
	TConstArrayView<Private::FImplementedModuleEvent> ImplementedModuleEvents = Private::GetImplementedModuleEvents(VM);

	// Setup tick function graph using module events
	if (ImplementedModuleEvents.Num() > 0)
	{
		TransitionToInitState(EInitState::CreatingTasks);

		// Allocate tick functions
		TickFunctions.Reserve(ImplementedModuleEvents.Num());
		bool bFoundFirstUserEvent = false;
		FModuleEventTickFunction* PrevTickFunction = nullptr;
		for (int32 EventIndex = 0; EventIndex < ImplementedModuleEvents.Num(); EventIndex++)
		{
			const Private::FImplementedModuleEvent& ModuleEvent = ImplementedModuleEvents[EventIndex];
			if (!ModuleEvent.bIsTask)
			{
				continue;
			}

			FModuleEventTickFunction& TickFunction = TickFunctions.AddDefaulted_GetRef();
			TickFunction.bRunOnAnyThread = !ModuleEvent.bIsGameThreadTask;
			TickFunction.ModuleInstance = this;
			TickFunction.EventName = ModuleEvent.EventName;
			TickFunction.TickGroup = ModuleEvent.TickGroup;
			TickFunction.bUserEvent = ModuleEvent.bUserEvent;

			// Perform custom setup
			FTickFunctionBindingContext Context(*this, Object, World);
			ModuleEvent.Binding(Context, TickFunction);

			// Establish linear dependency chain
			if (PrevTickFunction != nullptr)
			{
				TickFunction.AddPrerequisite(Object, *PrevTickFunction);
			}
			PrevTickFunction = &TickFunction;

			// Set up dependencies, if any
			for (const TInstancedStruct<FRigVMTrait_ModuleEventDependency>& DependencyInstance : Module->Dependencies)
			{
				const FRigVMTrait_ModuleEventDependency* Dependency = DependencyInstance.GetPtr<FRigVMTrait_ModuleEventDependency>();
				if (Dependency != nullptr && Dependency->EventName == ModuleEvent.EventName)
				{
					FModuleDependencyContext ModuleDependencyContext(Object, TickFunction);
					Dependency->OnAddDependency(ModuleDependencyContext);
				}
			}

			if (ModuleEvent.bUserEvent && !bFoundFirstUserEvent)
			{
				TickFunction.bFirstUserEvent = true;
				bFoundFirstUserEvent = true;

				// Set this first user event to run the bindings event, if it exists
				auto IsExecuteBindingsEvent = [](const Private::FImplementedModuleEvent& InEvent)
				{
					return InEvent.EventName == FRigUnit_AnimNextExecuteBindings_WT::EventName;
				};

				TickFunction.bRunBindingsEvent = ImplementedModuleEvents.ContainsByPredicate(IsExecuteBindingsEvent);
			}
		}

		// Find the last user event - 'end' logic will be called from here
		for (int32 EventIndex = TickFunctions.Num() - 1; EventIndex >= 0; EventIndex--)
		{
			FModuleEventTickFunction& TickFunction = TickFunctions[EventIndex];
			if (TickFunction.bUserEvent)
			{
				TickFunction.bLastUserEvent = true;
				break;
			}
		}

		TransitionToInitState(EInitState::BindingTasks);

		// Register our tick functions
		if(World)
		{
			ULevel* Level = World->PersistentLevel;
			for (FModuleEventTickFunction& TickFunction : TickFunctions)
			{
				TickFunction.RegisterTickFunction(Level);
			}
		}

		TransitionToInitState(EInitState::SetupVariables);

		// TODO: code in EInitState::SetupVariables phase below can probably move to FModuleEventTickFunction::Initialize

		// Initialize variables
		const int32 NumVariables = Module->VariableDefaults.GetNumPropertiesInBag();
#if WITH_EDITOR
		if(bIsRecreatingOnCompile)
		{
			Variables.MigrateToNewBagInstance(Module->VariableDefaults);
		}
		else
#endif
		{
			Variables = Module->VariableDefaults;
		}

		if(Module->GetPublicVariableDefaults().GetPropertyBagStruct())
		{
			PublicVariablesProxy.Data = Module->GetPublicVariableDefaults();
			TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
			PublicVariablesProxy.DirtyFlags.SetNum(ProxyDescs.Num(), false);
		}

		// Initialize the RigVM context
		ExtendedExecuteContext = Module->GetRigVMExtendedExecuteContext();

		if(NumVariables > 0)
		{
			// Setup external variables memory ptrs manually as we dont follow the pattern of owning multiple URigVMHosts like control rig.
			// InitializeVM() is called, but only sets up handles for the defaults in the module, not for an instance
			TArray<FRigVMExternalVariableRuntimeData> ExternalVariableRuntimeData;
			ExternalVariableRuntimeData.Reserve(NumVariables);
			TConstArrayView<FPropertyBagPropertyDesc> Descs = Variables.GetPropertyBagStruct()->GetPropertyDescs();
			uint8* BasePtr = Variables.GetMutableValue().GetMemory();
			for(int32 VariableIndex = 0; VariableIndex < NumVariables; ++VariableIndex)
			{
				ExternalVariableRuntimeData.Emplace(Descs[VariableIndex].CachedProperty->ContainerPtrToValuePtr<uint8>(BasePtr));
			}
			ExtendedExecuteContext.ExternalVariableRuntimeData = MoveTemp(ExternalVariableRuntimeData);
		}

		// Now initialize the 'instance', cache memory handles etc. in the context
		VM->InitializeInstance(ExtendedExecuteContext);

		// Allocate compiled-in module components
		for(UScriptStruct* ComponentStruct : Module->RequiredComponents)
		{
			const FName ComponentName = ComponentStruct->GetFName();
			const int32 ComponentNameHash = GetTypeHash(ComponentName);

			TInstancedStruct<FAnimNextModuleInstanceComponent> Component(ComponentStruct);
			Component.GetMutable<FAnimNextModuleInstanceComponent>().Initialize(*this);
			AddComponentInternal(ComponentNameHash, ComponentName, MoveTemp(Component));
		}

		TransitionToInitState(EInitState::PendingInitializeEvent);
		TransitionToRunState(ERunState::Running);

		// Just pause now if we arent needing an initial update
		if(InitMethod == EAnimNextModuleInitMethod::None)
		{
			Enable(false);
		}
#if WITH_EDITOR
		else if(World)
		{
			// In editor worlds we run a linearized 'initial tick' to ensure we generate an initial output pose, as these worlds dont always tick
			if( World->WorldType == EWorldType::Editor ||
				World->WorldType == EWorldType::EditorPreview)
			{
				FModuleEventTickFunction::InitializeAndRunModule(*this);
			}
		}
#endif
	}
}

void FAnimNextModuleInstance::Uninitialize()
{
	for(TPair<FName, TInstancedStruct<FAnimNextModuleInstanceComponent>>& ComponentPair : ComponentMap)
	{
		ComponentPair.Value.GetMutable<FAnimNextModuleInstanceComponent>().Uninitialize();
	}
}

void FAnimNextModuleInstance::RemoveAllTickDependencies()
{
	using namespace UE::AnimNext;

	check(IsInGameThread());

	for (FModuleEventTickFunction& TickFunction : TickFunctions)
	{
		TickFunction.RemoveAllExternalSubsequents();
	}

	if(Pool)
	{
		TArray<FPrerequisiteReference, TInlineAllocator<8>> PrerequisiteRefsCopy;
		PrerequisiteRefsCopy.Append(PrerequisiteRefs);
		for (const FPrerequisiteReference& PrerequisiteHandle : PrerequisiteRefsCopy)
		{
			if(FAnimNextModuleInstance* PrerequisiteInstance = Pool->TryGet(PrerequisiteHandle.Handle))
			{
				RemovePrerequisite(*PrerequisiteInstance);
			}
		}
	}
}

void FAnimNextModuleInstance::ResetBindingsAndInstanceData()
{
	using namespace UE::AnimNext;

	check(IsInGameThread());

	FModuleWriteGuard Guard(this);

	TransitionToInitState(EInitState::NotInitialized);
	TransitionToRunState(ERunState::NotInitialized);

	for (FModuleEventTickFunction& TickFunction : TickFunctions)
	{
		// We should have released all external dependencies by now via RemoveAllTickDependencies
		check(TickFunction.ExternalSubsequents.Num() == 0);
		TickFunction.UnRegisterTickFunction();
	}

	TickFunctions.Reset();

	ExtendedExecuteContext.Reset();

#if WITH_EDITOR
	if(!bIsRecreatingOnCompile)
#endif
	{
		Variables.Reset();
	}
}

void FAnimNextModuleInstance::QueueInputTraitEvent(FAnimNextTraitEventPtr Event)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	InputEventList.Push(MoveTemp(Event));
}

void FAnimNextModuleInstance::QueueOutputTraitEvent(FAnimNextTraitEventPtr Event)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	OutputEventList.Push(MoveTemp(Event));
}

bool FAnimNextModuleInstance::IsEnabled() const
{
	using namespace UE::AnimNext;

	check(IsInGameThread());

	return RunState == ERunState::Running;
}

void FAnimNextModuleInstance::Enable(bool bInEnabled)
{
	using namespace UE::AnimNext;

	check(IsInGameThread());

	FModuleWriteGuard Guard(this);

	if(RunState == ERunState::Paused || RunState == ERunState::Running)
	{
		for (FModuleEventTickFunction& TickFunction : TickFunctions)
		{
			TickFunction.SetTickFunctionEnable(bInEnabled);
		}

		TransitionToRunState(bInEnabled ? ERunState::Running : ERunState::Paused);
	}
}

void FAnimNextModuleInstance::TransitionToInitState(EInitState InNewState)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	switch(InNewState)
	{
	case EInitState::NotInitialized:
		check(InitState == EInitState::NotInitialized || InitState == EInitState::PendingInitializeEvent || InitState == EInitState::SetupVariables || InitState == EInitState::FirstUpdate || InitState == EInitState::Initialized);
		break;
	case EInitState::CreatingTasks:
		check(InitState == EInitState::NotInitialized);
		break;
	case EInitState::BindingTasks:
		check(InitState == EInitState::CreatingTasks);
		break;
	case EInitState::SetupVariables:
		check(InitState == EInitState::BindingTasks);
		break;
	case EInitState::PendingInitializeEvent:
		check(InitState == EInitState::SetupVariables);
		break;
	case EInitState::FirstUpdate:
		check(InitState == EInitState::PendingInitializeEvent);
		break;
	case EInitState::Initialized:
		check(InitState == EInitState::FirstUpdate);
		break;
	default:
		checkNoEntry();
	}

	InitState = InNewState;
}

void FAnimNextModuleInstance::TransitionToRunState(ERunState InNewState)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	switch(InNewState)
	{
	case ERunState::Running:
		check(RunState == ERunState::NotInitialized || RunState == ERunState::Paused || RunState == ERunState::Running);
		break;
	case ERunState::Paused:
		check(RunState == ERunState::Paused || RunState == ERunState::Running);
		break;
	case ERunState::NotInitialized:
		check(RunState == ERunState::NotInitialized || RunState == ERunState::Paused || RunState == ERunState::Running);
		break;
	default:
		checkNoEntry();
	}

	RunState = InNewState;
}

void FAnimNextModuleInstance::CopyProxyVariables()
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	// TODO: we can avoid the copies here by adopting a scheme where we:
	// - Hold double-buffered memory handles
	// - Update the memory handle's ptr to the currently-written double-buffered public variable on write
	// - Swap the memory handles in ExtendedExecuteContext here
	// 
//	if(IAnimNextVariableProxyHost* ProxyHost = Cast<IAnimNextVariableProxyHost>(Object))
	if(ProxyHost)
	{
		// Flip the proxy
		ProxyHost->FlipPublicVariablesProxy(UE::AnimNext::FProxyVariablesContext(*this));

		if(PublicVariablesProxy.bIsDirty)
		{
			// Copy dirty properties
			TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = Variables.GetPropertyBagStruct()->GetPropertyDescs();
			TConstArrayView<FPropertyBagPropertyDesc> PublicProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
			const uint8* SourceContainerPtr = PublicVariablesProxy.Data.GetValue().GetMemory();
			uint8* TargetContainerPtr = Variables.GetMutableValue().GetMemory();
			for (TConstSetBitIterator<> It(PublicVariablesProxy.DirtyFlags); It; ++It)
			{
				const int32 Index = It.GetIndex();
				const FProperty* SourceProperty = PublicProxyDescs[Index].CachedProperty;
				const FProperty* TargetProperty = ProxyDescs[Index].CachedProperty;
				checkSlow(SourceProperty->GetClass() == TargetProperty->GetClass());
				ProxyDescs[Index].CachedProperty->CopyCompleteValue_InContainer(TargetContainerPtr, SourceContainerPtr);
			}

			// Reset dirty flags
			PublicVariablesProxy.DirtyFlags.SetRange(0, PublicVariablesProxy.DirtyFlags.Num(), false);
			PublicVariablesProxy.bIsDirty = false;
		}
	}

#if ANIMNEXT_TRACE_ENABLED
	bTracedThisFrame = false;
#endif
}

#if ANIMNEXT_TRACE_ENABLED
void FAnimNextModuleInstance::Trace()
{
	if (!bTracedThisFrame)
	{
		TRACE_ANIMNEXT_VARIABLES(this, Object);
		bTracedThisFrame = true;
	}
}
#endif

const UAnimNextModule* FAnimNextModuleInstance::GetModule() const
{
	return CastChecked<UAnimNextModule>(DataInterface);
}

#if WITH_EDITOR
void FAnimNextModuleInstance::OnModuleCompiled()
{
	using namespace UE::AnimNext;

	FGuardValue_Bitfield(bIsRecreatingOnCompile, true);

	ResetBindingsAndInstanceData();
	Initialize();
}
#endif

const FAnimNextModuleInstanceComponent* FAnimNextModuleInstance::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	if (const TInstancedStruct<FAnimNextModuleInstanceComponent>* InstancedComponent = ComponentMap.FindByHash(ComponentNameHash, ComponentName))
	{
		return InstancedComponent->GetPtr();
	}

	return nullptr;
}
FAnimNextModuleInstanceComponent* FAnimNextModuleInstance::TryGetComponent(int32 ComponentNameHash, FName ComponentName) 
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	if (TInstancedStruct<FAnimNextModuleInstanceComponent>* InstancedComponent = ComponentMap.FindByHash(ComponentNameHash, ComponentName))
	{
		return InstancedComponent->GetMutablePtr();
	}

	return nullptr;
}

FAnimNextModuleInstanceComponent& FAnimNextModuleInstance::AddComponentInternal(int32 ComponentNameHash, FName ComponentName, TInstancedStruct<FAnimNextModuleInstanceComponent>&& Component)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	TInstancedStruct<FAnimNextModuleInstanceComponent>& InstancedComponent = ComponentMap.AddByHash(ComponentNameHash, ComponentName, Component);
	
	FAnimNextModuleInstanceComponent* ModuleInstanceComponent = InstancedComponent.GetMutablePtr();
	check(ModuleInstanceComponent != nullptr);

	return *ModuleInstanceComponent;
}

#if UE_ENABLE_DEBUG_DRAWING
FRigVMDrawInterface* FAnimNextModuleInstance::GetDebugDrawInterface()
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	return DebugDraw ? &DebugDraw->DrawInterface : nullptr;
}

void FAnimNextModuleInstance::ShowDebugDrawing(bool bInShowDebugDrawing)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	if(DebugDraw)
	{
		DebugDraw->SetEnabled(bInShowDebugDrawing);
	}
}
#endif

void FAnimNextModuleInstance::RunTaskOnGameThread(TUniqueFunction<void(void)>&& InFunction)
{
	if(IsInGameThread())
	{
		InFunction();
	}
	else
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(InFunction), TStatId(), nullptr, ENamedThreads::GameThread);
	}
}

UE::AnimNext::FModuleEventTickFunction* FAnimNextModuleInstance::FindTickFunctionByName(FName InEventName)
{
	using namespace UE::AnimNext;

	return const_cast<FModuleEventTickFunction*>(const_cast<const FAnimNextModuleInstance*>(this)->FindTickFunctionByName(InEventName));
}

const UE::AnimNext::FModuleEventTickFunction* FAnimNextModuleInstance::FindTickFunctionByName(FName InEventName) const
{
	using namespace UE::AnimNext;

	for(const FModuleEventTickFunction& TickFunction : TickFunctions)
	{
		if (TickFunction.EventName == InEventName)
		{
			return &TickFunction;
		}
	}
	return nullptr;
}

void FAnimNextModuleInstance::EndExecution(float InDeltaTime)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	// Give the module a chance to handle events
	RaiseTraitEvents(OutputEventList);

	// Give each component a chance to finalize execution
	for(auto It = ComponentMap.CreateIterator(); It; ++It)
	{
		FAnimNextModuleInstanceComponent& Component = It->Value.GetMutable<FAnimNextModuleInstanceComponent>();
		Component.OnEndExecution(InDeltaTime);
	}
}

void FAnimNextModuleInstance::RaiseTraitEvents(const UE::AnimNext::FTraitEventList& EventList)
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	for(auto It = ComponentMap.CreateIterator(); It; ++It)
	{
		FAnimNextModuleInstanceComponent& Component = It->Value.GetMutable<FAnimNextModuleInstanceComponent>();

		// Event handlers can raise events and as such the list may change while we iterate
		// However, if an event is added while we iterate, we will not visit it
		const int32 NumEvents = EventList.Num();
		for (int32 EventIndex = 0; EventIndex < NumEvents; ++EventIndex)
		{
			const FAnimNextTraitEventPtr Event = EventList[EventIndex];
			if (Event->IsValid())
			{
				Component.OnTraitEvent(*Event);
			}
		}
	}
}

void FAnimNextModuleInstance::AddPrerequisite(FAnimNextModuleInstance& InPrerequisiteInstance)
{
	check(IsInGameThread());
	check(&InPrerequisiteInstance != this);

	UE::AnimNext::FModuleWriteGuard Guard(this);

	FPrerequisiteReference* FoundReference = PrerequisiteRefs.FindByPredicate([&InPrerequisiteInstance](const FPrerequisiteReference& InPrerequisiteRef)
	{
		return InPrerequisiteRef.Handle == InPrerequisiteInstance.Handle;
	});

	if(FoundReference)
	{
		FoundReference->ReferenceCount++;
	}
	else
	{
		// Ensure all of our tick functions execute after the prerequisite's last tick function
		TickFunctions.Last().AddPrerequisite(InPrerequisiteInstance.Object, InPrerequisiteInstance.TickFunctions.Last());
		for(UE::AnimNext::FModuleEventTickFunction& TickFunction : TickFunctions)
		{
			TickFunction.AddPrerequisite(InPrerequisiteInstance.Object, InPrerequisiteInstance.TickFunctions.Last());
		}

		PrerequisiteRefs.Add({ InPrerequisiteInstance.Handle, 1});
		InPrerequisiteInstance.SubsequentRefs.Add(GetHandle());
	}
}

void FAnimNextModuleInstance::RemovePrerequisite(FAnimNextModuleInstance& InPrerequisiteInstance)
{
	check(IsInGameThread());
	check(&InPrerequisiteInstance != this);

	UE::AnimNext::FModuleWriteGuard Guard(this);

	int32 FoundIndex = PrerequisiteRefs.IndexOfByPredicate([&InPrerequisiteInstance](const FPrerequisiteReference& InPrerequisiteRef)
	{
		return InPrerequisiteRef.Handle == InPrerequisiteInstance.Handle;
	});

	ensureAlways(FoundIndex != INDEX_NONE);	// Shouldnt really be calling this if we dont have a prerequisite already
	if(FoundIndex != INDEX_NONE)
	{
		FPrerequisiteReference& FoundReference = PrerequisiteRefs[FoundIndex]; 
		FoundReference.ReferenceCount--;
		if(FoundReference.ReferenceCount == 0)
		{
			// Remove dependency on the prerequisite's last tick function
			TickFunctions.Last().RemovePrerequisite(InPrerequisiteInstance.Object, InPrerequisiteInstance.TickFunctions.Last());
			for(UE::AnimNext::FModuleEventTickFunction& TickFunction : TickFunctions)
			{
				TickFunction.RemovePrerequisite(InPrerequisiteInstance.Object, InPrerequisiteInstance.TickFunctions.Last());
			}

			PrerequisiteRefs.RemoveAtSwap(FoundIndex, EAllowShrinking::No);

			const int32 CountRemoved = InPrerequisiteInstance.SubsequentRefs.RemoveSwap(GetHandle(), EAllowShrinking::No);
			check(CountRemoved == 1);
		}
	}
}

bool FAnimNextModuleInstance::IsPrerequisite(const FAnimNextModuleInstance& InPrerequisiteInstance) const
{
	UE::AnimNext::FModuleWriteGuard Guard(this);
	return PrerequisiteRefs.ContainsByPredicate([&InPrerequisiteInstance](const FPrerequisiteReference& InPrerequisiteRef)
	{
		return InPrerequisiteRef.Handle == InPrerequisiteInstance.Handle;
	});
}

void FAnimNextModuleInstance::ForEachPrerequisite(TFunctionRef<void(FAnimNextModuleInstance& InPrerequisiteInstance)> InFunction) const
{
	UE::AnimNext::FModuleWriteGuard Guard(this);

	if(Pool == nullptr)
	{
		return;
	}

	for(const FPrerequisiteReference& PrerequisiteHandle : PrerequisiteRefs)
	{
		FAnimNextModuleInstance* PrerequisiteInstance = Pool->TryGet(PrerequisiteHandle.Handle);
		if(PrerequisiteInstance == nullptr)
		{
			continue;
		}

		InFunction(*PrerequisiteInstance);
	}
}

void FAnimNextModuleInstance::RunRigVMEvent(FName InEventName, float InDeltaTime)
{
	UE::AnimNext::FModuleWriteGuard PrerequisiteGuard(this);

	URigVM* VM = GetModule()->RigVM;
	if(VM == nullptr)
	{
		return;
	}

	if(!VM->ContainsEntry(InEventName))
	{
		return;
	}

	check(ExtendedExecuteContext.VMHash == VM->GetVMHash());

	FAnimNextExecuteContext& AnimNextContext = ExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();

	// RigVM setup
	AnimNextContext.SetDeltaTime(InDeltaTime);
	AnimNextContext.SetOwningObject(Object);

#if UE_ENABLE_DEBUG_DRAWING
	AnimNextContext.SetDrawInterface(GetDebugDrawInterface());
#endif

	// Insert our context data for the scope of execution
	FAnimNextModuleContextData ContextData(this);
	UE::AnimNext::FScopedExecuteContextData ContextDataScope(AnimNextContext, ContextData);

	// Run the VM for this event
	VM->ExecuteVM(ExtendedExecuteContext, InEventName);
}


TArrayView<UE::AnimNext::FModuleEventTickFunction> FAnimNextModuleInstance::GetTickFunctions()
{
	UE::AnimNext::FModuleWriteGuard PrerequisiteGuard(this);

	return TickFunctions;
}

UE::AnimNext::FModuleEventTickFunction* FAnimNextModuleInstance::FindFirstUserTickFunction()
{
	UE::AnimNext::FModuleWriteGuard PrerequisiteGuard(this);

	UE::AnimNext::FModuleEventTickFunction* FoundTickFunction = TickFunctions.FindByPredicate([](const UE::AnimNext::FModuleEventTickFunction& InTickFunction)
	{
		return InTickFunction.bFirstUserEvent;
	});

	return FoundTickFunction;
}

void FAnimNextModuleInstance::QueueTask(FName InEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation)
{
	using namespace UE::AnimNext;

	FModuleEventTickFunction* FoundTickFunction = nullptr;
	if(TickFunctions.Num() > 0)
	{
		if(!InEventName.IsNone())
		{
			// Match according to event desc
			FoundTickFunction = TickFunctions.FindByPredicate([InEventName](const FModuleEventTickFunction& InTickFunction)
			{
				return InTickFunction.EventName == InEventName;
			});
		}

		if (FoundTickFunction == nullptr)
		{
			// Fall back to first user function
			FoundTickFunction = TickFunctions.FindByPredicate([](const UE::AnimNext::FModuleEventTickFunction& InTickFunction)
			{
				return InTickFunction.bFirstUserEvent;
			});
		}
	}

	TSpscQueue<TUniqueFunction<void(const FModuleTaskContext&)>>* Queue = nullptr;
	if (FoundTickFunction)
	{
		switch (InLocation)
		{
		case ETaskRunLocation::Before:
			Queue = &FoundTickFunction->PreExecuteTasks;
			break;
		case ETaskRunLocation::After:
			Queue = &FoundTickFunction->PostExecuteTasks;
			break;
		}
	}

	if (Queue)
	{
		Queue->Enqueue(MoveTemp(InTaskFunction));
	}
	else
	{
		UE_LOGFMT(LogAnimation, Warning, "QueueTask: Could not find event '{EventName}' in module '{ModuleName}'", InEventName, GetDataInterfaceName());
	}
}

void FAnimNextModuleInstance::QueueTaskOnOtherModule(const UE::AnimNext::FModuleHandle InOtherModuleHandle, const FName InEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation)
{
	if (Pool == nullptr)
	{
		return;
	}

	FAnimNextModuleInstance* OtherModuleInstance = Pool->TryGet(InOtherModuleHandle);
	if (OtherModuleInstance == nullptr)
	{
		return;
	}

	OtherModuleInstance->QueueTask(InEventName, MoveTemp(InTaskFunction), InLocation);
}
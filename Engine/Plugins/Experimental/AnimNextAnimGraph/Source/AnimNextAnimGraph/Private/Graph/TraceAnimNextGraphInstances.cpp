// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceAnimNextGraphInstances.h"
#include "Graph/AnimNextGraphInstance.h"
#include "Module/AnimNextModuleInstance.h"
#include "TraitCore/ExecutionContext.h"
#include "TraitCore/NodeInstance.h"
#include "TraitInterfaces/IHierarchy.h"
#include "EvaluationVM/EvaluationProgram.h"
#include "EvaluationVM/SerializableEvaluationProgram.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if ANIMNEXT_TRACE_ENABLED

UE_TRACE_EVENT_BEGIN(AnimNextAnimGraph, EvaluationProgram)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(double, RecordingTime)
	UE_TRACE_EVENT_FIELD(uint64, OuterObjectId)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint8[], ProgramData)
UE_TRACE_EVENT_END()

namespace UE::AnimNext
{
	void TraceGraphInstances(const FAnimNextGraphInstance& RootGraph)
	{
		bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimNextChannel);
    	if (!bChannelEnabled)
    	{
    		return;
    	}

		UObject* OuterObject = RootGraph.GetModuleInstance() ? RootGraph.GetModuleInstance()->GetObject() : nullptr;

		FExecutionContext ExecutionContext;
		TArray<FWeakTraitPtr, TInlineAllocator<8, TMemStackAllocator<>>> Traits;
		TSet<uint64> TracedInstances;
		
		FMemStack& MemStack = FMemStack::Get();
		FMemMark Mark(MemStack);
		
		ExecutionContext.BindTo(RootGraph.GetGraphRootPtr());

		Traits.Push(RootGraph.GetGraphRootPtr());

		TTraitBinding<IHierarchy> HierarchyTrait;

		while (!Traits.IsEmpty())
		{
			FWeakTraitPtr Trait = Traits.Pop();
			if (Trait.IsValid())
			{
				FAnimNextGraphInstance& Graph = Trait.GetNodeInstance()->GetOwner();
				if (!TracedInstances.Contains(Graph.GetUniqueId()))
				{
					TRACE_ANIMNEXT_VARIABLES(&Graph, Graph.GetModuleInstance() ? Graph.GetModuleInstance()->GetObject() : nullptr);
				}

				FTraitStackBinding TraitStack;
				ExecutionContext.GetStack(Trait, TraitStack);
				
				if (TraitStack.GetInterface(HierarchyTrait))
				{
					IHierarchy::GetStackChildren(ExecutionContext, TraitStack, Traits);
				}
			}
		}
	}

	void TraceEvaluationProgram(const FEvaluationProgram& Program, const FAnimNextGraphInstance& RootGraph)
	{
		bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimNextChannel);
		if (!bChannelEnabled)
		{
			return;
		}

		uint64 OuterObjectId = 0;
		if (FAnimNextModuleInstance* ModuleInstance = RootGraph.GetModuleInstance())
		{
			if (const UObject* OuterObject = ModuleInstance->GetObject())
			{
				OuterObjectId = FObjectTrace::GetObjectId(OuterObject);
			}
		}
		
		uint64 InstanceId = RootGraph.GetUniqueId();
		
		double RecordingTime = 0;
		if (RootGraph.GetModuleInstance())
		{
			if (UObject* OuterObject = RootGraph.GetModuleInstance()->GetObject())
			{
				if (CANNOT_TRACE_OBJECT(OuterObject))
				{
					return;
				}
				RecordingTime = FObjectTrace::GetWorldElapsedTime(OuterObject->GetWorld());
			}
		}

		TArray<uint8> ArchiveData;
		FMemoryWriter WriterArchive(ArchiveData);
		FObjectAndNameAsStringProxyArchive Archive(WriterArchive, /*bInLoadIfFindFails*/true);
		
		static const FSerializableEvaluationProgram Defaults;
		FSerializableEvaluationProgram SerializableProgram(Program);
		FSerializableEvaluationProgram::StaticStruct()->SerializeItem(Archive, &SerializableProgram, &Defaults);
		
		UE_TRACE_LOG(AnimNextAnimGraph, EvaluationProgram, AnimNextChannel)
				<< EvaluationProgram.Cycle(FPlatformTime::Cycles64())
				<< EvaluationProgram.OuterObjectId(OuterObjectId)
				<< EvaluationProgram.RecordingTime(RecordingTime)
				<< EvaluationProgram.InstanceId(InstanceId)
				<< EvaluationProgram.ProgramData(ArchiveData.GetData(), ArchiveData.Num());
	}
}
#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Injection/AnimNextModuleInjectionComponent.h"

#include "InjectionEvents.h"
#include "Graph/AnimNextAnimGraph.h"
#include "Graph/AnimNextGraphInstance.h"
#include "Logging/StructuredLog.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/ModuleTaskContext.h"


void FAnimNextModuleInjectionComponent::OnInitialize()
{
	FAnimNextModuleInstance& ModuleInstance = GetModuleInstance();
	InjectionInfo = UE::AnimNext::FInjectionInfo(ModuleInstance);
	
	// Register re-injection for each (user) tick function
	for(UE::AnimNext::FModuleEventTickFunction& TickFunction : ModuleInstance.GetTickFunctions())
	{
		if(TickFunction.bUserEvent)
		{
			TickFunction.OnPreModuleEvent.AddStatic(&FAnimNextModuleInjectionComponent::OnReapplyInjection);
		}
	}
}

void FAnimNextModuleInjectionComponent::OnTraitEvent(FAnimNextTraitEvent& Event)
{
	if(UE::AnimNext::FInjection_InjectEvent* InjectionEvent = Event.AsType<UE::AnimNext::FInjection_InjectEvent>())
	{
		OnInjectionEvent(*InjectionEvent);
	}
	else if(UE::AnimNext::FInjection_UninjectEvent* UninjectionEvent = Event.AsType<UE::AnimNext::FInjection_UninjectEvent>())
	{
		OnUninjectionEvent(*UninjectionEvent);
	}
}

void FAnimNextModuleInjectionComponent::OnInjectionEvent(UE::AnimNext::FInjection_InjectEvent& InEvent)
{
	using namespace UE::AnimNext;

	if(InEvent.IsHandled())
	{
		return;
	}

	const FInjectionRequestArgs& RequestArgs = InEvent.Request->GetArgs();

	FName FoundName = NAME_None;
	TStructView<FAnimNextAnimGraph> InjectableGraph = InjectionInfo.FindInjectableGraphInstance(RequestArgs.Site, FoundName);
	if(!InjectableGraph.IsValid())
	{
		UE_LOGFMT(LogAnimation, Warning, "Could not find injection site {SiteName} for injection request", RequestArgs.Site.DesiredSiteName);
		InEvent.MarkConsumed();
		return;
	}

	// Correct the name found above as we may have targeted NAME_None (any)
	if(RequestArgs.Site.DesiredSiteName != FoundName)
	{
		InEvent.Request->GetMutableArgs().Site = FInjectionSite(FoundName);
	}

	// Mark as handled so any additional trait events dont get processed at the module level
	InEvent.MarkHandled();

	// Store request as it will need to be re-applied each frame to ensure that bindings do not override it
	FInjectionRecord& InjectionRecord = CurrentRequests.FindOrAdd(InEvent.Request->GetArgs().Site.DesiredSiteName);

	FAnimNextAnimGraph& Graph = InjectableGraph.Get<FAnimNextAnimGraph>();
	switch(RequestArgs.Type)
	{
	case EAnimNextInjectionType::InjectObject:
		// Bump serial number to identify this injection routing
		ensureAlways(RequestArgs.Object != nullptr);
		Graph.InjectionData.InjectionSerialNumber = IncrementSerialNumber();
		InEvent.SerialNumber = Graph.InjectionData.InjectionSerialNumber;
		InjectionRecord.SerialNumber = Graph.InjectionData.InjectionSerialNumber;
		InjectionRecord.GraphRequest = InEvent.Request;

		// Note we dont consume here, as we want the event to forward to the injection site trait
		break;
	case EAnimNextInjectionType::EvaluationModifier:
		// We dont increment serial number when applying evaluation modifiers as we dont want to trigger a graph instantiation
		ensureAlways(RequestArgs.EvaluationModifier != nullptr);
		Graph.InjectionData.EvaluationModifier = RequestArgs.EvaluationModifier;
		InjectionRecord.ModifierRequest = InEvent.Request;

		// Update status
		{
			auto StatusUpdateEvent = MakeTraitEvent<FInjection_StatusUpdateEvent>();
			StatusUpdateEvent->Request = InEvent.Request;
			StatusUpdateEvent->Status = EInjectionStatus::Playing;
			GetModuleInstance().QueueOutputTraitEvent(StatusUpdateEvent);
		}

		// Evaluation modifiers are consumed straight away
		InEvent.MarkConsumed();
		break;
	}
}

void FAnimNextModuleInjectionComponent::OnUninjectionEvent(UE::AnimNext::FInjection_UninjectEvent& InEvent)
{
	using namespace UE::AnimNext;

	const FInjectionRequestArgs& RequestArgs = InEvent.Request->GetArgs();

	// Injection modifiers can have private access to take over low-level tasks
	FName FoundName = NAME_None;
	TStructView<FAnimNextAnimGraph> InjectableGraph = InjectionInfo.FindInjectableGraphInstance(RequestArgs.Site, FoundName);
	if(!InjectableGraph.IsValid())
	{
		UE_LOGFMT(LogAnimation, Warning, "Could not find injection site {SiteName} for un-injection request", RequestArgs.Site.DesiredSiteName);
		InEvent.MarkConsumed();
		return;
	}

	FInjectionRecord& InjectionRecord = CurrentRequests.FindOrAdd(FoundName);

	// Update graph and bump serial number to identify this un-injection routing
	FAnimNextAnimGraph& Graph = InjectableGraph.Get<FAnimNextAnimGraph>();
	switch(RequestArgs.Type)
	{
	case EAnimNextInjectionType::InjectObject:
		// Update graph and bump serial number to identify this injection routing
		Graph.InjectionData.InjectionSerialNumber = IncrementSerialNumber();
		InEvent.SerialNumber = Graph.InjectionData.InjectionSerialNumber;
		InjectionRecord.GraphRequest.Reset();

		// Note we dont consume here, as we want the event to forward to the injection site trait
		break;
	case EAnimNextInjectionType::EvaluationModifier:
		// We dont increment serial number when applying evaluation modifiers as we dont want to trigger a graph instantiation
		Graph.InjectionData.EvaluationModifier = nullptr;
		InjectionRecord.ModifierRequest.Reset();

		// Update status
		{
			auto StatusUpdateEvent = MakeTraitEvent<FInjection_StatusUpdateEvent>();
			StatusUpdateEvent->Request = InEvent.Request;
			StatusUpdateEvent->Status = EInjectionStatus::Completed;
			GetModuleInstance().QueueOutputTraitEvent(StatusUpdateEvent);
		}
		
		// Evaluation modifiers are consumed straight away
		InEvent.MarkConsumed();
		break;
	}

	if(!InjectionRecord.IsValid())
	{
		CurrentRequests.Remove(InEvent.Request->GetArgs().Site.DesiredSiteName);
	}
}

void FAnimNextModuleInjectionComponent::OnReapplyInjection(const UE::AnimNext::FModuleTaskContext& InContext)
{
	using namespace UE::AnimNext;

	FAnimNextModuleInjectionComponent& Component = InContext.ModuleInstance->GetComponent<FAnimNextModuleInjectionComponent>();
	
	for(const TPair<FName, FInjectionRecord>& RequestPair : Component.CurrentRequests)
	{
		FName FoundName = NAME_None;
		TStructView<FAnimNextAnimGraph> InjectableGraph = Component.InjectionInfo.FindInjectableGraphInstance(UE::AnimNext::FInjectionSite(RequestPair.Key), FoundName);
		if(!InjectableGraph.IsValid())
		{
			continue;
		}

		// Re-apply this request, as it may have been overwritten by subsequent bindings/calculations
		FAnimNextAnimGraph& Graph = InjectableGraph.Get<FAnimNextAnimGraph>();
		if(RequestPair.Value.GraphRequest.IsValid())
		{
			Graph.InjectionData.InjectionSerialNumber = RequestPair.Value.SerialNumber;
		}
		if(RequestPair.Value.ModifierRequest.IsValid())
		{
			const FInjectionRequestArgs& ModifierRequestArgs = RequestPair.Value.ModifierRequest->GetArgs();
			Graph.InjectionData.EvaluationModifier = ModifierRequestArgs.EvaluationModifier;
		}
	}
}

uint32 FAnimNextModuleInjectionComponent::IncrementSerialNumber()
{
	// Avoid zero as this is 'invalid' and  will ensure at injection sites (indicating incorrect routing)
	if(++SerialNumber == 0)
	{
		++SerialNumber;
	}
	return SerialNumber;
}

void FAnimNextModuleInjectionComponent::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<FName, FInjectionRecord>& RequestPair : CurrentRequests)
	{
		RequestPair.Value.AddReferencedObjects(Collector);
	}
}

void FAnimNextModuleInjectionComponent::FInjectionRecord::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (GraphRequest.IsValid())
	{
		GraphRequest->AddReferencedObjects(Collector);
	}
	if (ModifierRequest.IsValid())
	{
		ModifierRequest->AddReferencedObjects(Collector);
	}
}

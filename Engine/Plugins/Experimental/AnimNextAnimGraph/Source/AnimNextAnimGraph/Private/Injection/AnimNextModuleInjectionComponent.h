// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InjectionInfo.h"
#include "DataInterface/AnimNextDataInterfaceHost.h"
#include "Injection/InjectionRequest.h"
#include "Module/AnimNextModuleInstanceComponent.h"
#include "UObject/GCObject.h"
#include "AnimNextModuleInjectionComponent.generated.h"

namespace UE::AnimNext
{
	struct FInjection_InjectEvent;
	struct FInjection_UninjectEvent;
	struct FModuleTaskContext;
}

// Module component that holds info about injection sites and routes injection requests
USTRUCT()
struct FAnimNextModuleInjectionComponent : public FAnimNextModuleInstanceComponent
{
	GENERATED_BODY()

	FAnimNextModuleInjectionComponent() = default;

	// Get the cached injection info for our module 
	const UE::AnimNext::FInjectionInfo& GetInjectionInfo() const { return InjectionInfo; }

	void AddStructReferencedObjects(class FReferenceCollector& Collector);

private:
	// FAnimNextModuleInstanceComponent interface
	virtual void OnInitialize() override;
	virtual void OnTraitEvent(FAnimNextTraitEvent& Event) override;

	void OnInjectionEvent(UE::AnimNext::FInjection_InjectEvent& InEvent);
	void OnUninjectionEvent(UE::AnimNext::FInjection_UninjectEvent& InEvent);

	static void OnReapplyInjection(const UE::AnimNext::FModuleTaskContext& InContext);

	uint32 IncrementSerialNumber();

private:
	// Info for injection
	UE::AnimNext::FInjectionInfo InjectionInfo;

	struct FInjectionRecord
	{
		bool IsValid() const
		{
			return GraphRequest.IsValid() || ModifierRequest.IsValid();
		}

		void AddReferencedObjects(FReferenceCollector& Collector);

		TSharedPtr<UE::AnimNext::FInjectionRequest> GraphRequest;
		TSharedPtr<UE::AnimNext::FInjectionRequest> ModifierRequest;
		uint32 SerialNumber = 0;
	};
	
	// Currently-injected requests
	TMap<FName, FInjectionRecord> CurrentRequests;

	// Serial number used to identify forwarded requests
	uint32 SerialNumber = 0;
};

template<>
struct TStructOpsTypeTraits<FAnimNextModuleInjectionComponent> : public TStructOpsTypeTraitsBase2<FAnimNextModuleInjectionComponent>
{
	enum
	{
		WithAddStructReferencedObjects = true,
	};
};

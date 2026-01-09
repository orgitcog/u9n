// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextDataInterfaceHost.h"
#include "StructUtils/StructView.h"

struct FAnimNextAnimGraph;

namespace UE::AnimNext
{

// Interface adapter used to inject a graph into a host.
// Uses the host's data interface and the name of the variable in the host's data interface to apply the supplied graph instance
struct FInjectionDataInterfaceHostAdapter : public IDataInterfaceHost
{
	FInjectionDataInterfaceHostAdapter() = default;
	
	FInjectionDataInterfaceHostAdapter(FAnimNextGraphInstance& InHostInstance, FName InName, TStructView<FAnimNextAnimGraph> InGraphInstance)
		: HostInstance(&InHostInstance)
		, Name(InName)
		, GraphInstance(InGraphInstance)
	{}

	// IDataInterfaceHost interface
	virtual const UAnimNextDataInterface* GetDataInterface() const override;
	virtual uint8* GetMemoryForVariable(int32 InVariableIndex, FName InVariableName, const FProperty* InVariableProperty) const override;

	FAnimNextGraphInstance* HostInstance = nullptr;
	FName Name;
	TStructView<FAnimNextAnimGraph> GraphInstance;
};

}

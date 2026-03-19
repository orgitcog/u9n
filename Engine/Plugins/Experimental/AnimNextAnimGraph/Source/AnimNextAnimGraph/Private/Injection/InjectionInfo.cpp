// Copyright Epic Games, Inc. All Rights Reserved.

#include "InjectionInfo.h"
#include "Injection/InjectionSite.h"

#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "Graph/AnimNextAnimGraph.h"
#include "StructUtils/PropertyBag.h"

namespace UE::AnimNext
{

FInjectionInfo::FInjectionInfo(const FAnimNextDataInterfaceInstance& InInstance)
	: Instance(&InInstance)
{
	CacheInfo();
}

void FInjectionInfo::CacheInfo() const
{
	check(Instance);
	check(Instance->GetDataInterface());

	DefaultInjectableGraphIndex = INDEX_NONE;
	InjectableGraphs.Reset();

	const UPropertyBag* Struct = Instance->Variables.GetPropertyBagStruct();
	const uint8* Memory = Instance->Variables.GetValue().GetMemory();
	if(Struct == nullptr || Memory == nullptr)
	{
		return;
	}

	// Find the default value in our set of variables
	// Note we dont recurse here as the struct views we cache cannot be relocated (e.g. held in an array) due to them being referenced
	// elsewhere via raw ptrs in RigVM memory handles
	int32 VariableIndex = 0;
	for (TPropertyValueIterator<FProperty> It(Struct, Memory, EPropertyValueIteratorFlags::NoRecursion); It; ++It, ++VariableIndex)
	{
		const FStructProperty* Property = CastField<FStructProperty>(It.Key());
		if(Property == nullptr)
		{
			continue;
		}

		if(Property->Struct->IsChildOf(FAnimNextAnimGraph::StaticStruct()))
		{
			const FAnimNextAnimGraph* Value = static_cast<const FAnimNextAnimGraph*>(It.Value());
			FName GraphName = Property->GetFName();
			TStructView<FAnimNextAnimGraph> GraphStructView(const_cast<FAnimNextAnimGraph&>(*Value));

			int32 GraphIndex = InjectableGraphs.Emplace(GraphName, GraphStructView);

			if(VariableIndex == Instance->GetDataInterface()->DefaultInjectionSiteIndex)
			{
				DefaultInjectableGraphIndex = GraphIndex;
			}
		}
	}

	if(DefaultInjectableGraphIndex == INDEX_NONE && InjectableGraphs.Num() > 0)
	{
		DefaultInjectableGraphIndex = 0;
	}
}

bool FInjectionInfo::GetDefaultInjectableGraph(FName& OutName, TStructView<FAnimNextAnimGraph>& OutGraph) const
{
	if(DefaultInjectableGraphIndex == INDEX_NONE || !InjectableGraphs.IsValidIndex(DefaultInjectableGraphIndex))
	{
		return false;
	}

	const FInjectableGraphInfo& InjectableGraph = InjectableGraphs[DefaultInjectableGraphIndex];
	OutName = InjectableGraph.Name;
	OutGraph = InjectableGraph.StructView;

	return OutName != NAME_None && OutGraph.IsValid();
}

TStructView<FAnimNextAnimGraph> FInjectionInfo::FindInjectableGraphInstance(const FInjectionSite& InSite, FName& OutRealName) const
{
	FName SiteName = InSite.DesiredSiteName;
	if(SiteName != NAME_None)
	{
		// Linear search all the injectable graphs for the name
		const int32 InjectableGraphNum = InjectableGraphs.Num();
		for(int32 InjectableGraphIndex = 0; InjectableGraphIndex < InjectableGraphNum; ++InjectableGraphIndex)
		{
			const FInjectableGraphInfo& InjectableGraphInfo = InjectableGraphs[InjectableGraphIndex];
			if(SiteName == InjectableGraphInfo.Name)
			{
				OutRealName = SiteName;
				return InjectableGraphInfo.StructView;
			}
		}
	}

	const bool bUseModuleDefault = SiteName == NAME_None || InSite.bUseModuleFallback;
	if(bUseModuleDefault && InjectableGraphs.IsValidIndex(DefaultInjectableGraphIndex))
	{
		const FInjectableGraphInfo& InjectableGraphInfo = InjectableGraphs[DefaultInjectableGraphIndex];
		OutRealName = InjectableGraphInfo.Name;
		return InjectableGraphInfo.StructView;
	}

	return TStructView<FAnimNextAnimGraph>();
}

}

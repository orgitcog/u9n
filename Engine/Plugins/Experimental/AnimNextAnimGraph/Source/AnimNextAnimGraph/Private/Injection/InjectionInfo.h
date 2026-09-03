// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/StructView.h"

struct FAnimNextDataInterfaceInstance;
struct FAnimNextAnimGraph;
struct FAnimNextModuleInjectionComponent;
struct FAnimNextInjectionSite;

namespace UE::AnimNext
{
using FInjectionSite = FAnimNextInjectionSite;

// Info used to track injection sites for a data interface instance (graph, module etc)
struct FInjectionInfo
{
	FInjectionInfo() = default;

	explicit FInjectionInfo(const FAnimNextDataInterfaceInstance& InInstance);

	// Get the default injectable graph data
	bool GetDefaultInjectableGraph(FName& OutName, TStructView<FAnimNextAnimGraph>& OutGraph) const;

	// Find an injectable graph instance by name.
	// @param   InSite            The injection site. If bUseModuleFallback is true, the default site will be returned if the site does not exist
	// @param   OutRealName       The name of the actual injection site we found (in the case NAME_None was passed)
	TStructView<FAnimNextAnimGraph> FindInjectableGraphInstance(const FInjectionSite& InSite, FName& OutRealName) const;

	// Iterate each injectable graph instance
	template<typename PredicateType>
	void ForEachInjectableGraphInstance(PredicateType InPredicate) const
	{
		for(const FInjectableGraphInfo& InjectableGraphInfo : InjectableGraphs)
		{
			InPredicate(InjectableGraphInfo.Name, InjectableGraphInfo.StructView);
		}
	}

private:
	void CacheInfo() const;

private:
	friend ::FAnimNextModuleInjectionComponent;

	struct FInjectableGraphInfo
	{
		FInjectableGraphInfo(FName InName, TStructView<FAnimNextAnimGraph> InStructView)
			: StructView(InStructView)
			, Name(InName)
		{}

		TStructView<FAnimNextAnimGraph> StructView;
		FName Name;
	};

	// Lookup of injectable graphs by name
	// Lookup is a linear search for now as we expect there to be 'not many' of these things. If we find this is a bottleneck, we can evaluate the
	// performance/memory cost of a TMap
	mutable TArray<FInjectableGraphInfo> InjectableGraphs;

	// Instance we are tracking
	const FAnimNextDataInterfaceInstance* Instance = nullptr;

	// The name of the default injectable graph for our module (user-adjustable via SetDefaultInjectionSite) 
	FName DefaultInjectableGraphName;

	// Index into InjectableGraphs of the default graph
	mutable int32 DefaultInjectableGraphIndex = INDEX_NONE;
};

}

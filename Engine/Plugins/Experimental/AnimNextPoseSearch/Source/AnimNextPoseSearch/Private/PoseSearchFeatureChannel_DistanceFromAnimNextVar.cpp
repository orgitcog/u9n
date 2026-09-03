// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_DistanceFromAnimNextVar.h"
#include "Component/AnimNextComponent.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"

#include "PoseSearch/PoseSearchContext.h"
#include "StructUtils/PropertyBag.h"


namespace {
// Helper function to find the first data interface instance in the context 
FAnimNextDataInterfaceInstance* GetFirstDataInterfaceInstance(FChooserEvaluationContext& Context)
{
	for(const FStructView& Param : Context.Params)
	{
		if(Param.GetScriptStruct() == FAnimNextDataInterfaceInstance::StaticStruct())
		{
			return Param.GetPtr<FAnimNextDataInterfaceInstance>();
		}
	}

	return nullptr;
}
}


void UPoseSearchFeatureChannel_DistanceFromAnimNextVar::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;
	FChooserEvaluationContext* Context = SearchContext.GetContext(SampleRole);
	
	float Distance = 0.0f;

	if (FAnimNextDataInterfaceInstance* Instance = GetFirstDataInterfaceInstance(*Context))
	{
		Instance->GetVariable(DistanceVariableName, Distance);
	}
	
	FFeatureVectorHelper::EncodeFloat(SearchContext.EditFeatureVector(), ChannelDataOffset, Distance);
}

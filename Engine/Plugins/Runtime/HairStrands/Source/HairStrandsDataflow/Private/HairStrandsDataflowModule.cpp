// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsDataflowModule.h"

#include "AttachGuidesRootsNode.h"
#include "BuildGuidesLODsNode.h"
#include "GetGroomAssetNode.h"
#include "GroomCollectionFacades.h"
#include "GroomAssetTerminalNode.h"
#include "GetGroomAttributesNodes.h"
#include "ResampleGuidesPointsNode.h"
#include "BuildGroomSkinningNodes.h"
#include "GenerateGuidesCurvesNode.h"
#include "SmoothGuidesCurvesNode.h"
#include "GroomDataflowRendering.h"
#include "GroomDataflowVisualization.h"
#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"
#include "Dataflow/DataflowCategoryRegistry.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "HairStrandsDataflow"

class FGroomCollectionAddScalarVertexPropertyCallbacks : public IDataflowAddScalarVertexPropertyCallbacks
{
public:

	const static FName Name;

	virtual ~FGroomCollectionAddScalarVertexPropertyCallbacks() override = default;

	virtual FName GetName() const override
	{
		return Name;
	}

	virtual TArray<FName> GetTargetGroupNames() const override
	{
		return {UE::Groom::FGroomGuidesFacade::VerticesGroup, UE::Groom::FGroomStrandsFacade::VerticesGroup};
	}

	virtual TArray<UE::Dataflow::FRenderingParameter> GetRenderingParameters() const override
	{
		return { {TEXT("GuidesRender"), FName("FGroomCollection"), {TEXT("Collection")}},
			{TEXT("StrandsRender"), FName("FGroomCollection"), {TEXT("Collection")}} };
	}
};

const FName FGroomCollectionAddScalarVertexPropertyCallbacks::Name = FName(TEXT("FGroomCollectionAddScalarVertexPropertyCallbacks"));

void FHairStrandsDataflowModule::StartupModule()
{
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetGroomAssetDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGroomAssetTerminalDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FResampleGuidesPointsDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTransferSkinWeightsGroomNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateGuidesCurvesDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetGroomAttributesDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSmoothGuidesCurvesDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAttachGuidesRootsDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBuildGuidesLODsDataflowNode);
	
	UE::Groom::RegisterRenderingCallbacks();
	
	FDataflowAddScalarVertexPropertyCallbackRegistry::Get().RegisterCallbacks(
		MakeUnique<FGroomCollectionAddScalarVertexPropertyCallbacks>());

	UE::Dataflow::FDataflowSimulationVisualizationRegistry::GetInstance().RegisterVisualization(
		MakeUnique<UE::Groom::FGroomDataflowSimulationVisualization>());

	// register node category for groom asset
	UE_DATAFLOW_REGISTER_CATEGORY_FORASSET_TYPE("Groom", UGroomAsset);
}

void FHairStrandsDataflowModule::ShutdownModule()
{
	UE::Groom::DeregisterRenderingCallbacks();

	FDataflowAddScalarVertexPropertyCallbackRegistry::Get().DeregisterCallbacks(
		FGroomCollectionAddScalarVertexPropertyCallbacks::Name);

	UE::Dataflow::FDataflowSimulationVisualizationRegistry::GetInstance().DeregisterVisualization(
		UE::Groom::FGroomDataflowSimulationVisualization::Name);
}

IMPLEMENT_MODULE(FHairStrandsDataflowModule, HairStrandsDataflow)

#undef LOCTEXT_NAMESPACE

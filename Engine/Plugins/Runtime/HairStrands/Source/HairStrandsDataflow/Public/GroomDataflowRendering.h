// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowRenderingFactory.h"
#include "Dataflow/DataflowRenderingViewMode.h"

namespace UE::Groom
{

class FGroomStrandsFacade;
class FGroomGuidesFacade;

/** Strands rendering callback for the dataflow editor */
class HAIRSTRANDSDATAFLOW_API FGroomStrandsRenderingCallbacks : public Dataflow::FRenderingFactory::ICallbackInterface
{
public :
	static Dataflow::FRenderKey RenderKey;
	
protected:

	//¬ Begin ICallbackInterface interface
	virtual Dataflow::FRenderKey GetRenderKey() const override
	{
		return RenderKey;
	}

	virtual bool CanRender(const Dataflow::IDataflowConstructionViewMode& ViewMode) const override
	{
		return (ViewMode.GetName() == Dataflow::FDataflowConstruction3DViewMode::Name);
	}

	virtual void Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State) override;
	//¬ End ICallbackInterface interface
	
	virtual void ComputeVertexColors(const FGroomStrandsFacade& StrandsFacade, TArray<FLinearColor>& VertexColors) const;

	/** Get the group attribute used to create each geometry groups */
	virtual int32 GetGroupAttribute(const FGroomStrandsFacade& StrandsFacade, FString& GroupAttribute, FString& GroupName) const;
};

/** Guides rendering callback for the dataflow editor */
class HAIRSTRANDSDATAFLOW_API FGroomGuidesRenderingCallbacks : public Dataflow::FRenderingFactory::ICallbackInterface
{
	public :
		static Dataflow::FRenderKey RenderKey;
	
protected:

	//¬ Begin ICallbackInterface interface
	virtual Dataflow::FRenderKey GetRenderKey() const override
	{
		return RenderKey;
	}

	virtual bool CanRender(const Dataflow::IDataflowConstructionViewMode& ViewMode) const override
	{
		return (ViewMode.GetName() == Dataflow::FDataflowConstruction3DViewMode::Name);
	}

	virtual void Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State) override;
	
	virtual void ComputeVertexColors(const FGroomGuidesFacade& GuidesFacade, TArray<FLinearColor>& VertexColors) const;
	
	/** Get the group attribute used to create each geometry groups */
	virtual int32 GetGroupAttribute(const FGroomGuidesFacade& StrandsFacade, FString& GroupAttribute, FString& GroupName) const;
    	
	//¬ End ICallbackInterface interface
};

/** Register rendering callbacks */
void RegisterRenderingCallbacks();

/** Deregister rendering callbacks */
void DeregisterRenderingCallbacks();
	
}  // End namespace UE::Groom

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GroomDataflowRendering.h"
#include "Dataflow/DataflowRenderingFactory.h"
#include "Dataflow/DataflowRenderingViewMode.h"

namespace UE::CardGen::Private
{
	class FCardsGeometryRenderingCallbacks : public Dataflow::FRenderingFactory::ICallbackInterface
	{
	public :
		static Dataflow::FRenderKey RenderKey;
		
	protected:

		virtual Dataflow::FRenderKey GetRenderKey() const override
		{
			return RenderKey;
		}

		virtual bool CanRender(const Dataflow::IDataflowConstructionViewMode& ViewMode) const override
		{
			return (ViewMode.GetName() == Dataflow::FDataflowConstruction3DViewMode::Name);
		}

		virtual void Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State) override;
		virtual void ComputeVertexColors(const FManagedArrayCollection& Collection, const int32 CardIndex, TArray<FLinearColor>& VertexColors) const {}
	};

	class FCardsClumpsRenderingCallbacks : public UE::Groom::FGroomStrandsRenderingCallbacks
	{
	public :
		static Dataflow::FRenderKey RenderKey;
		
	private:

		virtual Dataflow::FRenderKey GetRenderKey() const override
		{
			return RenderKey;
		}
		
		virtual void ComputeVertexColors(const UE::Groom::FGroomStrandsFacade& StrandsFacade, TArray<FLinearColor>& VertexColors) const override;
		
		/** Get the group attribute used to create each geometry groups */
		virtual int32 GetGroupAttribute(const UE::Groom::FGroomStrandsFacade& StrandsFacade, FString& GroupAttribute, FString& GroupName) const override;
	};

	class FCardsTextureRenderingCallbacks : public FCardsGeometryRenderingCallbacks
	{
	public :
			static Dataflow::FRenderKey RenderKey;
		
	private:

		virtual bool CanRender(const Dataflow::IDataflowConstructionViewMode& ViewMode) const override
		{
			return (ViewMode.GetName() == Dataflow::FDataflowConstruction3DViewMode::Name || ViewMode.GetName() == Dataflow::FDataflowConstructionUVViewMode::Name);
		}

		virtual Dataflow::FRenderKey GetRenderKey() const override
		{
			return RenderKey;
		}
		virtual void Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State) override;
		virtual void ComputeVertexColors(const FManagedArrayCollection& Collection, const int32 CardIndex, TArray<FLinearColor>& VertexColors) const override;
	};
	
	static void RegisterRenderingCallbacks()
	{
		Dataflow::FRenderingFactory::GetInstance()->RegisterCallbacks(MakeUnique<FCardsClumpsRenderingCallbacks>());
		Dataflow::FRenderingFactory::GetInstance()->RegisterCallbacks(MakeUnique<FCardsGeometryRenderingCallbacks>());
		Dataflow::FRenderingFactory::GetInstance()->RegisterCallbacks(MakeUnique<FCardsTextureRenderingCallbacks>());
	}

	static void DeregisterRenderingCallbacks()
	{
		Dataflow::FRenderingFactory::GetInstance()->DeregisterCallbacks(FCardsGeometryRenderingCallbacks::RenderKey);
		Dataflow::FRenderingFactory::GetInstance()->DeregisterCallbacks(FCardsClumpsRenderingCallbacks::RenderKey);
		Dataflow::FRenderingFactory::GetInstance()->DeregisterCallbacks(FCardsTextureRenderingCallbacks::RenderKey);
	}

}  // End namespace Private

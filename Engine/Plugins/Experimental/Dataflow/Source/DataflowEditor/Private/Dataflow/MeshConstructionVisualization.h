// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Dataflow/DataflowConstructionVisualization.h"
#include "Math/Color.h" 

namespace UE::Dataflow
{
	/** Visualization for drawing things on and around the mesh in the construction viewport */
	class FMeshConstructionVisualization final : public IDataflowConstructionVisualization
	{
	public:
		static FName Name;

		FLinearColor GetVertexIDColor() const { return VertexIDColor; }
		void SetVertexIDColor(const FLinearColor Value) { VertexIDColor = Value; }

		FLinearColor GetFaceIDColor() const { return FaceIDColor; }
		void SetFaceIDColor(const FLinearColor Value) { FaceIDColor = Value; }

		float GetVertexNormalLength() const { return VertexNormalLength; }
		void SetVertexNormalLength(const float Value) { VertexNormalLength = Value; }
		float GetVertexNormalThickness() const { return VertexNormalThickness; }
		void SetVertexNormalThickness(const float Value) { VertexNormalThickness = Value; }
		FLinearColor GetVertexNormalColor() const { return VertexNormalColor; }
		void SetVertexNormalColor(const FLinearColor Value) { VertexNormalColor = Value; }

		float GetFaceNormalLength() const { return FaceNormalLength; }
		void SetFaceNormalLength(const float Value) { FaceNormalLength = Value; }
		float GetFaceNormalThickness() const { return FaceNormalThickness; }
		void SetFaceNormalThickness(const float Value) { FaceNormalThickness = Value; }
		FLinearColor GetFaceNormalColor() const { return FaceNormalColor; }
		void SetFaceNormalColor(const FLinearColor Value) { FaceNormalColor = Value; }

		float GetDistanceCutoff() const { return DistanceCutoff; }
		void SetDistanceCutoff(const float Value) { DistanceCutoff = Value; }

	private:
		virtual FName GetName() const override
		{
			return Name;
		}
		virtual void ExtendViewportShowMenu(const TSharedPtr<FDataflowConstructionViewportClient>& ViewportClient, FMenuBuilder& MenuBuilder) override;
		virtual void Draw(const FDataflowConstructionScene* ConstructionScene, FPrimitiveDrawInterface* PDI, const FSceneView* SceneView = nullptr) override;
		virtual void DrawCanvas(const FDataflowConstructionScene* ConstructionScene, FCanvas* Canvas, const FSceneView* SceneView) override;

		bool bMeshVertexIDVisualizationEnabled = false;
		bool bMeshFaceIDVisualizationEnabled = false;
		bool bMeshVertexNormalsVisualizationEnabled = false;
		bool bMeshFaceNormalsVisualizationEnabled = false;

		FLinearColor VertexIDColor = FLinearColor::Green;
		
		FLinearColor FaceIDColor = FLinearColor::Yellow;

		float VertexNormalLength = 3.f;
		float VertexNormalThickness = 0.3f;
		FLinearColor VertexNormalColor = FLinearColor::Blue;

		float FaceNormalLength = 3.f;
		float FaceNormalThickness = 0.3f;
		FLinearColor FaceNormalColor = FLinearColor::Red;

		float DistanceCutoff = 500.f;

		bool bIgnoreOccludedTriangles = false;
	};
}


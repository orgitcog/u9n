// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/MeshConstructionVisualization.h"
#include "Components/DynamicMeshComponent.h"
#include "Dataflow/DataflowConstructionViewportClient.h"
#include "Dataflow/DataflowConstructionScene.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/Engine.h"  // for GEngine->GetSmallFont()
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneView.h"
#include "BoxTypes.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMeshEditor.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "MeshConstructionVisualization"

namespace UE::Dataflow
{
	const FMargin LabelWidgetsMargin(15.0f, 0.0f, 3.0f, 0.0f);
	const FMargin WidgetsMargin(2.0f, 0.0f, 3.0f, 0.0f);
	const FVector2D SpacerSize = FVector2D(1, 4);

	TSharedRef<SWidget> CreateNumericEntryWidget(
		const TSharedRef<SWidget>& InNumericBoxWidget,
		const FText& InLabel)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(LabelWidgetsMargin)
			.AutoWidth()
			[
				SNew(STextBlock)
					.Text(InLabel)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.Padding(FMargin(6.0f, 0))
			.FillContentWidth(1.0)
			[
				SNew(SBox)
					.Padding(WidgetsMargin)
					.MinDesiredWidth(80.0f)
					[
						InNumericBoxWidget
					]
			];
	}

	TSharedRef<SWidget> CreateColorEntryWidget(
		const TSharedRef<SWidget>& InColorWidget,
		const FText& InLabel)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(LabelWidgetsMargin)
			.AutoWidth()
			[
				SNew(STextBlock)
					.Text(InLabel)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.Padding(FMargin(6.0f, 0))
			.FillContentWidth(1.0)
			[
				SNew(SBox)
					.Padding(WidgetsMargin)
					.MinDesiredWidth(80.0f)
					[
						InColorWidget
					]
			];
	}

	FName FMeshConstructionVisualization::Name = FName("MeshConstructionVisualization");

	void FMeshConstructionVisualization::ExtendViewportShowMenu(const TSharedPtr<FDataflowConstructionViewportClient>& ViewportClient, FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection(TEXT("MeshVisualization"), LOCTEXT("MeshVisualizationSectionName", "Mesh"));
		{
			TSharedRef<SWidget> VertexIDColorEntryWidget =
				SNew(SColorBlock)
				.ToolTipText(LOCTEXT("VertexId", "Show Vertex identifier"))
				.Color_Lambda([this]()
					{
						return GetVertexIDColor();
					})
				.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&)
					{
						FColorPickerArgs PickerArgs;
						PickerArgs.bUseAlpha = false;
						FLinearColor Color = GetVertexNormalColor();
						PickerArgs.InitialColor = Color;
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this](FLinearColor Color)
							{
								SetVertexIDColor(Color);
							});
						OpenColorPicker(PickerArgs);
						return FReply::Handled();
					});

			TSharedRef<SWidget> FaceIDColorEntryWidget =
				SNew(SColorBlock)
				.ToolTipText(LOCTEXT("FaceId", "Show Face identifier"))
				.Color_Lambda([this]()
					{
						return GetFaceIDColor();
					})
				.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&)
					{
						FColorPickerArgs PickerArgs;
						PickerArgs.bUseAlpha = false;
						FLinearColor Color = GetVertexNormalColor();
						PickerArgs.InitialColor = Color;
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this](FLinearColor Color)
							{
								SetFaceIDColor(Color);
							});
						OpenColorPicker(PickerArgs);
						return FReply::Handled();
					});

			TSharedRef<SWidget> VertexNormalLengthNumericEntryWidget =
				SNew(SNumericEntryBox<float>)
				.ToolTipText(LOCTEXT("VertexNormalLength", "Adjust the length of the per-Vertex normals"))
				.MinValue(1.f)
				.MaxValue(10.f)
				.MaxSliderValue(10.f)
				.AllowSpin(true)
				.MaxFractionalDigits(1)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Lambda([this](float InValue)
					{
						SetVertexNormalLength(InValue);
					})
				.Value_Lambda([this]()
					{
						return GetVertexNormalLength();
					});

			TSharedRef<SWidget> VertexNormalThicknessNumericEntryWidget =
				SNew(SNumericEntryBox<float>)
				.ToolTipText(LOCTEXT("VertexNormalThickness", "Adjust the thickness of the per-Vertex normals"))
				.MinValue(0.2f)
				.MaxValue(1.f)
				.MaxSliderValue(1.f)
				.AllowSpin(true)
				.MaxFractionalDigits(2)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Lambda([this](float InValue)
					{
						SetVertexNormalThickness(InValue);
					})
				.Value_Lambda([this]()
					{
						return GetVertexNormalThickness();
					});

			TSharedRef<SWidget> VertexNormalColorEntryWidget =
				SNew(SColorBlock)
				.ToolTipText(LOCTEXT("VertexNormalColor", "Adjust the color of the per-Vertex normals"))
				.Color_Lambda([this]()
					{
						return GetVertexNormalColor();
					})
				.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&)
					{
						FColorPickerArgs PickerArgs;
						PickerArgs.bUseAlpha = false;
						FLinearColor Color = GetVertexNormalColor();
						PickerArgs.InitialColor = Color;
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this](FLinearColor Color)
							{
								SetVertexNormalColor(Color);
							});
						OpenColorPicker(PickerArgs);
						return FReply::Handled();
					});

			TSharedRef<SWidget> FaceNormalLengthNumericEntryWidget =
				SNew(SNumericEntryBox<float>)
				.ToolTipText(LOCTEXT("FaceNormalLength", "Adjust the length of the per-Face normals"))
				.MinValue(1.f)
				.MaxValue(10.f)
				.MaxSliderValue(10.f)
				.AllowSpin(true)
				.MaxFractionalDigits(1)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Lambda([this](float InValue)
					{
						SetFaceNormalLength(InValue);
					})
				.Value_Lambda([this]()
					{
						return GetFaceNormalLength();
					});

			TSharedRef<SWidget> FaceNormalThicknessNumericEntryWidget =
				SNew(SNumericEntryBox<float>)
				.ToolTipText(LOCTEXT("FaceNormalThickness", "Adjust the thickness of the per-Face normals"))
				.MinValue(0.2f)
				.MaxValue(1.f)
				.MaxSliderValue(1.f)
				.AllowSpin(true)
				.MaxFractionalDigits(2)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Lambda([this](float InValue)
					{
						SetFaceNormalThickness(InValue);
					})
				.Value_Lambda([this]()
					{
						return GetFaceNormalThickness();
					});

			TSharedRef<SWidget> FaceNormalColorEntryWidget =
				SNew(SColorBlock)
				.ToolTipText(LOCTEXT("FaceNormalColor", "Adjust the Color of the per-Face normals"))
				.Color_Lambda([this]()
					{
						return GetFaceNormalColor();
					})
				.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&)
					{
						FColorPickerArgs PickerArgs;
						PickerArgs.bUseAlpha = false;
						FLinearColor Color = GetFaceNormalColor();
						PickerArgs.InitialColor = Color;
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this](FLinearColor Color)
							{
								SetFaceNormalColor(Color);
							});
						OpenColorPicker(PickerArgs);
						return FReply::Handled();
					});

			TSharedRef<SWidget> DistanceCutoffNumericEntryWidget =
				SNew(SNumericEntryBox<float>)
				.ToolTipText(LOCTEXT("DistanceCutoff", "Adjust the distance at which the visualization elements are not display anymore"))
				.MinValue(1.f)
				.MaxValue(1000.f)
				.MaxSliderValue(1000.f)
				.AllowSpin(true)
				.MaxFractionalDigits(1)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Lambda([this](float InValue)
					{
						SetDistanceCutoff(InValue);
					})
				.Value_Lambda([this]()
					{
						return GetDistanceCutoff();
					});

			///////////////////////////////////////////////////////////////////////////////////////////////////////////////

			const FUIAction MeshVertexNumbersToggleAction(
				FExecuteAction::CreateLambda([this, ViewportClient]()
					{
						bMeshVertexIDVisualizationEnabled = !bMeshVertexIDVisualizationEnabled;
						if (ViewportClient)
						{
							ViewportClient->Invalidate();
						}
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						return bMeshVertexIDVisualizationEnabled;
					}));

			const FUIAction MeshFaceNumbersToggleAction(
				FExecuteAction::CreateLambda([this, ViewportClient]()
					{
						bMeshFaceIDVisualizationEnabled = !bMeshFaceIDVisualizationEnabled;
						if (ViewportClient)
						{
							ViewportClient->Invalidate();
						}
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						return bMeshFaceIDVisualizationEnabled;
					}));

			const FUIAction MeshVertexNormalsToggleAction(
				FExecuteAction::CreateLambda([this, ViewportClient]()
					{
						bMeshVertexNormalsVisualizationEnabled = !bMeshVertexNormalsVisualizationEnabled;
						if (ViewportClient)
						{
							ViewportClient->Invalidate();
						}
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						return bMeshVertexNormalsVisualizationEnabled;
					}));

			const FUIAction MeshFaceNormalsToggleAction(
				FExecuteAction::CreateLambda([this, ViewportClient]()
					{
						bMeshFaceNormalsVisualizationEnabled = !bMeshFaceNormalsVisualizationEnabled;
						if (ViewportClient)
						{
							ViewportClient->Invalidate();
						}
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						return bMeshFaceNormalsVisualizationEnabled;
					}));

			const FUIAction IgnoreOccludedTrianglesToggleAction(
				FExecuteAction::CreateLambda([this, ViewportClient]()
					{
						bIgnoreOccludedTriangles = !bIgnoreOccludedTriangles;
						if (ViewportClient)
						{
							ViewportClient->Invalidate();
						}
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						return bIgnoreOccludedTriangles;
					}));

			///////////////////////////////////////////////////////////////////////////////////////////////////////////////

			MenuBuilder.AddMenuEntry(LOCTEXT("MeshVisualization_VertexNumbersEnabled", "Vertex ID"),
				LOCTEXT("MeshVisualization_VertexNumbersEnabled_TooltipText", "Display vertex ID"),
				FSlateIcon(),
				MeshVertexNumbersToggleAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton);

			MenuBuilder.AddWidget(
				CreateColorEntryWidget(VertexIDColorEntryWidget, LOCTEXT("MeshVisualization_VertexIDColor", "Color")),
				FText::GetEmpty(), true);

			MenuBuilder.AddWidget(SNew(SSpacer).Size(SpacerSize), FText::GetEmpty(), false, false);

			MenuBuilder.AddMenuEntry(LOCTEXT("MeshVisualization_FaceNumbersEnabled", "Face ID"),
				LOCTEXT("MeshVisualization_FaceNumbersEnabled_TooltipText", "Display face ID"),
				FSlateIcon(),
				MeshFaceNumbersToggleAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton);

			MenuBuilder.AddWidget(
				CreateColorEntryWidget(FaceIDColorEntryWidget, LOCTEXT("MeshVisualization_FaceIDColor", "Color")),
				FText::GetEmpty(), true);

			MenuBuilder.AddWidget(SNew(SSpacer).Size(SpacerSize), FText::GetEmpty(), false, false);

			MenuBuilder.AddMenuEntry(LOCTEXT("MeshVisualization_VertexNormalsEnabled", "Vertex Normals"),
				LOCTEXT("MeshVisualization_VertexNormalsEnabled_TooltipText", "Display vertex normals"),
				FSlateIcon(),
				MeshVertexNormalsToggleAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton);

			MenuBuilder.AddWidget(
				CreateNumericEntryWidget(VertexNormalLengthNumericEntryWidget, LOCTEXT("MeshVisualization_VertexNormalLength", "Length")),
				FText::GetEmpty(), true);

			MenuBuilder.AddWidget(
				CreateNumericEntryWidget(VertexNormalThicknessNumericEntryWidget, LOCTEXT("MeshVisualization_VertexNormalThickness", "Thickness")),
				FText::GetEmpty(), true);

			MenuBuilder.AddWidget(
				CreateColorEntryWidget(VertexNormalColorEntryWidget, LOCTEXT("MeshVisualization_VertexNormalColor", "Color")),
				FText::GetEmpty(), true);

			MenuBuilder.AddWidget(SNew(SSpacer).Size(SpacerSize), FText::GetEmpty(), false, false);

			MenuBuilder.AddMenuEntry(LOCTEXT("MeshVisualization_FaceNormalsEnabled", "Face Normals"),
				LOCTEXT("MeshVisualization_FaceNormalsEnabled_TooltipText", "Display face normals"),
				FSlateIcon(),
				MeshFaceNormalsToggleAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton);

			MenuBuilder.AddWidget(
				CreateNumericEntryWidget(FaceNormalLengthNumericEntryWidget, LOCTEXT("MeshVisualization_FaceNormalLength", "Length")),
				FText::GetEmpty(), true);

			MenuBuilder.AddWidget(
				CreateNumericEntryWidget(FaceNormalThicknessNumericEntryWidget, LOCTEXT("MeshVisualization_FaceNormalThickness", "Thickness")),
				FText::GetEmpty(), true);

			MenuBuilder.AddWidget(
				CreateColorEntryWidget(FaceNormalColorEntryWidget, LOCTEXT("MeshVisualization_FaceNormalColor", "Color")),
				FText::GetEmpty(), true);

			// Disabled for now
			// TODO: Optimize this functionality
#if 0
			MenuBuilder.AddWidget(SNew(SSpacer).Size(SpacerSize), FText::GetEmpty(), false, false);
			MenuBuilder.AddWidget(SNew(SSpacer).Size(SpacerSize), FText::GetEmpty(), false, false);

			MenuBuilder.AddWidget(
				CreateNumericEntryWidget(DistanceCutoffNumericEntryWidget, LOCTEXT("MeshVisualization_DistanceCutoff", "Distance Cutoff")),
				FText::GetEmpty(), true);

			MenuBuilder.AddMenuEntry(LOCTEXT("MeshVisualization_IgnoreOccludedTriangles", "Ignore Occluded Triangles"),
				LOCTEXT("MeshVisualization_IgnoreOccludedTriangles_TooltipText", "Ignore Occluded Triangles"),
				FSlateIcon(),
				IgnoreOccludedTrianglesToggleAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton);
#endif
		}
		MenuBuilder.EndSection();
	}

	static void DrawText(FCanvas* Canvas, const FSceneView* SceneView, const FVector& Pos, const FText& Text, const FLinearColor& Color, const float Scale = 1.f)
	{
#if WITH_EDITOR
		if (Canvas && SceneView)
		{
			FVector2D PixelLocation;
			if (SceneView->WorldToPixel(Pos, PixelLocation))
			{
				// WorldToPixel doesn't account for DPIScale
				const float DPIScale = Canvas->GetDPIScale();
				FCanvasTextItem TextItem(PixelLocation / DPIScale, Text, GEngine->GetSmallFont(), Color);
				TextItem.Scale = FVector2D::UnitVector * Scale;
				TextItem.EnableShadow(FLinearColor::Black);
				TextItem.Draw(Canvas);
			}
		}
#endif
	}

	static bool IsTriangleVisible(const FDataflowConstructionScene* ConstructionScene, const FSceneView* SceneView, const int32 TriangleID, float& DistanceMin, float& DistanceMax, TMap<int32, float>& TriangleDistances, const bool bIgnoreOccludedTriangles)
	{
		const UE::Geometry::FDynamicMesh3& ResultMesh = ConstructionScene->DebugMesh.ResultMesh;
		const UE::Geometry::FDynamicMeshAABBTree3& Spatial = ConstructionScene->DebugMesh.Spatial;

		const FVector TriangleCentroid = ResultMesh.GetTriCentroid(TriangleID);
		FVector LocalEyeDirection = TriangleCentroid - SceneView->ViewLocation;

		FRay3d LocalEyeRay;
		LocalEyeRay.Origin = SceneView->ViewLocation;
		FVector LocalPosition = ResultMesh.GetTriCentroid(TriangleID);
		LocalEyeRay.Direction = UE::Geometry::Normalized(LocalPosition - SceneView->ViewLocation);
		if (LocalEyeRay.Direction.Dot(LocalEyeDirection) < 0)
		{
			return false;
		}

		double LocalRayHitT;
		int HitTriangleID;
		FVector3d HitBaryCoords;
		if (bIgnoreOccludedTriangles)
		{
			if (Spatial.FindNearestHitTriangle(LocalEyeRay, LocalRayHitT, HitTriangleID, HitBaryCoords))
			{
				if (HitTriangleID != TriangleID)
				{
					return false;
				}
				else
				{
					if (LocalRayHitT < DistanceMin)
					{
						DistanceMin = LocalRayHitT;
					}
					else if (LocalRayHitT > DistanceMax)
					{
						DistanceMax = LocalRayHitT;
					}

					TriangleDistances.Add(TriangleID, LocalRayHitT);
				}
			}
		}

		return true;
	}

	static void ComputeVisibleTriangles(const FDataflowConstructionScene* ConstructionScene, const FSceneView* SceneView, const UE::Geometry::FDynamicMesh3& ResultMesh, TArray<int32>& VisibleTriangles, float& DistanceMin, float& DistanceMax, TMap<int32, float>& TriangleDistances, const bool bIgnoreOccludedTriangles)
	{
		for (int32 TriangleID : ResultMesh.TriangleIndicesItr())
		{
			if (IsTriangleVisible(ConstructionScene, SceneView, TriangleID, DistanceMin, DistanceMax, TriangleDistances, bIgnoreOccludedTriangles))
			{
				VisibleTriangles.Add(TriangleID);
			}
		}
	}

	void FMeshConstructionVisualization::DrawCanvas(const FDataflowConstructionScene* ConstructionScene, FCanvas* Canvas, const FSceneView* SceneView)
	{
		constexpr float CEndColorProgress = 0.8f;

		if (ConstructionScene && Canvas && SceneView && (bMeshVertexIDVisualizationEnabled || bMeshFaceIDVisualizationEnabled))
		{
			const UE::Geometry::FDynamicMesh3& ResultMesh = ConstructionScene->DebugMesh.ResultMesh;

			if (ResultMesh.TriangleCount() > 0)
			{
				TMap<int32, float> TriangleDistances;
				float DistanceMin = FLT_MAX, DistanceMax = -FLT_MAX;

				TArray<int32> VisibleTriangles;
				ComputeVisibleTriangles(ConstructionScene, SceneView, ResultMesh, VisibleTriangles, DistanceMin, DistanceMax, TriangleDistances, bIgnoreOccludedTriangles);

				if (bMeshVertexIDVisualizationEnabled && VisibleTriangles.Num() > 0)
				{
					TSet<int32> VerticesSet;
					TMap<int32, int32> VertexToTriangle;
					for (int32 TriangleID : VisibleTriangles)
					{
						if (bIgnoreOccludedTriangles && (TriangleDistances[TriangleID] > GetDistanceCutoff()))
						{
							continue;
						}

						const UE::Geometry::FIndex3i TriangleIndices = ResultMesh.GetTriangle(TriangleID);

						auto SetVertex = [&VerticesSet, &VertexToTriangle](const int32 VertexID, const int32 TriangleID) {
							if (!VerticesSet.Contains(VertexID))
							{
								VerticesSet.Add(VertexID);
								VertexToTriangle.Add(VertexID, TriangleID);
							}
						};

						SetVertex(TriangleIndices.A, TriangleID);
						SetVertex(TriangleIndices.B, TriangleID);
						SetVertex(TriangleIndices.C, TriangleID);
					}

					for (int32 VertexID : VerticesSet)
					{
						const FVector VertexPos = ResultMesh.GetVertex(VertexID) + FVector(0.15, 0, 0.15);
						const FText Text = FText::AsNumber(ConstructionScene->DebugMesh.VertexMap[VertexID]);

						if (bIgnoreOccludedTriangles)
						{
							FLinearColor EndColor = FLinearColor::LerpUsingHSV(GetVertexIDColor(), FLinearColor::Black, CEndColorProgress);
							float Progress = (TriangleDistances[VertexToTriangle[VertexID]] - DistanceMin) / (DistanceMax - DistanceMin);
							FLinearColor Color = FLinearColor::LerpUsingHSV(GetVertexIDColor(), EndColor, Progress);

							DrawText(Canvas, SceneView, VertexPos, Text, Color);
						}
						else
						{
							DrawText(Canvas, SceneView, VertexPos, Text, GetVertexIDColor());
						}
					}
				}

				if (bMeshFaceIDVisualizationEnabled && VisibleTriangles.Num() > 0)
				{
					for (int32 TriangleID : VisibleTriangles)
					{
						const FVector TriangleCentroid = ResultMesh.GetTriCentroid(TriangleID);
						const FText Text = FText::AsNumber(ConstructionScene->DebugMesh.FaceMap[TriangleID]);

						if (bIgnoreOccludedTriangles)
						{
							if (TriangleDistances[TriangleID] > GetDistanceCutoff())
							{
								continue;
							}

							FLinearColor EndColor = FLinearColor::LerpUsingHSV(GetFaceIDColor(), FLinearColor::Black, CEndColorProgress);
							float Progress = (TriangleDistances[TriangleID] - DistanceMin) / (DistanceMax - DistanceMin);
							FLinearColor Color = FLinearColor::LerpUsingHSV(GetFaceIDColor(), EndColor, Progress);

							DrawText(Canvas, SceneView, TriangleCentroid, Text, Color);
						}
						else
						{
							DrawText(Canvas, SceneView, TriangleCentroid, Text, GetFaceIDColor());
						}
					}
				}
			}
		}
	}

	void FMeshConstructionVisualization::Draw(const FDataflowConstructionScene* ConstructionScene, FPrimitiveDrawInterface* PDI, const FSceneView* SceneView)
	{
		if (ConstructionScene && PDI && SceneView && (bMeshVertexNormalsVisualizationEnabled || bMeshFaceNormalsVisualizationEnabled))
		{
			const UE::Geometry::FDynamicMesh3& ResultMesh = ConstructionScene->DebugMesh.ResultMesh;

			if (ResultMesh.TriangleCount() > 0)
			{
				TArray<int32> VisibleTriangles;
				TMap<int32, float> TriangleDistances;
				float DistanceMin = FLT_MAX, DistanceMax = -FLT_MAX;
				ComputeVisibleTriangles(ConstructionScene, SceneView, ResultMesh, VisibleTriangles, DistanceMin, DistanceMax, TriangleDistances, bIgnoreOccludedTriangles);

				if (bMeshVertexNormalsVisualizationEnabled && VisibleTriangles.Num() > 0)
				{
					if (ResultMesh.HasAttributes())
					{
						if (const UE::Geometry::FDynamicMeshNormalOverlay* const NormalOverlay = ResultMesh.Attributes()->PrimaryNormals())
						{
							TSet<int32> VerticesSet;
							TArray<FVector> Normals;
							TArray<FVector> VertexPositions;

							for (int32 TriangleID : VisibleTriangles)
							{
								if (bIgnoreOccludedTriangles && (TriangleDistances[TriangleID] > GetDistanceCutoff()))
								{
									continue;
								}

								const UE::Geometry::FIndex3i TriangleIndices = ResultMesh.GetTriangle(TriangleID);

								auto SetVertex = [&VerticesSet, &Normals, &VertexPositions, &NormalOverlay , &ResultMesh](const int32 VertexID, const int32 TriangleID) {
									if (!VerticesSet.Contains(VertexID))
									{
										VerticesSet.Add(VertexID);

										const FVector Normal = (FVector)NormalOverlay->GetElementAtVertex(TriangleID, VertexID);
										Normals.Add(Normal);

										const FVector VertexPosition = (FVector)ResultMesh.GetVertex(VertexID);
										VertexPositions.Add(VertexPosition);
									}
								};

								SetVertex(TriangleIndices.A, TriangleID);
								SetVertex(TriangleIndices.B, TriangleID);
								SetVertex(TriangleIndices.C, TriangleID);
							}

							PDI->AddReserveLines(SDPG_World, Normals.Num());

							for (int32 Idx = 0; Idx < Normals.Num(); ++Idx)
							{
								PDI->DrawLine(VertexPositions[Idx], VertexPositions[Idx] + GetVertexNormalLength() * Normals[Idx], GetVertexNormalColor().ToFColor(true), SDPG_World, GetVertexNormalThickness());
							}
						}
					}
				}

				if (bMeshFaceNormalsVisualizationEnabled && VisibleTriangles.Num() > 0)
				{
					const int32 NumTriangles = ResultMesh.TriangleCount();

					PDI->AddReserveLines(SDPG_World, NumTriangles);

					for (int32 TriangleID : VisibleTriangles)
					{
						if (bIgnoreOccludedTriangles && (TriangleDistances[TriangleID] > GetDistanceCutoff()))
						{
							continue;
						}

						const FVector TriangleCentroid = ResultMesh.GetTriCentroid(TriangleID);
						const FVector TriangleNormal = ResultMesh.GetTriNormal(TriangleID);

						PDI->DrawLine(TriangleCentroid, TriangleCentroid + GetFaceNormalLength() * TriangleNormal, GetFaceNormalColor().ToFColor(true), SDPG_World, GetFaceNormalThickness());
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE 


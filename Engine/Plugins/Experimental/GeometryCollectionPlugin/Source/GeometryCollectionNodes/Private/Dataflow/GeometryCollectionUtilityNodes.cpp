// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionUtilityNodes.h"
#include "Dataflow/DataflowCore.h"
#if WITH_EDITOR
#include "Dataflow/DataflowRenderingViewMode.h"
#endif

#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "GeometryCollection/Facades/CollectionHierarchyFacade.h"
#include "GeometryCollection/Facades/CollectionTransformSelectionFacade.h"
#include "Dataflow/DataflowDebugDrawInterface.h"
#include "Dataflow/DataflowDebugDraw.h"
#include "Dataflow/DataflowDynamicMeshDebugDrawMesh.h"
#include "FractureEngineConvex.h"
#include "DynamicMesh/DynamicMesh3.h"

#include "Operations/MeshSelfUnion.h"
#include "MeshQueries.h"

#include "MeshSimplification.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionUtilityNodes)

namespace UE::Dataflow
{

	void GeometryCollectionUtilityNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeDataflowConvexDecompositionSettingsNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateLeafConvexHullsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSimplifyConvexHullsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateNonOverlappingConvexHullsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateClusterConvexHullsFromLeafHullsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateClusterConvexHullsFromChildrenHullsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FClearConvexHullsDataflowNode);
		// Note: FCopyConvexHullsFromRootDataflowNode is temporarily disabled as we rework its functionality
		//DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCopyConvexHullsFromRootDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMergeConvexHullsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FUpdateVolumeAttributesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetConvexHullVolumeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFixTinyGeoDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRecomputeNormalsInGeometryCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FResampleGeometryCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FValidateGeometryCollectionDataflowNode);
	}
}

FMakeDataflowConvexDecompositionSettingsNode::FMakeDataflowConvexDecompositionSettingsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&MinSizeToDecompose).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MaxGeoToHullVolumeRatioToDecompose).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&ErrorTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MaxHullsPerGeometry).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinThicknessTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&NumAdditionalSplits).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&bProtectNegativeSpace).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&bOnlyConnectedToHull).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&NegativeSpaceTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&NegativeSpaceMinRadius).SetCanHidePin(true).SetPinIsHidden(true);

	RegisterOutputConnection(&DecompositionSettings);
}

void FMakeDataflowConvexDecompositionSettingsNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&DecompositionSettings))
	{
		FDataflowConvexDecompositionSettings OutSettings;
		OutSettings.MinSizeToDecompose = GetValue(Context, &MinSizeToDecompose);
		OutSettings.MaxGeoToHullVolumeRatioToDecompose = GetValue(Context, &MaxGeoToHullVolumeRatioToDecompose);
		OutSettings.ErrorTolerance = GetValue(Context, &ErrorTolerance);
		OutSettings.MaxHullsPerGeometry = GetValue(Context, &MaxHullsPerGeometry);
		OutSettings.MinThicknessTolerance = GetValue(Context, &MinThicknessTolerance);
		OutSettings.NumAdditionalSplits = GetValue(Context, &NumAdditionalSplits);
		OutSettings.bProtectNegativeSpace = GetValue(Context, &bProtectNegativeSpace);
		OutSettings.bOnlyConnectedToHull = GetValue(Context, &bOnlyConnectedToHull);
		OutSettings.NegativeSpaceTolerance = GetValue(Context, &NegativeSpaceTolerance);
		OutSettings.NegativeSpaceMinRadius = GetValue(Context, &NegativeSpaceMinRadius);

		SetValue(Context, OutSettings, &DecompositionSettings);
	}
}

/* --------------------------------------------------------------------------------------------------------------------------- */

namespace UE::Dataflow::Convex
{
	static FLinearColor GetRandomColor(const int32 RandomSeed, int32 Idx)
	{
		FRandomStream RandomStream(RandomSeed * 23 + Idx * 4078);

		const uint8 R = static_cast<uint8>(RandomStream.FRandRange(128, 255));
		const uint8 G = static_cast<uint8>(RandomStream.FRandRange(128, 255));
		const uint8 B = static_cast<uint8>(RandomStream.FRandRange(128, 255));

		return FLinearColor(FColor(R, G, B, 255));
	}

	static void DebugDrawProc(IDataflowDebugDrawInterface& DataflowRenderingInterface, const FManagedArrayCollection& InCollection, const bool bRandomizeColor, const int32 ColorRandomSeed, const FDataflowTransformSelection& Selection)
	{
		using namespace UE::Geometry;

		TArray<FDynamicMesh3> HullsMeshes;
		const bool bRestrictToSelection = (Selection.Num() > 0);
	
		UE::FractureEngine::Convex::GetConvexHullsAsDynamicMeshes(InCollection, HullsMeshes, bRestrictToSelection, Selection.AsArray());

		int32 Idx = 0;
		for (const FDynamicMesh3& Mesh : HullsMeshes)
		{
			FDynamicMeshDebugDrawMesh DebugdrawMesh(&Mesh);
			DataflowRenderingInterface.DrawMesh(DebugdrawMesh);

			if (bRandomizeColor)
			{
				DataflowRenderingInterface.SetColor(UE::Dataflow::Convex::GetRandomColor(ColorRandomSeed, Idx++));
			}
		}
	}

	static void SphereCoveringDebugDrawProc(IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDataflowSphereCovering& OutSpheres, const FDataflowNodeSphereCoveringDebugDrawSettings& SphereCoveringDebugDrawRenderSettings)
	{
		constexpr int32 CMaxNumberOfSpheres = 500;
		using namespace UE::Geometry;

		const int32 NumSpheres = OutSpheres.Spheres.Num();
		if (NumSpheres > 0)
		{
			DataflowRenderingInterface.SetLineWidth(SphereCoveringDebugDrawRenderSettings.LineWidthMultiplier);
			if (SphereCoveringDebugDrawRenderSettings.RenderType == EDataflowDebugDrawRenderType::Shaded)
			{
				DataflowRenderingInterface.SetShaded(true);
				DataflowRenderingInterface.SetTranslucent(SphereCoveringDebugDrawRenderSettings.bTranslucent);
				DataflowRenderingInterface.SetWireframe(true);
			}
			else
			{
				DataflowRenderingInterface.SetShaded(false);
				DataflowRenderingInterface.SetWireframe(true);
			}
			DataflowRenderingInterface.SetWorldPriority();
			DataflowRenderingInterface.SetColor(SphereCoveringDebugDrawRenderSettings.Color);

			int32 N = 1;
			if (NumSpheres > CMaxNumberOfSpheres)
			{
				N = NumSpheres / CMaxNumberOfSpheres;
			}

			float MinRadius = FLT_MAX, MaxRadius = -FLT_MAX;
			for (int32 Idx = 0; Idx < NumSpheres; ++Idx)
			{
				if (Idx % N == 0)
				{
					const float Radius = OutSpheres.Spheres.GetRadius(Idx);

					if (Radius < MinRadius)
					{
						MinRadius = Radius;
					}
					else if (Radius > MaxRadius)
					{
							MaxRadius = Radius;
					}
				}
			}

			for (int32 Idx = 0; Idx < NumSpheres; ++Idx)
			{
				if (Idx % N == 0)
				{
					if (SphereCoveringDebugDrawRenderSettings.ColorMethod == EDataflowSphereCoveringColorMethod::Random)
					{
						DataflowRenderingInterface.SetColor(UE::Dataflow::Convex::GetRandomColor(SphereCoveringDebugDrawRenderSettings.ColorRandomSeed + 7, Idx));
					}
					else if (SphereCoveringDebugDrawRenderSettings.ColorMethod == EDataflowSphereCoveringColorMethod::ColorByRadius)
					{
						float Progress = (OutSpheres.Spheres.GetRadius(Idx) - MinRadius) / (MaxRadius - MinRadius);
						FLinearColor Color = FLinearColor::LerpUsingHSV(SphereCoveringDebugDrawRenderSettings.ColorA, SphereCoveringDebugDrawRenderSettings.ColorB, Progress);
						DataflowRenderingInterface.SetColor(Color);
					}

					DataflowRenderingInterface.DrawSphere(OutSpheres.Spheres.GetCenter(Idx), OutSpheres.Spheres.GetRadius(Idx));
				}
			}
		}
	}
}

FCreateLeafConvexHullsDataflowNode::FCreateLeafConvexHullsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&OptionalSelectionFilter);
	RegisterInputConnection(&SimplificationDistanceThreshold).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&ConvexDecompositionSettings).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterOutputConnection(&Collection);
	RegisterOutputConnection(&SphereCovering);

	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

void FCreateLeafConvexHullsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection) || Out->IsA(&SphereCovering))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		FDataflowSphereCovering CombinedSphereCovering;
		if (InCollection.NumElements(FGeometryCollection::TransformGroup) == 0)
		{
			SetValue(Context, InCollection, &Collection);
			SetValue(Context, MoveTemp(CombinedSphereCovering), &SphereCovering);
			return;
		}

		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			TArray<int32> SelectedBones;
			bool bRestrictToSelection = false;
			if (IsConnected(&OptionalSelectionFilter))
			{
				const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue<FDataflowTransformSelection>(Context, &OptionalSelectionFilter);
				bRestrictToSelection = true;
				SelectedBones = InOptionalSelectionFilter.AsArray();
				GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(InCollection);
				SelectionFacade.Sanitize(SelectedBones, /* bFavorParent */false);
			}

			float InSimplificationDistanceThreshold = GetValue(Context, &SimplificationDistanceThreshold);

			FGeometryCollectionConvexUtility::FLeafConvexHullSettings LeafSettings(InSimplificationDistanceThreshold, GenerateMethod);
			LeafSettings.IntersectFilters.OnlyIntersectIfComputedIsSmallerFactor = IntersectIfComputedIsSmallerByFactor;
			LeafSettings.IntersectFilters.MinExternalVolumeToIntersect = MinExternalVolumeToIntersect;
			FDataflowConvexDecompositionSettings InDecompSettings = GetValue(Context, &ConvexDecompositionSettings);
			LeafSettings.DecompositionSettings.MaxGeoToHullVolumeRatioToDecompose = InDecompSettings.MaxGeoToHullVolumeRatioToDecompose;
			LeafSettings.DecompositionSettings.MinGeoVolumeToDecompose = InDecompSettings.MinSizeToDecompose * InDecompSettings.MinSizeToDecompose * InDecompSettings.MinSizeToDecompose;
			LeafSettings.DecompositionSettings.ErrorTolerance = InDecompSettings.ErrorTolerance;
			LeafSettings.DecompositionSettings.MaxHullsPerGeometry = InDecompSettings.MaxHullsPerGeometry;
			LeafSettings.DecompositionSettings.MinThicknessTolerance = InDecompSettings.MinThicknessTolerance;
			LeafSettings.DecompositionSettings.NumAdditionalSplits = InDecompSettings.NumAdditionalSplits;
			LeafSettings.DecompositionSettings.bProtectNegativeSpace = InDecompSettings.bProtectNegativeSpace;
			LeafSettings.DecompositionSettings.bOnlyConnectedToHull = InDecompSettings.bOnlyConnectedToHull;
			LeafSettings.DecompositionSettings.NegativeSpaceMinRadius = InDecompSettings.NegativeSpaceMinRadius;
			LeafSettings.DecompositionSettings.NegativeSpaceTolerance = InDecompSettings.NegativeSpaceTolerance;

			LeafSettings.bComputeIntersectionsBeforeHull = bComputeIntersectionsBeforeHull;
			TArray<FGeometryCollectionConvexUtility::FSphereCoveringInfo> SphereCoverings;
			FGeometryCollectionConvexUtility::GenerateLeafConvexHulls(*GeomCollection, bRestrictToSelection, SelectedBones, LeafSettings, &SphereCoverings);
			SetValue(Context, (const FManagedArrayCollection&)(*GeomCollection), &Collection);
			
			for (FGeometryCollectionConvexUtility::FSphereCoveringInfo& Info : SphereCoverings)
			{
				CombinedSphereCovering.Spheres.AppendTransformed(Info.SphereCovering, Info.Transform);
			}
			
			SetValue(Context, MoveTemp(CombinedSphereCovering), &SphereCovering);
		}
	}
}

#if WITH_EDITOR
bool FCreateLeafConvexHullsDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FCreateLeafConvexHullsDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (const FDataflowOutput* Output = FindOutput(&Collection))
		{
			const FManagedArrayCollection& InCollection = Output->GetValue(Context, Collection);
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue(Context, &OptionalSelectionFilter);
			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, InCollection, bRandomizeColor, ColorRandomSeed, InOptionalSelectionFilter);
		}

		if (SphereCoveringDebugDrawRenderSettings.bDisplaySphereCovering)
		{
			if (const FDataflowOutput* SphereCoveringOutput = FindOutput(&SphereCovering))
			{
				const FDataflowSphereCovering& OutSpheres = SphereCoveringOutput->GetValue(Context, SphereCovering);

				UE::Dataflow::Convex::SphereCoveringDebugDrawProc(DataflowRenderingInterface, OutSpheres, SphereCoveringDebugDrawRenderSettings);
			}
		}
	}
}
#endif

/* --------------------------------------------------------------------------------------------------------------------------- */

FSimplifyConvexHullsDataflowNode::FSimplifyConvexHullsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&OptionalSelectionFilter);
	RegisterInputConnection(&SimplificationAngleThreshold).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&SimplificationDistanceThreshold).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinTargetTriangleCount).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterOutputConnection(&Collection);

	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

void FSimplifyConvexHullsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection) && IsConnected(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue(Context, &Collection);
		if (InCollection.NumElements(FGeometryCollection::TransformGroup) == 0)
		{
			SetValue(Context, MoveTemp(InCollection), &Collection);
			return;
		}

		TArray<int32> SelectedBones;
		bool bRestrictToSelection = false;
		if (IsConnected(&OptionalSelectionFilter))
		{
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue(Context, &OptionalSelectionFilter);
			bRestrictToSelection = true;
			SelectedBones = InOptionalSelectionFilter.AsArray();
		}

		UE::FractureEngine::Convex::FSimplifyHullSettings Settings;
		Settings.SimplifyMethod = SimplifyMethod;
		Settings.ErrorTolerance = GetValue(Context, &SimplificationDistanceThreshold);
		Settings.AngleThreshold = GetValue(Context, &SimplificationAngleThreshold);
		Settings.bUseGeometricTolerance = true;
		Settings.bUseTargetTriangleCount = true;
		Settings.bUseExistingVertexPositions = bUseExistingVertices;
		Settings.TargetTriangleCount = GetValue(Context, &MinTargetTriangleCount);
		UE::FractureEngine::Convex::SimplifyConvexHulls(InCollection, Settings, bRestrictToSelection, SelectedBones);
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

#if WITH_EDITOR
bool FSimplifyConvexHullsDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FSimplifyConvexHullsDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (const FDataflowOutput* Output = FindOutput(&Collection))
		{
			const FManagedArrayCollection& OutCollection = Output->GetValue(Context, Collection);
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue(Context, &OptionalSelectionFilter);

			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, OutCollection, bRandomizeColor, ColorRandomSeed, InOptionalSelectionFilter);
		}
	}
}
#endif

/* --------------------------------------------------------------------------------------------------------------------------- */

FCreateNonOverlappingConvexHullsDataflowNode::FCreateNonOverlappingConvexHullsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&CanRemoveFraction).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&SimplificationDistanceThreshold).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&CanExceedFraction).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&OverlapRemovalShrinkPercent).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterOutputConnection(&Collection);

	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

void FCreateNonOverlappingConvexHullsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection) && IsConnected(&Collection))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			float InCanRemoveFraction = GetValue<float>(Context, &CanRemoveFraction);
			float InCanExceedFraction = GetValue<float>(Context, &CanExceedFraction);
			float InSimplificationDistanceThreshold = GetValue<float>(Context, &SimplificationDistanceThreshold);
			float InOverlapRemovalShrinkPercent = GetValue<float>(Context, &OverlapRemovalShrinkPercent);

			FGeometryCollectionConvexUtility::FGeometryCollectionConvexData ConvexData = FGeometryCollectionConvexUtility::CreateNonOverlappingConvexHullData(GeomCollection.Get(), 
				InCanRemoveFraction, 
				InSimplificationDistanceThreshold, 
				InCanExceedFraction,
				(EConvexOverlapRemoval)OverlapRemovalMethod,
				InOverlapRemovalShrinkPercent);

			SetValue<const FManagedArrayCollection&>(Context, *GeomCollection, &Collection);
		}
	}
}

#if WITH_EDITOR
bool FCreateNonOverlappingConvexHullsDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FCreateNonOverlappingConvexHullsDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (const FDataflowOutput* Output = FindOutput(&Collection))
		{
			const FManagedArrayCollection& InCollection = Output->GetValue(Context, Collection);
			FDataflowTransformSelection EmptySelection;

			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, InCollection, bRandomizeColor, ColorRandomSeed, EmptySelection);
		}
	}
}
#endif

/* --------------------------------------------------------------------------------------------------------------------------- */

// local helper to convert the dataflow enum
static UE::Geometry::FNegativeSpaceSampleSettings::ESampleMethod ConvertNegativeSpaceSampleMethodDataflowEnum(ENegativeSpaceSampleMethodDataflowEnum SampleMethod)
{
	switch (SampleMethod)
	{
	case ENegativeSpaceSampleMethodDataflowEnum::Uniform:
		return UE::Geometry::FNegativeSpaceSampleSettings::ESampleMethod::Uniform;
	case ENegativeSpaceSampleMethodDataflowEnum::VoxelSearch:
		return UE::Geometry::FNegativeSpaceSampleSettings::ESampleMethod::VoxelSearch;
	case ENegativeSpaceSampleMethodDataflowEnum::NavigableVoxelSearch:
		return UE::Geometry::FNegativeSpaceSampleSettings::ESampleMethod::NavigableVoxelSearch;
	}
	return UE::Geometry::FNegativeSpaceSampleSettings::ESampleMethod::Uniform;
}

FGenerateClusterConvexHullsFromLeafHullsDataflowNode::FGenerateClusterConvexHullsFromLeafHullsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&ConvexCount).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&ErrorTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&OptionalSelectionFilter);
	RegisterInputConnection(&bAllowMergingLeafHulls).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&bProtectNegativeSpace).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&TargetNumSamples).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinSampleSpacing).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&NegativeSpaceTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinRadius).SetCanHidePin(true).SetPinIsHidden(true);

	RegisterOutputConnection(&Collection);
	RegisterOutputConnection(&SphereCovering);

	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

void FGenerateClusterConvexHullsFromLeafHullsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection) || Out->IsA(&SphereCovering))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowSphereCovering Spheres;

		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			TArray<int32> SelectionArray;
			bool bHasSelectionFilter = IsConnected(&OptionalSelectionFilter);
			if (bHasSelectionFilter)
			{
				const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue<FDataflowTransformSelection>(Context, &OptionalSelectionFilter);
				SelectionArray = InOptionalSelectionFilter.AsArray();
				GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(InCollection);
				SelectionFacade.Sanitize(SelectionArray, /* bFavorParent */false);
			}

			bool bHasNegativeSpace = false;
			UE::Geometry::FSphereCovering NegativeSpace;
			if (GetValue(Context, &bProtectNegativeSpace))
			{
				UE::Geometry::FNegativeSpaceSampleSettings NegativeSpaceSettings;
				NegativeSpaceSettings.TargetNumSamples = GetValue(Context, &TargetNumSamples);
				NegativeSpaceSettings.MinRadius = GetValue(Context, &MinRadius);
				NegativeSpaceSettings.ReduceRadiusMargin = GetValue(Context, &NegativeSpaceTolerance);
				NegativeSpaceSettings.MinSpacing = GetValue(Context, &MinSampleSpacing);
				NegativeSpaceSettings.SampleMethod = ConvertNegativeSpaceSampleMethodDataflowEnum(SampleMethod);
				NegativeSpaceSettings.bRequireSearchSampleCoverage = bRequireSearchSampleCoverage;
				NegativeSpaceSettings.bOnlyConnectedToHull = bOnlyConnectedToHull;
				NegativeSpaceSettings.Sanitize();
				bHasNegativeSpace = UE::FractureEngine::Convex::ComputeConvexHullsNegativeSpace(*GeomCollection, NegativeSpace, NegativeSpaceSettings, bHasSelectionFilter, SelectionArray);
			}

			const int32 InConvexCount = GetValue(Context, &ConvexCount);
			const double InErrorToleranceInCm = GetValue(Context, &ErrorTolerance);
			FGeometryCollectionConvexUtility::FClusterConvexHullSettings HullMergeSettings(InConvexCount, InErrorToleranceInCm, bPreferExternalCollisionShapes);
			HullMergeSettings.AllowMergesMethod = AllowMerges;
			HullMergeSettings.bAllowMergingLeafHulls = GetValue(Context, &bAllowMergingLeafHulls);
			HullMergeSettings.EmptySpace = bHasNegativeSpace ? &NegativeSpace : nullptr;
			HullMergeSettings.ProximityFilter = MergeProximityFilter;
			HullMergeSettings.ProximityDistanceThreshold = MergeProximityDistanceThreshold;

			if (bHasSelectionFilter)
			{
				FGeometryCollectionConvexUtility::GenerateClusterConvexHullsFromLeafHulls(
					*GeomCollection,
					HullMergeSettings,
					SelectionArray
				);
			}
			else
			{
				FGeometryCollectionConvexUtility::GenerateClusterConvexHullsFromLeafHulls(
					*GeomCollection,
					HullMergeSettings
				);
			}

			SetValue(Context, static_cast<const FManagedArrayCollection>(*GeomCollection), &Collection);
			// Move the negative space to the output container at the end to be sure it is no longer needed
			Spheres.Spheres = MoveTemp(NegativeSpace);
		}
		else
		{
			UE_LOG(LogChaos, Error, TEXT("Error: Input collection could not be converted to a valid Geometry Collection"));
			SetValue(Context, InCollection, &Collection);
		}

		SetValue(Context, MoveTemp(Spheres), &SphereCovering);
	}
}

#if WITH_EDITOR
bool FGenerateClusterConvexHullsFromLeafHullsDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FGenerateClusterConvexHullsFromLeafHullsDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (const FDataflowOutput* Output = FindOutput(&Collection))
		{
			const FManagedArrayCollection& InCollection = Output->GetValue(Context, Collection);
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue<FDataflowTransformSelection>(Context, &OptionalSelectionFilter);

			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, InCollection, bRandomizeColor, ColorRandomSeed, InOptionalSelectionFilter);
		}

		if (SphereCoveringDebugDrawRenderSettings.bDisplaySphereCovering)
		{
			if (const FDataflowOutput* SphereCoveringOutput = FindOutput(&SphereCovering))
			{
				const FDataflowSphereCovering& OutSpheres = SphereCoveringOutput->GetValue(Context, SphereCovering);

				UE::Dataflow::Convex::SphereCoveringDebugDrawProc(DataflowRenderingInterface, OutSpheres, SphereCoveringDebugDrawRenderSettings);
			}
		}
	}
}
#endif

/* --------------------------------------------------------------------------------------------------------------------------- */

FGenerateClusterConvexHullsFromChildrenHullsDataflowNode::FGenerateClusterConvexHullsFromChildrenHullsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&ConvexCount).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&ErrorTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&OptionalSelectionFilter);
	RegisterInputConnection(&bAllowMergingLeafHulls).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&bProtectNegativeSpace).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&TargetNumSamples).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinSampleSpacing).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&NegativeSpaceTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinRadius).SetCanHidePin(true).SetPinIsHidden(true);

	RegisterOutputConnection(&Collection);
	RegisterOutputConnection(&SphereCovering);

	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

void FGenerateClusterConvexHullsFromChildrenHullsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection) || Out->IsA(&SphereCovering))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowSphereCovering Spheres;

		if (TUniquePtr<FGeometryCollection> GeomCollection = TUniquePtr<FGeometryCollection>(InCollection.NewCopy<FGeometryCollection>()))
		{
			TArray<int32> SelectionArray;
			bool bHasSelectionFilter = IsConnected(&OptionalSelectionFilter);
			if (bHasSelectionFilter)
			{
				const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue<FDataflowTransformSelection>(Context, &OptionalSelectionFilter);
				SelectionArray = InOptionalSelectionFilter.AsArray();
				GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(InCollection);
				SelectionFacade.Sanitize(SelectionArray, /* bFavorParent */false);
			}

			bool bHasNegativeSpace = false;
			UE::Geometry::FSphereCovering NegativeSpace;
			if (GetValue(Context, &bProtectNegativeSpace))
			{
				UE::Geometry::FNegativeSpaceSampleSettings NegativeSpaceSettings;
				NegativeSpaceSettings.TargetNumSamples = GetValue(Context, &TargetNumSamples);
				NegativeSpaceSettings.MinRadius = GetValue(Context, &MinRadius);
				NegativeSpaceSettings.ReduceRadiusMargin = GetValue(Context, &NegativeSpaceTolerance);
				NegativeSpaceSettings.MinSpacing = GetValue(Context, &MinSampleSpacing);
				NegativeSpaceSettings.SampleMethod = ConvertNegativeSpaceSampleMethodDataflowEnum(SampleMethod);
				NegativeSpaceSettings.bRequireSearchSampleCoverage = bRequireSearchSampleCoverage;
				NegativeSpaceSettings.bOnlyConnectedToHull = bOnlyConnectedToHull;
				NegativeSpaceSettings.Sanitize();
				bHasNegativeSpace = UE::FractureEngine::Convex::ComputeConvexHullsNegativeSpace(*GeomCollection, NegativeSpace, NegativeSpaceSettings, bHasSelectionFilter, SelectionArray);
			}

			const int32 InConvexCount = GetValue(Context, &ConvexCount);
			const double InErrorToleranceInCm = GetValue(Context, &ErrorTolerance);
			FGeometryCollectionConvexUtility::FClusterConvexHullSettings HullMergeSettings(InConvexCount, InErrorToleranceInCm, bPreferExternalCollisionShapes);
			HullMergeSettings.AllowMergesMethod = EAllowConvexMergeMethod::Any; // Note: Only 'Any' is supported for this node currently
			HullMergeSettings.EmptySpace = bHasNegativeSpace ? &NegativeSpace : nullptr;
			HullMergeSettings.bAllowMergingLeafHulls = GetValue(Context, &bAllowMergingLeafHulls);
			HullMergeSettings.ProximityFilter = MergeProximityFilter;
			HullMergeSettings.ProximityDistanceThreshold = MergeProximityDistanceThreshold;

			if (bHasSelectionFilter)
			{
				FGeometryCollectionConvexUtility::GenerateClusterConvexHullsFromChildrenHulls(
					*GeomCollection,
					HullMergeSettings,
					SelectionArray
				);
			}
			else
			{
				FGeometryCollectionConvexUtility::GenerateClusterConvexHullsFromChildrenHulls(
					*GeomCollection,
					HullMergeSettings
				);
			}

			SetValue<const FManagedArrayCollection&>(Context, *GeomCollection, &Collection);
			// Move the negative space to the output container at the end to be sure it is no longer needed
			Spheres.Spheres = MoveTemp(NegativeSpace);
		}
		else
		{
			UE_LOG(LogChaos, Error, TEXT("Error: Input collection could not be converted to a valid Geometry Collection"));
			SetValue(Context, InCollection, &Collection);
		}
		
		SetValue(Context, MoveTemp(Spheres), &SphereCovering);
	}
}

#if WITH_EDITOR
bool FGenerateClusterConvexHullsFromChildrenHullsDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FGenerateClusterConvexHullsFromChildrenHullsDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (const FDataflowOutput* Output = FindOutput(&Collection))
		{
			const FManagedArrayCollection& InCollection = Output->GetValue(Context, Collection);
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue(Context, &OptionalSelectionFilter);

			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, InCollection, bRandomizeColor, ColorRandomSeed, InOptionalSelectionFilter);
		}

		if (SphereCoveringDebugDrawRenderSettings.bDisplaySphereCovering)
		{
			if (const FDataflowOutput* SphereCoveringOutput = FindOutput(&SphereCovering))
			{
				const FDataflowSphereCovering& OutSpheres = SphereCoveringOutput->GetValue(Context, SphereCovering);

				UE::Dataflow::Convex::SphereCoveringDebugDrawProc(DataflowRenderingInterface, OutSpheres, SphereCoveringDebugDrawRenderSettings);
			}
		}
	}
}
#endif

/* --------------------------------------------------------------------------------------------------------------------------- */

FMergeConvexHullsDataflowNode::FMergeConvexHullsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&MaxConvexCount).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&ErrorTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&OptionalSelectionFilter);
	RegisterInputConnection(&bProtectNegativeSpace).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&TargetNumSamples).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinSampleSpacing).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&NegativeSpaceTolerance).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&MinRadius).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterOutputConnection(&Collection);
	RegisterOutputConnection(&SphereCovering);

	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

#if WITH_EDITOR
bool FCopyConvexHullsFromRootDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FCopyConvexHullsFromRootDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (const FDataflowOutput* Output = FindOutput(&Collection))
		{
			const FManagedArrayCollection& InCollection = Output->GetValue(Context, Collection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectRootBones();

			FDataflowTransformSelection RootSelection;
			RootSelection.Initialize(InCollection.NumElements(FTransformCollection::TransformGroup), false);
			RootSelection.SetFromArray(SelectionArr);

			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, InCollection, bRandomizeColor, ColorRandomSeed, RootSelection);
		}
	}
}
#endif


FCopyConvexHullsFromRootDataflowNode::FCopyConvexHullsFromRootDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: Super(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&FromCollection);
	RegisterInputConnection(&bSkipIfEmpty).SetCanHidePin(true).SetPinIsHidden(true);

	RegisterOutputConnection(&Collection, &Collection);
	
	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

void FCopyConvexHullsFromRootDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue(Context, &Collection);

		if (IsConnected(&Collection) && IsConnected(&FromCollection))
		{
			const FManagedArrayCollection& InFromCollection = GetValue(Context, &FromCollection);
			const bool bInSkipIfEmpty = GetValue(Context, &bSkipIfEmpty);

			if (FGeometryCollectionConvexUtility::HasConvexHullData(&InFromCollection))
			{
				GeometryCollection::Facades::FCollectionTransformSelectionFacade ToTransformSelectionFacade(InCollection);
				const TArray<int32> ToRoots = ToTransformSelectionFacade.SelectRootBones();
				GeometryCollection::Facades::FCollectionTransformSelectionFacade FromTransformSelectionFacade(InFromCollection);
				const TArray<int32> FromRoots = FromTransformSelectionFacade.SelectRootBones();
				if (ToRoots.Num() != FromRoots.Num())
				{
					UE_LOG(LogChaosDataflow, Warning, TEXT("Failed to copy root collision across collections with different number of root nodes (%d vs %d)"), ToRoots.Num(), FromRoots.Num());
				}
				else
				{
					FGeometryCollectionConvexUtility::CopyConvexHulls(InCollection, ToRoots, InFromCollection, FromRoots, bInSkipIfEmpty);
				}
			}
			else
			{
				if (!bInSkipIfEmpty)
				{
					GeometryCollection::Facades::FCollectionTransformSelectionFacade ToTransformSelectionFacade(InCollection);
					const TArray<int32> ToRoots = ToTransformSelectionFacade.SelectRootBones();
					FGeometryCollectionConvexUtility::RemoveConvexHulls(&InCollection, ToRoots);
				}
			}
		}
		
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FClearConvexHullsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue(Context, &Collection);

		if (!IsConnected(&Collection) || !FGeometryCollectionConvexUtility::HasConvexHullData(&InCollection))
		{
			SetValue(Context, MoveTemp(InCollection), &Collection);
			return;
		}

		GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(InCollection);

		TArray<int32> ToClear;
		if (IsConnected(&TransformSelection))
		{ 
			const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);
			ToClear = InTransformSelection.AsArray();

			SelectionFacade.Sanitize(ToClear, /* bFavorParent */false);
		}
		else
		{
			ToClear = SelectionFacade.SelectAll();
		}

		FGeometryCollectionConvexUtility::RemoveConvexHulls(&InCollection, ToClear);
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FMergeConvexHullsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection) || Out->IsA(&SphereCovering))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FDataflowSphereCovering Spheres;

		TArray<int32> SelectionArray;
		bool bHasSelectionFilter = IsConnected(&OptionalSelectionFilter);
		if (bHasSelectionFilter)
		{
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue<FDataflowTransformSelection>(Context, &OptionalSelectionFilter);
			SelectionArray = InOptionalSelectionFilter.AsArray();
			GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(InCollection);
			SelectionFacade.Sanitize(SelectionArray, /* bFavorParent */false);
		}

		bool bHasPrecomputedNegativeSpace = false;
		UE::Geometry::FSphereCovering NegativeSpace;
		bool bInProtectNegativeSpace = GetValue(Context, &bProtectNegativeSpace);
		UE::Geometry::FNegativeSpaceSampleSettings NegativeSpaceSettings;
		if (bInProtectNegativeSpace)
		{
			NegativeSpaceSettings.TargetNumSamples = GetValue(Context, &TargetNumSamples);
			NegativeSpaceSettings.MinRadius = GetValue(Context, &MinRadius);
			NegativeSpaceSettings.ReduceRadiusMargin = GetValue(Context, &NegativeSpaceTolerance);
			NegativeSpaceSettings.MinSpacing = GetValue(Context, &MinSampleSpacing);
			NegativeSpaceSettings.SampleMethod = ConvertNegativeSpaceSampleMethodDataflowEnum(SampleMethod);
			NegativeSpaceSettings.bRequireSearchSampleCoverage = bRequireSearchSampleCoverage;
			NegativeSpaceSettings.bOnlyConnectedToHull = bOnlyConnectedToHull;
			NegativeSpaceSettings.Sanitize();
		}
		if (bInProtectNegativeSpace && !bComputeNegativeSpacePerBone)
		{
			bHasPrecomputedNegativeSpace = UE::FractureEngine::Convex::ComputeConvexHullsNegativeSpace(InCollection, NegativeSpace, NegativeSpaceSettings, bHasSelectionFilter, SelectionArray, false);
		}

		const int32 InMaxConvexCount = GetValue(Context, &MaxConvexCount);
		const double InErrorToleranceInCm = GetValue(Context, &ErrorTolerance);
		FGeometryCollectionConvexUtility::FMergeConvexHullSettings HullMergeSettings;
		HullMergeSettings.EmptySpace = bHasPrecomputedNegativeSpace ? &NegativeSpace : nullptr;
		HullMergeSettings.ErrorToleranceInCm = InErrorToleranceInCm;
		HullMergeSettings.MaxConvexCount = InMaxConvexCount;
		HullMergeSettings.ComputeEmptySpacePerBoneSettings = (bInProtectNegativeSpace && bComputeNegativeSpacePerBone) ? &NegativeSpaceSettings : nullptr;
		HullMergeSettings.ProximityFilter = MergeProximityFilter;
		HullMergeSettings.ProximityDistanceThreshold = MergeProximityDistanceThreshold;

		UE::Geometry::FSphereCovering UsedNegativeSpace;
		FGeometryCollectionConvexUtility::MergeHullsOnTransforms(InCollection, HullMergeSettings, bHasSelectionFilter, SelectionArray, &UsedNegativeSpace);

		SetValue(Context, MoveTemp(InCollection), &Collection);

		Spheres.Spheres = MoveTemp(UsedNegativeSpace);
		SetValue(Context, MoveTemp(Spheres), &SphereCovering);
	}
}

#if WITH_EDITOR
bool FMergeConvexHullsDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FMergeConvexHullsDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (const FDataflowOutput* Output = FindOutput(&Collection))
		{
			const FManagedArrayCollection& InCollection = Output->GetValue(Context, Collection);
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue<FDataflowTransformSelection>(Context, &OptionalSelectionFilter);

			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, InCollection, bRandomizeColor, ColorRandomSeed, InOptionalSelectionFilter);
		}

		if (SphereCoveringDebugDrawRenderSettings.bDisplaySphereCovering)
		{
			if (const FDataflowOutput* SphereCoveringOutput = FindOutput(&SphereCovering))
			{
				const FDataflowSphereCovering& OutSpheres = SphereCoveringOutput->GetValue(Context, SphereCovering);

				UE::Dataflow::Convex::SphereCoveringDebugDrawProc(DataflowRenderingInterface, OutSpheres, SphereCoveringDebugDrawRenderSettings);
			}
		}
	}
}
#endif

/* --------------------------------------------------------------------------------------------------------------------------- */

FUpdateVolumeAttributesDataflowNode::FUpdateVolumeAttributesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection);
}

void FUpdateVolumeAttributesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue(Context, &Collection);
		if (InCollection.NumElements(FGeometryCollection::TransformGroup) > 0)
		{
			FGeometryCollectionConvexUtility::SetVolumeAttributes(&InCollection);
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

/* --------------------------------------------------------------------------------------------------------------------------- */

FGetConvexHullVolumeDataflowNode::FGetConvexHullVolumeDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&TransformSelection);
	RegisterOutputConnection(&Volume);

	DebugDrawRenderSettings.RenderType = EDataflowDebugDrawRenderType::Wireframe;
	DebugDrawRenderSettings.Color = FLinearColor::Green;
	DebugDrawRenderSettings.LineWidthMultiplier = 2.0;
}

void FGetConvexHullVolumeDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Volume))
	{
		float VolumeSum = 0;

		if (!IsConnected(&Collection) || !IsConnected(&TransformSelection))
		{
			SetValue(Context, VolumeSum, &Volume);
			return;
		}

		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		const FDataflowTransformSelection& InSelection = GetValue(Context, &TransformSelection);

		if (!FGeometryCollectionConvexUtility::HasConvexHullData(&InCollection))
		{
			SetValue(Context, VolumeSum, &Volume);
			return;
		}

		const int32 NumTransforms = InCollection.NumElements(FGeometryCollection::TransformGroup);
		TArray<int32> SelectionToSum = InSelection.AsArray();
		GeometryCollection::Facades::FCollectionTransformSelectionFacade SelectionFacade(InCollection);
		SelectionFacade.Sanitize(SelectionToSum);
		if (NumTransforms == 0 || SelectionToSum.Num() == 0)
		{
			SetValue(Context, VolumeSum, &Volume);
			return;
		}

		const TManagedArray<TSet<int32>>& TransformToConvexIndices = InCollection.GetAttribute<TSet<int32>>("TransformToConvexIndices", FTransformCollection::TransformGroup);
		const TManagedArray<Chaos::FConvexPtr>& ConvexHulls = InCollection.GetAttribute<Chaos::FConvexPtr>(FGeometryCollection::ConvexHullAttribute, FGeometryCollection::ConvexGroup);

		Chaos::Facades::FCollectionHierarchyFacade HierarchyFacade(InCollection);

		auto IterateHulls = [this, &TransformToConvexIndices, &HierarchyFacade](TArray<int32>& SelectionToSum, TFunctionRef<void(int32)> ProcessFn)
		{
			while (!SelectionToSum.IsEmpty())
			{
				int32 TransformIdx = SelectionToSum.Pop(EAllowShrinking::No);
				if (!bSumChildrenForClustersWithoutHulls || !TransformToConvexIndices[TransformIdx].IsEmpty())
				{
					ProcessFn(TransformIdx);
				}
				else if (const TSet<int32>* Children = HierarchyFacade.FindChildren(TransformIdx))
				{
					SelectionToSum.Append(Children->Array());
				}
			}
		};

		if (!bVolumeOfUnion)
		{
			IterateHulls(SelectionToSum, [&VolumeSum, &ConvexHulls, &TransformToConvexIndices](int32 TransformIdx)
				{
					for (int32 ConvexIdx : TransformToConvexIndices[TransformIdx])
					{
						VolumeSum += ConvexHulls[ConvexIdx]->GetVolume();
					}
				});
		}
		else
		{
			TArray<int32> SelectedBones;
			SelectedBones.Reserve(SelectionToSum.Num());
			IterateHulls(SelectionToSum, [&SelectedBones](int32 TransformIdx)
				{
					SelectedBones.Add(TransformIdx);
				});
			UE::Geometry::FDynamicMesh3 Mesh;
			UE::FractureEngine::Convex::GetConvexHullsAsDynamicMesh(InCollection, Mesh, true, SelectedBones);
			UE::Geometry::FMeshSelfUnion Union(&Mesh);
			// Disable quality-related features, since we just want the volume
			Union.TryToImproveTriQualityThreshold = -1;
			Union.bWeldSharedEdges = false;
			Union.Compute();
			VolumeSum = UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeNonWatertight(Mesh);
		}
		
		SetValue(Context, VolumeSum, &Volume);
	}
}

#if WITH_EDITOR
bool FGetConvexHullVolumeDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FGetConvexHullVolumeDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	using namespace UE::Geometry;

	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		if (IsConnected(&Collection))
		{
			const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
			const FDataflowTransformSelection& InSelection = GetValue(Context, &TransformSelection);

			UE::Dataflow::Convex::DebugDrawProc(DataflowRenderingInterface, InCollection, bRandomizeColor, ColorRandomSeed, InSelection);
		}
	}
}
#endif

/* --------------------------------------------------------------------------------------------------------------------------- */

void FFixTinyGeoDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		FDataflowTransformSelection InTransformSelection = GetValue(Context, &TransformSelection);
		//
		// If not connected select everything by default
		//
		if (!IsConnected(&TransformSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectAll();

			FDataflowTransformSelection NewTransformSelection;
			NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
			NewTransformSelection.SetFromArray(SelectionArr);

			InTransformSelection = NewTransformSelection;
		}

		if (InTransformSelection.AnySelected())
		{
			FManagedArrayCollection InCollection = GetValue(Context, &Collection);

			FFractureEngineUtility::FixTinyGeo(InCollection,
				InTransformSelection,
				MergeType,
				bOnFractureLevel,
				SelectionMethod,
				MinVolumeCubeRoot,
				RelativeVolume,
				UseBoneSelection,
				bOnlyClusters,
				NeighborSelection,
				bOnlyToConnected,
				bOnlySameParent,
				bUseCollectionProximityForConnections);

			SetValue(Context, MoveTemp(InCollection), &Collection);

			return;
		}

		SafeForwardInput(Context, &Collection, &Collection);
	}
}

void FRecomputeNormalsInGeometryCollectionDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		FDataflowTransformSelection InTransformSelection = GetValue(Context, &TransformSelection);
		//
		// If not connected select everything by default
		//
		if (!IsConnected(&TransformSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectAll();

			FDataflowTransformSelection NewTransformSelection;
			NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
			NewTransformSelection.SetFromArray(SelectionArr);

			InTransformSelection = NewTransformSelection;
		}

		if (InTransformSelection.AnySelected())
		{
			FManagedArrayCollection InCollection = GetValue(Context, &Collection);

			FFractureEngineUtility::RecomputeNormalsInGeometryCollection(InCollection,
				InTransformSelection,
				bOnlyTangents,
				bRecomputeSharpEdges,
				SharpEdgeAngleThreshold,
				bOnlyInternalSurfaces);

			SetValue(Context, MoveTemp(InCollection), &Collection);

			return;
		}

		SafeForwardInput(Context, &Collection, &Collection);
	}
}

void FResampleGeometryCollectionDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		FDataflowTransformSelection InTransformSelection = GetValue(Context, &TransformSelection);
		//
		// If not connected select everything by default
		//
		if (!IsConnected(&TransformSelection))
		{
			const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);

			GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);
			const TArray<int32>& SelectionArr = TransformSelectionFacade.SelectAll();

			FDataflowTransformSelection NewTransformSelection;
			NewTransformSelection.Initialize(InCollection.NumElements(FGeometryCollection::TransformGroup), false);
			NewTransformSelection.SetFromArray(SelectionArr);

			InTransformSelection = NewTransformSelection;
		}

		if (InTransformSelection.AnySelected())
		{
			FManagedArrayCollection InCollection = GetValue(Context, &Collection);

			FFractureEngineUtility::ResampleGeometryCollection(InCollection,
				InTransformSelection,
				GetValue(Context, &CollisionSampleSpacing));

			SetValue(Context, MoveTemp(InCollection), &Collection);

			return;
		}

		SafeForwardInput(Context, &Collection, &Collection);
	}
}

void FValidateGeometryCollectionDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue(Context, &Collection);

		FFractureEngineUtility::ValidateGeometryCollection(InCollection,
			bRemoveUnreferencedGeometry,
			bRemoveClustersOfOne,
			bRemoveDanglingClusters);

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

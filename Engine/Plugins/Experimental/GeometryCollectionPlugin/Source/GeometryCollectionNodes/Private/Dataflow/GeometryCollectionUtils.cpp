// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionUtils.h"

#include "GeometryCollection/GeometryCollectionAlgo.h"

namespace UE::Dataflow
{
	namespace Utils
	{
#if WITH_EDITOR
		void DebugDrawProximity(IDataflowDebugDrawInterface& DataflowRenderingInterface,
			const FManagedArrayCollection& Collection,
			const FLinearColor Color,
			const float LineWidthMultiplier,
			const float CenterSize,
			const FLinearColor CenterColor,
			const bool bRandomizeColor,
			const int32 ColorRandomSeed)
		{
			DataflowRenderingInterface.SetLineWidth(LineWidthMultiplier);
			DataflowRenderingInterface.SetWireframe(true);
			DataflowRenderingInterface.SetWorldPriority();
			DataflowRenderingInterface.SetPointSize(CenterSize);

			if (Collection.HasAttribute("Proximity", FGeometryCollection::GeometryGroup) &&
				Collection.HasAttribute("BoundingBox", FGeometryCollection::GeometryGroup))
			{
				const TManagedArray<TSet<int32>>& Proximity = Collection.GetAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup);
				const TManagedArray<FBox>& BoundingBox = Collection.GetAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);
				const TManagedArray<FTransform3f>* Transform = Collection.FindAttributeTyped<FTransform3f>(FGeometryCollection::TransformAttribute, FGeometryCollection::TransformGroup);
				const TManagedArray<int32>* Parent = Collection.FindAttributeTyped<int32>(FGeometryCollection::ParentAttribute, FGeometryCollection::TransformGroup);
				const TManagedArray<int32>* TransformIndex = Collection.FindAttributeTyped<int32>(FGeometryCollection::TransformIndexAttribute, FGeometryCollection::GeometryGroup);
				TArray<FTransform> GlobalTransformArray;
				if (Transform && Parent && TransformIndex)
				{
					GeometryCollectionAlgo::GlobalMatrices(*Transform, *Parent, GlobalTransformArray);
				}
				auto TransformFromGeometry = [&GlobalTransformArray, &TransformIndex](int32 GeometryIdx, FVector Point) -> FVector
				{
					if (GlobalTransformArray.IsEmpty() || !TransformIndex || !TransformIndex->IsValidIndex(GeometryIdx))
					{
						return Point;
					}
					else
					{
						return GlobalTransformArray[(*TransformIndex)[GeometryIdx]].TransformPosition(Point);
					}
				};

				const int32 NumGeometry = Collection.NumElements(FGeometryCollection::GeometryGroup);
				TArray<FVector> Centers;
				for (int32 Idx = 0; Idx < NumGeometry; ++Idx)
				{
					if (Proximity[Idx].Num() > 0)
					{
						const FVector Center = TransformFromGeometry(Idx, BoundingBox[Idx].GetCenter());

						Centers.Add(Center);

						if (bRandomizeColor)
						{
							DataflowRenderingInterface.SetColor(UE::Dataflow::Color::GetRandomColor(ColorRandomSeed + 17, Idx));
						}
						else
						{
							DataflowRenderingInterface.SetColor(Color);
						}

						for (int32 IdxOther : Proximity[Idx])
						{
							const FVector CenterOther = TransformFromGeometry(IdxOther, BoundingBox[IdxOther].GetCenter());

							DataflowRenderingInterface.DrawLine(Center, CenterOther);
						}

						DataflowRenderingInterface.SetPointSize(CenterSize);
						DataflowRenderingInterface.SetColor(CenterColor);
						for (FVector& Point : Centers)
						{
							DataflowRenderingInterface.DrawPoint(Point);
						}

						Centers.Reset();
					}
				}
			}
		}
#endif
	}
}

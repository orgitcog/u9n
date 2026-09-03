// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionMakeNodes.h"
#include "Dataflow/DataflowCore.h"
#if WITH_EDITOR
#include "Dataflow/DataflowRenderingViewMode.h"
#endif

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/Facades/CollectionMeshFacade.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineUtility.h"
#include "GeometryCollection/GeometryCollectionEngineRemoval.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealTypePrivate.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshAttributes.h"
#include "DynamicMeshEditor.h"
#include "Operations/MeshBoolean.h"
#include "Materials/Material.h"

#include "EngineGlobals.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "Voronoi/Voronoi.h"
#include "PlanarCut.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "FractureEngineClustering.h"
#include "FractureEngineSelection.h"
#include "GeometryCollection/Facades/CollectionBoundsFacade.h"
#include "GeometryCollection/Facades/CollectionAnchoringFacade.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "GeometryCollection/Facades/CollectionTransformFacade.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Dataflow/DataflowDebugDraw.h"
#include "Dataflow/DataflowSimpleDebugDrawMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generators/SphereGenerator.h"
#include "Generators/BoxSphereGenerator.h"
#include "Generators/CapsuleGenerator.h"
#include "Generators/SweepGenerator.h"
#include "Generators/GridBoxMeshGenerator.h"
#include "Generators/DiscMeshGenerator.h"
#include "Generators/StairGenerator.h"
#include "Generators/RectangleMeshGenerator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionMakeNodes)

namespace UE::Dataflow
{
	void GeometryCollectionMakeNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralStringDataflowNode_v2);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakePointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeBoxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeSphereDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralFloatDataflowNode_v2);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralDoubleDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralIntDataflowNode_v2);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralBoolDataflowNode_v2);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeQuaternionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeFloatArrayDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeCollectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeRotatorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBreakTransformDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralStringDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralIntDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeLiteralBoolDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeSphereMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeCapsuleMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeCylinderMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeBoxMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakePlaneDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeDiscMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeStairMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeRectangleMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeTorusMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeTransformDataflowNode);

		// Deprecated
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeTransformDataflowNode_v2);
	}
}

void FMakeLiteralStringDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		SetValue(Context, Value, &String);
	}
}

void FMakeLiteralStringDataflowNode_v2::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FString>(&String))
	{
		SetValue(Context, String, &String);
	}
}

/*--------------------------------------------------------------------------------------------------------*/

#if WITH_EDITOR
bool FMakePointsDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FMakePointsDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		FVector Min(UE_BIG_NUMBER);
		FVector Max(-UE_BIG_NUMBER);

		// Compute (Min, Max) of BoundingBox
		for (const FVector& Pt : Point)
		{
			Min = FVector::Min(Pt, Min);
			Max = FVector::Max(Pt, Max);
		}

		DataflowRenderingInterface.SetLineWidth(1.0);
		DataflowRenderingInterface.SetWireframe(true);
		DataflowRenderingInterface.SetWorldPriority();
		DataflowRenderingInterface.SetColor(FLinearColor::Gray);
		
		DataflowRenderingInterface.DrawBox(0.5 * FVector(Max - Min), FQuat::Identity, 0.5 * FVector(Min + Max), 1.0);
	}
}
#endif

void FMakePointsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&Points))
	{
		SetValue(Context, Point, &Points);
	}
}

/*--------------------------------------------------------------------------------------------------------*/

void FMakeBoxDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FBox>(&Box))
	{
		if (DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax)
		{
			FVector MinVal = GetValue<FVector>(Context, &Min);
			FVector MaxVal = GetValue<FVector>(Context, &Max);

			SetValue(Context, FBox(MinVal, MaxVal), &Box);
		}
		else if (DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize)
		{
			FVector CenterVal = GetValue<FVector>(Context, &Center);
			FVector SizeVal = GetValue<FVector>(Context, &Size);

			SetValue(Context, FBox(CenterVal - 0.5 * SizeVal, CenterVal + 0.5 * SizeVal), &Box);
		}
	}
}

/*--------------------------------------------------------------------------------------------------------*/

void FMakeSphereDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FSphere>(&Sphere))
	{
		FVector CenterVal = GetValue<FVector>(Context, &Center);
		float RadiusVal = GetValue<float>(Context, &Radius);

		SetValue(Context, FSphere(CenterVal, RadiusVal), &Sphere);
	}
}

#if WITH_EDITOR
bool FMakeSphereDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FMakeSphereDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		FVector Min = Center - FVector(Radius);
		FVector Max = Center + FVector(Radius);

		DataflowRenderingInterface.SetLineWidth(1.0);
		DataflowRenderingInterface.SetWireframe(true);
		DataflowRenderingInterface.SetWorldPriority();
		DataflowRenderingInterface.SetColor(FLinearColor::Gray);

		DataflowRenderingInterface.DrawBox(0.5 * FVector(Max - Min), FQuat::Identity, 0.5 * FVector(Min + Max), 1.0);
	}
}
#endif

/*--------------------------------------------------------------------------------------------------------*/

void FMakeLiteralFloatDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Float))
	{
		SetValue(Context, Value, &Float);
	}
}

void FMakeLiteralFloatDataflowNode_v2::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Float))
	{
		SetValue(Context, Float, &Float);
	}
}

//-----------------------------------------------------------------------------------------------

FMakeLiteralDoubleDataflowNode::FMakeLiteralDoubleDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Double);
}

void FMakeLiteralDoubleDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Double))
	{
		SetValue(Context, Double, &Double);
	}
}

//-----------------------------------------------------------------------------------------------

void FMakeLiteralIntDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Int))
	{
		SetValue(Context, Value, &Int);
	}
}

void FMakeLiteralIntDataflowNode_v2::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&Int))
	{
		SetValue(Context, Int, &Int);
	}
}

void FMakeLiteralBoolDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<bool>(&Bool))
	{
		SetValue(Context, Value, &Bool);
	}
}

void FMakeLiteralBoolDataflowNode_v2::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<bool>(&Bool))
	{
		SetValue(Context, Bool, &Bool);
	}
}

void FMakeLiteralVectorDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&Vector))
	{
		const FVector Value(GetValue<float>(Context, &X, X), GetValue<float>(Context, &Y, Y), GetValue<float>(Context, &Z, Z));
		SetValue(Context, Value, &Vector);
	}
}

void FMakeTransformDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FTransform>(&OutTransform))
	{
		SetValue(Context,
			FTransform(FQuat::MakeFromEuler(GetValue<FVector>(Context, &InRotation))
				, GetValue<FVector>(Context, &InTranslation)
				, GetValue<FVector>(Context, &InScale))
			, &OutTransform);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeTransformDataflowNode_v2::FMakeTransformDataflowNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Translation);
	RegisterInputConnection(&Rotation);
	RegisterInputConnection(&Rotator) .SetCanHidePin(true) .SetPinIsHidden(true);
	RegisterInputConnection(&Quat).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&Scale);
	RegisterOutputConnection(&Transform);
}

void FMakeTransformDataflowNode_v2::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FTransform>(&Transform))
	{
		const FVector InTranslation = GetValue(Context, &Translation);
		const FVector InScale = GetValue(Context, &Scale);
		FQuat OutQuat;
		if (IsConnected(&Rotation))
		{
			const FVector InRotation = GetValue(Context, &Rotation);
			OutQuat = FQuat::MakeFromEuler(InRotation);
		}
		else if (IsConnected(&Rotator))
		{
			const FRotator InRotator = GetValue(Context, &Rotator);
			OutQuat = FQuat::MakeFromRotator(InRotator);
		}
		else if (IsConnected(&Quat))
		{
			const FQuat InQuat = GetValue(Context, &Quat);
			OutQuat = InQuat;
		}

		FTransform OutTransform = FTransform(OutQuat, InTranslation, InScale);
		SetValue(Context, OutTransform, &Transform);
	}
}

/* -------------------------------------------------------------------------------- */

void FMakeQuaternionDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FQuat>(&Quaternion))
	{
		const FQuat Value(GetValue<float>(Context, &X, X), GetValue<float>(Context, &Y, Y), GetValue<float>(Context, &Z, Z), GetValue<float>(Context, &W, W));
		SetValue(Context, Value, &Quaternion);
	}
}

void FMakeFloatArrayDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&FloatArray))
	{
		const int32 InNumElements = GetValue(Context, &NumElements);
		const float InValue = GetValue(Context, &Value);

		TArray<float> OutFloatArray;
		OutFloatArray.Init(InValue, InNumElements);

		SetValue(Context, OutFloatArray, &FloatArray);
	}
}

void FMakeCollectionDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Collection))
	{
		SetValue(Context, FManagedArrayCollection(), &Collection);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeRotatorDataflowNode::FMakeRotatorDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Pitch);
	RegisterInputConnection(&Yaw);
	RegisterInputConnection(&Roll);
	RegisterOutputConnection(&Rotator);
}

void FMakeRotatorDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Rotator))
	{
		const float InPitch = GetValue(Context, &Pitch);
		const float InYaw = GetValue(Context, &Yaw);
		const float InRoll = GetValue(Context, &Roll);
		SetValue(Context, FRotator(InPitch, InYaw, InRoll), &Rotator);
	}
}

/* -------------------------------------------------------------------------------- */

FBreakTransformDataflowNode::FBreakTransformDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Transform);
	RegisterOutputConnection(&Translation);
	RegisterOutputConnection(&Rotation);
	RegisterOutputConnection(&Rotator);
	RegisterOutputConnection(&Quat);
	RegisterOutputConnection(&Scale);
}

void FBreakTransformDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Translation) ||
		Out->IsA(&Rotation) ||
		Out->IsA(&Rotator) ||
		Out->IsA(&Quat) ||
		Out->IsA(&Scale))
	{
		const FTransform InTransform = GetValue(Context, &Transform);

		const FVector OutTranslation = InTransform.GetTranslation();
		const FVector OutRotationAsEuler = InTransform.GetRotation().Euler();
		const FRotator OutRotator = InTransform.GetRotation().Rotator();
		const FQuat OutQuat = InTransform.GetRotation();
		const FVector OutScale = InTransform.GetScale3D();

		SetValue(Context, OutTranslation, &Translation);
		SetValue(Context, OutRotationAsEuler, &Rotation);
		SetValue(Context, OutRotator, &Rotator);
		SetValue(Context, OutQuat, &Quat);
		SetValue(Context, OutScale, &Scale);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeSphereMeshDataflowNode::FMakeSphereMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Mesh);
}

void FMakeSphereMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		FSphereGenerator SphereGenerator;
		SphereGenerator.Radius = FMath::Max(FMathf::ZeroTolerance, Radius);
		SphereGenerator.NumPhi = FMath::Max(3, NumPhi);
		SphereGenerator.NumTheta = FMath::Max(3, NumTheta);
		SphereGenerator.bPolygroupPerQuad = false;
		SphereGenerator.Generate();

		DynMesh.Copy(&SphereGenerator);
		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeCapsuleMeshDataflowNode::FMakeCapsuleMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Mesh);
}

void FMakeCapsuleMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		FCapsuleGenerator CapsuleGenerator;
		CapsuleGenerator.Radius = FMath::Max(FMathf::ZeroTolerance, Radius);
		CapsuleGenerator.SegmentLength = FMath::Max(FMathf::ZeroTolerance, SegmentLength);
		CapsuleGenerator.NumHemisphereArcSteps = FMath::Max(5, NumHemisphereArcSteps);
		CapsuleGenerator.NumCircleSteps = FMath::Max(3, NumCircleSteps);
		CapsuleGenerator.NumSegmentSteps = FMath::Max(0, NumSegmentSteps);
		CapsuleGenerator.bPolygroupPerQuad = false;
		CapsuleGenerator.Generate();

		DynMesh.Copy(&CapsuleGenerator);
		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeCylinderMeshDataflowNode::FMakeCylinderMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Mesh);
}

void FMakeCylinderMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		FCylinderGenerator CylinderGenerator;
		CylinderGenerator.Radius[0] = FMath::Max(FMathf::ZeroTolerance, Radius1);
		CylinderGenerator.Radius[1] = FMath::Max(FMathf::ZeroTolerance, Radius2);
		CylinderGenerator.Height = FMath::Max(FMathf::ZeroTolerance, Height);
		CylinderGenerator.LengthSamples = LengthSamples;
		CylinderGenerator.AngleSamples = AngleSamples;
		CylinderGenerator.bCapped = true;
		CylinderGenerator.bPolygroupPerQuad = false;
		CylinderGenerator.Generate();

		DynMesh.Copy(&CylinderGenerator);
		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeBoxMeshDataflowNode::FMakeBoxMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Mesh);
}

void FMakeBoxMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		FGridBoxMeshGenerator GridBoxMeshGenerator;
		GridBoxMeshGenerator.Box = UE::Geometry::FOrientedBox3d(Center, 0.5 * Size);
		GridBoxMeshGenerator.EdgeVertices = FIndex3i(SubdivisionsX + 1, SubdivisionsY + 1, SubdivisionsZ + 1);
		GridBoxMeshGenerator.bPolygroupPerQuad = false;
		GridBoxMeshGenerator.Generate();

		DynMesh.Copy(&GridBoxMeshGenerator);
		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

FMakePlaneDataflowNode::FMakePlaneDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&BasePoint) .SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&Normal) .SetCanHidePin(true).SetPinIsHidden(true);
	RegisterOutputConnection(&Plane);
}

void FMakePlaneDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Plane))
	{
		const FVector InBasePoint = GetValue(Context, &BasePoint);
		const FVector InNormal = GetValue(Context, &Normal);

		const FPlane OutPlane = FPlane(InBasePoint, InNormal);

		SetValue(Context, OutPlane, &Plane);
	}
}

#if WITH_EDITOR
bool FMakePlaneDataflowNode::CanDebugDrawViewMode(const FName& ViewModeName) const
{
	return ViewModeName == UE::Dataflow::FDataflowConstruction3DViewMode::Name;
}

void FMakePlaneDataflowNode::DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const
{
	if ((DebugDrawParameters.bNodeIsSelected || DebugDrawParameters.bNodeIsPinned))
	{
		DebugDrawRenderSettings.SetDebugDrawSettings(DataflowRenderingInterface);

		const FVector InBasePoint = GetValue(Context, &BasePoint);
		FVector InNormal = GetValue(Context, &Normal);

		FSimpleDebugDrawMesh Mesh;
		Mesh.MakeRectangleMesh(FVector(0.0), PlaneSizeMultiplier * 10.f, PlaneSizeMultiplier * 10.f, 11, 11);

		const FVector Up = FVector::UpVector;
		FQuat Quat = FQuat::FindBetweenVectors(Up, Normal);

		FTransform PlaneTransform = FTransform::Identity;
		PlaneTransform.SetRotation(Quat);
		PlaneTransform.SetTranslation(InBasePoint);

		for (int32 VertexIdx = 0; VertexIdx < Mesh.GetMaxVertexIndex(); ++VertexIdx)
		{
			Mesh.Vertices[VertexIdx] = PlaneTransform.TransformPosition(Mesh.Vertices[VertexIdx]);
		}

		DataflowRenderingInterface.DrawMesh(Mesh);

		// Draw normal
		InNormal.Normalize();
		DataflowRenderingInterface.DrawLine(InBasePoint, InBasePoint + InNormal * 2.f);
	}
}
#endif

/* -------------------------------------------------------------------------------- */

FMakeDiscMeshDataflowNode::FMakeDiscMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Mesh);
}

void FMakeDiscMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		FDiscMeshGenerator DiscGenerator;
		DiscGenerator.Radius = FMath::Max(FMathf::ZeroTolerance, Radius);
		DiscGenerator.Normal = FVector3f(Normal);
		DiscGenerator.AngleSamples = AngleSamples;
		DiscGenerator.RadialSamples = RadialSamples;
		DiscGenerator.StartAngle = StartAngle;
		DiscGenerator.EndAngle = EndAngle;
		DiscGenerator.bSinglePolygroup = true;
		DiscGenerator.Generate();

		DynMesh.Copy(&DiscGenerator);
		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeStairMeshDataflowNode::FMakeStairMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Mesh);
}

void FMakeStairMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		if (StairType == EDataflowStairTypeEnum::Linear)
		{
			FLinearStairGenerator StairGenerator;
			StairGenerator.NumSteps = NumSteps;
			StairGenerator.StepWidth = StepWidth;
			StairGenerator.StepHeight = StepHeight;
			StairGenerator.StepDepth = StepDepth;
			StairGenerator.bScaleUVByAspectRatio = true;
			StairGenerator.bPolygroupPerQuad = true;
			StairGenerator.Generate();

			DynMesh.Copy(&StairGenerator);
		}
		else if (StairType == EDataflowStairTypeEnum::Floating)
		{
			FFloatingStairGenerator StairGenerator;
			StairGenerator.NumSteps = NumSteps;
			StairGenerator.StepWidth = StepWidth;
			StairGenerator.StepHeight = StepHeight;
			StairGenerator.StepDepth = StepDepth;
			StairGenerator.bScaleUVByAspectRatio = true;
			StairGenerator.bPolygroupPerQuad = true;
			StairGenerator.Generate();

			DynMesh.Copy(&StairGenerator);
		}
		else if (StairType == EDataflowStairTypeEnum::Curved)
		{
			FCurvedStairGenerator StairGenerator;
			StairGenerator.NumSteps = NumSteps;
			StairGenerator.StepWidth = StepWidth;
			StairGenerator.StepHeight = StepHeight;
			StairGenerator.CurveAngle = CurveAngle;
			StairGenerator.InnerRadius = InnerRadius;
			StairGenerator.bScaleUVByAspectRatio = true;
			StairGenerator.bPolygroupPerQuad = true;
			StairGenerator.Generate();

			DynMesh.Copy(&StairGenerator);
		}
		else if (StairType == EDataflowStairTypeEnum::Spiral)
		{
			FSpiralStairGenerator StairGenerator;
			StairGenerator.NumSteps = NumSteps;
			StairGenerator.StepWidth = StepWidth;
			StairGenerator.StepHeight = StepHeight;
			StairGenerator.CurveAngle = CurveAngle;
			StairGenerator.InnerRadius = InnerRadius;
			StairGenerator.bScaleUVByAspectRatio = true;
			StairGenerator.bPolygroupPerQuad = true;
			StairGenerator.Generate();

			DynMesh.Copy(&StairGenerator);
		}

		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeRectangleMeshDataflowNode::FMakeRectangleMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Origin) .SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&Normal) .SetCanHidePin(true).SetPinIsHidden(true);
	RegisterOutputConnection(&Mesh);
}

void FMakeRectangleMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		const FVector InOrigin = GetValue(Context, &Origin);
		const FVector InNormal = GetValue(Context, &Normal);

		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		FRectangleMeshGenerator RectangleGenerator;
		RectangleGenerator.Origin = InOrigin;
		RectangleGenerator.Normal = FVector3f(InNormal);
		RectangleGenerator.Width = Width;
		RectangleGenerator.Height = Height;
		RectangleGenerator.WidthVertexCount = WidthVertexCount;
		RectangleGenerator.HeightVertexCount = HeightVertexCount;
		RectangleGenerator.bSinglePolyGroup = true;
		RectangleGenerator.Generate();

		DynMesh.Copy(&RectangleGenerator);
		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

FMakeTorusMeshDataflowNode::FMakeTorusMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Origin).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterOutputConnection(&Mesh);
}

void FMakeTorusMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		const FVector InOrigin = GetValue(Context, &Origin);

		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3& DynMesh = NewMesh->GetMeshRef();

		TArray<FVector3d> ProfileCurve; ProfileCurve.SetNumUninitialized(ProfileVertexCount);
		TArray<FFrame3d> SweepCurve; SweepCurve.SetNumUninitialized(SweepVertexCount);

		FVector Vec1(0.0, -Radius1, 0.0);
		const float RotateAngle1 = 360.f / float(ProfileVertexCount);

		// Create profile curve
		for (int32 Idx = 0; Idx < ProfileVertexCount; ++Idx)
		{
			ProfileCurve[Idx] = Vec1;

			Vec1 = Vec1.RotateAngleAxis(RotateAngle1, FVector::XAxisVector);
		}

		FVector Vec2(0.0, -Radius2, 0.0);
		const float RotateAngle2 = 360.f / float(SweepVertexCount);
		TQuaternion<double> Quat(FVector::ZAxisVector, RotateAngle2, true);

		FFrame3d Frame; // Construct a frame positioned at(0, 0, 0) aligned to the unit axes

		// Create sweep curve
		for (int32 Idx = 0; Idx < SweepVertexCount; ++Idx)
		{
			FFrame3d Frame1 = Frame;
			Frame1.Origin = Vec2 + InOrigin;

			SweepCurve[Idx] = Frame1;

			Frame.Rotate(Quat);
			Vec2 = Vec2.RotateAngleAxis(RotateAngle2, FVector::ZAxisVector);
		}

		FProfileSweepGenerator SweepGenerator;
		SweepGenerator.ProfileCurve = ProfileCurve;
		SweepGenerator.SweepCurve = SweepCurve;
		SweepGenerator.bSweepCurveIsClosed = true;
		SweepGenerator.bProfileCurveIsClosed = true;
		SweepGenerator.Generate();

		DynMesh.Copy(&SweepGenerator);
		SetValue(Context, NewMesh, &Mesh);
	}
}

/* -------------------------------------------------------------------------------- */

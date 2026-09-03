// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Math/MathFwd.h"
#include "Containers/Array.h"
#include "DynamicMesh/DynamicMesh3.h"

struct FDataflowBaseElement;

/** Dataflow object debug draw interface */
struct IDataflowDebugDrawObject : public FRefCountedObject
{
	virtual ~IDataflowDebugDrawObject() = default;

	static FName StaticType() { return FName("IDataflowDebugDrawObject"); }

	/** Check of the object type */
	virtual bool IsA(FName InType) const  {return false;}
};

class IDataflowDebugDrawInterface
{
public:

	using FDataflowElementsType = TArray<TSharedPtr<FDataflowBaseElement>>;

	virtual ~IDataflowDebugDrawInterface() = default;

	// State management
	virtual void SetColor(const FLinearColor& InColor) = 0;
	virtual void SetPointSize(float Size) = 0;
	virtual void SetLineWidth(double Width) = 0;
	virtual void SetWireframe(bool bInWireframe) = 0;
	virtual void SetShaded(bool bInShaded) = 0;
	virtual void SetTranslucent(bool bInShadedTranslucent) = 0;
	virtual void SetForegroundPriority() = 0;
	virtual void SetWorldPriority() = 0;
	virtual void ResetAllState() = 0;

	virtual void ReservePoints(int32 NumAdditionalPoints) = 0;
	virtual void DrawObject(const TRefCountPtr<IDataflowDebugDrawObject>& Object) = 0;
	virtual void DrawPoint(const FVector& Position) = 0;
	virtual void DrawLine(const FVector& Start, const FVector& End) const = 0;

	struct IDebugDrawMesh
	{
		virtual ~IDebugDrawMesh() = default;

		virtual int32 GetMaxVertexIndex() const = 0;
		virtual bool IsValidVertex(int32 VertexIndex) const = 0;
		virtual FVector GetVertexPosition(int32 VertexIndex) const = 0;
		virtual FVector GetVertexNormal(int32 VertexIndex) const = 0;

		virtual int32 GetMaxTriangleIndex() const = 0;
		virtual bool IsValidTriangle(int32 TriangleIndex) const = 0;
		virtual FIntVector3 GetTriangle(int32 TriangleIndex) const = 0;
	};

	virtual void DrawMesh(const IDebugDrawMesh& Mesh) const = 0;

	virtual void DrawBox(const FVector& Extents, const FQuat& Rotation, const FVector& Center, double UniformScale) const = 0;
	virtual void DrawSphere(const FVector& Center, double Radius) const = 0;
	virtual void DrawCapsule(const FVector& Center, const double& Radius, const double& HalfHeight, const FVector& XAxis, const FVector& YAxis, const FVector &ZAxis) const = 0;

	virtual void DrawOverlayText(const FString& InString) = 0;
	virtual FString GetOverlayText() const = 0;

	/** Dataflow elements non const accessor */
	virtual FDataflowElementsType& ModifyDataflowElements() = 0;

	/** Dataflow elements const accessor */
	virtual const FDataflowElementsType& GetDataflowElements() const = 0;

	// TODO: More draw functions

};



// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "UDynamicMesh.h"
#include "Dataflow/DataflowSelection.h"
#include "Math/MathFwd.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Dataflow/DataflowDebugDraw.h"

#include "GeometryCollectionMakeNodes.generated.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5
namespace Dataflow = UE::Dataflow;
#else
namespace UE_DEPRECATED(5.5, "Use UE::Dataflow instead.") Dataflow {}
#endif

/**
 * Make a literal string
 * Deprecated (5.6)
 */
USTRUCT(meta = (DataflowGeometryCollection, Deprecated = "5.6"))
struct FMakeLiteralStringDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralStringDataflowNode, "MakeLiteralString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "String");
	FString Value = FString("");

	UPROPERTY(meta = (DataflowOutput))
	FString String;

	FMakeLiteralStringDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a literal string
 */
USTRUCT()
struct FMakeLiteralStringDataflowNode_v2 : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralStringDataflowNode_v2, "MakeLiteralString", "Utilities|String", "")

public:
	UPROPERTY(EditAnywhere, Category = "String", meta = (DataflowOutput));
	FString String;

	FMakeLiteralStringDataflowNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&String);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * Make a points array from specified points
 *
 */
USTRUCT()
struct FMakePointsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakePointsDataflowNode, "MakePoints", "Generators|Point", "")
	DATAFLOW_NODE_RENDER_TYPE("PointsRender", FName("TArray<FVector>"), "Points")

public:
	UPROPERTY(EditAnywhere, Category = "Point")
	TArray<FVector> Point;

	UPROPERTY(meta = (DataflowOutput))
	TArray<FVector> Points;

	FMakePointsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Points);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

#if WITH_EDITOR
	virtual bool CanDebugDraw() const override
	{
		return true;
	}
	virtual bool CanDebugDrawViewMode(const FName& ViewModeName) const override;
	virtual void DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const override;
#endif
};


UENUM(BlueprintType)
enum class EMakeBoxDataTypeEnum : uint8
{
	Dataflow_MakeBox_DataType_MinMax UMETA(DisplayName = "Min/Max"),
	Dataflow_MakeBox_DataType_CenterSize UMETA(DisplayName = "Center/Size"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 * Make a box
 */
USTRUCT()
struct FMakeBoxDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeBoxDataflowNode, "MakeBox", "Generators|Box", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender",FName("FBox"), "Box")

public:
	UPROPERTY(EditAnywhere, Category = "Box", meta = (DisplayName = "Input Data Type"));
	EMakeBoxDataTypeEnum DataType = EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax;

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax", EditConditionHides));
	FVector Min = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_MinMax", EditConditionHides));
	FVector Max = FVector(10.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize", EditConditionHides));
	FVector Center = FVector(0.0);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (DataflowInput, EditCondition = "DataType == EMakeBoxDataTypeEnum::Dataflow_MakeBox_DataType_CenterSize", EditConditionHides));
	FVector Size = FVector(10.0);

	UPROPERTY(meta = (DataflowOutput));
	FBox Box = FBox(ForceInit);

	FMakeBoxDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Min);
		RegisterInputConnection(&Max);
		RegisterInputConnection(&Center);
		RegisterInputConnection(&Size);
		RegisterOutputConnection(&Box);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


/**
 *
 * Description for this node
 *
 */
USTRUCT()
struct FMakeSphereDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeSphereDataflowNode, "MakeSphere", "Generators|Sphere", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FSphere"), "Sphere")

public:
	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (DataflowInput));
	FVector Center = FVector(0.f);

	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (DataflowInput));
	float Radius = 10.f;

	UPROPERTY(meta = (DataflowOutput));
	FSphere Sphere = FSphere(ForceInit);

	FMakeSphereDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Center);
		RegisterInputConnection(&Radius);
		RegisterOutputConnection(&Sphere);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

#if WITH_EDITOR
	virtual bool CanDebugDraw() const override
	{
		return true;
	}
	virtual bool CanDebugDrawViewMode(const FName& ViewModeName) const override;
	virtual void DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const override;
#endif
};

/**
 * Make a float value
 * Deprecated (5.6)
 */
USTRUCT(meta = (DataflowGeometryCollection, Deprecated = "5.6"))
struct FMakeLiteralFloatDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralFloatDataflowNode, "MakeLiteralFloat", "Math|Float", "")

public:
	UPROPERTY(EditAnywhere, Category = "Float");
	float Value = 0.f;

	UPROPERTY(meta = (DataflowOutput))
	float Float = 0.f;

	FMakeLiteralFloatDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a float value
 */
USTRUCT()
struct FMakeLiteralFloatDataflowNode_v2 : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralFloatDataflowNode_v2, "MakeLiteralFloat", "Math|Float", "")

public:
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowOutput));
	float Float = 0.f;

	FMakeLiteralFloatDataflowNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Float);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * Make a double value
 *
 */
USTRUCT()
struct FMakeLiteralDoubleDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralDoubleDataflowNode, "MakeLiteralDouble", "Math|Double", "")

private:
	UPROPERTY(EditAnywhere, Category = "Double", meta = (DataflowOutput));
	double Double = 0.0;

public:
	FMakeLiteralDoubleDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make an integer value
 * Deprecated (5.6)
 */
USTRUCT(meta = (DataflowGeometryCollection, Deprecated = "5.6"))
struct FMakeLiteralIntDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralIntDataflowNode, "MakeLiteralInt", "Math|Int", "")

public:
	UPROPERTY(EditAnywhere, Category = "Int");
	int32 Value = 0;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Int"))
	int32 Int = 0;

	FMakeLiteralIntDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Int);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make an integer value
 */
USTRUCT()
struct FMakeLiteralIntDataflowNode_v2 : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralIntDataflowNode_v2, "MakeLiteralInt", "Math|Int", "")

public:
	UPROPERTY(EditAnywhere, Category = "Int", meta = (DataflowOutput));
	int32 Int = 0;

	FMakeLiteralIntDataflowNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Int);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a bool value
 * Deprecated(5.6)
 */
USTRUCT(meta = (DataflowGeometryCollection, Deprecated = "5.6"))
struct FMakeLiteralBoolDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralBoolDataflowNode, "MakeLiteralBool", "Math|Boolean", "")

public:
	UPROPERTY(EditAnywhere, Category = "Bool");
	bool Value = false;

	UPROPERTY(meta = (DataflowOutput))
	bool Bool = false;

	FMakeLiteralBoolDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Bool);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a bool value
 */
USTRUCT()
struct FMakeLiteralBoolDataflowNode_v2 : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralBoolDataflowNode_v2, "MakeLiteralBool", "Math|Boolean", "")

public:
	UPROPERTY(EditAnywhere, Category = "Bool", meta = (DataflowOutput));
	bool Bool = false;

	FMakeLiteralBoolDataflowNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Bool);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


/**
 * Make a vector
 * Deprecated(5.6)
 * Use MakeVector3 instead
 */
USTRUCT(meta = (DataflowGeometryCollection, Deprecated = "5.6"))
struct FMakeLiteralVectorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeLiteralVectorDataflowNode, "MakeLiteralVector", "Math|Vector", "")

public:
	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput));
	float X = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput));
	float Y = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Vector", meta = (DataflowInput));
	float Z = float(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Vector"))
	FVector Vector = FVector(0.0);

	FMakeLiteralVectorDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&X);
		RegisterInputConnection(&Y);
		RegisterInputConnection(&Z);
		RegisterOutputConnection(&Vector);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make an FTransform
 * Note: Originaly this version was depricated and replaced with FMakeTransformDataflowNode_v2 but when AnyRotationType was
 * introduced with the ConvertAnyRotation node FMakeTransformDataflowNode_v2 became obsolete and this version became the current version again
 */
USTRUCT()
struct FMakeTransformDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeTransformDataflowNode, "MakeTransform", "Generators|Transform", "")

private:
	/** Translation */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput, DisplayName = "Translation"));
	FVector InTranslation = FVector(0, 0, 0);

	/** Rotation as Euler */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput, DisplayName = "Rotation"));
	FVector InRotation = FVector(0, 0, 0);

	/** Scale */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput, DisplayName = "Scale"));
	FVector InScale = FVector(1, 1, 1);

	/** Result transform */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Transform"));
	FTransform OutTransform = FTransform::Identity;

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

public:
	FMakeTransformDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&InTranslation);
		RegisterInputConnection(&InRotation);
		RegisterInputConnection(&InScale);
		RegisterOutputConnection(&OutTransform);
	}
};

/**
*
* Make a FTransform
* Deprecated (5.6)
* Use FMakeTransformDataflowNode instead
*/
USTRUCT(meta = (Deprecated = "5.6"))
struct FMakeTransformDataflowNode_v2 : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeTransformDataflowNode_v2, "MakeTransform", "Generators|Transform", "")

private:

	/** Translation */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput));
	FVector Translation = FVector(0.f, 0.f, 0.f);

	/** Rotation as Euler */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput));
	FVector Rotation = FVector(0.f, 0.f, 0.f);

	/** Rotation a Rotator */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput));
	FRotator Rotator = FRotator(0.f, 0.f, 0.f);

	/** Rotation as a quaternion */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput));
	FQuat Quat = FQuat(ForceInit);

	/** Scale */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput));
	FVector Scale = FVector(1.f, 1.f, 1.f);

	/** Result transform */
	UPROPERTY(meta = (DataflowOutput));
	FTransform Transform = FTransform::Identity;

public:
	FMakeTransformDataflowNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 *
 *
 */
USTRUCT()
struct FMakeQuaternionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeQuaternionDataflowNode, "MakeQuaternion", "Math|Vector", "")

public:
	UPROPERTY(EditAnywhere, Category = "Quaternion ", meta = (DataflowInput));
	float X = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Quaternion", meta = (DataflowInput));
	float Y = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Quaternion", meta = (DataflowInput));
	float Z = float(0.0);

	UPROPERTY(EditAnywhere, Category = "Quaternion", meta = (DataflowInput));
	float W = float(0.0);

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Quaternion"))
	FQuat Quaternion = FQuat(ForceInitToZero);

	FMakeQuaternionDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&X);
		RegisterInputConnection(&Y);
		RegisterInputConnection(&Z);
		RegisterInputConnection(&W);
		RegisterOutputConnection(&Quaternion);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * M
 *
 */
USTRUCT()
struct FMakeFloatArrayDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeFloatArrayDataflowNode, "MakeFloatArray", "Math|Float", "")

public:
	/** Number of elements of the array */
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowInput, DisplayName = "Number of Elements", UIMin = "0"));
	int32 NumElements = 1;

	/** Value to initialize the array with */
	UPROPERTY(EditAnywhere, Category = "Float", meta = (DataflowInput));
	float Value = 0.f;

	/** Output float array */
	UPROPERTY(meta = (DataflowOutput))
	TArray<float> FloatArray;

	FMakeFloatArrayDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&NumElements);
		RegisterInputConnection(&Value);
		RegisterOutputConnection(&FloatArray);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * Make an empty ManagedArrayCollection
 *
 */
USTRUCT()
struct FMakeCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeCollectionDataflowNode, "MakeCollection", "Generators|Collection", "")

private:
	UPROPERTY(meta = (DataflowOutput));
	FManagedArrayCollection Collection;

public:
	FMakeCollectionDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * Make a Rotator
 *
 */
USTRUCT()
struct FMakeRotatorDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeRotatorDataflowNode, "MakeRotator", "Generators|Transform", "")

private:
	/** Rotation around the right axis (around Y axis), Looking up and down (0=Straight Ahead, +Up, -Down) */
	UPROPERTY(EditAnywhere, Category = "Rotator", meta = (DataflowInput));
	float Pitch = 0.f;

	/** Rotation around the up axis (around Z axis), Turning around (0=Forward, +Right, -Left)*/
	UPROPERTY(EditAnywhere, Category = "Rotator", meta = (DataflowInput));
	float Yaw = 0.f;

	/** Rotation around the forward axis (around X axis), Tilting your head, (0=Straight, +Clockwise, -CCW) */
	UPROPERTY(EditAnywhere, Category = "Rotator", meta = (DataflowInput));
	float Roll = 0.f;

	/** Rotator output */
	UPROPERTY(meta = (DataflowOutput));
	FRotator Rotator = FRotator(0.f, 0.f, 0.f);

public:
	FMakeRotatorDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
* Break a Transform into Translation, Rotation (Euler, Rotator, Quaternion), Scale
*/
USTRUCT()
struct FBreakTransformDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBreakTransformDataflowNode, "BreakTransform", "Math|Transform", "")

private:
	/** Transform to break into components */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DataflowInput));
	FTransform Transform;

	/** Translation */
	UPROPERTY(meta = (DataflowOutput));
	FDataflowVectorTypes Translation;

	/** Rotation as Euler */
	UPROPERTY(meta = (DataflowOutput));
	FVector Rotation = FVector(0.f, 0.f, 0.f);

	/** Rotation as a rotator */
	UPROPERTY(meta = (DataflowOutput));
	FRotator Rotator = FRotator(0.f, 0.f, 0.f);

	/** Rotation as a quaternion */
	UPROPERTY(meta = (DataflowOutput));
	FQuat Quat = FQuat(ForceInit);

	/** Scale */
	UPROPERTY(meta = (DataflowOutput));
	FDataflowVectorTypes Scale;

public:
	FBreakTransformDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

UENUM(BlueprintType)
enum class EMakeMeshTypeEnum : uint8
{
	Sphere UMETA(DisplayName = "Sphere"),
	Capsule UMETA(DisplayName = "Capsule"),
	Cylinder UMETA(DisplayName = "Cylinder"),
};

/**
 * Make a sphere mesh
 */
USTRUCT()
struct FMakeSphereMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeSphereMeshDataflowNode, "MakeSphereMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/** Sphere Radius */
	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float Radius = 1.f;

	/** Sphere numphi */
	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (DisplayName = "Steps Phi", UIMin = "3", ClampMin = "3"));
	int32 NumPhi = 12;

	/** Sphere numtheta */
	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (DisplayName = "Steps Theta", UIMin = "3", ClampMin = "3"));
	int32 NumTheta = 12;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeSphereMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a capsule mesh
 */
USTRUCT()
struct FMakeCapsuleMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeCapsuleMeshDataflowNode, "MakeCapsuleMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/** Radius of capsule */
	UPROPERTY(EditAnywhere, Category = "Capsule", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float Radius = 1.f;

	/** Length of capsule line segment, so total height is SegmentLength + 2*Radius */
	UPROPERTY(EditAnywhere, Category = "Capsule", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float SegmentLength = 1.f;

	/** Number of vertices along the 90-degree arc from the pole to edge of spherical cap. */
	UPROPERTY(EditAnywhere, Category = "Capsule", meta = (UIMin = "5", ClampMin = "5"));
	int32 NumHemisphereArcSteps = 5;

	/** Number of vertices along each circle */
	UPROPERTY(EditAnywhere, Category = "Capsule", meta = (UIMin = "3", ClampMin = "3"));
	int32 NumCircleSteps = 6;

	/** Number of subdivisions lengthwise along the cylindrical section */
	UPROPERTY(EditAnywhere, Category = "Capsule", meta = (UIMin = "0", ClampMin = "0"));
	int32 NumSegmentSteps = 0;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeCapsuleMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a cylinder mesh
 */
USTRUCT()
struct FMakeCylinderMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeCylinderMeshDataflowNode, "MakeCylinderMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/** Radius1 of cylinder */
	UPROPERTY(EditAnywhere, Category = "Cylinder", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float Radius1 = 1.f;

	/** Radius2 of cylinder */
	UPROPERTY(EditAnywhere, Category = "Cylinder", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float Radius2 = 1.f;

	/** Height of cylinder */
	UPROPERTY(EditAnywhere, Category = "Cylinder", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float Height = 5.f;

	/** LengthSamples of cylinder */
	UPROPERTY(EditAnywhere, Category = "Cylinder", meta = (UIMin = "0", ClampMin = "0"));
	int32 LengthSamples = 0;

	/** AngleSamples of cylinder */
	UPROPERTY(EditAnywhere, Category = "Cylinder", meta = (UIMin = "4", ClampMin = "4"));
	int32 AngleSamples = 12;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeCylinderMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a box mesh
 */
USTRUCT()
struct FMakeBoxMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeBoxMeshDataflowNode, "MakeBoxMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/**  */
	UPROPERTY(EditAnywhere, Category = "Box");
	FVector Center = FVector(0.0);

	/**  */
	UPROPERTY(EditAnywhere, Category = "Box", meta = (UIMin = "0.1", ClampMin = "0.1"));
	FVector Size = FVector(5.0);

	/**  */
	UPROPERTY(EditAnywhere, Category = "Box", meta = (UIMin = "1", ClampMin = "1"));
	int32 SubdivisionsX = 3;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Box", meta = (UIMin = "1", ClampMin = "1"));
	int32 SubdivisionsY = 3;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Box", meta = (UIMin = "1", ClampMin = "1"));
	int32 SubdivisionsZ = 3;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeBoxMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a plane
 */
USTRUCT()
struct FMakePlaneDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakePlaneDataflowNode, "MakePlane", "Generators|Plane", "")

private:
	/** Base point */
	UPROPERTY(EditAnywhere, Category = "Plane", meta = (DataflowInput));
	FVector BasePoint = FVector(0.0);

	/** Normal vector */
	UPROPERTY(EditAnywhere, Category = "Plane", meta = (DataflowInput));
	FVector Normal = FVector::UpVector;

	/** DebugDraw settings */
	UPROPERTY(EditAnywhere, Category = "Debug Draw", meta = (ShowOnlyInnerProperties))
	FDataflowNodeDebugDrawSettings DebugDrawRenderSettings;

	UPROPERTY(EditAnywhere, Category = "Debug Draw", meta = (UIMin = "1.0", UIMax = "10.0", ClampMin = "1.0", ClampMax = "10.0"));
	float PlaneSizeMultiplier = 1.f;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput));
	FPlane Plane = FPlane(ForceInit);

public:
	FMakePlaneDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
#if WITH_EDITOR
	virtual bool CanDebugDraw() const override
	{
		return true;
	}
	virtual bool CanDebugDrawViewMode(const FName& ViewModeName) const override;
	virtual void DebugDraw(UE::Dataflow::FContext& Context, IDataflowDebugDrawInterface& DataflowRenderingInterface, const FDebugDrawParameters& DebugDrawParameters) const override;
#endif
};

/**
 * Make a disc mesh
 */
USTRUCT()
struct FMakeDiscMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeDiscMeshDataflowNode, "MakeDiscMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/** Radius */
	UPROPERTY(EditAnywhere, Category = "Disc", meta = (UIMin = "0.1"));
	float Radius = 1.f;

	/** Normal vector of all vertices will be set to this value. Default is +Z axis. */
	UPROPERTY(EditAnywhere, Category = "Disc");
	FVector Normal = FVector::UnitZ();

	/** Number of vertices around circumference */
	UPROPERTY(EditAnywhere, Category = "Disc", meta = (UIMin = "2", ClampMin = "2"));
	int32 AngleSamples = 12;

	/** Number of vertices along radial spokes */
	UPROPERTY(EditAnywhere, Category = "Disc", meta = (UIMin = "2", ClampMin = "2"));
	int32 RadialSamples = 2;

	/** Start of angle range spanned by disc, in degrees */
	UPROPERTY(EditAnywhere, Category = "Disc");
	float StartAngle = 0.f;

	/** End of angle range spanned by disc, in degrees */
	UPROPERTY(EditAnywhere, Category = "Disc");
	float EndAngle = 360.f;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeDiscMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

UENUM(BlueprintType)
enum class EDataflowStairTypeEnum : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	Floating UMETA(DisplayName = "Floating"),
	Curved UMETA(DisplayName = "Curved"),
	Spiral UMETA(DisplayName = "Spiral"),
};
/**
 * Make a stair mesh
 */
USTRUCT()
struct FMakeStairMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeStairMeshDataflowNode, "MakeStairMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/** Type of staircase */
	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "2"));
	EDataflowStairTypeEnum StairType = EDataflowStairTypeEnum::Linear;

	/** The number of steps in this staircase. */
	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "2"));
	int32 NumSteps = 8;

	/** The width of each step. */
	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "10.", ClampMin = "10"));
	float StepWidth = 150.f;

	/** The height of each step. */
	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "10.", ClampMin = "10"));
	float StepHeight = 20.f;

	/** The height of each step. */
	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "10.", ClampMin = "10", EditCondition = "StairType == EDataflowStairTypeEnum::Linear || StairType == EDataflowStairTypeEnum::Floating", EditConditionHides));
	float StepDepth = 30.f;

	/** Inner radius of the curved staircase */
	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "10.", ClampMin = "10", EditCondition = "StairType == EDataflowStairTypeEnum::Curved || StairType == EDataflowStairTypeEnum::Spiral", EditConditionHides));
	float CurveAngle = 90.f;

	/** Curve angle of the staircase (in degrees) */
	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "10.", ClampMin = "10", EditCondition = "StairType == EDataflowStairTypeEnum::Curved || StairType == EDataflowStairTypeEnum::Spiral", EditConditionHides));
	float InnerRadius = 150.f;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeStairMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a rectangle mesh
 */
USTRUCT()
struct FMakeRectangleMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeRectangleMeshDataflowNode, "MakeRectangleMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/** Rectangle will be translated so that center is at this point */
	UPROPERTY(EditAnywhere, Category = "Rectangle", meta = (DataflowInput));
	FVector Origin = FVector(0.0);

	/** Normal vector of all vertices will be set to this value. Default is +Z axis. */
	UPROPERTY(EditAnywhere, Category = "Rectangle", meta = (DataflowInput));
	FVector Normal = FVector::UnitZ();

	/** Width of rectangle */
	UPROPERTY(EditAnywhere, Category = "Rectangle", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float Width = 5.f;

	/** Height of rectangle */
	UPROPERTY(EditAnywhere, Category = "Rectangle", meta = (UIMin = "0.1", ClampMin = "0.1"));
	float Height = 5.f;

	/** Number of vertices along Width axis */
	UPROPERTY(EditAnywhere, Category = "Rectangle", meta = (UIMin = "2", ClampMin = "2"));
	int32 WidthVertexCount = 3;

	/** Number of vertices along Height axis */
	UPROPERTY(EditAnywhere, Category = "Rectangle", meta = (UIMin = "2", ClampMin = "2"));
	int32 HeightVertexCount = 3;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeRectangleMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Make a torus mesh
 */
USTRUCT()
struct FMakeTorusMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeTorusMeshDataflowNode, "MakeTorusMesh", "Generators|Mesh", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

private:
	/** Torus will be translated so that center is at this point */
	UPROPERTY(EditAnywhere, Category = "Torus", meta = (DataflowInput));
	FVector Origin = FVector(0.0);

	/** Radius of the profile */
	UPROPERTY(EditAnywhere, Category = "Torus", meta = (UIMin = "0.01", ClampMin = "0.01"));
	float Radius1 = 4.f;

	/** Number of vertices on the profile */
	UPROPERTY(EditAnywhere, Category = "Torus", meta = (UIMin = "3", ClampMin = "3"));
	int32 ProfileVertexCount = 12;

	/** Radius of sweep curve */
	UPROPERTY(EditAnywhere, Category = "Torus", meta = (UIMin = "0.01", ClampMin = "0.01"));
	float Radius2 = 10.f;

	/** Number of vertices on the sweep curve */
	UPROPERTY(EditAnywhere, Category = "Torus", meta = (UIMin = "3", ClampMin = "3"));
	int32 SweepVertexCount = 12;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

public:
	FMakeTorusMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

namespace UE::Dataflow
{
	void GeometryCollectionMakeNodes();
}

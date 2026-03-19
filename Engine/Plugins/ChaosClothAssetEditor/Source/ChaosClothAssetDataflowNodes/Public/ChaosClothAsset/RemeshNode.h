// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "ChaosClothAsset/ConnectableValue.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "RemeshNode.generated.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5
namespace Dataflow = UE::Dataflow;
#else
namespace UE_DEPRECATED(5.5, "Use UE::Dataflow instead.") Dataflow {}
#endif

UENUM(BlueprintType)
enum class EChaosClothAssetRemeshMethod : uint8
{
	Remesh,
	Simplify
};

/** Remesh the cloth surface(s) to get the specified mesh resolution(s).
 *  NOTE: Weight Maps, Skinning Data, Self Collision Spheres, and Long Range Attachment Constraints will be reconstructed on the output mesh, however all other Selections will be removed
 */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetRemeshNode_v2 : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetRemeshNode_v2, "Remesh", "Cloth", "Cloth Remesh")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FClothCollection"), "Collection")

public:

	FChaosClothAssetRemeshNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh")
	bool bRemeshSim = true;

	/**
	 * Range of target mesh resolutions, as a percentage of input triangle mesh resolution. A value of 50 on all vertices should roughly halve the total number of triangles.
	 * If a valid vertex weight map is specified, it will use vertex weights to interpolate between the Lo and Hi values. Otherwise it will use the Lo value on all vertices.
	 */
	UPROPERTY(EditAnywhere, Category = "Sim Mesh", Meta = (EditCondition = "bRemeshSim"))
	FChaosClothAssetWeightedValueNonAnimatable DensityMapSim = { 100.f, 200.f, TEXT("DensityMapSim") };

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "200", EditCondition = "bRemeshSim"))
	int32 IterationsSim = 10;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshSim"))
	double SmoothingSim = 0.25;

	UPROPERTY(EditAnywhere, Category = "Render Mesh")
	bool bRemeshRender = false;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshRender"))
	EChaosClothAssetRemeshMethod RemeshMethodRender = EChaosClothAssetRemeshMethod::Remesh;

	/**
	 * Range of target mesh resolutions when using the Remesh method, as a percentage of input triangle mesh resolution. A value of 50 on all vertices should roughly halve the total number of triangles.
	 * If a valid vertex weight map is specified, it will use vertex weights to interpolate between the Lo and Hi values. Otherwise it will use the Lo value on all vertices.
	 */
	UPROPERTY(EditAnywhere, Category = "Render Mesh", Meta = (EditCondition = "bRemeshRender && RemeshMethodRender == EChaosClothAssetRemeshMethod::Remesh"))
	FChaosClothAssetWeightedValueNonAnimatable DensityMapRender = { 100.f, 200.f, TEXT("DensityMapRender") };

	/**
	 * Target mesh resolution when using the Simplify method, as a percentage of input triangle mesh resolution. A value of 50 should roughly halve the total number of triangles.
	 */
	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "1", UIMax = "200", ClampMin = "1", EditCondition = "bRemeshRender && RemeshMethodRender == EChaosClothAssetRemeshMethod::Simplify"))
	int32 TargetPercentRender = 100;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "100", EditCondition = "bRemeshRender && RemeshMethodRender == EChaosClothAssetRemeshMethod::Remesh"))
	int32 IterationsRender = 10;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshRender && RemeshMethodRender == EChaosClothAssetRemeshMethod::Remesh"))
	double SmoothingRender = 0.25;

	/** If checked, attempt to find matching vertices along Render mesh boundaries and remesh these separately */
	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (EditCondition = "bRemeshRender"))
	bool bRemeshRenderSeams = false;

	/** Number of remesh iterations over the Render mesh seams */
	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "100", EditCondition = "bRemeshRender && bRemeshRenderSeams"))
	int32 RenderSeamRemeshIterations = 1;

	virtual void Evaluate(UE::Dataflow::FContext & Context, const FDataflowOutput * Out) const override;

};



/** Remesh the cloth surface(s) to get the specified mesh resolution(s).
 *  NOTE: Weight Maps, Skinning Data, Self Collision Spheres, and Long Range Attachment Constraints will be reconstructed on the output mesh, however all other Selections will be removed
 */
USTRUCT(Meta = (DataflowCloth, Deprecated = "5.6"))
struct UE_DEPRECATED(5.6, "Use the newer version of this node instead.") FChaosClothAssetRemeshNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetRemeshNode, "Remesh", "Cloth", "Cloth Remesh")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FClothCollection"), "Collection")

public:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh")
	bool bRemeshSim = true;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta=(UIMin = "1", UIMax = "200", ClampMin = "1", EditCondition = "bRemeshSim"))
	int32 TargetPercentSim = 100;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "200", EditCondition = "bRemeshSim"))
	int32 IterationsSim = 10;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshSim"))
	double SmoothingSim = 0.25;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", Meta = (EditCondition = "bRemeshSim"))
	FChaosClothAssetConnectableIStringValue DensityMapSim;

	UPROPERTY(EditAnywhere, Category = "Render Mesh")
	bool bRemeshRender = false;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta=(UIMin = "1", UIMax = "200", ClampMin = "1", EditCondition = "bRemeshRender"))
	int32 TargetPercentRender = 100;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshRender"))
	EChaosClothAssetRemeshMethod RemeshMethodRender = EChaosClothAssetRemeshMethod::Remesh;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "100", EditCondition = "bRemeshRender && RemeshMethodRender == EChaosClothAssetRemeshMethod::Remesh"))
	int32 IterationsRender = 10;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshRender && RemeshMethodRender == EChaosClothAssetRemeshMethod::Remesh"))
	double SmoothingRender = 0.25;

	/** If checked, attempt to find matching vertices along Render mesh boundaries and remesh these separately */ 
	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (EditCondition = "bRemeshRender"))
	bool bRemeshRenderSeams = false;

	/** Number of remesh iterations over the Render mesh seams */
	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "100", EditCondition = "bRemeshRender && bRemeshRenderSeams"))
	int32 RenderSeamRemeshIterations = 1;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", Meta = (EditCondition = "bRemeshRender && RemeshMethodRender == EChaosClothAssetRemeshMethod::Remesh"))
	FChaosClothAssetConnectableIStringValue DensityMapRender;

	FChaosClothAssetRemeshNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

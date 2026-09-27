// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "Dataflow/DataflowMesh.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Dataflow/DataflowSelection.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMeshProcessor.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
// Some mesh-specific nodes have been moved to their own plugin
#include "Dataflow/MeshBooleanNodes.h"
#endif

#include "GeometryCollectionMeshNodes.generated.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5
namespace Dataflow = UE::Dataflow;
#else
namespace UE_DEPRECATED(5.5, "Use UE::Dataflow instead.") Dataflow {}
#endif

class FGeometryCollection;
class UStaticMesh;
class UDynamicMesh;
class UMaterialInterface;

/**
 *
 * Converts points into a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FPointsToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FPointsToMeshDataflowNode, "PointsToMesh", "Mesh|Utilities", "")

public:
	/** Points input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<FVector> Points;

	/** Mesh output */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Mesh triangle count */
	UPROPERTY(meta = (DataflowOutput))
	int32 TriangleCount = 0;

	FPointsToMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Points);
		RegisterOutputConnection(&Mesh);
		RegisterOutputConnection(&TriangleCount);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 * Base class for nodes that applying Geometry Script Mesh Processors
 */
USTRUCT()
struct FMeshProcessorDataflowNodeBase : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

public:

	FMeshProcessorDataflowNodeBase() = default;

	FMeshProcessorDataflowNodeBase(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid), OwningObject(InParam.OwningObject)
	{
	}

	~FMeshProcessorDataflowNodeBase()
	{
		TeardownBlueprintEvent();
	}

protected:

	UPROPERTY(EditAnywhere, Category = "Processor Type")
	TSubclassOf<UDynamicMeshProcessorBlueprint> MeshProcessor = nullptr;

	UPROPERTY(EditAnywhere, Instanced, NoClear, Category = "Instance", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UDynamicMeshProcessorBlueprint> MeshProcessorInstance;

	virtual void PostSerialize(const FArchive& Ar) override final
	{
		Super::PostSerialize(Ar);
		if (Ar.IsLoading())
		{
			TeardownBlueprintEvent();
			SetupBlueprintEvent();
		}
	}

private:

	virtual void OnPropertyChanged(UE::Dataflow::FContext& Context, const FPropertyChangedEvent& PropertyChangedEvent) override;

	// Handling for the selected blueprint being changed under the node (e.g., recompiled)
	FDelegateHandle BlueprintChangeDelegateHandle;
	void TeardownBlueprintEvent();
	void SetupBlueprintEvent();

	// Remember the parent UObject so that we can properly parent MeshProcessorInstance when the MeshProcessor changes 
	UPROPERTY(Transient)
	TObjectPtr<UObject> OwningObject;
};

/**
 * Apply a Geometry Script Mesh Processors to an input UDynamicMesh
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FApplyMeshProcessorToMeshDataflowNode final : public FMeshProcessorDataflowNodeBase
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FApplyMeshProcessorToMeshDataflowNode, "ApplyGeometryScriptToMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Mesh")

public:

	/** Input/Output mesh */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Mesh", DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	FApplyMeshProcessorToMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FMeshProcessorDataflowNodeBase(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh);
		RegisterOutputConnection(&Mesh, &Mesh);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 * Apply a Geometry Script Mesh Processors to the geometry of selected transforms in a geometry collection
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FApplyMeshProcessorToGeometryCollectionDataflowNode final : public FMeshProcessorDataflowNodeBase
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FApplyMeshProcessorToGeometryCollectionDataflowNode, "ApplyGeometryScriptToCollection", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FGeometryCollection::StaticType(), "Collection")

public:

	/** Input/Output mesh */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Selected bones will have geometry script processing applied (if they have geometry). If not connected, all geometry will be processed. */
	UPROPERTY(meta = (DataflowInput))
	FDataflowTransformSelection TransformSelection;

	/** Whether the processed mesh will have edges at normal/UV/color seams welded so they are treated as one edge during processing. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bWeldVertices = true;

	/** Whether to preserve isolated vertices which aren't used by any triangles. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bPreserveIsolatedVertices = true;

	FApplyMeshProcessorToGeometryCollectionDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FMeshProcessorDataflowNodeBase(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);

		RegisterInputConnection(&TransformSelection);
		RegisterOutputConnection(&Collection, &Collection);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * Converts a Collection to a set of Dynamic Meshes per selected Transform
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FCollectionSelectionToMeshesDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCollectionSelectionToMeshesDataflowNode, "CollectionSelectionToMeshes", "Mesh|Utilities", "")
	//DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), "Meshes")

public:
	/** Collection to convert*/
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FManagedArrayCollection Collection;

	/** Geometry on or under selected bones will be converted to meshes, optionally after filtering the selection to leaves. If not connected, all geometry will be processed. */
	UPROPERTY(meta = (DataflowInput))
	FDataflowTransformSelection TransformSelection;

	/** Whether to convert the input selection to only leaves, which may directly store geometry. Otherwise, meshes for selected cluster nodes will be generated by appending together geometry from leaf nodes. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bConvertSelectionToLeaves = true;

	/** Whether the processed mesh will have edges at normal/UV/color seams welded so they are treated as one edge during processing. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bWeldVertices = true;

	/** Whether to preserve isolated vertices which aren't used by any triangles. */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bPreserveIsolatedVertices = true;

	/** Output Array of DynamicMesh */
	UPROPERTY(meta = (DataflowOutput))
	TArray<TObjectPtr<UDynamicMesh>> Meshes;

	FCollectionSelectionToMeshesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 * Append Array of Meshes to Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FAppendMeshesToCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FAppendMeshesToCollectionDataflowNode, "AppendMeshesToCollection", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FGeometryCollection::StaticType(), "Collection")

public:
	/** Meshes will be appended to this collection */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DataflowIntrinsic))
	FManagedArrayCollection Collection;
	
	/** Selection of added transforms */
	UPROPERTY(meta = (DataflowOutput))
	FDataflowTransformSelection AddedSelection;

	/** Dynamic Meshes to append */
	UPROPERTY(meta = (DataflowInput))
	TArray<TObjectPtr<UDynamicMesh>> Meshes;

	/** Index of parent bone for appended meshes. If invalid, meshes will be appended to a root node. */
	UPROPERTY(EditAnywhere, Category = Options, meta = (DataflowInput))
	int32 ParentIndex = -1;

	FAppendMeshesToCollectionDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a BoundingBox into a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FBoxToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBoxToMeshDataflowNode, "BoxToMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender",FName("FDynamicMesh3"), "Mesh")

public:
	/** BoundingBox input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FBox Box = FBox(ForceInit);

	/** Mesh output */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Mesh triangle count */
	UPROPERTY(meta = (DataflowOutput))
	int32 TriangleCount = 0;

	FBoxToMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Box);
		RegisterOutputConnection(&Mesh);
		RegisterOutputConnection(&TriangleCount);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Collects information from the DynamicMesh and outputs it into a formatted string
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshInfoDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshInfoDataflowNode, "MeshInfo", "Mesh|Utilities", "")

public:
	/** DynamicMesh for the information */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Formatted output string */
	UPROPERTY(meta = (DataflowOutput))
	FString InfoString = FString("");

	FMeshInfoDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh);
		RegisterOutputConnection(&InfoString);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a DynamicMesh to a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshToCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshToCollectionDataflowNode, "MeshToCollection", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender",FGeometryCollection::StaticType(),  "Collection")

public:
	/** DynamicMesh to convert */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Output Collection */
	UPROPERTY(meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	/** Whether to split the mesh into multiple bones based on the mesh connectivity */
	UPROPERTY(EditAnywhere, Category = "General", meta = (DataflowInput));
	bool bSplitIslands = false;

	/** Whether to consider coincident vertices as connected even if the topology does not connect them */
	UPROPERTY(EditAnywhere, Category = "General");
	bool bConnectIslandsByVertexOverlap = false;

	/** Vertices closer than this distance are considered to be overlapping */
	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = 0.0));
	float ConnectVerticesThreshold = 0.001;

	/** Whether to add a root cluster for the single mesh case. Note if the mesh is split, the root cluster will always be added. */
	UPROPERTY(EditAnywhere, Category = "General", meta = (DataflowInput));
	bool bAddClusterRootForSingleMesh = false;

	FMeshToCollectionDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a Collection to a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FCollectionToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCollectionToMeshDataflowNode, "CollectionToMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender",FName("FDynamicMesh3"), "Mesh")

public:
	/** Collection to convert*/
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "General", meta = (DisplayName = "Center Pivot"));
	bool bCenterPivot = false;

	/** Output DynamicMesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FCollectionToMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a StaticMesh into a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FStaticMeshToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FStaticMeshToMeshDataflowNode, "StaticMeshToMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender",FName("FDynamicMesh3"), {"Mesh", "MaterialArray"})

public:
	/** StaticMesh to convert */
	UPROPERTY(EditAnywhere, Category = "StaticMesh", meta = (DataflowInput));
	TObjectPtr<UStaticMesh> StaticMesh;

	/** Output the HiRes representation, if set to true and HiRes doesn't exist it will output empty mesh */
	UPROPERTY(EditAnywhere, Category = "StaticMesh", meta = (DisplayName = "Use HiRes"));
	bool bUseHiRes = false;

	/** Specifies the LOD level to use */
	UPROPERTY(EditAnywhere, Category = "StaticMesh", meta = (DisplayName = "LOD Level"));
	int32 LODLevel = 0;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Output materials */
	UPROPERTY(meta = (DataflowOutput))
	TArray<TObjectPtr<UMaterialInterface>> MaterialArray;

	FStaticMeshToMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&StaticMesh);
		RegisterOutputConnection(&Mesh);
		RegisterOutputConnection(&MaterialArray);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Appends two meshes
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshAppendDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshAppendDataflowNode, "MeshAppend", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender",FName("FDynamicMesh3"), "Mesh")

public:
	/** Mesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh1;

	/** Mesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh2;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FMeshAppendDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh1);
		RegisterInputConnection(&Mesh2);
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 * Combine two Dataflow meshes
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FDataflowMeshAppendDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMeshAppendDataflowNode, "DataflowMeshAppend", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("UDataflowMesh"), "Mesh" )

public:

	FDataflowMeshAppendDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	/** Mesh input/output */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Mesh", DataflowIntrinsic))
	TObjectPtr<UDataflowMesh> Mesh;

	/** Mesh to append  */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<const UDataflowMesh> AppendMesh;

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};

/**
 *
 * Create a UDataflow mesh from an input UDynamicMesh and material array
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMakeDataflowMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeDataflowMeshDataflowNode, "MakeDataflowMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("UDataflowMesh"), "Mesh")

public:

	FMakeDataflowMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	/** DynamicMesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> InMesh;

	/** Materials input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<TObjectPtr<UMaterialInterface>> InMaterials;

	/** DataflowMesh output */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDataflowMesh> Mesh;

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


/**
 * Create a new UV layer/channel in a UDataflowMesh
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FDuplicateMeshUVChannelNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDuplicateMeshUVChannelNode, "DuplicateMeshUVChannelNode", "Mesh|Utilities", "Mesh UV DataflowMesh")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("UDataflowMesh"), "Mesh", "UVChannel")

public:

	FDuplicateMeshUVChannelNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	/** DataflowMesh input/output */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Mesh", DataflowIntrinsic))
	TObjectPtr<UDataflowMesh> Mesh;

	/** Index of the source UV channel */
	UPROPERTY(EditAnywhere, Category = Options)
	int32 SourceUVChannel = -1;

	/** Index of the added UV channel */
	UPROPERTY(meta = (DataflowOutput))
	int32 NewUVChannel = -1;

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


UENUM(BlueprintType)
enum class EDataflowMeshSplitIslandsMethod : uint8
{
	NoSplit,
	ByMeshTopology,
	ByVertexOverlap
};


/**
 *
 * Split a mesh into a connected islands
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FSplitMeshIslandsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSplitMeshIslandsDataflowNode, "SplitMeshIslands", "Mesh|Utilities", "")

public:

	FSplitMeshIslandsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	/** Mesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Meshes output */
	UPROPERTY(meta = (DataflowOutput))
	TArray<TObjectPtr<UDynamicMesh>> Meshes;

	/** Whether to consider coincident vertices as connected even if the topology does not connect them */
	UPROPERTY(EditAnywhere, Category = "General");
	EDataflowMeshSplitIslandsMethod SplitMethod = EDataflowMeshSplitIslandsMethod::ByMeshTopology;

	/** Vertices closer than this distance are considered to be overlapping */
	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = 0.0, EditCondition = "SplitMethod == EDataflowMeshSplitIslandsMethod::ByVertexOverlap"));
	float ConnectVerticesThreshold = 0.001;

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

/**
 *
 * Split a UDataflow mesh into a UDynamicMesh and a material array
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FSplitDataflowMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSplitDataflowMeshDataflowNode, "SplitDataflowMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FDynamicMesh3"), { "Mesh", "MaterialArray" })

public:

	FSplitDataflowMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	/** DataflowMesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDataflowMesh> InMesh;

	/** DyanmicMesh output */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Materials output */
	UPROPERTY(meta = (DataflowOutput))
	TArray<TObjectPtr<UMaterialInterface>> MaterialArray;

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};




/**
 *
 * Copies the same mesh with scale onto points
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshCopyToPointsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshCopyToPointsDataflowNode, "ScatterMeshes", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender",FName("FDynamicMesh3"), "Mesh")

public:
	/** Points to copy meshes onto */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<FVector> Points;

	/** Mesh to copy onto points */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic, DisplayName = "MeshToScatter"))
	TObjectPtr<UDynamicMesh> MeshToCopy;

	/** Scale applied to the mesh */
	UPROPERTY(EditAnywhere, Category = "Copy");
	float Scale = 1.f;

	/** merged result mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Result meshes as individual ones */
	UPROPERTY(meta = (DataflowOutput))
	TArray<TObjectPtr<UDynamicMesh>> Meshes;

	FMeshCopyToPointsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Points);
		RegisterInputConnection(&MeshToCopy);
		RegisterOutputConnection(&Mesh);
		RegisterOutputConnection(&Meshes);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Outputs Mesh data
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetMeshDataDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetMeshDataDataflowNode, "GetMeshData", "Mesh|Utilities", "")

public:
	/** Mesh for the data */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Number of vertices */
	UPROPERTY(meta = (DataflowOutput))
	int32 VertexCount = 0;

	/** Number of edges */
	UPROPERTY(meta = (DataflowOutput))
	int32 EdgeCount = 0;

	/** Number of triangles */
	UPROPERTY(meta = (DataflowOutput))
	int32 TriangleCount = 0;

	FGetMeshDataDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh);
		RegisterOutputConnection(&VertexCount);
		RegisterOutputConnection(&EdgeCount);
		RegisterOutputConnection(&TriangleCount);
	}

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


namespace UE::Dataflow
{
	void GeometryCollectionMeshNodes();
}


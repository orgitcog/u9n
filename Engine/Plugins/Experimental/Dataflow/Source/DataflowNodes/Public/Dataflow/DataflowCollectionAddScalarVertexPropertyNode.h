// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Dataflow/DataflowCollectionAttributeKeyNodes.h"

#include "DataflowCollectionAddScalarVertexPropertyNode.generated.h"

#define UE_API DATAFLOWNODES_API


class IDataflowAddScalarVertexPropertyCallbacks
{
public:
	virtual ~IDataflowAddScalarVertexPropertyCallbacks() = default;
	virtual FName GetName() const = 0;
	virtual TArray<FName> GetTargetGroupNames() const = 0;
	virtual TArray<UE::Dataflow::FRenderingParameter> GetRenderingParameters() const = 0;
};

class FDataflowAddScalarVertexPropertyCallbackRegistry
{
public:

	DATAFLOWNODES_API static FDataflowAddScalarVertexPropertyCallbackRegistry& Get();

	DATAFLOWNODES_API void RegisterCallbacks(TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>&& Callbacks);

	DATAFLOWNODES_API void DeregisterCallbacks(const FName& CallbacksName);

	DATAFLOWNODES_API TArray<FName> GetTargetGroupNames() const;

	DATAFLOWNODES_API TArray<UE::Dataflow::FRenderingParameter> GetRenderingParameters() const;
	
	DATAFLOWNODES_API TArray<UE::Dataflow::FRenderingParameter> GetRenderingParameters(const FName& TargetGroup) const;

private:

	TMap<FName, TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>> AllCallbacks;
};

class UE_DEPRECATED(5.6, "Please use FDataflowAddScalarVertexPropertyCallbackRegistry") DataflowAddScalarVertexPropertyCallbackRegistry
{
public:
	DATAFLOWNODES_API static DataflowAddScalarVertexPropertyCallbackRegistry& Get();
	DATAFLOWNODES_API static void TearDown();
	DATAFLOWNODES_API void RegisterCallbacks(TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks> && Callbacks);
	DATAFLOWNODES_API void DeregisterCallbacks(const FName & CallbacksName);
	DATAFLOWNODES_API TArray<FName> GetTargetGroupNames() const;
	DATAFLOWNODES_API TArray<UE::Dataflow::FRenderingParameter> GetRenderingParameters() const;
private:
	TMap<FName, TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>> AllCallbacks;
};

/*
* Custom type so that we can use property type customization
*/
USTRUCT()
struct FScalarVertexPropertyGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Vertex Group")
	FName Name = FGeometryCollection::VerticesGroup;
};

/** How the map stored on the AddWeightMapNode should be applied to an existing map. If no map exists, it is treated as zero.*/
UENUM()
enum class EDataflowWeightMapOverrideType : uint8
{
	
	/** Replace all the values.*/
	ReplaceAll,
	/** Add the values difference to the input one. */
	AddDifference,
	/** Replace only the values that has changed. */
	ReplaceChanged,
};

/** Scalar vertex properties. */
USTRUCT(Meta = (DataflowCollection))
struct FDataflowCollectionAddScalarVertexPropertyNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowCollectionAddScalarVertexPropertyNode, "PaintWeightMap", "Collection", "Paint a weight map and save it to collection")

	UE_API virtual TArray<UE::Dataflow::FRenderingParameter> GetRenderParametersImpl() const override;

	
public:

	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The name to be set as a weight map attribute. */
	UPROPERTY(EditAnywhere, Category = "Vertex Attributes", meta = (DisplayName = "Attribute Name"))
	FString Name;

	UPROPERTY(meta = (DisplayName = "Attribute Key", DataflowInput, DataflowOutput, DataflowPassthrough = "AttributeKey"))
	FCollectionAttributeKey AttributeKey;

	UPROPERTY()
	TArray<float> VertexWeights;

	UPROPERTY(EditAnywhere, Category = "Vertex Attributes", meta = (DisplayName = "Vertex Group"))
	FScalarVertexPropertyGroup TargetGroup;
	
	/** This type will define how the data are applied to the input data */
	UPROPERTY(EditAnywhere, Category = "Weight Map")
	EDataflowWeightMapOverrideType OverrideType = EDataflowWeightMapOverrideType::ReplaceAll;

	UE_API FDataflowCollectionAddScalarVertexPropertyNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	/** Report the vertex weights back onto the property ones */
	UE_API void ReportVertexWeights(const TArray<float>& SetupWeights, const TArray<float>& FinalWeights, const TArray<int32>& WeightIndices);

	/** Extract the vertex weights back from the property ones */
	UE_API void ExtractVertexWeights(const TArray<float>& SetupWeights, TArrayView<float> FinalWeights) const;

	/** Fill the weight attribute values from the collection */
	UE_API bool FillAttributeWeights(const TSharedPtr<const FManagedArrayCollection> SelectedCollection, const FCollectionAttributeKey& InAttributeKey, TArray<float>& OutAttributeValues) const;

	/** Get the weights attribute key to retrieve/set the weight values*/
	UE_API FCollectionAttributeKey GetWeightAttributeKey(UE::Dataflow::FContext& Context) const;

private:

	/** Pass through value to skip replacing the weight map value if nothing has changed */
	static constexpr float ReplaceChangedPassthroughValue = UE_BIG_NUMBER;
	
	UE_API virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

#undef UE_API

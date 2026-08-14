// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/AnimNextGraphEntryPoint.h"
#include "DataInterface/AnimNextDataInterface.h"
#include "RigVMCore/RigVM.h"
#include "TraitCore/TraitPtr.h"
#include "TraitCore/TraitHandle.h"
#include "TraitCore/EntryPointHandle.h"
#include "RigVMHost.h"
#include "AnimNextGraphEvaluatorExecuteDefinition.h"

#include "AnimNextAnimationGraph.generated.h"

#define UE_API ANIMNEXTANIMGRAPH_API

class UEdGraph;
class UAnimNextModule;
class UAnimGraphNode_AnimNextGraph;
struct FAnimNode_AnimNextGraph;
struct FRigUnit_AnimNextGraphEvaluator;
struct FAnimNextGraphInstance;
struct FAnimNextScheduleGraphTask;
struct FAnimNextEditorParam;
struct FAnimNextParam;
struct FAnimNextModuleInstance;
struct FAnimNextModuleAnimGraphComponent;

namespace UE::AnimNext
{
	struct FContext;
	struct FExecutionContext;
	class FAnimNextModuleImpl;
	struct FTestUtils;
	struct FParametersProxy;
	struct FPlayAnimSlotTrait;
	struct FBlendStackCoreTrait;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FModuleEditor;
	class FVariableCustomization;
}

namespace UE::AnimNext::AnimGraph
{
	class FAnimNextAnimGraphModule;
}

namespace UE::AnimNext::Graph
{
	extern ANIMNEXTANIMGRAPH_API const FName EntryPointName;
	extern ANIMNEXTANIMGRAPH_API const FName ResultName;
}

// A user-created collection of animation logic & data
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextAnimationGraph : public UAnimNextDataInterface
{
	GENERATED_BODY()

public:
	UE_API UAnimNextAnimationGraph(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	UE_API virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITORONLY_DATA
	UE_API virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
#endif

	// Allocates an instance of the graph with an optional module and/or parent graph instance
	// @param	InModuleInstance			The module instance to use
	// @param	InParentGraphInstance		The parent graph instance to use
	// @param	InEntryPoint				The entry point to use. If this is NAME_None then the default entry point for this graph is used
	// @return the allocated instance. This can be invalid if the entry point was not found or the graph's root trait is invalid.
	UE_API TSharedPtr<FAnimNextGraphInstance> AllocateInstance(FAnimNextModuleInstance* InModuleInstance = nullptr, UE::AnimNext::FExecutionContext* InParentContext = nullptr, FAnimNextGraphInstance* InParentGraphInstance = nullptr, FName InEntryPoint = NAME_None) const;

private:
	// Loads the graph data from the provided archive buffer and returns true on success, false otherwise
	UE_API bool LoadFromArchiveBuffer(const TArray<uint8>& SharedDataArchiveBuffer);

#if WITH_EDITORONLY_DATA
	// During graph compilation, if we have existing graph instances, we freeze them by releasing their memory before thawing them
	// Freezing is a partial release of resources that retains the necessary information to re-create things safely
	UE_API void FreezeGraphInstances();

	// During graph compilation, once compilation is done we thaw existing graph instances to reallocate their memory
	UE_API void ThawGraphInstances();
#endif
	
	friend class UAnimNextAnimationGraphFactory;
	friend class UAnimNextAnimationGraph_EditorData;
	friend class UAnimNextVariableEntry;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FModuleEditor;
	friend struct UE::AnimNext::FTestUtils;
	friend FAnimNextGraphInstance;
	friend class UAnimGraphNode_AnimNextGraph;
	friend UE::AnimNext::FExecutionContext;
	friend struct FAnimNextScheduleGraphTask;
	friend UE::AnimNext::AnimGraph::FAnimNextAnimGraphModule;
	friend class UE::AnimNext::Editor::FVariableCustomization;
	friend struct UE::AnimNext::FParametersProxy;
	friend struct UE::AnimNext::FPlayAnimSlotTrait;
	friend struct UE::AnimNext::FBlendStackCoreTrait;
	friend struct FAnimNextModuleAnimGraphComponent;

protected:
#if WITH_EDITORONLY_DATA
	mutable FCriticalSection GraphInstancesLock;

	// This is a list of live graph instances that have been allocated, used in the editor to reset instances when we re-compile/live edit
	mutable TSet<FAnimNextGraphInstance*> GraphInstances;
#endif

	// This is the execute method definition used by a graph to evaluate latent pins
	UPROPERTY()
	FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition;

	// Data for each entry point in this graph
	UPROPERTY()
	TArray<FAnimNextGraphEntryPoint> EntryPoints;

	// This is a resolved handle to the root trait in our graph, for each entry point 
	TMap<FName, FAnimNextTraitHandle> ResolvedRootTraitHandles;

	// This is an index into EntryPoints, for each entry point
	TMap<FName, int32> ResolvedEntryPoints;

	// This is the graph shared data used by the trait system, the output of FTraitReader
	// We de-serialize manually into this buffer from the archive buffer, this is never saved on disk
	TArray<uint8> SharedDataBuffer;

	// This is a list of all referenced UObjects in the graph shared data
	// We collect all the references here to make it quick and easy for the GC to query them
	// It means that object references in the graph shared data are not visited at runtime by the GC (they are immutable)
	// The shared data serialization archive stores indices to these to perform UObject serialization
	UPROPERTY()
	TArray<TObjectPtr<UObject>> GraphReferencedObjects;

	// This is a list of all referenced soft objects in the graph shared data
	// Used to serialize the soft objects correctly as we dont use FArchiveUObject
	UPROPERTY()
	TArray<FSoftObjectPath> GraphReferencedSoftObjects;

	// The entry point that this graph defaults to using
	UPROPERTY(EditAnywhere, Category = "Graph")
	FName DefaultEntryPoint = TEXT("Root");

	// Default state for this graph
	UPROPERTY()
	FAnimNextGraphState DefaultState;

#if WITH_EDITORONLY_DATA
	// This buffer holds the output of the FTraitWriter post compilation
	// We serialize it manually and it is discarded at runtime
	TArray<uint8> SharedDataArchiveBuffer;
#endif
};

#undef UE_API

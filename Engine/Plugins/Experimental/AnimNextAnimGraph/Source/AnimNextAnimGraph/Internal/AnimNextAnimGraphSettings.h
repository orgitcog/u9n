// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/AnimNextAnimationGraph.h"
#include "StructUtils/StructView.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectKey.h"

#include "AnimNextAnimGraphSettings.generated.h"

#define UE_API ANIMNEXTANIMGRAPH_API

struct FInstancedPropertyBag;
class UAnimNextAnimationGraph;
class UAnimNextAnimGraphSettings;
struct FAnimNextDataInterfacePayload;

USTRUCT()
struct FAnimNextAssetGraphMapping
{
	GENERATED_BODY()

private:
	friend UAnimNextAnimGraphSettings;

	// The object type/class that this mapping handles
	UPROPERTY(EditAnywhere, Category = "Mapping")
	TSoftClassPtr<UObject> AssetType;

	// The animation graph class that the type maps to
	UPROPERTY(EditAnywhere, Category = "Mapping")
	TSoftObjectPtr<const UAnimNextAnimationGraph> AnimationGraph;

	// The public variable that will be set when creating an instance of AnimationGraph, from an asset of AssetType.
	// For example, when mapping from an AnimSequence asset to a graph, the graph will have the AnimSequence asset set on a property of this name.
	// Leave as 'None' to not set the asset into a variable of the graph.
	// Must be an 'object' property of a compatible type to AssetType.
	UPROPERTY(EditAnywhere, Category = "Mapping")
	FName Variable;

	UPROPERTY(EditAnywhere, Category = "Mapping")
	TArray<TSoftObjectPtr<const UScriptStruct>> RequiredDataInterfaces;

	// Index of the next mapping for this AssetType
	int32 NextMappingIndex = INDEX_NONE;
};

UCLASS(MinimalAPI, Config=AnimNextAnimGraph, defaultconfig)
class UAnimNextAnimGraphSettings : public UObject
{
	GENERATED_BODY()

public:
	// Given an object, return an animation graph to instantiate and a set of native interface structs that can be used to communicate with the instance
	UE_API const UAnimNextAnimationGraph* GetGraphFromObject(const UObject* InObject) const;

	// Given an object, return an animation graph to instantiate and a set of native interface structs that can be used to communicate with the instance
	UE_API const UAnimNextAnimationGraph* GetGraphFromObject(const UObject* InObject, const FAnimNextDataInterfacePayload& InGraphPayload) const;

	// Given an asset class, return whether an animation graph can be made via GetGraphFromObject.
	UE_API bool CanGetGraphFromAssetClass(const UClass* InClass) const;

	// Given an object, return the name of the variable to inject into the graph's payload when making a payload for that object's graph (the
	// 'Variable' property of its mapping.
	UE_API FName GetInjectedVariableNameFromObject(const UObject* InObject) const;

	// Given an object and the resulting graph, generate native interface payloads for the graph.
	// If the native interface already exists in InOutGraphPayload, it will not be created by this call.
	UE_API void GetNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, FAnimNextDataInterfacePayload& InOutGraphPayload) const;
	UE_API void GetNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, TArray<FInstancedStruct>& InOutGraphPayload) const;

	// Given an object and the resulting graph, generate an interface payload for the graph, regardless if a native payload exists for the graph.
	// The generated instanced property bag will encompass public variables of ALL data interfaces implemented by the graph in one bundle
	UE_API void GetNonNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, FAnimNextDataInterfacePayload& InOutGraphPayload) const;
	UE_API void GetNonNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, FInstancedPropertyBag& InOutGraphPayload) const;

	// Rebuild mappings (loading assets synchronously if required) for lookup
	UE_API void LoadAndRebuildMappings(bool bLoadAsync);

	// Get the animation graph we run by default when hosting in a module
	const UAnimNextAnimationGraph* GetDefaultRunGraphHost() const
	{
		return DefaultRunGraphHost.Get();
	}

	// Gets all allowed asset classes that users can reference that map via GetGraphFromObject
	UFUNCTION()
	static UE_API TArray<UClass*> GetAllowedAssetClasses();

private:
#if WITH_EDITOR
	// UObject interface
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UE_API TArray<UClass*> GetAllowedAssetClassesImpl() const;

	UE_API void OnDefaultRunGraphHostLoaded(const UAnimNextAnimationGraph* AnimationGraph, bool bLoadAsync);
	UE_API void OnMappingAnimationGraphLoaded(const UAnimNextAnimationGraph* AnimationGraph, FAnimNextAssetGraphMapping& Mapping, int32 MappingIndex);
	UE_API void FinalizeAsyncLoad();
	
private:
	// The animation graph we run by default when hosting in a module
	UPROPERTY(Config, EditAnywhere, Category = "Graphs")
	TSoftObjectPtr<const UAnimNextAnimationGraph> DefaultRunGraphHost;

	// Mappings from assets to animation graphs
	UPROPERTY(Config, EditAnywhere, DisplayName = "Asset/Graph Mappings", Category = "Mappings")
	TArray<FAnimNextAssetGraphMapping> AssetGraphMappings;

	// Map derived from AssetGraphMappings
	// Maps an asset type (e.g. anim sequence) to the index in the mappings array for the first entry for that object
	TMap<TObjectKey<const UClass>, int32> AssetGraphMap;

	// Counter to keep track of how mappings have finished loading
	int32 NumMappingsLoaded = 0;

	// Flag to verify correct mapping rebuild behavior during startup
	std::atomic<bool> bMappingsBuiltAtLeastOnce = false;
};

#undef UE_API

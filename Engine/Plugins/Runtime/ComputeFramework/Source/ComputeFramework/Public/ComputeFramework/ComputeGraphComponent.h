// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ComputeFramework/ComputeGraphInstance.h"
#include "ComputeGraphComponent.generated.h"

#define UE_API COMPUTEFRAMEWORK_API

class UComputeGraph;

/** 
 * Component which holds a context for a UComputeGraph.
 * This object binds the graph to its data providers, and queues the execution. 
 */
UCLASS(MinimalAPI, Blueprintable, meta = (BlueprintSpawnableComponent))
class UComputeGraphComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API UComputeGraphComponent();
	UE_API ~UComputeGraphComponent();

	/** The Compute Graph asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compute")
	TObjectPtr<UComputeGraph> ComputeGraph = nullptr;

	/** Create all the Data Provider objects for a given binding object of the ComputeGraph. */
	UFUNCTION(BlueprintCallable, Category = "Compute")
	UE_API void CreateDataProviders(int32 InBindingIndex, UObject* InBindingObject);

	/** Destroy all associated DataProvider objects. */
	UFUNCTION(BlueprintCallable, Category = "Compute")
	UE_API void DestroyDataProviders();

	/** Queue the graph for execution at the next render update. */
	UFUNCTION(BlueprintCallable, Category = "Compute")
	UE_API void QueueExecute();

protected:
	//~ Begin UActorComponent Interface
	UE_API void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UE_API void SendRenderDynamicData_Concurrent() override;
	UE_API void DestroyRenderState_Concurrent() override;
	bool ShouldCreateRenderState() const override { return true; }
	//~ End UActorComponent Interface

private:
	UPROPERTY()
	FComputeGraphInstance ComputeGraphInstance;
};

#undef UE_API

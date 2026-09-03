// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EchoStateNetwork.generated.h"

/**
 * Echo State Network Node - Individual processing node in the network
 */
USTRUCT(BlueprintType)
struct FEchoStateNode
{
	GENERATED_BODY()

	/** Node identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "DeepTreeEcho")
	FString NodeID;

	/** Current node state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "DeepTreeEcho")
	TArray<float> State;

	/** Node activation level */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "DeepTreeEcho")
	float Activation;

	FEchoStateNode()
		: Activation(0.0f)
	{
	}
};

/**
 * Deep-Tree Echo-State Network - Hierarchical embodied cognition system
 */
UCLASS(Blueprintable, BlueprintType)
class DEEPTREEECHO_API UEchoStateNetwork : public UObject
{
	GENERATED_BODY()

public:
	UEchoStateNetwork();

	/** Network nodes organized in tree structure */
	UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho")
	TArray<FEchoStateNode> Nodes;

	/** Network depth (tree levels) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "DeepTreeEcho")
	int32 NetworkDepth;

	/** Reservoir size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "DeepTreeEcho")
	int32 ReservoirSize;

	/** Spectral radius for stability */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "DeepTreeEcho")
	float SpectralRadius;

	/** Initialize network */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
	void InitializeNetwork(int32 Depth, int32 Size);

	/** Process input through echo-state network */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
	TArray<float> ProcessInput(const TArray<float>& Input);

	/** Update network state */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
	void UpdateNetworkState(float DeltaTime);

	/** Get embodied cognitive output */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
	TArray<float> GetEmbodiedOutput();

	/** Reset network to initial state */
	UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
	void ResetNetwork();

protected:
	/** Propagate activation through tree */
	void PropagateActivation();

	/** Apply echo-state dynamics */
	void ApplyEchoDynamics();

	/** Calculate readout layer */
	TArray<float> CalculateReadout();

	TArray<float> InternalState;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoStateNetwork.h"

UEchoStateNetwork::UEchoStateNetwork()
	: NetworkDepth(5)
	, ReservoirSize(100)
	, SpectralRadius(0.95f)
{
}

void UEchoStateNetwork::InitializeNetwork(int32 Depth, int32 Size)
{
	NetworkDepth = Depth;
	ReservoirSize = Size;
	
	// Initialize nodes in tree structure
	Nodes.Empty();
	InternalState.Empty();
	
	int32 NodesPerLevel = 1;
	for (int32 Level = 0; Level < NetworkDepth; ++Level)
	{
		for (int32 i = 0; i < NodesPerLevel; ++i)
		{
			FEchoStateNode Node;
			Node.NodeID = FString::Printf(TEXT("Node_L%d_%d"), Level, i);
			Node.Activation = 0.0f;
			Node.State.Init(0.0f, ReservoirSize);
			Nodes.Add(Node);
		}
		NodesPerLevel *= 2; // Binary tree
	}
	
	InternalState.Init(0.0f, ReservoirSize);
	
	UE_LOG(LogTemp, Log, TEXT("EchoStateNetwork: Initialized with depth %d, reservoir size %d, total nodes %d"), 
		NetworkDepth, ReservoirSize, Nodes.Num());
}

TArray<float> UEchoStateNetwork::ProcessInput(const TArray<float>& Input)
{
	// Process input through echo-state dynamics
	for (int32 i = 0; i < FMath::Min(Input.Num(), InternalState.Num()); ++i)
	{
		InternalState[i] = Input[i];
	}
	
	ApplyEchoDynamics();
	PropagateActivation();
	
	return CalculateReadout();
}

void UEchoStateNetwork::UpdateNetworkState(float DeltaTime)
{
	ApplyEchoDynamics();
	PropagateActivation();
}

TArray<float> UEchoStateNetwork::GetEmbodiedOutput()
{
	return CalculateReadout();
}

void UEchoStateNetwork::ResetNetwork()
{
	for (FEchoStateNode& Node : Nodes)
	{
		Node.Activation = 0.0f;
		for (float& StateValue : Node.State)
		{
			StateValue = 0.0f;
		}
	}
	
	for (float& StateValue : InternalState)
	{
		StateValue = 0.0f;
	}
	
	UE_LOG(LogTemp, Log, TEXT("EchoStateNetwork: Network reset"));
}

void UEchoStateNetwork::PropagateActivation()
{
	// Propagate activation through tree structure
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		FEchoStateNode& Node = Nodes[i];
		
		// Update activation based on state
		float TotalActivation = 0.0f;
		for (float StateValue : Node.State)
		{
			TotalActivation += StateValue;
		}
		
		Node.Activation = FMath::Tanh(TotalActivation / ReservoirSize);
	}
}

void UEchoStateNetwork::ApplyEchoDynamics()
{
	// Apply echo-state reservoir dynamics
	for (FEchoStateNode& Node : Nodes)
	{
		for (int32 i = 0; i < Node.State.Num(); ++i)
		{
			// Leaky integration with spectral radius
			float NewValue = SpectralRadius * Node.State[i];
			
			// Add random recurrent connections
			if (InternalState.IsValidIndex(i))
			{
				NewValue += InternalState[i] * 0.1f;
			}
			
			Node.State[i] = FMath::Tanh(NewValue);
		}
	}
}

TArray<float> UEchoStateNetwork::CalculateReadout()
{
	TArray<float> Output;
	Output.Init(0.0f, 10); // Default output size
	
	// Calculate output from network state
	for (int32 i = 0; i < Output.Num(); ++i)
	{
		float Sum = 0.0f;
		for (const FEchoStateNode& Node : Nodes)
		{
			Sum += Node.Activation;
		}
		Output[i] = Sum / Nodes.Num();
	}
	
	return Output;
}

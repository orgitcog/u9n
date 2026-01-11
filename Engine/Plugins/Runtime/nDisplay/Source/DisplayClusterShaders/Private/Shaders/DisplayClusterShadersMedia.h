// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRDGBuilder;
struct FDisplayClusterShaderParameters_MediaPQ;


/**
 * API for media shaders
 */
class FDisplayClusterShadersMedia
{
public:

	/** Adds Linear-To-PQ encoding pass (API wrapper) */
	static void AddLinearToPQPass(FRDGBuilder& GraphBuilder, const FDisplayClusterShaderParameters_MediaPQ& Parameters);

	/** Adds PQ-To-Linear decoding pass (API wrapper) */
	static void AddPQToLinearPass(FRDGBuilder& GraphBuilder, const FDisplayClusterShaderParameters_MediaPQ& Parameters);
};

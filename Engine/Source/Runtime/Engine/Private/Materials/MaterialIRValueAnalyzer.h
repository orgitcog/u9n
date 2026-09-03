// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRDebug.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIRInternal.h"
#include "Materials/MaterialInsights.h"
#include "MaterialShared.h"
#include "Engine/Texture.h"

struct FMaterialIRValueAnalyzer
{
	//
	void Setup(UMaterial* InMaterial, FMaterialIRModule* InModule, FMaterialCompilationOutput* InCompilationOutput, FMaterialInsights* InInsights = nullptr);
		
	//
	void Analyze(MIR::FValue* Value);

	//
	void PropagateStateInStage(MIR::FValue* Value, MIR::EStage Stage);

	//
	UMaterial* Material{};
	
	//
	FMaterialIRModule* Module{};
	
	//
	FMaterialCompilationOutput* CompilationOutput{};
		
	//
	TMap<UE::Shader::FValue, uint32> UniformDefaultValueOffsets{};
		
	//
	TArray<uint32, TInlineAllocator<8>> FreeOffsetsPerNumComponents[3];

	// List of enabled shader environment defines.
	TSet<FName> EnvironmentDefines;
		
	//
	FMaterialInsights* Insights{}; // Optional
};

#endif // #if WITH_EDITOR

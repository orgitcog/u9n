// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIR.h"
#include "MaterialShared.h"

#if WITH_EDITOR

// This class represents the intermediate representation (IR) of a material build.
// The IRModule includes an IR value graph, produced through expression analysis,
// as well as metadata on resource usage and reflection. The IR graph serves as an
// abstract representation of the material and must be translated into a target backend
// such as HLSL or specific Preshader opcodes for execution.
//
// This class is designed to be backend-agnostic, meaning it does not contain any
// HLSL code nor does it configure a MaterialCompilationOutput instance. The data
// stored within this class should be sufficient to enable translation to any supported
// backend without requiring additional processing or validation.
class FMaterialIRModule
{
public:
	// Represents an error encountered during material processing.
	struct FError
	{
		// The expression that caused the error.
		UMaterialExpression* Expression;

		// Description of the error.
		FString Message;
	};

	// Stores information about the resources used by the translated material.
	struct FStatistics
	{
		// Tracks external inputs used per frequency.
		TBitArray<> ExternalInputUsedMask[MIR::NumStages]; 

		// Number of vertex texture coordinates used.
		int32 NumVertexTexCoords;

		// Number of pixel texture coordinates used.
		int32 NumPixelTexCoords;
	};

public:
	FMaterialIRModule();
	~FMaterialIRModule();

	// Clears the module, releasing all stored data.
	void Empty();

	// Returns the shader platform associated with this module.
	EShaderPlatform GetShaderPlatform() const { return ShaderPlatform; }

	// Retrieves the material compilation output.
	const FMaterialCompilationOutput& GetCompilationOutput() const { return CompilationOutput; }

	// Returns the material outputs for a given stage.
	TArrayView<const MIR::FSetMaterialOutput* const> GetOutputs(MIR::EStage Stage) const { return Outputs[Stage]; }

	// Retrieves the root block (i.e. the "main" scope) for a specific shader stage.
	const MIR::FBlock& GetRootBlock(MIR::EStage Stage) const { return *RootBlock[Stage]; }

	// Returns a list of all environment define names this module requires to be enabled for shader compilation.
	const TSet<FName>& GetEnvironmentDefines() const { return EnvironmentDefines; }

	// Provides access to the translated material statistics.
	const FStatistics& GetStatistics() const { return Statistics; }

	// Provides mutable access to the translated material statistics.
	FStatistics& GetStatistics() { return Statistics; }

	// Retrieves parameter info for a given parameter ID.
	const FMaterialParameterInfo& GetParameterInfo(uint32 ParameterId) const { return ParameterIdToData[ParameterId].Key; }

	// Retrieves parameter metadata for a given parameter ID.
	const FMaterialParameterMetadata& GetParameterMetadata(uint32 ParameterId) const { return ParameterIdToData[ParameterId].Value; }

	// Stores a user-defined string and returns a pointer to it.
	const TCHAR* PushUserString(FString InString);

	// Checks if the module is valid (i.e., contains no errors).
	bool IsValid() const { return Errors.IsEmpty(); }

	// Returns a list of errors encountered during processing.
	TArrayView<const FError> GetErrors() const { return Errors; }

	// Reports a translation error.
	void AddError(UMaterialExpression* Expression, FString Message);

private:
	// Target shader platform.
	EShaderPlatform ShaderPlatform;

	// Compilation output data.
	FMaterialCompilationOutput CompilationOutput;

	// Memory allocator used to allocate IR data (values, payloads, etc).
	FMemStackBase Allocator{};

	// List of all the IR values contained in this module.
	TArray<MIR::FValue*> Values;

	// Output nodes per stage.
	TArray<MIR::FSetMaterialOutput*> Outputs[MIR::NumStages];

	// Root blocks per stage.
	MIR::FBlock* RootBlock[MIR::NumStages];

	// Compilation statistics.
	FStatistics Statistics;

	// Maps parameter info to IDs.
	TMap<FMaterialParameterInfo, uint32> ParameterInfoToId;

	// Parameter metadata.
	TArray<TPair<FMaterialParameterInfo, FMaterialParameterMetadata>> ParameterIdToData;

	// Stores user-defined strings.	
	TArray<FString> UserStrings;

	// Environment define names for shader compilation.
	TSet<FName> EnvironmentDefines;

	// List of compilation errors.
	TArray<FError> Errors;

	friend MIR::FEmitter;
	friend FMaterialIRModuleBuilder;
	friend FMaterialIRModuleBuilderImpl;
};

#endif // #if WITH_EDITOR

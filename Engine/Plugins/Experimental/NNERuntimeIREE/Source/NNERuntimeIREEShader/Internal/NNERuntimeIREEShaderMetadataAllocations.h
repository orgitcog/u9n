// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "ShaderParameterMetadata.h"
#include "Templates/UniquePtr.h"

struct NNERUNTIMEIREESHADER_API FNNERuntimeIREEShaderParametersMetadataAllocations
{
	/** Allocated metadata. Should include the parent metadata allocation. */
	TUniquePtr<FShaderParametersMetadata> ShaderParameterMetadatas;
	/** Allocated name dictionary. */
	TArray<FString> Names;

	FNNERuntimeIREEShaderParametersMetadataAllocations() = default;
	FNNERuntimeIREEShaderParametersMetadataAllocations(FNNERuntimeIREEShaderParametersMetadataAllocations& Other) = delete;

	~FNNERuntimeIREEShaderParametersMetadataAllocations();
};
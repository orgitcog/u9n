// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCompiler.h: Platform independent shader compilation definitions.
=============================================================================*/

#pragma once

#include "ShaderCompilerCore.h"
#include "ShaderCompilerJobTypes.h"


/** Wrapper for internal shader compiler utilities that can be accessed by plugins for internal use. */
class FShaderCompileInternalUtilities
{
public:
	/** Execute the specified (single or pipeline) shader compile job. */
	static ENGINE_API void ExecuteShaderCompileJob(FShaderCommonCompileJob& Job);
};


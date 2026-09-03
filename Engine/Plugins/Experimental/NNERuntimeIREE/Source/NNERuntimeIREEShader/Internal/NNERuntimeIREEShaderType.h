// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GlobalShader.h"
#include "Shader.h"
#if WITH_EDITOR
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderCompiler.h"
#endif

class FShaderCommonCompileJob;
class FShaderCompileJob;

class FNNERuntimeIREEResource;


struct FNNERuntimeIREEShaderPermutationParameters : public FShaderPermutationParameters
{
	FNNERuntimeIREEShaderPermutationParameters(EShaderPlatform InPlatform)
		: FShaderPermutationParameters(InPlatform)
	{
	}
};

class FNNERuntimeIREEShaderType : public FShaderType
{
public:
	struct FParameters : public FShaderType::FParameters
	{
		const FShaderParametersMetadata& ShaderParamMetadata;

		FParameters(
			const FShaderParametersMetadata& InShaderParamMetadata
			)
			: ShaderParamMetadata(InShaderParamMetadata)
		{
		}
	};

	struct CompiledShaderInitializerType : FGlobalShaderType::CompiledShaderInitializerType
	{
		const FString DebugDescription;

		CompiledShaderInitializerType(
			const FShaderType* InType,
			const FParameters* InParameters,
			int32 InPermutationId,
			const FShaderCompilerOutput& CompilerOutput,
			const FSHAHash& InKernelShaderMapHash,
			const FString& InDebugDescription
			)
			: FGlobalShaderType::CompiledShaderInitializerType(
				InType,
				InParameters,
				InPermutationId,
				CompilerOutput, 
				InKernelShaderMapHash,
				nullptr,
				nullptr
				)
			, DebugDescription(InDebugDescription)
		{
		}
	};

	FNNERuntimeIREEShaderType(
		FTypeLayoutDesc& InTypeLayout,
		const TCHAR* InName,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		uint32 InFrequency,					// ugly - ignored for but needed for IMPLEMENT_SHADER_TYPE macro magic
		int32 InTotalPermutationCount,
		ConstructSerializedType InConstructSerializedRef,
		ConstructCompiledType InConstructCompiledRef,
		ShouldCompilePermutationType InShouldCompilePermutationRef,
		ShouldPrecachePermutationType InShouldPrecachePermutationRef,
		GetRayTracingPayloadTypeType InGetRayTracingPayloadTypeRef,
		GetShaderBindingLayoutType InGetShaderBindingLayoutTypeRef,
#if WITH_EDITOR
		ModifyCompilationEnvironmentType InModifyCompilationEnvironmentRef,
		ValidateCompiledResultType InValidateCompiledResultRef,
		GetOverrideJobPriorityType InGetOverrideJobPriorityRef,
#endif // WITH_EDITOR
		uint32 InTypeSize,
		const FShaderParametersMetadata* InRootParametersMetadata = nullptr
#if WITH_EDITOR
		, GetPermutationIdStringType InGetPermutationIdStringRef = nullptr
#endif // WITH_EDITOR
		):
		FShaderType(
			EShaderTypeForDynamicCast::NNERuntimeIREE, 
			InTypeLayout, 
			InName, 
			InSourceFilename, 
			InFunctionName, 
			SF_Compute, 
			InTotalPermutationCount,
			InConstructSerializedRef,
			InConstructCompiledRef,
			InShouldCompilePermutationRef,
			InShouldPrecachePermutationRef,
			InGetRayTracingPayloadTypeRef,
			InGetShaderBindingLayoutTypeRef,
#if WITH_EDITOR
			InModifyCompilationEnvironmentRef,
			InValidateCompiledResultRef,
			InGetOverrideJobPriorityRef,
#endif // WITH_EDITOR
			InTypeSize,
			InRootParametersMetadata
#if WITH_EDITOR
			, InGetPermutationIdStringRef
#endif // WITH_EDITOR
			)
	{
	}

#if WITH_EDITOR
	void BeginCompileShader(
		uint32 ShaderMapId,
		int32 PermutationId,
		const FNNERuntimeIREEResource* InKernel,
		FSharedShaderCompilerEnvironment* InCompilationEnvironment,
		EShaderPlatform Platform,
		TArray<FShaderCommonCompileJobPtr>& InOutNewJobs,
		FShaderTarget Target
		);

	/**
	 * Either creates a new instance of this type or returns an equivalent existing shader.
	 */
	FShader* FinishCompileShader(
		const FSHAHash& InKernelShaderMapHash,
		const FShaderCompileJob& CurrentJob,
		const FString& InDebugDescription
		) const;
#endif // WITH_EDITOR

	/**
	 * Checks if the shader type should be cached.
	 * @return True if this shader type should be cached.
	 */
	bool ShouldCache(EShaderPlatform InPlatform, const FNNERuntimeIREEResource* Kernel) const
	{
		return ShouldCompilePermutation(FNNERuntimeIREEShaderPermutationParameters(InPlatform));
	}

#if WITH_EDITOR
protected:
	/**
	 * Sets up the environment used to compile an instance of this shader type.
	 * @param InPlatform - Platform to compile for.
	 * @param OutEnvironment - The shader compile environment that the function modifies.
	 */
	void SetupCompileEnvironment(EShaderPlatform InPlatform, const FNNERuntimeIREEResource* Kernel, FShaderCompilerEnvironment& OutEnvironment) const
	{
		ModifyCompilationEnvironment(FNNERuntimeIREEShaderPermutationParameters(InPlatform), OutEnvironment);

		if (FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(InPlatform) != ERHIFeatureSupport::Unsupported)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}
#endif // WITH_EDITOR
};
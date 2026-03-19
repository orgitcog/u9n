// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIShaderPlatform.h"
#include "Serialization/JsonSerializerMacros.h"

#include "IREECompilerRDG.generated.h"

USTRUCT()
struct FIREECompilerRDGBuildTargetResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString ShaderPlatform;

	UPROPERTY()
	FString RelativeDirPath;
	
	UPROPERTY()
	TArray<FString> SharedLibraryFileNames;

	UPROPERTY()
	FString VmfbFileName;
};

USTRUCT()
struct FIREECompilerRDGResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FIREECompilerRDGBuildTargetResult> BuildTargetResult;
};

#ifdef WITH_IREE_DRIVER_RDG
#if WITH_EDITOR

namespace UE::IREE::Compiler::RDG
{
	struct FBuildTarget : public FJsonSerializable
	{
		FString ShaderPlatform;
		FString CompilerArguments;
		FString LinkerArguments;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("ShaderPlatform", ShaderPlatform);
			JSON_SERIALIZE("CompilerArguments", CompilerArguments);
		END_JSON_SERIALIZER
	};

	struct FBuildConfig : public FJsonSerializable
	{
		TArray<FString> ImporterCommand;
		FString ImporterArguments;
		TArray<FString> CompilerCommand;
		TArray<FBuildTarget> BuildTargets;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE_ARRAY("ImporterCommand", ImporterCommand);
			JSON_SERIALIZE("ImporterArguments", ImporterArguments);
			JSON_SERIALIZE_ARRAY("CompilerCommand", CompilerCommand);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("Targets", BuildTargets, FBuildTarget);
		END_JSON_SERIALIZER
	};

	class IREEDRIVERRDG_API FCompiler
	{
	private:
		FCompiler(const ITargetPlatform* TargetPlatform, const FString& InImporterCommand, const FString& InImporterArguments, const FString& InCompilerCommand, const FString& InSharedLibExt, TConstArrayView<FBuildTarget> InBuildTargets);

	public:
		~FCompiler() {};
		static TUniquePtr<FCompiler> Make(const ITargetPlatform* TargetPlatform);

	public:
		bool ImportOnnx(TConstArrayView<uint8> InFileData, const FString& InModelName, const FString& InOutputDir, TArray64<uint8>& OutMlirData);
		bool CompileMlir(TConstArrayView<uint8> InFileData, const FString& InModelName, const FString& InOutputDir, TConstArrayView<EShaderPlatform> ShaderPlatforms, FIREECompilerRDGResult& OutCompilerResult);

	private:
		const ITargetPlatform* TargetPlatform;
		FString ImporterCommand;
		FString ImporterArguments;
		FString CompilerCommand;
		TArray<FBuildTarget> BuildTargets;
	};
} // namespace UE::IREE::Compiler::RDG

#endif // WITH_EDITOR
#endif // WITH_IREE_DRIVER_RDG
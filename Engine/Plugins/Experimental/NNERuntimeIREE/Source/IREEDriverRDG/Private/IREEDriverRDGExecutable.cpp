// Copyright Epic Games, Inc. All Rights Reserved.

#include "IREEDriverRDGExecutable.h"

#ifdef WITH_IREE_DRIVER_RDG

#include "IREEDriverRDGLog.h"
#include "IREEDriverRDGShaderParametersMetadata.h"
#include "Misc/FileHelper.h"
#include "NNERuntimeIREEShaderShared.h"
#include "Serialization/MemoryReader.h"

#if PLATFORM_MICROSOFT
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include "Microsoft/AllowMicrosoftPlatformAtomics.h"
#endif // PLATFORM_MICROSOFT
THIRD_PARTY_INCLUDES_START
#include "iree/hal/utils/executable_debug_info.h"

// flatcc schemas:
#include "iree/base/internal/flatcc/parsing.h"
#include "iree/schemas/executable_debug_info_reader.h"
#include "iree/schemas/executable_debug_info_verifier.h"
#include "iree/schemas/unreal_executable_def_reader.h"
#include "iree/schemas/unreal_executable_def_verifier.h"
THIRD_PARTY_INCLUDES_END
#if PLATFORM_MICROSOFT
#include "Microsoft/HideMicrosoftPlatformAtomics.h"
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif // PLATFORM_MICROSOFT

namespace UE::IREE::HAL::RDG
{

namespace Private
{

TArray<uint32> GetBufferBindings(const FIREEDriverRDGShaderParametersMetadata& Metadata)
{
	TArray<uint32> Result;
	for (const FIREEDriverRDGShaderParametersMetadataEntry& Entry : Metadata.Entries)
	{
		if (Entry.Type == FIREEDriverRDGUniformBufferBaseType::BUFFER_UAV)
		{
			Result.Add(Entry.Binding);
		}
	}
	return Result;
}

class FExecutable
{
public:
	static iree_status_t Create(iree_allocator_t HostAllocator, const FString& BasePath, const iree_hal_executable_params_t* ExecutableParams, iree_hal_executable_t** OutExecutable)
	{
		SCOPED_NAMED_EVENT_TEXT("FExecutable::Create", FColor::Magenta);

		check(OutExecutable);

		FExecutable* Executable;
		IREE_RETURN_IF_ERROR(iree_allocator_malloc(HostAllocator, sizeof(*Executable), (void**)&Executable));
		iree_hal_resource_initialize((const void*)&FExecutable::VTable, &Executable->Resource);
		Executable->HostAllocator = HostAllocator;

		iree_hal_unreal_ExecutableDef_table_t ExecutableDef = iree_hal_unreal_ExecutableDef_as_root(ExecutableParams->executable_data.data);
		iree_hal_unreal_UnrealShaderDef_vec_t UnrealShaderVec = iree_hal_unreal_ExecutableDef_unreal_shaders_get(ExecutableDef);
		iree_host_size_t UnrealShaderCount = iree_hal_unreal_UnrealShaderDef_vec_len(UnrealShaderVec);

		for (iree_host_size_t i = 0; i < UnrealShaderCount; i++)
		{
			iree_hal_unreal_UnrealShaderDef_table_t UnrealShaderDef = iree_hal_unreal_UnrealShaderDef_vec_at(UnrealShaderVec, i);
			flatbuffers_string_t SourceFileName = iree_hal_unreal_UnrealShaderDef_source_file_name_get(UnrealShaderDef);
			// flatbuffers_string_t ModuleName = iree_hal_unreal_UnrealShaderDef_module_name_get(UnrealShaderDef);
			// flatbuffers_string_t EntryPoint = iree_hal_unreal_UnrealShaderDef_entry_point_get(UnrealShaderDef);
		
			const FString ShaderMapFileNameNoExt(ANSI_TO_TCHAR(SourceFileName));
			const FString ShaderMapFileName = ShaderMapFileNameNoExt + TEXT(".ireeshadermap");
			const FString ShaderMapFullPath = FPaths::Combine(BasePath, ShaderMapFileName);

			Executable->DebugShaderInfos.Add(ShaderMapFileNameNoExt);

			TArray/*64*/<uint8> Data;
			if (!FFileHelper::LoadFileToArray(Data, *ShaderMapFullPath))
			{
				return iree_make_status(IREE_STATUS_NOT_FOUND, "Could not load shader map.");
			}

			FMemoryReaderView Reader(Data, /*bIsPersitent =*/ true);

			FIREEDriverRDGShaderParametersMetadata IREEShaderParametersMetadata{};
			FIREEDriverRDGShaderParametersMetadata::StaticStruct()->SerializeBin(Reader, &IREEShaderParametersMetadata);

			TUniquePtr<FNNERuntimeIREEShaderParametersMetadataAllocations> ShaderParameterMetadataAllocations = MakeUnique<FNNERuntimeIREEShaderParametersMetadataAllocations>();
			FShaderParametersMetadata* ShaderParametersMetadata = HAL::RDG::BuildShaderParametersMetadata(IREEShaderParametersMetadata, *ShaderParameterMetadataAllocations);

			TUniquePtr<FNNERuntimeIREEResource> KernelResource = MakeUnique<FNNERuntimeIREEResource>();
			KernelResource->SetupResource(
				GMaxRHIFeatureLevel,
				ShaderMapFileNameNoExt,
				FString(),
				FString(),
				FString(),
				MoveTemp(ShaderParameterMetadataAllocations),
				ShaderParametersMetadata,
				FName(),
				GetBufferBindings(IREEShaderParametersMetadata)
			);
			

			if (!KernelResource->SerializeShaderMap(Reader))
			{
				return iree_make_status(IREE_STATUS_NOT_FOUND, "Loaded shader map is not valid/complete.");
			}

			Executable->KernelResources.Add(MoveTemp(KernelResource));
		}

		*OutExecutable = (iree_hal_executable_t*)Executable;
		return iree_ok_status();
	}

	static iree_status_t GetResource(iree_hal_executable_t* BaseExecutable, int32 EntryPoint, const FNNERuntimeIREEResource** OutResource)
	{
		FExecutable* Executable = Cast(BaseExecutable);

		if (EntryPoint < 0 || EntryPoint >= Executable->KernelResources.Num())
		{
			return iree_make_status(IREE_STATUS_OUT_OF_RANGE, "Invalid entry point index %i requested from executable", EntryPoint);
		}

		*OutResource = Executable->KernelResources[EntryPoint].Get();

		return iree_ok_status();
	}

private:
	static FExecutable* Cast(iree_hal_executable_t* Executable)
	{
		checkf(iree_hal_resource_is(Executable, &FExecutable::VTable), TEXT("FExecutable: type does not match"));
		return (FExecutable*)Executable;
	}

	static void Destroy(iree_hal_executable_t* BaseExecutable)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		FExecutable* Executable = Cast(BaseExecutable);
		iree_allocator_free(Executable->HostAllocator, Executable);
	}

	inline static const iree_hal_executable_vtable_t VTable = []
	{
		iree_hal_executable_vtable_t Result =
		{
			.destroy = FExecutable::Destroy,
		};
		return Result;

	}();

private:
	iree_hal_resource_t Resource;
	iree_allocator_t HostAllocator;
	TArray<TUniquePtr<FNNERuntimeIREEResource>> KernelResources;

	// Debug
	TArray<FString> DebugShaderInfos;
};

} // namespace Private

iree_status_t ExecutableCreate(iree_allocator_t HostAllocator, const FString& BasePath, const iree_hal_executable_params_t* ExecutableParams, iree_hal_executable_t** OutExecutable)
{
	return Private::FExecutable::Create(HostAllocator, BasePath, ExecutableParams, OutExecutable);
}

iree_status_t ExecutableGetResource(iree_hal_executable_t* Executable, int32 EntryPoint, const FNNERuntimeIREEResource** OutResource)
{
	return Private::FExecutable::GetResource(Executable, EntryPoint, OutResource);
}

} // UE::IREE

#endif // WITH_IREE_DRIVER_RDG
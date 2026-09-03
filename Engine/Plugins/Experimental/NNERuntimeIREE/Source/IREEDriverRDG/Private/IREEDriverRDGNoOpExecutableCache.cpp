// Copyright Epic Games, Inc. All Rights Reserved.

#include "IREEDriverRDGNoOpExecutableCache.h"

#ifdef WITH_IREE_DRIVER_RDG

#include "IREEDriverRDGExecutable.h"
#include "IREEDriverRDGLog.h"

namespace UE::IREE::HAL::RDG
{

namespace Private
{

class FNoOpExecutableCache
{
public:
	static iree_status_t Create(iree_allocator_t HostAllocator, const FString& BasePath, iree_hal_executable_cache_t** OutExecutableCache)
	{
		check(OutExecutableCache);

		FNoOpExecutableCache* ExecutableCache;
		IREE_RETURN_IF_ERROR(iree_allocator_malloc(HostAllocator, sizeof(*ExecutableCache), (void**)&ExecutableCache));
		iree_hal_resource_initialize((const void*)&FNoOpExecutableCache::VTable, &ExecutableCache->Resource);
		ExecutableCache->HostAllocator = HostAllocator;
		ExecutableCache->BasePath = BasePath;

		*OutExecutableCache = (iree_hal_executable_cache_t*)ExecutableCache;
		return iree_ok_status();
	}

	FString GetBasePath() const
	{
		return BasePath;
	}

private:
	static FNoOpExecutableCache* Cast(iree_hal_executable_cache_t* ExecutableCache)
	{
		checkf(iree_hal_resource_is(ExecutableCache, &FNoOpExecutableCache::VTable), TEXT("FNoOpExecutableCache: type does not match"));
		return (FNoOpExecutableCache*)ExecutableCache;
	}

	static void Destroy(iree_hal_executable_cache_t* BaseExecutableCache)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		FNoOpExecutableCache* ExecutableCache = Cast(BaseExecutableCache);
		iree_allocator_free(ExecutableCache->HostAllocator, ExecutableCache);
	}

	static bool CanPrepareFormat(iree_hal_executable_cache_t* BaseExecutableCache, iree_hal_executable_caching_mode_t CachingMode, iree_string_view_t ExecutableFormat) 
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s format: %*.hs"), StringCast<TCHAR>(__FUNCTION__).Get(), (int)ExecutableFormat.size, ExecutableFormat.data);
#endif
		if (iree_string_view_equal(ExecutableFormat, iree_make_cstring_view("vulkan-spirv-fb")))
		{
			return true;
		}
		// else if (iree_string_view_equal(ExecutableFormat, iree_make_cstring_view("vulkan-spirv-fb-ptr")))
		// {
		// 	return iree_all_bits_set(executable_cache->logical_device->enabled_features(), IREE_HAL_VULKAN_FEATURE_ENABLE_BUFFER_DEVICE_ADDRESSES);
		// }
		return false;
	}

	static iree_status_t PrepareExecutable(iree_hal_executable_cache_t* BaseExecutableCache, const iree_hal_executable_params_t* ExecutableParams, iree_hal_executable_t** OutExecutable) 
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		if (!FNoOpExecutableCache::CanPrepareFormat(BaseExecutableCache, ExecutableParams->caching_mode, ExecutableParams->executable_format))
		{
			return iree_make_status(IREE_STATUS_NOT_FOUND, "No executable implementation registered for the given executable format '%.*s'", (int)ExecutableParams->executable_format.size, ExecutableParams->executable_format.data);
		}

		FNoOpExecutableCache* ExecutableCache = Cast(BaseExecutableCache);

		return ExecutableCreate(ExecutableCache->HostAllocator, ExecutableCache->GetBasePath(), ExecutableParams, OutExecutable);
	}

	static const iree_hal_executable_cache_vtable_t VTable;

	iree_hal_resource_t Resource;
	iree_allocator_t HostAllocator;
	FString BasePath;
};

const iree_hal_executable_cache_vtable_t FNoOpExecutableCache::VTable = 
{
	.destroy = FNoOpExecutableCache::Destroy,
	.can_prepare_format = FNoOpExecutableCache::CanPrepareFormat,
	.prepare_executable = FNoOpExecutableCache::PrepareExecutable
};

} // namespace Private

iree_status_t NoOpExecutableCacheCreate(iree_allocator_t HostAllocator, const FString& BasePath, iree_hal_executable_cache_t** OutExecutableCache)
{
	return Private::FNoOpExecutableCache::Create(HostAllocator, BasePath, OutExecutableCache);
}

} // UE::IREE

#endif // WITH_IREE_DRIVER_RDG
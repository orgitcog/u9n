// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanDevice.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanMemory.h"
#include "VulkanResources.h"
#include "VulkanQueue.h"
#include "VulkanQuery.h"
#include "GPUProfiler.h"

class FVulkanSamplerState;
class FVulkanDynamicRHI;
class FVulkanDescriptorSetCache;
class FVulkanDescriptorPool;
class FVulkanDescriptorPoolsManager;
class FVulkanBindlessDescriptorManager;
class FVulkanCommandListContextImmediate;
class FVulkanTransientHeapCache;
class FVulkanDeviceExtension;
class FVulkanRenderPassManager;
class FVulkanRayTracingCompactionRequestHandler;
class FVulkanRayTracingPipelineLibraryCache;
struct FVulkanParallelRenderPassInfo;

#define VULKAN_USE_DEBUG_NAMES 1
#if VULKAN_USE_DEBUG_NAMES
#define VULKAN_SET_DEBUG_NAME(Device, Type, Handle, Format, ...) (Device).VulkanSetObjectName(Type, (uint64)Handle, *FString::Printf(Format, __VA_ARGS__))
#else
#define VULKAN_SET_DEBUG_NAME(Device, Type, Handle, Format, ...) do{}while(0)
#endif

struct FOptionalVulkanDeviceExtensions
{
	union
	{
		struct
		{
			// Optional Extensions
			uint64 HasEXTValidationCache : 1;
			uint64 HasMemoryPriority : 1;
			uint64 HasMemoryBudget : 1;
			uint64 HasEXTASTCDecodeMode : 1;
			uint64 HasEXTFragmentDensityMap : 1;
			uint64 HasEXTFragmentDensityMap2 : 1;
			uint64 HasKHRFragmentShadingRate : 1;
			uint64 HasKHRFragmentShaderBarycentric : 1;
			uint64 HasEXTFullscreenExclusive : 1;
			uint64 HasImageAtomicInt64 : 1;
			uint64 HasAccelerationStructure : 1;
			uint64 HasRayTracingPipeline : 1;
			uint64 HasRayQuery : 1;
			uint64 HasKHRPipelineLibrary : 1;
			uint64 HasDeferredHostOperations : 1;
			uint64 HasEXTCalibratedTimestamps : 1;
			uint64 HasEXTDescriptorBuffer : 1;
			uint64 HasEXTDeviceFault : 1;
			uint64 HasEXTLoadStoreOpNone : 1;
			uint64 HasEXTMeshShader : 1;
			uint64 HasEXTToolingInfo : 1;
			uint64 HasEXTImageCompressionControl : 1;
			uint64 HasEXTMutableDescriptorType : 1;

			// Vendor specific
			uint64 HasAMDBufferMarker : 1;
			uint64 HasNVDiagnosticCheckpoints : 1;
			uint64 HasNVDeviceDiagnosticConfig : 1;
			uint64 HasANDROIDExternalMemoryHardwareBuffer : 1;

			// Promoted to 1.1
			uint64 HasKHRMultiview : 1;
			uint64 HasKHR16bitStorage : 1;
			uint64 HasKHRSamplerYcbcrConversion : 1;

			// Promoted to 1.2
			uint64 HasKHRRenderPass2 : 1;
			uint64 HasKHRImageFormatList : 1;
			uint64 HasKHRShaderAtomicInt64 : 1;
			uint64 HasEXTScalarBlockLayout : 1;
			uint64 HasBufferDeviceAddress : 1;
			uint64 HasSPIRV_14 : 1;
			uint64 HasShaderFloatControls : 1;
			uint64 HasKHRShaderFloat16 : 1;
			uint64 HasEXTDescriptorIndexing : 1;
			uint64 HasSeparateDepthStencilLayouts : 1;
			uint64 HasEXTHostQueryReset : 1;
			uint64 HasQcomRenderPassShaderResolve : 1;
			uint64 HasKHRDepthStencilResolve : 1;
			uint64 HasKHRTimelineSemaphore : 1;

			// Promoted to 1.3
			uint64 HasEXTTextureCompressionASTCHDR : 1;
			uint64 HasKHRMaintenance4 : 1;
			uint64 HasKHRMaintenance5 : 1;
			uint64 HasKHRSynchronization2 : 1;
			uint64 HasEXTSubgroupSizeControl : 1;
			uint64 HasEXTPipelineCreationCacheControl : 1;
		};
		uint64 Packed;
	};

	FOptionalVulkanDeviceExtensions()
	{
		static_assert(sizeof(Packed) == sizeof(FOptionalVulkanDeviceExtensions), "More bits needed for Packed!");
		Packed = 0;
	}

	inline bool HasGPUCrashDumpExtensions() const
	{
		return HasAMDBufferMarker || HasNVDiagnosticCheckpoints;
	}

	inline bool HasRaytracingExtensions() const
	{
		return 
			HasAccelerationStructure && 
			((HasRayTracingPipeline && HasKHRPipelineLibrary) || HasRayQuery) &&
			HasEXTDescriptorIndexing &&
			HasBufferDeviceAddress && 
			HasDeferredHostOperations && 
			HasSPIRV_14 && 
			HasShaderFloatControls;
	}
};

// All the features and properties we need to keep around from extension initialization
struct FOptionalVulkanDeviceExtensionProperties
{
	FOptionalVulkanDeviceExtensionProperties()
	{
		FMemory::Memzero(*this);
	}

	VkPhysicalDeviceDriverPropertiesKHR PhysicalDeviceDriverProperties;

	VkPhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBufferProps;
	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT SubgroupSizeControlProperties;

	VkPhysicalDeviceAccelerationStructurePropertiesKHR AccelerationStructureProps;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProps;

	VkPhysicalDeviceFragmentShadingRateFeaturesKHR FragmentShadingRateFeatures;
	VkPhysicalDeviceFragmentDensityMapFeaturesEXT FragmentDensityMapFeatures;
	VkPhysicalDeviceFragmentDensityMap2FeaturesEXT FragmentDensityMap2Features;

	VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR FragmentShaderBarycentricProps;
	VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR ComputeShaderDerivativesFeatures;
	VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT GraphicsPipelineLibraryProperties;

	VkPhysicalDeviceMeshShaderPropertiesEXT MeshShaderProperties;
};

class FVulkanPhysicalDeviceFeatures
{
public:
	FVulkanPhysicalDeviceFeatures()
	{
		FMemory::Memzero(*this);
	}

	void Query(VkPhysicalDevice PhysicalDevice, uint32 APIVersion);

	VkPhysicalDeviceFeatures	     Core_1_0;
	VkPhysicalDeviceVulkan11Features Core_1_1;
private:
	// Anything above Core 1.1 cannot be assumed, they should only be used by the device at init time
	VkPhysicalDeviceVulkan12Features Core_1_2;
	VkPhysicalDeviceVulkan13Features Core_1_3;

	friend class FVulkanDevice;
	friend FVulkanDynamicRHI;
};


namespace VulkanRHI
{
	class FDeferredDeletionQueue2 : public VulkanRHI::FDeviceChild
	{

	public:
		FDeferredDeletionQueue2(FVulkanDevice* InDevice);
		~FDeferredDeletionQueue2();

		enum class EType
		{
			RenderPass,
			Buffer,
			BufferView,
			Image,
			ImageView,
			Pipeline,
			PipelineLayout,
			Framebuffer,
			DescriptorSetLayout,
			Sampler,
			Semaphore,
			ShaderModule,
			Event,
			ResourceAllocation,
			DeviceMemoryAllocation,
			BufferSuballocation,
			AccelerationStructure,
			BindlessHandle,
		};

		template <typename T>
		inline void EnqueueResource(EType Type, T Handle)
		{
			static_assert(sizeof(T) <= sizeof(uint64), "Vulkan resource handle type size too large.");
			EnqueueGenericResource(Type, (uint64)Handle);
		}

		inline void EnqueueBindlessHandle(FRHIDescriptorHandle DescriptorHandle)
		{
			if (DescriptorHandle.IsValid())
			{
				const uint64 Type = (uint64)DescriptorHandle.GetRawType();
				const uint64 Index = (uint64)DescriptorHandle.GetIndex();
				const uint64 AsUInt64 = (Type << 32) | Index;
				EnqueueResource(EType::BindlessHandle, AsUInt64);
			}
		}

		void EnqueueResourceAllocation(FVulkanAllocation& Allocation);
		void EnqueueDeviceAllocation(FDeviceMemoryAllocation* DeviceMemoryAllocation);

		void ReleaseResources(bool bDeleteImmediately = false);

		inline void Clear()
		{
			ReleaseResources(true);
		}

	private:
		void EnqueueGenericResource(EType Type, uint64 Handle);

		struct FEntry
		{
			EType StructureType;
			uint32 FrameNumber;
			uint64 Handle;
			FVulkanAllocation Allocation;
			FDeviceMemoryAllocation* DeviceMemoryAllocation;
		};

		void ReleaseResourcesImmediately(const TArray<FEntry>& InEntries);

		FCriticalSection CS;
		TArray<FEntry> Entries;
	};
}


class FVulkanDevice
{
public:
	FVulkanDevice(FVulkanDynamicRHI* InRHI, VkPhysicalDevice Gpu);

	~FVulkanDevice();

	void InitGPU();

	void CreateDevice(TArray<const ANSICHAR*>& DeviceLayers, FVulkanDeviceExtensionArray& UEExtensions);
	void ChooseVariableRateShadingMethod();

	void PrepareForDestroy();
	void Destroy();

	void WaitUntilIdle();

	EGpuVendorId GetVendorId() const
	{
		return VendorId;
	}

	bool HasAsyncComputeQueue() const
	{
		return Queues[(int32)EVulkanQueueType::AsyncCompute] != nullptr;
	}

	bool HasTransferQueue() const
	{
		return Queues[(int32)EVulkanQueueType::Transfer] != nullptr;
	}

	bool HasMultipleQueues() const
	{
		return HasAsyncComputeQueue() || HasTransferQueue();
	}

	bool CanPresentOnComputeQueue() const
	{
		return bPresentOnComputeQueue;
	}

	FVulkanQueue* GetQueue(ERHIPipeline Pipeline)
	{
		if (Pipeline == ERHIPipeline::Graphics) 
		{
			return GetQueue(EVulkanQueueType::Graphics); 
		}
		else if (Pipeline == ERHIPipeline::AsyncCompute) 
		{ 
			return GetQueue(EVulkanQueueType::AsyncCompute); 
		}
		checkNoEntry();
		return nullptr;
	}

	FVulkanQueue* GetQueue(EVulkanQueueType QueueType)
	{
		return Queues[(int32)QueueType];
	}

	FVulkanQueue* GetGraphicsQueue()
	{
		return GetQueue(EVulkanQueueType::Graphics);
	}

	FVulkanQueue* GetComputeQueue()
	{
		return GetQueue(EVulkanQueueType::AsyncCompute);
	}

	FVulkanQueue* GetTransferQueue()
	{
		return GetQueue(EVulkanQueueType::Transfer);
	}

	FVulkanQueue* GetPresentQueue()
	{
		return PresentQueue;
	}

	void ForEachQueue(TFunctionRef<void(FVulkanQueue&)> Callback);

	VkPhysicalDevice GetPhysicalHandle() const
	{
		return Gpu;
	}

	const VkPhysicalDeviceProperties& GetDeviceProperties() const
	{
		return GpuProps;
	}

	VkExtent2D GetBestMatchedFragmentSize(EVRSShadingRate Rate) const
	{
		return FragmentSizeMap[Rate];
	}

	const VkPhysicalDeviceLimits& GetLimits() const
	{
		return GpuProps.limits;
	}

	const VkPhysicalDeviceIDPropertiesKHR& GetDeviceIdProperties() const
	{
		return GpuIdProps;
	}

	const VkPhysicalDeviceSubgroupProperties& GetDeviceSubgroupProperties() const
	{
		return GpuSubgroupProps;
	}

	FVulkanRayTracingCompactionRequestHandler* GetRayTracingCompactionRequestHandler()
	{
		return RayTracingCompactionRequestHandler; 
	}

	FVulkanRayTracingPipelineLibraryCache* GetRayTracingPipelineLibraryCache()
	{
		return RayTracingPipelineLibraryCache;
	}

	void InitializeRayTracing();
	void CleanUpRayTracing();

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	VkValidationCacheEXT GetValidationCache() const
	{
		return ValidationCache;
	}
#endif

	const FVulkanPhysicalDeviceFeatures& GetPhysicalDeviceFeatures() const
	{
		return PhysicalDeviceFeatures;
	}

	bool HasUnifiedMemory() const
	{
		return DeviceMemoryManager.HasUnifiedMemory();
	}

	bool SupportsBindless() const;

	const VkComponentMapping& GetFormatComponentMapping(EPixelFormat UEFormat) const;

	VkDevice GetInstanceHandle() const
	{
		return Device;
	}

	const FVulkanSamplerState& GetDefaultSampler() const
	{
		return GetGlobalSamplers(FVulkanShaderHeader::EGlobalSamplerType::PointWrappedSampler);
	}

	const VkFormatProperties& GetFormatProperties(VkFormat InFormat) const;

	VulkanRHI::FDeviceMemoryManager& GetDeviceMemoryManager()
	{
		return DeviceMemoryManager;
	}

	const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const
	{
		return DeviceMemoryManager.GetMemoryProperties();
	}

	VulkanRHI::FMemoryManager& GetMemoryManager()
	{
		return MemoryManager;
	}

	VulkanRHI::FDeferredDeletionQueue2& GetDeferredDeletionQueue()
	{
		return DeferredDeletionQueue;
	}

	VulkanRHI::FStagingManager& GetStagingManager()
	{
		return StagingManager;
	}

	VulkanRHI::FFenceManager& GetFenceManager()
	{
		return FenceManager;
	}

	VulkanRHI::FTempBlockAllocator& GetTempBlockAllocator()
	{
		return *TempBlockAllocator;
	}

	FVulkanRenderPassManager& GetRenderPassManager()
	{
		return *RenderPassManager;
	}

	FVulkanDescriptorSetCache& GetDescriptorSetCache()
	{
		return *DescriptorSetCache;
	}

	FVulkanDescriptorPoolsManager& GetDescriptorPoolsManager()
	{
		return *DescriptorPoolsManager;
	}

	FVulkanBindlessDescriptorManager* GetBindlessDescriptorManager()
	{
		return BindlessDescriptorManager;
	}

	TMap<uint32, FSamplerStateRHIRef>& GetSamplerMap()
	{
		return SamplerMap;
	}

	FVulkanShaderFactory& GetShaderFactory()
	{
		return ShaderFactory;
	}

	FVulkanCommandListContextImmediate& GetImmediateContext()
	{
		return *ImmediateContext;
	}

	void NotifyDeletedImage(VkImage Image, bool bRenderTarget);

#if VULKAN_ENABLE_DRAW_MARKERS
	PFN_vkCmdBeginDebugUtilsLabelEXT GetCmdBeginDebugLabel() const
	{
		return DebugMarkers.CmdBeginDebugLabel;
	}

	PFN_vkCmdEndDebugUtilsLabelEXT GetCmdEndDebugLabel() const
	{
		return DebugMarkers.CmdEndDebugLabel;
	}

	PFN_vkSetDebugUtilsObjectNameEXT GetSetDebugName() const
	{
		return DebugMarkers.SetDebugName;
	}
#endif

	FVulkanQueryPool* AcquireOcclusionQueryPool(uint32 NumQueries);
	FVulkanQueryPool* AcquireTimingQueryPool();
	void ReleaseQueryPool(FVulkanQueryPool* Pool);
	void RemoveStaleQueryPools();

	class FVulkanPipelineStateCacheManager* GetPipelineStateCache()
	{
		return PipelineStateCache;
	}

	void NotifyDeletedGfxPipeline(class FVulkanRHIGraphicsPipelineState* Pipeline);
	void NotifyDeletedComputePipeline(class FVulkanComputePipeline* Pipeline);

	void VulkanSetObjectName(VkObjectType Type, uint64_t Handle, const TCHAR* Name);
	const FOptionalVulkanDeviceExtensions& GetOptionalExtensions() const
	{
		return OptionalDeviceExtensions;
	}

	const FOptionalVulkanDeviceExtensionProperties& GetOptionalExtensionProperties() const
	{
		return OptionalDeviceExtensionProperties;
	}

	inline bool NeedsAllPlanes() const
	{
		return !SupportsParallelRendering();
	}

	inline bool SupportsParallelRendering() const
	{
		return OptionalDeviceExtensions.HasSeparateDepthStencilLayouts && OptionalDeviceExtensions.HasKHRSynchronization2 && OptionalDeviceExtensions.HasKHRRenderPass2;
	}

	void SetupPresentQueue(VkSurfaceKHR Surface);

	const TArray<VkQueueFamilyProperties>& GetQueueFamilyProps()
	{
		return QueueFamilyProps;
	}

	FVulkanTransientHeapCache& GetOrCreateTransientHeapCache();

	const TArray<const ANSICHAR*>& GetDeviceExtensions() { return DeviceExtensions; }

#if RHI_NEW_GPU_PROFILER
	void GetCalibrationTimestamp(FVulkanTiming& InOutTiming);
#else
	// Performs a GPU and CPU timestamp at nearly the same time.
	// This allows aligning GPU and CPU events on the same timeline in profile visualization.
	FGPUTimingCalibrationTimestamp GetCalibrationTimestamp();
#endif

	const FVulkanSamplerState& GetGlobalSamplers(FVulkanShaderHeader::EGlobalSamplerType Type) const
	{
		return *GlobalSamplers[(uint32)Type];
	}

	VkEvent GetBarrierEvent();
	void ReleaseBarrierEvent(VkEvent Handle);

	VkBuffer CreateBuffer(VkDeviceSize BufferSize, VkBufferUsageFlags BufferUsageFlags, VkBufferCreateFlags BufferCreateFlags=0) const;

	const TArray<uint32>& GetActiveQueueFamilies() const
	{
		return ActiveQueueFamilies;
	}

private:
	void MapBufferFormatSupport(FPixelFormatInfo& PixelFormatInfo, EPixelFormat UEFormat, VkFormat VulkanFormat);
	void MapImageFormatSupport(FPixelFormatInfo& PixelFormatInfo, const TArrayView<const VkFormat>& PrioritizedFormats, EPixelFormatCapabilities RequiredCapabilities);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, EPixelFormatCapabilities RequiredCapabilities, int32 BlockBytes);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, int32 BlockBytes);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, EPixelFormatCapabilities RequiredCapabilities);

	void InitGlobalSamplers();
	TStaticArray<FVulkanSamplerState*, (uint32)FVulkanShaderHeader::EGlobalSamplerType::Count> GlobalSamplers;

	VkDevice Device;

	VulkanRHI::FDeviceMemoryManager DeviceMemoryManager;

	VulkanRHI::FMemoryManager MemoryManager;

	VulkanRHI::FDeferredDeletionQueue2 DeferredDeletionQueue;

	VulkanRHI::FStagingManager StagingManager;

	VulkanRHI::FFenceManager FenceManager;

	VulkanRHI::FTempBlockAllocator* TempBlockAllocator = nullptr;

	FVulkanRenderPassManager* RenderPassManager;

	FVulkanTransientHeapCache* TransientHeapCache = nullptr;

	// Active on ES3.1
	FVulkanDescriptorSetCache* DescriptorSetCache = nullptr;
	// Active on >= SM4
	FVulkanDescriptorPoolsManager* DescriptorPoolsManager = nullptr;

	FVulkanBindlessDescriptorManager* BindlessDescriptorManager = nullptr;

	FVulkanShaderFactory ShaderFactory;

	VkPhysicalDevice Gpu;
	VkPhysicalDeviceProperties GpuProps;

	TArray<VkPhysicalDeviceFragmentShadingRateKHR> FragmentShadingRates;
	TStaticArray<VkExtent2D, (EVRSShadingRate::VRSSR_Last+1)> FragmentSizeMap;

	// Extension specific properties
	VkPhysicalDeviceIDPropertiesKHR GpuIdProps;
	VkPhysicalDeviceSubgroupProperties GpuSubgroupProps;

	FVulkanRayTracingCompactionRequestHandler* RayTracingCompactionRequestHandler = nullptr;
	FVulkanRayTracingPipelineLibraryCache* RayTracingPipelineLibraryCache = nullptr;

	FVulkanPhysicalDeviceFeatures PhysicalDeviceFeatures;

	TArray<VkQueueFamilyProperties> QueueFamilyProps;
	VkFormatProperties FormatProperties[VK_FORMAT_RANGE_SIZE];
	// Info for formats that are not in the core Vulkan spec (i.e. extensions)
	mutable TMap<VkFormat, VkFormatProperties> ExtensionFormatProperties;

	// Reusable query pools
	FCriticalSection QueryPoolLock;
	uint32 OcclusionQueryPoolSize = 256;
	TStaticArray<TArray<FVulkanQueryPool*>, (int32)EVulkanQueryPoolType::Count> FreeQueryPools;

	// Reusable gpu-only barrier events
	FCriticalSection BarrierEventLock;
	TArray<VkEvent> BarrierEvents;

	TStaticArray<FVulkanQueue*, (int32)EVulkanQueueType::Count> Queues;
	FVulkanQueue* PresentQueue = nullptr;  // points to an existing queue
	bool bPresentOnComputeQueue = false;
	TArray<uint32> ActiveQueueFamilies;

	EGpuVendorId VendorId = EGpuVendorId::NotQueried;

	VkComponentMapping PixelFormatComponentMapping[PF_MAX];

	TMap<uint32, FSamplerStateRHIRef> SamplerMap;

	FVulkanCommandListContextImmediate* ImmediateContext;

	FVulkanDynamicRHI* RHI = nullptr;
	
	TArray<const ANSICHAR*> SetupDeviceLayers(FVulkanDeviceExtensionArray& UEExtensions);

	FOptionalVulkanDeviceExtensions	OptionalDeviceExtensions;
	FOptionalVulkanDeviceExtensionProperties OptionalDeviceExtensionProperties;
	TArray<const ANSICHAR*>			DeviceExtensions;

	void SetupFormats();

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	VkValidationCacheEXT ValidationCache = VK_NULL_HANDLE;
#endif

#if VULKAN_ENABLE_DRAW_MARKERS
	bool bUseLegacyDebugMarkerExt = false;
	struct
	{
		PFN_vkSetDebugUtilsObjectNameEXT	SetDebugName = nullptr;
		PFN_vkCmdBeginDebugUtilsLabelEXT	CmdBeginDebugLabel = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT		CmdEndDebugLabel = nullptr;
	} DebugMarkers;
	friend class FVulkanCommandListContext;
#endif
	void SetupDrawMarkers();

	class FVulkanPipelineStateCacheManager* PipelineStateCache;
	friend class FVulkanDynamicRHI;
	friend class FVulkanRHIGraphicsPipelineState;
};

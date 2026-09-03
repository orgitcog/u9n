// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RayTracingGeometryManagerInterface.h"

#include "Containers/SparseArray.h"
#include "Containers/Map.h"
#include "IO/IoBuffer.h"
#include "Serialization/BulkData.h"

#if RHI_RAYTRACING

class FPrimitiveSceneProxy;
class UStaticMesh;
class FRayTracingStreamableAsset;

class FRayTracingGeometryManager : public IRayTracingGeometryManager
{
public:

	ENGINE_API FRayTracingGeometryManager();
	ENGINE_API virtual ~FRayTracingGeometryManager();

	ENGINE_API virtual FBuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode) override;

	ENGINE_API virtual void RemoveBuildRequest(FBuildRequestIndex InRequestIndex) override;
	ENGINE_API virtual void BoostPriority(FBuildRequestIndex InRequestIndex, float InBoostValue) override;
	ENGINE_API virtual void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries) override;
	ENGINE_API virtual void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false) override;

	ENGINE_API virtual FGeometryHandle RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry) override;
	ENGINE_API virtual void ReleaseRayTracingGeometryHandle(FGeometryHandle Handle) override;

	ENGINE_API virtual RayTracing::FGeometryGroupHandle RegisterRayTracingGeometryGroup(uint32 NumLODs, uint32 CurrentFirstLODIdx = 0) override;
	ENGINE_API virtual void ReleaseRayTracingGeometryGroup(RayTracing::FGeometryGroupHandle Handle) override;

	ENGINE_API virtual void RefreshRegisteredGeometry(FGeometryHandle Handle) override;

	void SetRayTracingGeometryStreamingData(const FRayTracingGeometry* Geometry, FRayTracingStreamableAsset& StreamableAsset);
	void SetRayTracingGeometryGroupCurrentFirstLODIndex(FRHICommandListBase& RHICmdList, RayTracing::FGeometryGroupHandle Handle, uint8 CurrentFirstLODIdx);

	ENGINE_API virtual void PreRender() override;
	ENGINE_API virtual void Tick(FRHICommandList& RHICmdList) override;

	ENGINE_API void RegisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::FGeometryGroupHandle InRayTracingGeometryGroupHandle);
	ENGINE_API void UnregisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::FGeometryGroupHandle InRayTracingGeometryGroupHandle);

	ENGINE_API virtual void RequestUpdateCachedRenderState(RayTracing::FGeometryGroupHandle InRayTracingGeometryGroupHandle) override;

	ENGINE_API void AddReferencedGeometry(const FRayTracingGeometry* Geometry);
	ENGINE_API void AddReferencedGeometryGroups(const TSet<RayTracing::FGeometryGroupHandle>& GeometryGroups);
	ENGINE_API void AddReferencedGeometryGroupsForDynamicUpdate(const TSet<RayTracing::FGeometryGroupHandle>& GeometryGroups);

#if DO_CHECK
	ENGINE_API bool IsGeometryReferenced(const FRayTracingGeometry* Geometry) const;
	ENGINE_API bool IsGeometryGroupReferenced(RayTracing::FGeometryGroupHandle GeometryGroup) const;
#endif

	ENGINE_API virtual bool IsGeometryVisible(FGeometryHandle GeometryHandle) const override;
	ENGINE_API virtual void AddVisibleGeometry(FGeometryHandle GeometryHandle) override;
	ENGINE_API void ResetVisibleGeometries(); // TODO: Temp - is it needed?

private:

	struct FBuildRequest
	{
		FRayTracingGeometry* Owner = nullptr;

		FBuildRequestIndex RequestIndex = INDEX_NONE;

		float BuildPriority = 0.0f;
		EAccelerationStructureBuildMode BuildMode;
		bool bReleaseBuffersAfterBuild = false;

		// TODO: Implement use-after-free checks in FBuildRequestIndex using some bits to identify generation
	};

	void SetupBuildParams(const FBuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams, TArray<FRayTracingGeometry*>& InReleaseBuffers, bool bRemoveFromRequestArray = true);

	void ReleaseRayTracingGeometryGroupReference(RayTracing::FGeometryGroupHandle Handle);

	bool RequestRayTracingGeometryStreamIn(FRHICommandList& RHICmdList, FGeometryHandle GeometryHandle);
	void ProcessCompletedStreamingRequests(FRHICommandList& RHICmdList);

	FCriticalSection RequestCS;

	TSparseArray<FBuildRequest> GeometryBuildRequests;

	// Working array with all active build build params in the RHI
	TArray<FBuildRequest> SortedRequests;
	TArray<FRayTracingGeometryBuildParams> BuildParams;

	// Operations such as registering geometry/groups can be done from different render command pipes (eg: SkeletalMesh)
	// so need to use critical section in relevant functions
	FCriticalSection MainCS;

	struct FRayTracingGeometryGroup
	{
		TArray<FGeometryHandle> GeometryHandles;

		TSet<FPrimitiveSceneProxy*> ProxiesWithCachedRayTracingState;

		uint8 CurrentFirstLODIdx = INDEX_NONE;

		// Due to the way we batch release FRenderResource and SceneProxies, 
		// ReleaseRayTracingGeometryGroup(...) can end up being called before all FRayTracingGeometry and SceneProxies are actually released.
		// To deal with this, we keep track of whether the group is still referenced and only release the group handle once all references are released.
		uint32 NumReferences = 0;

		// TODO: Implement use-after-free checks in RayTracing::FGeometryGroupHandle using some bits to identify generation
	};

	struct FRegisteredGeometry
	{
		FRayTracingGeometry* Geometry = nullptr;
		uint64 LastReferencedFrame = 0;
		uint32 Size = 0;

		FRayTracingStreamableAsset* StreamableAsset = nullptr;
		uint32 StreamableBVHSize = 0;
		uint32 StreamableBuffersSize = 0;

		int16 StreamingRequestIndex = INDEX_NONE;

		enum class FStatus : uint8
		{
			StreamedOut,
			Streaming,
			StreamedIn,
		};

		FStatus Status = FStatus::StreamedOut;
		bool bEvicted : 1 = false;
		bool bAlwaysResident : 1 = false;
	};

	struct FStreamingRequest
	{
		FIoBuffer RequestBuffer;
		FBulkDataBatchRequest Request;

		FGeometryHandle GeometryHandle = INDEX_NONE;
		uint32 GeometrySize = 0;

		bool bBuffersOnly = false;

		bool bCancelled = false;

		bool IsValid() const { return (GeometryHandle != INDEX_NONE) || bCancelled; }

		void Cancel()
		{
			GeometryHandle = INDEX_NONE;
			bCancelled = true;

			if (Request.IsPending())
			{
				Request.Cancel();
			}
		}

		void Reset()
		{
			GeometryHandle = INDEX_NONE;
			bCancelled = false;

			checkf(!Request.IsPending(), TEXT("Can't cancel ray tracing geometry streaming request that is still in-flight."));
			Request.Reset();
			RequestBuffer = {};
		}
	};

	void CancelStreamingRequest(FRegisteredGeometry& RegisteredGeometry);

	// Helper function to stream out geometry
	// Must call this instead of FRayTracingGeometry::ReleaseRHIForStreaming() directly
	void StreamOutGeometry(FRHIResourceReplaceBatcher& Batcher, FRegisteredGeometry& RegisteredGeometry);

	// Helper function to make geometry resident + necessary management
	// Must call this instead of FRayTracingGeometry::MakeResident() directly
	void MakeGeometryResident(FRHICommandList& RHICmdList, FRegisteredGeometry& RegisteredGeometry);

	// Helper function to evict geometry + necessary streaming management
	// Must call this instead of FRayTracingGeometry::Evict() directly
	void EvictGeometry(FRHICommandListBase& RHICmdList, FRegisteredGeometry& RegisteredGeometry);

	static bool IsAlwaysResidentGeometry(const FRayTracingGeometry* InGeometry, const FRayTracingGeometryGroup& Group);

	TSparseArray<FRayTracingGeometryGroup> RegisteredGroups;

	// Used for keeping track of geometries when ray tracing is dynamic
	TSparseArray<FRegisteredGeometry> RegisteredGeometries;

	TSet<FGeometryHandle> ResidentGeometries;
	uint64 TotalResidentSize = 0;

	TSet<FGeometryHandle> AlwaysResidentGeometries;
	uint64 TotalAlwaysResidentSize = 0;

	TSet<FGeometryHandle> EvictableGeometries;

	TSet<FGeometryHandle> ReferencedGeometryHandles;
	TSet<RayTracing::FGeometryGroupHandle> ReferencedGeometryGroups;

	TSet<RayTracing::FGeometryGroupHandle> ReferencedGeometryGroupsForDynamicUpdate;

	TSet<FGeometryHandle> PendingStreamingRequests;

	TArray<FStreamingRequest> StreamingRequests;
	int32 NumStreamingRequests = 0;
	int32 NextStreamingRequestIndex = 0;

	// Total size of the BLASes currently being streamed.
	// Used to keep track of how much TotalResidentSize will increase when the requests complete.
	uint64 TotalStreamingSize = 0;

	// Feedback
	TSet<int32> VisibleGeometryHandles;

	bool bRenderedFrame = false;
};

#endif // RHI_RAYTRACING

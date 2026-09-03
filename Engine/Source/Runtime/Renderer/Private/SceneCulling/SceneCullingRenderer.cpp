// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneCullingRenderer.h"
#include "SceneCulling.h"
#include "SceneCulling.inl"
#include "RenderGraphUtils.h"
#include "SceneRendererInterface.h"
#include "SystemTextures.h"
#include "SceneRendering.h"
#include "ShaderPrintParameters.h"

static TAutoConsoleVariable<int32> CVarSceneCullingDebugRenderMode(
	TEXT("r.SceneCulling.DebugRenderMode"), 
	0, 
	TEXT("SceneCulling debug render mode.\n")
	TEXT(" 0 = Disabled (default)\n")
	TEXT(" 1 = Enabled"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CvarSceneCullingCullChunkViewDistance(
	TEXT("r.SceneCulling.CullChunkViewDistance"), 
	false, 
	TEXT("Set to true (default is false) to enable per-chunk view distance culling.\n")
	TEXT("  For some scenes it may increase overhead due to poor load balancing in the cell culling. This may be mitigated by lowering r.SceneCulling.MinCellSize"),
	ECVF_RenderThreadSafe);

FInstanceHierarchyParameters& FSceneCullingRenderer::GetShaderParameters(FRDGBuilder& GraphBuilder) 
{ 
	// Sync any update that is in progress.
	SceneCulling.EndUpdate(GraphBuilder, GetSceneRenderer().GetSceneUniforms(), true);

	// This should not need to be done more than once per frame
	if (CellHeadersRDG == nullptr)
	{
		CellBlockDataRDG = SceneCulling.CellBlockDataBuffer.Register(GraphBuilder);
		CellHeadersRDG = SceneCulling.CellHeadersBuffer.Register(GraphBuilder);
		ItemChunksRDG = SceneCulling.ItemChunksBuffer.Register(GraphBuilder);
		InstanceIdsRDG = SceneCulling.InstanceIdsBuffer.Register(GraphBuilder);
		UsedChunkIdMaskRDG = GraphBuilder.RegisterExternalBuffer(SceneCulling.UsedChunkIdMaskBuffer);
		ExplicitChunkBoundsRDG = SceneCulling.ExplicitChunkBoundsBuffer.Register(GraphBuilder);
		ExplicitChunkCellIdsRDG = SceneCulling.ExplicitChunkCellIdsBuffer.Register(GraphBuilder);

#if 0
		// Fully upload the buffers for debugging. 
		CellBlockDataRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.BlockData"), SceneCulling.CellBlockData);
		CellHeadersRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.CellHeaders"), SceneCulling.CellHeaders);
		ItemChunksRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.ItemChunks"), SceneCulling.PackedCellChunkData);
		InstanceIdsRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.Items"), SceneCulling.PackedCellData);
#endif

		ShaderParameters.NumCellsPerBlockLog2 = FSpatialHash::NumCellsPerBlockLog2;
		ShaderParameters.CellBlockDimLog2 = FSpatialHash::CellBlockDimLog2;
		ShaderParameters.LocalCellCoordMask = (1U << FSpatialHash::CellBlockDimLog2) - 1U;
		ShaderParameters.FirstLevel = SceneCulling.SpatialHash.GetFirstLevel();
		ShaderParameters.bCullChunkViewDistance = CvarSceneCullingCullChunkViewDistance.GetValueOnRenderThread();
		ShaderParameters.InstanceHierarchyCellBlockData = GraphBuilder.CreateSRV(CellBlockDataRDG);
		ShaderParameters.InstanceHierarchyCellHeaders = GraphBuilder.CreateSRV(CellHeadersRDG);
		ShaderParameters.InstanceIds = GraphBuilder.CreateSRV(InstanceIdsRDG);
		ShaderParameters.UsedChunkIdMask = GraphBuilder.CreateSRV(UsedChunkIdMaskRDG);
		ShaderParameters.InstanceHierarchyItemChunks = GraphBuilder.CreateSRV(ItemChunksRDG);
		ShaderParameters.ExplicitChunkBounds = GraphBuilder.CreateSRV(ExplicitChunkBoundsRDG);
		ShaderParameters.ExplicitChunkCellIds = GraphBuilder.CreateSRV(ExplicitChunkCellIdsRDG);
	}

	return ShaderParameters; 
}

FSceneInstanceCullingQuery* FSceneCullingRenderer::CullInstances(FRDGBuilder& GraphBuilder, const TConstArrayView<FConvexVolume>& ViewCullVolumes)
{
	SCOPED_NAMED_EVENT(FSceneCullingRenderer_CullInstances, FColor::Emerald);

	if (SceneCulling.IsEnabled())
	{
		FSceneInstanceCullingQuery* Query = GraphBuilder.AllocObject<FSceneInstanceCullingQuery>(*this);

		for (int32 Index = 0; Index < ViewCullVolumes.Num(); ++Index)
		{
			FCullingVolume CullingVolume;
			// Assume world-space
			CullingVolume.WorldToVolumeTranslation = FVector3d::ZeroVector;
			CullingVolume.ConvexVolume = ViewCullVolumes[Index];
			Query->Add(Index, 1, 1, CullingVolume);
		}

		Query->Dispatch(GraphBuilder);

		return Query;
	}
	return nullptr;
}


class FSceneCullingDebugRender_CS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSceneCullingDebugRender_CS);
	SHADER_USE_PARAMETER_STRUCT(FSceneCullingDebugRender_CS, FGlobalShader);

	static constexpr int32 NumThreadsPerGroup = 64;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE( ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer )
		SHADER_PARAMETER_STRUCT_INCLUDE( FInstanceHierarchyParameters, InstanceHierarchyParameters )
		SHADER_PARAMETER(FVector3f, PickingRayStart)
		SHADER_PARAMETER(FVector3f, PickingRayEnd)
		SHADER_PARAMETER(int32, DebugMode)
		SHADER_PARAMETER(int32, MaxCells)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWDrawCellInfoCounter)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, ValidCellsMask)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FSceneCullingDebugRender_CS, "/Engine/Private/SceneCulling/SceneCullingDebugRender.usf", "DebugRender", SF_Compute);


void FSceneCullingRenderer::DebugRender(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views)
{
#if !UE_BUILD_SHIPPING
	int32 MaxCellCount = SceneCulling.CellHeaders.Num();

	int32 DebugMode = CVarSceneCullingDebugRenderMode.GetValueOnRenderThread();
	if (DebugMode != 0 && MaxCellCount > 0)
	{
		// Force ShaderPrint on.
		ShaderPrint::SetEnabled(true); 

		// This lags by one frame, so may miss some in one frame, also overallocates since we will cull a lot.
		ShaderPrint::RequestSpaceForLines(MaxCellCount * 12 * Views.Num());

		// Note: we have to construct this as the GPU currently does not have a mapping of what cells are valid.
		//       Normally this comes from the CPU during the broad phase culling. Thus it is only needed here for debug purposes.
		TBitArray<> ValidCellsMask(false, MaxCellCount);
		for (int32 Index = 0; Index < MaxCellCount; ++Index)
		{
			ValidCellsMask[Index] =  IsValidCell(SceneCulling.CellHeaders[Index]); 
		}
		FRDGBuffer* ValidCellsMaskRdg = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.Debug.ValidCellsMaskRdg"), TConstArrayView<uint32>(ValidCellsMask.GetData(), FBitSet::CalculateNumWords(ValidCellsMask.Num())));

		for (auto &View : Views)
		{
			if (ShaderPrint::IsEnabled(View.ShaderPrintData))
			{	
				FRDGBufferRef DrawCellInfoCounterRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(4, 1), TEXT("SceneCulling.Debug.DrawCellInfoCounter"));
				FRDGBufferUAVRef DrawCellInfoCounterUAV = GraphBuilder.CreateUAV(DrawCellInfoCounterRDG);
				AddClearUAVPass(GraphBuilder, DrawCellInfoCounterUAV, 0u);

				FSceneCullingDebugRender_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSceneCullingDebugRender_CS::FParameters>();
				ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrintUniformBuffer);
				PassParameters->InstanceHierarchyParameters = GetShaderParameters(GraphBuilder);
				PassParameters->MaxCells = MaxCellCount;
				PassParameters->ValidCellsMask = GraphBuilder.CreateSRV(ValidCellsMaskRdg);
				PassParameters->DebugMode = DebugMode;
				FVector PickingRayStart(ForceInit);
				FVector PickingRayDir(ForceInit);
				FIntPoint CursorPos = View.CursorPos;
				if (CursorPos.GetMin() < 0)
				{
					CursorPos = View.ViewRect.Size() / 2;
				}
				View.DeprojectFVector2D(CursorPos, PickingRayStart, PickingRayDir);
				PassParameters->PickingRayStart = FVector3f(PickingRayStart);
				PassParameters->PickingRayEnd = FVector3f(PickingRayStart + PickingRayDir * WORLD_MAX);
				PassParameters->RWDrawCellInfoCounter = DrawCellInfoCounterUAV;

				auto ComputeShader = View.ShaderMap->GetShader<FSceneCullingDebugRender_CS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SceneCullingDebugRender"),
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCountWrapped(MaxCellCount, FSceneCullingDebugRender_CS::NumThreadsPerGroup)
				);
			}
		}
	}
#endif
}

FSceneInstanceCullingQuery* FSceneCullingRenderer::CreateInstanceQuery(FRDGBuilder& GraphBuilder)
{
	SCOPED_NAMED_EVENT(FSceneCullingRenderer_CullInstances, FColor::Emerald);

	if (SceneCulling.IsEnabled())
	{
		FSceneInstanceCullingQuery* Query = GraphBuilder.AllocObject<FSceneInstanceCullingQuery>(*this);
		return Query;
	}
	return nullptr;
}

int32 FSceneInstanceCullingQuery::Add(uint32 FirstPrimaryView, uint32 NumPrimaryViews, uint32 MaxNumViews, const FCullingVolume& CullingVolume)
{
	check(!CullingResult);
	check(!AsyncTaskHandle.IsValid());

	if (SceneCullingRenderer.IsEnabled())
	{
		FCullingJob Job;
		Job.MaxNumViews = MaxNumViews;
		Job.ViewDrawGroup.FirstView = FirstPrimaryView;
		Job.ViewDrawGroup.NumViews = NumPrimaryViews;
		Job.CullingVolume = CullingVolume;
		CullingJobs.Add(Job);

		ViewDrawGroups.Add(Job.ViewDrawGroup);

		// return index of the job / view group
		return CullingJobs.Num() - 1;
	}
	return INDEX_NONE;
}

void FSceneInstanceCullingQuery::Dispatch(FRDGBuilder& GraphBuilder, bool bInAllowAsync)
{
	check(!CullingResult);
	check(!AsyncTaskHandle.IsValid());

	const bool bAllowAsync = SceneCullingRenderer.SceneCulling.bUseAsyncUpdate && bInAllowAsync;

	if (!CullingJobs.IsEmpty())
	{
		// Must wait if this query is not running async or we might race against the update task.
		UE::Tasks::FTask UpdateTaskHandle = SceneCullingRenderer.SceneCulling.GetUpdateTaskHandle();
		if (!bAllowAsync && UpdateTaskHandle.IsValid())
		{
			UpdateTaskHandle.Wait();
		}

		CullingResult = GraphBuilder.AllocObject<FSceneInstanceCullResult>();

		AsyncTaskHandle = GraphBuilder.AddSetupTask([this]()
		{
			ComputeResult();
		},
		nullptr, TArray<UE::Tasks::FTask>{ UpdateTaskHandle }, UE::Tasks::ETaskPriority::High, bAllowAsync);
	}
}

FSceneInstanceCullResult* FSceneInstanceCullingQuery::GetResult()
{
	SCOPED_NAMED_EVENT(FSceneInstanceCullingQuery_GetResult, FColor::Emerald);

	if (AsyncTaskHandle.IsValid())
	{
		AsyncTaskHandle.Wait();
	}

	return CullingResult;
}

void FSceneInstanceCullingQuery::ComputeResult()
{
	SCOPED_NAMED_EVENT(FSceneInstanceCullingQuery_ComputeResult, FColor::Emerald);

	const FSceneCulling& SceneCulling = SceneCullingRenderer.SceneCulling;
	// loop and append all results
	for (uint32 ViewGroupId = 0; ViewGroupId < uint32(CullingJobs.Num()); ++ViewGroupId)
	{
		const FCullingJob& CullingJob = CullingJobs[ViewGroupId];
		if (SceneCulling.IsSmallCullingVolume(CullingJob.CullingVolume))
		{
			struct FResultConsumer
			{
				FSceneInstanceCullResult* CullingResult;
				uint32 ViewGroupId;
				const TArray<FPackedCellHeader>& CellHeaders;

				void OnCellOverlap(uint32 CellId)
				{
					FCellHeader CellHeader = UnpackCellHeader(CellHeaders[CellId]);
					if (IsValidCell(CellHeader))
					{
						CullingResult->CellChunkDraws.Add(FCellChunkDraw{ CellHeader.ItemChunksOffset, ViewGroupId }, CellHeader.NumItemChunks);
					}
				}
			};

			FResultConsumer ResultConsumer { CullingResult, ViewGroupId, SceneCulling.CellHeaders };

			SceneCulling.TestSphere(CullingJob.CullingVolume.Sphere, ResultConsumer);
		}
		else 
		{
			// broad phase test, go wide over chunks will dispatch one thread per view group ID
			CullingResult->ChunkCullViewGroupIds.Add(ViewGroupId);
		}

	}

	CullingResult->CellChunkDraws.FinalizeBatches();
	CullingResult->NumAllocatedChunks = SceneCulling.CellChunkIdAllocator.GetMaxSize();
	// All chunks may possibly be occluded in the first pass (except the uncullable ones).
	CullingResult->MaxOccludedChunkDraws = SceneCulling.CellChunkIdAllocator.GetSparselyAllocatedSize() * CullingResult->ChunkCullViewGroupIds.Num()
		+ CullingResult->CellChunkDraws.GetTotalChildren();
	CullingResult->UncullableNumItemChunks = SceneCullingRenderer.SceneCulling.UncullableNumItemChunks;
	// All chunks (plus the uncullable, once per group) may potentially get through culling in the first pass.
	CullingResult->NumInstanceGroups += CullingResult->MaxOccludedChunkDraws 
		+ CullingResult->UncullableNumItemChunks * CullingJobs.Num();
	CullingResult->UncullableItemChunksOffset = SceneCullingRenderer.SceneCulling.UncullableItemChunksOffset;
	CullingResult->SceneCullingRenderer = &SceneCullingRenderer;
}

FSceneInstanceCullingQuery::FSceneInstanceCullingQuery(FSceneCullingRenderer& InSceneCullingRenderer)
	: SceneCullingRenderer(InSceneCullingRenderer)
{
}


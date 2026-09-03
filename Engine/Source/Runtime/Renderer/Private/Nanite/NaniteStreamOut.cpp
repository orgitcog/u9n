// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteStreamOut.h"

#include "Rendering/NaniteStreamingManager.h"

#include "NaniteShared.h"

#include "PrimitiveSceneInfo.h"
#include "SceneInterface.h"

#include "ShaderParameterStruct.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"

#include "ScenePrivate.h"

DECLARE_GPU_STAT(NaniteStreamOutData);

static bool GNaniteStreamOutCacheTraversalData = true;
static FAutoConsoleVariableRef CVarNaniteStreamOutCacheTraversalData(
	TEXT("r.Nanite.StreamOut.CacheTraversalData"),
	GNaniteStreamOutCacheTraversalData,
	TEXT("Cache traversal data during count pass to be able to skip traversal during stream out pass."),
	ECVF_RenderThreadSafe
);

static const uint32 CandidateClusterSizeInUints = 3;

namespace Nanite
{
	BEGIN_SHADER_PARAMETER_STRUCT(FQueueParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FQueuePassState>, QueueState)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, Nodes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CandidateClusters)
		SHADER_PARAMETER(uint32, MaxNodes)
		SHADER_PARAMETER(uint32, MaxCandidateClusters)
	END_SHADER_PARAMETER_STRUCT()

	class FInitQueueCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FInitQueueCS);
		SHADER_USE_PARAMETER_STRUCT(FInitQueueCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FQueueParameters, QueueParameters)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, VertexAndIndexAllocator)
			SHADER_PARAMETER(uint32, CurrentAllocationFrameIndex)
			SHADER_PARAMETER(uint32, NumAllocationFrames)
			SHADER_PARAMETER(uint32, VertexBufferSize)
			SHADER_PARAMETER(uint32, IndexBufferSize)
		END_SHADER_PARAMETER_STRUCT()

		class FAllocateRangesDim : SHADER_PERMUTATION_BOOL("ALLOCATE_VERTICES_AND_TRIANGLES_RANGES");
		using FPermutationDomain = TShaderPermutationDomain<FAllocateRangesDim>;
	};
	IMPLEMENT_GLOBAL_SHADER(FInitQueueCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "InitQueue", SF_Compute);

	struct FNaniteStreamOutTraversalCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNaniteStreamOutTraversalCS);
		SHADER_USE_PARAMETER_STRUCT(FNaniteStreamOutTraversalCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HierarchyBuffer)
			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
			SHADER_PARAMETER(FIntVector4, PageConstants)

			SHADER_PARAMETER_STRUCT_INCLUDE(FQueueParameters, QueueParameters)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, AuxiliaryDataBufferRW)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, VertexBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IndexBuffer)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutputClustersRW)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutputClustersStateRW)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SegmentMappingBuffer)

			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CurrentNodeIndirectArgs)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, NextNodeIndirectArgs)

			SHADER_PARAMETER(float, StreamOutCutError)
			SHADER_PARAMETER(uint32, NodeLevel)

			SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)

			RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()

		class FCountVerticesAndTrianglesDim : SHADER_PERMUTATION_BOOL("NANITE_STREAM_OUT_COUNT_VERTICES_AND_TRIANGLES");
		class FCacheClustersDim : SHADER_PERMUTATION_BOOL("NANITE_STREAM_OUT_CACHE_CLUSTERS");
		class FCullingTypeDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_TYPE", NANITE_CULLING_TYPE_NODES, NANITE_CULLING_TYPE_CLUSTERS);
		using FPermutationDomain = TShaderPermutationDomain<FCountVerticesAndTrianglesDim, FCacheClustersDim, FCullingTypeDim>;

		static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

			OutEnvironment.SetDefine(TEXT("NANITE_HIERARCHY_TRAVERSAL"), 1);

			OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FNaniteStreamOutTraversalCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "NaniteStreamOutTraversalCS", SF_Compute);

	class FAllocateRangesCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FAllocateRangesCS);
		SHADER_USE_PARAMETER_STRUCT(FAllocateRangesCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, VertexAndIndexAllocator)
			SHADER_PARAMETER(uint32, CurrentAllocationFrameIndex)
			SHADER_PARAMETER(uint32, NumAllocationFrames)
			SHADER_PARAMETER(uint32, VertexBufferSize)
			SHADER_PARAMETER(uint32, IndexBufferSize)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutputClustersStateRW)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, StreamOutDispatchIndirectArgsRW)

			SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)
		END_SHADER_PARAMETER_STRUCT()
	};
	IMPLEMENT_GLOBAL_SHADER(FAllocateRangesCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "AllocateRangesCS", SF_Compute);

	struct FNaniteStreamOutCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNaniteStreamOutCS);
		SHADER_USE_PARAMETER_STRUCT(FNaniteStreamOutCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
			SHADER_PARAMETER(FIntVector4, PageConstants)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, AuxiliaryDataBufferRW)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, VertexBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IndexBuffer)

			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, OutputClusters)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutputClustersStateRW)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SegmentMappingBuffer)

			SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)

			RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()

		static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FNaniteStreamOutCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "NaniteStreamOutCS", SF_Compute);

	void AddPass_InitNodeCullArgs(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGEventName&& PassName, FRDGBufferUAVRef QueueState, FRDGBufferRef NodeCullArgs0, FRDGBufferRef NodeCullArgs1, uint32 CullingPass);
	void AddPass_InitClusterCullArgs(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGEventName&& PassName, FRDGBufferUAVRef QueueState, FRDGBufferRef ClusterCullArgs, uint32 CullingPass);

	static void AddInitQueuePass(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* ShaderMap,
		FQueueParameters& QueueParameters,
		FRDGBufferSRVRef RequestsDataSRV,
		uint32 NumRequests,
		bool bAllocateRanges,
		FRDGBufferUAVRef MeshDataBufferUAV,
		FRDGBufferUAVRef VertexAndIndexAllocatorUAV,
		uint32 CurrentAllocationFrameIndex,
		uint32 NumAllocationFrames,
		uint32 VertexBufferSize,
		uint32 IndexBufferSize)
	{
		// Reset queue to empty state
		AddClearUAVPass(GraphBuilder, QueueParameters.QueueState, 0);

		// Init queue with requests
		{
			FInitQueueCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitQueueCS::FParameters>();
			PassParameters->QueueParameters = QueueParameters;

			PassParameters->StreamOutRequests = RequestsDataSRV;
			PassParameters->NumRequests = NumRequests;

			PassParameters->MeshDataBuffer = MeshDataBufferUAV;

			PassParameters->VertexAndIndexAllocator = VertexAndIndexAllocatorUAV;
			PassParameters->CurrentAllocationFrameIndex = CurrentAllocationFrameIndex;
			PassParameters->NumAllocationFrames = NumAllocationFrames;
			PassParameters->VertexBufferSize = VertexBufferSize;
			PassParameters->IndexBufferSize = IndexBufferSize;

			FInitQueueCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FInitQueueCS::FAllocateRangesDim>(bAllocateRanges);

			auto ComputeShader = ShaderMap->GetShader<FInitQueueCS>(PermutationVector);

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::InitQueue"), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCountWrapped(NumRequests, 64));
		}
	}

	static void GetQueueParams(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FQueueParameters& OutQueueParameters)
	{
		const uint32 MaxNodes = Nanite::FGlobalResources::GetMaxNodes();
		const uint32 MaxCandidateClusters = Nanite::FGlobalResources::GetMaxCandidateClusters();

		FRDGBufferRef QueueState = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) + 2 * (6 * sizeof(uint32)), 1), TEXT("NaniteStreamOut.QueueState"));

		// Allocate buffer for nodes
		FRDGBufferRef NodesBuffer = nullptr;
		{
			const uint32 CandidateNodeSizeInUints = 3;
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxNodes * CandidateNodeSizeInUints);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			NodesBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("NaniteStreamOut.NodesBuffer"));
		}
		
		// Allocate candidate cluster buffer
		FRDGBufferRef CandidateClustersBuffer = nullptr;
		{
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxCandidateClusters * CandidateClusterSizeInUints);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			CandidateClustersBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("NaniteStreamOut.CandidateClustersBuffer"));
		}

		OutQueueParameters.QueueState = GraphBuilder.CreateUAV(QueueState);
		OutQueueParameters.Nodes = GraphBuilder.CreateUAV(NodesBuffer);
		OutQueueParameters.CandidateClusters = GraphBuilder.CreateUAV(CandidateClustersBuffer);
		OutQueueParameters.MaxNodes = MaxNodes;
		OutQueueParameters.MaxCandidateClusters = MaxCandidateClusters;
	}

	static void AddPass_StreamOutTraversal(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* ShaderMap,
		FSceneUniformBuffer& SceneUniformBuffer,
		float CutError,
		uint32 NumRequests,
		FRDGBufferRef RequestBuffer,
		FRDGBufferRef SegmentMappingBuffer,
		FRDGBufferRef MeshDataBuffer,
		FRDGBufferRef AuxiliaryDataBuffer,
		FRDGBufferRef VertexBuffer,
		uint32 MaxNumVertices,
		FRDGBufferRef IndexBuffer,
		uint32 MaxNumIndices,
		FRDGBufferRef VertexAndIndexAllocatorBuffer,
		FRDGBufferRef OutputClustersBuffer,
		FRDGBufferUAVRef OutputClustersStateUAV,
		FQueueParameters& QueueParameters,
		bool bCountPass
	)	
	{
		const bool bAllocateRanges = bCountPass ? false : true;
		AddInitQueuePass(
			GraphBuilder,
			ShaderMap,
			QueueParameters,
			GraphBuilder.CreateSRV(RequestBuffer),
			NumRequests,
			bAllocateRanges,
			GraphBuilder.CreateUAV(MeshDataBuffer),
			GraphBuilder.CreateUAV(VertexAndIndexAllocatorBuffer),
			0,
			1,
			MaxNumVertices,
			MaxNumIndices);

		FNaniteStreamOutTraversalCS::FParameters SharedParameters;

		SharedParameters.Scene = SceneUniformBuffer.GetBuffer(GraphBuilder);

		SharedParameters.QueueParameters = QueueParameters;

		SharedParameters.HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
		SharedParameters.ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		SharedParameters.PageConstants.X = 0;
		SharedParameters.PageConstants.Y = Nanite::GStreamingManager.GetMaxStreamingPages();

		SharedParameters.StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
		SharedParameters.NumRequests = NumRequests;

		SharedParameters.SegmentMappingBuffer = GraphBuilder.CreateSRV(SegmentMappingBuffer);

		SharedParameters.MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);
			
		SharedParameters.StreamOutCutError = CutError;

		ShaderPrint::SetParameters(GraphBuilder, SharedParameters.ShaderPrint);

		FNaniteStreamOutTraversalCS::FPermutationDomain PermutationVector;
		if (bCountPass)
		{
			SharedParameters.AuxiliaryDataBufferRW = nullptr;
			SharedParameters.VertexBuffer = nullptr;
			SharedParameters.IndexBuffer = nullptr;
			SharedParameters.OutputClustersRW = GraphBuilder.CreateUAV(OutputClustersBuffer);
			SharedParameters.OutputClustersStateRW = OutputClustersStateUAV;

			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCountVerticesAndTrianglesDim>(true);
			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCacheClustersDim>(GNaniteStreamOutCacheTraversalData);
		}
		else
		{
			SharedParameters.AuxiliaryDataBufferRW = GraphBuilder.CreateUAV(AuxiliaryDataBuffer);
			SharedParameters.VertexBuffer = GraphBuilder.CreateUAV(VertexBuffer);
			SharedParameters.IndexBuffer = GraphBuilder.CreateUAV(IndexBuffer);

			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCountVerticesAndTrianglesDim>(false);
			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCacheClustersDim>(false);
		}

		{
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, bCountPass, "CountVerticesAndTriangles");
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, !bCountPass, "StreamOutWithTraversal");

			// Node passes
			{
				FRDGBufferRef NodeCullArgs0 = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc((NANITE_MAX_CLUSTER_HIERARCHY_DEPTH + 1) * NANITE_NODE_CULLING_ARG_COUNT), TEXT("Nanite.CullArgs0"));
				FRDGBufferRef NodeCullArgs1 = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc((NANITE_MAX_CLUSTER_HIERARCHY_DEPTH + 1) * NANITE_NODE_CULLING_ARG_COUNT), TEXT("Nanite.CullArgs1"));

				AddPass_InitNodeCullArgs(GraphBuilder, ShaderMap, RDG_EVENT_NAME("InitNodeCullArgs"), QueueParameters.QueueState, NodeCullArgs0, NodeCullArgs1, 0);

				PermutationVector.Set<FNaniteStreamOutTraversalCS::FCullingTypeDim>(NANITE_CULLING_TYPE_NODES);
				auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutTraversalCS>(PermutationVector);

				const uint32 MaxLevels = Nanite::GStreamingManager.GetMaxHierarchyLevels();
				for (uint32 NodeLevel = 0; NodeLevel < MaxLevels; NodeLevel++)
				{
					auto* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutTraversalCS::FParameters>(&SharedParameters);

					FRDGBufferRef CurrentIndirectArgs = (NodeLevel & 1) ? NodeCullArgs1 : NodeCullArgs0;
					FRDGBufferRef NextIndirectArgs = (NodeLevel & 1) ? NodeCullArgs0 : NodeCullArgs1;

					PassParameters->CurrentNodeIndirectArgs = GraphBuilder.CreateSRV(CurrentIndirectArgs);
					PassParameters->NextNodeIndirectArgs = GraphBuilder.CreateUAV(NextIndirectArgs);
					PassParameters->IndirectArgs = CurrentIndirectArgs;
					PassParameters->NodeLevel = NodeLevel;

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("NodeCull_%d", NodeLevel),
						ComputeShader,
						PassParameters,
						CurrentIndirectArgs,
						NodeLevel * NANITE_NODE_CULLING_ARG_COUNT * sizeof(uint32)
					);
				}
			}
				
			// Cluster culling pass
			{
				FRDGBufferRef ClusterCullArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(3), TEXT("Nanite.ClusterCullArgs"));
				AddPass_InitClusterCullArgs(GraphBuilder, ShaderMap, RDG_EVENT_NAME("InitClusterCullArgs"), QueueParameters.QueueState, ClusterCullArgs, 0);

				PermutationVector.Set<FNaniteStreamOutTraversalCS::FCullingTypeDim>(NANITE_CULLING_TYPE_CLUSTERS);
				auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutTraversalCS>(PermutationVector);

				auto* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutTraversalCS::FParameters>(&SharedParameters);
				PassParameters->IndirectArgs = ClusterCullArgs;

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClusterCull"),
					ComputeShader,
					PassParameters,
					ClusterCullArgs,
					0
				);
			}
		}
	}

	void StreamOutData(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* ShaderMap,
		FSceneUniformBuffer &SceneUniformBuffer,
		float CutError,
		uint32 NumRequests,
		FRDGBufferRef RequestBuffer,
		FRDGBufferRef SegmentMappingBuffer,
		FRDGBufferRef MeshDataBuffer,
		FRDGBufferRef AuxiliaryDataBuffer,
		FRDGBufferRef VertexBuffer,
		uint32 MaxNumVertices,
		FRDGBufferRef IndexBuffer,
		uint32 MaxNumIndices)
	{
		RDG_EVENT_SCOPE_STAT(GraphBuilder, NaniteStreamOutData, "NaniteStreamOutData");
		RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteStreamOutData);

		FQueueParameters QueueParameters;
		GetQueueParams(GraphBuilder, ShaderMap, QueueParameters);

		FRDGBufferRef VertexAndIndexAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2), TEXT("NaniteStreamOut.VertexAndIndexAllocatorBuffer"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VertexAndIndexAllocatorBuffer), 0);

		const uint32 MaxCandidateClusters = Nanite::FGlobalResources::GetMaxCandidateClusters();

		// Allocate output cluster buffer
		FRDGBufferRef OutputClustersBuffer = nullptr;
		{
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxCandidateClusters * CandidateClusterSizeInUints);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			OutputClustersBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("NaniteStreamOut.OutputClustersBuffer"));
		}

		FRDGBufferRef OutputClustersStateBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2), TEXT("NaniteStreamOut.OutputClustersStateBuffer"));
		FRDGBufferUAVRef OutputClustersStateUAV = GraphBuilder.CreateUAV(OutputClustersStateBuffer);
		AddClearUAVPass(GraphBuilder, OutputClustersStateUAV, 0);

		// count pass
		AddPass_StreamOutTraversal(
			GraphBuilder, ShaderMap, SceneUniformBuffer, CutError, NumRequests, RequestBuffer, SegmentMappingBuffer,
			MeshDataBuffer, nullptr, nullptr, MaxNumVertices, nullptr, MaxNumIndices, VertexAndIndexAllocatorBuffer, OutputClustersBuffer, OutputClustersStateUAV, QueueParameters, true);

		// write pass
		if(!GNaniteStreamOutCacheTraversalData)
		{
			AddPass_StreamOutTraversal(
				GraphBuilder, ShaderMap, SceneUniformBuffer, CutError, NumRequests, RequestBuffer, SegmentMappingBuffer,
				MeshDataBuffer, AuxiliaryDataBuffer, VertexBuffer, MaxNumVertices, IndexBuffer, MaxNumIndices, VertexAndIndexAllocatorBuffer, nullptr, nullptr, QueueParameters, false);
		}
		else
		{
			FRDGBufferRef StreamOutDispatchIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("NaniteStreamOut.DispatchIndirectArgs"));

			// allocate vertex and index buffer ranges
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FAllocateRangesCS::FParameters>();

				PassParameters->StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
				PassParameters->NumRequests = NumRequests;

				PassParameters->MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);

				PassParameters->VertexAndIndexAllocator = GraphBuilder.CreateUAV(VertexAndIndexAllocatorBuffer);
				PassParameters->CurrentAllocationFrameIndex = 0;
				PassParameters->NumAllocationFrames = 1;
				PassParameters->VertexBufferSize = MaxNumVertices;
				PassParameters->IndexBufferSize = MaxNumIndices;

				PassParameters->OutputClustersStateRW = OutputClustersStateUAV;
				PassParameters->StreamOutDispatchIndirectArgsRW = GraphBuilder.CreateUAV(StreamOutDispatchIndirectArgsBuffer);

				ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

				auto ComputeShader = ShaderMap->GetShader<FAllocateRangesCS>();

				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::AllocateRanges"), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCountWrapped(NumRequests, 64));
			}

			// stream out mesh data
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutCS::FParameters>();

				PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
				PassParameters->PageConstants.X = 0;
				PassParameters->PageConstants.Y = Nanite::GStreamingManager.GetMaxStreamingPages();

				PassParameters->StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
				PassParameters->NumRequests = NumRequests;

				PassParameters->SegmentMappingBuffer = GraphBuilder.CreateSRV(SegmentMappingBuffer);

				PassParameters->AuxiliaryDataBufferRW = GraphBuilder.CreateUAV(AuxiliaryDataBuffer);

				PassParameters->MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);
				PassParameters->VertexBuffer = GraphBuilder.CreateUAV(VertexBuffer);
				PassParameters->IndexBuffer = GraphBuilder.CreateUAV(IndexBuffer);

				PassParameters->OutputClusters = GraphBuilder.CreateSRV(OutputClustersBuffer);
				PassParameters->OutputClustersStateRW = OutputClustersStateUAV;

				PassParameters->IndirectArgs = StreamOutDispatchIndirectArgsBuffer;

				ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

				auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutCS>();

				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::StreamOut"), ComputeShader, PassParameters, StreamOutDispatchIndirectArgsBuffer, 0);
			}
		}
	}
} // namespace Nanite

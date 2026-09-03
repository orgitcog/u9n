// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteAssemblyBuild.h"
#include "NaniteIntermediateResources.h"
#include "NaniteBuilder.h"
#include "Rendering/NaniteResources.h"
#include "ClusterDAG.h"
#include "Cluster.h"

namespace Nanite
{
#if NANITE_ASSEMBLY_DATA

static bool MergeAssemblyIntermediate(
	FIntermediateResources& Output,
	TArray<FCluster>& MipTailClusters,
	const FIntermediateResources& SrcIntermediate,
	const FMaterialRemapTable* MaterialRemap = nullptr,
	const TConstArrayView<FMatrix44f>& TransformList = TConstArrayView<FMatrix44f>(),
	uint32 AssemblyPartIndex = MAX_uint32)
{
	bool bHasHiResClusters = false;
	
	const int32 NumInstances = AssemblyPartIndex == MAX_uint32 ? 1 : TransformList.Num();
	check(NumInstances > 0);

	// Combine the part's contribution to the final product, multiplied by number of instances where applicable
	Output.NumInputTriangles += SrcIntermediate.NumInputTriangles * NumInstances;
	Output.NumInputVertices += SrcIntermediate.NumInputVertices * NumInstances;
	Output.MaxMipLevel = FMath::Max(Output.MaxMipLevel, SrcIntermediate.MaxMipLevel);

	FClusterDAG& DstDAG = Output.ClusterDAG;
	const FClusterDAG& SrcDAG = SrcIntermediate.ClusterDAG;

	DstDAG.bHasSkinning	|= SrcDAG.bHasSkinning;
	DstDAG.bHasTangents	|= SrcDAG.bHasTangents;
	DstDAG.bHasColors	|= SrcDAG.bHasColors;
	
	if (AssemblyPartIndex != MAX_uint32)
	{
		// Add the part's transformed bounds into the final product as well
		for (const FMatrix44f& Transform : TransformList)
		{
			const FBox3f LocalBox(SrcDAG.TotalBounds.Min, SrcDAG.TotalBounds.Max);
			const FBox3f Box = LocalBox.TransformBy(Transform);
			const float MaxScale = Transform.GetScaleVector().GetMax();
			DstDAG.TotalBounds	+= { .Min = FVector4f(Box.Min, 0.0f), .Max = FVector4f(Box.Max, 0.0f) };
			Output.SurfaceArea	+= SrcIntermediate.SurfaceArea * FMath::Square(MaxScale);
		}
	}
	else
	{
		DstDAG.TotalBounds	+= SrcDAG.TotalBounds;
		Output.SurfaceArea	+= SrcIntermediate.SurfaceArea;
	}

	TArray<uint32> GroupRemap, ClusterRemap, MipTailClusterIndices;
	GroupRemap.Init(MAX_uint32, SrcDAG.Groups.Num());
	ClusterRemap.Init(MAX_uint32, SrcDAG.Clusters.Num());
	MipTailClusterIndices.Reserve(SrcDAG.Clusters.Num());

	// Create and copy select groups and clusters and generate a lookup from source->output
	const int32 FirstOutputCluster = DstDAG.Clusters.Num();
	for (int32 SrcGroupIndex = 0; SrcGroupIndex < SrcDAG.Groups.Num(); ++SrcGroupIndex)
	{
		const FClusterGroup& SrcGroup = SrcDAG.Groups[SrcGroupIndex];
		if (SrcGroup.bTrimmed || SrcGroup.MeshIndex != 0)
		{
			// Ignore trimmed groups or groups from any mesh other than mesh 0
			continue;
		}

		if (SrcGroup.MipLevel == SrcIntermediate.MaxMipLevel)
		{
			// We'll add this group's clusters to the mip tail
			MipTailClusterIndices.Append(SrcGroup.Children);
			continue;
		}

		const int32 DstGroupIndex = DstDAG.Groups.Num();
		FClusterGroup& DstGroup = DstDAG.Groups.Add_GetRef(SrcGroup);
		GroupRemap[SrcGroupIndex] = DstGroupIndex;

		check(DstGroup.AssemblyPartIndex == MAX_uint32);
		DstGroup.AssemblyPartIndex = AssemblyPartIndex;

		DstGroup.Children.Empty(SrcGroup.Children.Num());
		for (uint32 SrcClusterIndex : SrcGroup.Children)
		{
			const FCluster& SrcCluster = SrcDAG.Clusters[SrcClusterIndex];
			const int32 DstClusterIndex = DstDAG.Clusters.Add(SrcCluster);
			DstGroup.Children.Add(DstClusterIndex);
			ClusterRemap[SrcClusterIndex] = DstClusterIndex;
		}
	}

	// Run through and fix up the copied clusters
	for (int32 ClusterIndex = FirstOutputCluster; ClusterIndex < DstDAG.Clusters.Num(); ++ClusterIndex)
	{
		FCluster& Cluster = DstDAG.Clusters[ClusterIndex];
		
		// Translate group indices
		Cluster.GroupIndex = GroupRemap[Cluster.GroupIndex];
		if (Cluster.GeneratingGroupIndex != MAX_uint32)
		{
			Cluster.GeneratingGroupIndex = GroupRemap[Cluster.GeneratingGroupIndex];
		}

		// Translate adjacency information
		{
			TMap<uint32, uint32> Temp;
			Temp.Reserve(Cluster.AdjacentClusters.Num());
			for (TPair<uint32, uint32>& KeyValue : Cluster.AdjacentClusters)
			{
				const uint32 RemappedClusterIndex = ClusterRemap[KeyValue.Key];
				if (RemappedClusterIndex != MAX_uint32)
				{
					Temp.Add(RemappedClusterIndex, KeyValue.Value);
				}
			}
			Cluster.AdjacentClusters = Temp;
		}

		// Remap materials
		if (MaterialRemap)
		{
			for (int32& MaterialIndex : Cluster.MaterialIndexes)
			{
				MaterialIndex = (*MaterialRemap)[MaterialIndex];
			}
			for (FMaterialRange& MaterialRange : Cluster.MaterialRanges)
			{
				MaterialRange.MaterialIndex = (*MaterialRemap)[MaterialRange.MaterialIndex];
			}
		}
	}

	auto MergeIntoMipTail = [&](auto&& TransformCluster)
	{
		for (uint32 ClusterIndex : MipTailClusterIndices)
		{
			FCluster& NewCluster = MipTailClusters.Add_GetRef(SrcDAG.Clusters[ClusterIndex]);

			// Clean up things that will need to be recalculated later
			NewCluster.Bricks.Empty();
			NewCluster.AdjacentClusters.Empty();
			NewCluster.MaterialRanges.Empty();
			NewCluster.QuantizedPositions.Empty();
			NewCluster.GroupIndex = MAX_uint32;

			if (NewCluster.GeneratingGroupIndex != MAX_uint32)
			{
				// Translate the generating group index
				NewCluster.GeneratingGroupIndex = GroupRemap[NewCluster.GeneratingGroupIndex];
			}

			// Remap materials
			if (MaterialRemap)
			{
				for (int32& MaterialIndex : NewCluster.MaterialIndexes)
				{
					MaterialIndex = (*MaterialRemap)[MaterialIndex];
				}
				for (FMaterialRange& MaterialRange : NewCluster.MaterialRanges)
				{
					MaterialRange.MaterialIndex = (*MaterialRemap)[MaterialRange.MaterialIndex];
				}
			}

			// Transform cluster
			TransformCluster(NewCluster);
		}
	};

	if (AssemblyPartIndex == MAX_uint32)
	{
		// No transformation or duplication needed
		MergeIntoMipTail([](FCluster&){});
	}
	else
	{
		// Duplicate, transform, and add highest mip level clusters to the mip tail list
		for (const FMatrix44f& Transform : TransformList)
		{
			const FMatrix44f NormalTransform = Transform.RemoveTranslation().Inverse().GetTransposed();
			MergeIntoMipTail(
				[&](FCluster& NewCluster)
				{
					// Transform positions/normals
					for (uint32 VertIndex = 0; VertIndex < NewCluster.NumVerts; VertIndex++)
					{
						FVector3f& Position = NewCluster.GetPosition(VertIndex);
						Position = Transform.TransformPosition(Position);

						FVector3f& Normal = NewCluster.GetNormal(VertIndex);
						Normal = NormalTransform.TransformVector(Normal);
						Normal.Normalize();

						if (SrcDAG.bHasTangents)
						{
							FVector3f& TangentX = NewCluster.GetTangentX(VertIndex);
							TangentX = Transform.TransformVector(TangentX);
							TangentX.Normalize();
						}
					}

					// Recompute bounds
					NewCluster.Bound();
				}
			);
		}
	}

	// return true if we actually added clusters to the output
	return FirstOutputCluster < DstDAG.Clusters.Num();
}

static bool BuildAssemblyParts(
	FIntermediateResources& Output,
	TArray<FCluster>& MipTailClusters,
	const FInputAssemblyData& AssemblyData)
{
	const int32 NumInputParts = AssemblyData.Parts.Num();
	const int32 NumAssemblyNodes = AssemblyData.Nodes.Num();
	TArray<FMatrix44f> FlattenedNodeTransforms;
	TArray<TArray<uint32>> NodeIndicesPerPart;
	TArray<uint32> PartsToMerge;

	FClusterDAG& DAG = Output.ClusterDAG;

	FlattenedNodeTransforms.Reserve(NumAssemblyNodes);
	NodeIndicesPerPart.AddDefaulted(NumInputParts);

	// Flatten the input hierarchy and create transform lists by part
	{
		for (int32 NodeIndex = 0; NodeIndex < NumAssemblyNodes; ++NodeIndex)
		{
			const auto& Node = AssemblyData.Nodes[NodeIndex];
			
			// Sanity check the caller arranged the hierarchy in hierarchy order and the node has a valid part index
			check(Node.ParentIndex < NodeIndex);
			check(AssemblyData.Parts.IsValidIndex(Node.PartIndex));

			NodeIndicesPerPart[Node.PartIndex].Add(NodeIndex);

			FMatrix44f& FlattenedTransform = FlattenedNodeTransforms.Emplace_GetRef(Node.Transform);
			if (Node.ParentIndex >= 0)
			{
				FlattenedTransform *= FlattenedNodeTransforms[Node.ParentIndex];
			}
		}
	}

	// Merge parts' clusters into the assembly output, save the last mip level off for the mip tail. For parts that 
	DAG.AssemblyPartData.Reserve(DAG.AssemblyPartData.Num() + NumInputParts);
	for (int32 InputPartIndex = 0; InputPartIndex < NumInputParts; ++InputPartIndex)
	{
		const FIntermediateResources& AssemblyPartIntermediate = *AssemblyData.Parts[InputPartIndex].Resource;		

		const int32 NumNodesInPart = NodeIndicesPerPart[InputPartIndex].Num();
		if (NumNodesInPart == 0)
		{
			// Don't need to merge this part (Should this be an error condition?)
			continue;
		}

		if (!AssemblyPartIntermediate.ClusterDAG.AssemblyPartData.IsEmpty())
		{
			// Not currently handled; unclear how to identify and de-duplicate common inner clusters
			// TODO: Nanite-Assemblies: Assemblies of assemblies
			UE_LOG(LogStaticMesh, Warning, TEXT("Failed to build Nanite assembly. Assemblies of assemblies is not currently supported."));
			return false;
		}

		const TArray<uint32>& PartNodeIndices = NodeIndicesPerPart[InputPartIndex];
		const FAssemblyPartData NewPart = {
			.FirstTransform = uint32(DAG.AssemblyTransforms.Num()),
			.NumTransforms = uint32(PartNodeIndices.Num())
		};
		for (uint32 TransformIndex : PartNodeIndices)
		{
			DAG.AssemblyTransforms.Add(FlattenedNodeTransforms[TransformIndex]);
		}
		const TConstArrayView<FMatrix44f> SrcTransforms = MakeConstArrayView(
			&DAG.AssemblyTransforms[NewPart.FirstTransform],
			NewPart.NumTransforms
		);

		bool bCreatePart = MergeAssemblyIntermediate(
			Output,
			MipTailClusters,
			AssemblyPartIntermediate,
			&AssemblyData.Parts[InputPartIndex].MaterialRemap,
			SrcTransforms,
			DAG.AssemblyPartData.Num()
		);

		if (bCreatePart)
		{
			// At least one high-res cluster is instanced more than once, so we need the part's transforms
			DAG.AssemblyPartData.Add(NewPart);
		}
		else
		{
			// All clusters are in the mip tail, so bail on adding this part
			DAG.AssemblyTransforms.SetNum(NewPart.FirstTransform);
		}
	}

	// Error out if the resulting transform count is too large
	const int32 NumFinalTransforms = DAG.AssemblyTransforms.Num();
	if (NumFinalTransforms > NANITE_MAX_ASSEMBLY_TRANSFORMS)
	{
		UE_LOG(LogStaticMesh, Error,
			TEXT("Merged Nanite assembly has too many transforms (%d). Max is %d."),
			NumFinalTransforms, NANITE_MAX_ASSEMBLY_TRANSFORMS);
		return false;
	}

	return true;
}

static void BuildAssemblyMipTail(FIntermediateResources& Output, TArray<FCluster>&& MipTailClusters)
{
	FClusterDAG& DAG = Output.ClusterDAG;

	const int32 MipTailClusterRangeStart = DAG.Clusters.Num();
	
	// Set the mip level for all mip tail clusters to the same number
	DAG.Clusters.Append(MoveTemp(MipTailClusters));
	for (int32 ClusterIndex = MipTailClusterRangeStart; ClusterIndex < DAG.Clusters.Num(); ++ClusterIndex)
	{
		DAG.Clusters[ClusterIndex].MipLevel = Output.MaxMipLevel;
	}
	
	// Now build a new DAG for the combined mip tail
	DAG.ReduceMesh( MipTailClusterRangeStart, DAG.Clusters.Num() - MipTailClusterRangeStart, 0 );

	// Push out the max mip level with the groups we just created
	Output.MaxMipLevel = DAG.Groups.Num() > 0 ? DAG.Groups.Last().MipLevel : 0;
}

bool BuildAssemblyData(FIntermediateResources& ParentIntermediate, const FInputAssemblyData& AssemblyData)
{
	TArray<FCluster> MipTailClusters;

	{
		FIntermediateResources Temp;
		Temp.ClusterDAG.Settings = ParentIntermediate.ClusterDAG.Settings;

		// Flatten all hierarchy transforms and merge all part clusters and groups, except for the final mip level of each.
		if (!BuildAssemblyParts(Temp, MipTailClusters, AssemblyData))
		{
			return false;
		}

		// Merge in parent's clusters and groups
		MergeAssemblyIntermediate(Temp, MipTailClusters, ParentIntermediate);

		ParentIntermediate = MoveTemp(Temp);
	}

	// Merge final mip of all parts and continue the DAG
	BuildAssemblyMipTail(ParentIntermediate, MoveTemp(MipTailClusters));

	return true;
}

#else 

bool BuildAssemblyData(FIntermediateResources& ParentIntermediate, const FInputAssemblyData& AssemblyData)
{
	return false;
}

#endif // NANITE_ASSEMBLY_DATA

} // namespace Nanite

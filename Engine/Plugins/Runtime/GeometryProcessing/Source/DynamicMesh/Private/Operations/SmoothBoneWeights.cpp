// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/SmoothBoneWeights.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/NonManifoldMappingSupport.h"
#include "Util/ProgressCancel.h"
#include "Math/UnrealMathUtility.h"
#include "Parameterization/MeshLocalParam.h"
#include "DynamicMesh/MeshNormals.h"
#include "Tasks/Task.h"
#include "Async/TaskGraphInterfaces.h"

using namespace UE::AnimationCore;
using namespace UE::Geometry;

namespace SmoothBoneWeightsLocals 
{	
	template <typename BoneIndexType, typename BoneWeightType>
	void NormalizeWeights(TMap<BoneIndexType, BoneWeightType>& InOutWeights)
	{
		BoneWeightType TotalWeight = (BoneWeightType) 0.f;
		for (const TTuple<BoneIndexType, BoneWeightType>& Weight : InOutWeights)
		{
			TotalWeight += Weight.Value;
		}

		if (!FMath::IsNearlyZero(TotalWeight))
		{
			for (TTuple<BoneIndexType, BoneWeightType>& Weight : InOutWeights)
			{
				Weight.Value /= TotalWeight;
			}
		}
	}
	
	/** Data source implementation for bone weights data stored in the FDynamicMeshVertexSkinWeightsAttribute. */
	class FSkinWeightsAttributeDataSource : public TBoneWeightsDataSource<FBoneIndexType, float>
	{
	public:
		FSkinWeightsAttributeDataSource(const FDynamicMeshVertexSkinWeightsAttribute* InAttribute)
		:
		Attribute(InAttribute) 
		{
			checkSlow(Attribute);
		}

		virtual ~FSkinWeightsAttributeDataSource() = default; 

		virtual int32 GetBoneNum(const int32 VertexID) override
		{
			FBoneWeights Weights;
			Attribute->GetValue(VertexID, Weights);
			return Weights.Num();
		}

		virtual FBoneIndexType GetBoneIndex(const int32 VertexID, const int32 Index) override
		{
			FBoneWeights Weights;
			Attribute->GetValue(VertexID, Weights);
			return Weights[Index].GetBoneIndex();
		}

		virtual float GetBoneWeight(const int32 VertexID, const int32 Index) override
		{
			FBoneWeights Weights;
			Attribute->GetValue(VertexID, Weights);
			return Weights[Index].GetWeight();
		}

		virtual float GetWeightOfBoneOnVertex(const int32 VertexID, const FBoneIndexType BoneIndex) override
		{
			FBoneWeights Weights;
			Attribute->GetValue(VertexID, Weights);
			for (const FBoneWeight& BoneWeight : Weights)
			{
				if (BoneWeight.GetBoneIndex() == BoneIndex)
				{
					return BoneWeight.GetWeight();;
				}
			}

			return 0.f;
		}

	protected:
		const FDynamicMeshVertexSkinWeightsAttribute* Attribute = nullptr;
	};
}


//
// TSmoothBoneWeights
//

template <typename BoneIndexType, typename BoneWeightType>
TSmoothBoneWeights<BoneIndexType, BoneWeightType>::TSmoothBoneWeights(const FDynamicMesh3* InSourceMesh,
																	  TBoneWeightsDataSource<BoneIndexType, BoneWeightType>* InDataSource)
:
SourceMesh(InSourceMesh),
DataSource(InDataSource)
{
	checkSlow(InSourceMesh);
}

template <typename BoneIndexType, typename BoneWeightType>
bool TSmoothBoneWeights<BoneIndexType, BoneWeightType>::Cancelled()
{
	return (Progress == nullptr) ? false : Progress->Cancelled();
}

template <typename BoneIndexType, typename BoneWeightType>
EOperationValidationResult TSmoothBoneWeights<BoneIndexType, BoneWeightType>::Validate()
{	
	if (SourceMesh == nullptr) 
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}

	if (DataSource == nullptr)
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}

	return EOperationValidationResult::Ok;
}

template <typename BoneIndexType, typename BoneWeightType>
bool TSmoothBoneWeights<BoneIndexType, BoneWeightType>::SmoothWeightsAtVertex(const int32  VertexID,
																			  const BoneWeightType VertexFalloff,
																			  TMap<BoneIndexType, BoneWeightType>& OutFinalWeights)
{
	using namespace SmoothBoneWeightsLocals;
	
	const UE::Geometry::FNonManifoldMappingSupport NonManifoldMappingSupport(*SourceMesh);

	// for each vertex in the stamp...
	constexpr int32 AvgNumNeighbors = 8;
	using VertexNeighborWeights = TArray<BoneWeightType, TInlineAllocator<AvgNumNeighbors>>;
	TArray<int32> AllNeighborVertices;
	TMap<BoneIndexType, VertexNeighborWeights> WeightsOnAllNeighbors;
	const int32 SrcVertexID = NonManifoldMappingSupport.GetOriginalNonManifoldVertexID(VertexID);

	// get list of all neighboring vertices, AND this vertex
	AllNeighborVertices.Add(VertexID);
	for (const int32 NeighborVertexID : SourceMesh->VtxVerticesItr(SrcVertexID))
	{
		AllNeighborVertices.Add(NeighborVertexID);
	}

	// get all weights above a given threshold across ALL neighbors (including self)
	for (const int32 NeighborVertexID : AllNeighborVertices)
	{
		for (int32 Index = 0; Index < DataSource->GetBoneNum(NeighborVertexID); ++Index)
		{	
			const BoneWeightType Weight = DataSource->GetBoneWeight(NeighborVertexID, Index);
			if (Weight > MinimumWeightThreshold)
			{
				VertexNeighborWeights& BoneWeights = WeightsOnAllNeighbors.FindOrAdd(DataSource->GetBoneIndex(NeighborVertexID, Index));
				BoneWeights.Add(Weight);
			}
		}
	}

	// calculate single average weight of each bone on all the neighbors
	OutFinalWeights.Reset();
	for (const TTuple<BoneIndexType, VertexNeighborWeights>& NeighborWeights : WeightsOnAllNeighbors)
	{
		BoneWeightType TotalWeightOnThisBone = (BoneWeightType) 0.f;
		for (const BoneWeightType& Value : NeighborWeights.Value)
		{
			TotalWeightOnThisBone += Value;
		}
		OutFinalWeights.Add(NeighborWeights.Key, TotalWeightOnThisBone / (BoneWeightType)NeighborWeights.Value.Num());
	}

	// normalize the weights
	NormalizeWeights(OutFinalWeights);

	// lerp weights from previous values, to fully relaxed values by brush strength scaled by falloff
	for (TTuple<BoneIndexType, BoneWeightType>& FinalWeight : OutFinalWeights)
	{
		const BoneIndexType BoneIndex = FinalWeight.Key;
		BoneWeightType NewWeight = FinalWeight.Value;
		BoneWeightType OldWeight = DataSource->GetWeightOfBoneOnVertex(VertexID, BoneIndex);
		FinalWeight.Value = FMath::Lerp<BoneWeightType>(OldWeight, NewWeight, VertexFalloff);
	}

	// normalize again
	NormalizeWeights(OutFinalWeights);

	if (Cancelled()) 
	{
		return false;
	}
		
	return true;
}


//
// FSmoothDynamicMeshVertexSkinWeights
//

FSmoothDynamicMeshVertexSkinWeights::FSmoothDynamicMeshVertexSkinWeights(const FDynamicMesh3* InSourceMesh, const FName InProfile)
:
TSmoothBoneWeights(InSourceMesh, nullptr)
{
	if (SourceMesh && SourceMesh->Attributes())
	{
		Attribute = SourceMesh->Attributes()->GetSkinWeightsAttribute(InProfile);
		if (Attribute)
		{
			SkinWeightsAttributeDataSource = MakeUnique<SmoothBoneWeightsLocals::FSkinWeightsAttributeDataSource>(Attribute);
			DataSource = SkinWeightsAttributeDataSource.Get();
		}
	}
}

FSmoothDynamicMeshVertexSkinWeights::FSmoothDynamicMeshVertexSkinWeights(const FDynamicMesh3* InSourceMesh, FDynamicMeshVertexSkinWeightsAttribute* InAttribute)
:
TSmoothBoneWeights(InSourceMesh, nullptr),
Attribute(InAttribute)
{
	if (InAttribute)
	{
		SkinWeightsAttributeDataSource = MakeUnique<SmoothBoneWeightsLocals::FSkinWeightsAttributeDataSource>(InAttribute);
		DataSource = SkinWeightsAttributeDataSource.Get();
	}
}


EOperationValidationResult FSmoothDynamicMeshVertexSkinWeights::Validate()
{	
	if (Attribute == nullptr) 
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}

	if (MaxNumInfluences <= 0)
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}

	return TSmoothBoneWeights::Validate();
}

bool FSmoothDynamicMeshVertexSkinWeights::SmoothWeightsAtVertex(const int32 VertexID, const float VertexFalloff)
{	
	TMap<FBoneIndexType, float> FinalWeights;
	if (TSmoothBoneWeights<FBoneIndexType, float>::SmoothWeightsAtVertex(VertexID, VertexFalloff, FinalWeights))
	{
		FBoneWeightsSettings BoneSettings;
		BoneSettings.SetNormalizeType(EBoneWeightNormalizeType::None);
		FBoneWeights WeightArray;
		
		for (const TTuple<FBoneIndexType, float>& FinalWeight : FinalWeights)
		{
			WeightArray.SetBoneWeight(FBoneWeight(FinalWeight.Key, FinalWeight.Value), BoneSettings);
		}
		
		BoneSettings.SetNormalizeType(EBoneWeightNormalizeType::Always);
		BoneSettings.SetMaxWeightCount(MaxNumInfluences);

		// make sure we do not exceed the max influence limit
		WeightArray.Renormalize(BoneSettings);

		Attribute->SetValue(VertexID, WeightArray);

		return true;
	}

	return false;
}

bool FSmoothDynamicMeshVertexSkinWeights::SmoothWeightsAtVerticesWithinDistance(const TArray<int32>& Vertices, 
																  				const float Strength, 
																  				const double FloodFillUpToDistance,
																				const int32 NumIterations)
{
	TSet<int32> VerticesToSmooth;
	VerticesToSmooth.Append(Vertices);

	const double FloodFillUpToDistanceSquared = FloodFillUpToDistance * FloodFillUpToDistance;

	// We want to add vertices to the VerticesToSmooth within FloodFillUpToDistance away from each vertex in the 
	// Vertices array
	if (FloodFillUpToDistance > 0)
	{
		// Now this process is quite fast, so cancellation can be check at the beginning
		if (Cancelled())
		{
			return false;
		}

		const int32 NumVertices = Vertices.Num();
		const int32 NumTasks = FMath::Max(FMath::Min(FTaskGraphInterface::Get().GetNumWorkerThreads(), NumVertices), 1);
		constexpr int32 MinVerticesByTask = 20;
		const int32 VerticesByTask = FMath::Max(FMath::DivideAndRoundUp(NumVertices, NumTasks), MinVerticesByTask);
		const int32 NumBatches = FMath::DivideAndRoundUp(NumVertices, VerticesByTask);
		TArray<UE::Tasks::FTask> PendingTasks;
		std::atomic<int32> NumAccessSet = 0;
		for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
		{
			const int32 StartIndex = BatchIndex * VerticesByTask;
			int32 EndIndex = (BatchIndex + 1) * VerticesByTask;
			EndIndex = BatchIndex == NumBatches - 1 ? FMath::Min(NumVertices, EndIndex) : EndIndex;
			UE::Tasks::FTask PendingTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, StartIndex, EndIndex, &Vertices, &VerticesToSmooth, FloodFillUpToDistance, FloodFillUpToDistanceSquared, &NumAccessSet]()
			{
				for (int32 Index = StartIndex; Index < EndIndex; ++Index)
				{
					const int32 SeedVID = Vertices[Index];
					
					// If at least one neighboring vertex is not part of the set of vertices to smooth then we need to flood
					bool bNeedToFlood = false;
					for (const int32 NeighborVertexID : SourceMesh->VtxVerticesItr(SeedVID))
					{
						bool bVerticesToSmoothContainsNeighborVertexID;
						while (true)
						{
							// Make sure to read in the set only when we are not writing in it, INDEX_NONE is used when are are writing in the set
							// Store the number of thread which are reading from the set in NumAccessSet
							if (int32 NumAccessSetExpected = NumAccessSet.load(std::memory_order_relaxed); NumAccessSetExpected != INDEX_NONE)
							{
								// Increment the NumAccessSet to read
								if (NumAccessSet.compare_exchange_weak(NumAccessSetExpected, NumAccessSetExpected+1, std::memory_order_relaxed))
								{
									bVerticesToSmoothContainsNeighborVertexID = VerticesToSmooth.Contains(NeighborVertexID);
									NumAccessSet.fetch_sub(1, std::memory_order_relaxed); // Decrement the NumAccessSet to release
									break;
								}
							}
						}
						if (!bVerticesToSmoothContainsNeighborVertexID &&
							DistanceSquared(SourceMesh->GetVertex(SeedVID), SourceMesh->GetVertex(NeighborVertexID)) < FloodFillUpToDistanceSquared)
						{
							bNeedToFlood = true;
							break;
						}
					}

					if (bNeedToFlood)
					{
						FVector3d Normal = FMeshNormals::ComputeVertexNormal(*SourceMesh, SeedVID);
						FFrame3d SeedFrame = SourceMesh->GetVertexFrame(SeedVID, false, &Normal);

						TMeshLocalParam<FDynamicMesh3> Param(SourceMesh);
						Param.ParamMode = ELocalParamTypes::ExponentialMapUpwindAvg;
						Param.ComputeToMaxDistance(SeedVID, SeedFrame, FloodFillUpToDistance);

						// Only points within FloodFillUpToDistance should have UVs set
						TArray<int32> PointsWithinDistance;
						Param.GetPointsWithUV(PointsWithinDistance);

						while (true)
						{
							// If none set are reading from the set try to write new data and set the atomic to INDEX_NONE
							if (int32 Expected = NumAccessSet.load(std::memory_order_relaxed); Expected == 0)
							{
								if (NumAccessSet.compare_exchange_weak(Expected, INDEX_NONE, std::memory_order_relaxed))
								{
									VerticesToSmooth.Append(PointsWithinDistance);
									NumAccessSet.store(0, std::memory_order_relaxed); // Release the atomic for reading or writing
									break;
								}
							}
						}
					}
				}
			});
			PendingTasks.Add(PendingTask);
		}
		UE::Tasks::Wait(PendingTasks);
	}

	for (int32 Itr = 0; Itr < NumIterations; ++Itr)
	{
		for (const int32 VertexID : VerticesToSmooth)
		{
			if (!FSmoothDynamicMeshVertexSkinWeights::SmoothWeightsAtVertex(VertexID, Strength))
			{
				return false;
			}
		}
	}

	return true;
}


// template instantiation
template class DYNAMICMESH_API UE::Geometry::TSmoothBoneWeights<FBoneIndexType, float>;
template class DYNAMICMESH_API UE::Geometry::TSmoothBoneWeights<int, float>;
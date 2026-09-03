// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEngineUtil.h"
#include "ReferenceSkeleton.h"

namespace UE::Dataflow
{

	namespace Animation
	{
		void GlobalTransformsInternal(int32 Index, const FReferenceSkeleton& Ref, TArray<FTransform>& Mat, TArray<bool>& Visited)
		{
			if (!Visited[Index])
			{
				const TArray<FTransform>& RefMat = Ref.GetRefBonePose();

				int32 ParentIndex = Ref.GetParentIndex(Index);
				if (ParentIndex != INDEX_NONE && ParentIndex != Index) // why self check?
				{
					GlobalTransformsInternal(ParentIndex, Ref, Mat, Visited);
					Mat[Index].SetFromMatrix(RefMat[Index].ToMatrixWithScale() * Mat[ParentIndex].ToMatrixWithScale());
				}
				else
				{
					Mat[Index] = RefMat[Index];
				}

				Visited[Index] = true;
			}
		}

		void GlobalTransforms(const FReferenceSkeleton& Ref, TArray<FTransform>& Mat)
		{
			TArray<bool> Visited;
			Visited.Init(false, Ref.GetNum());
			Mat.SetNum(Ref.GetNum());

			int32 Index = Ref.GetNum() - 1;
			while (Index >= 0)
			{
				GlobalTransformsInternal(Index, Ref, Mat, Visited);
				Index--;
			}
		}
	}

	namespace Color
	{
		FLinearColor GetRandomColor(const int32 RandomSeed, int32 Idx)
		{
			FRandomStream RandomStream(RandomSeed * 7 + Idx * 41);

			const uint8 R = static_cast<uint8>(RandomStream.FRandRange(128, 255));
			const uint8 G = static_cast<uint8>(RandomStream.FRandRange(128, 255));
			const uint8 B = static_cast<uint8>(RandomStream.FRandRange(128, 255));

			return FLinearColor(FColor(R, G, B, 255));
		}
	}
}


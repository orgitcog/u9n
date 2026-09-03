// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/PBDSpringConstraintsBase.h"
#include "Chaos/XPBDCorotatedConstraints.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitQRSVD.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/NewtonCorotatedCache.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/Utilities.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/XPBDWeakConstraints.h"
#include "Chaos/Triangle.h"
#include "Chaos/TriangleCollisionPoint.h"
#include "Chaos/TriangleMesh.h"
#include <unordered_map>
namespace Chaos::Softs
{
	using Chaos::TVec3;

	template <typename T>
	struct FGaussSeidelWeakConstraintSingleData
	{
		TArray<int32> SingleIndices  = {};
		TArray<int32> SingleSecondIndices = {};
		T SingleStiffness = (T)0.;
		TArray<T> SingleWeights = {};
		TArray<T> SingleSecondWeights = {};
		bool bIsAnisotropic = false;
		TVec3<T> SingleNormal = TVec3<T>((T)0.);
		bool bIsZeroRestLength = false;
		T RestLength = (T)0.;
	};

	template<class T>
	class TGaussSeidelWeakConstraintData : public TArrayCollection
	{
	public:
		TGaussSeidelWeakConstraintData()
		{
			AddArray(&MIndices);
			AddArray(&MSecondIndices);
			AddArray(&MWeights);
			AddArray(&MSecondWeights);
			AddArray(&MStiffness);
			AddArray(&MIsAnisotropic);
			AddArray(&MNormals);
			AddArray(&MIsZeroRestLength);
			AddArray(&MRestLength);
		}
		TGaussSeidelWeakConstraintData(const TGaussSeidelWeakConstraintData<T>& Other) = delete;
		TGaussSeidelWeakConstraintData(TGaussSeidelWeakConstraintData<T>&& Other)
		    : TArrayCollection(), MIndices(MoveTemp(Other.MIndices))
			, MSecondIndices(MoveTemp(Other.MSecondIndices))
			, MWeights(MoveTemp(Other.MWeights))
			, MSecondWeights(MoveTemp(Other.MSecondWeights))
			, MStiffness(MoveTemp(Other.MStiffness))
			, MIsAnisotropic(MoveTemp(Other.MIsAnisotropic))
		{
			AddParticles(Other.Size());
			AddArray(&MIndices);
			AddArray(&MSecondIndices);
			AddArray(&MWeights);
			AddArray(&MSecondWeights);
			AddArray(&MStiffness);
			AddArray(&MIsAnisotropic);
			AddArray(&MNormals);
			AddArray(&MIsZeroRestLength);
			AddArray(&MRestLength);
			Other.MSize = 0;
		}

		virtual ~TGaussSeidelWeakConstraintData()
		{}

		void AddConstraints(const int32 Num)
		{
			AddElementsHelper(Num);
		}

		void RemoveConstraint(const int32 Idx)
		{
			RemoveAtSwapHelper(Idx);
		}

		void SetSingleConstraint(const FGaussSeidelWeakConstraintSingleData<T>& SingleData, const int32 ConstraintIndex)
		{
			MIndices[ConstraintIndex] = SingleData.SingleIndices;
			MSecondIndices[ConstraintIndex] = SingleData.SingleSecondIndices;
			MStiffness[ConstraintIndex] = SingleData.SingleStiffness;
			MWeights[ConstraintIndex] = SingleData.SingleWeights;
			MSecondWeights[ConstraintIndex] = SingleData.SingleSecondWeights;
			MNormals[ConstraintIndex] = SingleData.SingleNormal;
			MIsAnisotropic[ConstraintIndex] = SingleData.bIsAnisotropic;
			MIsZeroRestLength[ConstraintIndex] = SingleData.bIsZeroRestLength;
		}

		void AddSingleConstraint(const FGaussSeidelWeakConstraintSingleData<T>& SingleData)
		{
			AddConstraints(1);
			SetSingleConstraint(SingleData, MSize - 1);
		}

		int32 Size() const 
		{
			return static_cast<int32>(MSize);
		}

		void Resize(const int32 Num)
		{
			ResizeHelper(Num);
		}

		TGaussSeidelWeakConstraintData& operator=(TGaussSeidelWeakConstraintData<T>&& Other)
		{
			MIndices = MoveTemp(Other.MIndices);
			MSecondIndices = MoveTemp(Other.MSecondIndices);
			MWeights = MoveTemp(Other.MWeights);
			MSecondWeights = MoveTemp(Other.MSecondWeights);
			MStiffness = MoveTemp(Other.MStiffness);
			MIsAnisotropic = MoveTemp(Other.MIsAnisotropic);
			MNormals = MoveTemp(Other.MNormals);
			MIsZeroRestLength = MoveTemp(Other.MIsZeroRestLength);
			ResizeHelper(Other.Size());
			Other.MSize = 0;
			return *this;
		}

		inline const TArrayCollectionArray<TArray<int32>>& Indices() const
		{
			return MIndices;
		}

		const TArray<int32>& GetIndices(const int32 Index) const
		{
			return MIndices[Index];
		}

		void SetIndices(const int32 Index, const TArray<int32>& InIndices)
		{
			MIndices[Index] = InIndices;
		}

		inline const TArrayCollectionArray<TArray<int32>>& SecondIndices() const
		{
			return MSecondIndices;
		}

		const TArray<int32>& GetSecondIndices(const int32 Index) const
		{
			return MSecondIndices[Index];
		}

		void SetSecondIndices(const int32 Index, const TArray<int32>& InIndices)
		{
			MSecondIndices[Index] = InIndices;
		}

		inline const TArrayCollectionArray<TArray<T>>& Weights() const
		{
			return MWeights;
		}

		const TArray<T>& GetWeights(const int32 Index) const
		{
			return MWeights[Index];
		}

		void SetWeights(const int32 Index, const TArray<int32>& InWeights)
		{
			MWeights[Index] = InWeights;
		}

		inline const TArrayCollectionArray<TArray<T>>& SecondWeights() const
		{
			return MSecondWeights;
		}

		const TArray<T>& GetSecondWeights(const int32 Index) const
		{
			return MSecondWeights[Index];
		}

		void SetSecondWeights(const int32 Index, const TArray<int32>& InWeights)
		{
			MSecondWeights[Index] = InWeights;
		}

		const bool GetIsAnisotropic(const int32 Index) const
		{
			return MIsAnisotropic[Index];
		}

		void SetIsAnisotropic(const int32 Index, const bool InIsAnisotropic)
		{
			MIsAnisotropic[Index] = InIsAnisotropic;
		}

		inline const TArrayCollectionArray<TVec3<T>>& Normals() const
		{
			return MNormals;
		}

		const TVec3<T>& GetNormal(const int32 Index) const
		{
			return MNormals[Index];
		}

		void SetNormal(const int32 Index, const TVec3<T>& InNormal)
		{
			MNormals[Index] = InNormal;
		}

		inline const TArrayCollectionArray<T>& Stiffness() const
		{
			return MStiffness;
		}

		T GetStiffness(const int32 Index) const
		{
			return MStiffness[Index];
		}

		void SetStiffness(const int32 Index, const T InStiffness)
		{
			MStiffness[Index] = InStiffness;
		}

		const bool GetIsZeroRestLength(const int32 Index) const
		{
			return MIsZeroRestLength[Index];
		}

		void SetIsZeroRestLength(const int32 Index, const bool InIsZeroRestLength)
		{
			MIsZeroRestLength[Index] = InIsZeroRestLength;
		}

		void SetRestLength(const int32 Index, const T InRestLength)
		{
			MRestLength[Index] = InRestLength;
		}

		const FGaussSeidelWeakConstraintSingleData<T> GetSingleConstraintData(const int32 ConstraintIndex) const 
		{
			FGaussSeidelWeakConstraintSingleData<T> SingleConstraintData;
			check(static_cast<uint32>(ConstraintIndex) < MSize);
			if (ConstraintIndex > INDEX_NONE && static_cast<uint32>(ConstraintIndex) < MSize)
			{
				SingleConstraintData.SingleIndices = MIndices[ConstraintIndex];
				SingleConstraintData.SingleSecondIndices = MSecondIndices[ConstraintIndex];
				SingleConstraintData.SingleStiffness = MStiffness[ConstraintIndex];
				SingleConstraintData.SingleWeights = MWeights[ConstraintIndex];
				SingleConstraintData.SingleSecondWeights = MSecondWeights[ConstraintIndex];
				SingleConstraintData.bIsAnisotropic = MIsAnisotropic[ConstraintIndex];
				SingleConstraintData.SingleNormal = MNormals[ConstraintIndex];
				SingleConstraintData.bIsZeroRestLength = MIsZeroRestLength[ConstraintIndex];
				SingleConstraintData.RestLength = MRestLength[ConstraintIndex];
			}
			return SingleConstraintData;
		}

	private:
		TArrayCollectionArray<TArray<int32>> MIndices;
		TArrayCollectionArray<TArray<int32>> MSecondIndices;
		TArrayCollectionArray<TArray<T>> MWeights;
		TArrayCollectionArray<TArray<T>> MSecondWeights;
		TArrayCollectionArray<T> MStiffness;
		TArrayCollectionArray<bool> MIsAnisotropic;
		TArrayCollectionArray<TVector<T, 3>> MNormals;
		TArrayCollectionArray<bool> MIsZeroRestLength;
		TArrayCollectionArray<T> MRestLength;
	};



	template <typename T, typename ParticleType>
	struct FGaussSeidelWeakConstraints 
	{
		//TODO(Yizhou): Add unittest for Gauss Seidel Weak Constraints
		FGaussSeidelWeakConstraints(
			const TArray<TArray<int32>>& InIndices,
			const TArray<TArray<T>>& InWeights,
			const TArray<T>& InStiffness,
			const TArray<TArray<int32>>& InSecondIndices,
			const TArray<TArray<T>>& InSecondWeights,
			const FDeformableXPBDWeakConstraintParams& InParams
		): DebugDrawParams(InParams)
		{
			ensureMsgf(InIndices.Num() == InSecondIndices.Num(), TEXT("Input Double Bindings have wrong size"));

			ConstraintsData.Resize(0);

			ConstraintsData.AddConstraints(InIndices.Num());

			for (int32 i = 0; i < InIndices.Num(); i++)
			{
				FGaussSeidelWeakConstraintSingleData<T> SingleConstraintData;
				SingleConstraintData.SingleIndices = InIndices[i];
				SingleConstraintData.SingleSecondIndices = InSecondIndices[i];
				SingleConstraintData.SingleWeights = InWeights[i];
				SingleConstraintData.SingleSecondWeights = InSecondWeights[i];
				SingleConstraintData.SingleStiffness = InStiffness[i];
				ConstraintsData.SetSingleConstraint(SingleConstraintData, i);
			}

			for (int32 i = 0; i < ConstraintsData.Size(); i++)
			{
				const TArray<int32>& SingleIndices = ConstraintsData.GetIndices(i);
				const TArray<int32>& SingleSecondIndices = ConstraintsData.GetSecondIndices(i);
				for (int32 j = 0; j < SingleSecondIndices.Num(); j++)
				{
					ensureMsgf(!SingleIndices.Contains(SingleSecondIndices[j]), TEXT("Indices and Second Indices overlaps. Currently not supported"));
				}
			}
		}

		struct FGaussSeidelConstraintHandle
		{
			int32 ConstraintIndex;
		};

		virtual ~FGaussSeidelWeakConstraints() {}

		void ComputeInitialWCData(const ParticleType& InParticles)
		{
			TArray<TArray<int32>> ExtraConstraints;
			ExtraConstraints.Init(TArray<int32>(), ConstraintsData.Size());
			for (int32 ConstraintIdx = 0; ConstraintIdx < ExtraConstraints.Num(); ++ConstraintIdx)
			{
				ExtraConstraints[ConstraintIdx].SetNum(ConstraintsData.GetIndices(ConstraintIdx).Num() + ConstraintsData.GetSecondIndices(ConstraintIdx).Num());
				for (int32 LocalIdx = 0; LocalIdx < ConstraintsData.GetIndices(ConstraintIdx).Num(); ++LocalIdx)
				{
					ExtraConstraints[ConstraintIdx][LocalIdx] = ConstraintsData.GetIndices(ConstraintIdx)[LocalIdx];
				}
				for (int32 LocalSecondIdx = 0; LocalSecondIdx < ConstraintsData.GetSecondIndices(ConstraintIdx).Num(); ++LocalSecondIdx)
				{
					ExtraConstraints[ConstraintIdx][LocalSecondIdx+ConstraintsData.GetIndices(ConstraintIdx).Num()] = ConstraintsData.GetSecondIndices(ConstraintIdx)[LocalSecondIdx];
				}
			}
			WCIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &WCIncidentElementsLocal);

			//Update rest state normal and nodal weights
			UpdateTriangleNormalAndNodalWeight(InParticles, /*bUseParticleX = */true);
			
			NoCollisionNodalWeights = NodalWeights;
			NoCollisionConstraints = ExtraConstraints;
			InitialWCSize = ConstraintsData.Size();

			NoCollisionWCIncidentElements = WCIncidentElements;
			NoCollisionWCIncidentElementsLocal = WCIncidentElementsLocal;

			// Compute rest length
			for (int32 ConstraintIdx = 0; ConstraintIdx < ConstraintsData.Size(); ++ConstraintIdx)
			{
				if (ConstraintsData.GetIsZeroRestLength(ConstraintIdx))
				{
					ConstraintsData.SetRestLength(ConstraintIdx, 0);
				}
				else
				{
					const TArray<int32>& Indices = ConstraintsData.GetIndices(ConstraintIdx);
					const TArray<int32>& SecondIndices = ConstraintsData.GetSecondIndices(ConstraintIdx);
					const TArray<T>& Weights = ConstraintsData.GetWeights(ConstraintIdx);
					const TArray<T>& SecondWeights = ConstraintsData.GetSecondWeights(ConstraintIdx);
					const TVec3<T> RestSpringEdge = ComputeSpringEdge(InParticles, Indices, SecondIndices, Weights, SecondWeights, /*bUseParticleX =*/true);
					if (ConstraintsData.GetIsAnisotropic(ConstraintIdx))
					{
						//if the spring is anisotropic, rest length could be negative depending on the normal direction
						ConstraintsData.SetRestLength(ConstraintIdx, TVec3<T>::DotProduct(ConstraintsData.GetNormal(ConstraintIdx), RestSpringEdge));
					}
					else
					{
						ConstraintsData.SetRestLength(ConstraintIdx, RestSpringEdge.Size());
					}
				}
			}
		}

		TVec3<T> ComputeSpringEdge(const ParticleType& InParticles, const TArray<int32>& LocalIndices,
			const TArray<int32>& LocalSecondIndices, const TArray<T>& Weight, const TArray<T>& SecondWeight, bool bUseParticleX) const
		{
			TVec3<T> SpringEdge((T)0.);
			if (ensure(LocalIndices.Num() == Weight.Num() && LocalSecondIndices.Num() == SecondWeight.Num()))
			{
				if (bUseParticleX)
				{
					for (int32 Idx = 0; Idx < LocalIndices.Num(); ++Idx)
					{
						for (int32 beta = 0; beta < 3; beta++)
						{
							SpringEdge[beta] += Weight[Idx] * InParticles.X(LocalIndices[Idx])[beta];
						}
					}
					for (int32 SecondIdx = 0; SecondIdx < LocalSecondIndices.Num(); ++SecondIdx)
					{
						for (int32 beta = 0; beta < 3; beta++)
						{
							SpringEdge[beta] -= SecondWeight[SecondIdx] * InParticles.X(LocalSecondIndices[SecondIdx])[beta];
						}
					}
				}
				else
				{
					for (int32 Idx = 0; Idx < LocalIndices.Num(); ++Idx)
					{
						for (int32 beta = 0; beta < 3; beta++)
						{
							SpringEdge[beta] += Weight[Idx] * InParticles.P(LocalIndices[Idx])[beta];
						}
					}
					for (int32 SecondIdx = 0; SecondIdx < LocalSecondIndices.Num(); ++SecondIdx)
					{
						for (int32 beta = 0; beta < 3; beta++)
						{
							SpringEdge[beta] -= SecondWeight[SecondIdx] * InParticles.P(LocalSecondIndices[SecondIdx])[beta];
						}
					}
				}
			}
			return SpringEdge;
		}

		void AddWCHessian(const int32 p, const T Dt, Chaos::PMatrix<T, 3, 3>& ParticleHessian) const
		{
			if (NodalWeights[p].Num() > 0)
			{
				for (int32 alpha = 0; alpha < 3; alpha++)
				{
					ParticleHessian.SetAt(alpha, alpha, ParticleHessian.GetAt(alpha, alpha) + Dt * Dt * NodalWeights[p][alpha]);
				}

				ParticleHessian.SetAt(0, 1, ParticleHessian.GetAt(0, 1) + Dt * Dt * NodalWeights[p][3]);
				ParticleHessian.SetAt(0, 2, ParticleHessian.GetAt(0, 2) + Dt * Dt * NodalWeights[p][4]);
				ParticleHessian.SetAt(1, 2, ParticleHessian.GetAt(1, 2) + Dt * Dt * NodalWeights[p][5]);
				ParticleHessian.SetAt(1, 0, ParticleHessian.GetAt(1, 0) + Dt * Dt * NodalWeights[p][3]);
				ParticleHessian.SetAt(2, 0, ParticleHessian.GetAt(2, 0) + Dt * Dt * NodalWeights[p][4]);
				ParticleHessian.SetAt(2, 1, ParticleHessian.GetAt(2, 1) + Dt * Dt * NodalWeights[p][5]);
				//TODO(Yizhou): Clean up the following after debugging:
				//ParticleHessian.SetAt(0, 2) += Dt * Dt * NodalWeights[p][4];
				//ParticleHessian.SetAt(1, 2) += Dt * Dt * NodalWeights[p][5];
				//ParticleHessian.SetAt(1, 0) += Dt * Dt * NodalWeights[p][3];
				//ParticleHessian.SetAt(2, 0) += Dt * Dt * NodalWeights[p][4];
				//ParticleHessian.SetAt(2, 1) += Dt * Dt * NodalWeights[p][5];
			}
		}

		void AddExtraConstraints(const TArray<TArray<int32>>& InIndices,
								const TArray<TArray<T>>& InWeights,
								const TArray<T>& InStiffness,
								const TArray<TArray<int32>>& InSecondIndices,
								const TArray<TArray<T>>& InSecondWeights,
								const TArray<bool>& InIsAnisotrpic,
								const TArray<bool>& InIsZeroRestLength)
		{
			const int32 Offset = ConstraintsData.Size();

			ConstraintsData.AddConstraints(InIndices.Num());

			for (int32 i = 0; i < InIndices.Num(); i++)
			{
				FGaussSeidelWeakConstraintSingleData<T> SingleConstraintData;
				SingleConstraintData.SingleIndices = InIndices[i];
				SingleConstraintData.SingleSecondIndices = InSecondIndices[i];
				SingleConstraintData.SingleWeights = InWeights[i];
				SingleConstraintData.SingleSecondWeights = InSecondWeights[i];
				SingleConstraintData.SingleStiffness = InStiffness[i];
				SingleConstraintData.bIsAnisotropic = InIsAnisotrpic[i];
				SingleConstraintData.bIsZeroRestLength = InIsZeroRestLength[i];
				ConstraintsData.SetSingleConstraint(SingleConstraintData, i + Offset);
			}
		}

		void Resize(int32 Size)
		{
			ConstraintsData.Resize(Size);
		}

		void UpdatePointTriangleCollisionWCData(const FSolverParticles& Particles)
		{	
			TGaussSeidelWeakConstraintData<T> OriginalConstraintsData = ConstraintsData;

			ConstraintsData.Resize(InitialWCSize);

			for (int32 i = InitialWCSize; i < OriginalConstraintsData.Size(); i++)
			{
				const TArray<int32>& IndicesTemp = OriginalConstraintsData.GetIndices(i);
				const TArray<int32>& SecondIndicesTemp = OriginalConstraintsData.GetSecondIndices(i);
				ensureMsgf(OriginalConstraintsData.GetIndices(i).Num() == 3, TEXT("Collision format is not point-triangle"));
				ensureMsgf(OriginalConstraintsData.GetSecondIndices(i).Num() == 1, TEXT("Collision format is not point-triangle"));
				Chaos::TVector<float, 3> TriPos0(Particles.P(IndicesTemp[0])), TriPos1(Particles.P(IndicesTemp[1])), TriPos2(Particles.P(IndicesTemp[2])), ParticlePos(Particles.P(SecondIndicesTemp[0]));
				Chaos::TVector<T, 3> Normal = FVector3f::CrossProduct(TriPos2 - TriPos0, TriPos1 - TriPos0); //triangle normal convention (see FTriangleMesh::GetFaceNormals())
				if (FVector3f::DotProduct(ParticlePos - TriPos0, Normal) < 0.f) //not resolved, keep the spring
				{
					ConstraintsData.AddConstraints(OriginalConstraintsData.GetSingleConstraintData(i));
				}
			}
		}

		void VisualizeAllBindings(const FSolverParticles& InParticles, const T Dt) const
		{
#if WITH_EDITOR
			auto DoubleVert = [](Chaos::TVec3<T> V) { return FVector3d(V.X, V.Y, V.Z); };
			for (int32 i = 0; i < ConstraintsData.Size(); i++)
			{
				const FGaussSeidelWeakConstraintSingleData<T>& SingleConstraintData = ConstraintsData.GetSingleConstraintData(i);
				Chaos::TVec3<T> SourcePos((T)0.), TargetPos((T)0.);
				for (int32 j = 0; j < SingleConstraintData.SingleIndices.Num(); j++)
				{
					SourcePos += SingleConstraintData.SingleWeights[j] * InParticles.P(SingleConstraintData.SingleIndices[j]);
				}
				for (int32 j = 0; j < SingleConstraintData.SingleSecondIndices.Num(); j++)
				{
					TargetPos += SingleConstraintData.SingleSecondWeights[j] * InParticles.P(SingleConstraintData.SingleSecondIndices[j]);
				}

				float ParticleThickness = DebugDrawParams.DebugParticleWidth;
				float LineThickness = DebugDrawParams.DebugLineWidth;

				if (SingleConstraintData.SingleIndices.Num() == 1)
				{
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(SourcePos), FColor::Red, false, Dt, 0, ParticleThickness);
					for (int32 j = 0; j < SingleConstraintData.SingleSecondIndices.Num(); j++)
					{
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(InParticles.P(SingleConstraintData.SingleSecondIndices[j])), FColor::Green, false, Dt, 0, ParticleThickness);
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(DoubleVert(InParticles.P(SingleConstraintData.SingleSecondIndices[j])), DoubleVert(InParticles.P(SingleConstraintData.SingleSecondIndices[(j + 1) % SingleConstraintData.SingleSecondIndices.Num()])), FColor::Green, false, Dt, 0, LineThickness);
					}

				}

				if (SingleConstraintData.SingleSecondIndices.Num() == 1)
				{
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(TargetPos), FColor::Red, false, Dt, 0, ParticleThickness);
					for (int32 j = 0; j < SingleConstraintData.SingleIndices.Num(); j++)
					{
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(InParticles.P(SingleConstraintData.SingleIndices[j])), FColor::Green, false, Dt, 0, ParticleThickness);
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(DoubleVert(InParticles.P(SingleConstraintData.SingleIndices[j])), DoubleVert(InParticles.P(SingleConstraintData.SingleIndices[(j + 1) % SingleConstraintData.SingleIndices.Num()])), FColor::Green, false, Dt, 0, LineThickness);
					}
				}

				Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(DoubleVert(SourcePos), DoubleVert(TargetPos), FColor::Yellow, false, Dt, 0, LineThickness);
			}
#endif
		}

		void Init(const FSolverParticles& InParticles, const T Dt)
		{
			UpdateTriangleNormalAndNodalWeight(InParticles, /*bUseParticleX =*/ false);
			if (DebugDrawParams.bVisualizeBindings)
			{
				VisualizeAllBindings(InParticles, Dt);
			}
		}

		void UpdateTriangleNormalAndNodalWeight(const FSolverParticles& InParticles, bool bUseParticleX)
		{
			for (int32 i = 0; i < ConstraintsData.Size(); i++)
			{
				if (ConstraintsData.GetIsAnisotropic(i))
				{
					const TArray<int32>& IndicesTemp = ConstraintsData.GetIndices(i);
					const TArray<int32>& SecondIndicesTemp = ConstraintsData.GetSecondIndices(i);
					ensureMsgf(ConstraintsData.GetIndices(i).Num() == 3, TEXT("Collision format is not point-triangle"));
					ensureMsgf(ConstraintsData.GetSecondIndices(i).Num() == 1, TEXT("Collision format is not point-triangle"));
					Chaos::TVector<T, 3> Normal;
					if (bUseParticleX)
					{
						Chaos::TVector<float, 3> TriPos0(InParticles.X(IndicesTemp[0])), TriPos1(InParticles.X(IndicesTemp[1])), TriPos2(InParticles.X(IndicesTemp[2]));
						Normal = FVector3f::CrossProduct(TriPos2 - TriPos0, TriPos1 - TriPos0).GetSafeNormal(); //triangle normal convention (see FTriangleMesh::GetFaceNormals())
					}
					else
					{
						Chaos::TVector<float, 3> TriPos0(InParticles.P(IndicesTemp[0])), TriPos1(InParticles.P(IndicesTemp[1])), TriPos2(InParticles.P(IndicesTemp[2]));
						Normal = FVector3f::CrossProduct(TriPos2 - TriPos0, TriPos1 - TriPos0).GetSafeNormal(); //triangle normal convention (see FTriangleMesh::GetFaceNormals())
					}
					ConstraintsData.SetNormal(i, Normal);
				}
			}

			NodalWeights.Init({}, InParticles.Size());
			for (int32 p = 0; p < WCIncidentElements.Num(); ++p)
			{
				if (WCIncidentElements[p].Num() > 0)
				{
					NodalWeights[p].Init(T(0), 6);
					for (int32 j = 0; j < WCIncidentElements[p].Num(); j++)
					{
						int32 ConstraintIndex = WCIncidentElements[p][j];
						int32 LocalIndex = WCIncidentElementsLocal[p][j];

						T Weight = T(0);
						if (LocalIndex >= ConstraintsData.GetIndices(ConstraintIndex).Num())
						{
							Weight = ConstraintsData.GetSecondWeights(ConstraintIndex)[LocalIndex - ConstraintsData.GetIndices(ConstraintIndex).Num()];
						}
						else
						{
							Weight = ConstraintsData.GetWeights(ConstraintIndex)[LocalIndex];
						}

						if (ConstraintsData.GetIsAnisotropic(ConstraintIndex))
						{
							for (int32 alpha = 0; alpha < 3; alpha++)
							{
								NodalWeights[p][alpha] += ConstraintsData.GetNormal(ConstraintIndex)[alpha] * ConstraintsData.GetNormal(ConstraintIndex)[alpha] * Weight * Weight * ConstraintsData.GetStiffness(ConstraintIndex);
							}

							NodalWeights[p][3] += ConstraintsData.GetNormal(ConstraintIndex)[0] * ConstraintsData.GetNormal(ConstraintIndex)[1] * Weight * Weight * ConstraintsData.GetStiffness(ConstraintIndex);
							NodalWeights[p][4] += ConstraintsData.GetNormal(ConstraintIndex)[0] * ConstraintsData.GetNormal(ConstraintIndex)[2] * Weight * Weight * ConstraintsData.GetStiffness(ConstraintIndex);
							NodalWeights[p][5] += ConstraintsData.GetNormal(ConstraintIndex)[1] * ConstraintsData.GetNormal(ConstraintIndex)[2] * Weight * Weight * ConstraintsData.GetStiffness(ConstraintIndex);
						}
						else
						{
							for (int32 alpha = 0; alpha < 3; alpha++)
							{
								NodalWeights[p][alpha] += Weight * Weight * ConstraintsData.GetStiffness(ConstraintIndex);
							}
						}
					}
				}
			}
		}

		//CollisionDetectionSpatialHash should be faster than CollisionDetectionBVH
		void CollisionDetectionBVH(const FSolverParticles& Particles, const TArray<TVec3<int32>>& SurfaceElements, const TArray<int32>& ComponentIndex, float DetectRadius = 1.f, float PositionTargetStiffness = 10000.f, bool UseAnisotropicSpring = true)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelWeakConstraintsCollisionDetection);
			Resize(InitialWCSize);

			TArray<Chaos::TVector<int32, 3>> SurfaceElementsArray;
			for (int32 i = 0; i < SurfaceElements.Num(); i++)
			{
				Chaos::TVector<int32, 3> CurrentSurfaceElements(0);
				for (int32 j = 0; j < 3; j++)
				{
					CurrentSurfaceElements[j] = SurfaceElements[i][j];
				}
				if (CurrentSurfaceElements[0] != INDEX_NONE
					&& CurrentSurfaceElements[1] != INDEX_NONE
					&& CurrentSurfaceElements[2] != INDEX_NONE)
				{
					SurfaceElementsArray.Emplace(CurrentSurfaceElements);
				}
			}
			TArray<TArray<int32>> LocalIndex;
			TArray<TArray<int32>>* LocalIndexPtr = &LocalIndex;
			TArray<TArray<int>> GlobalIndex = Chaos::Utilities::ComputeIncidentElements(SurfaceElementsArray, LocalIndexPtr);
			int32 ActualParticleCount = 0;
			for (int32 l = 0; l < GlobalIndex.Num(); l++)
			{
				if (GlobalIndex[l].Num() > 0)
				{
					ActualParticleCount += 1;
				}
			}
			TArray<Chaos::TVector<float, 3>> SurfaceElementsPositions;
			SurfaceElementsPositions.SetNum(ActualParticleCount);
			TArray<int32> SurfaceElementsMap;
			SurfaceElementsMap.SetNum(ActualParticleCount);
			int32 CurrentParticleIndex = 0;
			for (int32 i = 0; i < GlobalIndex.Num(); i++)
			{
				if (GlobalIndex[i].Num() > 0)
				{
					SurfaceElementsPositions[CurrentParticleIndex] = Particles.P(SurfaceElements[GlobalIndex[i][0]][LocalIndex[i][0]]);
					SurfaceElementsMap[CurrentParticleIndex] = SurfaceElements[GlobalIndex[i][0]][LocalIndex[i][0]];
					CurrentParticleIndex += 1;
				}
			}

			TArray<Chaos::FSphere*> VertexSpherePtrs;
			TArray<Chaos::FSphere> VertexSpheres;

			VertexSpheres.Init(Chaos::FSphere(Chaos::TVec3<Chaos::FReal>(0), DetectRadius), SurfaceElementsPositions.Num());
			VertexSpherePtrs.SetNum(SurfaceElementsPositions.Num());

			for (int32 i = 0; i < SurfaceElementsPositions.Num(); i++)
			{
				Chaos::TVec3<Chaos::FReal> SphereCenter(SurfaceElementsPositions[i]);
				Chaos::FSphere VertexSphere(SphereCenter, DetectRadius);
				VertexSpheres[i] = Chaos::FSphere(SphereCenter, DetectRadius);
				VertexSpherePtrs[i] = &VertexSpheres[i];
			}
			Chaos::TBoundingVolumeHierarchy<
				TArray<Chaos::FSphere*>,
				TArray<int32>,
				Chaos::FReal,
				3> VertexBVH(VertexSpherePtrs);

			for (int32 i = 0; i < SurfaceElements.Num(); i++)
			{
				TArray<int32> TriangleIntersections0 = VertexBVH.FindAllIntersections(Particles.P(SurfaceElements[i][0]));
				TArray<int32> TriangleIntersections1 = VertexBVH.FindAllIntersections(Particles.P(SurfaceElements[i][1]));
				TArray<int32> TriangleIntersections2 = VertexBVH.FindAllIntersections(Particles.P(SurfaceElements[i][2]));
				TriangleIntersections0.Sort();
				TriangleIntersections1.Sort();
				TriangleIntersections2.Sort();

				TArray<int32> TriangleIntersections({});
				for (int32 k = 0; k < TriangleIntersections0.Num(); k++)
				{
					if (TriangleIntersections1.Contains(TriangleIntersections0[k])
						&& TriangleIntersections2.Contains(TriangleIntersections0[k]))
					{
						TriangleIntersections.Emplace(TriangleIntersections0[k]);
					}
				}

				int32 TriangleIndex = ComponentIndex[SurfaceElements[i][0]];
				int32 MinIndex = INDEX_NONE;
				float MinDis = DetectRadius;
				Chaos::TVector<float, 3> ClosestBary(0.f);
				Chaos::TVector<float, 3> FaceNormal;
				for (int32 j = 0; j < TriangleIntersections.Num(); j++)
				{
					if (ComponentIndex[SurfaceElementsMap[TriangleIntersections[j]]] >= 0 && TriangleIndex >= 0 && ComponentIndex[SurfaceElementsMap[TriangleIntersections[j]]] != TriangleIndex)
					{
						Chaos::TVector<float, 3> Bary, TriPos0(Particles.P(SurfaceElements[i][0])), TriPos1(Particles.P(SurfaceElements[i][1])), TriPos2(Particles.P(SurfaceElements[i][2])), ParticlePos(Particles.P(SurfaceElementsMap[TriangleIntersections[j]]));
						Chaos::TVector<Chaos::FRealSingle, 3> ClosestPoint = Chaos::FindClosestPointAndBaryOnTriangle(TriPos0, TriPos1, TriPos2, ParticlePos, Bary);
						Chaos::FRealSingle CurrentDistance = (Particles.P(SurfaceElementsMap[TriangleIntersections[j]]) - ClosestPoint).Size();
						if (CurrentDistance < MinDis)
						{
							Chaos::TVector<T, 3> Normal = FVector3f::CrossProduct(TriPos2 - TriPos0, TriPos1 - TriPos0); //The normal needs to point outwards of the geometry
							if (FVector3f::DotProduct(ParticlePos - TriPos0, Normal) < 0.f)
							{
								Normal.SafeNormalize(1e-8f);
								MinDis = CurrentDistance;
								MinIndex = SurfaceElementsMap[TriangleIntersections[j]];
								ClosestBary = Bary;
								FaceNormal = Normal;
							}
						}

					}
				}
				if (MinIndex != INDEX_NONE
					&& MinIndex != SurfaceElements[i][0]
					&& MinIndex != SurfaceElements[i][1]
					&& MinIndex != SurfaceElements[i][2])
				{
					FGaussSeidelWeakConstraintSingleData<T> SingleConstraintData;
					SingleConstraintData.SingleIndices = {SurfaceElements[i][0], SurfaceElements[i][1], SurfaceElements[i][2]};
					SingleConstraintData.SingleSecondIndices = {MinIndex};
					SingleConstraintData.Weights = {ClosestBary[0], ClosestBary[1], ClosestBary[2]};
					SingleConstraintData.SecondWeights = {T(1.f)};
					SingleConstraintData.bIsAnisotropic = UseAnisotropicSpring;
					SingleConstraintData.SingleNormal = FaceNormal;
					SingleConstraintData.bIsZeroRestLength = true; //Push-out type collision springs should be zero rest length

					float SpringStiffness = 0.f;
					for (int32 k = 0; k < 3; k++)
					{
						SpringStiffness += ClosestBary[k] * PositionTargetStiffness * Particles.M(SurfaceElements[i][k]);
					}
					SpringStiffness += PositionTargetStiffness * Particles.M(MinIndex);
					SingleConstraintData.SingleStiffness = (T)SpringStiffness;
					ConstraintsData.AddSingleConstraint(SingleConstraintData);
				}
			}
		}

		template<typename SpatialAccelerator>
		void CollisionDetectionSpatialHash(const FSolverParticles& Particles, const TArray<int32>& SurfaceVertices, const FTriangleMesh& TriangleMesh, const TArray<int32>& ComponentIndex, const SpatialAccelerator& Spatial, float DetectRadius = 1.f, float PositionTargetStiffness = 10000.f, bool UseAnisotropicSpring = true)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelWeakConstraintsCollisionDetectionSpatialHash);
			Resize(InitialWCSize + Particles.Size());
			std::atomic<int32> ConstraintIndex(InitialWCSize);
			const TArray<TVec3<int32>>& Elements = TriangleMesh.GetSurfaceElements();
			const float HalfRadius = DetectRadius / 2;
			PhysicsParallelFor(SurfaceVertices.Num(),
				[this, &Spatial, &Particles, &SurfaceVertices, &ConstraintIndex, &TriangleMesh, &Elements, &HalfRadius, &ComponentIndex, &PositionTargetStiffness, &UseAnisotropicSpring](int32 i)
				{
					const int32 Index = SurfaceVertices[i];
					TArray< TTriangleCollisionPoint<FSolverReal>> Result;
					//PointProximityQuery
					if (TriangleMesh.PointClosestTriangleQuery(Spatial, static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), Index, Particles.GetX(Index), HalfRadius, HalfRadius,
						[this, &ComponentIndex, &Elements](const int32 PointIndex, const int32 TriangleIndex)->bool
						{
							//Skip particles that are bound in initial springs
							return ComponentIndex[PointIndex] != ComponentIndex[Elements[TriangleIndex][0]] && (!NoCollisionWCIncidentElements.IsValidIndex(PointIndex) || NoCollisionWCIncidentElements[PointIndex].Num() == 0);
						},
						Result))
					{
						for (const TTriangleCollisionPoint<FSolverReal>& CollisionPoint : Result)
						{
							if (CollisionPoint.Phi < 0)
							{
								const TVector<int32, 3>& Elem = Elements[CollisionPoint.Indices[1]];
								const int32 IndexToWrite = ConstraintIndex.fetch_add(1);

								FGaussSeidelWeakConstraintSingleData<T> SingleConstraintData;
								SingleConstraintData.SingleIndices = { Elem[0], Elem[1] ,Elem[2] };
								SingleConstraintData.SingleSecondIndices =  { Index };
								SingleConstraintData.SingleWeights = { CollisionPoint.Bary[1], CollisionPoint.Bary[2], CollisionPoint.Bary[3] };
								SingleConstraintData.SingleSecondWeights = {T(1.f)};
								SingleConstraintData.bIsAnisotropic = UseAnisotropicSpring;
								SingleConstraintData.SingleNormal = CollisionPoint.Normal;
								SingleConstraintData.bIsZeroRestLength = true; //Push-out type collision springs should be zero rest length

								float SpringStiffness = 0.f;
								for (int32 k = 0; k < 3; k++)
								{
									SpringStiffness += SingleConstraintData.SingleWeights[k] * PositionTargetStiffness * Particles.M(Elem[k]);
								}
								SpringStiffness += PositionTargetStiffness * Particles.M(Index);
								SingleConstraintData.SingleStiffness = (T)SpringStiffness;
								ConstraintsData.SetSingleConstraint(SingleConstraintData, IndexToWrite);
							}
						}
					}
				}
			);

			// Shrink the arrays to the actual number of found constraints.
			const int32 ConstraintNum = ConstraintIndex.load();
			Resize(ConstraintNum);
		}

		template<typename SpatialAccelerator>
		void CollisionDetectionSpatialHashInComponent(const FSolverParticles& Particles, const TArray<int32>& SurfaceVertices, const FTriangleMesh& TriangleMesh, const TMap<int32, TSet<int32>>& ExcludeMap, const SpatialAccelerator& Spatial, float DetectRadius = 0.f, float PositionTargetStiffness = 10000.f, bool UseAnisotropicSpring = true)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelWeakConstraintsCollisionDetectionSpatialHashInComponent);
			Resize(InitialWCSize + Particles.Size());
			std::atomic<int32> ConstraintIndex(InitialWCSize);
			const TArray<TVec3<int32>>& Elements = TriangleMesh.GetSurfaceElements();
			const float HalfRadius = DetectRadius/2;
			PhysicsParallelFor(SurfaceVertices.Num(),
				[this, &Spatial, &Particles, &SurfaceVertices, &ConstraintIndex, &TriangleMesh, &ExcludeMap, &Elements, &HalfRadius, &PositionTargetStiffness, &UseAnisotropicSpring](int32 i)
				{
					const int32 Index = SurfaceVertices[i];
					TArray< TTriangleCollisionPoint<FSolverReal>> Result;
					//PointProximityQuery
					if (TriangleMesh.PointClosestTriangleQuery(Spatial, static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), Index, Particles.GetX(Index), HalfRadius, HalfRadius,
						[this, &Elements, &ExcludeMap](const int32 PointIndex, const int32 TriangleIndex)->bool
						{	
							return  !(ExcludeMap.Find(PointIndex) && ExcludeMap[PointIndex].Contains(TriangleIndex));
						},
						Result))
					{
						for (const TTriangleCollisionPoint<FSolverReal>& CollisionPoint : Result)
						{
							if (CollisionPoint.Phi < 0)
							{
								const TVector<int32, 3>& Elem = Elements[CollisionPoint.Indices[1]];
								const int32 IndexToWrite = ConstraintIndex.fetch_add(1);

								FGaussSeidelWeakConstraintSingleData<T> SingleConstraintData;
								SingleConstraintData.SingleIndices = { Elem[0], Elem[1] ,Elem[2] };
								SingleConstraintData.SingleSecondIndices =  { Index };
								SingleConstraintData.SingleWeights = { CollisionPoint.Bary[1], CollisionPoint.Bary[2], CollisionPoint.Bary[3] };
								SingleConstraintData.SingleSecondWeights = {T(1.f)};
								SingleConstraintData.bIsAnisotropic = UseAnisotropicSpring;
								SingleConstraintData.SingleNormal = CollisionPoint.Normal;
								SingleConstraintData.bIsZeroRestLength = true; //Push-out type collision springs should be zero rest length

								float SpringStiffness = 0.f;
								for (int32 k = 0; k < 3; k++)
								{
									SpringStiffness += SingleConstraintData.SingleWeights[k] * PositionTargetStiffness * Particles.M(Elem[k]);
								}
								SpringStiffness += PositionTargetStiffness * Particles.M(Index);
								SingleConstraintData.SingleStiffness = (T)SpringStiffness;
								ConstraintsData.SetSingleConstraint(SingleConstraintData, IndexToWrite);
							}
						}
					}
				}
			);

			// Shrink the arrays to the actual number of found constraints.
			const int32 ConstraintNum = ConstraintIndex.load();
			Resize(ConstraintNum);
		}

		void ComputeCollisionWCDataSimplified(TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraWCIncidentElements, TArray<TArray<int32>>& ExtraWCIncidentElementsLocal)
		{
			ensureMsgf(ConstraintsData.Size() >= InitialWCSize, TEXT("The size of Indices is smaller than InitialWCSize"));

			ExtraConstraints.Init(TArray<int32>(), ConstraintsData.Size() - InitialWCSize);
			for (int32 i = InitialWCSize; i < static_cast<int32>(ConstraintsData.Size()); i++)
			{
				const TArray<int32>& LocalIndices = ConstraintsData.GetIndices(i);
				const TArray<int32>& LocalSecondIndices = ConstraintsData.GetSecondIndices(i);

				ExtraConstraints[i - InitialWCSize].SetNum(LocalIndices.Num() + LocalSecondIndices.Num());
				for (int32 j = 0; j < LocalIndices.Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j] = LocalIndices[j];
				}
				for (int32 j = 0; j < LocalSecondIndices.Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j + LocalIndices.Num()] = LocalSecondIndices[j];
				}
			}

			ExtraWCIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraWCIncidentElementsLocal);

			NodalWeights = NoCollisionNodalWeights;
			for (int32 i = 0; i < ExtraWCIncidentElements.Num(); i++)
			{
				if (ExtraWCIncidentElements[i].Num() > 0)
				{
					int32 p = ExtraConstraints[ExtraWCIncidentElements[i][0]][ExtraWCIncidentElementsLocal[i][0]];
					if (NodalWeights[p].Num() == 0)
					{
						NodalWeights[p].Init(T(0), 6);
					}
					for (int32 j = 0; j < ExtraWCIncidentElements[i].Num(); j++)
					{
						int32 LocalIndex = ExtraWCIncidentElementsLocal[i][j];
						int32 ConstraintIndex = ExtraWCIncidentElements[i][j] + InitialWCSize;

						const FGaussSeidelWeakConstraintSingleData<T>& SingleData = ConstraintsData.GetSingleConstraintData(ConstraintIndex);

						T weight = T(0);
						if (LocalIndex >= SingleData.SingleIndices.Num())
						{
							weight = SingleData.SingleWeights[LocalIndex - SingleData.SingleIndices.Num()];
						}
						else
						{
							weight = SingleData.SingleWeights[LocalIndex];
						}
						if (SingleData.bIsAnisotropic)
						{
							for (int32 alpha = 0; alpha < 3; alpha++)
							{
								NodalWeights[p][alpha] += SingleData.SingleNormal[alpha] * SingleData.SingleNormal[alpha] * weight * weight * SingleData.SingleStiffness;
							}

							NodalWeights[p][3] += SingleData.SingleNormal[0] * SingleData.SingleNormal[1] * weight * weight * SingleData.SingleStiffness;
							NodalWeights[p][4] += SingleData.SingleNormal[0] * SingleData.SingleNormal[2] * weight * weight * SingleData.SingleStiffness;
							NodalWeights[p][5] += SingleData.SingleNormal[1] * SingleData.SingleNormal[2] * weight * weight * SingleData.SingleStiffness;
						}
						else
						{
							for (int32 alpha = 0; alpha < 3; alpha++)
							{
								NodalWeights[p][alpha] += weight * weight * SingleData.SingleStiffness;
							}
						}
					}
				}
			}
		}


		const TArray<TArray<int32>>& GetStaticConstraintArrays(TArray<TArray<int32>>& IncidentElements, TArray<TArray<int32>>& IncidentElementsLocal) const
		{
			IncidentElements = NoCollisionWCIncidentElements;
			IncidentElementsLocal = NoCollisionWCIncidentElementsLocal;
			return NoCollisionConstraints;
		}

		TArray<TArray<int32>> GetDynamicConstraintArrays(TArray<TArray<int32>>& IncidentElements, TArray<TArray<int32>>& IncidentElementsLocal) const
		{
			TArray<TArray<int32>> ExtraConstraints;
			ExtraConstraints.Init(TArray<int32>(), ConstraintsData.Size());
			for (int32 i = InitialWCSize; i < ConstraintsData.Size(); i++)
			{
				const TArray<int32>& LocalIndices = ConstraintsData.GetIndices(i);
				const TArray<int32>& LocalSecondIndices = ConstraintsData.GetSecondIndices(i);
				ExtraConstraints[i - InitialWCSize].SetNum(LocalIndices.Num() + LocalSecondIndices.Num());
				for (int32 j = 0; j < LocalIndices.Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j] = LocalIndices[j];
				}
				for (int32 j = 0; j < LocalSecondIndices.Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j + LocalIndices.Num()] = LocalSecondIndices[j];
				}
			}

			IncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &IncidentElementsLocal);

			return ExtraConstraints;
		}

		// Deprecated, now replaced with AddWCResidual for more general cases
		void AddZeroRestLengthWCResidualAndHessian(const ParticleType& InParticles, const int32 ConstraintIndex, const int32 LocalIndex, const T Dt, TVec3<T>& ParticleResidual, Chaos::PMatrix<T, 3, 3>& ParticleHessian) const
		{
			const FGaussSeidelWeakConstraintSingleData<T>& SingleData = ConstraintsData.GetSingleConstraintData(ConstraintIndex);

			const TVec3<T> SpringEdge = ComputeSpringEdge(InParticles, SingleData.SingleIndices, SingleData.SingleSecondIndices,
				SingleData.SingleWeights, SingleData.SingleSecondWeights, /*bUseParticleX =*/false);
			T weight = T(0);
			if (LocalIndex >= SingleData.SingleIndices.Num())
			{
				weight = -SingleData.SingleSecondWeights[LocalIndex - SingleData.SingleIndices.Num()];
			}
			else
			{
				weight = SingleData.SingleWeights[LocalIndex];
			}
			if (SingleData.bIsAnisotropic)
			{
				T comp = TVec3<T>::DotProduct(SpringEdge, SingleData.SingleNormal);
				TVec3<T> proj = SingleData.SingleNormal * comp;
				for (int32 alpha = 0; alpha < 3; alpha++)
				{
					ParticleResidual[alpha] += Dt * Dt * SingleData.SingleStiffness * proj[alpha] * weight;
				}
			}
			else
			{
				for (int32 alpha = 0; alpha < 3; alpha++)
				{
					ParticleResidual[alpha] += Dt * Dt * SingleData.SingleStiffness * SpringEdge[alpha] * weight;
				}
			}
		}

		void AddWCResidual(const ParticleType& InParticles, const int32 ConstraintIndex, const int32 LocalIndex, const T Dt, TVec3<T>& ParticleResidual, Chaos::PMatrix<T, 3, 3>& ParticleHessian) const
		{
			const FGaussSeidelWeakConstraintSingleData<T>& SingleData = ConstraintsData.GetSingleConstraintData(ConstraintIndex);

			const TVec3<T> SpringEdge = ComputeSpringEdge(InParticles, SingleData.SingleIndices, SingleData.SingleSecondIndices,
				SingleData.SingleWeights, SingleData.SingleSecondWeights, /*bUseParticleX =*/false);
			T weight = T(0);
			if (LocalIndex >= SingleData.SingleIndices.Num())
			{
				weight = -SingleData.SingleSecondWeights[LocalIndex - SingleData.SingleIndices.Num()];
			}
			else
			{
				weight = SingleData.SingleWeights[LocalIndex];
			}
			TVec3<T> Projection;
			if (SingleData.bIsAnisotropic)
			{
				T LengthDiff = TVec3<T>::DotProduct(SpringEdge, SingleData.SingleNormal) - SingleData.RestLength;
				Projection = SingleData.SingleNormal * LengthDiff;
			}
			else
			{
				Projection = SpringEdge;
				if (!SingleData.bIsZeroRestLength) // if not zero rest-length, apply repulsion force
				{
					Projection -= SingleData.RestLength * SpringEdge.GetSafeNormal();
				}	
			}
			for (int32 alpha = 0; alpha < 3; ++alpha)
			{
				ParticleResidual[alpha] += Dt * Dt * SingleData.SingleStiffness * Projection[alpha] * weight;
			}
		}

		TGaussSeidelWeakConstraintData<T> ConstraintsData;

		TArray<TArray<T>> NodalWeights;

		TArray<TArray<int32>> WCIncidentElements;
		TArray<TArray<int32>> WCIncidentElementsLocal;

		FDeformableXPBDWeakConstraintParams DebugDrawParams;

		int32 InitialWCSize;
		TArray<TArray<T>> NoCollisionNodalWeights;
		TArray<TArray<int32>> NoCollisionConstraints;
		TArray<TArray<int32>> NoCollisionWCIncidentElements;
		TArray<TArray<int32>> NoCollisionWCIncidentElementsLocal;
	};


}// End namespace Chaos::Softs

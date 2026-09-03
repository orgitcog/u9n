// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Utilities.h"

namespace Chaos::Softs
{
	template <typename T, typename ParticleType>
	class FGaussSeidelUnilateralTetConstraints
	{
	public: 
		FGaussSeidelUnilateralTetConstraints(
			const ParticleType& Particles,
			TArray<TVector<int32, 4>>&& InConstraints,
			TArray<T> InStiffnessArray)
			: Constraints(InConstraints), 
			StiffnessArray(InStiffnessArray)
		{
			IncidentElements = Chaos::Utilities::ComputeIncidentElements(Constraints, &IncidentElementsLocal);
			Volumes.SetNumUninitialized(Constraints.Num());

			for (int32 Idx = 0; Idx < Constraints.Num(); ++Idx)
			{
				const TVec4<int32>& Constraint = Constraints[Idx];
				Volumes[Idx] = ComputeVolume(
					Particles.X(Constraint[0]),
					Particles.X(Constraint[1]),
					Particles.X(Constraint[2]),
					Particles.X(Constraint[3]));
				StiffnessArray[Idx] /= Volumes[Idx];
			}
		};

		static T ComputeVolume(const TVec3<T>& P1, const TVec3<T>& P2, const TVec3<T>& P3, const TVec3<T>& P4)
		{
			const TVec3<T> P2P1 = P2 - P1;
			const TVec3<T> P4P1 = P4 - P1;
			const TVec3<T> P3P1 = P3 - P1;
			return TVec3<T>::DotProduct(TVec3<T>::CrossProduct(P2P1, P3P1), P4P1) / (T)6.;
		}

		void AddEnergy(const ParticleType& InParticles, const int32 ConstraintIndex, const T Dt, T& Energy) const
		{
			const TVec4<int32>& Constraint = Constraints[ConstraintIndex];
			T CurrentVol = ComputeVolume(
				InParticles.P(Constraint[0]),
				InParticles.P(Constraint[1]),
				InParticles.P(Constraint[2]),
				InParticles.P(Constraint[3]));
			Energy += Dt * Dt * StiffnessArray[ConstraintIndex] / (T)2. * (CurrentVol - Volumes[ConstraintIndex]) * (CurrentVol - Volumes[ConstraintIndex]);
		}

		void AddResidualAndHessian(const ParticleType& InParticles, const int32 ConstraintIndex, const int32 LocalIndex, const T Dt, TVec3<T>& ParticleResidual, Chaos::PMatrix<T, 3, 3>& ParticleHessian) const
		{
			const TVec4<int32>& Constraint = Constraints[ConstraintIndex];
			const int32 Index1 = Constraint[0];
			const int32 Index2 = Constraint[1];
			const int32 Index3 = Constraint[2];
			const int32 Index4 = Constraint[3];
			
			const TVec3<T>& P1 = InParticles.P(Index1);
			const TVec3<T>& P2 = InParticles.P(Index2);
			const TVec3<T>& P3 = InParticles.P(Index3);
			const TVec3<T>& P4 = InParticles.P(Index4);

			TVec4<TVec3<T>> Grads;
			const TVec3<T> P2P1 = P2 - P1;
			const TVec3<T> P4P1 = P4 - P1;
			const TVec3<T> P3P1 = P3 - P1;
			Grads[1] = TVec3<T>::CrossProduct(P3P1, P4P1) / (T)6.;
			Grads[2] = TVec3<T>::CrossProduct(P4P1, P2P1) / (T)6.;
			Grads[3] = TVec3<T>::CrossProduct(P2P1, P3P1) / (T)6.;
			Grads[0] = -(Grads[1] + Grads[2] + Grads[3]);

			const T Volume = ComputeVolume(P1, P2, P3, P4);
			const T C_Hessian = StiffnessArray[ConstraintIndex] * Dt * Dt;
			const T C_Residual = (Volume - Volumes[ConstraintIndex]) * C_Hessian;
			ParticleResidual += C_Residual * Grads[LocalIndex];
			ParticleHessian += C_Hessian * Chaos::PMatrix<T, 3, 3>::OuterProduct(Grads[LocalIndex], Grads[LocalIndex]);
		}

		TArray<TArray<int32>> GetStaticConstraintArrays(TArray<TArray<int32>>& InIncidentElements, TArray<TArray<int32>>& InIncidentElementsLocal) const
		{
			InIncidentElements = IncidentElements;
			InIncidentElementsLocal = IncidentElementsLocal;
			TArray<TArray<int32>> NestedConstraints;
			NestedConstraints.SetNum(Constraints.Num());
			for (int32 Idx = 0; Idx < NestedConstraints.Num(); ++Idx)
			{
				NestedConstraints[Idx] = {Constraints[Idx][0], Constraints[Idx][1], Constraints[Idx][2], Constraints[Idx][3]};
			}
			return NestedConstraints;
		}

		int32 NumConstraints() const
		{
			return Constraints.Num();
		}

	private:
		TArray<TVector<int32, 4>> Constraints;
		TArray<T> Volumes;
		TArray<TArray<int32>> IncidentElements;
		TArray<TArray<int32>> IncidentElementsLocal;
		TArray<T> StiffnessArray;
	};


}// End namespace Chaos::Softs

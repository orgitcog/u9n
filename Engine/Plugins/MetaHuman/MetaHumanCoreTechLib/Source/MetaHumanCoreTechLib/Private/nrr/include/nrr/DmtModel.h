// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/utils/TaskThreadPool.h>
#include <nrr/PatchBlendModel.h>

#include <memory>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class DmtModel
{
	struct Data;
public:
	struct SolveOptions
	{
		bool symmetric{ true };
		float pcaThreshold{ T(3) };
		bool markerCompensate{ false };
	};

public:
	DmtModel(std::shared_ptr<const PatchBlendModel<T>> patchBlendModel,
		const std::vector<int>& patchModelSymmetries,
		const std::shared_ptr<TaskThreadPool>& taskThreadPool);

	std::shared_ptr<DmtModel> Clone() const;

	void Init(const std::vector<int>& vertexIndices, int vertexIndexOffset, bool singleRegionPerLandmark, T regularizationWeight);

	//! @solve for delta pca parameters based on landmark delta. The landmark delta should be in model space (not canonical)
	void ForwardDmtDelta(PatchBlendModel<T>::State& state, int landmarkIndex, const Eigen::Vector3<T>& delta, bool symmetric, float pcaThreshold) const;

	//! @solve for delta pca parameters based on landmark delta. The landmark delta should be in model space (not canonical)
	void ForwardDmtDelta(PatchBlendModel<T>::State& state, int landmarkIndex, const Eigen::Vector3<T>& delta, const SolveOptions& solveOptions) const;

	//! @returns the regularization weight that we used during Init
	float GetRegularizationWeight() const { return m_regularizationWeight; }

private:
	//! Create forward solve matrices i.e. min || A * x - markers || + reg || x ||
	void CreateForwardSolveMatrices(std::shared_ptr<Data> modelData, T regularizationWeight);

private:
	//! patch blend model for the entire head (joints + assets)
	std::shared_ptr<const PatchBlendModel<T>> m_patchBlendModel;

	//! symmetries for the entrie head (joints + assets)
	std::shared_ptr<const std::vector<int>> m_patchModelSymmetries;

	//! the linear dmt model
	std::shared_ptr<const Data> m_modelData;

	//! thread pool for parallelization
	std::shared_ptr<TaskThreadPool> m_taskThreadPool;

	//! regularization weight that was used
	T m_regularizationWeight{};
};

template <class T>
struct DmtModel<T>::Data
{
	//! number of pca parameters
	int numParameters;

	//! mean shape of the landmarks
	Eigen::Matrix<T, 3, -1> base;

	//! The indices in patchBlendModel the marker correspond to
	std::vector<int> vertexIndices;

	//! symmetry mapping with indices into @see vertexIndices
	std::vector<int> symmetries;

	//! for each region which markers that are being used (index into @p base)
	std::vector<std::vector<int>> regionMarkerIds;

	//! for each region marker, to which region vertex it maps
	std::vector<std::vector<int>> regionVertexIds;

	//! forward dmt solve matrices (solves from marker(deltas) to pca coefficients
	std::vector<Eigen::MatrixX<T>> forwardSolveMatrices;

	//! whether landmark index @p idx has a symmetric mapping
	bool HasSymmetry(int idx) const { return idx >= 0 && idx < (int)symmetries.size() && symmetries[idx] >= 0; }

	//! @returns the symmetric landmark index of @p idx
	int GetSymmetricIndex(int idx) const { return (idx >= 0 && idx < (int)symmetries.size()) ? symmetries[idx] : -1; }

	//! @returns True if landmark index @p idx is self-symmetric
	bool IsSelfSymmetric(int idx) const { return (GetSymmetricIndex(idx) == idx); }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

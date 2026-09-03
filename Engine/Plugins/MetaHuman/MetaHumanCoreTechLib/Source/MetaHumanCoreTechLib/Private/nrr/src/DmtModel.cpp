// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/DmtModel.h>
#include <carbon/Algorithm.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

static std::vector<int> GetSymmetricIndices(const std::vector<int>& vertexIndices, const std::vector<int>& symmetry)
{
	std::vector<int> out(vertexIndices.size(), -1);
	if (symmetry.size() > 0)
	{
		for (size_t j = 0; j < vertexIndices.size(); ++j)
		{
			const int vID = vertexIndices[j];
			if (vID < 0) continue;
			const int sym_vID = symmetry[vID];
			if (sym_vID == vID)
			{
				out[j] = (int)j;
			}
			else
			{
				const int sym_j = GetItemIndex(vertexIndices, sym_vID);
				out[j] = sym_j;
				if (sym_j < 0)
				{
					CARBON_CRITICAL("symmetries are defined but landmarks are not symmetric: {} vs {}", vID, sym_vID);
				}
			}
		}
	}
	return out;
}

template <class T>
DmtModel<T>::DmtModel(std::shared_ptr<const PatchBlendModel<T>> patchBlendModel,
	const std::vector<int>& patchModelSymmetries,
	const std::shared_ptr<TaskThreadPool>& taskThreadPool)
	: m_patchBlendModel(patchBlendModel)
	, m_modelData()
	, m_taskThreadPool(taskThreadPool)
{
	if ((int)patchModelSymmetries.size() == patchBlendModel->NumVertices())
	{
		m_patchModelSymmetries = std::make_shared<std::vector<int>>(patchModelSymmetries);
	}
}

template <class T>
std::shared_ptr<DmtModel<T>> DmtModel<T>::Clone() const
{
	return std::make_shared<DmtModel>(*this);
}

template <class T>
void DmtModel<T>::Init(const std::vector<int>& vertexIndices, int vertexIndexOffset, bool singleRegionPerLandmark, T regularizationWeight)
{
	auto modelData = std::make_shared<Data>();
	const int numRegions = m_patchBlendModel->NumPatches();

	// prepare forward dmt
	modelData->numParameters = 0;
	for (int regionIndex = 0; regionIndex < numRegions; ++regionIndex)
	{
		modelData->numParameters += m_patchBlendModel->NumPcaModesForPatch(regionIndex);
	}
	const Eigen::Matrix<T, 3, -1>& baseVertices = m_patchBlendModel->BaseVertices();
	modelData->base.resize(3, (int)vertexIndices.size());
	modelData->vertexIndices.resize(vertexIndices.size());

	modelData->regionMarkerIds.clear();
	modelData->regionMarkerIds.resize(numRegions);
	modelData->regionVertexIds.clear();
	modelData->regionVertexIds.resize(numRegions);

	for (int idx = 0; idx < (int)vertexIndices.size(); ++idx)
	{
		const int vID = vertexIndices[idx] + vertexIndexOffset;
		modelData->base.col(idx) = baseVertices.col(vID);
		modelData->vertexIndices[idx] = vID;
		if (singleRegionPerLandmark)
		{
			int bestK = 0;
			for (int k = 0; k < (int)m_patchBlendModel->BlendMatrix()[vID].size(); ++k)
			{
				if (std::get<2>(m_patchBlendModel->BlendMatrix()[vID][k]) > std::get<2>(m_patchBlendModel->BlendMatrix()[vID][bestK]))
				{
					bestK = k;
				}
			}
			// only use landmark with highest weight
			{
				const auto& [regionIndex, region_vID, _] = m_patchBlendModel->BlendMatrix()[vID][bestK];
				modelData->regionMarkerIds[regionIndex].push_back(idx);
				modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 0);
				modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 1);
				modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 2);
			}

			// also add regions that have exactly the same weight
			for (int k = 0; k < (int)m_patchBlendModel->BlendMatrix()[vID].size(); ++k)
			{
				if (k != bestK && std::get<2>(m_patchBlendModel->BlendMatrix()[vID][k]) == std::get<2>(m_patchBlendModel->BlendMatrix()[vID][bestK]))
				{
					const auto& [regionIndex, region_vID, _] = m_patchBlendModel->BlendMatrix()[vID][k];
					modelData->regionMarkerIds[regionIndex].push_back(idx);
					modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 0);
					modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 1);
					modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 2);
				}
			}
		}
		else
		{
			for (const auto& [regionIndex, region_vID, _] : m_patchBlendModel->BlendMatrix()[vID])
			{
				modelData->regionMarkerIds[regionIndex].push_back(idx);
				modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 0);
				modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 1);
				modelData->regionVertexIds[regionIndex].push_back(3 * region_vID + 2);
			}
		}
	}

	modelData->forwardSolveMatrices.clear();
	modelData->forwardSolveMatrices.resize(numRegions);

	CreateForwardSolveMatrices(modelData, regularizationWeight);

	if (m_patchModelSymmetries)
	{
		// get symmetry between landmarks
		modelData->symmetries = GetSymmetricIndices(modelData->vertexIndices, *m_patchModelSymmetries);
	}

	m_modelData = modelData;
}

template <class T>
void DmtModel<T>::CreateForwardSolveMatrices(std::shared_ptr<Data> modelData, T regularizationWeight)
{
	m_regularizationWeight = regularizationWeight;
	auto calculateForwardSolveMatrix = [&](int start, int end) {
		for (int regionIndex = start; regionIndex < end; ++regionIndex)
		{
			const Eigen::MatrixX<T> A = m_patchBlendModel->PatchModels()[regionIndex].Modes(rt::LinearVertexModel<T>::EvaluationMode::STATIC)(modelData->regionVertexIds[regionIndex], Eigen::all);
			const Eigen::Matrix<T, -1, -1> E = Eigen::Matrix<T, -1, -1>::Identity(A.cols(), A.cols());
			modelData->forwardSolveMatrices[regionIndex] = (A.transpose() * A + regularizationWeight * E).inverse() * A.transpose();
		}
		};
	if (m_taskThreadPool) m_taskThreadPool->AddTaskRangeAndWait(m_patchBlendModel->NumPatches(), calculateForwardSolveMatrix);
	else calculateForwardSolveMatrix(0, m_patchBlendModel->NumPatches());
}

#ifdef _MSC_VER
__pragma(warning(push))
__pragma(warning(disable:4324))
// see explanation of warning: https://eigen.tuxfamily.org/dox-devel/group__TopicStructHavingEigenMembers.html#StructHavingEigenMembers_othersolutions
// this should not be an issue due to use of the EIGEN_MAKE_ALIGNED_OPERATOR_NEW macro
#endif
template <class T>
void DmtModel<T>::ForwardDmtDelta(PatchBlendModel<T>::State& state,
	int landmarkIndex,
	const Eigen::Vector3<T>& delta,
	bool symmetric,
	float pcaThreshold) const
{
	SolveOptions solveOptions;
	solveOptions.symmetric = symmetric;
	solveOptions.pcaThreshold = pcaThreshold;
	ForwardDmtDelta(state, landmarkIndex, delta, solveOptions);
}

template <class T>
void DmtModel<T>::ForwardDmtDelta(PatchBlendModel<T>::State& state,
	int landmarkIndex,
	const Eigen::Vector3<T>& delta,
	const SolveOptions& solveOptions) const
{
	if (landmarkIndex < 0 || landmarkIndex >= (int)m_modelData->vertexIndices.size()) return;

	std::vector<bool> markerMoved(m_modelData->vertexIndices.size(), false);
	markerMoved[landmarkIndex] = true;
	const int symmetricLandmarkIndex = solveOptions.symmetric ? m_modelData->GetSymmetricIndex(landmarkIndex) : -1;
	if (symmetricLandmarkIndex >= 0) markerMoved[symmetricLandmarkIndex] = true;

	for (int regionIndex = 0; regionIndex < (int)m_modelData->forwardSolveMatrices.size(); ++regionIndex)
	{
		// check if region is being modified
		bool regionModified = false;
		for (int k = 0; k < (int)m_modelData->regionMarkerIds[regionIndex].size(); ++k)
		{
			regionModified |= markerMoved[m_modelData->regionMarkerIds[regionIndex][k]];
		}

		if (regionModified)
		{
			Eigen::Matrix<T, 3, -1> regionMarkerDeltas = Eigen::Matrix<T, 3, -1>::Zero(3, m_modelData->regionMarkerIds[regionIndex].size());
			for (int k = 0; k < (int)m_modelData->regionMarkerIds[regionIndex].size(); ++k)
			{
				if (m_modelData->regionMarkerIds[regionIndex][k] == landmarkIndex)
				{
					const auto [rotation, scale] = m_patchBlendModel->EstimateRotationAndScale(m_modelData->vertexIndices[landmarkIndex], state);
					Eigen::Vector3f transformCompensatedDelta = rotation.inverse().toRotationMatrix() * delta * scale;
					if (symmetricLandmarkIndex == landmarkIndex)
					{
						transformCompensatedDelta[0] = 0;
					}
					regionMarkerDeltas.col(k) = transformCompensatedDelta;
				}
				else if (m_modelData->regionMarkerIds[regionIndex][k] == symmetricLandmarkIndex)
				{

					const auto [rotation, scale] = m_patchBlendModel->EstimateRotationAndScale(m_modelData->vertexIndices[symmetricLandmarkIndex], state);
					Eigen::Vector3f symmetricDelta(-delta[0], delta[1], delta[2]);
					Eigen::Vector3f transformCompensatedDelta = rotation.inverse().toRotationMatrix() * symmetricDelta * scale;
					regionMarkerDeltas.col(k) = transformCompensatedDelta;
				}
			}

			Eigen::VectorXf perParameterThreshold = state.PatchPcaWeights(regionIndex);
			perParameterThreshold = perParameterThreshold.array().abs().max(solveOptions.pcaThreshold);
			Eigen::VectorX<T> coeffsDelta = m_modelData->forwardSolveMatrices[regionIndex] * regionMarkerDeltas.reshaped();
			Eigen::VectorX<T> newCoeffs = coeffsDelta + state.PatchPcaWeights(regionIndex);
			if (solveOptions.markerCompensate)
			{
				// compensate for landmarks of the region by moving in the inverse direction
				const Eigen::MatrixX<T> A = m_patchBlendModel->PatchModels()[regionIndex].Modes(rt::LinearVertexModel<T>::EvaluationMode::STATIC)(m_modelData->regionVertexIds[regionIndex], Eigen::all);
				Eigen::VectorX<T> resultingRegionMarkerDeltas = A * coeffsDelta;
				for (int k = 0; k < (int)m_modelData->regionMarkerIds[regionIndex].size(); ++k)
				{
					if (m_modelData->regionMarkerIds[regionIndex][k] == landmarkIndex)
					{
					}
					else if (m_modelData->regionMarkerIds[regionIndex][k] == symmetricLandmarkIndex)
					{
					}
					else
					{
						regionMarkerDeltas.col(k) = -resultingRegionMarkerDeltas.segment(3 * k, 3);
					}
				}
				coeffsDelta = m_modelData->forwardSolveMatrices[regionIndex] * regionMarkerDeltas.reshaped();
				newCoeffs = coeffsDelta + state.PatchPcaWeights(regionIndex);
			}
			newCoeffs = newCoeffs.array().min(perParameterThreshold.array()).max(-perParameterThreshold.array());
			state.SetPatchPcaWeights(regionIndex, newCoeffs);
		}
	}
}
#ifdef _MSC_VER
__pragma(warning(pop))
#endif


// explicitly instantiate the DmtModel classes
template class DmtModel<float>;
// template class DmtModel<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

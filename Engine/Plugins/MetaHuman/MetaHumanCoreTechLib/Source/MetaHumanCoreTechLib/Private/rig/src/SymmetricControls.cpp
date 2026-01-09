// Copyright Epic Games, Inc. All Rights Reserved.

#include "rig/SymmetricControls.h"

#include <nls/DiffScalar.h>
#include <nls/VectorVariable.h>

#include <set>
#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct SymmetricControls<T>::Private
{
    // mapping from for symmetric gui controls based on r_ and l_ names
    std::vector<Eigen::Vector2i> solveToGuiControlMapping;
    Eigen::SparseMatrix<T, Eigen::RowMajor> solveToGuiControlMatrix;
    std::vector<int> symmetryMapping;
    std::vector<std::string> controlNames;
    int solveCount;
    int guiControlCount;
};

template <class T>
SymmetricControls<T>::SymmetricControls(const BodyLogic<T>& refBodyLogic) : m(new Private)
{
    const auto controls = refBodyLogic.GuiControlNames();
    m->guiControlCount = static_cast<int>(controls.size());
    m->solveCount = 0;
    m->symmetryMapping.resize(controls.size());
    std::vector<Eigen::Triplet<T>> triplets;

    for (size_t i = 0; i < controls.size(); i++) {
        std::string name = controls[i];

        if (name.substr(0, 5) == "local") {
            const auto pos = name.find("_l_");
            if (pos != std::string::npos) {
                // if this a left control, find the corresponding right one and add symmetric control
                name.replace(pos, 3, "_r_");
                const auto symIt = std::find(controls.begin(), controls.end(), name);
                if (symIt != controls.end()) {
                    const auto symIndex = static_cast<int>(symIt - controls.begin());
                    m->solveToGuiControlMapping.push_back({ m->solveCount, i });
                    m->solveToGuiControlMapping.push_back({ m->solveCount, symIndex });
                    m->symmetryMapping[i] = m->solveCount;
                    m->symmetryMapping[symIndex] = m->solveCount;
                    m->controlNames.push_back(name.erase(pos, 3));

                    triplets.push_back(Eigen::Triplet<T>((int)i, m->solveCount, T(1)));
                    triplets.push_back(Eigen::Triplet<T>(symIndex, m->solveCount, T(1)));
                    m->solveCount++;
                }
            }
            else if (name.find("_r_") == std::string::npos) {
                // neither left or right control, just keep it as it is
                m->symmetryMapping[i] = m->solveCount;
                m->solveToGuiControlMapping.push_back({ m->solveCount, i });
                m->controlNames.push_back(name);
                triplets.push_back(Eigen::Triplet<T>((int)i, m->solveCount, T(1)));

                m->solveCount++;
            }
        }
        else if (name.substr(0, 6) == "global") {
            m->symmetryMapping[i] = m->solveCount;
            m->solveToGuiControlMapping.push_back({ m->solveCount, i });
            m->controlNames.push_back(name);
            triplets.push_back(Eigen::Triplet<T>((int)i, m->solveCount, T(1)));
            m->solveCount++;
        }
    //    else if (name.find("pose_") != name.npos) {
    //        if (name.find("length") != name.npos || name.find("width") != name.npos || name.find("extension") != name.npos) {
    //            m->symmetryMapping[i] = m->solveCount;
    //            m->solveToGuiControlMapping.push_back({ m->solveCount, i });
    //            m->solveCount++;
    //        }
    //    }
    }

    m->solveToGuiControlMatrix = Eigen::SparseMatrix<T>(m->guiControlCount, m->solveCount);
    m->solveToGuiControlMatrix.setFromTriplets(triplets.begin(), triplets.end());
}

template <class T> SymmetricControls<T>::~SymmetricControls() = default;
template <class T> SymmetricControls<T>::SymmetricControls(SymmetricControls&&) = default;
template <class T> SymmetricControls<T>& SymmetricControls<T>::operator=(SymmetricControls&&) = default;


template <class T>
int SymmetricControls<T>::NumSolveControls() const {
    return m->solveCount;
}

template <class T>
const std::vector<int> SymmetricControls<T>::GetSymmetryMapping() const {
    return m->symmetryMapping;
}

template <class T>
const std::vector<std::string>& SymmetricControls<T>::GetControlNames() const {
    return m->controlNames;
}

template <class T>
const Eigen::SparseMatrix<T, Eigen::RowMajor>& SymmetricControls<T>::SymmetricToGuiControlsMatrix() const
{
    return m->solveToGuiControlMatrix;
}

template <class T>
DiffData<T> SymmetricControls<T>::EvaluateSymmetricControls(const DiffData<T>& solveControls) const
{
    if (solveControls.Size() != m->solveCount)
    {
        CARBON_CRITICAL("solveControl control count incorrect");
    }

    Eigen::VectorX<T> output = Eigen::VectorX<T>::Zero(m->guiControlCount);

    // evaluate Solve controls
    for (int i = 0; i < int(m->solveToGuiControlMapping.size()); i++)
    {
        const int inputIndex = m->solveToGuiControlMapping[i][0];
        const int outputIndex = m->solveToGuiControlMapping[i][1];
        const T value = solveControls.Value()[inputIndex];
        output[outputIndex] += value;
    }

    JacobianConstPtr<T> Jacobian;
    if (solveControls.HasJacobian())
    {
        SparseMatrix<T> localJacobian(m->guiControlCount, solveControls.Size());
        std::vector<Eigen::Triplet<T>> triplets;
        for (int i = 0; i < int(m->solveToGuiControlMapping.size()); i++)
        {
            const int inputIndex = m->solveToGuiControlMapping[i][0];
            const int outputIndex = m->solveToGuiControlMapping[i][1];
            triplets.push_back(Eigen::Triplet<T>(outputIndex, inputIndex, T(1.0)));
        }
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = solveControls.Jacobian().Premultiply(localJacobian);
    }

    return DiffData<T>(std::move(output), Jacobian);
}

template <class T>
const Vector<T> SymmetricControls<T>::GuiToSymmetricControls(const Vector<T>& guiControls) const {
    if (guiControls.size() != m->guiControlCount) {
        CARBON_CRITICAL("guiControl control count incorrect");
    }

    Eigen::VectorX<T> output = Eigen::VectorX<T>::Zero(m->solveCount);
    Eigen::VectorX<T> count = Eigen::VectorX<T>::Zero(m->solveCount);

    // evaluate inverse controls
    for (int i = 0; i < int(m->solveToGuiControlMapping.size()); i++) {
        const int inputIndex = m->solveToGuiControlMapping[i][0];
        const int outputIndex = m->solveToGuiControlMapping[i][1];
        output[inputIndex] += guiControls[outputIndex];
        count[inputIndex] += T(1);
    }

    for (int i = 0; i < m->solveCount; i++) {
        if (count[i] > T(0)) {
            output[i] /= count[i];
        }
    }

    return output;
}

template <class T>
const Vector<T> SymmetricControls<T>::SymmetricToGuiControls(const Vector<T>& symmetricControls) const {
    if (symmetricControls.size() != m->solveCount) {
        CARBON_CRITICAL("solveControl control count incorrect");
    }

    Eigen::VectorX<T> output = Eigen::VectorX<T>::Zero(m->guiControlCount);

    // evaluate Solve controls
    for (int i = 0; i < int(m->solveToGuiControlMapping.size()); i++) {
        const int inputIndex = m->solveToGuiControlMapping[i][0];
        const int outputIndex = m->solveToGuiControlMapping[i][1];
        output[outputIndex] += symmetricControls[inputIndex];
    }

    return output;
}


// explicitly instantiate the rig logic classes
template class SymmetricControls<float>;
template class SymmetricControls<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

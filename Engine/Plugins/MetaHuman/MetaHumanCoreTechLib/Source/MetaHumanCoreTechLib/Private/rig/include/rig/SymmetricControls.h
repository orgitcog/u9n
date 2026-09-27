// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <rig/BodyLogic.h>
#include <nls/DiffData.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Creates a symmetric control layer on top of the exisitng riglogic gui controls
 */
template <class T>
class SymmetricControls
{
public:
    SymmetricControls(const BodyLogic<T>& bodyLogicReference);
    ~SymmetricControls();
    SymmetricControls(SymmetricControls&&);
    SymmetricControls(SymmetricControls&) = delete;
    SymmetricControls& operator=(SymmetricControls&&);
    SymmetricControls& operator=(const SymmetricControls&) = delete;

    //! @returns the number of solve controls of the rig
    int NumSolveControls() const;

    /**
     * Evaluate the GUI controls given the solve controls.
     */
    DiffData<T> EvaluateSymmetricControls(const DiffData<T>& solveControls) const;

    const Vector<T> GuiToSymmetricControls(const Vector<T>& guiControls) const;
    const Vector<T> SymmetricToGuiControls(const Vector<T>& symmetricControls) const;

    const std::vector<int> GetSymmetryMapping() const;
    const std::vector<std::string>& GetControlNames() const;

    const Eigen::SparseMatrix<T, Eigen::RowMajor>& SymmetricToGuiControlsMatrix() const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

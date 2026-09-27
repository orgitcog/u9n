// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/Context.h>
#include <nls/DiffData.h>
#include <nls/math/Math.h>

#include <vector>

namespace dna {
class Reader;
}

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * BodyLogic implements rig logic including Jacobian for parametric body models
 * 
 * This is quite more open than the face rig version, as we need to have easy acces to edit/change the values of things
*/
template <class T>
class BodyLogic
{
public:
    struct GuiToRawInfo
    {
        int inputIndex;  // gui control (col index)
        int outputIndex; // raw control (row index)
        T from;
        T to;
        T slope;
        T cut;
    };

public:
	BodyLogic();
    BodyLogic(int _numLods);
    ~BodyLogic() = default;
    BodyLogic(BodyLogic&&) = default;
    BodyLogic(BodyLogic&) = default;
    BodyLogic& operator=(BodyLogic&&) = default;
    BodyLogic& operator=(const BodyLogic&) = default;

    std::shared_ptr<BodyLogic> Clone() const;

    //! Initializes BodyLogic with the data from the dna::BinaryStreamReader
    bool Init(const dna::Reader* reader);

    //! @return the number of gui controls of the rig
    int NumGUIControls() const { return int(guiControlNames.size()); }

    //! @return the number of raw controls of the rig
    int NumRawControls() const { return int(rawControlNames.size()); }

    //! @return the number of lods
    int NumLODs() const { return numLODs; }
    void SetNumLODs(const int l);

    /**
     * Evaluate the raw controls given the input gui controls. Throws an expection is the size is incorrect.
     * There is no clamping involved, any gui controls exceeding their range will extrapolate the values.
     */
    DiffData<T> EvaluateRawControls(const DiffData<T>& guiControls) const;

    /**
     * Evaluate the joint values from the raw values. Throws an expection is the size is incorrect.
     * You would typically call it with the output from EvaluateRawControls().
     */
    DiffData<T> EvaluateJoints(const int lod, const DiffData<T>& rawControls) const;
    DiffData<T> EvaluateRbfJoints(const int lod, const DiffData<T>& rbfControls) const;

    /**
     * @return the names of the GUI controls.
     */
    const std::vector<std::string>& GuiControlNames() const { return guiControlNames; }
    std::vector<std::string>& GuiControlNames() { return guiControlNames; }

    /**
     * @return the names of the raw controls.
     */
    const std::vector<std::string>& RawControlNames() const { return rawControlNames; }
    std::vector<std::string>& RawControlNames() { return rawControlNames; }

    /**
     * @return the gui control mapping
     */
    const std::vector<GuiToRawInfo>& GuiToRawMapping() const { return guiToRawMapping; };
    std::vector<GuiToRawInfo>& GuiToRawMapping() { return guiToRawMapping; }

    /**
     * @return the gui control ranges
     */
    const Eigen::Matrix<T, 2, -1>& GuiControlRanges() const { return guiControlRanges; };
    Eigen::Matrix<T, 2, -1>& GuiControlRanges() { return guiControlRanges; }

    /**
     * @return the joint matrix mapping raw controls to joint maps for the given lod
     */
    const SparseMatrix<T>& GetJointMatrix(const int lod) const { return jointMatrix[lod]; }
    SparseMatrix<T>& GetJointMatrix(const int lod) { return jointMatrix[lod]; }

    const SparseMatrix<T>& GetRbfJointMatrix(const int lod) const { return rbfJointMatrix[lod]; }
    SparseMatrix<T>& GetRbfJointMatrix(const int lod) { return rbfJointMatrix[lod]; }

private:
    void SortGuiControlMapping();

private:
    int numLODs;

    std::vector<std::string> guiControlNames;
    std::vector<std::string> rawControlNames;

    // ### GUI to Raw mapping ###
    std::vector<GuiToRawInfo> guiToRawMapping;

    // the ranges for each gui control
    Eigen::Matrix<T, 2, -1> guiControlRanges;

    // joint mapping
    std::vector<SparseMatrix<T>> jointMatrix;

    // rbf joint matrix
    std::vector<SparseMatrix<T>> rbfJointMatrix;
};


CARBON_NAMESPACE_END(TITAN_NAMESPACE)

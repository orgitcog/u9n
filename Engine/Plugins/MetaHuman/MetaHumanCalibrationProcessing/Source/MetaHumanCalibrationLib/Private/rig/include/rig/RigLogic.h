// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/Context.h>
#include <nls/DiffData.h>
#include <nls/DiffDataMatrix.h>
#include <nls/DiffDataSparseMatrix.h>
#include <nls/functions/MatrixMultiplyFunction.h>
#include <nls/math/Math.h>

#include <vector>

namespace dna
{

class Reader;
class Writer;

} // namespace dna

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * RigLogic implements rig logic including Jacobian.
 *
 * Opens:
 *  - TODO: animated map clamping needs to be fixed, once clamping in RigLogic has been fixed
 *  - TODO: the clamping scheme needs to be investigated: when using clamps the Jacobians would become 0, which would hurt any optimization. currently
 *    Jacobians are not supported when clamping is enabled
 */
template <class T>
class RigLogic
{
public:
    RigLogic();
    ~RigLogic();
    RigLogic(RigLogic&&);
    RigLogic(RigLogic&) = delete;
    RigLogic& operator=(RigLogic&&);
    RigLogic& operator=(const RigLogic&) = delete;

    std::shared_ptr<RigLogic> Clone() const;

    //! Initializes RigLogic with the data from the dna::Reader
    bool Init(const dna::Reader* reader, bool withJointScaling = false);

    //! @return the number of gui controls of the rig
    int NumGUIControls() const;

    //! @return the number of raw controls of the rig
    int NumRawControls() const;

    //! @return the number of psd controls of the rig
    int NumPsdControls() const;

    //! @return the number of ML controls of the rig
    int NumMLControls() const;

    //! @return the number of RBF controls of the rig
    int NumRBFControls() const;

    //! @return the number of total output controls of the rig i.e. sum of raw, psd, and ml controls, and the side of the data that is output of EvaluatePSD()
    int NumTotalControls() const;

    //! @return the number of neural networks
    int NumNeuralNetworks() const;

    /**
     * Evaluate the raw controls given the input gui controls. Throws an expection is the size is incorrect.
     * There is no clamping involved, any gui controls exceeding their range will extrapolate the values.
     */
    DiffData<T> EvaluateRawControls(const DiffData<T>& guiControls) const;
   
    /**
     * Evaluate the raw controls given the input euler rbf controls. Throws an expection is the size is incorrect.
     */
    DiffData<T> EvaluateRawRBFControls(const DiffData<T>& eulerRBFControls) const;

    /**
     * Evaluates the full controls from raw controls. Throws an expection is the size is incorrect.
     * Full controls contains [raw_controls, psd_controls, ml_controls]
     * You would typically call it with the output from EvaluateRawControls().
     * @param rawControls  The raw control values. @see EvaluateRawControls()
     * @param maskWeights  Neural networks are weighted by a mask weight. An empty vector is equivalent to weight=1.
     * @note we may want to rename EvaluatePSD() to EvaluateControls() to be clear that it does not only refer to PSD controls.
     */
    DiffData<T> EvaluatePSD(const DiffData<T>& rawControls, const Eigen::VectorX<T>& maskWeights = Eigen::VectorX<T>()) const;

    // /**
    // * Evaluate the blendshape coefficients from the psd values. Throws an expection is the size is incorrect.
    // * You would typically call it with the output from EvaluatePSD().
    // */
    // DiffData<T> EvaluateBlendshapes(const DiffData<T>& psdControls, int lod) const;

    /**
     * Evaluate the joint values from the psd values. Throws an expection if the size is incorrect.
     * You would typically call it with the output from EvaluatePSD().
     */
    DiffData<T> EvaluateJoints(const DiffData<T>& psdControls, int lod) const;

    /**
     * Evaluate the joint values from the psd values and additionaly a specified joint matrix.
     * Throws an expection if the size is incorrect.
     * You would typically call it with the output from EvaluatePSD().
     */
    DiffData<T> EvaluateJoints(const DiffData<T>& psdControls, const DiffDataSparseMatrix<T>& jointMatrix) const;

    /**
     * Evaluate the animated map coefficients from the psd values. Throws an expection is the size is incorrect.
     * You would typically call it with the output from EvaluatePSD().
     */
    DiffData<T> EvaluateAnimatedMaps(const DiffData<T>& psdControls, int lod) const;

    /**
     * @return the names of the GUI controls.
     */
    const std::vector<std::string>& GuiControlNames() const;

    /**
     * @return the names of the raw controls.
     */
    const std::vector<std::string>& RawControlNames() const;

    /**
     * @return the names of the ML controls.
     */
    const std::vector<std::string>& MLControlNames() const;

    /**
     * @return the names of the RBF poses.
     */
    const std::vector<std::string>& RBFPoseNames() const;

    /**
     * @return the names of the RBF pose controls.
     */
    const std::vector<std::string>& RBFPoseControlNames() const;

    /**
     * @return the names of the RBF euler controls
     */
    const std::vector<std::string>& RBFEulerControlNames() const;

    //! @return the name of neural networks
    const std::vector<std::string>& MLNetworkNames() const;

    /**
     * @return the range of the GUI controls
     */
    const Eigen::Matrix<T, 2, -1>& GuiControlRanges() const;

    /**
     * @return the number of LODs in the rig.
     */
    int NumLODs() const;

    /**
     * @return the number of joints in the rig.
     */
    int NumJoints() const;

    /**
     * @return the matrix mapping psd controls to raw controls i.e psd = PsdToRawMap() * rawControls. (output size NumTotalControls())
     * @note the first NumRawControls() rows map raw controls to itself.
     */
    const SparseMatrix<T>& PsdToRawMap() const;

    /**
     * @return the matrix mapping psd controls to joint deltas i.e. jointDeltas = jointMatrix * psdControls
     * Matrix size is therefore (dofPerJoint * numJoints, totalControlCount) == (jointRowCount, jointColumnCount)
     */
    const SparseMatrix<T>& JointMatrix(int lod) const;

    //! Set the joint matrix @p mat for lod @p load.
    void SetJointMatrix(int lod, const SparseMatrix<T>& mat);

    //! Remove all LODs besides the highest.
    void ReduceToLOD0Only();

    //! @return whether joint scaling is output by RigLogic
    bool WithJointScaling() const;

    /**
     * The PSD values for all expressions (raw controls, and each corretive expression) of the rig sorted by the number of raw
     * controls that affect the expression.
     * @return vector with tuples containing the number of raw controls affecting the expression, the index into the PSD output (EvaluatePSD())
     * for the expression, and the psd control activations.
     */
    std::vector<std::tuple<int, int, Eigen::VectorX<T>>> GetAllExpressions() const;

    /**
     * @brief Remove joints from RigLogic. This is being together with simplification in RigGeometry.
     */
    void RemoveJoints(const std::vector<int>& newToOldJointMapping);

    //! @brief Reduce internal PSD logic to only support @p guiControls. All other gui controls will be ignored even if the are non-zero.
    void ReduceToGuiControls(const std::vector<int>& guiControls);

    /**
     * @brief Calculates the inverse of EvaluateRawControls i.e. gets the gui controls based on the raw controls.
     * @param[out] inconsistentGuiControls  It is possible that the raw controls lead to inconsistent gui controls. the gui controls that are not well defined
     * are set here.
     * @returns the GUI controls that map to raw controls.
     */
    Eigen::VectorX<T> GuiControlsFromRawControls(const Eigen::VectorX<T>& rawControls, std::vector<int>& inconsistentGuiControls) const;

    //! @returns all gui controls that do not affect any raw control.
    std::vector<int> UnusedGuiControls() const;

    //! @returns all raw controls that are not affected by gui controls.
    std::vector<int> UnusedRawControls() const;

    //! @returns which joints are unmapped by riglogic
    std::vector<int> UnmappedJoints() const;

    //! @returns the index for gui control @p name
    int GuiControlIndex(const char* name) const;

    //! @returns the index for raw control @p name
    int RawControlIndex(const char* name) const;

    //! Mirror the riglogic joint activations.
    void MirrorJoints(const std::vector<int>& symmetricJointIndices);


    /**
     * Get the symmetric gui control indices. Entries point to the symmetric index.
     * The multiplier needs to be applied to the symmetric controls. This is
     * used for controls such as CTRL_jaw.tx as it has a [-1, 1] mapping as well
     * as the symmetric CTRL_L_eye.tx and CTRL_R_eye.tx.
     */
    std::vector<std::pair<int, T>> GetSymmetricGuiControlIndices() const;

    //! Get the used raw controls for a gui control
    std::vector<std::vector<int>> GetUsedRawControls() const;

    //! Get the symmetric raw control indices
    std::vector<int> GetSymmetricRawControlIndices() const;

    //! Get the symmetric psd indices
    std::vector<int> GetSymmetricPsdIndices() const;

    //! Get the joint group indices per joint
    const std::vector<int>& GetJointGroupIndices() const;

    //! Get the joint group joint indices
    const std::vector<Eigen::VectorX<std::uint16_t>>& GetJointGroupJointIndices() const;

    //! Get the joint group input indices
    const std::vector<Eigen::VectorX<std::uint16_t>>& GetJointGroupInputIndices() const;

    //! Get the joint group output indices
    const std::vector<Eigen::VectorX<std::uint16_t>>& GetJointGroupOutputIndices() const;

    //! Save joint deltas to the dna binary stream
    void SaveJointDeltas(dna::Writer* writer) const;

private:
    void SortGuiControlMapping();

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

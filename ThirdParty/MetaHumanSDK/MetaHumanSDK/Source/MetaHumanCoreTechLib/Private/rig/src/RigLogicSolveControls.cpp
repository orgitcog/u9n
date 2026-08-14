// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/RigLogicSolveControls.h>

#include <nls/DiffScalar.h>
#include <nls/VectorVariable.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>

#include <riglogic/RigLogic.h>

#include <set>
#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct RigLogicSolveControls<T>::Private
{
    //! name of riglogic solve controls
    std::string name;

    int solveControlCount = 0;
    int guiControlCount = 0;

    std::vector<std::string> solveControlNames;

    //! list of indices of which gui controls are used by this RigLogicSolveControl
    std::vector<int> usedGuiControls;

    // ### Solve to GUI mapping ###
    // mapping from input (rowIndex=0) to output (rowIndex=1) for Solve controls (columns)
    Eigen::Matrix<int, 2, -1> solveToGuiControlMapping;
    // from(rowIndex=0), to(rowIndex=1), slope(rowIndex=2), cut(rowIndex=3), from_output(rowIndex=4), to_output(rowIndex=5)
    // for each Solve control (columns)
    Eigen::Matrix<T, 6, -1> solveToGuiControlValues;
    // the ranges for each solve control
    Eigen::Matrix<T, 2, -1> solveControlRanges;
    Eigen::Matrix<T, 2, -1> guiControlMappingRanges;
    //! regularization scaling describes how much each control show be regularized.
    Eigen::VectorX<T> solveControlRegularizationScaling;

    // TEMPORARY version of procedural controls: upper chin raise is procedurally applied based on the
    // settings of jawopen, lower chin raise and upper lip raise. the solver control json file needs
    // to include the "Procedural Controls" key to enable those
    int jawOpen = -1;
    int L_upperLipRaise = -1;
    int L_chinRaiseD = -1;
    int L_chinRaiseU = -1;
    int R_upperLipRaise = -1;
    int R_chinRaiseD = -1;
    int R_chinRaiseU = -1;
};


template <class T>
RigLogicSolveControls<T>::RigLogicSolveControls() : m(new Private)
{}

template <class T> RigLogicSolveControls<T>::~RigLogicSolveControls() = default;
template <class T> RigLogicSolveControls<T>::RigLogicSolveControls(RigLogicSolveControls&&) = default;
template <class T> RigLogicSolveControls<T>& RigLogicSolveControls<T>::operator=(RigLogicSolveControls&&) = default;

template <class T>
bool RigLogicSolveControls<T>::Init(const RigLogic<T>& rigLogicReference, const JsonElement& rigLogicSolveControlJson)
{
    if (!rigLogicSolveControlJson.IsObject())
    {
        CARBON_CRITICAL("json does not describe solve controls");
    }
    m->guiControlCount = rigLogicReference.NumGUIControls();
    const std::vector<std::string>& guiControlNames = rigLogicReference.GuiControlNames();

    m->name = rigLogicSolveControlJson["Name"].String();
    const JsonElement& solveControlJson = rigLogicSolveControlJson["Controls"];

    const int solveControlCount = int(solveControlJson.Size());
    struct Segment
    {
        int guiControlIndex;
        T from_input;
        T to_input;
        T from_output;
        T to_output;
    };
    std::map<std::string, std::vector<Segment>> solveControlSegments;
    std::vector<std::string> solveControNameVector;
    std::vector<T> regularizationScaling;
    int numSolveToGuiAssignments = 0;
    if (solveControlJson.IsArray())
    {
        for (const auto& entry : solveControlJson.Array())
        {
            if (entry.IsString())
            {
                const std::string solveControlName = entry.String();
                if (solveControlSegments.find(solveControlName) != solveControlSegments.end())
                {
                    CARBON_CRITICAL("control with name {} already defined", solveControlName);
                }
                solveControNameVector.push_back(solveControlName);
                regularizationScaling.push_back(T(1));
                const std::string guiControlName = solveControlName;
                auto it = std::find(guiControlNames.begin(), guiControlNames.end(), guiControlName);
                if (it == guiControlNames.end())
                {
                    CARBON_CRITICAL("no gui control {}", guiControlName);
                }
                const int guiControlIndex = int(std::distance(guiControlNames.begin(), it));
                T from_input = rigLogicReference.GuiControlRanges()(0, guiControlIndex);
                T to_input = rigLogicReference.GuiControlRanges()(1, guiControlIndex);
                if (from_input > to_input)
                {
                    std::swap(from_input, to_input);
                }
                solveControlSegments[guiControlName].push_back(Segment{ guiControlIndex, from_input, to_input, from_input, to_input });
                numSolveToGuiAssignments++;
            }
            else if (entry.IsObject())
            {
                const std::string solveControlName = entry["Name"].String();
                if (solveControlSegments.find(solveControlName) != solveControlSegments.end())
                {
                    CARBON_CRITICAL("control with name {} already defined", solveControlName);
                }
                solveControNameVector.push_back(solveControlName);
                if (entry.Contains("Regularization"))
                {
                    regularizationScaling.push_back(entry["Regularization"].Get<T>());
                }
                else
                {
                    regularizationScaling.push_back(T(1));
                }
                if (entry.Contains("Map"))
                {
                    for (const auto& [guiControlName, mapping] : entry["Map"].Map())
                    {
                        auto it = std::find(guiControlNames.begin(), guiControlNames.end(), guiControlName);
                        if (it == guiControlNames.end())
                        {
                            CARBON_CRITICAL("no gui control {}", guiControlName);
                        }

                        const int guiControlIndex = int(std::distance(guiControlNames.begin(), it));
                        auto [from_input, from_output] = mapping["from"].template Get<std::pair<T, T>>();
                        auto [to_input, to_output] = mapping["to"].template Get<std::pair<T, T>>();
                        if (from_input > to_input)
                        {
                            std::swap(from_input, to_input);
                            std::swap(from_output, to_output);
                        }
                        solveControlSegments[solveControlName].push_back(Segment{ guiControlIndex, from_input, to_input, from_output, to_output });
                        numSolveToGuiAssignments++;
                    }
                }
                else
                {
                    const std::string guiControlName = solveControlName;
                    auto it = std::find(guiControlNames.begin(), guiControlNames.end(), guiControlName);
                    if (it == guiControlNames.end())
                    {
                        CARBON_CRITICAL("no gui control {}", guiControlName);
                    }
                    const int guiControlIndex = int(std::distance(guiControlNames.begin(), it));
                    T from_input = rigLogicReference.GuiControlRanges()(0, guiControlIndex);
                    T to_input = rigLogicReference.GuiControlRanges()(1, guiControlIndex);
                    if (from_input > to_input)
                    {
                        std::swap(from_input, to_input);
                    }
                    solveControlSegments[guiControlName].push_back(Segment{ guiControlIndex, from_input, to_input, from_input, to_input });
                    numSolveToGuiAssignments++;
                }
            }
            else
            {
                CARBON_CRITICAL("mapping needs to be a string or a dictionary");
            }
        }
    }
    else if (solveControlJson.IsObject())
    {
        for (const auto& [solveControlName, guiControlMappings] : solveControlJson.Map())
        {
            if (solveControlSegments.find(solveControlName) != solveControlSegments.end())
            {
                CARBON_CRITICAL("control with name {} already defined", solveControlName);
            }
            solveControNameVector.push_back(solveControlName);
            regularizationScaling.push_back(T(1));
            if (guiControlMappings.IsString())
            {
                const std::string guiControlName = guiControlMappings.String();
                auto it = std::find(guiControlNames.begin(), guiControlNames.end(), guiControlName);
                if (it == guiControlNames.end())
                {
                    CARBON_CRITICAL("no gui control {}", guiControlName);
                }
                const int guiControlIndex = int(std::distance(guiControlNames.begin(), it));
                T from_input = rigLogicReference.GuiControlRanges()(0, guiControlIndex);
                T to_input = rigLogicReference.GuiControlRanges()(1, guiControlIndex);
                if (from_input > to_input)
                {
                    std::swap(from_input, to_input);
                }
                solveControlSegments[solveControlName].push_back(Segment{ guiControlIndex, from_input, to_input, from_input, to_input });
                numSolveToGuiAssignments++;
            }
            else if (guiControlMappings.IsObject())
            {
                for (const auto& [guiControlName, mapping] : guiControlMappings.Map())
                {
                    auto it = std::find(guiControlNames.begin(), guiControlNames.end(), guiControlName);
                    if (it == guiControlNames.end())
                    {
                        CARBON_CRITICAL("no gui control {}", guiControlName);
                    }

                    const int guiControlIndex = int(std::distance(guiControlNames.begin(), it));
                    auto [from_input, from_output] = mapping["from"].template Get<std::pair<T, T>>();
                    auto [to_input, to_output] = mapping["to"].template Get<std::pair<T, T>>();
                    if (from_input > to_input)
                    {
                        std::swap(from_input, to_input);
                        std::swap(from_output, to_output);
                    }
                    solveControlSegments[solveControlName].push_back(Segment{ guiControlIndex, from_input, to_input, from_output, to_output });
                    numSolveToGuiAssignments++;
                }
            }
            else
            {
                CARBON_CRITICAL("mapping needs to be a string or a dictionary");
            }
        }
    }
    else
    {
        CARBON_CRITICAL("invalid solve control mapping");
    }

    // setup solve to gui calculation
    m->solveControlCount = solveControlCount;
    m->solveControlNames = solveControNameVector;
    m->solveControlRegularizationScaling = Eigen::Map<const Eigen::VectorX<T>>(regularizationScaling.data(), (int)regularizationScaling.size());

    m->solveControlRanges = Eigen::Matrix<T, 2, -1>(2, solveControlCount);
    m->solveControlRanges.row(0).setConstant(1e6f);
    m->solveControlRanges.row(1).setConstant(-1e6f);
    m->guiControlMappingRanges = rigLogicReference.GuiControlRanges();
    m->guiControlMappingRanges.row(0).setConstant(1e6f);
    m->guiControlMappingRanges.row(1).setConstant(-1e6f);

    m->solveToGuiControlMapping.resize(2, numSolveToGuiAssignments);
    m->solveToGuiControlValues.resize(6, numSolveToGuiAssignments);

    int segmentIndex = 0;
    for (const auto& [controlName, segments] : solveControlSegments)
    {
        const int controlIndex =
            static_cast<int>(std::distance(solveControNameVector.begin(), std::find(solveControNameVector.begin(), solveControNameVector.end(), controlName)));

        for (const auto& segment : segments)
        {
            m->solveControlRanges(0, controlIndex) = std::min(m->solveControlRanges(0, controlIndex), segment.from_input);
            m->solveControlRanges(1, controlIndex) = std::max(m->solveControlRanges(1, controlIndex), segment.to_input);
            m->guiControlMappingRanges(0, segment.guiControlIndex) = std::min(m->guiControlMappingRanges(0, segment.guiControlIndex), segment.from_output);
            m->guiControlMappingRanges(0, segment.guiControlIndex) = std::min(m->guiControlMappingRanges(0, segment.guiControlIndex), segment.to_output);
            m->guiControlMappingRanges(1, segment.guiControlIndex) = std::max(m->guiControlMappingRanges(1, segment.guiControlIndex), segment.from_output);
            m->guiControlMappingRanges(1, segment.guiControlIndex) = std::max(m->guiControlMappingRanges(1, segment.guiControlIndex), segment.to_output);
            m->solveToGuiControlMapping(0, segmentIndex) = controlIndex;
            m->solveToGuiControlMapping(1, segmentIndex) = segment.guiControlIndex;
            m->solveToGuiControlValues(0, segmentIndex) = segment.from_input;
            m->solveToGuiControlValues(1, segmentIndex) = segment.to_input;
            const T slope = (segment.to_output - segment.from_output) / (segment.to_input - segment.from_input);
            const T cut = segment.from_output - slope * segment.from_input;
            m->solveToGuiControlValues(2, segmentIndex) = slope;
            m->solveToGuiControlValues(3, segmentIndex) = cut;
            m->solveToGuiControlValues(4, segmentIndex) = segment.from_output;
            m->solveToGuiControlValues(5, segmentIndex) = segment.to_output;

            segmentIndex++;
        }
    }

    std::set<int> usedGuiControls;

    // check all gui controls that are used
    for (int i = 0; i < int(m->solveToGuiControlMapping.cols()); i++)
    {
        usedGuiControls.emplace(m->solveToGuiControlMapping(1, i));
    }

    // semi-hardcoded procedural methods. whether to activate the procedural method is depending on the solver definition json file.
    if (rigLogicSolveControlJson.Contains("Procedural Controls"))
    {
        const auto& proceduralControlsJson = rigLogicSolveControlJson["Procedural Controls"];
        m->jawOpen = static_cast<int>(std::distance(guiControlNames.begin(), std::find(guiControlNames.begin(), guiControlNames.end(), "CTRL_C_jaw.ty")));
        if (proceduralControlsJson.Contains("CTRL_L_jaw_ChinRaiseU.ty"))
        {
            m->L_chinRaiseD =
                static_cast<int>(std::distance(guiControlNames.begin(), std::find(guiControlNames.begin(), guiControlNames.end(), "CTRL_L_jaw_ChinRaiseD.ty")));
            m->L_chinRaiseU =
                static_cast<int>(std::distance(guiControlNames.begin(), std::find(guiControlNames.begin(), guiControlNames.end(), "CTRL_L_jaw_ChinRaiseU.ty")));
            m->L_upperLipRaise =
                static_cast<int>(std::distance(guiControlNames.begin(),
                                               std::find(guiControlNames.begin(), guiControlNames.end(), "CTRL_L_mouth_upperLipRaise.ty")));
            usedGuiControls.emplace(m->L_chinRaiseU);
        }
        if (proceduralControlsJson.Contains("CTRL_R_jaw_ChinRaiseU.ty"))
        {
            m->R_chinRaiseD =
                static_cast<int>(std::distance(guiControlNames.begin(), std::find(guiControlNames.begin(), guiControlNames.end(), "CTRL_R_jaw_ChinRaiseD.ty")));
            m->R_chinRaiseU =
                static_cast<int>(std::distance(guiControlNames.begin(), std::find(guiControlNames.begin(), guiControlNames.end(), "CTRL_R_jaw_ChinRaiseU.ty")));
            m->R_upperLipRaise =
                static_cast<int>(std::distance(guiControlNames.begin(),
                                               std::find(guiControlNames.begin(), guiControlNames.end(), "CTRL_R_mouth_upperLipRaise.ty")));
            usedGuiControls.emplace(m->R_chinRaiseU);
        }
    }

    m->usedGuiControls = std::vector<int>(usedGuiControls.begin(), usedGuiControls.end());

    return true;
}

template <class T>
const std::string& RigLogicSolveControls<T>::Name() const
{
    return m->name;
}

template <class T>
int RigLogicSolveControls<T>::NumSolveControls() const
{
    return m->solveControlCount;
}

template <class T>
const std::vector<std::string>& RigLogicSolveControls<T>::SolveControlNames() const
{
    return m->solveControlNames;
}

template <class T>
const Eigen::Matrix<T, 2, -1>& RigLogicSolveControls<T>::SolveControlRanges() const
{
    return m->solveControlRanges;
}

template <class T>
DiffData<T> RigLogicSolveControls<T>::EvaluateGuiControls(const DiffData<T>& solveControls) const
{
    if (solveControls.Size() != m->solveControlCount)
    {
        CARBON_CRITICAL("solveControl control count incorrect");
    }

    Eigen::VectorX<T> output = Eigen::VectorX<T>::Zero(m->guiControlCount);

    // evaluate Solve controls
    for (int i = 0; i < int(m->solveToGuiControlMapping.cols()); i++)
    {
        const int inputIndex = m->solveToGuiControlMapping(0, i);
        const int outputIndex = m->solveToGuiControlMapping(1, i);
        const T from = m->solveToGuiControlValues(0, i);
        const T to = m->solveToGuiControlValues(1, i);
        const T slope = m->solveToGuiControlValues(2, i);
        const T cut = m->solveToGuiControlValues(3, i);
        const T value = solveControls.Value()[inputIndex];
        const bool belowRange = (value <= from);
        const bool aboveRange = (value >= to);
        if ((from <= value) && (value < to))
        {
            output[outputIndex] += slope * value + cut;
        }
        else if (belowRange)
        {
            output[outputIndex] += slope * from + cut; // clamp to minimum range value
        }
        else if (aboveRange)
        {
            output[outputIndex] += slope * to + cut; // clamp to maximum range value
        }
    }

    JacobianConstPtr<T> Jacobian;

    if (solveControls.HasJacobian())
    {
        SparseMatrix<T> localJacobian(m->guiControlCount, solveControls.Size());
        std::vector<Eigen::Triplet<T>> triplets;
        for (int i = 0; i < int(m->solveToGuiControlMapping.cols()); i++)
        {
            const int inputIndex = m->solveToGuiControlMapping(0, i);
            const int outputIndex = m->solveToGuiControlMapping(1, i);
            const T rangeStart = m->solveControlRanges(0, inputIndex);
            const T rangeEnd = m->solveControlRanges(1, inputIndex);
            const T from = m->solveToGuiControlValues(0, i);
            const T to = m->solveToGuiControlValues(1, i);
            const T slope = m->solveToGuiControlValues(2, i);
            const T value = solveControls.Value()[inputIndex];
            const bool belowRange = (from == rangeStart && value < from);
            const bool aboveRange = (to == rangeEnd && value >= to);
            if (((from <= value) && (value < to)) || belowRange || aboveRange)
            {
                if (slope != 0)
                {
                    // Keep the Jacobian even when the values are clamped, see similar explanation in RigLogic::EvaluateRawControls()
                    triplets.push_back(Eigen::Triplet<T>(outputIndex, inputIndex, slope));
                }
            }
        }
        localJacobian.setFromTriplets(triplets.begin(), triplets.end());
        Jacobian = solveControls.Jacobian().Premultiply(localJacobian);
    }

    // check for additional procedural controls
    std::vector<DiffScalar<T>> proceduralResults;
    const int numOutputs = int(output.size());
    SparseMatrix<T> ident = Eigen::Matrix<T, -1, -1>::Identity(numOutputs, numOutputs).sparseView();
    SparseMatrix<T> proceduralTransform = ident;
    bool proceduralOutput = false;
    if ((m->L_chinRaiseU >= 0) && (m->jawOpen >= 0) && (m->L_chinRaiseD >= 0) && (m->L_upperLipRaise >= 0))
    {
        DiffScalar<T> jawOpenValue(output[m->jawOpen], ident.row(m->jawOpen));
        DiffScalar<T> L_chinRaiseDValue(output[m->L_chinRaiseD], ident.row(m->L_chinRaiseD));
        DiffScalar<T> L_upperLipRaiseValue(output[m->L_upperLipRaise], ident.row(m->L_upperLipRaise));
        DiffScalar<T> L_chinRaiseUValue = max<T>(0, L_chinRaiseDValue * clamp<T>(T(3.33) * (T(0.3) - jawOpenValue), T(0), T(1)) - L_upperLipRaiseValue);
        output[m->L_chinRaiseU] = L_chinRaiseUValue.Value();
        proceduralTransform.row(m->L_chinRaiseU) = L_chinRaiseUValue.Jacobian();
        proceduralOutput = true;
    }

    if ((m->R_chinRaiseU >= 0) && (m->jawOpen >= 0) && (m->R_chinRaiseD >= 0) && (m->R_upperLipRaise >= 0))
    {
        DiffScalar<T> jawOpenValue(output[m->jawOpen], ident.row(m->jawOpen));
        DiffScalar<T> R_chinRaiseDValue(output[m->R_chinRaiseD], ident.row(m->R_chinRaiseD));
        DiffScalar<T> R_upperLipRaiseValue(output[m->R_upperLipRaise], ident.row(m->R_upperLipRaise));
        DiffScalar<T> R_chinRaiseUValue = max<T>(0, R_chinRaiseDValue * clamp<T>(T(3.33) * (T(0.3) - jawOpenValue), T(0), T(1)) - R_upperLipRaiseValue);
        output[m->R_chinRaiseU] = R_chinRaiseUValue.Value();
        proceduralTransform.row(m->R_chinRaiseU) = R_chinRaiseUValue.Jacobian();
        proceduralOutput = true;
    }

    if (proceduralOutput)
    {
        if (Jacobian)
        {
            Jacobian = Jacobian->Premultiply(proceduralTransform);
        }
    }

    return DiffData<T>(std::move(output), Jacobian);
}

template <class T>
Eigen::VectorX<T> RigLogicSolveControls<T>::SolveControlsFromGuiControls(const Eigen::VectorX<T>& guiControls,
                                                                         std::vector<int>& inconsistentSolveControls) const
{
    Eigen::Matrix<T, 2, -1> solveValuesInterval = m->solveControlRanges;
    inconsistentSolveControls.clear();

    // get solve control from gui control
    for (int i = 0; i < int(m->solveToGuiControlMapping.cols()); i++)
    {
        const int inputIndex = m->solveToGuiControlMapping(0, i);
        const int outputIndex = m->solveToGuiControlMapping(1, i);
        const T from = m->solveToGuiControlValues(0, i);
        const T to = m->solveToGuiControlValues(1, i);
        const T slope = m->solveToGuiControlValues(2, i);
        const T cut = m->solveToGuiControlValues(3, i);
        const T outputValue = std::clamp<T>(guiControls[outputIndex], m->guiControlMappingRanges(0, outputIndex), m->guiControlMappingRanges(1, outputIndex));
        const T outputValueFrom = m->solveToGuiControlValues(4, i);
        const T outputValueTo = m->solveToGuiControlValues(5, i);
        const T outputValueMin = std::min<T>(outputValueFrom, outputValueTo);
        const T outputValueMax = std::max<T>(outputValueFrom, outputValueTo);
        bool hasInconsistency = false;
        if (slope != 0)
        {
            if ((outputValueMin < outputValue) && (outputValue < outputValueMax))
            {
                const T inputValue = (outputValue - cut) / slope;
                if ((inputValue < (solveValuesInterval(0, inputIndex) - 1e-4)) || (inputValue > (solveValuesInterval(1, inputIndex) + 1e-4)))
                {
                    hasInconsistency = true;
                    LOG_INFO("map {} to {}, but current valid interval is {}/{}", m->solveControlNames[inputIndex], inputValue,
                             solveValuesInterval(0, inputIndex), solveValuesInterval(1, inputIndex));
                }
                solveValuesInterval(0, inputIndex) = inputValue;
                solveValuesInterval(1, inputIndex) = inputValue;
            }
            else if (outputValueFrom == outputValue)
            {
                if (from < solveValuesInterval(0, inputIndex))
                {
                    hasInconsistency = true;
                    LOG_INFO("limit {} to have max {}, but current valid interval is {}/{}", m->solveControlNames[inputIndex], from,
                             solveValuesInterval(0, inputIndex), solveValuesInterval(1, inputIndex));
                }
                solveValuesInterval(1, inputIndex) = std::min<T>(from, solveValuesInterval(1, inputIndex));
            }
            else if (outputValueTo == outputValue)
            {
                if (to > solveValuesInterval(1, inputIndex))
                {
                    hasInconsistency = true;
                    LOG_INFO("limit {} to have min {}, but current valid interval is {}/{}", m->solveControlNames[inputIndex], to,
                             solveValuesInterval(0, inputIndex), solveValuesInterval(1, inputIndex));
                }
                solveValuesInterval(0, inputIndex) = std::max<T>(to, solveValuesInterval(0, inputIndex));
            }
        }
        if (hasInconsistency)
        {
            inconsistentSolveControls.push_back(inputIndex);
        }
    }

    return solveValuesInterval.colwise().mean();
}

template <class T>
const std::vector<int>& RigLogicSolveControls<T>::UsedGuiControls() const
{
    return m->usedGuiControls;
}

template <class T>
std::vector<std::vector<int>> RigLogicSolveControls<T>::UsedGuiControlsPerSolveControl() const
{
    std::vector<std::vector<int>> usedControls(NumSolveControls());
    for (int i = 0; i < int(m->solveToGuiControlMapping.cols()); i++)
    {
        const int inputIndex = m->solveToGuiControlMapping(0, i);
        const int outputIndex = m->solveToGuiControlMapping(1, i);
        usedControls[inputIndex].push_back(outputIndex);
    }
    return usedControls;
}

template <class T>
const Eigen::VectorX<T>& RigLogicSolveControls<T>::SolveControlRegularizationScaling() const
{
    return m->solveControlRegularizationScaling;
}

// explicitly instantiate the rig logic classes
template class RigLogicSolveControls<float>;
template class RigLogicSolveControls<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>
#include <nrr/rt/EulerXYZTransform.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rt)

//! Head state with vertices for face, teeth, left eye, and right eye.
template <class T>
struct HeadVertexState
{
    Eigen::Matrix<T, 3, -1> faceVertices;
    Eigen::Matrix<T, 3, -1> teethVertices;
    Eigen::Matrix<T, 3, -1> eyeLeftVertices;
    Eigen::Matrix<T, 3, -1> eyeRightVertices;

    bool IsValidFaceData() const { return faceVertices.size() > 0; }
    bool IsValidTeethData() const { return teethVertices.size() > 0; }
    bool IsValidLeftEyeData() const { return (eyeLeftVertices.size() > 0); }
    bool IsValidRightEyeData() const { return (eyeRightVertices.size() > 0); }
    bool IsValidEyesData() const { return IsValidLeftEyeData() && IsValidRightEyeData(); }

    void Reset()
    {
        ResetFace();
        ResetTeeth();
        ResetEyes();
    }

    void ResetFace() { faceVertices.resize(3, 0); }

    void ResetTeeth() { teethVertices.resize(3, 0); }

    void ResetEyes()
    {
        eyeLeftVertices.resize(3, 0);
        eyeRightVertices.resize(3, 0);
    }

    Eigen::VectorX<T> Flatten() const
    {
        Eigen::VectorX<T> output(faceVertices.size() + teethVertices.size() + eyeLeftVertices.size() + eyeRightVertices.size());
        int offset = 0;
        output.segment(offset, faceVertices.size()) = Eigen::Map<const Eigen::VectorX<T>>(faceVertices.data(), faceVertices.size());
        offset += int(faceVertices.size());
        output.segment(offset, teethVertices.size()) = Eigen::Map<const Eigen::VectorX<T>>(teethVertices.data(), teethVertices.size());
        offset += int(teethVertices.size());
        output.segment(offset, eyeLeftVertices.size()) = eyeLeftVertices;
        offset += int(eyeLeftVertices.size());
        output.segment(offset, eyeRightVertices.size()) = eyeRightVertices;
        offset += int(eyeRightVertices.size());
        return output;
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rt)

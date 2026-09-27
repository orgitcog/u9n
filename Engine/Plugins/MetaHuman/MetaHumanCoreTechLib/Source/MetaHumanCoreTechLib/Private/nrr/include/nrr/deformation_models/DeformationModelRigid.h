// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nrr/deformation_models/DeformationModel.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/DiffDataAffine.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class DeformationModelRigid final : public DeformationModel<T>
{
public:
    static constexpr const char* ConfigName() { return "Rigid Deformation Model Configuration"; }

    DeformationModelRigid();
    virtual ~DeformationModelRigid() override;
    DeformationModelRigid(DeformationModelRigid&& other);
    DeformationModelRigid(const DeformationModelRigid& other);
    DeformationModelRigid& operator=(DeformationModelRigid&& other);
    DeformationModelRigid& operator=(const DeformationModelRigid& other);

    //! inherited from DeformationModel
    virtual DiffDataMatrix<T, 3, -1> EvaluateVertices(Context<T>* context) override;
    virtual Cost<T> EvaluateModelConstraints(Context<T>* context) override;
    virtual const Configuration& GetConfiguration() const override;
    virtual void SetConfiguration(const Configuration& config) override;

    //! Evaluate the affine transformation
    DiffDataAffine<T, 3, 3> EvaluateAffine(Context<T>* context);

    //! Set the current mesh (required)
    void SetVertices(const Eigen::Matrix<T, 3, -1>& vertices);

    //! Set the current rigid transformation
    void SetRigidTransformation(const Affine<T, 3, 3>& affine);

    //! Returns the current rigid transformation
    Affine<T, 3, 3> RigidTransformation() const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nrr/deformation_models/DeformationModel.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/Mesh.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class DeformationModelIdentity : public DeformationModel<T>
{
public:
    DeformationModelIdentity();
    virtual ~DeformationModelIdentity() override;
    DeformationModelIdentity(DeformationModelIdentity&& other);
    DeformationModelIdentity(const DeformationModelIdentity& other) = delete;
    DeformationModelIdentity& operator=(DeformationModelIdentity&& other);
    DeformationModelIdentity& operator=(const DeformationModelIdentity& other) = delete;

    //! inherited from DeformationModel
    virtual DiffDataMatrix<T, 3, -1> EvaluateVertices(Context<T>* context) override final;
    virtual Cost<T> EvaluateModelConstraints(Context<T>* context) override final;
    virtual const Configuration& GetConfiguration() const override final;
    virtual void SetConfiguration(const Configuration& config) override final;

    //! Load the identity model
    void LoadModel(const std::string& identityModelFileOrJsonString);

    //! @returns the number of parameters
    int NumParameters() const;

    //! Resets the model parameters to default.
    void ResetParameters();

    //! @returns the current model parameters
    const Vector<T>& ModelParameters() const;

    //! @returns sets the current model parameters
    void SetModelParameters(const Vector<T>& params);

    //! Returns the deformed vertex positions without any rigid transformation
    Eigen::Matrix<T, 3, -1> DeformedVertices() const;

    //! Set the current rigid transformation
    void SetRigidTransformation(const Affine<T, 3, 3>& affine);

    //! Returns the current rigid transformation
    Affine<T, 3, 3> RigidTransformation() const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

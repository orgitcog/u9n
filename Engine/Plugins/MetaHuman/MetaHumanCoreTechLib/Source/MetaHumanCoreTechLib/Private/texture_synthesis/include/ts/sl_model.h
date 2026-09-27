// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <string>
#include <map>
#include <carbon/utils/TaskThreadPool.h>
#include <carbon/Math.h>
#include <ts/model_data_provider_interface.h>
#include <ts/pca.h>
#include <ts/ts_types.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

class SLModel
{
public:
    using Scalar = float;
    using Vector2 = Eigen::Vector2<Scalar>;
    using Vector3 = Eigen::Vector3<Scalar>;
    using VectorX = Eigen::VectorX<Scalar>;
    using RowVectorX = Eigen::RowVectorX<Scalar>;
    using MatrixType = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using ImageType = Eigen::Matrix<Vector3, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

public:
    SLModel() {}
    SLModel(const TextureModelParams& params);

    // load the required data
    bool Load(IModel_Data_Provider& modelDataProvider);

    //! @returns True if the SL model has valid data.
    bool is_valid() const;
    ImageType synthesize_neutral_LF(const Vector2& v, const float yellow_offset=0.0f) const;
    Vector3 skin_tone(const Vector2& v) const { return m_skin_tones_pca.Reconstruct(v); }
    const MatrixType& LF_model() const;
    Vector2 ProjectSkinTone(const Vector3& skin_tone) const;

private:
    MatrixType yellow_graded_LF_model(const float yellow_offset) const;

    int m_res_LF{};
    PCA<MatrixType> m_skin_tones_pca{};
    MatrixType m_LF_model{};
    RowVectorX m_yellow_mask{};
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

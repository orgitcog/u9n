// Copyright Epic Games, Inc. All Rights Reserved.

#include <ts/sl_model.h>
#include <ts/utils/ts_utils.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

SLModel::SLModel(const TextureModelParams& params)
    : m_res_LF(params.resolution_LF)
{}

bool SLModel::Load(IModel_Data_Provider& modelDataProvider)
{
    // Load skin tones PCA
    Eigen::VectorX<Scalar> mu = detail::ModelDataToEigenMap<float>(modelDataProvider.load(Data_Type::pca_mu));
    MatrixType S = detail::ModelDataToEigenMap<float>(modelDataProvider.load(Data_Type::pca_S));
    MatrixType T = detail::ModelDataToEigenMap<float>(modelDataProvider.load(Data_Type::pca_T));
    m_skin_tones_pca = PCA<MatrixType>(mu, S, T);

    // Load LF model
    m_LF_model = detail::ModelDataToEigenMap<float>(modelDataProvider.load(TextureType::ALBEDO));

    // Load yellow mask
    m_yellow_mask = detail::ModelDataToEigenMap<float>(modelDataProvider.load(Data_Type::yellow_mask)).transpose();

    return is_valid();
}

bool SLModel::is_valid() const
{
    if (m_skin_tones_pca.Size() != 3)
    {
        LOG_ERROR("unexpected skin tones pca size: {} instead of 3", m_skin_tones_pca.Size());
        return false;
    }
    if (m_skin_tones_pca.NumCoeffs() != 2) 
    {
        LOG_ERROR("unexpected skin tones pca coefficients size: {} instead of 2", m_skin_tones_pca.NumCoeffs());
        return false;
    }
    const int size = m_res_LF * m_res_LF * 3;
    if (m_LF_model.cols() != size)
    {
        LOG_ERROR("unexpected LF model size: {} instead of {}", m_LF_model.cols(), size);
        return false;
    }
    if (m_yellow_mask.cols() != size)
    {
        LOG_ERROR("unexpected yellow mask size: {} instead of {}", m_yellow_mask.cols(), size);
        return false;
    }
    return true;
}

const SLModel::MatrixType& SLModel::LF_model() const
{
    return m_LF_model;
}

SLModel::MatrixType SLModel::yellow_graded_LF_model(const float yellow_offset) const
{
    if (yellow_offset == 0.0f)
    {
        return m_LF_model;
    }

    const Scalar gain_R = 1.0f + 1.2f * yellow_offset;
    const Scalar gain_G = 1.0f + 0.25f * yellow_offset;
    const Scalar gain_B = 1.0f - 2.0f * yellow_offset;
    const int n_pixels = m_res_LF * m_res_LF;
    
    MatrixType graded = m_LF_model;
    RowVectorX LF_1 = m_LF_model.row(1);
    RowVectorX LF_1_graded = LF_1;

    // Full yellow grading
    for (int i = 0; i < n_pixels; ++i)
    { 
        LF_1_graded[3 * i] *= gain_R;
        LF_1_graded[3 * i + 1] *= gain_G;
        LF_1_graded[3 * i + 2] *= gain_B;
    }

    // Masked yellow grading
    const RowVectorX one = VectorX::Ones(3 * n_pixels).transpose();
    LF_1 = LF_1.cwiseProduct(one - m_yellow_mask) + LF_1_graded.cwiseProduct(m_yellow_mask);
    graded.row(1) = LF_1;
    
    return graded;
}

SLModel::ImageType SLModel::synthesize_neutral_LF(const Vector2& v, const float yellow_offset) const
{
    const MatrixType graded_LF_model = yellow_graded_LF_model(yellow_offset);
    const VectorX texture_LF_c1 = graded_LF_model.transpose() * Vector3(Scalar(1), v[0], v[1]);
    const auto texture_LF = detail::ReshapeSquare<Scalar>(texture_LF_c1);
    return texture_LF;
}

SLModel::Vector2 SLModel::ProjectSkinTone(const Vector3& skin_tone) const
{
    const auto T = m_skin_tones_pca.T();
    const auto T_pseudoinv = (T.transpose() * T).inverse() * T.transpose();
    return T_pseudoinv * (skin_tone - m_skin_tones_pca.Mu());
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

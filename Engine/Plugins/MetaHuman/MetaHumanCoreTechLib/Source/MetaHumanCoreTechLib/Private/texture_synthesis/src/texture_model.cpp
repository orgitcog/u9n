// Copyright Epic Games, Inc. All Rights Reserved.

#include <ts/texture_model.h>
#include <ts/utils/ts_utils.h>
#include <ts/ResizeHelper.h>


#if defined(TS_SSE_SUPPORT_RESIZE)
#define TS_SSE_SUPPORT_TS_MODEL
#endif // TS_SSE_SUPPORT

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

bool TextureModel::VersionCheck()
{
    const size_t dash_pos = m_params.ts_version.find('-');
    if (dash_pos == std::string::npos) 
    {
        return false;
    }
    if (m_params.ts_version.substr(0, dash_pos) != m_requiredTsModelVersion)
    {
        return false;
    }
    return true;
}

bool TextureModel::Load(const std::string& ts_params_path, IModel_Data_Provider& modelDataProvider)
{
    // TS parameters
    if (!ts::detail::LoadTSParams(ts_params_path, m_params, m_characterParams))
    {
        return false;
    };

    // If the local model has an incorrect version, fail initalization
    if (!VersionCheck())
    {
        return false;
    }

    // v1 ranges
    Model_Data v1_ranges_data = modelDataProvider.load(Data_Type::v1_ranges);
    m_v1Ranges = detail::ModelDataToEigenMap<float>(v1_ranges_data);
    
    // SL Model
    m_slModel = SLModel(m_params);
    m_slModel.Load(modelDataProvider);

    return IsValid();
}

bool TextureModel::IsValid() const
{
    if (!m_slModel.is_valid())
    {
        LOG_ERROR("SL model is invalid");
        return false;
    }
    return true;
}

bool TextureModel::DirectSelection(Eigen::Vector4<uint8_t>* const out_data,
                                   const SynthesizeParams& params,
                                   IModel_Data_Provider& modelDataProvider,
                                   TITAN_NAMESPACE::TaskThreadPool* task_thread_pool) const
{
    // Direct selection must not be used for albedos
    if (params.texture_type == TextureType::ALBEDO)
        return false;

    const std::pair<Eigen::Vector3f, Eigen::Vector3f> noop_range{Eigen::Vector3f::Zero(), Eigen::Vector3f::Ones()};
    const std::pair<Eigen::Vector3f, Eigen::Vector3f> full_range{-Eigen::Vector3f::Ones(), Eigen::Vector3f::Ones()};

    Model_Data data = modelDataProvider.load(params.texture_type, Frequency::HF, params.map_id, params.HF_index);

    const int resolution_HF = data.cols();
    CARBON_ASSERT(resolution_HF == params.resolution, "Incorrect model resolution");

    ImageType texture;
    if (params.map_id == 0)
    {
        // data contains the full neutral normal/cavity, which is what we want. The range is already correct.
        texture = detail::ModelDataToImageType(data, noop_range, task_thread_pool);
    }
    else 
    {
        // We want an animated normal, either delta or full texture
        // The deltas are stored in the 0-1 range, so we need to restore them to -1, 1
        ImageType delta = detail::ModelDataToImageType(data, full_range, task_thread_pool);
        texture.resize(resolution_HF, resolution_HF);
        if (params.animated_delta)
        {
            // Scale and offset matching Nicolas' formula for exporting a normal delta
            const float scale = 0.5f;
            const Eigen::Vector3f offset(0.5f, 0.5f, 0.5f);
            auto scale_and_offset = [&](int start, int end) {
                for (int r = start; r < end; ++r) {
                    for (int c = 0; c < resolution_HF; ++c) {
                        texture(r, c) = scale * delta(r, c) + offset;
                    }
                }
            };
            if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_HF, scale_and_offset);
            else scale_and_offset(0, resolution_HF);
        }
        else
        {
            Model_Data neutralData = modelDataProvider.load(params.texture_type, Frequency::HF, /*map_i=*/0, params.HF_index);
            ImageType neutral = detail::ModelDataToImageType(neutralData, noop_range, task_thread_pool);
            
            // Add neutral and delta to obtain a full animated map
            auto add_neutral_and_delta = [&](int start, int end) {
                for (int r = start; r < end; ++r) {
                    for (int c = 0; c < resolution_HF; ++c) {
                        texture(r, c) = neutral(r, c) + delta(r, c);
                    }
                }
            };
            if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_HF, add_neutral_and_delta);
            else add_neutral_and_delta(0, resolution_HF);
        }
    }

    // Clip, set range, and copy to output
    Eigen::Map<Eigen::Matrix<Eigen::Vector4<uint8_t>, -1, -1, Eigen::RowMajor>> texture_BGRA(out_data, resolution_HF, resolution_HF);
    auto finalize_synthesis = [&](int start, int end) {
        for (int r = start; r < end; ++r) {
            for (int c = 0; c < resolution_HF; ++c) {
                Eigen::Vector3f value = (texture(r, c).array()).min(1.0f).max(0.0f) * 255.0f;
                texture_BGRA(r, c)[0] = static_cast<uint8_t>(value[2]);
                texture_BGRA(r, c)[1] = static_cast<uint8_t>(value[1]);
                texture_BGRA(r, c)[2] = static_cast<uint8_t>(value[0]);
                texture_BGRA(r, c)[3] = static_cast<uint8_t>(255);
            }
        }
    };
    if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_HF, finalize_synthesis);
    else finalize_synthesis(0, resolution_HF);

    return true;
}

bool TextureModel::SynthesizeAlbedoAnimatedDelta(Eigen::Vector4<uint8_t>* const out_data,
                                                 const SynthesizeParams& params,
                                                 IModel_Data_Provider& modelDataProvider,
                                                 TITAN_NAMESPACE::TaskThreadPool* task_thread_pool) const
{
    // Synthesis is for animated albedos only, and only if animated delta is requested
    if (params.texture_type != TextureType::ALBEDO || params.map_id == 0 || !params.animated_delta)
        return false;

    const int HF_i = params.HF_index;

    // Parameters
    const float v_ui_0 = params.v_ui[0];
    const float v_ui_chr_0 = m_characterParams[HF_i].v_ui_chr_0;
    const float gain_LF = detail::InterpolatePiecewise(m_characterParams[HF_i].gain_LF, v_ui_0, v_ui_chr_0, 1.0f);
    const float gain_HF = detail::InterpolatePiecewise(m_characterParams[HF_i].gain_HF, v_ui_0, v_ui_chr_0, 1.0f);

    // Load data
    Model_Data deltaDataLF = modelDataProvider.load(params.texture_type, Frequency::LF, params.map_id, HF_i);
    Model_Data deltaDataHF = modelDataProvider.load(params.texture_type, Frequency::HF, params.map_id, HF_i);

    // Resolution check
    const int resolution_HF = deltaDataHF.cols();
    CARBON_ASSERT(resolution_HF == params.resolution, "Incorrect model resolution");

    // Convert to ImageType
    const auto range_LF = m_characterParams[HF_i].GetLFColourRange(params.map_id);
    const auto range_HF = m_characterParams[HF_i].GetHFColourRange(resolution_HF, params.map_id);
    ImageType deltaLF = detail::ModelDataToImageType(deltaDataLF, range_LF, task_thread_pool);
    ImageType deltaHF = detail::ModelDataToImageType(deltaDataHF, range_HF, task_thread_pool);
    
    // Synthesis
    ImageType texture;
    texture.resize(resolution_HF, resolution_HF);
    deltaLF = detail::ResizeImage(deltaLF, resolution_HF, Vector3::Zero(), task_thread_pool);
    auto animated_synthesis = [&](int start, int end) {
        for (int r = start; r < end; ++r) {
            for (int c = 0; c < resolution_HF; ++c) {
                texture(r, c) = gain_LF * deltaLF(r, c) + gain_HF * deltaHF(r, c);
            }
        }
    };
    if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_HF, animated_synthesis);
    else animated_synthesis(0, resolution_HF);

    // Apply Nicolas' formula for exported animated albedo delta, clip, set 255 range, and copy to output
    Eigen::Map<Eigen::Matrix<Eigen::Vector4<uint8_t>, -1, -1, Eigen::RowMajor>> texture_BGRA(out_data, resolution_HF, resolution_HF);
    auto finalize_synthesis = [&](int start, int end) {
        for (int r = start; r < end; ++r) {
            for (int c = 0; c < resolution_HF; ++c) {
                Eigen::Vector3f value = (texture(r, c).array() * 2.0f + 0.5f).min(1.0f).max(0.0f) * 255.0f;
                texture_BGRA(r, c)[0] = static_cast<uint8_t>(value[2]);
                texture_BGRA(r, c)[1] = static_cast<uint8_t>(value[1]);
                texture_BGRA(r, c)[2] = static_cast<uint8_t>(value[0]);
                texture_BGRA(r, c)[3] = static_cast<uint8_t>(255);
            }
        }
    };
    if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_HF, finalize_synthesis);
    else finalize_synthesis(0, resolution_HF);

    return true;
}

bool TextureModel::SynthesizeAlbedo(Eigen::Vector4<uint8_t>* const out_data,
                                    const SynthesizeParams& params,
                                    IModel_Data_Provider& modelDataProvider,
                                    TITAN_NAMESPACE::TaskThreadPool* task_thread_pool) const
{
    // Synthesis is for albedo maps only
    if (params.texture_type != TextureType::ALBEDO)
        return false;

    const float v_ui_0 = params.v_ui[0];
    const int HF_i = params.HF_index;
    
    const float v_ui_chr_0 = m_characterParams[HF_i].v_ui_chr_0;
    const float gain_LF = detail::InterpolatePiecewise(m_characterParams[HF_i].gain_LF, v_ui_0, v_ui_chr_0, 1.0f);
    const float gain_HF = detail::InterpolatePiecewise(m_characterParams[HF_i].gain_HF, v_ui_0, v_ui_chr_0, 1.0f);
    const float redness_HF = detail::InterpolatePiecewise(m_characterParams[HF_i].redness_HF, v_ui_0, v_ui_chr_0, 0.0f);
    const float saturation_HF = detail::InterpolatePiecewise(m_characterParams[HF_i].saturation_HF, v_ui_0, v_ui_chr_0, 1.0f);
    const float yellowness_LF = detail::InterpolatePiecewise(m_characterParams[HF_i].yellowness_LF, v_ui_0, v_ui_chr_0, 0.0f);

    // load data
    Model_Data modelData = modelDataProvider.load(params.texture_type, Frequency::HF, params.map_id, HF_i);
    Model_Data modelDataLF = modelDataProvider.load(params.texture_type, Frequency::LF, params.map_id, HF_i);
    Model_Data neutralModelData = modelData;
    Model_Data neutralModelDataLF = modelDataLF;
    if (params.map_id > 0)
    {
        neutralModelData = modelDataProvider.load(params.texture_type, Frequency::HF, /*map_i=*/0, HF_i);
        neutralModelDataLF = modelDataProvider.load(params.texture_type, Frequency::LF, /*map_i=*/0, HF_i);
    }

    // temporary images (should be preallocated for better performance)
    ImageType LF;
    ImageType LF_complement;
    ImageType LF_texture;
    ImageType texture;
    ImageType HF_neutral;
    ImageType LF_delta;
    ImageType HF_delta;
    ImageType LF_delta_upscaled;

    // Parameters
    const Vector2 v = VuiToV(params.v_ui);
    const int resolution_HF = neutralModelData.cols();
    const int resolution_LF = neutralModelDataLF.cols();

    CARBON_ASSERT(resolution_HF == params.resolution, "Incorrect model resolution");

    // LF images
    LF = m_slModel.synthesize_neutral_LF(v, yellowness_LF);
    const auto range_LF_complement = m_characterParams.at(params.HF_index).GetLFColourRange(/*map_id=*/0);
    LF_complement = detail::ModelDataToImageType(neutralModelDataLF, range_LF_complement, task_thread_pool);
    
    LF_texture.resize(resolution_LF, resolution_LF);
    auto LF_synthesis = [&](int start, int end) {
        for (int r = start; r < end; ++r) {
            for (int c = 0; c < resolution_LF; ++c) {
                LF_texture(r, c) = LF(r, c) + gain_LF * LF_complement(r, c);
            }
        }
    };
    if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_LF, LF_synthesis);
    else LF_synthesis(0, resolution_LF);

    // Offset applied during LF resize, it can be used to apply the skin tone
    Vector3 offset = Vector3::Zero();
    if (params.texture_type == TextureType::ALBEDO)
    {
        offset = m_slModel.skin_tone(v);
    }

    // LF resize
    texture = detail::ResizeImage(LF_texture, resolution_HF, offset, task_thread_pool);
    
    // HF neutral
    const auto range_HF_neutral = m_characterParams.at(params.HF_index).GetHFColourRange(resolution_HF, /*map_id=*/0);
    HF_neutral = detail::ModelDataToImageType(neutralModelData, range_HF_neutral, task_thread_pool);

    // Grade HF neutral
    GradeHF(HF_neutral, redness_HF, saturation_HF, task_thread_pool);

    // Complete NEUTRAL: Add LF and HF
    auto neutral_synthesis = [&](int start, int end) {
        const int size = 3 * (int)texture.cols();
        Eigen::Map<Eigen::VectorXf> texture_flattened((float*)texture.data(), texture.size() * 3);
        Eigen::Map<const Eigen::VectorXf> HF_neutral_flattened((const float*)HF_neutral.data(), HF_neutral.size() * 3);
        texture_flattened.segment(start * size, (end - start) * size) += gain_HF * HF_neutral_flattened.segment(start * size, (end - start) * size);
    };
    if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_HF, neutral_synthesis, 8); // TODO: use better hint based on number of cores?
    else neutral_synthesis(0, resolution_HF);

    // If an animated map is requested, add delta
    if (params.map_id > 0)
    {
        // LF animated delta
        const auto range_LF_delta = m_characterParams.at(params.HF_index).GetLFColourRange(params.map_id);
        LF_delta = detail::ModelDataToImageType(modelDataLF, range_LF_delta, task_thread_pool);
        LF_delta = detail::ResizeImage(LF_delta, resolution_HF, Vector3::Zero(), task_thread_pool);

        // HF animated delta
        const auto range_HF_delta = m_characterParams.at(params.HF_index).GetHFColourRange(resolution_HF, params.map_id);
        HF_delta = detail::ModelDataToImageType(modelData, range_HF_delta, task_thread_pool);

        // Complete ANIMATED: Add deltas
        auto animated_synthesis = [&](int start, int end) {
            for (int r = start; r < end; ++r) {
                for (int c = 0; c < resolution_HF; ++c) {
                    texture(r, c) += gain_LF * LF_delta(r, c) + gain_HF * HF_delta(r, c);
                }
            }
        };
        if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution_HF, animated_synthesis);
        else animated_synthesis(0, resolution_HF);
    }

    // Clip, set range, and copy to output
    Eigen::Map<Eigen::Matrix<Eigen::Vector4<uint8_t>, -1, -1, Eigen::RowMajor>> texture_BGRA(out_data, params.resolution, params.resolution);
    auto finalize_synthesis = [&](int start, int end) {
#ifdef TS_SSE_SUPPORT_TS_MODEL
        if ((texture.cols() % 4) == 0)
        {
            std::uint8_t* outPtr = (std::uint8_t*)(texture_BGRA.data() + start * texture_BGRA.cols());
            const __m128* inPtr = (const __m128*)(texture.data() + start * texture.cols());
            int outArray[12];
            int pixelsToProcess = (int)texture_BGRA.cols() * (end - start);
            for (int i = 0; i < pixelsToProcess / 4; ++i)
            {
                for (int k = 0; k < 3; ++k)
                {
                    __m128 result = _mm_min_ps(inPtr[3 * i + k], _mm_set1_ps(1.0f));
                    result = _mm_max_ps(result, _mm_setzero_ps());
                    result = _mm_mul_ps(result, _mm_set1_ps(255.0f));
                    __m128i out = _mm_cvttps_epi32(result);
                    _mm_store_si128((__m128i *)(outArray + k * 4), out);
                }
                for (int j = 0; j < 4; ++j)
                {
                    outPtr[4 * (4 * i + j) + 2] = (std::uint8_t)outArray[3 * j + 0];
                    outPtr[4 * (4 * i + j) + 1] = (std::uint8_t)outArray[3 * j + 1];
                    outPtr[4 * (4 * i + j) + 0] = (std::uint8_t)outArray[3 * j + 2];
                    outPtr[4 * (4 * i + j) + 3] = 255;
                }
            }
        }
        else
#endif // TS_SSE_SUPPORT_TS_MODEL
        {
            for (int r = start; r < end; ++r) {
                for (int c = 0; c < params.resolution; ++c) {
                    Eigen::Vector3f value = (texture(r, c).array()).min(1.0f).max(0.0f) * 255.0f;
                    texture_BGRA(r, c)[0] = static_cast<uint8_t>(value[2]);
                    texture_BGRA(r, c)[1] = static_cast<uint8_t>(value[1]);
                    texture_BGRA(r, c)[2] = static_cast<uint8_t>(value[0]);
                    texture_BGRA(r, c)[3] = static_cast<uint8_t>(255);
                }
            }
        }
    };
    if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(params.resolution, finalize_synthesis);
    else finalize_synthesis(0, params.resolution);
    
    return true;
}

bool TextureModel::GetTexture(Eigen::Vector4<uint8_t>* const out_data,
                              const SynthesizeParams& params,
                              IModel_Data_Provider& modelDataProvider,
                              TITAN_NAMESPACE::TaskThreadPool* task_thread_pool) const
{
    if (params.HF_index < 0 || params.HF_index >= m_params.n_HF_index)
        return false;
    if (params.map_id < 0 || params.map_id >= m_params.n_textures_of_type_per_chr.at(params.texture_type))
        return false;
    
    if (params.texture_type != TextureType::ALBEDO)
        return DirectSelection(out_data, params, modelDataProvider, task_thread_pool);
    else
        if (params.map_id > 0 && params.animated_delta)
            return SynthesizeAlbedoAnimatedDelta(out_data, params, modelDataProvider, task_thread_pool);
        else
            return SynthesizeAlbedo(out_data, params, modelDataProvider, task_thread_pool);
        
}

TextureModel::Vector2 TextureModel::VuiToV(const Vector2& v_ui) const
{
    CARBON_ASSERT(v_ui[0] >= 0.0f && v_ui[0] <= 1.0f, "v_ui values out of bounds");
    CARBON_ASSERT(v_ui[1] >= 0.0f && v_ui[1] <= 1.0f, "vui values out of bounds");

    // apply axis transform first
    const Vector2 v_ui_transformed = ApplyAxisTransform(v_ui, m_params.v_ui_axis_transform);

    const float v0_min = m_params.v0_range_min;
    const float v0_max = m_params.v0_range_max;

    // Apply full range to v0
    const float v0 = v0_min + v_ui_transformed[0] * (v0_max - v0_min);

    // Apply map range to v1: 1) Find closest v0 in the map and corresponding limits for v1
    const auto map_length = m_v1Ranges.rows();
    const int v0_i = int((map_length - 1.0) * v_ui_transformed[0]);
    const float v1_min = m_v1Ranges(v0_i, 0);
    const float v1_max = m_v1Ranges(v0_i, 1);

    // 2) Apply range to v1
    const float v1 = v1_min + v_ui_transformed[1] * (v1_max - v1_min);

    return Vector2(v0, v1);
}

TextureModel::Vector2 TextureModel::VToVui(const Vector2& v) const
{
    // calculate v_ui_0
    const float v0_min = m_params.v0_range_min;
    const float v0_max = m_params.v0_range_max;
    const float v_ui_0 = std::clamp((v(0) - v0_min) / (v0_max - v0_min), 0.0f, 1.0f);

    // calculate v_ui_1
    const int n_points = static_cast<int>(m_v1Ranges.rows());
    const int closest_v0_i = static_cast<int>((n_points - 1) * v_ui_0);
    const float v1_min = m_v1Ranges(closest_v0_i, 0);
    const float v1_max = m_v1Ranges(closest_v0_i, 1);
    const float v_ui_1 = std::clamp((v(1) - v1_min) / (v1_max - v1_min), 0.0f, 1.0f);

    // apply axis transform last
    const Vector2 v_ui = ApplyAxisTransform(Vector2(v_ui_0, v_ui_1), m_params.v_ui_axis_transform);
    return v_ui;
}

TextureModel::Vector2 TextureModel::ApplyAxisTransform(const Vector2& v_ui, int vUiAxisTransform) const
{
    CARBON_ASSERT(v_ui[0] >= 0.0f && v_ui[0] <= 1.0f, "v_ui values out of bounds");
    CARBON_ASSERT(v_ui[1] >= 0.0f && v_ui[1] <= 1.0f, "v_ui values out of bounds");

    Vector2 vuitransformed = v_ui;
    switch (vUiAxisTransform) {
        case 0: // No transform
            break;
        case 1: // Flip horizontally
            vuitransformed[0] = 1.0f - vuitransformed[0];
            break;
        case 2: // Flip vertically
            vuitransformed[1] = 1.0f - vuitransformed[1];
            break;
        case 3: // Flip horizontally and vertically
            vuitransformed[0] = 1.0f - vuitransformed[0];
            vuitransformed[1] = 1.0f - vuitransformed[1];
            break;
        default:
            CARBON_CRITICAL("Invalid axis transform");
    }

    return vuitransformed;
}

TextureModel::Vector2 TextureModel::ProjectSkinTone(const Vector3& skin_tone) const
{
    return m_slModel.ProjectSkinTone(skin_tone);
}

TextureModel::Vector3 TextureModel::BodyAlbedoGain(const Vector2& vui) const
{
    const float x = vui(0);

	// For color map V1
	const float curve1_r = 111.976f * x * x - 141.994f * x + 57.0f;
	const float curve1_g = 55.9888f * x * x - 69.9972f * x + 28.0f;
	const float curve1_b = 39.992f * x * x - 49.998f * x + 20.0f;
	Vector3 gain1 = Vector3(curve1_r, curve1_g, curve1_b);

	// For color map V2
	const float curve2_r = 56.0f * x * x - 130.0f * x + 81.0f;;
	const float curve2_g = 16.0f * x * x - 40.0f * x + 26.0f;
	const float curve2_b = -8.0f * x * x + 6.0f * x + 4.0f;
	Vector3 gain2 = Vector3(curve2_r, curve2_g, curve2_b);

	Vector3 result;
	result = x < 0.5 ? gain1 : gain2;

	return result;
}

void TextureModel::GradeHF(ImageType& HF,
                           const float redness_HF,
                           const float saturation_HF,
                           TITAN_NAMESPACE::TaskThreadPool* task_thread_pool) const
{
    const int resolution = int(HF.cols());
    const Scalar div3{1.0 / 3.0};
    auto grade_hf = [&](int start, int end) {
        for (int r = start; r < end; ++r) {
            for (int c = 0; c < resolution; ++c) {
                // Luminance (average-based)
                const Eigen::Vector3<Scalar> lum = Eigen::Vector3<Scalar>::Constant(HF(r, c).sum() * div3);
                
                // Saturation
                HF(r, c) = lum + (HF(r, c) - lum) * saturation_HF;
                
                // Redness
                HF(r, c)[0] *= (1.0f - redness_HF);
            }
        }
    };
    if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution, grade_hf);
    else grade_hf(0, resolution);   
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)
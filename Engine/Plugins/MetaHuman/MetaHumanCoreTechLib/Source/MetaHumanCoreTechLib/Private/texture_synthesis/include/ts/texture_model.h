// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/io/JsonIO.h>
#include <carbon/utils/TaskThreadPool.h>
#include <ts/pca.h>
#include <ts/sl_model.h>
#include <ts/model_data_provider_interface.h>
#include <ts/ts_types.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

class TextureModel
{
public:
    using Scalar = SLModel::Scalar;
    using Vector2 = SLModel::Vector2;
    using Vector3 = SLModel::Vector3;
    using VectorX = SLModel::VectorX;
    using MatrixType = SLModel::MatrixType;
    using ImageType = SLModel::ImageType;

public:
    // load the required data and initialize the texture model
    bool Load(const std::string& ts_params_path, IModel_Data_Provider& modelDataProvider);

    bool IsValid() const;
    std::string LocalModelVersion() const { return m_params.ts_version; }
    std::string RequiredModelVersion() const { return m_requiredTsModelVersion; }
    const TextureModelParams& Parameters() { return m_params; }
    Vector2 ApplyAxisTransform(const Vector2& vui, int vuiAxisTransform) const;

    // Parameters for synthezing texture with the model
    struct SynthesizeParams
    {
        TextureType texture_type;   // The texture to synthesize, current selection between ALBEDO/NORMAL/CAVITY
        Vector2 v_ui;               // Vector2 containing values in [0, 1], representing the UI coordinates of the skin tone selection
        int HF_index;               // Index for the HF model data
        int map_id;                 // 0: Neutral map; 1, 2, 3: Animated maps
        bool animated_delta;        // An animated delta is returned instead of a full texture. Ignored if map_id == 0
        int resolution;             // Expected image resolution. An error is thrown if this doesn't match the model resolution
    };

    /*
    * Generates the texture based on the input params
    * See more the SynthesizeParams & IModel_Data_Provider properties for more details
    * 
    * outData should be preallocated Eigen buffer matching the size of the model data providers
    * after a successfully execution, it will contain the returned image in BGRA 8 bits per color
    * optionally a TaskThreadPool can be passed to use multi threading, otherwise all operations will execute on the caller's thread
    */
    bool GetTexture(Eigen::Vector4<uint8_t>* const outData, 
                    const SynthesizeParams& params,
                    IModel_Data_Provider& modelDataProvider,
                    TITAN_NAMESPACE::TaskThreadPool* taskThreadPool = nullptr) const;

    bool SynthesizeAlbedo(Eigen::Vector4<uint8_t>* const outData, 
                          const SynthesizeParams& params,
                          IModel_Data_Provider& modelDataProvider,
                          TITAN_NAMESPACE::TaskThreadPool* taskThreadPool = nullptr) const;

    bool SynthesizeAlbedoAnimatedDelta(Eigen::Vector4<uint8_t>* const out_data,
                                       const SynthesizeParams& params,
                                       IModel_Data_Provider& modelDataProvider,
                                       TITAN_NAMESPACE::TaskThreadPool* task_thread_pool) const;

    bool DirectSelection(Eigen::Vector4<uint8_t>* const outData, 
                         const SynthesizeParams& params,
                         IModel_Data_Provider& modelDataProvider,
                         TITAN_NAMESPACE::TaskThreadPool* taskThreadPool = nullptr) const;

    Vector3 SkinTone(const Vector2& v) const { return m_slModel.skin_tone(v); }
    Vector2 VuiToV(const Vector2& vui) const;
    Vector2 VToVui(const Vector2& v) const;
    Vector2 ProjectSkinTone(const Vector3& skin_tone) const;
    Vector3 BodyAlbedoGain(const Vector2& vui) const;

private:
    bool VersionCheck();
    void GradeHF(ImageType& HF,
                 const float redness_HF,
                 const float saturation_HF,
                 TITAN_NAMESPACE::TaskThreadPool* taskThreadPool = nullptr) const;

    const std::string m_requiredTsModelVersion{"1.3"};
    TextureModelParams m_params;
    std::vector<CharacterParams> m_characterParams;
    SLModel m_slModel{};
    MatrixType m_v1Ranges{};
};
 
CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

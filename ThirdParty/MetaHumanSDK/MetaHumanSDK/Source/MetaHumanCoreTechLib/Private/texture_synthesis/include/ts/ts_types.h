// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <carbon/common/Defs.h>
#include <carbon/io/JsonIO.h>
#include <string>
#include <map>
#include <vector>
#include <array>
#include <Eigen/Eigen>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

enum Data_Type : uint8_t
{
    LF_model = 0,
    pca_mu,
    pca_S,
    pca_T,
    v1_ranges,
    yellow_mask,
    Count
};

enum TextureType : uint8_t
{
    ALBEDO = 0,
    NORMAL = 1,
    CAVITY = 2
};

enum Frequency : uint8_t
{
    LF = 0,
    HF = 1
};

struct TextureModelParams
{
    std::string ts_version;
    int resolution_LF;
    int n_HF_index;
    std::map<TextureType, int> n_textures_of_type_per_chr;
    int v_ui_axis_transform;
    float v0_range_min;
    float v0_range_max;

    TextureModelParams(const JsonElement& j);
    TextureModelParams() {}

    static constexpr TextureType ALL_TEXTURE_TYPES[3] = { TextureType::ALBEDO, TextureType::NORMAL, TextureType::CAVITY }; 
    static const char* TextureTypeToStr(TextureType texture_type);
    static const char* FrequencyToStr(Frequency frequency);
    static TextureType TextureStrToTextureType(const std::string& texture_str);
};

struct CharacterParams
{
    std::array<float, 2> gain_LF;
    std::array<float, 2> gain_HF;
    std::array<float, 2> redness_HF;
    std::array<float, 2> saturation_HF;
    std::array<float, 2> yellowness_LF;
    float v_ui_chr_0;
    std::array<std::array<Eigen::Vector3f, 2>, 4> colour_ranges_LF;
    std::map<int, std::array<std::array<Eigen::Vector3f, 2>, 4>> colour_ranges_HF;

    CharacterParams(const JsonElement& j);
    std::pair<Eigen::Vector3f, Eigen::Vector3f> GetLFColourRange(const int map_i) const;
    std::pair<Eigen::Vector3f, Eigen::Vector3f> GetHFColourRange(const int res, const int map_i) const;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

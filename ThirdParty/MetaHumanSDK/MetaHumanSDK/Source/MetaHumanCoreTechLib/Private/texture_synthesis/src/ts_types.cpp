// Copyright Epic Games, Inc. All Rights Reserved.

#include <ts/ts_types.h>
#include "carbon/common/Log.h"

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

TextureModelParams::TextureModelParams(const JsonElement& j)
{
    ts_version = j["ts_version"].Get<std::string>();
    resolution_LF = j["resolution_LF"].Get<int>();
    n_HF_index = j["n_characters"].Get<int>();
    v_ui_axis_transform = j["v_ui_axis_transform"].Get<int>();

    // Parse texture type as string
    std::map<std::string, int> temp_texture_type_str = j["n_textures_of_type_per_chr"].Get<std::map<std::string, int>>();
    for (auto ntextures_of_type : temp_texture_type_str)
    {
        n_textures_of_type_per_chr.emplace(TextureModelParams::TextureStrToTextureType(ntextures_of_type.first), ntextures_of_type.second);
    }

    std::vector<std::string> v0_range_str = j["v0_range"].Get<std::vector<std::string>>();
    v0_range_min = std::stof(v0_range_str[0]);
    v0_range_max = std::stof(v0_range_str[1]);
}

const char* TextureModelParams::TextureTypeToStr(TextureType texture_type)
{
    // Order should match TextureType enum
    constexpr const char* texture_type_str_map[] = { "albedo", "normal", "cavity" };

    return texture_type_str_map[static_cast<uint8_t>(texture_type)];
}

const char* TextureModelParams::FrequencyToStr(Frequency frequency)
{
    // Order should match Frequency enum
    constexpr const char* frequency_str_map[] = { "LF", "HF" };

    return frequency_str_map[static_cast<uint8_t>(frequency)];
}

TextureType TextureModelParams::TextureStrToTextureType(const std::string& texture_str)
{
    if (texture_str == "albedo")
        return TextureType::ALBEDO;
    if (texture_str == "normal")
        return TextureType::NORMAL;
    if (texture_str == "cavity")
        return TextureType::CAVITY;

    LOG_ERROR("Unknown texture type");
    return TextureType::ALBEDO;
}

CharacterParams::CharacterParams(const JsonElement& j)
{
    gain_LF = { j["gain_LF"][0].Get<float>(), j["gain_LF"][1].Get<float>() };
    gain_HF = { j["gain_HF"][0].Get<float>(), j["gain_HF"][1].Get<float>() };
    redness_HF = { j["redness_HF"][0].Get<float>(), j["redness_HF"][1].Get<float>() };
    saturation_HF = { j["saturation_HF"][0].Get<float>(), j["saturation_HF"][1].Get<float>() };
    yellowness_LF = { j["yellowness_LF"][0].Get<float>(), j["yellowness_LF"][1].Get<float>() };
    v_ui_chr_0 = j["v_ui_chr_0"].Get<float>();

    // LF ranges
    for (int range_i = 0; range_i < 4; ++range_i)
    {
        for (int i = 0; i < 2; ++i)
        {
            for (int k = 0; k < 3; ++k)
            {
                const auto ranges = j["colour_ranges"]["LF"].Get<std::vector<std::vector<float>>>();
                colour_ranges_LF[range_i][i](k) = ranges[range_i][3 * i + k];
            }
        }
    }

    // HF ranges
    const auto& HF_map = j["colour_ranges"]["HF"].Map();
    for (const auto& [res_str, res_HF_ranges] : HF_map)
    {
        std::array<std::array<Eigen::Vector3f, 2>, 4> HF_range;
        for (int range_i = 0; range_i < 4; ++range_i)
        {
            for (int i = 0; i < 2; ++i)
            {
                for (int k = 0; k < 3; ++k)
                {
                    HF_range[range_i][i](k) = res_HF_ranges.Get<std::vector<std::vector<float>>>()[range_i][3 * i + k];
                }
            }
        }
        colour_ranges_HF[std::stoi(res_str)] = HF_range;
    }
}

std::pair<Eigen::Vector3f, Eigen::Vector3f> CharacterParams::GetLFColourRange(const int map_i) const
{
    const Eigen::Vector3f minima = colour_ranges_LF[map_i][0];
    const Eigen::Vector3f maxima = colour_ranges_LF[map_i][1];
    return std::pair{minima, maxima};
}

std::pair<Eigen::Vector3f, Eigen::Vector3f> CharacterParams::GetHFColourRange(const int res, const int map_i) const
{
    const Eigen::Vector3f minima = colour_ranges_HF.at(res)[map_i][0];
    const Eigen::Vector3f maxima = colour_ranges_HF.at(res)[map_i][1];
    return std::pair{minima, maxima};
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)
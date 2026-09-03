// Copyright Epic Games, Inc. All Rights Reserved.

#include <filesystem>
#include <ts/utils/ts_utils.h>
#include <ts/ResizeHelper.h>
#include <carbon/Math.h>
#include <carbon/io/Utils.h>
#include <carbon/utils/Timer.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

namespace detail
{
using ImageType = Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
using ImageTypeUint8 = Eigen::Matrix<uint8_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

ImageType ResizeImage(const ImageType& image, const int new_size, const Eigen::Vector3f& offset, TaskThreadPool* threadPool)
{
    if (image.cols() == new_size && offset == Eigen::Vector3f::Zero())
    {
        return image;
    }
    return ResizeImageCubic3f(image, new_size, new_size, offset, threadPool);
}

ImageType ModelDataToImageType(const Model_Data& model_data,
                               const std::pair<Eigen::Vector3f, Eigen::Vector3f>& range,
                               TITAN_NAMESPACE::TaskThreadPool* task_thread_pool)
{
    CARBON_ASSERT(model_data.word_size() == 1 || model_data.word_size() == 2, "Incorrect word size");
    if (model_data.word_size() == 1)
        return detail::UintToFp<uint8_t>(model_data.data<uint8_t>(), model_data.cols(), range, task_thread_pool);
    else
        return detail::UintToFp<uint16_t>(model_data.data<uint8_t>(), model_data.cols(), range, task_thread_pool);
}

bool HasFourTextureColourRangesFormat(const JsonElement& four_textures_ranges_json)
{
    if (!four_textures_ranges_json.IsArray()) { return false; }
    if (four_textures_ranges_json.Array().size() != 4) { return false; }
    for (int i = 0; i < 4; ++i )
    {
        const auto& texture_ranges_json = four_textures_ranges_json[i];
        if (!texture_ranges_json.IsArray()) { return false; }
        if (texture_ranges_json.Array().size() != 6) { return false; }
    }
    return true;
}

bool HasTextureModelParamsFormat(const JsonElement& model_json)
{
    if (!model_json.IsObject()) { return false; }
    
    // Check that all the model parameter keys are present
    const auto& model_map = model_json.Map();
    const std::vector<std::string> required_keys {"ts_version", "resolution_LF", "n_characters", "n_textures_of_type_per_chr", "v_ui_axis_transform", "v0_range"};
    for (const std::string& key : required_keys)
    {
        if (model_map.find(key) == model_map.end()) { return false; }
    }

    return true;
}

bool HasCharacterParamsFormat(const JsonElement& chr_json)
{
    // Check that all the character parameter keys are present
    if (!chr_json.IsObject()) { return false; }
    const auto& chr_map = chr_json.Map();
    const std::vector<std::string> chr_required_keys {"gain_LF", "gain_HF", "yellowness_LF", "redness_HF", "saturation_HF", "v_ui_chr_0", "colour_ranges"};
    for (const std::string& key : chr_required_keys)
    {
        if (chr_map.find(key) == chr_map.end()) { return false; }
    }

    // Check that the colour ranges are a map containing both LF and HF keys
    const auto& ranges_json = chr_json["colour_ranges"];
    if (!ranges_json.IsObject()) { return false; }    
    const auto& ranges_map = ranges_json.Map();
    const std::vector<std::string> ranges_required_keys {"LF", "HF"};
    for (const std::string& key : ranges_required_keys)
    {
        if (ranges_map.find(key) == ranges_map.end()) { return false; }
    }

    // Check that LF parameters are an array of 4 arrays of 6 elements each
    const auto& LF_json = ranges_json["LF"];
    if (!HasFourTextureColourRangesFormat(LF_json)) { return false; }

    // Check that the HF parameters are a map, and for each of its elements we have colour ranges for four textures
    const auto& HF_json = ranges_json["HF"];
    if (!HF_json.IsObject()) { return false; }
    for (const auto& [_unused_, HF_res_json] : HF_json.Map())
    {
        if (!HasFourTextureColourRangesFormat(HF_res_json)) { return false; }
    }

    return true;
}

bool LoadTSParams(const std::string& ts_params_path, 
                  TextureModelParams& model_params,
                  std::vector<CharacterParams>& characters_params)
{   
    if (!std::filesystem::exists(ts_params_path)) {
        return false;
    }
    const JsonElement ts_params_json = ReadJson(ReadFile(ts_params_path));
    if (!ts_params_json.IsObject()) { return false; }
    const auto& ts_params_map = ts_params_json.Map();
    if (ts_params_map.find("model") == ts_params_map.end()) { return false; }
    if (ts_params_map.find("characters") == ts_params_map.end()) { return false; }

    // TextureModel parameters
    const JsonElement& model_json = ts_params_json["model"];
    if (!HasTextureModelParamsFormat(model_json)) { return false; }
    model_params = TextureModelParams(model_json);
    const int n_characters = model_params.n_HF_index;

    // Characters parameters
    const JsonElement& characters_json = ts_params_json["characters"];
    if (!characters_json.IsObject()) { return false; }
    const auto& characters_map = characters_json.Map();

    characters_params.clear();
    characters_params.reserve(n_characters);
    for (int chr_i = 0; chr_i < n_characters; ++chr_i)
    {
        const std::string chr_str = "chr_" + zfill(std::to_string(chr_i + 1), 4);

        // Check that the character parameters are in the json
        if (characters_map.find(chr_str) == characters_map.end()) { return false; }

        const JsonElement& chr_json = characters_json[chr_str];
        if (!HasCharacterParamsFormat(chr_json)) { return false; }
        characters_params.push_back(CharacterParams(chr_json));
    }
    return true;
}

float Interpolate(const std::array<float, 2>& extremes, const float x)
{
    return (1.0f - x) * extremes[0] + x * extremes[1];
}

float InterpolatePiecewise(const std::array<float, 2>& extremes, const float x, const float v_ui_chr_0, const float param_baseline)
{
    if (AreFloatsEqual(x, v_ui_chr_0))
    {
        return param_baseline;
    }
    else if (AreFloatsEqual(x, 0.0f))
    {
        return extremes[0];
    }
    else if (AreFloatsEqual(x, 1.0f))
    {
        return extremes[1];
    }
    else if (x < v_ui_chr_0)
    {
        const std::array<float, 2> _extremes {extremes[0], param_baseline};
        const float _x = x / v_ui_chr_0;
        return Interpolate(_extremes, _x);
    }
    else
    {
        const std::array<float, 2> _extremes {param_baseline, extremes[1]};
        const float _x = (x - v_ui_chr_0) / (1.0f - v_ui_chr_0);
        return Interpolate(_extremes, _x);
    }
}

std::string zfill(const std::string& input, size_t width)
{
    if (input.length() >= width) {
        return input;
    }
    return std::string(width - input.length(), '0') + input;
}

bool AreFloatsEqual(const float a, const float b) { 
    return std::fabs(a - b) < std::numeric_limits<float>::epsilon();
}

} // namespace detail

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

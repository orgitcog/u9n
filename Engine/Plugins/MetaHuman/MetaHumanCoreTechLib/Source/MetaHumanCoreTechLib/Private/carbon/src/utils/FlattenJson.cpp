// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/utils/FlattenJson.h>
#include <carbon/Common.h>
#include <carbon/utils/Base64.h>
#include <carbon/utils/StringUtils.h>
#include <carbon/io/Utils.h>
#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

JsonElement FlattenJson(const JsonElement& jsonIn, const std::string& baseDir)
{
    auto makeAbsolute = [&](const std::string& filename)
        {
            if (std::filesystem::path(filename).is_relative())
            {
                return baseDir + "/" + filename;
            }
            else
            {
                return filename;
            }
        };

    switch (jsonIn.Type())
    {
        default:
        case JsonElement::JsonType::False:
        case JsonElement::JsonType::Null:
        case JsonElement::JsonType::True:
        {
            return JsonElement(jsonIn.Type());
        }
        case JsonElement::JsonType::Object:
        {
            JsonElement out(JsonElement::JsonType::Object);
            for (const auto& [key, value] : jsonIn.Map())
            {
                out.Insert(key, FlattenJson(value, baseDir));
            }
            return out;
        }
        case JsonElement::JsonType::Array:
        {
            JsonElement out(JsonElement::JsonType::Array);
            for (const auto& value : jsonIn.Array())
            {
                out.Append(FlattenJson(value, baseDir));
            }
            return out;
        }
        case JsonElement::JsonType::Int:
        {
            return JsonElement(jsonIn.Value<int>());
        }
        case JsonElement::JsonType::Double:
        {
            return JsonElement(jsonIn.Value<double>());
        }
        case JsonElement::JsonType::String:
        {
            const std::string possibleFilename = makeAbsolute(jsonIn.String());
            if (!jsonIn.String().empty() && std::filesystem::exists(possibleFilename))
            {
                const std::string fileContent = ReadFile(possibleFilename);
                if (StringEndsWith(possibleFilename, ".json"))
                {
                    const std::string newBaseDir = std::filesystem::absolute(std::filesystem::path(possibleFilename)).parent_path().string();
                    // try to load the file content as json file
                    return FlattenJson(ReadJson(fileContent), newBaseDir);
                }
                else
                {
                    // convert the file content as binary
                    LOG_INFO("converting content of {} to base64", possibleFilename);
                    return JsonElement(Base64Encode((const unsigned char*)fileContent.data(), fileContent.size()));
                }
            }
            else
            {
                return JsonElement(jsonIn.String());
            }
        }
    }
}

JsonElement FlattenJson(const std::string& fileName)
{
    const std::string baseDir = std::filesystem::absolute(std::filesystem::path(fileName)).parent_path().string();
    JsonElement json = ReadJson(ReadFile(fileName));
    return FlattenJson(json, baseDir);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

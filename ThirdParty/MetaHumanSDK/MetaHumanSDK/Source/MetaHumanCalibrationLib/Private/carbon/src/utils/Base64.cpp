// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/utils/Base64.h>
#include <carbon/Common.h>

#include <cstdint>
#include <map>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

namespace
{

// 64 characters to use for base 64 encoding
static const std::string base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// get the map to decode from base 64 string character to uchars which can be combined to give the original binary data
static std::map<char, unsigned char> decodingLookup = []()
    {
        std::map<char, unsigned char> out;
        for (unsigned i = 0; i < 64; i++)
        {
            out[base64Chars[i]] = (unsigned char)i;
        }
        return out;
    }();

} // namespace

std::string Base64Encode(const unsigned char* data, size_t length)
{
    const size_t outputSize = 4 * ((length + 2) / 3);
    std::string encodedString(outputSize, ' ');

    // do the encoding
    size_t j = 0;
    for (size_t i = 0; i < length;)
    {
        // extract 3 bytes from the data
        unsigned char threeBytes[3] = { 0, 0, 0 };
        for (unsigned k = 0; k < 3; k++)
        {
            if (i < length)
            {
                threeBytes[k] = data[i++];
            }
        }

        // convert the 3 bytes into 4 characters from the set of 64
        const unsigned triple = (threeBytes[0] << 0x10) + (threeBytes[1] << 0x08) + threeBytes[2];
        for (unsigned k = 0; k < 4; k++)
        {
            encodedString[j++] = base64Chars[(triple >> (3 - k) * 6) & 0x3F];
        }
    }

    // add any required padding
    if (length % 3 == 1)
    {
        encodedString[encodedString.size() - 2] = '=';
        encodedString[encodedString.size() - 1] = '=';
    }
    else if (length % 3 == 2)
    {
        encodedString[encodedString.size() - 1] = '=';
    }

    return encodedString;
}

bool Base64Decode(std::string const& encodedString, std::vector<unsigned char>& decodedData)
{
    // encoded string must be a multiple of 4
    if (encodedString.size() % 4 != 0)
    {
        return false;
    }

    // figure out the length of the output data
    size_t outputDataLength = encodedString.size() / 4 * 3;

    if (encodedString[encodedString.size() - 1] == '=')
    {
        outputDataLength--;
    }
    if (encodedString[encodedString.size() - 2] == '=')
    {
        outputDataLength--;
    }

    decodedData.resize(outputDataLength);

    for (size_t i = 0, j = 0; i < encodedString.size();)
    {
        // get 4 characters each corresponding to six bits in the original data
        unsigned char sixbits[4] = { 0, 0, 0, 0 };
        for (unsigned k = 0; k < 4; k++)
        {
            if (encodedString[i] == '=')
            {
                sixbits[k] = 0 & i++;
            }
            else
            {
                // check we have a valid encoded character
                auto it = decodingLookup.find(encodedString[i]);
                if (it == decodingLookup.end())
                {
                    return false;
                }
                sixbits[k] = it->second;
                i++;
            }
        }

        // convert the 4 lots of six bits back into 3 bytes
        unsigned threeBytes = (sixbits[0] << 3 * 6)
            + (sixbits[1] << 2 * 6)
            + (sixbits[2] << 1 * 6)
            + (sixbits[3] << 0 * 6);

        for (unsigned k = 0; k < 3; k++)
        {
            if (j < outputDataLength)
            {
                decodedData[j++] = (threeBytes >> (2 - k) * 8) & 0xFF;
            }
        }
    }

    return true;
}

std::string Base64Decode(std::string const& encodedString)
{
    std::vector<uint8_t> decodedData;
    if (!Base64Decode(encodedString, decodedData))
    {
        CARBON_CRITICAL("failed to decode base64 data");
    }
    return std::string(decodedData.begin(), decodedData.end());
}

bool IsBase64Data(const std::string& encodedString)
{
    // an empty string is also Base64
    if (encodedString.empty()) { return true; }

    // encoded string must be a multiple of 4
    if (encodedString.size() % 4 != 0)
    {
        return false;
    }

    size_t sizeToCheck = encodedString.size();

    if (encodedString[encodedString.size() - 1] == '=') { sizeToCheck--; }
    if (encodedString[encodedString.size() - 2] == '=') { sizeToCheck--; }

    for (size_t i = 0; i < sizeToCheck; ++i)
    {
        if (decodingLookup.find(encodedString[i]) == decodingLookup.end())
        {
            return false;
        }
    }

    return true;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

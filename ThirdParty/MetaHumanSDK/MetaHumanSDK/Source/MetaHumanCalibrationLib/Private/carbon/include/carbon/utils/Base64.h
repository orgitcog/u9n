// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/*
 *  Perform base64 encoding of binary data to return a string using base64 alphabet defined in RFC 4648 ( https://en.wikipedia.org/wiki/Base64 )
 *  @param[in] data: the binary data to be encoded
 *  @param[in] length: the length of the binary data to be encoded
 *  @returns the base64 encoded string
 */
std::string Base64Encode(const unsigned char* data, size_t length);

/*
 *  Perform base64 decoding of string to return binary data using base64 alphabet defined in RFC 4648 ( https://en.wikipedia.org/wiki/Base64 )
 *  @param[in] encodedString: the base64 encoded string (length must be a multiple of 4).
 *  @param[out] decodedData: the base64 decoded data
 *  @returns true if decoded successfully, false otherwise (ie encoded string is not valid base64 data)
 */
bool Base64Decode(std::string const& encodedString, std::vector<unsigned char>& decodedData);

/**
 * Perform base64 decoding of string to return binary data using base64 alphabet defined in RFC 4648 ( https://en.wikipedia.org/wiki/Base64 )
 *  @param[in] encodedString: the base64 encoded string (length must be a multiple of 4).
 *  @returns The decoded binary string
 *  @throws exception if string cannot be decoded (ie encoded string is not valid base64 data).
 */
std::string Base64Decode(std::string const& encodedString);

//! @return True if string is base64 data
bool IsBase64Data(const std::string& encodedString);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

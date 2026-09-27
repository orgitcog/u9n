// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/*
 *  Flatten the supplied json in the supplied directory, ie for any strings in the Json file, if they exist as a file, if the file extension is .json
 *  they will be loaded as json, and if not, they will be loaded as binary and converted to a Base64 string and embedded in the output json
 *  @param[in] baseDir: the directory the base json file is in
 *  @returns the flattened Json
 */
JsonElement FlattenJson(const JsonElement& jsonIn, const std::string& baseDir);


/*
 *  Load the selected file as Json and flatten it, ie for any strings in the Json file, if they exist as a file, if the file extension is .json
 *  they will be loaded as json, and if not, they will be loaded as binary and converted to a Base64 string and embedded in the output json
 *  @param[in] fileName: the filename of the Json file to flatten
 *  @returns the flattened Json
 */
JsonElement FlattenJson(const std::string& fileName);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * @brief Reads all data in @p filename and returns it as std::string.
 * @return std::string with all data in the file.
 * @throw std::runtime_error if the file cannot be opened.
 */
std::string ReadFile(const std::string& filename);

/**
 * @brief Writes @p data to file @p filename.
 * @throw std::runtime_error if the file could not be written.
 */
void WriteFile(const std::string& filename, const std::string& data);

/**
 * @brief Throw file reading error with troubleshooting information if the file reading fails  
 * @throw std::runtime_error if the file cannot be opened.
 */
void ThrowReadFileErrorWithInfo(const std::string& filename);

/**
 * @return The username as read from the environment.
 */
std::string GetUsername();

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

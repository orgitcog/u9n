// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Common.h>
#include <string>

#define CARBON_SUPPRESS_UNUSED(x) (void)(x)

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct always_false : std::false_type {};

/**
 * @brief support converting u8string to string as of C++20
 *        based on https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1423r2.html: "Use explicit conversion functions"
 */
inline std::string from_u8string(const std::string &s)
{
  return s;
}

inline std::string from_u8string(std::string &&s)
{
  return std::move(s);
}

#if defined(__cpp_lib_char8_t)
inline std::string from_u8string(const std::u8string &s)
{
  return std::string(s.begin(), s.end());
}
#endif

#define FROM_U8STRING(x) TITAN_NAMESPACE::from_u8string(x).c_str()

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

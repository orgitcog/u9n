// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsTypes.h"
#include "Containers/StringFwd.h"

namespace PlainProps
{

template<typename T>	PLAINPROPS_API TOptional<T>						Parse(FUtf8StringView String);

template<>				PLAINPROPS_API TOptional<ERangeSizeType>		Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<ELeafWidth>			Parse(FUtf8StringView String);

template<>				PLAINPROPS_API TOptional<FUnpackedLeafType>		Parse(FUtf8StringView String);

template<>				PLAINPROPS_API TOptional<bool>					Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<int8> 					Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<int16>					Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<int32>					Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<int64>					Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<uint8> 				Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<uint16>				Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<uint32>				Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<uint64>				Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<float>					Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<double>				Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<char>					Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<char8_t>				Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<char16_t>				Parse(FUtf8StringView String);
template<>				PLAINPROPS_API TOptional<char32_t>				Parse(FUtf8StringView String);

///////////////////////////////////////////////////////////////////////////////

void PLAINPROPS_API ParseYamlBatch(TArray64<uint8>& OutBinary, FUtf8StringView Yaml);

} // namespace PlainProps

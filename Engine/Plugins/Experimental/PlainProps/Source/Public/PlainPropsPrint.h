// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsTypes.h"
#include "Containers/StringFwd.h"

namespace PlainProps
{

class FBatchIds;
struct FDiffPath;
struct FMemberSchema;
class FSchemaBatchId;
struct FReadDiffPath;
struct FStructView;

PLAINPROPS_API FAnsiStringView ToString(ELeafWidth Width);
PLAINPROPS_API FAnsiStringView ToString(ERangeSizeType Width);
PLAINPROPS_API FAnsiStringView ToString(ESchemaFormat Format);
PLAINPROPS_API FAnsiStringView ToString(FUnpackedLeafType Leaf);

///////////////////////////////////////////////////////////////////////////////

template<typename T> inline void Print(FUtf8Builder& Out, T Value)
{
	static_assert(!sizeof(T), "Unsupported type for Print");
}

template<> PLAINPROPS_API void Print(FUtf8Builder& Out, ERangeSizeType Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, ELeafWidth Value);

template<> PLAINPROPS_API void Print(FUtf8Builder& Out, bool Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, int8 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, int16 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, int32 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, int64 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, uint8 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, uint16 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, uint32 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, uint64 Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, float Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, double Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, char8_t Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, char16_t Value);
template<> PLAINPROPS_API void Print(FUtf8Builder& Out, char32_t Value);

///////////////////////////////////////////////////////////////////////////////

PLAINPROPS_API void PrintMemberSchema(FUtf8Builder& Out, const FIds& Ids, FMemberSchema Member);
PLAINPROPS_API void PrintYamlBatch(FUtf8Builder& Out, const FBatchIds& Ids, TConstArrayView<FStructView> Objects);
PLAINPROPS_API void PrintDiff(FUtf8Builder& Out, const FIds& Ids, const FDiffPath& Diff);
PLAINPROPS_API void PrintDiff(FUtf8Builder& Out, const FBatchIds& Ids, const FReadDiffPath& Diff);

} // namespace PlainProps

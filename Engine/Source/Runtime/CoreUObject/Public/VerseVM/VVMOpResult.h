// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "VVMValue.h"

#define V_RETURN(...)                             \
	return ::Verse::FOpResult                     \
	{                                             \
		::Verse::FOpResult::Return, (__VA_ARGS__) \
	}
#define V_REQUIRE_CONCRETE(...)                                                          \
	if (VValue MaybePlaceholderVal = (__VA_ARGS__); MaybePlaceholderVal.IsPlaceholder()) \
	{                                                                                    \
		return ::Verse::FOpResult{::Verse::FOpResult::Block, MaybePlaceholderVal};       \
	}
#define V_FAIL_IF(...)                                       \
	if (__VA_ARGS__)                                         \
	{                                                        \
		return ::Verse::FOpResult{::Verse::FOpResult::Fail}; \
	}
#define V_FAIL_UNLESS(...)                                   \
	if (!(__VA_ARGS__))                                      \
	{                                                        \
		return ::Verse::FOpResult{::Verse::FOpResult::Fail}; \
	}
#define V_YIELD()                 \
	return ::Verse::FOpResult     \
	{                             \
		::Verse::FOpResult::Yield \
	}

namespace Verse
{

// Represents the result of a single VM operation
struct FOpResult
{
	enum EKind
	{
		Return, // All went well, and Value is the result.
		Block,  // A placeholder was encountered, and this operation should be enqueued on Value.
		Fail,   // The current choice failed. Value is undefined.
		Yield,  // The task suspended, and execution should continue in the resumer. Value is undefined.
		Error,  // A runtime error occurred, and an exception was raised. Value is undefined.
	};

	FOpResult(EKind Kind, VValue Value = VValue())
		: Kind(Kind)
		, Value(Value)
	{
	}

	bool IsReturn() const
	{
		return Kind == Return;
	}

	bool IsError() const
	{
		return Kind == Error;
	}

	EKind Kind;
	VValue Value;
};

} // namespace Verse
#endif // WITH_VERSE_VM

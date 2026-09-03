// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"

template<typename InElementType>
struct TLinkedListBuilderNextLink
{
	using ElementType = InElementType;

	[[nodiscard]] FORCEINLINE static ElementType** GetNextPtr(ElementType& Element)
	{
		return &(Element.Next);
	}
};

template<typename InElementType, InElementType* InElementType::* NextLink>
struct TLinkedListBuilderNextLinkMemberVar
{
	using ElementType = InElementType;

	[[nodiscard]] FORCEINLINE static ElementType** GetNextPtr(ElementType& Element)
	{
		return &(Element.*NextLink);
	}
};

/**
 * Single linked list builder
 */
template<typename InElementType, typename InLinkAccessor = TLinkedListBuilderNextLink<InElementType>>
struct TLinkedListBuilder
{
public:
	using ElementType = InElementType;
	using LinkAccessor = InLinkAccessor;

	UE_NONCOPYABLE(TLinkedListBuilder);

private:
	FORCEINLINE void WriteEndPtr(InElementType* NewValue)
	{
		// Do not overwrite the same value to avoid dirtying the cache and
		// also prevent TSAN from thinking we are messing around with existing data.
		if (*EndPtr != NewValue)
		{
			*EndPtr = NewValue;
		}
	}
public:

	[[nodiscard]] explicit TLinkedListBuilder(ElementType** ListStartPtr) :
		StartPtr(ListStartPtr),
		EndPtr(ListStartPtr)
	{
		check(ListStartPtr);
	}

	// Move builder back to start and prepare for overwriting
	// It only changes state of builder, use NullTerminate() to mark list as empty!
	FORCEINLINE void Restart()
	{
		EndPtr = StartPtr;
	}

	UE_DEPRECATED(5.6, "Append is deprecated. Please use AppendTerminated instead.")
	FORCEINLINE void Append(ElementType& Element)
	{
		AppendTerminated(Element);
	}

	// Append element, don't touch next link
	FORCEINLINE void AppendNoTerminate(ElementType& Element)
	{
		WriteEndPtr(&Element);
		EndPtr = LinkAccessor::GetNextPtr(Element);
	}

	// Append element and mark it as last
	FORCEINLINE void AppendTerminated(ElementType& Element)
	{
		AppendNoTerminate(Element);
		NullTerminate();
	}

	FORCEINLINE void Remove(ElementType& Element)
	{
		ElementType** PrevIt = StartPtr;
		for (ElementType* It = *StartPtr; It; It = GetNext(*It))
		{
			if (It == &Element)
			{
				ElementType*& ElementNextRef = GetNextRef(Element);
				*PrevIt = ElementNextRef;
				ElementNextRef = nullptr;
				break;
			}

			PrevIt = &It;
		}
	}

	// Mark end of the list
	FORCEINLINE void NullTerminate()
	{
		WriteEndPtr(nullptr);
	}

	FORCEINLINE void MoveToEnd()
	{
		for (ElementType* It = *StartPtr; It; It = GetNext(*It))
		{
			EndPtr = LinkAccessor::GetNextPtr(*It);
		}
	}

	FORCEINLINE bool MoveToNext()
	{
		if (*EndPtr)
		{
			EndPtr = LinkAccessor::GetNextPtr(**EndPtr);
			return true;
		}

		return false;
	}

	[[nodiscard]] FORCEINLINE ElementType* GetNext(ElementType& Element) const
	{
		return GetNextRef(Element);
	}

	[[nodiscard]] FORCEINLINE ElementType* GetListStart() const
	{
		return *StartPtr;
	}

	[[nodiscard]] FORCEINLINE ElementType* GetListEnd() const
	{
		return *EndPtr;
	}

private:
	[[nodiscard]] FORCEINLINE ElementType*& GetNextRef(ElementType& Element) const
	{
		return *LinkAccessor::GetNextPtr(Element);
	}

	ElementType** StartPtr;
	ElementType** EndPtr;
};

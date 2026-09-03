// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ReverseIterate.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"
#include "Delegates/IntegerSequence.h"
#include "Templates/TypeHash.h"

namespace UE::Core::Private
{
	// This is a workaround for a parsing error in MSVC under /persmissive- builds, which would
	// get confused by the fold expression in the constraint in the constructor.
	template <typename InElementType, typename... ArgTypes>
	constexpr bool TCanBeConvertedToFromAll_V = (std::is_convertible_v<ArgTypes, InElementType> && ...);
}

/** An array with a static number of elements. */
template <typename InElementType, uint32 NumElements, uint32 Alignment = alignof(InElementType)>
class alignas(Alignment) TStaticArray
{
public:
	using ElementType = InElementType;

	[[nodiscard]] constexpr TStaticArray() = default;

	// Constructs each element with Args
	template <typename... ArgTypes>
	[[nodiscard]] constexpr explicit TStaticArray(EInPlace, ArgTypes&&... Args)
		: Storage(InPlace, TMakeIntegerSequence<uint32, NumElements>(), Forward<ArgTypes>(Args)...)
	{
	}

	// Directly initializes the array with the provided values.
	template <
		typename... ArgTypes
		UE_REQUIRES((sizeof...(ArgTypes) > 0 && sizeof...(ArgTypes) <= NumElements) && UE::Core::Private::TCanBeConvertedToFromAll_V<InElementType, ArgTypes...>)
	>
	[[nodiscard]] constexpr TStaticArray(ArgTypes&&... Args)
		: Storage(PerElement, Forward<ArgTypes>(Args)...)
	{
	}

	[[nodiscard]] constexpr TStaticArray(TStaticArray&& Other) = default;
	[[nodiscard]] constexpr TStaticArray(const TStaticArray& Other) = default;
	constexpr TStaticArray& operator=(TStaticArray&& Other) = default;
	constexpr TStaticArray& operator=(const TStaticArray& Other) = default;

	// Accessors.
	[[nodiscard]] FORCEINLINE_DEBUGGABLE constexpr InElementType& operator[](uint32 Index)
	{
		checkSlow(Index < NumElements);
		return Storage.Elements[Index].Element;
	}

	[[nodiscard]] FORCEINLINE_DEBUGGABLE constexpr const InElementType& operator[](uint32 Index) const
	{
		checkSlow(Index < NumElements);
		return Storage.Elements[Index].Element;
	}

	// Comparisons.
	[[nodiscard]] constexpr friend bool operator==(const TStaticArray& A,const TStaticArray& B)
	{
		for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			if(!(A[ElementIndex] == B[ElementIndex]))
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] constexpr bool operator!=(const TStaticArray& B) const
	{
		for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			if(!((*this)[ElementIndex] == B[ElementIndex]))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Returns true if the array is empty and contains no elements. 
	 *
	 * @returns True if the array is empty.
	 * @see Num
	 */
	[[nodiscard]] constexpr bool IsEmpty() const
	{
		return NumElements == 0;
	}

	/** The number of elements in the array. */
	[[nodiscard]] FORCEINLINE_DEBUGGABLE constexpr int32 Num() const
	{
		return NumElements;
	}

	/** A pointer to the first element of the array */
	[[nodiscard]] FORCEINLINE_DEBUGGABLE constexpr InElementType* GetData()
	{
		static_assert((alignof(ElementType) % Alignment) == 0, "GetData() cannot be called on a TStaticArray with non-standard alignment");
		return &Storage.Elements[0].Element;
	}
	[[nodiscard]] FORCEINLINE_DEBUGGABLE constexpr const InElementType* GetData() const
	{
		return const_cast<TStaticArray*>(this)->GetData();
	}

private:

	struct alignas(Alignment) TArrayStorageElementAligned
	{
		[[nodiscard]] constexpr TArrayStorageElementAligned() = default;

		// Index is used to achieve pack expansion in TArrayStorage's first constructor, but is unused here
		template <typename... ArgTypes>
		[[nodiscard]] constexpr explicit TArrayStorageElementAligned(EInPlace, uint32 /*Index*/, ArgTypes&&... Args)
			: Element(Forward<ArgTypes>(Args)...)
		{
		}

		InElementType Element;
	};

	struct TArrayStorage
	{
		[[nodiscard]] constexpr TArrayStorage() = default;

		template<uint32... Indices, typename... ArgTypes>
		[[nodiscard]] constexpr explicit TArrayStorage(EInPlace, TIntegerSequence<uint32, Indices...>, ArgTypes&&... Args)
			: Elements{ TArrayStorageElementAligned(InPlace, Indices, Args...)... }
		{
			// The arguments are deliberately not forwarded arguments here, because we're initializing multiple elements
			// and don't want an argument to be mutated by the first element's constructor and then that moved-from state
			// be used to construct the remaining elements.
			//
			// This'll mean that it'll be a compile error to use move-only types like TUniquePtr when in-place constructing
			// TStaticArray elements, which is a natural expectation because that TUniquePtr can only transfer ownership to
			// a single element.
		}

		template<typename... ArgTypes>
		[[nodiscard]] constexpr explicit TArrayStorage(EPerElement, ArgTypes&&... Args)
			: Elements{ TArrayStorageElementAligned(InPlace, 0 /* dummy index */, Forward<ArgTypes>(Args))... }
		{
		}

		TArrayStorageElementAligned Elements[NumElements];
	};

	TArrayStorage Storage;


public:

	template <typename StorageElementType, bool bReverse = false>
	struct FRangedForIterator
	{
		[[nodiscard]] constexpr explicit FRangedForIterator(StorageElementType* InPtr)
			: Ptr(InPtr)
		{
		}

		[[nodiscard]] constexpr auto& operator*() const
		{
			if constexpr (bReverse)
			{
				return (Ptr - 1)->Element;
			}
			else
			{
				return Ptr->Element;
			}
		}

		constexpr FRangedForIterator& operator++()
		{
			if constexpr (bReverse)
			{
				--Ptr;
			}
			else
			{
				++Ptr;
			}
			return *this;
		}

		[[nodiscard]] constexpr bool operator!=(const FRangedForIterator& B) const
		{
			return Ptr != B.Ptr;
		}

	private:
		StorageElementType* Ptr;
	};

	using RangedForIteratorType             = FRangedForIterator<      TArrayStorageElementAligned>;
	using RangedForConstIteratorType        = FRangedForIterator<const TArrayStorageElementAligned>;
	using RangedForReverseIteratorType      = FRangedForIterator<      TArrayStorageElementAligned, true>;
	using RangedForConstReverseIteratorType = FRangedForIterator<const TArrayStorageElementAligned, true>;

	/** STL-like iterators to enable range-based for loop support. */
	[[nodiscard]] FORCEINLINE RangedForIteratorType             constexpr begin()        { return RangedForIteratorType(Storage.Elements); }
	[[nodiscard]] FORCEINLINE RangedForConstIteratorType        constexpr begin()  const { return RangedForConstIteratorType(Storage.Elements); }
	[[nodiscard]] FORCEINLINE RangedForIteratorType             constexpr end()          { return RangedForIteratorType(Storage.Elements + NumElements); }
	[[nodiscard]] FORCEINLINE RangedForConstIteratorType        constexpr end()    const { return RangedForConstIteratorType(Storage.Elements + NumElements); }
	[[nodiscard]] FORCEINLINE RangedForReverseIteratorType      constexpr rbegin()       { return RangedForReverseIteratorType(Storage.Elements + NumElements); }
	[[nodiscard]] FORCEINLINE RangedForConstReverseIteratorType constexpr rbegin() const { return RangedForConstReverseIteratorType(Storage.Elements + NumElements); }
	[[nodiscard]] FORCEINLINE RangedForReverseIteratorType      constexpr rend()         { return RangedForReverseIteratorType(Storage.Elements); }
	[[nodiscard]] FORCEINLINE RangedForConstReverseIteratorType constexpr rend()   const { return RangedForConstReverseIteratorType(Storage.Elements); }
};

/** Creates a static array filled with the specified value. */
template <typename InElementType, uint32 NumElements>
[[nodiscard]] constexpr TStaticArray<InElementType,NumElements> MakeUniformStaticArray(typename TCallTraits<InElementType>::ParamType InValue)
{
	TStaticArray<InElementType,NumElements> Result;
	for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
	{
		Result[ElementIndex] = InValue;
	}
	return Result;
}

template <typename ElementType, uint32 NumElements, uint32 Alignment>
struct TIsContiguousContainer<TStaticArray<ElementType, NumElements, Alignment>>
{
	enum { Value = (alignof(ElementType) % Alignment) == 0 };
};

/** Serializer. */
template <typename ElementType, uint32 NumElements, uint32 Alignment>
FArchive& operator<<(FArchive& Ar,TStaticArray<ElementType, NumElements, Alignment>& StaticArray)
{
	for(uint32 Index = 0;Index < NumElements;++Index)
	{
		Ar << StaticArray[Index];
	}
	return Ar;
}

/** Hash function. */
template <typename ElementType, uint32 NumElements, uint32 Alignment>
[[nodiscard]] uint32 GetTypeHash(const TStaticArray<ElementType, NumElements, Alignment>& Array)
{
	uint32 Hash = 0;
	for (const ElementType& Element : Array)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Element));
	}
	return Hash;
}

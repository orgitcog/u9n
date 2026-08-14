// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/StructTypeBitSet.h"
#include "MassProcessingTypes.h"
#include "StructUtils/StructArrayView.h"
#include "Subsystems/Subsystem.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassExternalSubsystemTraits.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "StructUtils/SharedStruct.h"
#include "MassEntityElementTypes.h"
#include "MassEntityConcepts.h"
#include "MassTestableEnsures.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassExternalSubsystemTraits.h"
#include "MassEntityHandle.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassEntityTypes.generated.h"


MASSENTITY_API DECLARE_LOG_CATEGORY_EXTERN(LogMass, Warning, All);

DECLARE_STATS_GROUP(TEXT("Mass"), STATGROUP_Mass, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Mass Total Frame Time"), STAT_Mass_Total, STATGROUP_Mass, MASSENTITY_API);

DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassFragmentBitSet, FMassFragment);
DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassTagBitSet, FMassTag);
DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassChunkFragmentBitSet, FMassChunkFragment);
DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassSharedFragmentBitSet, FMassSharedFragment);
DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassConstSharedFragmentBitSet, FMassConstSharedFragment);
DECLARE_CLASSTYPEBITSET_EXPORTED(MASSENTITY_API, FMassExternalSubsystemBitSet, USubsystem);

namespace UE::Mass
{
	template<typename T>
	static constexpr bool TAlwaysFalse = false;
}

/** The type summarily describing a composition of an entity or an archetype. It contains information on both the
 *  fragments and tags */
struct FMassArchetypeCompositionDescriptor
{
	FMassArchetypeCompositionDescriptor() = default;
	FMassArchetypeCompositionDescriptor(const FMassFragmentBitSet& InFragments,
		const FMassTagBitSet& InTags,
		const FMassChunkFragmentBitSet& InChunkFragments,
		const FMassSharedFragmentBitSet& InSharedFragments,
		const FMassConstSharedFragmentBitSet& InConstSharedFragments)
		: Fragments(InFragments)
		, Tags(InTags)
		, ChunkFragments(InChunkFragments)
		, SharedFragments(InSharedFragments)
		, ConstSharedFragments(InConstSharedFragments)
	{}

	FMassArchetypeCompositionDescriptor(TConstArrayView<const UScriptStruct*> InFragments,
		const FMassTagBitSet& InTags,
		const FMassChunkFragmentBitSet& InChunkFragments,
		const FMassSharedFragmentBitSet& InSharedFragments,
		const FMassConstSharedFragmentBitSet& InConstSharedFragments)
		: FMassArchetypeCompositionDescriptor(FMassFragmentBitSet(InFragments), InTags, InChunkFragments, InSharedFragments, InConstSharedFragments)
	{}

	FMassArchetypeCompositionDescriptor(TConstArrayView<FInstancedStruct> InFragmentInstances,
		const FMassTagBitSet& InTags,
		const FMassChunkFragmentBitSet& InChunkFragments,
		const FMassSharedFragmentBitSet& InSharedFragments,
		const FMassConstSharedFragmentBitSet& InConstSharedFragments)
		: FMassArchetypeCompositionDescriptor(FMassFragmentBitSet(InFragmentInstances), InTags, InChunkFragments, InSharedFragments, InConstSharedFragments)
	{}

	FMassArchetypeCompositionDescriptor(FMassFragmentBitSet&& InFragments,
		FMassTagBitSet&& InTags,
		FMassChunkFragmentBitSet&& InChunkFragments,
		FMassSharedFragmentBitSet&& InSharedFragments,
		FMassConstSharedFragmentBitSet&& InConstSharedFragments)
		: Fragments(MoveTemp(InFragments))
		, Tags(MoveTemp(InTags))
		, ChunkFragments(MoveTemp(InChunkFragments))
		, SharedFragments(MoveTemp(InSharedFragments))
		, ConstSharedFragments(MoveTemp(InConstSharedFragments))
	{}

	FMassArchetypeCompositionDescriptor(FMassFragmentBitSet&& InFragments)
		: Fragments(MoveTemp(InFragments))
	{}

	FMassArchetypeCompositionDescriptor(FMassTagBitSet&& InTags)
		: Tags(MoveTemp(InTags))
	{}

	void Reset()
	{
		Fragments.Reset();
		Tags.Reset();
		ChunkFragments.Reset();
		SharedFragments.Reset();
		ConstSharedFragments.Reset();
	}

	/**
	 * Compares contents of two FMassArchetypeCompositionDescriptor instances, ignoring the trailing empty bits in the bitsets
	 */
	bool IsEquivalent(const FMassArchetypeCompositionDescriptor& OtherDescriptor) const
	{
		return Fragments.IsEquivalent(OtherDescriptor.Fragments)
			&& Tags.IsEquivalent(OtherDescriptor.Tags)
			&& ChunkFragments.IsEquivalent(OtherDescriptor.ChunkFragments)
			&& SharedFragments.IsEquivalent(OtherDescriptor.SharedFragments)
			&& ConstSharedFragments.IsEquivalent(OtherDescriptor.ConstSharedFragments);
	}

	/**
	 * Checks whether contents of two FMassArchetypeCompositionDescriptor instances are identical.
	 */
	bool IsIdentical(const FMassArchetypeCompositionDescriptor& OtherDescriptor) const
	{
		return Fragments == OtherDescriptor.Fragments
			&& Tags == OtherDescriptor.Tags
			&& ChunkFragments == OtherDescriptor.ChunkFragments
			&& SharedFragments == OtherDescriptor.SharedFragments
			&& ConstSharedFragments == OtherDescriptor.ConstSharedFragments;
	}

	bool IsEmpty() const 
	{ 
		return Fragments.IsEmpty()
			&& Tags.IsEmpty()
			&& ChunkFragments.IsEmpty()
			&& SharedFragments.IsEmpty()
			&& ConstSharedFragments.IsEmpty();
	}

	bool HasAll(const FMassArchetypeCompositionDescriptor& OtherDescriptor) const
	{
		return Fragments.HasAll(OtherDescriptor.Fragments)
			&& Tags.HasAll(OtherDescriptor.Tags)
			&& ChunkFragments.HasAll(OtherDescriptor.ChunkFragments)
			&& SharedFragments.HasAll(OtherDescriptor.SharedFragments)
			&& ConstSharedFragments.HasAll(OtherDescriptor.ConstSharedFragments);
	}

	void Append(const FMassArchetypeCompositionDescriptor& OtherDescriptor)
	{
		Fragments += OtherDescriptor.Fragments;
		Tags += OtherDescriptor.Tags;
		ChunkFragments += OtherDescriptor.ChunkFragments;
		SharedFragments += OtherDescriptor.SharedFragments;
		ConstSharedFragments += OtherDescriptor.ConstSharedFragments;
	}

	void Remove(const FMassArchetypeCompositionDescriptor& OtherDescriptor)
	{
		Fragments -= OtherDescriptor.Fragments;
		Tags -= OtherDescriptor.Tags;
		ChunkFragments -= OtherDescriptor.ChunkFragments;
		SharedFragments -= OtherDescriptor.SharedFragments;
		ConstSharedFragments -= OtherDescriptor.ConstSharedFragments;
	}

	/**
	 * Finds all the elements contained in `this` while missing in `OtherDescriptor` and returns
	 * the data as a FMassArchetypeCompositionDescriptor instance
	 */
	FMassArchetypeCompositionDescriptor CalculateDifference(const FMassArchetypeCompositionDescriptor& OtherDescriptor) const
	{
		FMassArchetypeCompositionDescriptor Diff;

		Diff.Fragments = Fragments - OtherDescriptor.Fragments;
		Diff.Tags = Tags - OtherDescriptor.Tags;
		Diff.ChunkFragments = ChunkFragments - OtherDescriptor.ChunkFragments;
		Diff.SharedFragments = SharedFragments - OtherDescriptor.SharedFragments;
		Diff.ConstSharedFragments = ConstSharedFragments - OtherDescriptor.ConstSharedFragments;

		return Diff;
	}

	MASSENTITY_API static uint32 CalculateHash(const FMassFragmentBitSet& InFragments, const FMassTagBitSet& InTags
		, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragmentBitSet
		, const FMassConstSharedFragmentBitSet& InConstSharedFragmentBitSet);

	uint32 CalculateHash() const 
	{
		return CalculateHash(Fragments, Tags, ChunkFragments, SharedFragments, ConstSharedFragments);
	}

	MASSENTITY_API int32 CountStoredTypes() const;

	MASSENTITY_API void DebugOutputDescription(FOutputDevice& Ar) const;

	template<typename T>
	auto& GetContainer() const;

	template<typename T>
	auto& GetContainer();

	template<typename T>
	bool Contains() const;

	template<typename T>
	void Add();

	FMassFragmentBitSet Fragments;
	FMassTagBitSet Tags;
	FMassChunkFragmentBitSet ChunkFragments;
	FMassSharedFragmentBitSet SharedFragments;
	FMassConstSharedFragmentBitSet ConstSharedFragments;

	UE_DEPRECATED(5.5, "This FMassArchetypeCompositionDescriptor constructor is deprecated. Please explicitly provide FConstSharedFragmentBitSet.")
	FMassArchetypeCompositionDescriptor(const FMassFragmentBitSet& InFragments, const FMassTagBitSet& InTags, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragments)
		: FMassArchetypeCompositionDescriptor(InFragments, InTags, InChunkFragments, InSharedFragments, FMassConstSharedFragmentBitSet())
	{}

	UE_DEPRECATED(5.5, "This FMassArchetypeCompositionDescriptor constructor is deprecated. Please explicitly provide FConstSharedFragmentBitSet.")
	FMassArchetypeCompositionDescriptor(TConstArrayView<const UScriptStruct*> InFragments, const FMassTagBitSet& InTags, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragments)
		: FMassArchetypeCompositionDescriptor(FMassFragmentBitSet(InFragments), InTags, InChunkFragments, InSharedFragments, FMassConstSharedFragmentBitSet())
	{}

	UE_DEPRECATED(5.5, "This FMassArchetypeCompositionDescriptor constructor is deprecated. Please explicitly provide FConstSharedFragmentBitSet.")
	FMassArchetypeCompositionDescriptor(TConstArrayView<FInstancedStruct> InFragmentInstances, const FMassTagBitSet& InTags, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragments)
		: FMassArchetypeCompositionDescriptor(FMassFragmentBitSet(InFragmentInstances), InTags, InChunkFragments, InSharedFragments, FMassConstSharedFragmentBitSet())
	{}

	UE_DEPRECATED(5.5, "This FMassArchetypeCompositionDescriptor constructor is deprecated. Please explicitly provide FConstSharedFragmentBitSet.")
	FMassArchetypeCompositionDescriptor(FMassFragmentBitSet&& InFragments, FMassTagBitSet&& InTags, FMassChunkFragmentBitSet&& InChunkFragments, FMassSharedFragmentBitSet&& InSharedFragments)
	{
		ensureMsgf(false, TEXT("This constructor is defunct. Please update your implementation based on deprecation warning."));
	}
};

/** 
 * Wrapper for const and non-const shared fragment containers that tracks which struct types it holds (via a FMassSharedFragmentBitSet).
 * Note that having multiple instanced of a given struct type is not supported and Add* functions will fetch the previously 
 * added fragment instead of adding a new one.
 */
struct FMassArchetypeSharedFragmentValues
{
	FMassArchetypeSharedFragmentValues() = default;
	FMassArchetypeSharedFragmentValues(const FMassArchetypeSharedFragmentValues& OtherFragmentValues) = default;
	FMassArchetypeSharedFragmentValues(FMassArchetypeSharedFragmentValues&& OtherFragmentValues) = default;
	FMassArchetypeSharedFragmentValues& operator=(const FMassArchetypeSharedFragmentValues& OtherFragmentValues) = default;
	FMassArchetypeSharedFragmentValues& operator=(FMassArchetypeSharedFragmentValues&& OtherFragmentValues) = default;

	FORCEINLINE bool HasExactFragmentTypesMatch(const FMassSharedFragmentBitSet& InSharedFragmentBitSet, const FMassConstSharedFragmentBitSet& InConstSharedFragmentBitSet) const
	{
		return HasExactSharedFragmentTypesMatch(InSharedFragmentBitSet)
			&& HasExactConstSharedFragmentTypesMatch(InConstSharedFragmentBitSet);
	}

	FORCEINLINE bool HasExactSharedFragmentTypesMatch(const FMassSharedFragmentBitSet& InSharedFragmentBitSet) const
	{
		return SharedFragmentBitSet.IsEquivalent(InSharedFragmentBitSet);
	}

	FORCEINLINE bool HasAllRequiredSharedFragmentTypes(const FMassSharedFragmentBitSet& InSharedFragmentBitSet) const
	{
		return SharedFragmentBitSet.HasAll(InSharedFragmentBitSet);
	}

	FORCEINLINE bool HasExactConstSharedFragmentTypesMatch(const FMassConstSharedFragmentBitSet& InConstSharedFragmentBitSet) const
	{
		return ConstSharedFragmentBitSet.IsEquivalent(InConstSharedFragmentBitSet);
	}

	FORCEINLINE bool HasAllRequiredConstSharedFragmentTypes(const FMassConstSharedFragmentBitSet& InConstSharedFragmentBitSet) const
	{
		return ConstSharedFragmentBitSet.HasAll(InConstSharedFragmentBitSet);
	}

	/**
	 * @return whether the stored shared fragment values exactly match shared fragment types indicated by InDescriptor
	 */
	bool DoesMatchComposition(const FMassArchetypeCompositionDescriptor& InDescriptor) const 
	{
		return HasExactSharedFragmentTypesMatch(InDescriptor.SharedFragments)
			&& HasExactConstSharedFragmentTypesMatch(InDescriptor.ConstSharedFragments);
	}

	FORCEINLINE bool IsEquivalent(const FMassArchetypeSharedFragmentValues& OtherSharedFragmentValues) const
	{
		return GetTypeHash(*this) == GetTypeHash(OtherSharedFragmentValues);
	}

	/** 
	 * Compares contents of `this` and the Other, and allows different order of elements in both containers.
	 * Note that the function ignores "nulls", i.e. empty FConstSharedStruct and FSharedStruct instances. The function
	 * does care however about matching "mode", meaning ConstSharedFragments and SharedFragments arrays are compared
	 * independently.
	 */
	MASSENTITY_API bool HasSameValues(const FMassArchetypeSharedFragmentValues& Other) const;

	FORCEINLINE bool ContainsType(const UScriptStruct* FragmentType) const
	{
		if (UE::Mass::IsA<FMassSharedFragment>(FragmentType))
		{
			return SharedFragmentBitSet.Contains(*FragmentType);
		}

		if (UE::Mass::IsA<FMassConstSharedFragment>(FragmentType))
		{
			return ConstSharedFragmentBitSet.Contains(*FragmentType);
		}

		return false;
	}

	template<typename T>
	FORCEINLINE bool ContainsType() const
	{
		if constexpr (UE::Mass::CConstSharedFragment<T>)
		{
			return ConstSharedFragmentBitSet.Contains(*T::StaticStruct());
		}
		else if constexpr (UE::Mass::CSharedFragment<T>)
		{
			return SharedFragmentBitSet.Contains(*T::StaticStruct());
		}
		else
		{
			return false;
		}
	}

	/**
	 * Adds Fragment to the collection.
	 * Method will ensure if a fragment of the given FMassConstSharedFragment subclass has already been added.
	 */
	void Add(const FConstSharedStruct& Fragment)
	{
		(void)Add_GetRef(Fragment);
	}

	/** 
	 * Adds Fragment to the collection.
	 * Method will ensure if a fragment of the given FMassConstSharedFragment subclass has already been added.
	 * In that case the method will return the previously added instance if the given type has been added
	 * as a CONST shared fragment and if not it will return an empty FConstSharedStruct.
	 */
	MASSENTITY_API FConstSharedStruct Add_GetRef(const FConstSharedStruct& Fragment);

	UE_DEPRECATED(5.6, "Use Add or Add_GetRef instead depending on whether you need the return value.")
		FConstSharedStruct AddConstSharedFragment(const FConstSharedStruct& Fragment)
	{
		return Add_GetRef(Fragment);
	}

	/**
	 * Adds Fragment to the collection.
	 * Method will ensure if a fragment of the given FMassSharedFragment subclass has already been added.
	 */
	void Add(const FSharedStruct& Fragment)
	{
		(void)Add_GetRef(Fragment);
	}

	/** 
	 * Adds Fragment to the collection.
	 * Method will ensure if a fragment of the given FMassSharedFragment subclass has already been added.
	 * In that case the method will return the previously added instance if the given type has been added
	 * as a NON-CONST shared fragment and if not it will return an empty FSharedStruct.
	 */
	MASSENTITY_API FSharedStruct Add_GetRef(const FSharedStruct& Fragment);

	UE_DEPRECATED(5.6, "Use Add or Add_GetRef instead depending on whether you need the return value.")
	FSharedStruct AddSharedFragment(const FSharedStruct& Fragment)
	{
		return Add_GetRef(Fragment);
	}

	/**
	 * Finds instances of fragment types given by Fragments and replaces their values with contents of respective
	 * element of Fragments.
	 * Note that it's callers responsibility to ensure every fragment type in Fragments already has an instance in
	 * this FMassArchetypeSharedFragmentValues instance. Failing that assumption will result in ensure failure. 
	 */
	MASSENTITY_API void ReplaceSharedFragments(TConstArrayView<FSharedStruct> Fragments);

	/** 
	 * Appends contents of Other to `this` instance. All common fragments will get overridden with values in Other.
	 * Note that changing a fragments "role" (being const or non-const) is not supported and the function will fail an
	 * ensure when that is attempted.
	 * @return number of fragments added or changed
	 */
	MASSENTITY_API int32 Append(const FMassArchetypeSharedFragmentValues& Other);

	/** 
	 * Note that the function removes the shared fragments by type
	 * @return number of fragments types removed
	 */
	MASSENTITY_API int32 Remove(const FMassSharedFragmentBitSet& SharedFragmentToRemoveBitSet);

	/** 
	 * Note that the function removes the const shared fragments by type
	 * @return number of fragments types removed
	 */
	MASSENTITY_API int32 Remove(const FMassConstSharedFragmentBitSet& ConstSharedFragmentToRemoveBitSet);

	/**
	 * Remove all the shared and const shared fragments indicated by InDescriptor
	 * @return number of fragments types removed
	 */
	int32 Remove(const FMassArchetypeCompositionDescriptor& InDescriptor)
	{
		return Remove(InDescriptor.SharedFragments) + Remove(InDescriptor.ConstSharedFragments);
	}

	FORCEINLINE const TArray<FConstSharedStruct>& GetConstSharedFragments() const
	{
		return ConstSharedFragments;
	}

	FORCEINLINE TArray<FSharedStruct>& GetMutableSharedFragments()
	{
		return SharedFragments;
	}
	
	FORCEINLINE const TArray<FSharedStruct>& GetSharedFragments() const
	{
		return SharedFragments;
	}
	
	FConstSharedStruct GetConstSharedFragmentStruct(const UScriptStruct* StructType) const
	{
		const int32 FragmentIndex = ConstSharedFragments.IndexOfByPredicate(FStructTypeEqualOperator(StructType));
		return FragmentIndex != INDEX_NONE ? ConstSharedFragments[FragmentIndex] : FConstSharedStruct();
	}
		
	FSharedStruct GetSharedFragmentStruct(const UScriptStruct* StructType)
	{
		const int32 FragmentIndex = SharedFragments.IndexOfByPredicate(FStructTypeEqualOperator(StructType));
		return FragmentIndex != INDEX_NONE ? SharedFragments[FragmentIndex] : FSharedStruct();
	}

	FConstSharedStruct GetSharedFragmentStruct(const UScriptStruct* StructType) const
	{
		const int32 FragmentIndex = SharedFragments.IndexOfByPredicate(FStructTypeEqualOperator(StructType));
		return FragmentIndex != INDEX_NONE ? SharedFragments[FragmentIndex] : FSharedStruct();
	}

	const FMassSharedFragmentBitSet& GetSharedFragmentBitSet() const
	{
		return SharedFragmentBitSet;
	}

	const FMassConstSharedFragmentBitSet& GetConstSharedFragmentBitSet() const
	{
		return ConstSharedFragmentBitSet;
	}

	FORCEINLINE void DirtyHashCache()
	{
		HashCache = UINT32_MAX;
		// we consider a single shared fragment as being "sorted"
		bSorted = (SharedFragments.Num() + ConstSharedFragments.Num() <= 1) ;
	}

	FORCEINLINE void CacheHash() const
	{
		if (HashCache == UINT32_MAX)
		{
			HashCache = CalculateHash();
		}
	}

	friend FORCEINLINE uint32 GetTypeHash(const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
	{
		SharedFragmentValues.CacheHash();
		return SharedFragmentValues.HashCache;
	}

	MASSENTITY_API uint32 CalculateHash() const;
	SIZE_T GetAllocatedSize() const;

	void Sort()
	{
		if(!bSorted)
		{
			ConstSharedFragments.Sort(FStructTypeSortOperator());
			SharedFragments.Sort(FStructTypeSortOperator());
			bSorted = true;
		}
	}

	bool IsSorted() const;

	bool IsEmpty() const;

	void Reset();

protected:
	mutable uint32 HashCache = UINT32_MAX;
	/**
	 * We consider empty FMassArchetypeSharedFragmentValues a sorted container.Same goes for a container containing
	 * a single element, @see DirtyHashCache
	 */ 
	mutable bool bSorted = true; 
	
	FMassSharedFragmentBitSet SharedFragmentBitSet;
	FMassConstSharedFragmentBitSet ConstSharedFragmentBitSet;
	TArray<FConstSharedStruct> ConstSharedFragments;
	TArray<FSharedStruct> SharedFragments;

public:
	//-----------------------------------------------------------------------------
	// DEPRECATED
	//-----------------------------------------------------------------------------
	UE_DEPRECATED(5.5, "HasExactFragmentTypesMatch is deprecated. Use HasExactSharedFragmentTypesMatch or the two-parameter version of HasExactFragmentTypesMatch.")
	FORCEINLINE bool HasExactFragmentTypesMatch(const FMassSharedFragmentBitSet& InSharedFragmentBitSet) const
	{
		return HasExactSharedFragmentTypesMatch(InSharedFragmentBitSet);
	}
};

UENUM()
enum class EMassObservedOperation : uint8
{
	Add,
	Remove,
	// @todo Keeping this here as an indication of design intent. For now, we handle entity destruction like removal, but 
	// there might be computationally expensive cases where we might want to avoid for soon-to-be-dead entities. 
	// Destroy,
	// @todo another planned supported operation type
	// Touch,
	MAX
};

enum class EMassExecutionContextType : uint8
{
	Local,
	Processor,
	MAX
};

/** 
 * Note that this is a view and is valid only as long as the source data is valid. Used when flushing mass commands to
 * wrap different kinds of data into a uniform package so that it can be passed over to a common interface.
 */
struct FMassGenericPayloadView
{
	FMassGenericPayloadView() = default;
	FMassGenericPayloadView(TArray<FStructArrayView>&SourceData)
		: Content(SourceData)
	{}
	FMassGenericPayloadView(TArrayView<FStructArrayView> SourceData)
		: Content(SourceData)
	{}

	int32 Num() const { return Content.Num(); }

	void Reset()
	{
		Content = TArrayView<FStructArrayView>();
	}

	FORCEINLINE void Swap(const int32 A, const int32 B)
	{
		for (FStructArrayView& View : Content)
		{
			View.Swap(A, B);
		}
	}

	/** Moves NumToMove elements to the back of the viewed collection. */
	void SwapElementsToEnd(int32 StartIndex, int32 NumToMove);

	TArrayView<FStructArrayView> Content;
};

/**
 * Used to indicate a specific slice of a preexisting FMassGenericPayloadView, it's essentially an access pattern
 * Note: accessing content generates copies of FStructArrayViews stored (still cheap, those are just views). 
 */
struct FMassGenericPayloadViewSlice
{
	FMassGenericPayloadViewSlice() = default;
	FMassGenericPayloadViewSlice(const FMassGenericPayloadView& InSource, const int32 InStartIndex, const int32 InCount)
		: Source(InSource), StartIndex(InStartIndex), Count(InCount)
	{
	}

	FStructArrayView operator[](const int32 Index) const
	{
		return Source.Content[Index].Slice(StartIndex, Count);
	}

	/** @return the number of "layers" (i.e. number of original arrays) this payload has been built from */
	int32 Num() const 
	{
		return Source.Num();
	}

	bool IsEmpty() const
	{
		return !(Source.Num() > 0 && Count > 0);
	}

private:
	FMassGenericPayloadView Source;
	const int32 StartIndex = 0;
	const int32 Count = 0;
};

namespace UE::Mass
{
	/**
	 * A statically-typed list of related types. Used mainly to differentiate type collections at compile-type as well as
	 * efficiently produce TStructTypeBitSet representing given collection.
	 */
	template<typename T, typename... TOthers>
	struct TMultiTypeList : TMultiTypeList<TOthers...>
	{
		using Super = TMultiTypeList<TOthers...>;
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		enum
		{
			Ordinal = Super::Ordinal + 1
		};

		template<typename TBitSetType>
		constexpr static void PopulateBitSet(TBitSetType& OutBitSet)
		{
			Super::PopulateBitSet(OutBitSet);
			OutBitSet += TBitSetType::template GetTypeBitSet<FType>();
		}
	};
		
	/** Single-type specialization of TMultiTypeList. */
	template<typename T>
	struct TMultiTypeList<T>
	{
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		enum
		{
			Ordinal = 0
		};

		template<typename TBitSetType>
		constexpr static void PopulateBitSet(TBitSetType& OutBitSet)
		{
			OutBitSet += TBitSetType::template GetTypeBitSet<FType>();
		}
	};

	/** 
	 * The type hosts a statically-typed collection of TArrays, where each TArray is strongly-typed (i.e. it contains 
	 * instances of given structs rather than structs wrapped up in FInstancedStruct). This type lets us do batched 
	 * fragment values setting by simply copying data rather than setting per-instance. 
	 */
	template<typename T, typename... TOthers>
	struct TMultiArray : TMultiArray<TOthers...>
	{
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		using Super = TMultiArray<TOthers...>;

		enum
		{
			Ordinal = Super::Ordinal + 1
		};

		SIZE_T GetAllocatedSize() const
		{
			return FragmentInstances.GetAllocatedSize() + Super::GetAllocatedSize();
		}

		int GetNumArrays() const { return Ordinal + 1; }

		void Add(const FType& Item, TOthers... Rest)
		{
			FragmentInstances.Add(Item);
			Super::Add(Rest...);
		}

		void GetAsGenericMultiArray(TArray<FStructArrayView>& A) /*const*/
		{
			Super::GetAsGenericMultiArray(A);
			A.Add(FStructArrayView(FragmentInstances));
		}

		void GetheredAffectedFragments(FMassFragmentBitSet& OutBitSet) const
		{
			Super::GetheredAffectedFragments(OutBitSet);
			OutBitSet += FMassFragmentBitSet::GetTypeBitSet<FType>();
		}

		void Reset()
		{
			Super::Reset();
			FragmentInstances.Reset();
		}

		TArray<FType> FragmentInstances;
	};

	/**TMultiArray single-type specialization */
	template<typename T>
	struct TMultiArray<T>
	{
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		enum { Ordinal = 0 };

		SIZE_T GetAllocatedSize() const
		{
			return FragmentInstances.GetAllocatedSize();
		}

		int GetNumArrays() const { return Ordinal + 1; }

		void Add(const FType& Item) { FragmentInstances.Add(Item); }

		void GetAsGenericMultiArray(TArray<FStructArrayView>& A) /*const*/
		{
			A.Add(FStructArrayView(FragmentInstances));
		}

		void GetheredAffectedFragments(FMassFragmentBitSet& OutBitSet) const
		{
			OutBitSet += FMassFragmentBitSet::GetTypeBitSet<FType>();
		}

		void Reset()
		{
			FragmentInstances.Reset();
		}

		TArray<FType> FragmentInstances;
	};

} // UE::Mass


struct FMassArchetypeCreationParams
{
	FMassArchetypeCreationParams() = default;
	explicit FMassArchetypeCreationParams(const struct FMassArchetypeData& Archetype);

	/** Created archetype will have chunks of this size. 0 denotes "use default" (see UE::Mass::ChunkSize) */
	int32 ChunkMemorySize = 0;

	/** Name to identify the archetype while debugging*/
	FName DebugName;

#if WITH_MASSENTITY_DEBUG
	FColor DebugColor{0};
#endif // WITH_MASSENTITY_DEBUG
};

//-----------------------------------------------------------------------------
// INLINES
//-----------------------------------------------------------------------------
template<typename T>
auto& FMassArchetypeCompositionDescriptor::GetContainer() const
{
	if constexpr (std::is_same_v<FMassFragment, T>)
	{
		return Fragments;
	}
	else if constexpr (std::is_same_v<FMassTag, T>)
	{
		return Tags;
	}
	else if constexpr (std::is_same_v<FMassChunkFragment, T>)
	{
		return ChunkFragments;
	}
	else if constexpr (std::is_same_v<FMassSharedFragment, T>)
	{
		return SharedFragments;
	}
	else if constexpr (std::is_same_v<FMassConstSharedFragment, T>)
	{
		return ConstSharedFragments;
	}
	else
	{
		static_assert(UE::Mass::TAlwaysFalse<T>, "Unknown element type passed to GetContainer.");
	}
}

template<typename T>
auto& FMassArchetypeCompositionDescriptor::GetContainer()
{
	if constexpr (std::is_same_v<FMassFragment, T>)
	{
		return Fragments;
	}
	else if constexpr (std::is_same_v<FMassTag, T>)
	{
		return Tags;
	}
	else if constexpr (std::is_same_v<FMassChunkFragment, T>)
	{
		return ChunkFragments;
	}
	else if constexpr (std::is_same_v<FMassSharedFragment, T>)
	{
		return SharedFragments;
	}
	else if constexpr (std::is_same_v<FMassConstSharedFragment, T>)
	{
		return ConstSharedFragments;
	}
	else
	{
		static_assert(UE::Mass::TAlwaysFalse<T>, "Unknown element type passed to GetContainer.");
	}
}

template<typename T>
bool FMassArchetypeCompositionDescriptor::Contains() const
{
	using FElementType = UE::Mass::TElementType<T>;
	return GetContainer<FElementType>().template Contains<T>();
}

template<typename T>
void FMassArchetypeCompositionDescriptor::Add()
{
	using FElementType = UE::Mass::TElementType<T>;
	GetContainer<FElementType>().template Add<T>();
}

inline SIZE_T FMassArchetypeSharedFragmentValues::GetAllocatedSize() const
{
	return SharedFragmentBitSet.GetAllocatedSize()
		+ ConstSharedFragmentBitSet.GetAllocatedSize()
		+ ConstSharedFragments.GetAllocatedSize()
		+ SharedFragments.GetAllocatedSize();
}

inline bool FMassArchetypeSharedFragmentValues::IsSorted() const
{
	return bSorted;
}

inline bool FMassArchetypeSharedFragmentValues::IsEmpty() const
{
	return ConstSharedFragments.IsEmpty() && SharedFragments.IsEmpty();
}

inline void FMassArchetypeSharedFragmentValues::Reset()
{
	HashCache = UINT32_MAX;
	bSorted = false; 
	SharedFragmentBitSet.Reset();
	ConstSharedFragmentBitSet.Reset();
	ConstSharedFragments.Reset();
	SharedFragments.Reset();
}

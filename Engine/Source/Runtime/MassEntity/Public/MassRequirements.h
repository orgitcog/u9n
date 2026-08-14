// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassExternalSubsystemTraits.h"
#include "Templates/SubclassOf.h"
#include "MassRequirements.generated.h"

struct FMassDebugger;
struct FMassArchetypeHandle;
struct FMassExecutionRequirements;
struct FMassRequirementAccessDetector;
class USubsystem;

UENUM()
enum class EMassFragmentAccess : uint8
{
	/** no binding required */
	None, 

	/** We want to read the data for the fragment */
	ReadOnly,

	/** We want to read and write the data for the fragment */
	ReadWrite,

	MAX
};

UENUM()
enum class EMassFragmentPresence : uint8
{
	/** All the required fragments must be present */
	All,

	/** One of the required fragments must be present */
	Any,

	/** None of the required fragments can be present */
	None,

	/** If fragment is present we'll use it */
	Optional,

	MAX
};


struct FMassFragmentRequirementDescription
{
	FMassFragmentRequirementDescription() = default;
	FMassFragmentRequirementDescription(const UScriptStruct* InStruct, const EMassFragmentAccess InAccessMode, const EMassFragmentPresence InPresence);

	bool RequiresBinding() const;
	bool IsOptional() const;

	/** these functions are used for sorting. See FScriptStructSortOperator */
	int32 GetStructureSize() const;

	FName GetFName() const;

	const UScriptStruct* StructType = nullptr;
	EMassFragmentAccess AccessMode = EMassFragmentAccess::None;
	EMassFragmentPresence Presence = EMassFragmentPresence::Optional;
};

/**
 *  FMassSubsystemRequirements is a structure that declares runtime subsystem access type given calculations require.
 */
USTRUCT()
struct FMassSubsystemRequirements
{
	GENERATED_BODY()

	friend FMassDebugger;
	friend FMassRequirementAccessDetector;

	template<typename T>
	FMassSubsystemRequirements& AddSubsystemRequirement(const EMassFragmentAccess AccessMode)
	{
		check(AccessMode != EMassFragmentAccess::None && AccessMode != EMassFragmentAccess::MAX);

		// Compilation errors here like: 'GameThreadOnly': is not a member of 'TMassExternalSubsystemTraits<USmartObjectSubsystem>
		// indicate that there is a missing header that defines the subsystem's trait or that you need to define one for that subsystem type.
		// @see "MassExternalSubsystemTraits.h" for details

		switch (AccessMode)
		{
		case EMassFragmentAccess::ReadOnly:
			RequiredConstSubsystems.Add<T>();
			bRequiresGameThreadExecution |= TMassExternalSubsystemTraits<T>::GameThreadOnly;
			break;
		case EMassFragmentAccess::ReadWrite:
			RequiredMutableSubsystems.Add<T>();
			bRequiresGameThreadExecution |= TMassExternalSubsystemTraits<T>::GameThreadOnly;
			break;
		default:
			check(false);
		}

		return *this;
	}

	FMassSubsystemRequirements& AddSubsystemRequirement(const TSubclassOf<USubsystem> SubsystemClass, const EMassFragmentAccess AccessMode, const bool bGameThreadOnly)
	{
		check(AccessMode != EMassFragmentAccess::None && AccessMode != EMassFragmentAccess::MAX);

		switch (AccessMode)
		{
		case EMassFragmentAccess::ReadOnly:
			RequiredConstSubsystems.Add(**SubsystemClass);
			bRequiresGameThreadExecution |= bGameThreadOnly;
			break;
		case EMassFragmentAccess::ReadWrite:
			RequiredMutableSubsystems.Add(**SubsystemClass);
			bRequiresGameThreadExecution |= bGameThreadOnly;
			break;
		default:
			check(false);
		}

		return *this;
	}

	FMassSubsystemRequirements& AddSubsystemRequirement(const TSubclassOf<USubsystem> SubsystemClass, const EMassFragmentAccess AccessMode, const TSharedRef<FMassEntityManager>& EntityManager)
	{
		return AddSubsystemRequirement(SubsystemClass, AccessMode, IsGameThreadOnlySubsystem(SubsystemClass, EntityManager));
	}

	UE_DEPRECATED(5.6, "This flavor of FMassSubsystemRequirements::AddSubsystemRequirement is deprecated. Use one of the other flavors, or call FMassEntityQuery::AddSubsystemRequirement if applicable.")
	FMassSubsystemRequirements& AddSubsystemRequirement(const TSubclassOf<USubsystem> SubsystemClass, const EMassFragmentAccess AccessMode)
	{
		return AddSubsystemRequirement(SubsystemClass, AccessMode, /*bGameThreadOnly=*/true);
	}

	MASSENTITY_API void Reset();

	const FMassExternalSubsystemBitSet& GetRequiredConstSubsystems() const;
	const FMassExternalSubsystemBitSet& GetRequiredMutableSubsystems() const;
	bool IsEmpty() const;

	bool DoesRequireGameThreadExecution() const;
	MASSENTITY_API void ExportRequirements(FMassExecutionRequirements& OutRequirements) const;

	friend uint32 GetTypeHash(const FMassSubsystemRequirements& Instance);

protected:
	MASSENTITY_API static bool IsGameThreadOnlySubsystem(const TSubclassOf<USubsystem> SubsystemClass, const TSharedRef<FMassEntityManager>& EntityManager);

	FMassExternalSubsystemBitSet RequiredConstSubsystems;
	FMassExternalSubsystemBitSet RequiredMutableSubsystems;

private:
	bool bRequiresGameThreadExecution = false;
};

/** 
 *  FMassFragmentRequirements is a structure that describes properties required of an archetype that's a subject of calculations.
 */
USTRUCT()
struct FMassFragmentRequirements
{
	GENERATED_BODY()

	friend FMassDebugger;
	friend FMassRequirementAccessDetector;

public:
	FMassFragmentRequirements() = default;
	MASSENTITY_API explicit FMassFragmentRequirements(const TSharedPtr<FMassEntityManager>& EntityManager);
	MASSENTITY_API explicit FMassFragmentRequirements(const TSharedRef<FMassEntityManager>& EntityManager);

	MASSENTITY_API void Initialize(const TSharedRef<FMassEntityManager>& EntityManager);

	FMassFragmentRequirements& AddRequirement(const UScriptStruct* FragmentType, const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(FragmentRequirements.FindByPredicate([FragmentType](const FMassFragmentRequirementDescription& Item){ return Item.StructType == FragmentType; }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *GetNameSafe(FragmentType));
		
		if (Presence != EMassFragmentPresence::None)
		{
			FragmentRequirements.Emplace(FragmentType, AccessMode, Presence);
		}

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllFragments.Add(*FragmentType);
			break;
		case EMassFragmentPresence::Any:
			RequiredAnyFragments.Add(*FragmentType);
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalFragments.Add(*FragmentType);
			break;
		case EMassFragmentPresence::None:
			RequiredNoneFragments.Add(*FragmentType);
			break;
		}
		// force recaching the next time this query is used or the following CacheArchetypes call.
		IncrementChangeCounter();
		return *this;
	}

	/** FMassFragmentRequirements ref returned for chaining */
	template<typename T>
	FMassFragmentRequirements& AddRequirement(const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(FragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());

		static_assert(UE::Mass::CFragment<T>, "Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");
		
		if (Presence != EMassFragmentPresence::None)
		{
			FragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
		}
		
		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllFragments.Add<T>();
			break;
		case EMassFragmentPresence::Any:
			RequiredAnyFragments.Add<T>();
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalFragments.Add<T>();
			break;
		case EMassFragmentPresence::None:
			RequiredNoneFragments.Add<T>();
			break;
		}
		// force recaching the next time this query is used or the following CacheArchetypes call.
		IncrementChangeCounter();
		return *this;
	}

	void AddTagRequirement(const UScriptStruct& TagType, const EMassFragmentPresence Presence)
	{
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(int(Presence) != int(EMassFragmentPresence::MAX), TEXT("MAX presence is not a valid value for AddTagRequirement"));
		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllTags.Add(TagType);
			break;
		case EMassFragmentPresence::Any:
			RequiredAnyTags.Add(TagType);
			break;
		case EMassFragmentPresence::None:
			RequiredNoneTags.Add(TagType);
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalTags.Add(TagType);
			break;
		}
		IncrementChangeCounter();
	}

	template<typename T>
	FMassFragmentRequirements& AddTagRequirement(const EMassFragmentPresence Presence)
	{
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(int(Presence) != int(EMassFragmentPresence::MAX), TEXT("MAX presence is not a valid value for AddTagRequirement"));
		static_assert(UE::Mass::CTag<T>, "Given struct doesn't represent a valid tag type. Make sure to inherit from FMassFragment or one of its child-types.");
		switch (Presence)
		{
			case EMassFragmentPresence::All:
				RequiredAllTags.Add<T>();
				break;
			case EMassFragmentPresence::Any:
				RequiredAnyTags.Add<T>();
				break;
			case EMassFragmentPresence::None:
				RequiredNoneTags.Add<T>();
				break;
			case EMassFragmentPresence::Optional:
				RequiredOptionalTags.Add<T>();
				break;
		}
		IncrementChangeCounter();
		return *this;
	}

	/** actual implementation in specializations */
	template<EMassFragmentPresence Presence> 
	FMassFragmentRequirements& AddTagRequirements(const FMassTagBitSet& TagBitSet)
	{
		static_assert(Presence == EMassFragmentPresence::None || Presence == EMassFragmentPresence::All || Presence == EMassFragmentPresence::Any
			, "The only valid values for AddTagRequirements are All, Any and None");
		return *this;
	}

	/** Clears given tags out of all collected requirements, including negative ones */
	MASSENTITY_API FMassFragmentRequirements& ClearTagRequirements(const FMassTagBitSet& TagsToRemoveBitSet);

	template<typename T>
	FMassFragmentRequirements& AddChunkRequirement(const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		static_assert(UE::Mass::CChunkFragment<T>, "Given struct doesn't represent a valid chunk fragment type. Make sure to inherit from FMassChunkFragment or one of its child-types.");
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(ChunkFragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());
		checkf(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddChunkRequirement."));

		switch (Presence)
		{
			case EMassFragmentPresence::All:
				RequiredAllChunkFragments.Add<T>();
				ChunkFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
				break;
			case EMassFragmentPresence::Optional:
				RequiredOptionalChunkFragments.Add<T>();
				ChunkFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
				break;
			case EMassFragmentPresence::None:
				RequiredNoneChunkFragments.Add<T>();
				break;
		}
		IncrementChangeCounter();
		return *this;
	}

	FMassFragmentRequirements& AddChunkRequirement(TNotNull<const UScriptStruct*> ChunkFragmentType, const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(ChunkFragmentRequirements.FindByPredicate([&ChunkFragmentType](const FMassFragmentRequirementDescription& Item) { return Item.StructType == ChunkFragmentType; }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *ChunkFragmentType->GetName());
		checkf(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddChunkRequirement."));

		switch (Presence)
		{
			case EMassFragmentPresence::All:
				RequiredAllChunkFragments.Add(*ChunkFragmentType);
				ChunkFragmentRequirements.Emplace(ChunkFragmentType, AccessMode, Presence);
				break;
			case EMassFragmentPresence::Optional:
				RequiredOptionalChunkFragments.Add(*ChunkFragmentType);
				ChunkFragmentRequirements.Emplace(ChunkFragmentType, AccessMode, Presence);
				break;
			case EMassFragmentPresence::None:
				RequiredNoneChunkFragments.Add(*ChunkFragmentType);
				break;
		}
		IncrementChangeCounter();
		return *this;
	}

	template<typename T>
	FMassFragmentRequirements& AddConstSharedRequirement(const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		static_assert(UE::Mass::CConstSharedFragment<T>, "Given struct doesn't represent a valid const shared fragment type. Make sure to inherit from FMassConstSharedFragment or one of its child-types.");
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(ConstSharedFragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());
		checkf(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddConstSharedRequirement."));

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllConstSharedFragments.Add<T>();
			ConstSharedFragmentRequirements.Emplace(T::StaticStruct(), EMassFragmentAccess::ReadOnly, Presence);
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalConstSharedFragments.Add<T>();
			ConstSharedFragmentRequirements.Emplace(T::StaticStruct(), EMassFragmentAccess::ReadOnly, Presence);
			break;
		case EMassFragmentPresence::None:
			RequiredNoneConstSharedFragments.Add<T>();
			break;
		}
		IncrementChangeCounter();
		return *this;
	}

	FMassFragmentRequirements& AddConstSharedRequirement(const UScriptStruct* FragmentType, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		if (!ensureMsgf(UE::Mass::IsA<FMassConstSharedFragment>(FragmentType)
			, TEXT("Given struct doesn't represent a valid const shared fragment type. Make sure to inherit from FMassConstSharedFragment or one of its child-types.")))
		{
			return *this;
		}

		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(ConstSharedFragmentRequirements.FindByPredicate([FragmentType](const FMassFragmentRequirementDescription& Item) { return Item.StructType == FragmentType; }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *FragmentType->GetName());
		checkf(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddConstSharedRequirement."));

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllConstSharedFragments.Add(*FragmentType);
			ConstSharedFragmentRequirements.Emplace(FragmentType, EMassFragmentAccess::ReadOnly, Presence);
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalConstSharedFragments.Add(*FragmentType);
			ConstSharedFragmentRequirements.Emplace(FragmentType, EMassFragmentAccess::ReadOnly, Presence);
			break;
		case EMassFragmentPresence::None:
			RequiredNoneConstSharedFragments.Add(*FragmentType);
			break;
		}
		IncrementChangeCounter();
		return *this;
	}

	template<typename T>
	FMassFragmentRequirements& AddSharedRequirement(const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		static_assert(UE::Mass::CSharedFragment<T>, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(SharedFragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());
		checkf(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddSharedRequirement."));

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllSharedFragments.Add<T>();
			SharedFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
			if (AccessMode == EMassFragmentAccess::ReadWrite)
			{
				bRequiresGameThreadExecution |= TMassSharedFragmentTraits<T>::GameThreadOnly;
			}
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalSharedFragments.Add<T>();
			SharedFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
			if (AccessMode == EMassFragmentAccess::ReadWrite)
			{
				bRequiresGameThreadExecution |= TMassSharedFragmentTraits<T>::GameThreadOnly;
			}
			break;
		case EMassFragmentPresence::None:
			RequiredNoneSharedFragments.Add<T>();
			break;
		}
		IncrementChangeCounter();
		return *this;
	}

	FMassFragmentRequirements& AddSharedRequirement(TNotNull<const UScriptStruct*> SharedFragmentType, const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		checkf(UE::Mass::IsA<FMassSharedFragment>(SharedFragmentType), TEXT("Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.")); 
		checkf(bInitialized, TEXT("Modifying requirements before initialization is not supported."));
		checkf(SharedFragmentRequirements.FindByPredicate([&SharedFragmentType](const FMassFragmentRequirementDescription& Item) { return Item.StructType == SharedFragmentType; }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *SharedFragmentType->GetName());
		checkf(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddSharedRequirement."));

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllSharedFragments.Add(*SharedFragmentType);
			SharedFragmentRequirements.Emplace(SharedFragmentType, AccessMode, Presence);
			if (AccessMode == EMassFragmentAccess::ReadWrite)
			{
				bRequiresGameThreadExecution |= IsGameThreadOnlySharedFragment(SharedFragmentType);
			}
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalSharedFragments.Add(*SharedFragmentType);
			SharedFragmentRequirements.Emplace(SharedFragmentType, AccessMode, Presence);
			if (AccessMode == EMassFragmentAccess::ReadWrite)
			{
				bRequiresGameThreadExecution |= IsGameThreadOnlySharedFragment(SharedFragmentType);
			}
			break;
		case EMassFragmentPresence::None:
			RequiredNoneSharedFragments.Add(*SharedFragmentType);
			break;
		}
		IncrementChangeCounter();
		return *this;
	}

	MASSENTITY_API void Reset();

	/** 
	 * The function validates requirements we make for queries. See the FMassFragmentRequirements struct description for details.
	 * Even though the code of the function is non-trivial the consecutive calls will be essentially free due to the result 
	 * being cached (note that the caching gets invalidated if the composition changes).
	 * @return whether this query's requirements follow the rules.
	 */
	MASSENTITY_API bool CheckValidity() const;

	TConstArrayView<FMassFragmentRequirementDescription> GetFragmentRequirements() const;
	TConstArrayView<FMassFragmentRequirementDescription> GetChunkFragmentRequirements() const;
	TConstArrayView<FMassFragmentRequirementDescription> GetConstSharedFragmentRequirements() const;
	TConstArrayView<FMassFragmentRequirementDescription> GetSharedFragmentRequirements() const;
	const FMassFragmentBitSet& GetRequiredAllFragments() const;
	const FMassFragmentBitSet& GetRequiredAnyFragments() const;
	const FMassFragmentBitSet& GetRequiredOptionalFragments() const;
	const FMassFragmentBitSet& GetRequiredNoneFragments() const;
	const FMassTagBitSet& GetRequiredAllTags() const;
	const FMassTagBitSet& GetRequiredAnyTags() const;
	const FMassTagBitSet& GetRequiredNoneTags() const;
	const FMassTagBitSet& GetRequiredOptionalTags() const;
	const FMassChunkFragmentBitSet& GetRequiredAllChunkFragments() const;
	const FMassChunkFragmentBitSet& GetRequiredOptionalChunkFragments() const;
	const FMassChunkFragmentBitSet& GetRequiredNoneChunkFragments() const;
	const FMassSharedFragmentBitSet& GetRequiredAllSharedFragments() const;
	const FMassSharedFragmentBitSet& GetRequiredOptionalSharedFragments() const;
	const FMassSharedFragmentBitSet& GetRequiredNoneSharedFragments() const;
	const FMassConstSharedFragmentBitSet& GetRequiredAllConstSharedFragments() const;
	const FMassConstSharedFragmentBitSet& GetRequiredOptionalConstSharedFragments() const;
	const FMassConstSharedFragmentBitSet& GetRequiredNoneConstSharedFragments() const;

	bool IsInitialized() const;
	MASSENTITY_API bool IsEmpty() const;
	bool HasPositiveRequirements() const;
	bool HasNegativeRequirements() const;
	bool HasOptionalRequirements() const;

	MASSENTITY_API bool DoesArchetypeMatchRequirements(const FMassArchetypeHandle& ArchetypeHandle) const;
	MASSENTITY_API bool DoesArchetypeMatchRequirements(const FMassArchetypeCompositionDescriptor& ArchetypeComposition) const;
	MASSENTITY_API bool DoesMatchAnyOptionals(const FMassArchetypeCompositionDescriptor& ArchetypeComposition) const;

	bool DoesRequireGameThreadExecution() const;
	MASSENTITY_API void ExportRequirements(FMassExecutionRequirements& OutRequirements) const;

	MASSENTITY_API friend uint32 GetTypeHash(const FMassFragmentRequirements& Instance);

protected:
	MASSENTITY_API void SortRequirements();

	void IncrementChangeCounter();
	void ConsumeIncrementalChangesCount();
	bool HasIncrementalChanges() const;
	
	/**
	 * A helper function that passes the query over to CachedEntityManager.
	 * Main purpose is to have the implementation in cpp and not include the EntityManager header here
	 * @todo this function always returns True at the moment, proper implementation waiting for implementation of "type trait information" (WIP)
	 */
	MASSENTITY_API bool IsGameThreadOnlySharedFragment(TNotNull<const UScriptStruct*> SharedFragmentType) const;

	friend FMassRequirementAccessDetector;

	TArray<FMassFragmentRequirementDescription> FragmentRequirements;
	TArray<FMassFragmentRequirementDescription> ChunkFragmentRequirements;
	TArray<FMassFragmentRequirementDescription> ConstSharedFragmentRequirements;
	TArray<FMassFragmentRequirementDescription> SharedFragmentRequirements;
	FMassTagBitSet RequiredAllTags;
	FMassTagBitSet RequiredAnyTags;
	FMassTagBitSet RequiredNoneTags;
	/**
	 * note that optional tags have meaning only if there are no other strict requirements, i.e. everything is optional,
	 * so we're looking for anything matching any of the optionals (both tags as well as fragments).
	 */
	FMassTagBitSet RequiredOptionalTags;
	FMassFragmentBitSet RequiredAllFragments;
	FMassFragmentBitSet RequiredAnyFragments;
	FMassFragmentBitSet RequiredOptionalFragments;
	FMassFragmentBitSet RequiredNoneFragments;
	FMassChunkFragmentBitSet RequiredAllChunkFragments;
	FMassChunkFragmentBitSet RequiredOptionalChunkFragments;
	FMassChunkFragmentBitSet RequiredNoneChunkFragments;
	FMassSharedFragmentBitSet RequiredAllSharedFragments;
	FMassSharedFragmentBitSet RequiredOptionalSharedFragments;
	FMassSharedFragmentBitSet RequiredNoneSharedFragments;
	FMassConstSharedFragmentBitSet RequiredAllConstSharedFragments;
	FMassConstSharedFragmentBitSet RequiredOptionalConstSharedFragments;
	FMassConstSharedFragmentBitSet RequiredNoneConstSharedFragments;

	TSharedPtr<FMassEntityManager> CachedEntityManager;

private:
	MASSENTITY_API void CacheProperties() const;
	mutable uint16 bPropertiesCached : 1 = false;
	mutable uint16 bHasPositiveRequirements : 1 = false;
	mutable uint16 bHasNegativeRequirements : 1 = false;
	/** 
	 * Indicates that the requirements specify only optional elements, which means any composition having any one of 
	 * the optional elements will be accepted. Note that RequiredNone* requirements are handled separately and if specified 
	 * still need to be satisfied.
	 */
	mutable uint16 bHasOptionalRequirements : 1 = false;

	uint16 bInitialized : 1 = false;
	uint16 IncrementalChangesCount = 0;

	bool bRequiresGameThreadExecution = false;

public:
	UE_DEPRECATED(5.6, "This type of FMassFragmentRequirements is no longer supported. Use one of the other constructors instead.")
	MASSENTITY_API FMassFragmentRequirements(std::initializer_list<UScriptStruct*> InitList);

	UE_DEPRECATED(5.6, "This type of FMassFragmentRequirements is no longer supported. Use one of the other constructors instead.")
	MASSENTITY_API FMassFragmentRequirements(TConstArrayView<const UScriptStruct*> InitList);
};

//-----------------------------------------------------------------------------
// INLINE
//-----------------------------------------------------------------------------
inline FMassFragmentRequirementDescription::FMassFragmentRequirementDescription(const UScriptStruct* InStruct, const EMassFragmentAccess InAccessMode, const EMassFragmentPresence InPresence)
	: StructType(InStruct)
	, AccessMode(InAccessMode)
	, Presence(InPresence)
{
	check(InStruct);
}

inline bool FMassFragmentRequirementDescription::RequiresBinding() const
{
	return (AccessMode != EMassFragmentAccess::None);
}

inline bool FMassFragmentRequirementDescription::IsOptional() const
{
	return (Presence == EMassFragmentPresence::Optional || Presence == EMassFragmentPresence::Any);
}

inline int32 FMassFragmentRequirementDescription::GetStructureSize() const
{
	return StructType->GetStructureSize();
}

inline FName FMassFragmentRequirementDescription::GetFName() const
{
	return StructType->GetFName();
}

inline const FMassExternalSubsystemBitSet& FMassSubsystemRequirements::GetRequiredConstSubsystems() const
{
	return RequiredConstSubsystems;
}

inline const FMassExternalSubsystemBitSet& FMassSubsystemRequirements::GetRequiredMutableSubsystems() const
{
	return RequiredMutableSubsystems;
}

inline bool FMassSubsystemRequirements::IsEmpty() const
{
	return RequiredConstSubsystems.IsEmpty() && RequiredMutableSubsystems.IsEmpty();
}

inline bool FMassSubsystemRequirements::DoesRequireGameThreadExecution() const
{
	return bRequiresGameThreadExecution;
}

inline uint32 GetTypeHash(const FMassSubsystemRequirements& Instance)
{
	return HashCombine(GetTypeHash(Instance.RequiredConstSubsystems), GetTypeHash(Instance.RequiredMutableSubsystems));
}

template<>
FORCEINLINE FMassFragmentRequirements& FMassFragmentRequirements::AddTagRequirements<EMassFragmentPresence::All>(const FMassTagBitSet& TagBitSet)
{
	RequiredAllTags += TagBitSet;
	// force re-caching the next time this query is used or the following CacheArchetypes call.
	IncrementChangeCounter();
	return *this;
}

template<>
FORCEINLINE FMassFragmentRequirements& FMassFragmentRequirements::AddTagRequirements<EMassFragmentPresence::Any>(const FMassTagBitSet& TagBitSet)
{
	RequiredAnyTags += TagBitSet;
	// force re-caching the next time this query is used or the following CacheArchetypes call.
	IncrementChangeCounter();
	return *this;
}

template<>
FORCEINLINE FMassFragmentRequirements& FMassFragmentRequirements::AddTagRequirements<EMassFragmentPresence::None>(const FMassTagBitSet& TagBitSet)
{
	RequiredNoneTags += TagBitSet;
	// force re-caching the next time this query is used or the following CacheArchetypes call.
	IncrementChangeCounter();
	return *this;
}

template<>
FORCEINLINE FMassFragmentRequirements& FMassFragmentRequirements::AddTagRequirements<EMassFragmentPresence::Optional>(const FMassTagBitSet& TagBitSet)
{
	RequiredOptionalTags += TagBitSet;
	// force re-caching the next time this query is used or the following CacheArchetypes call.
	IncrementChangeCounter();
	return *this;
}

inline TConstArrayView<FMassFragmentRequirementDescription> FMassFragmentRequirements::GetFragmentRequirements() const
{ 
	return FragmentRequirements; 
}

inline TConstArrayView<FMassFragmentRequirementDescription> FMassFragmentRequirements::GetChunkFragmentRequirements() const
{ 
	return ChunkFragmentRequirements; 
}

inline TConstArrayView<FMassFragmentRequirementDescription> FMassFragmentRequirements::GetConstSharedFragmentRequirements() const
{ 
	return ConstSharedFragmentRequirements; 
}

inline TConstArrayView<FMassFragmentRequirementDescription> FMassFragmentRequirements::GetSharedFragmentRequirements() const
{ 
	return SharedFragmentRequirements; 
}

inline const FMassFragmentBitSet& FMassFragmentRequirements::GetRequiredAllFragments() const
{ 
	return RequiredAllFragments; 
}

inline const FMassFragmentBitSet& FMassFragmentRequirements::GetRequiredAnyFragments() const
{ 
	return RequiredAnyFragments; 
}

inline const FMassFragmentBitSet& FMassFragmentRequirements::GetRequiredOptionalFragments() const
{ 
	return RequiredOptionalFragments; 
}

inline const FMassFragmentBitSet& FMassFragmentRequirements::GetRequiredNoneFragments() const
{ 
	return RequiredNoneFragments; 
}

inline const FMassTagBitSet& FMassFragmentRequirements::GetRequiredAllTags() const
{ 
	return RequiredAllTags; 
}

inline const FMassTagBitSet& FMassFragmentRequirements::GetRequiredAnyTags() const
{ 
	return RequiredAnyTags; 
}

inline const FMassTagBitSet& FMassFragmentRequirements::GetRequiredNoneTags() const
{ 
	return RequiredNoneTags; 
}

inline const FMassTagBitSet& FMassFragmentRequirements::GetRequiredOptionalTags() const
{ 
	return RequiredOptionalTags; 
}

inline const FMassChunkFragmentBitSet& FMassFragmentRequirements::GetRequiredAllChunkFragments() const
{ 
	return RequiredAllChunkFragments; 
}

inline const FMassChunkFragmentBitSet& FMassFragmentRequirements::GetRequiredOptionalChunkFragments() const
{ 
	return RequiredOptionalChunkFragments; 
}

inline const FMassChunkFragmentBitSet& FMassFragmentRequirements::GetRequiredNoneChunkFragments() const
{ 
	return RequiredNoneChunkFragments; 
}

inline const FMassSharedFragmentBitSet& FMassFragmentRequirements::GetRequiredAllSharedFragments() const
{ 
	return RequiredAllSharedFragments; 
}

inline const FMassSharedFragmentBitSet& FMassFragmentRequirements::GetRequiredOptionalSharedFragments() const
{ 
	return RequiredOptionalSharedFragments; 
}

inline const FMassSharedFragmentBitSet& FMassFragmentRequirements::GetRequiredNoneSharedFragments() const
{ 
	return RequiredNoneSharedFragments; 
}

inline const FMassConstSharedFragmentBitSet& FMassFragmentRequirements::GetRequiredAllConstSharedFragments() const
{ 
	return RequiredAllConstSharedFragments; 
}

inline const FMassConstSharedFragmentBitSet& FMassFragmentRequirements::GetRequiredOptionalConstSharedFragments() const
{ 
	return RequiredOptionalConstSharedFragments; 
}

inline const FMassConstSharedFragmentBitSet& FMassFragmentRequirements::GetRequiredNoneConstSharedFragments() const
{ 
	return RequiredNoneConstSharedFragments; 
}

inline bool FMassFragmentRequirements::IsInitialized() const 
{ 
	return bInitialized; 
}

inline bool FMassFragmentRequirements::HasPositiveRequirements() const 
{ 
	return bHasPositiveRequirements; 
}

inline bool FMassFragmentRequirements::HasNegativeRequirements() const 
{ 
	return bHasNegativeRequirements; 
}

inline bool FMassFragmentRequirements::HasOptionalRequirements() const 
{ 
	return bHasOptionalRequirements; 
}

inline bool FMassFragmentRequirements::DoesRequireGameThreadExecution() const
{
	return bRequiresGameThreadExecution;
}

FORCEINLINE void FMassFragmentRequirements::IncrementChangeCounter()
{ 
	++IncrementalChangesCount; 
	bPropertiesCached = false;
}

inline void FMassFragmentRequirements::ConsumeIncrementalChangesCount()
{
	IncrementalChangesCount = 0;
}

inline bool FMassFragmentRequirements::HasIncrementalChanges() const
{
	return IncrementalChangesCount > 0;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassArchetypeTypes.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "StructUtils/InstancedStruct.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassEntityView.generated.h"

#define UE_API MASSENTITY_API

struct FMassEntityManager;
struct FMassArchetypeData;
struct FMassArchetypeHandle;

/** 
 * The type representing a single entity in a single archetype. It's of a very transient nature so we guarantee it's 
 * validity only within the scope it has been created in. Don't store it. 
 */
USTRUCT()
struct FMassEntityView
{
	GENERATED_BODY()

	FMassEntityView() = default;

	/** 
	 *  Resolves Entity against ArchetypeHandle. Note that this approach requires the caller to ensure that Entity
	 *  indeed belongs to ArchetypeHandle. If not the call will fail a check. As a remedy calling the 
	 *  FMassEntityManager-flavored constructor is recommended since it will first find the appropriate archetype for
	 *  Entity. 
	 */
	UE_API FMassEntityView(const FMassArchetypeHandle& ArchetypeHandle, FMassEntityHandle Entity);

	/** 
	 *  Finds the archetype Entity belongs to and then resolves against it. The caller is responsible for ensuring
	 *  that the given Entity is in fact a valid ID tied to any of the archetypes 
	 */
	UE_API FMassEntityView(const FMassEntityManager& EntityManager, FMassEntityHandle Entity);

	/** 
	 * If the given handle represents a valid entity the function will create a FMassEntityView just like a constructor 
	 * would. If the entity is not valid the produced view will be "unset".
	 */
	static UE_API FMassEntityView TryMakeView(const FMassEntityManager& EntityManager, FMassEntityHandle Entity);

	FMassEntityHandle GetEntity() const	{ return Entity; }

	/** will fail a check if the viewed entity doesn't have the given fragment */	
	template<typename T>
	T& GetFragmentData() const
	{
		static_assert(!UE::Mass::CTag<T>,
			"Given struct doesn't represent a valid fragment type but a tag. Use HasTag instead.");
		static_assert(UE::Mass::CFragment<T>,
			"Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");

		return *((T*)GetFragmentPtrChecked(*T::StaticStruct()));
	}
		
	/** if the viewed entity doesn't have the given fragment the function will return null */
	template<typename T>
	T* GetFragmentDataPtr() const
	{
		static_assert(!UE::Mass::CTag<T>,
			"Given struct doesn't represent a valid fragment type but a tag. Use HasTag instead.");
		static_assert(UE::Mass::CFragment<T>,
			"Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");

		return (T*)GetFragmentPtr(*T::StaticStruct());
	}

	FStructView GetFragmentDataStruct(const UScriptStruct* FragmentType) const
	{
		check(FragmentType);
		return FStructView(FragmentType, static_cast<uint8*>(GetFragmentPtr(*FragmentType)));
	}

	/** if the viewed entity doesn't have the given const shared fragment the function will return null */
	template<typename T>
	const T* GetConstSharedFragmentDataPtr() const
	{
		static_assert(UE::Mass::CConstSharedFragment<T>,
			"Given struct doesn't represent a valid const shared fragment type. Make sure to inherit from FMassConstSharedFragment or one of its child-types.");

		return (const T*)GetConstSharedFragmentPtr(*T::StaticStruct());
	}

	/** will fail a check if the viewed entity doesn't have the given const shared fragment */
	template<typename T>
	const T& GetConstSharedFragmentData() const
	{
		static_assert(UE::Mass::CConstSharedFragment<T>,
			"Given struct doesn't represent a valid const shared fragment type. Make sure to inherit from FMassConstSharedFragment or one of its child-types.");

		return *((const T*)GetConstSharedFragmentPtrChecked(*T::StaticStruct()));
	}

	FConstStructView GetConstSharedFragmentDataStruct(const UScriptStruct* FragmentType) const
	{
		check(UE::Mass::IsA<FMassConstSharedFragment>(FragmentType));
		return FConstStructView(FragmentType, static_cast<const uint8*>(GetConstSharedFragmentPtr(*FragmentType)));
	}

	/** will fail a check if the viewed entity doesn't have the given shared fragment */
	template<UE::Mass::CSharedFragment T>
	T& GetSharedFragmentData() const
	{
		return *((T*)GetSharedFragmentPtrChecked(*T::StaticStruct()));
	}

	/** if the viewed entity doesn't have the given shared fragment the function will return null */
	template<UE::Mass::CSharedFragment T>
	T* GetSharedFragmentDataPtr() const
	{
		return (T*)GetSharedFragmentPtr(*T::StaticStruct());
	}

	template<UE::Mass::CConstSharedFragment T>
	UE_DEPRECATED(5.5, "Using GetSharedFragmentDataPtr with const shared fragments is deprecated. Use GetConstSharedFragmentDataPtr instead")
	T* GetSharedFragmentDataPtr() const
	{
		return const_cast<T*>(GetConstSharedFragmentDataPtr<T>());
	}

	template<UE::Mass::CConstSharedFragment T>
	UE_DEPRECATED(5.5, "Using GetSharedFragmentDataPtr with const shared fragments is deprecated. Use GetConstSharedFragmentData instead")
	T& GetSharedFragmentData() const
	{
		return const_cast<T&>(GetConstSharedFragmentData<T>());
	}

	FStructView GetSharedFragmentDataStruct(const UScriptStruct* FragmentType) const
	{
		check(UE::Mass::IsA<FMassSharedFragment>(FragmentType));
		return FStructView(FragmentType, static_cast<uint8*>(GetSharedFragmentPtr(*FragmentType)));
	}

	template<typename T>
	bool HasTag() const
	{
		static_assert(UE::Mass::CTag<T>, "Given struct doesn't represent a valid tag type. Make sure to inherit from FMassTag or one of its child-types.");
		return HasTag(*T::StaticStruct());
	}

	UE_API bool HasTag(const UScriptStruct& TagType) const;

	bool IsSet() const;
	bool IsValid() const;
	bool operator==(const FMassEntityView& Other) const;

protected:
	UE_API void* GetFragmentPtr(const UScriptStruct& FragmentType) const;
	UE_API void* GetFragmentPtrChecked(const UScriptStruct& FragmentType) const;
	UE_API const void* GetConstSharedFragmentPtr(const UScriptStruct& FragmentType) const;
	UE_API const void* GetConstSharedFragmentPtrChecked(const UScriptStruct& FragmentType) const;
	UE_API void* GetSharedFragmentPtr(const UScriptStruct& FragmentType) const;
	UE_API void* GetSharedFragmentPtrChecked(const UScriptStruct& FragmentType) const;

private:
	FMassEntityHandle Entity;
	FMassEntityInChunkDataHandle EntityDataHandle;
	FMassArchetypeData* Archetype = nullptr;
};

//-----------------------------------------------------------------------------
// INLINES
//-----------------------------------------------------------------------------
inline bool FMassEntityView::IsSet() const
{
	return EntityDataHandle.IsValid(Archetype);
}

inline bool FMassEntityView::IsValid() const
{
	return IsSet();
}

inline bool FMassEntityView::operator==(const FMassEntityView& Other) const
{
	return Archetype == Other.Archetype && EntityDataHandle == Other.EntityDataHandle;
}

#undef UE_API

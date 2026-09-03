// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"
#include "UObject/ObjectKey.h"
#include "Misc/NotNull.h"
#include "Misc/TVariant.h"
#include "Templates/SharedPointer.h"
#include "MassEntityConcepts.h"
#include "Subsystems/Subsystem.h"
#include "MassExternalSubsystemTraits.h"

#define UE_API MASSENTITY_API


struct FMassEntityManager;

namespace UE::Mass
{
	/** Handle for identifying and managing types in the type manager */
	struct FTypeHandle
	{
		FTypeHandle() = default;

		bool operator==(const FTypeHandle&) const = default;

		bool IsValid() const;

		const UClass* GetClass() const
		{
			return Cast<const UClass>(TypeKey.ResolveObjectPtr());
		}

		const UScriptStruct* GetScriptStruct() const
		{
			return Cast<UScriptStruct>(TypeKey.ResolveObjectPtr());
		}

		friend FORCEINLINE uint32 GetTypeHash(const FTypeHandle& InHandle)
		{
			return GetTypeHash(InHandle.TypeKey);
		}

	private:
		friend struct FTypeManager;
		explicit FTypeHandle(TObjectKey<const UStruct> InTypeKey);

		TObjectKey<const UStruct> TypeKey;
	};

	/** Placeholder to be used when no traits have been specified nor the type is known */
	struct FEmptyTypeTraits
	{
	};

	/** Traits of USubsystem-based types */
	struct FSubsystemTypeTraits
	{
		FSubsystemTypeTraits() = default;

		/** Factory function for creating traits specific to a given subsystem type */
		template <typename T>
		static FSubsystemTypeTraits Make()
		{
			FSubsystemTypeTraits Traits;
			Traits.bGameThreadOnly = TMassExternalSubsystemTraits<T>::GameThreadOnly;
			Traits.bThreadSafeWrite = TMassExternalSubsystemTraits<T>::ThreadSafeWrite;
			return MoveTemp(Traits);
		}

		/** Whether the subsystem must  be run on the Game Thread */
		bool bGameThreadOnly = true;
		/** Whether the subsystem supports thread-safe write operations */
		bool bThreadSafeWrite = false;
	};

	/** Traits of Shared Fragment types */
	struct FSharedFragmentTypeTraits
	{
		FSharedFragmentTypeTraits() = default;

		/** Factory function for creating traits specific to a given shared fragment type */
		template<CSharedFragment T>
		static FSharedFragmentTypeTraits Make()
		{
			FSharedFragmentTypeTraits Traits;
			Traits.bGameThreadOnly = TMassSharedFragmentTraits<T>::GameThreadOnly;
			return MoveTemp(Traits);
		}

		/** Whether the shared fragment has to be used only on the Game Thread */
		bool bGameThreadOnly = true;
	};

	/**
	 * Wrapper for metadata and traits about specific types. The type is used by the TypeManager
	 * to uniformly store information for all types.
	 */
	struct FTypeInfo
	{
		using FTypeTraits = TVariant<FEmptyTypeTraits, FSubsystemTypeTraits, FSharedFragmentTypeTraits>;

		FName TypeName;
		FTypeTraits Traits;

		/** Fetches stored data as FSubsystemTypeTraits, if applicable, or null otherwise */
		const FSubsystemTypeTraits* GetAsSystemTraits() const;

		/** Fetches stored data as FSharedFragmentTypeTraits, if applicable, or null otherwise */
		const FSharedFragmentTypeTraits* GetAsSharedFragmentTraits() const;
	};

	struct FTypeManager : TSharedFromThis<FTypeManager>
	{
		UE_API explicit FTypeManager(FMassEntityManager& InEntityManager);

		/**
		 * @return whether the type manager instance has any types registered at all.
		 */
		bool IsEmpty() const;

		/** Register traits for given subsystem type */
		UE_API FTypeHandle RegisterType(TNotNull<const UStruct*> InType, FSubsystemTypeTraits&&);
		/** Register traits for given shared fragments type */
		UE_API FTypeHandle RegisterType(TNotNull<const UStruct*> InType, FSharedFragmentTypeTraits&&);

		/** Registration helper for shared fragments */
		template<CSharedFragment T>
		FTypeHandle RegisterType();

		/** Registration helper for subsystems */
		template<typename T> requires TIsDerivedFrom<typename TRemoveReference<T>::Type, USubsystem>::Value
		FTypeHandle RegisterType();

		/**
		 * @return stored traits for the type represented by TypeHandle, or null if the type is unknown
		 */
		const FTypeInfo* GetTypeInfo(FTypeHandle TypeHandle) const;

		/**
		 * @return stored traits for the type represented by TypeKey, or null if the type is unknown
		 */
		const FTypeInfo* GetTypeInfo(TObjectKey<const UStruct> TypeKey) const;

		/** Alias for the iterator type that can be used to iterate all of stored type traits information */
		using FTypeInfoConstIterator = TMap<FTypeHandle, FTypeInfo>::TConstIterator;

		/**
		 * Alias for the iterator type that can be used to iterate over stored subsystem types.
		 * Note that this iterator will only point to types, not type trait data. Contents of this iterator
		 * need to be used with GetTypeInfo to get actual traits data. 
		 */
		using FSubsystemTypeConstIterator = TSet<FTypeHandle>::TConstIterator;

		FTypeInfoConstIterator MakeIterator() const;
		FSubsystemTypeConstIterator MakeSubsystemIterator() const;

		FMassEntityManager& GetEntityManager();

	private:
		/** Register traits for given type */
		UE_API FTypeHandle RegisterTypeInternal(TNotNull<const UStruct*> InType, FTypeInfo&&);

		FMassEntityManager& OuterEntityManager; 

		/** Mapping of types to their info */
		TMap<FTypeHandle, FTypeInfo> TypeDataMap;

		/** Contains all registered subsystem types. Can be used to filter access to TypeDataMap */
		TSet<FTypeHandle> SubsystemTypes;
	};

	//----------------------------------------------------------------------------
	// INLINES
	//----------------------------------------------------------------------------
	inline bool FTypeHandle::IsValid() const
	{
		// using this strange contraption since TObjectKey doesn't supply a way to check
		// whether it's set, while comparison and construction is trivial.
		// Note: we don't care to what the key's been set, we don't expect types to go away
		return TypeKey != TObjectKey<const UStruct>();
	}

	inline const FSubsystemTypeTraits* FTypeInfo::GetAsSystemTraits() const
	{
		return Traits.TryGet<FSubsystemTypeTraits>();
	}

	inline const FSharedFragmentTypeTraits* FTypeInfo::GetAsSharedFragmentTraits() const
	{
		return Traits.TryGet<FSharedFragmentTypeTraits>();
	}

	inline bool FTypeManager::IsEmpty() const
	{
		return TypeDataMap.IsEmpty();
	}

	inline FTypeManager::FTypeInfoConstIterator FTypeManager::MakeIterator() const
	{
		return FTypeManager::FTypeInfoConstIterator(TypeDataMap);
	}

	inline FTypeManager::FSubsystemTypeConstIterator FTypeManager::MakeSubsystemIterator() const
	{
		return FTypeManager::FSubsystemTypeConstIterator(SubsystemTypes);
	}

	template<CSharedFragment T>
	FTypeHandle FTypeManager::RegisterType()
	{
		return RegisterType(T::StaticStruct(), FSharedFragmentTypeTraits::Make<T>());
	}

	template<typename T>
	requires TIsDerivedFrom<typename TRemoveReference<T>::Type, USubsystem>::Value
	FTypeHandle FTypeManager::RegisterType()
	{
		return RegisterType(T::StaticClass(), FSubsystemTypeTraits::Make<T>());
	}

	inline const FTypeInfo* FTypeManager::GetTypeInfo(FTypeHandle TypeHandle) const
	{
		return TypeDataMap.Find(TypeHandle);
	}

	inline const FTypeInfo* FTypeManager::GetTypeInfo(TObjectKey<const UStruct> TypeKey) const
	{
		return TypeDataMap.Find(FTypeHandle(TypeKey));
	}

	inline FMassEntityManager& FTypeManager::GetEntityManager()
	{
		return OuterEntityManager;
	}
}

#undef UE_API

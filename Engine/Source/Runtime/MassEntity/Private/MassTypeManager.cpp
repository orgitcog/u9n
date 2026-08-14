// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassTypeManager.h"
#include "MassEntityManager.h"
#include "MassTestableEnsures.h"
#include "MassEntityElementTypes.h"


namespace UE::Mass
{
	//-----------------------------------------------------------------------------
	// FTypeHandle
	//-----------------------------------------------------------------------------
	FTypeHandle::FTypeHandle(TObjectKey<const UStruct> InTypeKey)
		: TypeKey(InTypeKey)
	{
		
	}

	//-----------------------------------------------------------------------------
	// FTypeManager
	//-----------------------------------------------------------------------------
	FTypeManager::FTypeManager(FMassEntityManager& InEntityManager)
		: OuterEntityManager(InEntityManager)
	{
	}

	FTypeHandle FTypeManager::RegisterTypeInternal(TNotNull<const UStruct*> InType, FTypeInfo&& TypeInfo)
	{
		FTypeHandle TypeHandle(InType);
		FTypeInfo* ExistingData = TypeDataMap.Find(FTypeHandle(TypeHandle.TypeKey));
		if (LIKELY(ExistingData == nullptr))
		{
			if (TypeInfo.Traits.IsType<FSubsystemTypeTraits>())
			{
				SubsystemTypes.Add(FTypeHandle(TypeHandle.TypeKey));
			}

			TypeDataMap.Add(FTypeHandle(TypeHandle.TypeKey), MoveTemp(TypeInfo));
		}
		else
		{
			// we're overriding the existing data with the new data in assumption it's more up-to-date.
			// The most common occurence of this will be with already registered subsystems' subclasses.
			// The subclasses can change the data registered on their behalf by the super class,
			// but most of the time that won't be necessary.
			*ExistingData = MoveTemp(TypeInfo);
		}
		
		return TypeHandle;
	}

	FTypeHandle FTypeManager::RegisterType(TNotNull<const UStruct*> InType, FSubsystemTypeTraits&& TypeTraits)
	{
		FTypeInfo TypeInfo;
		TypeInfo.TypeName = InType->GetFName();
		TypeInfo.Traits.Set<FSubsystemTypeTraits>(MoveTemp(TypeTraits));
		return RegisterTypeInternal(InType, MoveTemp(TypeInfo));
	}

	FTypeHandle FTypeManager::RegisterType(TNotNull<const UStruct*> InType, FSharedFragmentTypeTraits&& TypeTraits)
	{
		testableCheckfReturn(UE::Mass::IsA<FMassSharedFragment>(InType), {}
			, TEXT("%s is not a valid shared fragment type"), *InType->GetName());

		FTypeInfo TypeInfo;
		TypeInfo.TypeName = InType->GetFName();
		TypeInfo.Traits.Set<FSharedFragmentTypeTraits>(MoveTemp(TypeTraits));
		return RegisterTypeInternal(InType, MoveTemp(TypeInfo));
	}
}
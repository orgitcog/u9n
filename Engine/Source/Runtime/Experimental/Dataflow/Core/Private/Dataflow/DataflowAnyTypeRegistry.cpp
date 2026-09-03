// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowAnyTypeRegistry.h"

namespace UE::Dataflow
{
	bool FAnyTypesRegistry::AreTypesCompatibleStatic(FName TypeA, FName TypeB)
	{
		return GetInstance().AreTypesCompatible(TypeA, TypeB);
	}
	
	FName FAnyTypesRegistry::GetStorageTypeStatic(FName Type)
	{
		return GetInstance().GetStorageType(Type);
	}

	bool FAnyTypesRegistry::IsAnyTypeStatic(FName Type)
	{
		return GetInstance().IsAnyType(Type);
	}

	FAnyTypesRegistry& FAnyTypesRegistry::GetInstance()
	{
		static FAnyTypesRegistry Instance;
		return Instance;
	}

	bool FAnyTypesRegistry::AreTypesCompatible(FName TypeA, FName TypeB) const
	{
		if (TypeA == TypeB)
		{
			return true;
		}
		if (const FTypeInfo* TypeInfoA = TypeInfosByName.Find(TypeA))
		{
			if (TypeInfoA->SupportTypeFunction && (*TypeInfoA->SupportTypeFunction)(TypeB))
			{
				return true;
			}
		}
		if (const FTypeInfo* TypeInfoB = TypeInfosByName.Find(TypeB))
		{
			if (TypeInfoB->SupportTypeFunction && (*TypeInfoB->SupportTypeFunction)(TypeA))
			{
				return true;
			}
		}
		return false;
	}

	FName FAnyTypesRegistry::GetStorageType(FName Type) const
	{
		if (const FTypeInfo* TypeInfo = TypeInfosByName.Find(Type))
		{
			return TypeInfo->StorageType;
		}
		return NAME_None;
	}

	bool FAnyTypesRegistry::IsAnyType(FName Type) const
	{
		return TypeInfosByName.Contains(Type);
	}
};

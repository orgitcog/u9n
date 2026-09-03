// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Containers/Map.h"
#include "Dataflow/DataflowTypePolicy.h"

namespace UE::Dataflow
{
	struct FAnyTypesRegistry
	{
	public:
		template<typename T>
		static void RegisterTypeStatic(FName TypeName)
		{
			GetInstance().RegisterType<T>(TypeName);
		}
		static bool AreTypesCompatibleStatic(FName TypeA, FName TypeB);
		static FName GetStorageTypeStatic(FName Type);
		static bool IsAnyTypeStatic(FName Type);
		DATAFLOWCORE_API static FAnyTypesRegistry& GetInstance();

	private:
		typedef bool(*FSupportTypeFunction)(FName);

		struct FTypeInfo
		{
			FSupportTypeFunction SupportTypeFunction = nullptr;
			FName StorageType;
		};

		FAnyTypesRegistry() {};

		template<typename T>
		void RegisterType(FName TypeName)
		{
			FTypeInfo TypeInfo
			{
				.SupportTypeFunction = &T::FPolicyType::SupportsTypeStatic,
				.StorageType = FName(TDataflowPolicyTypeName<typename T::FStorageType>::GetName()),
			};
			TypeInfosByName.Emplace(TypeName, TypeInfo);
		}

		bool AreTypesCompatible(FName TypeA, FName TypeB) const;
		FName GetStorageType(FName Type) const;
		bool IsAnyType(FName Type) const;

		TMap<FName, FTypeInfo> TypeInfosByName;
	};
};

#define UE_DATAFLOW_REGISTER_ANYTYPE(TYPE) UE::Dataflow::FAnyTypesRegistry::RegisterTypeStatic<TYPE>(#TYPE)

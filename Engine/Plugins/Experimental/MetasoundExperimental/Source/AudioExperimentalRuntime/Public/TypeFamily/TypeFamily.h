// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/RetainedRef.h"
#include "UObject/NameTypes.h"

namespace Audio
{
	class FTypeFamily
	{
		UE_NONCOPYABLE(FTypeFamily); 
		
		FName Name;
		const FTypeFamily* Parent = nullptr;
		FString FriendlyName;
		
		template<typename Predicate> const FTypeFamily* FindAncestor(Predicate Pred) const
		{
			for (const FTypeFamily* i=this; i; i=i->Parent)
			{
				if (::Invoke(Pred,i))
				{
					return i;
				}
			}
			return nullptr;
		}
	public:
		FTypeFamily(const FName InName, const FTypeFamily* InParent, const FString& InFriendlyName)
			: Name(InName)
			, Parent(InParent)
			, FriendlyName(InFriendlyName)
		{}

		bool IsA(const FTypeFamily* Other) const { return nullptr != FindAncestor([Other](const FTypeFamily* i) -> bool { return i==Other; }); }
		bool IsA(const FName& Other) const { return nullptr != FindAncestor([&](const FTypeFamily* i) -> bool { return i->Name==Other; }); }
		FName GetName() const { return Name; }
		FString GetFriendlyName() const { return FriendlyName; }
	};

	// Simple type registry.
	class ITypeFamilyRegistry
	{
	public:
		virtual ~ITypeFamilyRegistry() = default;
		virtual bool RegisterType(const FName& InUniqueName, const TRetainedRef<FTypeFamily> InType) = 0;

		template<typename T> const T* Find(const FName& InUniqueName) const
		{
			// TODO: test if this is valid by using FName Comparison.	
			return static_cast<const T*>(FindTypeInternal(InUniqueName));
		}
	protected:
		virtual const FTypeFamily* FindTypeInternal(const FName InUniqueName) const = 0;
	};

	// Convenience template so we don't have to specialize each Find. 
	template<typename T> struct TFamilyRegistry
	{
		ITypeFamilyRegistry& FamilyRegistry;
		explicit TFamilyRegistry(ITypeFamilyRegistry& InFamilyRegistry)
			: FamilyRegistry(InFamilyRegistry)
		{}
		const T* Find(const FName& InName) const
		{
			return FamilyRegistry.Find<T>(InName);
		}
		const T& FindChecked(const FName& InName) const
		{
			const T* Found = Find(InName);
			check(Found);
			return *Found;
		}
	};
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsDeclare.h"
#include "Algo/Compare.h"
#include "Containers/Set.h"

namespace PlainProps
{

// Note: For automated upgrade purposes it could be better to not strip out enum flag aliases,
//		 E.g. Saving E::All in E { A=1, B=2, All=A|B }, adding C=4, All=A|B|C and loading will load A|B
//		 It's impossible to know if a user set A|B or All when saving though, we only have the value 3.
//		 To really know, we'd need to instead save an enum oplog, i.e. {set A, set B} or {set All}
static TConstArrayView<FEnumerator> StripAliases(TArray<FEnumerator, TInlineAllocator<64>>& OutTmp, TConstArrayView<FEnumerator> In, EEnumMode Mode, const FDebugIds& Debug)
{
	TBitArray<> Aliases;
	if (Mode == EEnumMode::Flag)
	{
		bool bSeen0 = false;
		uint64 Seen = 0;
		for (FEnumerator E : In)
		{
			bool bAlias = E.Constant ? (Seen & E.Constant) == E.Constant : bSeen0;
			checkf(bAlias || FMath::CountBits(E.Constant) <= 1, TEXT("Flag enums must use one bit per enumerator, %s is %llx"), *Debug.Print(E.Name), E.Constant);
			
			Aliases.Add(bAlias);
			Seen |= E.Constant;
			bSeen0 |= E.Constant == 0;
		}
	}
	else
	{
		TSet<uint64, DefaultKeyFuncs<uint64>,TInlineSetAllocator<64>> Seen;
		for (FEnumerator E : In)
		{
			bool bAlias;
			Seen.FindOrAdd(E.Constant, /* out*/ &bAlias);
			Aliases.Add(bAlias);
		}
	}

	if (int32 NumAliases = Aliases.CountSetBits())
	{
		// All aliases are frequently at the end
		int32 FirstAlias = Aliases.Find(true);
		if (FirstAlias == In.Num() - NumAliases)
		{
			return In.Slice(0, FirstAlias);
		}

		// Aliases mixed in with values, make a copy and return it
		const FEnumerator* InIt = &In[0];
		for (bool bAlias : Aliases)
		{
			if (!bAlias)
			{
				OutTmp.Add(*InIt);
			}
			++InIt;
		}
		return OutTmp;
	}

	return In;
}

static void ValidateDeclaration(const FEnumDeclaration& Enum)
{
	if (Enum.Mode == EEnumMode::Flag)
	{
		for (FEnumerator E : Enum.GetEnumerators())
		{
			checkf(FMath::CountBits(E.Constant) <= 1, TEXT("Flag enums must use one bit per enumerator"));
		}
	}

	TSet<uint32, DefaultKeyFuncs<uint32>, TInlineSetAllocator<64>> Names;
	TSet<uint64, DefaultKeyFuncs<uint64>,TInlineSetAllocator<64>> Constants;
	for (FEnumerator E : Enum.GetEnumerators())
	{
		//checkf(FMath::FloorLog2_64(E.Constant) < 8 * SizeOf(Enum.Width), TEXT("Enumerator constant larger than declared width"));

		bool bDeclared;
		Names.FindOrAdd(E.Name.Idx, /* out*/ &bDeclared);
		checkf(!bDeclared, TEXT("Enumerator name declared twice"));
		Constants.FindOrAdd(E.Constant, /* out*/ &bDeclared);
		checkf(!bDeclared, TEXT("Enumerator constant declared twice"));
	}
}

template<typename T>
void CopyItems(T* It, TConstArrayView<T> Items)
{
	for (T Item : Items)
	{
		(*It++)  = Item;
	}
}

static const FStructDeclaration& Declare(TArray<TUniquePtr<FStructDeclaration>>& Out, FDeclId Id, FType Type, uint16 Version, TConstArrayView<FMemberId> MemberOrder, EMemberPresence Occupancy, FOptionalDeclId Super, uint16 NumMembers)
{
	if (static_cast<int32>(Id.Idx) >= Out.Num())
	{
		Out.SetNum(Id.Idx + 1);
	}

	TUniquePtr<FStructDeclaration>& Ptr = Out[Id.Idx];
	if (Ptr)
	{
		check(Id == Ptr->Id);
		check(Type == Ptr->Type);
		check(Super == Ptr->Super);
		check(Version == Ptr->Version);
		check(NumMembers == Ptr->NumMembers);
		check(Occupancy == Ptr->Occupancy);
		check(Algo::Compare(MemberOrder, Ptr->GetMemberOrder()));

		Ptr->RefCount++;
	}
	else
	{
		FStructDeclaration Header{1, Id, Type, Super, Version, NumMembers, Occupancy};
		void* Data = FMemory::Malloc(sizeof(FStructDeclaration) + MemberOrder.Num() * MemberOrder.GetTypeSize());
		Ptr.Reset(new (Data) FStructDeclaration(Header));
		CopyItems(Ptr->MemberOrder, MemberOrder);
	}
	
	return *Ptr;
}

const FStructDeclaration& FDeclarations::DeclareStruct(FDeclId Id, FType Type, uint16 Version, TConstArrayView<FMemberId> MemberOrder, EMemberPresence Occupancy, FOptionalDeclId Super)
{
	checkf(!(Super && Occupancy == EMemberPresence::RequireAll), TEXT("'%s' is a dense substruct, this isn't supported, see BuildSuperStruct()"), *Debug.Print(Id));
	return Declare(/* out */ DeclaredStructs, Id, Type, Version, MemberOrder, Occupancy, Super, IntCastChecked<uint16>(MemberOrder.Num()));
}

const FStructDeclaration& FDeclarations::DeclareNumeralStruct(FDeclId Id, FType Type, TConstArrayView<FMemberId> Numerals, EMemberPresence Occupancy)
{
	return Declare(/* out */ DeclaredStructs, Id, Type, 0, Numerals, Occupancy, NoId, IntCastChecked<uint16>(Numerals.Num()));
}

const FEnumDeclaration& FDeclarations::DeclareEnum(FEnumId Id, FType Type, EEnumMode Mode, TConstArrayView<FEnumerator> Enumerators, EEnumAliases Policy)
{
	if (static_cast<int32>(Id.Idx) >= DeclaredEnums.Num())
	{
		DeclaredEnums.SetNum(Id.Idx + 1);
	}
	
	TUniquePtr<FEnumDeclaration>& Ptr = DeclaredEnums[Id.Idx];
	checkf(!Ptr, TEXT("'%s' is already declared"), *Debug.Print(Id));

	TArray<FEnumerator, TInlineAllocator<64>> Tmp;
	if (Policy == EEnumAliases::Strip)
	{
		Enumerators = StripAliases(/* out */ Tmp, Enumerators, Mode, Debug);
	}

	FEnumDeclaration Header{Type, Mode, IntCastChecked<uint16>(Enumerators.Num())};
	void* Data = FMemory::Malloc(sizeof(FEnumDeclaration) + Enumerators.Num() * Enumerators.GetTypeSize());
	Ptr.Reset(new (Data) FEnumDeclaration(Header));
	CopyItems(Ptr->Enumerators, Enumerators);

	ValidateDeclaration(*Ptr);

	return *Ptr;
}

void FDeclarations::DropStructRef(FDeclId Id)
{
	Check(Id);
	
	TUniquePtr<FStructDeclaration>& Ptr = DeclaredStructs[Id.Idx];
	Ptr->RefCount--;
	if (Ptr->RefCount == 0)
	{
		Ptr.Reset();
	}
}

const FStructDeclaration*
FDeclarations::Find(FDeclId Id) const
{
	return Id.Idx < uint32(DeclaredStructs.Num()) ? DeclaredStructs[Id.Idx].Get() : nullptr;
}

#if DO_CHECK
void FDeclarations::Check(FEnumId Id) const
{
	checkf(Id.Idx < (uint32)DeclaredEnums.Num() && DeclaredEnums[Id.Idx], TEXT("'%s' is undeclared"), *Debug.Print(Id));
}

void FDeclarations::Check(FDeclId Id) const
{
	checkf(Id.Idx < (uint32)DeclaredStructs.Num() && DeclaredStructs[Id.Idx], TEXT("'%s' is undeclared"), *Debug.Print(Id));
}
#endif

} // namespace PlainProps
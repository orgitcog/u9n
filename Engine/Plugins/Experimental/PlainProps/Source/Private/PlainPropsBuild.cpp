// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsBuild.h"
#include "PlainPropsIndex.h"
#include "PlainPropsInternalBuild.h"
#include "PlainPropsInternalFormat.h"
#include "Serialization/VarInt.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Hash/xxhash.h"

namespace PlainProps
{

uint8* FScratchAllocator::AllocateInNewPage(SIZE_T Size, uint32 Alignment)
{
	if (Size >= DataSize || (DataSize - Size) < static_cast<uint32>(PageEnd - Cursor))
	{
		SIZE_T LonePageSize = Align(offsetof(FPage, Data) + Size, Alignment);
		FPage*& PrevPage = LastPage ? LastPage->PrevPage : LastPage;
		FPage Header = { PrevPage };
		PrevPage = new (FMemory::Malloc(LonePageSize)) FPage { Header };
		return Align(PrevPage->Data, Alignment);
	}
	
	FPage Header = { LastPage };
	LastPage = new (FMemory::Malloc(PageSize)) FPage { Header };
	uint8* Out = Align(LastPage->Data, Alignment);
	Cursor = Out + Size;
	PageEnd = LastPage->Data + DataSize;
	check(Cursor <= PageEnd);
	return Out;
}

FScratchAllocator::~FScratchAllocator()
{
	for (FPage* It = LastPage; It; )
	{
		FPage* Prev = It->PrevPage;
		FMemory::Free(It);
		It = Prev;
	}
}

FMemberType& FMemberSchema::EditInnermostType(FScratchAllocator& Scratch)
{
	if (NumInnerRanges > 1)
	{
		FMemberType* Clone = Scratch.AllocateArray<FMemberType>(NumInnerRanges);
		FMemory::Memcpy(Clone, NestedRangeTypes, NumInnerRanges * sizeof(FMemberType));
		NestedRangeTypes = Clone;
		return Clone[NumInnerRanges - 1];
	}

	return NumInnerRanges == 0 ? Type : InnerRangeType;
}

//////////////////////////////////////////////////////////////////////////

FBuiltRange* FBuiltRange::Create(FScratchAllocator& Scratch, uint64 NumItems, SIZE_T ItemSize)
{
	check(NumItems > 0);
	FBuiltRange* Out = new (Scratch.Allocate(sizeof(FBuiltRange) + NumItems * ItemSize, alignof(FBuiltRange))) FBuiltRange;
	Out->Num = NumItems;
	return Out;
}

//////////////////////////////////////////////////////////////////////////

FMemberSchema MakeNestedRangeSchema(FScratchAllocator& Scratch, ERangeSizeType SizeType, FMemberSchema InnerRangeSchema)
{
	check(InnerRangeSchema.NumInnerRanges > 0);
	uint16 NumInnerRanges = IntCastChecked<uint16>(1 + InnerRangeSchema.NumInnerRanges);
	FMemberType* InnerRangeTypes = Scratch.AllocateArray<FMemberType>(NumInnerRanges);
	InnerRangeTypes[0] = InnerRangeSchema.Type;
	FMemory::Memcpy(&InnerRangeTypes[1], InnerRangeSchema.GetInnerRangeTypes().GetData(), InnerRangeSchema.NumInnerRanges * sizeof(FMemberType));

	return { FMemberType(SizeType), InnerRangeSchema.Type, NumInnerRanges, InnerRangeSchema.InnerSchema, InnerRangeTypes };
}

//////////////////////////////////////////////////////////////////////////

FBuiltRange* CloneLeaves(FScratchAllocator& Scratch, uint64 Num, const void* InData, SIZE_T LeafSize)
{
	if (Num == 0)
	{
		return nullptr;
	}

	FBuiltRange* Out = FBuiltRange::Create(Scratch, Num, LeafSize);
	FMemory::Memcpy(Out->Data, InData, Num * LeafSize);
	return Out;
}

//	template<typename FloatType>
//	void NormalizeFloats(FloatType* Values, uint64 Num)
//	{
//		for (uint64 Idx = 0; Idx < Num; ++Idx)
//		{
//			// Reject NaN / INF and ignore negative zero for now
//			checkf(FMath::IsFinite(Values[Idx]), TEXT("Saving NaN or INF isn't supported"));
//		}
//	}
//
//	void NormalizeLeafRange(FUnpackedLeafType Leaf, FBuiltRange& Out)
//	{
//		check(Leaf.Type == ELeafType::Float);
//		if (Leaf.Width == ELeafWidth::B32)
//		{
//			NormalizeFloats(reinterpret_cast<float*>(Out.Data), Out.Num);
//		}
//		else
//		{
//			check(Leaf.Width == ELeafWidth::B64);
//			NormalizeFloats(reinterpret_cast<double*>(Out.Data), Out.Num);
//		}
//	}
//} // namespace Private

//////////////////////////////////////////////////////////////////////////

void FMemberBuilder::AddSuperStruct(FStructId SuperSchema, FBuiltStruct* SuperStruct)
{
	check(SuperStruct);
	check(Members.IsEmpty());
	Members.Emplace(FBuiltMember::MakeSuper(SuperSchema, SuperStruct));
	check(IsSuper(Members[0].Schema.Type));
}

void FMemberBuilder::BuildSuperStruct(FScratchAllocator& Scratch, const FStructDeclaration& Super, const FDebugIds& Debug)
{
	// If we need to support dense substructs, we need access to struct declaration here 
	// or create an empty super structs that we throw away in BuildAndReset.
	if (Members.IsEmpty() || (Members.Num() == 1 && IsSuper(Members[0].Schema.Type)))
	{
		return;
	}
	
	FBuiltStruct* OnlyMember = BuildAndReset(Scratch, Super, Debug);
	Members.Emplace(FBuiltMember::MakeSuper(Super.Id, MoveTemp(OnlyMember)));
	check(IsSuper(Members[0].Schema.Type));
}

FBuiltStruct* FMemberBuilder::BuildAndReset(FScratchAllocator& Scratch, const FStructDeclaration& Declared, const FDebugIds& Debug)
{
#if DO_CHECK
	if (Declared.Occupancy == EMemberPresence::RequireAll)
	{
		checkf(Members.Num() == Declared.NumMembers, TEXT("'%s' requires exactly %d members but %d were added"), *Debug.Print(Declared.Id), Declared.NumMembers, Members.Num());
		checkf(!Declared.Super, TEXT("Bug, dense substructs should fail DeclareStruct() check"));
	}
	
	// Verify members were added in declared order
	if (int32 Num = Members.Num())
	{
		const bool HasSuper = IsSuper(Members[0].Schema.Type);
		int32 OrderIdx = 0;
		TConstArrayView<FMemberId> Order = Declared.GetMemberOrder();
		int32 SkipSuper = Declared.Super && HasSuper;
		for (FBuiltMember* It = Members.GetData() + SkipSuper, *End = Members.GetData() + Num; It != End; ++It)
		{
			FBuiltMember& Member = *It;
			for (; OrderIdx < Order.Num() && Order[OrderIdx] != Member.Name; ++OrderIdx)
			{}	
			checkf(OrderIdx < Order.Num(), TEXT("Member '%s' in '%s' %s"), *Debug.Print(Member.Name), *Debug.Print(Declared.Type),
					Order.Contains(Member.Name) ? TEXT("appeared in non-declared order") : TEXT("is undeclared"));
			++OrderIdx;
		}
	}
#endif

	uint32 Num = static_cast<uint32>(Members.Num());
	SIZE_T NumBytes = sizeof(FBuiltStruct) + Num * sizeof(FBuiltMember);
	FBuiltStruct* Out = reinterpret_cast<FBuiltStruct*>(Scratch.Allocate(NumBytes, alignof(FBuiltStruct)));
	Out->NumMembers = IntCastChecked<uint16>(Num);
	FMemory::Memcpy(Out->Members, Members.GetData(), Members.NumBytes());

	Members.Reset();

	return Out;
}

//////////////////////////////////////////////////////////////////////////

FBuiltStruct* FDenseMemberBuilder::BuildHomo(const FStructDeclaration& Declaration, FMemberType Leaf, TConstArrayView<FBuiltValue> Values) const
{
	check(Declaration.NumMembers == Values.Num());

	FMemberSchema Schema = {Leaf, Leaf};
	const uint32 Num = static_cast<uint32>(Values.Num());
	SIZE_T NumBytes = sizeof(FBuiltStruct) + Num * sizeof(FBuiltMember);
	FBuiltStruct* Out = reinterpret_cast<FBuiltStruct*>(Scratch.Allocate(NumBytes, alignof(FBuiltStruct)));
	Out->NumMembers = static_cast<uint16>(Num);

	const FMemberId* Names = Declaration.MemberOrder; 
	for (uint32 Idx = 0; Idx < Num; ++Idx)
	{
		new (Out->Members + Idx) FBuiltMember(Names[Idx], Schema, Values[Idx]);
	}

	return Out;
}

//////////////////////////////////////////////////////////////////////////

template<typename IntType, typename FloatType>
inline IntType CheckFiniteBitCast(FloatType Value)
{
	// Todo: Decide if we should accept, reject or sanitize NaN / INF / -0
	//checkf(FMath::IsFinite(Value), TEXT("Saving NaN or INF isn't supported"));
	return BitCast<IntType>(Value);
}

uint64 ValueCast(float Value)
{
	return CheckFiniteBitCast<uint32>(Value);
}

uint64 ValueCast(double Value)
{ 
	return CheckFiniteBitCast<uint64>(Value);
}

//////////////////////////////////////////////////////////////////////////

template<typename InnerIdType>
static FMemberSchema MakeMemberSchema(FMemberType Type, InnerIdType InnerSchema)
{
	return {Type, Type, 0, FOptionalInnerId(InnerSchema), nullptr};
}

FBuiltMember::FBuiltMember(FMemberId Name, FUnpackedLeafType Leaf, FOptionalEnumId Enum, uint64 Value)
: FBuiltMember(Name, MakeMemberSchema(Leaf.Pack(), Enum), { .Leaf = Value})
{}

FBuiltMember::FBuiltMember(FMemberId Name, FTypedRange Range)
: FBuiltMember(Name, MoveTemp(Range.Schema), { .Range = Range.Values })
{}

FBuiltMember::FBuiltMember(FMemberId Name, FStructId Id, FBuiltStruct* Value)
: FBuiltMember(Name, MakeMemberSchema(DefaultStructType, FInnerId(Id)), { .Struct = Value })
{}

FBuiltMember FBuiltMember::MakeSuper(FStructId Id, FBuiltStruct* Value)
{
	return FBuiltMember(NoId, MakeMemberSchema(SuperStructType, FInnerId(Id)), { .Struct = Value });
}

//////////////////////////////////////////////////////////////////////////

FTypedRange FStructRangeBuilder::BuildAndReset(FScratchAllocator& Scratch, const FStructDeclaration& Declared, const FDebugIds& Debug)
{
	FTypedRange Out = { MakeStructRangeSchema(SizeType, Declared.Id) };

	if (int64 Num = Structs.Num())
	{
		Out.Values = FBuiltRange::Create(Scratch, Structs.Num(), sizeof(FBuiltStruct*));
		FBuiltStruct** OutIt = reinterpret_cast<FBuiltStruct**>(Out.Values->Data);
		for (FMemberBuilder& Struct : Structs)
		{
			*OutIt++ = Struct.BuildAndReset(Scratch, Declared, Debug);
		}
	}		

	return Out;
}

//////////////////////////////////////////////////////////////////////////

FNestedRangeBuilder::~FNestedRangeBuilder()
{
	checkf(Ranges.IsEmpty(), TEXT("Half-built range, forgot to call BuildAndReset() before destruction?"));
}

FTypedRange FNestedRangeBuilder::BuildAndReset(FScratchAllocator& Scratch, ERangeSizeType SizeType)
{
	FBuiltRange* Out = nullptr;
	if (int64 Num = Ranges.Num())
	{
		Out = FBuiltRange::Create(Scratch, Num, sizeof(FBuiltRange*));
		FMemory::Memcpy(Out->Data, Ranges.GetData(), Ranges.NumBytes());
		Ranges.Reset();
	}

	return { MakeNestedRangeSchema(Scratch, SizeType, Schema), Out };
}

} // namespace PlainProps

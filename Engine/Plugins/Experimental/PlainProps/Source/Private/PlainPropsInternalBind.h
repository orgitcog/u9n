// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsBind.h"

namespace PlainProps 
{

// Iterates over member bindings
class FMemberVisitor
{
public:
	explicit FMemberVisitor(const FSchemaBinding& InSchema);

	bool						HasMore() const			{ return MemberIdx < NumMembers; }
	uint16						GetIndex() const		{ return MemberIdx; }
	
	EMemberKind					PeekKind() const;		// @pre HasMore()
	FMemberBindType				PeekType() const;		// @pre HasMore()
	uint32						PeekOffset() const;		// @pre HasMore()

	FLeafMemberBinding			GrabLeaf();				// @pre PeekKind() == EMemberKind::Leaf
	FRangeMemberBinding			GrabRange();			// @pre PeekKind() == EMemberKind::Range
	FStructMemberBinding		GrabStruct();			// @pre PeekKind() == EMemberKind::Struct
	FBindId						GrabSuper();			// @pre First grab and has declared super
	void						SkipMember();

protected: // for unit tests
	const FSchemaBinding&		Schema;
	const uint16				NumMembers;
	uint16						MemberIdx = 0;
	uint16						InnerRangeIdx = 0;		// Types of [nested] ranges
	uint16						InnerSchemaIdx = 0;		// Types of static structs and enums

	using FMemberBindTypeRange = TConstArrayView<FMemberBindType>;

	uint64						GrabMemberOffset();
	FMemberBindTypeRange		GrabInnerTypes();
	FInnerId					GrabInnerSchema();
	FBindId						GrabStructSchema(FStructType Type);
	FOptionalInnerId			GrabRangeSchema(FMemberType InnermostType);
	FEnumId						GrabEnumSchema()		{ return GrabInnerSchema().AsEnum(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemberBinderBase
{
	FMemberBinderBase(FSchemaBinding& InSchema)
	: Schema(InSchema)
	, MemberIt(Schema.Members)
	, RangeTypeIt(const_cast<FMemberBindType*>(Schema.GetInnerRangeTypes()))
	, OffsetIt(const_cast<uint32*>(Schema.GetOffsets()))
	, RangeBindingIt(const_cast<FRangeBinding*>(Schema.GetRangeBindings()))
	{}

	~FMemberBinderBase()
	{
		check(MemberIt == Schema.GetInnerRangeTypes());
		check(Align(RangeTypeIt, alignof(uint32)) == (const void*)Schema.GetOffsets());
		check(OffsetIt == (const void*)Schema.GetInnerSchemas());
		check(Schema.NumInnerRanges == RangeBindingIt - Schema.GetRangeBindings());
	}

	void AddMember(FMemberBindType Type, uint32 Offset)
	{
		*MemberIt++ = Type;
		*OffsetIt++ = Offset;
	}

	void AddRange(TConstArrayView<FRangeBinding> Ranges, FMemberBindType InnermostType, uint32 Offset)
	{
		AddMember(FMemberBindType(Ranges[0].GetSizeType()), Offset);

		for (FRangeBinding Range : Ranges.RightChop(1))
		{
			*RangeTypeIt++ = FMemberBindType(Range.GetSizeType());
		}
		*RangeTypeIt++ = InnermostType;

		FMemory::Memcpy(RangeBindingIt, Ranges.GetData(), Ranges.Num() * Ranges.GetTypeSize());
		RangeBindingIt += Ranges.Num();
	}
	
	FSchemaBinding& Schema;
	FMemberBindType* MemberIt;
	FMemberBindType* RangeTypeIt;
	uint32* OffsetIt;
	FRangeBinding* RangeBindingIt;
};

} // namespace PlainProps
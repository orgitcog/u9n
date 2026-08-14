// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "Serialization/StructuredArchiveFormatter.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMOverloaded.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

void FStructuredArchiveVisitor::Visit(VCell*& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(UObject*& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(VValue& Value, const TCHAR* ElementName)
{
	FStructuredArchiveRecord Record = Slot(ElementName).EnterRecord();

	if (IsLoading())
	{
		EEncodedType EncodedType = ReadElementType(Record);
		Value = ReadValueBody(Record, EncodedType);
	}
	else
	{
		WriteValueBody(Record, Value);
	}
}

void FStructuredArchiveVisitor::Visit(VInt& Value, const TCHAR* ElementName)
{
	Visit(static_cast<VValue&>(Value), ElementName);
}

void FStructuredArchiveVisitor::Visit(bool& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(uint8& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(int32& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(uint32& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(double& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(FUtf8String& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::VisitBulkData(void* Data, uint64 DataSize, const TCHAR* ElementName)
{
	Slot(ElementName).Serialize(Data, DataSize);
}

const TArray<FName, TFixedAllocator<uint32(FStructuredArchiveVisitor::EEncodedType::Count)>>& FStructuredArchiveVisitor::EncodedTypeNames()
{
	static TArray<FName, TFixedAllocator<uint32(EEncodedType::Count)>> Names{
		FName("None"),
		FName("Cell"),
		FName("Object"),
		FName("Char"),
		FName("Char32"),
		FName("Float"),
		FName("Int"),
	};
	return Names;
}

void FStructuredArchiveVisitor::WriteElementType(FStructuredArchiveRecord Record, EEncodedType EncodedType)
{
	if (Archive.IsTextFormat())
	{
		FName TypeName = EncodedTypeNames()[uint32(EncodedType)];
		Record.EnterField(TEXT("Type")) << TypeName;
	}
	else
	{
		uint8 ScratchType = static_cast<uint8>(EncodedType);
		Record.EnterField(TEXT("Type")) << ScratchType;
	}
}

FStructuredArchiveVisitor::EEncodedType FStructuredArchiveVisitor::ReadElementType(FStructuredArchiveRecord Record)
{
	EEncodedType EncodedType = EEncodedType::None;
	if (Archive.IsTextFormat())
	{
		FName TypeName;
		Record.EnterField(TEXT("Type")) << TypeName;

		EncodedType = EEncodedType(EncodedTypeNames().Find(TypeName));
	}
	else
	{
		uint8 ScratchType;
		Record.EnterField(TEXT("Type")) << ScratchType;

		EncodedType = EEncodedType(ScratchType);
	}
	return EncodedType;
}

void FStructuredArchiveVisitor::WriteValueBody(FStructuredArchiveRecord Record, VValue InValue, bool bAllowBatch)
{
	InValue = InValue.Follow();
	V_DIE_IF_MSG(InValue.IsPlaceholder(), "Unfollowable placeholder: 0x%" PRIxPTR, InValue.GetEncodedBits());

	if (InValue.IsUninitialized())
	{
		WriteElementType(Record, EEncodedType::None);
	}
	else if (VCell* Cell = InValue.ExtractCell())
	{
		WriteElementType(Record, EEncodedType::Cell);
		Record.EnterField(TEXT("Value")) << Cell;
	}
	else if (UObject* Object = InValue.ExtractUObject())
	{
		WriteElementType(Record, EEncodedType::Object);
		Record.EnterField(TEXT("Value")) << Object;
	}
	else if (InValue.IsChar())
	{
		uint8 Char = InValue.AsChar();
		WriteElementType(Record, EEncodedType::Char);
		Record.EnterField(TEXT("Value")) << Char;
	}
	else if (InValue.IsChar32())
	{
		uint32 Char32 = InValue.AsChar32();
		WriteElementType(Record, EEncodedType::Char32);
		Record.EnterField(TEXT("Value")) << Char32;
	}
	else if (InValue.IsFloat())
	{
		double Double = InValue.AsFloat().AsDouble();
		WriteElementType(Record, EEncodedType::Float);
		Record.EnterField(TEXT("Value")) << Double;
	}
	else if (InValue.IsInt32())
	{
		int32 Int = InValue.AsInt32();
		WriteElementType(Record, EEncodedType::Int);
		Record.EnterField(TEXT("Value")) << Int;
	}
	else
	{
		V_DIE("Unexpected Verse value encoding: 0x%" PRIxPTR, InValue.GetEncodedBits());
	}
}

VValue FStructuredArchiveVisitor::ReadValueBody(FStructuredArchiveRecord Record, EEncodedType EncodedType, bool bAllowBatch)
{
	switch (EncodedType)
	{
		case EEncodedType::None:
			return VValue();

		case EEncodedType::Cell:
		{
			VCell* Cell = nullptr;
			Record.EnterField(TEXT("Value")) << Cell;
			return VValue(*Cell);
		}

		case EEncodedType::Object:
		{
			UObject* Object = nullptr;
			Record.EnterField(TEXT("Value")) << Object;
			return VValue(Object);
		}

		case EEncodedType::Char:
		{
			uint8 Char;
			Record.EnterField(TEXT("Value")) << Char;
			return VValue::Char(Char);
		}

		case EEncodedType::Char32:
		{
			uint32 Char32;
			Record.EnterField(TEXT("Value")) << Char32;
			return VValue::Char32(Char32);
		}

		case EEncodedType::Float:
		{
			double Double;
			Record.EnterField(TEXT("Value")) << Double;
			return VValue(VFloat(Double));
		}

		case EEncodedType::Int:
		{
			int32 Int;
			Record.EnterField(TEXT("Value")) << Int;
			return VValue::FromInt32(Int);
		}

		default:
			V_DIE("Unexpected encoded type %u", static_cast<uint8>(EncodedType));
	}
}

FStructuredArchiveSlot FStructuredArchiveVisitor::Slot(const TCHAR* ElementName)
{
	return ::Visit(
		FOverloaded{
			[this](FStructuredArchiveSlot& Slot) {
				return Slot;
			},
			[this, ElementName](FStructuredArchiveRecord& Slot) {
				return Slot.EnterField(ElementName);
			},
			[this](FStructuredArchiveStream& Slot) {
				return Slot.EnterElement();
			}},
		CurrentSlot);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMNames.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMTextPrinting.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VNativeFunction);
TGlobalTrivialEmergentTypePtr<&VNativeFunction::StaticCppClassInfo> VNativeFunction::GlobalTrivialEmergentType;

template <typename TVisitor>
void VNativeFunction::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Name, TEXT("Name"));
	Visitor.Visit(Self, TEXT("Self"));
}

void VNativeFunction::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	if (IsCellFormat(Format))
	{
		Builder.Append(TEXT("Name="));
		Name->AppendToString(Builder, Context, Format, RecursionDepth + 1);
		if (Self)
		{
			Builder.Append(TEXT(", Self="));
			// NOTE: (yiliang.siew) `Self` should always be a class object instance, which should be a `VValueObject` or a `UObject`.
			// If no `Self` is present, it should be a `VFalse`.
			VValue SelfValue = Self.Get();
			SelfValue.AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
	}
	else
	{
		Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
		Builder << Name->AsStringView();
		Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
	}
}

void VNativeFunction::SerializeLayout(FAllocationContext Context, VNativeFunction*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = new (Context.AllocateFastCell(sizeof(VNativeFunction))) VNativeFunction(Context, 0, nullptr, nullptr, VValue());
	}
}

void VNativeFunction::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(NumPositionalParameters, TEXT("NumPositionalParameters"));
	Visitor.Visit(Name, TEXT("Name"));
	Visitor.Visit(Self, TEXT("Self"));
}

void VNativeFunction::SetThunk(Verse::VPackage* Package, FUtf8StringView VerseScopePath, FUtf8StringView DecoratedName, FThunkFn NativeThunkPtr)
{
	// Function names are decorated twice: Once with the scope path they are defined in,
	// and once with the scope path of their base definition (usually these two are the same).
	//
	// Native functions only support a flat list of arguments. To support features like tuple unpacking
	// or named/optional parameters, they may be wrapped in a bytecode entry point. The native function
	// itself lives at a Verse path nested underneath the public entry point:
	//
	// Wrapper: (/Verse/path/to/function/definition:)(/Verse/path/to/overridden/function:)FunctionName(...)
	// Native:  (/Verse/path/to/function/definition/(/Verse/path/to/overridden/function:)FunctionName(...):)Native
	TUtf8StringBuilder<Names::DefaultNameLength> Name = Names::GetDecoratedName<UTF8CHAR>(VerseScopePath, DecoratedName, DecoratorString);
	Verse::VNativeFunction* Function = Package->LookupDefinition<Verse::VNativeFunction>(Name.ToView());
	if (!ensureMsgf(Function, TEXT("Could not find %s"), StringCast<TCHAR>(Name.ToString()).Get()))
	{
		return;
	}

	Function->Thunk = NativeThunkPtr;
}

bool VNativeFunction::HasSelf() const
{
	return !!Self;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

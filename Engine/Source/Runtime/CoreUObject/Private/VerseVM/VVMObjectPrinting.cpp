// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMObjectPrinting.h"
#include "Containers/Set.h"
#include "Containers/Utf8String.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "Misc/TransactionallySafeRWLock.h"
#include "UObject/Object.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMNames.h"
#include "VerseVM/VVMNativeString.h"
#include "VerseVM/VVMObject.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMValuePrinting.h"

namespace
{
struct FObjectPrintHandlerRegistry
{
	FTransactionallySafeRWLock RWLock;
	TArray<Verse::ObjectPrinting::FHandler*> Handlers;

	static FObjectPrintHandlerRegistry& Get()
	{
		static FObjectPrintHandlerRegistry Registry;
		return Registry;
	}
};
} // namespace

namespace Verse
{
namespace ObjectPrinting
{
void RegisterHandler(FHandler* Handler)
{
	FObjectPrintHandlerRegistry& Registry = FObjectPrintHandlerRegistry::Get();
	UE::TWriteScopeLock Lock(Registry.RWLock);
	Registry.Handlers.Add(Handler);
}
void UnregisterHandler(FHandler* Handler)
{
	FObjectPrintHandlerRegistry& Registry = FObjectPrintHandlerRegistry::Get();
	UE::TWriteScopeLock Lock(Registry.RWLock);
	Registry.Handlers.Remove(Handler);
}
} // namespace ObjectPrinting

void AppendToString(FUtf8StringBuilderBase& Builder, UObject* Object, EValueStringFormat Format, uint32 RecursionDepth)
{
	if (IsCellFormat(Format))
	{
		Builder << UTF8TEXT("UObject");
		if (Format == EValueStringFormat::CellsWithAddresses)
		{
			Builder.Appendf(UTF8TEXT("@0x%p"), Object);
		}
		Builder << UTF8TEXT('(');
	}

	bool bHandled = false;
	if (Object)
	{
		// Give the registered print handlers a chance to handle this UObject first.
		FObjectPrintHandlerRegistry& Registry = FObjectPrintHandlerRegistry::Get();
		UE::TReadScopeLock Lock(Registry.RWLock);
		for (ObjectPrinting::FHandler* Handler : Registry.Handlers)
		{
			if (Handler->TryHandle(Object, Builder, Format, RecursionDepth))
			{
				bHandled = true;
				break;
			}
		}
	}

	if (!bHandled)
	{
		// Otherwise, just print its name.
		Builder << GetFullNameSafe(Object);
	}

	if (IsCellFormat(Format))
	{
		Builder << UTF8TEXT(')');
	}
}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void VObject::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	VEmergentType* EmergentType = GetEmergentType();
	const bool bJSON = Format == EValueStringFormat::JSON;

	if (!IsCellFormat(Format))
	{
		if (!bJSON)
		{
			EmergentType->Type->StaticCast<VClass>().AppendQualifiedName(Builder);
		}
		Builder << UTF8TEXT("{");
	}

	// Print the fields of the object.
	FUtf8StringView Separator = UTF8TEXT("");
	for (auto It = EmergentType->Shape->CreateFieldsIterator(); It; ++It)
	{
		Builder << Separator;
		Separator = UTF8TEXT(", ");

		VUniqueString* FieldName = It->Key.Get();
		if (bJSON)
		{
			Builder.Appendf(UTF8TEXT("\"%s\": "), Verse::Names::RemoveQualifier(FieldName->AsStringView()).GetData());
		}
		else
		{
			Builder << Verse::Names::RemoveQualifier(FieldName->AsStringView());
			Builder << UTF8TEXT(" := ");
		}

		FOpResult FieldResult = LoadField(Context, *FieldName);
		if (FieldResult.IsReturn())
		{
			FieldResult.Value.AppendToString(Builder, Context, Format, RecursionDepth + 1);
		}
		else
		{
			Builder << "\"(error)\"";
		}
	}

	if (!IsCellFormat(Format))
	{
		Builder << UTF8TEXT('}');
	}
}
#endif

} // namespace Verse

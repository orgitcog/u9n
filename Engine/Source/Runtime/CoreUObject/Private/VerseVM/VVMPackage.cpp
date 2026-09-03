// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMPackage.h"
#include "Containers/AnsiString.h"
#include "Containers/UnrealString.h"
#include "Containers/Utf8String.h"
#include "Templates/Casts.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEngineEnvironment.h"
#include "VerseVM/VVMGlobalProgram.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMPackageName.h"
#include "VerseVM/VVMProgram.h"
#include "VerseVM/VVMTupleType.h"
#include "VerseVM/VVMVerse.h"
#include "VerseVM/VVMWeakCellMap.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VPackage);
TGlobalTrivialEmergentTypePtr<&VPackage::StaticCppClassInfo> VPackage::GlobalTrivialEmergentType;

VPackage::VPackage(FAllocationContext Context, VUniqueString* InName, VUniqueString* InRootPath, uint32 Capacity)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, Name(Context, InName)
	, RootPath(Context, InRootPath)
	, Definitions(Context, Capacity)
{
}

VPackage::~VPackage()
{
	ResetRedirects();
}

void VPackage::RecordCells(FAllocationContext Context)
{
	Context.RecordCell(this);
	Context.RecordCell(Definitions.NameAndValues.Get());
}

template <typename TVisitor>
void VPackage::VisitReferencesImpl(TVisitor& Visitor)
{
	if (FVersionedDigest* DigestVariant = DigestVariants[(int)EDigestVariant::PublicAndEpicInternal].GetPtrOrNull())
	{
		Visitor.Visit(DigestVariant->Code, TEXT("PublicAndEpicInternalDigest.Code"));
	}
	if (FVersionedDigest* DigestVariant = DigestVariants[(int)EDigestVariant::PublicOnly].GetPtrOrNull())
	{
		Visitor.Visit(DigestVariant->Code, TEXT("PublicOnlyDigest.Code"));
	}
	Visitor.Visit(Name, TEXT("Name"));
	Visitor.Visit(RootPath, TEXT("RootPath"));
	Visitor.Visit(Definitions, TEXT("Definitions"));
	Visitor.Visit(UsedTupleTypes, TEXT("UsedTupleTypes"));
	Visitor.Visit(UsedImports, TEXT("UsedImports"));
	Visitor.Visit(AssociatedUPackage, TEXT("AssociatedUPackage"));
}

void VPackage::SerializeLayout(FAllocationContext Context, VPackage*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = new (Context.Allocate(FHeap::DestructorAndCensusSpace, sizeof(VPackage))) VPackage(Context, nullptr, nullptr, 0);
	}
}

void VPackage::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(Name, TEXT("Name"));
	Visitor.Visit(RootPath, TEXT("RootPath"));

	int32 ScratchNumDefinitions = Definitions.Num();
	Visitor.Visit(ScratchNumDefinitions, TEXT("NumDefinitions"));
	Visitor.VisitArray(TEXT("Definitions"), [this, Context, &Visitor, ScratchNumDefinitions] {
		for (int32 I = 0; I < ScratchNumDefinitions; ++I)
		{
			TPair<FUtf8String, VValue> Pair;
			if (!Visitor.IsLoading())
			{
				Pair.Key = Definitions.GetName(I).AsStringView();
				Pair.Value = Definitions.GetValue(I);
			}

			Visitor.Visit(Pair, TEXT(""));
			if (Visitor.IsLoading())
			{
				Definitions.AddValue(Context, VUniqueString::New(Context, Pair.Key), Pair.Value);
			}
		}
	});

	Visitor.Visit(AssociatedUPackage, TEXT("AssociatedUPackage"));
	if (Visitor.IsLoading())
	{
		GetOrCreateUPackage(Context);
		GlobalProgram->AddPackage(Context, *Name, *this, false);
	}
}

UPackage* VPackage::GetUPackage() const
{
	return Cast<UPackage>(AssociatedUPackage.Get().ExtractUObject());
}

UPackage* VPackage::GetOrCreateUPackage(FAllocationContext Context)
{
	UPackage* Package = GetUPackage();
	if (Package != nullptr)
	{
		return Package;
	}

	IEngineEnvironment* Environment = VerseVM::GetEngineEnvironment();
	ensure(Environment);
	FString UEPackageName = FString(Verse::Names::GetUPackagePath<UTF8CHAR>(GetName().AsStringView()));
	Package = Environment->CreateUPackage(Context, *UEPackageName);
	AssociatedUPackage.Set(Context, VValue(Package));
	return Package;
}

void VPackage::AddRedirect(FCoreRedirect&& Redirect)
{
	Redirects.Add(MoveTemp(Redirect));
}

void VPackage::ApplyRedirects()
{
	FCoreRedirects::AddRedirectList(Redirects, Name->AsString());
}

void VPackage::ResetRedirects()
{
	FCoreRedirects::RemoveRedirectList(Redirects, Name->AsString());
	Redirects.Empty();
}

void VPackage::NotifyUsedTupleType(FAllocationContext Context, VTupleType* TupleType)
{
	if (!UsedTupleTypes)
	{
		UsedTupleTypes.Set(Context, VWeakCellMap::New(Context));
	}
	UsedTupleTypes->Add(Context, TupleType, TupleType);
}

void VPackage::NotifyUsedImport(FAllocationContext Context, VNamedType* TypeWithImport)
{
	if (!UsedImports)
	{
		UsedImports.Set(Context, &VMutableArray::New(Context, 0, 8, EArrayType::VValue));
	}
	UsedImports->AddValue(Context, *TypeWithImport);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

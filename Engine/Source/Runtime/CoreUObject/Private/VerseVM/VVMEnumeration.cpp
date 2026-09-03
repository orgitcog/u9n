// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEnumeration.h"
#include "Async/ExternalMutex.h"
#include "UObject/CoreRedirects.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMEngineEnvironment.h"
#include "VerseVM/VVMEnumerator.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMVerse.h"
#include "VerseVM/VVMVerseEnum.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VEnumeration)
TGlobalTrivialEmergentTypePtr<&VEnumeration::StaticCppClassInfo> VEnumeration::GlobalTrivialEmergentType;

VEnumerator* VEnumeration::GetEnumerator(const VUniqueString& Name) const
{
	for (auto I = Enumerators, Last = I + NumEnumerators; I != Last; ++I)
	{
		if ((*I)->GetName() == &Name)
		{
			return I->Get();
		}
	}
	return nullptr;
}

void VEnumeration::SerializeLayout(FAllocationContext Context, VEnumeration*& This, FStructuredArchiveVisitor& Visitor)
{
	int32 NumEnumerators = 0;
	if (!Visitor.IsLoading())
	{
		NumEnumerators = This->NumEnumerators;
	}

	Visitor.Visit(NumEnumerators, TEXT("NumEnumerators"));
	if (Visitor.IsLoading())
	{
		This = new (Context.AllocateFastCell(sizeof(VEnumeration) + NumEnumerators * sizeof(TWriteBarrier<VEnumerator>))) VEnumeration(Context, NumEnumerators);
	}
}

void VEnumeration::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	VNamedType::SerializeImpl(Context, Visitor);
	Visitor.Visit(Enumerators, NumEnumerators, TEXT("Enumerators"));
}

template <typename TVisitor>
void VEnumeration::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Enumerators, NumEnumerators, TEXT("Enumerators"));
}

VEnumeration::VEnumeration(FAllocationContext Context, VPackage* InPackage, VArray* InRelativePath, VArray* InEnumName, VArray* InAttributeIndices, VArray* InAttributes, UEnum* InImportEnum, bool bInNative, const TArray<VEnumerator*>& InEnumerators)
	: VNamedType(Context, &GlobalTrivialEmergentType.Get(Context), InPackage, InRelativePath, InEnumName, InAttributeIndices, InAttributes, InImportEnum, bInNative)
	, NumEnumerators(InEnumerators.Num())
{
	for (VEnumerator* Enumerator : InEnumerators)
	{
		V_DIE_UNLESS(Enumerator->GetIntValue() >= 0 && Enumerator->GetIntValue() < int32(NumEnumerators));
		new (&Enumerators[Enumerator->GetIntValue()]) TWriteBarrier<VEnumerator>(Context, Enumerator);
	}

	if (InImportEnum != nullptr)
	{
		if (UVerseEnum* UeEnum = Cast<UVerseEnum>(InImportEnum))
		{
			if (EnumHasAnyFlags(UeEnum->VerseEnumFlags, EVerseEnumFlags::UHTNative))
			{
				UeEnum->Enumeration.Set(Context, this);
			}
		}
	}
}

VEnumeration::VEnumeration(FAllocationContext Context, int32 InNumEnumerators)
	: VNamedType(Context, &GlobalTrivialEmergentType.Get(Context), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false)
	, NumEnumerators(InNumEnumerators)
{
	for (int32 Index = 0; Index < NumEnumerators; ++Index)
	{
		new (&Enumerators[Index]) TWriteBarrier<VEnumerator>{};
	}
}

UEnum* VEnumeration::CreateUEType(FAllocationContext Context)
{
	ensure(!HasUEType()); // Caller must ensure that this is not already set

	// Create the new UEnumeration/UScriptStruct object

	IEngineEnvironment* Environment = VerseVM::GetEngineEnvironment();
	check(Environment);

	// Create package for the enumeration
	UPackage* UePackage = Package->GetOrCreateUPackage(Context);

	AddRedirect(ECoreRedirectFlags::Type_Enum);

	EVersePackageType PackageType;
	Names::GetUPackagePath(Package->GetName().AsStringView(), &PackageType);
	UTF8CHAR Separator = PackageType == EVersePackageType::VNI ? UTF8CHAR('_') : UTF8CHAR('-');

	TUtf8StringBuilder<Names::DefaultNameLength> UeName;
	AppendMangledName(UeName, Separator);

	TUtf8StringBuilder<Names::DefaultNameLength> QualifiedName;
	AppendQualifiedName(QualifiedName);

	// Create the UE enum
	UVerseEnum* UeEnum = NewObject<UVerseEnum>(UePackage, FName(UeName), RF_Public /* | RF_Transient*/);
	AssociatedUEType.Set(Context, UeEnum);
	UeEnum->Enumeration.Set(Context, this);
	if (IsNativeBound())
	{
		UeEnum->VerseEnumFlags |= EVerseEnumFlags::NativeBound;
	}
	UeEnum->QualifiedName = FUtf8String(QualifiedName);
	UeEnum->CppType = FString(UeName);

	// Initialize the UVerseEnum
	TArray<TPair<FName, int64>> NameValuePairs;
	// TODO: Initialize the enumerator array - currently not used
	UeEnum->SetEnums(NameValuePairs, UEnum::ECppForm::EnumClass);

	return UeEnum;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMTupleType.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMEngineEnvironment.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMPackageName.h"
#include "VerseVM/VVMVerse.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VTupleType);
TGlobalTrivialEmergentTypePtr<&VTupleType::StaticCppClassInfo> VTupleType::GlobalTrivialEmergentType;

template <typename TVisitor>
void VTupleType::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(UEMangledName, TEXT("UEMangledName"));
	Visitor.Visit(GetElementTypes(), NumElements, TEXT("ElementTypes"));

	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	Visitor.Visit(AssociatedUStructs, TEXT("AssociatedUStructs"));
}

UVerseStruct* VTupleType::CreateUStruct(FAllocationContext Context, VPackage* Scope, bool bIsInstanced)
{
	IEngineEnvironment* Environment = VerseVM::GetEngineEnvironment();
	check(Environment);

	// Create package for the struct
	UPackage* Package = Scope->GetOrCreateUPackage(Context);

	// Create the UE struct
	UVerseStruct* UeStruct = NewObject<UVerseStruct>(Package, FName(GetUEMangledName().AsStringView()), RF_Public /* | RF_Transient*/);
	{
		UE::FExternalMutex ExternalMutex(Mutex);
		UE::TUniqueLock Lock(ExternalMutex);
		AssociatedUStructs.Add({Context, Scope}, {Context, UeStruct});
	}

	UeStruct->VerseClassFlags |= VCLASS_Tuple;

#if WITH_EDITOR
	UeStruct->SetMetaData(TEXT("IsBlueprintBase"), TEXT("false"));
#endif

	// Generate properties
	FField::FLinkedListBuilder PropertyListBuilder(&UeStruct->ChildProperties);
	for (uint32 Index = 0; Index < NumElements; ++Index)
	{
		VValue TypeValue = GetElementTypes()[Index].Follow();
		V_DIE_IF(TypeValue.IsPlaceholder());

		VType& ElementType = TypeValue.StaticCast<VType>();
		FProperty* FieldProperty = Environment->CreateProperty(Context, Scope, UeStruct, FUtf8String::Printf("Elem%d", Index), FUtf8String::Printf("Elem%d", Index), &ElementType, true, bIsInstanced);
		PropertyListBuilder.AppendNoTerminate(*FieldProperty);
	}

	// Finalize struct
	UeStruct->Bind();
	UeStruct->StaticLink(/*bRelinkExistingProperties =*/true);

	return UeStruct;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

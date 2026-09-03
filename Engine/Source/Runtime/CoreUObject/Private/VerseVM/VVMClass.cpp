// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMClass.h"
#include "Async/ExternalMutex.h"
#include "Logging/LogMacros.h"
#include "UObject/Class.h"
#include "UObject/CoreRedirects.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/Interface.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/VerseValueProperty.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMEnterVMInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMNativeConstructorWrapperInline.h"
#include "VerseVM/Inline/VVMNativeStructInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMScopeInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUniqueStringInline.h"
#include "VerseVM/Inline/VVMValueObjectInline.h"
#include "VerseVM/VVMAttribute.h"
#include "VerseVM/VVMEngineEnvironment.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMInstantiationContext.h"
#include "VerseVM/VVMNames.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMNativeStruct.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMTypeInitOrValidate.h"
#include "VerseVM/VVMValuePrinting.h"
#include "VerseVM/VVMVar.h"
#include "VerseVM/VVMVerse.h"
#include "VerseVM/VVMVerseFunction.h"

DEFINE_LOG_CATEGORY_STATIC(LogVerseValidation, Log, All);

namespace Verse
{

namespace Private
{

// Wrapper class to implement the logging for the FInitOrValidate types
template <typename UEType>
struct FVerseVMInitOrValidate : FInitOrValidatorSelector<UEType>::Validator
{
	using Super = FInitOrValidatorSelector<UEType>::Validator;
	FVerseVMInitOrValidate(UEType* Type)
		: Super(Type, Type->IsUHTNative())
	{
	}

	~FVerseVMInitOrValidate()
	{
		if (bGotError)
		{
			UE_LOG(LogVerseValidation, Fatal, TEXT("Type \"%s\" validation terminated due to mismatches with UHT type."), *Super::GetField()->GetName());
		}
	}

	virtual void LogError(const FString& text) const override
	{
		UE_LOG(LogVerseValidation, Error, TEXT("Type \"%s\" %s"), *Super::GetField()->GetName(), *text);
		bGotError = true;
	}

	mutable bool bGotError = false;
};
} // namespace Private

template <typename TVisitor>
void Visit(TVisitor& Visitor, VArchetype::VEntry& Entry)
{
	Visitor.Visit(Entry.Name, TEXT("Name"));
	Visitor.Visit(Entry.Type, TEXT("Type"));
	Visitor.Visit(Entry.Value, TEXT("Value"));
	Visitor.Visit(reinterpret_cast<std::underlying_type_t<EArchetypeEntryFlags>&>(Entry.Flags), TEXT("Flags"));
}

bool VArchetype::VEntry::IsMethod() const
{
	VValue EntryValue = Value.Get();
	if (VFunction* EntryFunction = EntryValue.DynamicCast<VFunction>())
	{
		return !EntryFunction->HasSelf();
	}
	else if (VNativeFunction* EntryNativeFunction = EntryValue.DynamicCast<VNativeFunction>())
	{
		return !EntryNativeFunction->HasSelf();
	}
	return false;
}

DEFINE_DERIVED_VCPPCLASSINFO(VArchetype);
TGlobalTrivialEmergentTypePtr<&VArchetype::StaticCppClassInfo> VArchetype::GlobalTrivialEmergentType;

template <typename TVisitor>
void VArchetype::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Class, TEXT("Class"));
	Visitor.Visit(NextArchetype, TEXT("NextArchetype"));
	Visitor.Visit(Entries, NumEntries, TEXT("Entries"));
}

void VArchetype::SerializeLayout(FAllocationContext Context, VArchetype*& This, FStructuredArchiveVisitor& Visitor)
{
	uint32 NumEntries = 0;
	if (!Visitor.IsLoading())
	{
		NumEntries = This->NumEntries;
	}

	Visitor.Visit(NumEntries, TEXT("NumEntries"));
	if (Visitor.IsLoading())
	{
		This = &VArchetype::NewUninitialized(Context, NumEntries);
	}
}

void VArchetype::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(Class, TEXT("Class"));
	Visitor.Visit(NextArchetype, TEXT("NextArchetype"));
	Visitor.Visit(Entries, NumEntries, TEXT("Entries"));
}

void VArchetype::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	if (IsCellFormat(Format))
	{
		Builder.Append(UTF8TEXT("\n"));
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			const VEntry& Entry = Entries[Index];
			Builder << UTF8TEXT("\t");
			Builder.Appendf(UTF8TEXT("UniqueString(\"%s\")"), Verse::Names::RemoveQualifier(Entry.Name->AsStringView()).GetData());
			Builder << UTF8TEXT(" : Entry(Value: ");
			Entry.Value.Get().AppendToString(Builder, Context, Format, RecursionDepth + 1);
			Builder << UTF8TEXT(", IsConstant: ");
			Builder << (Entry.IsConstant() ? UTF8TEXT("true") : UTF8TEXT("false"));
			Builder << UTF8TEXT(", HasDefaultValueExpression: ");
			Builder << (Entry.HasDefaultValueExpression() ? UTF8TEXT("true") : UTF8TEXT("false"));
			Builder << UTF8TEXT(", IsNative: ");
			Builder << (Entry.IsNative() ? UTF8TEXT("true") : UTF8TEXT("false"));
			Builder << UTF8TEXT("))\n");
		}
	}
}

VFunction* VArchetype::LoadFunction(FAllocationContext Context, VUniqueString& FieldName, VValue SelfObject)
{
	// TODO: (yiliang.siew) This should probably be improved with inline caching or a hashtable instead for constructors
	// with lots of entries.
	for (uint32 Index = 0; Index < NumEntries; ++Index)
	{
		VEntry& CurrentEntry = Entries[Index];
		if (*CurrentEntry.Name.Get() != FieldName)
		{
			continue;
		}
		if (VFunction* Procedure = Entries[Index].Value.Get().DynamicCast<VFunction>(); Procedure && !Procedure->HasSelf())
		{
			// At this point `(super:)`/captures for the scope should already be filled in.
			VFunction& NewFunction = Procedure->Bind(Context, SelfObject);
			return &NewFunction;
		}
	}
	return nullptr;
}

DEFINE_DERIVED_VCPPCLASSINFO(VClass)
TGlobalTrivialEmergentTypePtr<&VClass::StaticCppClassInfo> VClass::GlobalTrivialEmergentType;

template <typename TVisitor>
void VClass::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(ImportStruct, TEXT("ImportStruct"));
	Visitor.Visit(Archetype, TEXT("Archetype"));
	Visitor.Visit(Constructor, TEXT("Constructor"));
	Visitor.Visit(Inherited, NumInherited, TEXT("Inherited"));

	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	Visitor.Visit(EmergentTypesCache, TEXT("EmergentTypesCache"));
}

void VClass::SerializeLayout(FAllocationContext Context, VClass*& This, FStructuredArchiveVisitor& Visitor)
{
	int32 NumInherited = 0;
	if (!Visitor.IsLoading())
	{
		NumInherited = This->NumInherited;
	}

	Visitor.Visit(NumInherited, TEXT("NumInherited"));
	if (Visitor.IsLoading())
	{
		size_t NumBytes = offsetof(VClass, Inherited) + NumInherited * sizeof(Inherited[0]);
		This = new (Context.Allocate(FHeap::DestructorSpace, NumBytes)) VClass(Context, NumInherited);
	}
}

void VClass::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	VNamedType::SerializeImpl(Context, Visitor);
	Visitor.Visit(reinterpret_cast<std::underlying_type_t<EKind>&>(Kind), TEXT("Kind"));
	Visitor.Visit(reinterpret_cast<std::underlying_type_t<EFlags>&>(Flags), TEXT("Flags"));
	Visitor.Visit(ImportStruct, TEXT("ImportStruct"));
	Visitor.Visit(Archetype, TEXT("Archetype"));
	Visitor.Visit(Constructor, TEXT("Constructor"));
	Visitor.Visit(Inherited, NumInherited, TEXT("Inherited"));
}

VClass::VClass(
	FAllocationContext Context,
	VPackage* InPackageScope,
	VArray* InPath,
	VArray* InClassName,
	VArray* InAttributeIndices,
	VArray* InAttributes,
	UStruct* InImportStruct,
	bool bInNativeBound,
	EKind InKind,
	EFlags InFlags,
	const TArray<VClass*>& InInherited,
	VArchetype& InArchetype,
	VProcedure& InConstructor)
	: VNamedType(
		Context,
		&GlobalTrivialEmergentType.Get(Context),
		InPackageScope,
		InPath,
		InClassName,
		InAttributeIndices,
		InAttributes,
		nullptr,
		bInNativeBound)
	, Kind(InKind)
	, Flags(InFlags)
	, NumInherited(InInherited.Num())
{
	checkSlow(!IsNativeBound() || IsNativeRepresentation());

	if (InImportStruct != nullptr)
	{
		ImportStruct.Set(Context, InImportStruct);
		Package->NotifyUsedImport(Context, this);
	}

	for (int32 Index = 0; Index < NumInherited; ++Index)
	{
		new (&Inherited[Index]) TWriteBarrier<VClass>(Context, InInherited[Index]);
	}

	// `InArchetype` is an immutable template, typically part of a module's top-level bytecode.
	// Clone it to fill out the parts that need to refer to this VClass or its superclass, which
	// generally do not exist yet during compilation when the template is produced.
	Archetype.Set(Context, VArchetype::NewUninitialized(Context, InArchetype.NumEntries));

	Archetype->Class.Set(Context, *this);

	VClass* SuperClass = nullptr;
	if (NumInherited > 0 && Inherited[0]->GetKind() == VClass::EKind::Class)
	{
		SuperClass = Inherited[0].Get();
		Archetype->NextArchetype.Set(Context, *SuperClass->Archetype.Get());
	}

	// The class body, and methods defined within it, are bare `VProcedure`s with no `VScope` yet.
	// Give them access to the lexical scope of the class definition (currently just `(super:)`).
	// When they are eventually invoked, they will be further augmented with a `Self` value.
	VScope& ClassScope = VScope::New(Context, SuperClass);
	Constructor.Set(Context, VFunction::NewUnbound(Context, InConstructor, ClassScope));
	for (uint32 Index = 0; Index < InArchetype.NumEntries; ++Index)
	{
		VArchetype::VEntry& CurrentEntry = *new (&Archetype->Entries[Index]) VArchetype::VEntry(InArchetype.Entries[Index]);
		if (VProcedure* CurrentProcedure = CurrentEntry.Value.Get().DynamicCast<VProcedure>())
		{
			CurrentEntry.Value.Set(Context, VFunction::NewUnbound(Context, *CurrentProcedure, ClassScope));
		}
	}
}

VClass::VClass(FAllocationContext Context, int32 InNumInherited)
	: VNamedType(Context, &GlobalTrivialEmergentType.Get(Context), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false)
	, NumInherited(InNumInherited)
{
	for (int32 Index = 0; Index < NumInherited; ++Index)
	{
		new (&Inherited[Index]) TWriteBarrier<VClass>{};
	}
}

VValueObject& VClass::NewVObject(FAllocationContext Context, VArchetype& InArchetype)
{
	V_DIE_IF(IsNativeRepresentation());

	VEmergentType& NewEmergentType = GetOrCreateEmergentTypeForVObject(Context, &VValueObject::StaticCppClassInfo, InArchetype);
	VValueObject& NewObject = VValueObject::NewUninitialized(Context, NewEmergentType);

	if (Kind == EKind::Struct)
	{
		NewObject.SetIsStruct();
	}

	// TODO(SOL-7928): Remove this check. It is a hack for BPVM compatibility.
	if (EnumHasAllFlags(FInstantiationScope::Context.Flags, RF_ArchetypeObject))
	{
		NewObject.Misc2 |= VCell::ArchetypeTag;
	}

	return NewObject;
}

VNativeConstructorWrapper& VClass::NewNativeStruct(FAllocationContext Context)
{
	V_DIE_UNLESS(IsNativeStruct());

	VEmergentType& EmergentType = GetOrCreateEmergentTypeForNativeStruct(Context);
	VNativeStruct& NewObject = VNativeStruct::NewUninitialized(Context, EmergentType);

	return VNativeConstructorWrapper::New(Context, NewObject);
}

VNativeConstructorWrapper& VClass::NewUObject(FAllocationContext Context)
{
	V_DIE_IF(IsStruct());

	UObject* Outer = FInstantiationScope::Context.Outer;
	// TODO: Should this use FContentScope's GetInstantiationOuter before falling back to the transient package?
	if (!Outer)
	{
		Outer = GetTransientPackage();
	}

	UClass* ObjectUClass = GetOrCreateUEType<UClass>(Context);
	FStaticConstructObjectParameters Parameters(ObjectUClass);
	// Note: Object will get a default name based on class name
	// TODO: Set instancing graph properly
	Parameters.Outer = Outer;
	// We are using RF_BeingRegenerated to signal to UVerseClass::PostInitInstance that we don't want the root constructor to run on this object
	// It is safe to repurpose this flag since it is otherwise just used for UClasses, and this object is guaranteed to not be a UClass
	checkSlow(!ObjectUClass->IsChildOf(UClass::StaticClass()));
	Parameters.SetFlags = FInstantiationScope::Context.Flags | RF_BeingRegenerated;
	UObject* NewObject = StaticConstructObject_Internal(Parameters);
	V_DIE_UNLESS(NewObject);
	NewObject->ClearFlags(RF_BeingRegenerated); // No longer needed once the object has been created

	return VNativeConstructorWrapper::New(Context, *NewObject);
}

void VClass::WalkArchetypeFields(
	FAllocationContext Context,
	VArchetype& InArchetype,
	int32 BaseIndex,
	bool bConstructBases,
	TFunction<void(VArchetype::VEntry&, int32)> FieldCallbackProc)
{
	/*
	  This function walks the entries of the given archetype recursively and helps to figure out the final shape for the
	  object to be allocated. The reason we track the object-only fields is so that we can calculate a hash for the emergent
	  type that distinguishes between different archetype instantiations that may override a field's default value (thus
	  demoting the data to be stored in the object instead of the shape).

	  It accounts for both actual class archetypes, and ones for constructor functions - those that represent the
	  initializers within a body of said function.

	  We can identify if we are currently walking a archetype that represents a class body, because of the backpointer
	  each archetype has to its class, along with each class also having a pointer to its archetype - we check that
	  the archetype points to a class that again points to said archetype for the former case.

	  In the case of a constructor function with no nested constructor function calls, the delegating archetype is set
	  to the class archetype that the constructor function is creating an instance of. This is because it needs to
	  finish initialization of the fields in the class since the class body itself may initialize fields that the
	  constructor function does not.

	  In the case of a constructor function that has a nested constructor call, but the nested constructor call returns
	  the same class type, the delegating archetype of the current archetype is set to that nested archetype as
	  per-normal, since _that_ archetype will be of the case mentioned directly prior.

	  In the case of a constructor function that has a nested constructor call that returns a superclass type, the
	  delegating archetype will be set to the delegating constructor function's archetype - however, as we iterate
	  over it here, we first walk the _current_ class body's archetype first. This follows the object archetype
	  construction semantics.

	  The `bShouldConstructBases` flag essentially acts as a check to help implement that fact that we're going to call
	  the current subclass's body constructor first to finish initializing the fields that the subclass would normally,
	  but we don't want it to also call the base class body constructor as part of its procedure since that would be
	  handled by the delegating constructor of the base class type. (Each class body procedure takes an argument for
	  this as well, which is done in the code generator.)

	  For a subclass's archetype - the delegating archetype should be set to the base class's archetype.
	  For a base class's archetype - there is no next constructor to be called, and thus we can finish walking at this point.
	 */
	for (int32 Index = 0; Index < InArchetype.NumEntries; ++Index)
	{
		FieldCallbackProc(InArchetype.Entries[Index], BaseIndex + Index);
	}
	BaseIndex += InArchetype.NumEntries;

	VClass& InArchetypeClass = InArchetype.Class.Get(Context).StaticCast<VClass>();
	VArchetype* NextArchetype = InArchetype.NextArchetype.Get(Context).DynamicCast<VArchetype>();
	if (NextArchetype)
	{
		VClass& NextArchetypeClass = NextArchetype->Class.Get(Context).StaticCast<VClass>();
		// If NextArchetype corresponds to a constructor function for a base class, finish constructing the current class first.
		bool bNextArchetypeForConstructor = NextArchetype != &NextArchetypeClass.GetArchetype();
		bool bNextArchetypeForBase = &InArchetypeClass != &NextArchetypeClass;
		if (bNextArchetypeForConstructor && bNextArchetypeForBase)
		{
			WalkArchetypeFields(Context, InArchetypeClass.GetArchetype(), BaseIndex, /*bConstructBases*/ false, FieldCallbackProc);
			BaseIndex += NextArchetype->NumEntries;
		}

		if (bConstructBases)
		{
			WalkArchetypeFields(Context, *NextArchetype, BaseIndex, /*bConstructBases*/ true, FieldCallbackProc);
		}
	}

	bool bIsClassArchetype = &InArchetype == &InArchetypeClass.GetArchetype();
	if (bConstructBases && bIsClassArchetype && InArchetypeClass.NumInherited > 0)
	{
		int32 Index = InArchetypeClass.Inherited[0]->GetKind() == VClass::EKind::Class;
		for (; Index < InArchetypeClass.NumInherited; ++Index)
		{
			WalkArchetypeFields(Context, InArchetypeClass.Inherited[Index]->GetArchetype(), BaseIndex, /*bConstructBases*/ true, FieldCallbackProc);
		}
	}
}

VEmergentType& VClass::GetOrCreateEmergentTypeForVObject(FAllocationContext Context, VCppClassInfo* CppClassInfo, VArchetype& InArchetype)
{
	V_DIE_IF_MSG(IsNativeRepresentation(), "This code path for archetype instantiation should only be executed for non-native Verse objects!");

	// TODO: This in the future shouldn't even require a hash table lookup when we introduce inline caching for this.
	TSet<VUniqueString*> ObjectFields;
	ObjectFields.Reserve(InArchetype.NumEntries);
	VShape::FieldsMap ShapeFields;
	ShapeFields.Reserve(InArchetype.NumEntries);
	WalkArchetypeFields(Context, InArchetype, 0, /*bConstructBases*/ true,
		[&ShapeFields, &ObjectFields, &Context](VArchetype::VEntry& Entry, int32) {
			// e.g. for `c := class { var X:int = 0 }`, `X`'s data is stored in the object, not the shape.
			if (!Entry.IsConstant())
			{
				const VShape::VEntry& ExistingEntry = ShapeFields.FindOrAdd({Context, *Entry.Name}, VShape::VEntry::Offset());
				// If it already was added as a constant field, don't add it to the fields that would live on the object.
				if (ExistingEntry.Type != EFieldType::Constant)
				{
					ObjectFields.FindOrAdd(Entry.Name.Get());
				}
			}
			else
			{
				ShapeFields.FindOrAdd({Context, *Entry.Name}, VShape::VEntry::Constant(Context, Entry.Value.Get()));
			}
		});

	// At this point, we have all the fields and their types, so we can now create an emergent type representing it.
	VUniqueStringSet& ObjectFieldNames = VUniqueStringSet::New(Context, ObjectFields);
	const uint32 ArcheTypeHash = GetSetVUniqueStringTypeHash(ObjectFields);
	// Note: We can look up the emergent type without locking our Mutex since this thread is the only one mutating the hash table
	if (TWriteBarrier<VEmergentType>* ExistingEmergentType = EmergentTypesCache.FindByHash(ArcheTypeHash, ObjectFieldNames))
	{
		return *ExistingEmergentType->Get();
	}

	// Compute the shape by interning the set of fields.
	VShape* NewShape = VShape::New(Context, MoveTemp(ShapeFields));
	VEmergentType* NewEmergentType = VEmergentType::New(Context, NewShape, this, CppClassInfo);
	V_DIE_IF(NewEmergentType == nullptr);

	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	// This new type will then be kept alive in the cache to re-vend if ever the exact same set of fields are used for
	// archetype instantiation of a different object.
	EmergentTypesCache.AddByHash(ArcheTypeHash, {Context, ObjectFieldNames}, {Context, *NewEmergentType});

	return *NewEmergentType;
}

VEmergentType& VClass::GetOrCreateEmergentTypeForNativeStruct(FAllocationContext Context)
{
	V_DIE_UNLESS(IsNativeStruct());

	// Note: We can look up the emergent type without locking our Mutex since this thread is the only one mutating the hash table
	const uint32 SingleHash = 0; // For native structs, we only ever store one emergent type, regardless of archetype
	if (TWriteBarrier<VEmergentType>* ExistingEmergentType = EmergentTypesCache.FindByHash(SingleHash, TWriteBarrier<VUniqueStringSet>{}))
	{
		return *ExistingEmergentType->Get();
	}

	VShape* Shape = nullptr;
	UScriptStruct* Struct = GetOrCreateUEType<UScriptStruct>(Context);
	if (UVerseStruct* VerseStruct = Cast<UVerseStruct>(Struct))
	{
		Shape = VerseStruct->Shape.Get();
	}

	UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
	V_DIE_UNLESS(CppStructOps->GetAlignment() <= VObject::DataAlignment);

	VEmergentType* NewEmergentType = VEmergentType::New(Context, Shape, this, &VNativeStruct::StaticCppClassInfo);

	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	// Keep alive in cache for future requests
	EmergentTypesCache.AddByHash(SingleHash, {Context, nullptr}, {Context, NewEmergentType});

	return *NewEmergentType;
}

void VClass::CreateShapeForUStruct(
	Verse::FAllocationContext Context,
	TFunction<FProperty*(VArchetype::VEntry&, int32)>&& CreateProperty,
	TFunction<UFunction*(VArchetype::VEntry&)>&& CreateFunction)
{
	UStruct* UeClassOrStruct = GetUETypeChecked<UStruct>(); // will fail if not set, must be set

	VShape* SuperShape = nullptr;
	if (UClass* UeClass = Cast<UClass>(UeClassOrStruct))
	{
		if (UVerseClass* SuperVerseUClass = Cast<UVerseClass>(UeClass->GetSuperStruct()))
		{
			SuperShape = SuperVerseUClass->Shape.Get();
		}
	}

	VShape::FieldsMap ShapeFields;
	ShapeFields.Reserve(Archetype->NumEntries);
	WalkArchetypeFields(Context, *Archetype.Get(), 0, /*bConstructBases*/ true,
		[this, Context, &CreateProperty, &CreateFunction, SuperShape, &ShapeFields](VArchetype::VEntry& Entry, int32 Index) {
			// UObjects store data fields as properties in the object, and methods in the function map.
			if (!Entry.IsMethod())
			{
				const VShape::VEntry* SuperEntry = SuperShape ? SuperShape->GetField(*Entry.Name) : nullptr;
				if (SuperEntry)
				{
					// If the super shape has it, recycle the same property
					V_DIE_UNLESS(SuperEntry->IsProperty());
					ShapeFields.FindOrAdd({Context, *Entry.Name}, *SuperEntry);
				}
				else if (!ShapeFields.Contains({Context, *Entry.Name}))
				{
					VValue FieldType = Entry.Type.Follow();
					V_DIE_UNLESS(!FieldType.IsUninitialized() && !FieldType.IsPlaceholder());
					FProperty* FieldProperty = CreateProperty(Entry, Index);
					V_DIE_UNLESS(FieldProperty != nullptr);
					if (FieldType.IsCellOfType<VPointerType>())
					{
						ShapeFields.FindOrAdd({Context, *Entry.Name}, VShape::VEntry::PropertyVar(FieldProperty));
					}
					else
					{
						ShapeFields.FindOrAdd({Context, *Entry.Name}, VShape::VEntry::Property(FieldProperty));
					}
				}
			}
			else
			{
				if (!ShapeFields.Contains({Context, *Entry.Name}))
				{
					CreateFunction(Entry);
				}
				ShapeFields.FindOrAdd({Context, *Entry.Name}, VShape::VEntry::Constant(Context, Entry.Value.Get()));
			}
		});

	VShape* Shape = VShape::New(Context, MoveTemp(ShapeFields));
	if (UVerseClass* UeClass = Cast<UVerseClass>(UeClassOrStruct))
	{
		UeClass->Shape.Set(Context, Shape);
		if (GUObjectArray.IsDisregardForGC(UeClass))
		{
			UeClass->Shape->AddRef(Context);
		}
	}
	else if (UVerseStruct* UeStruct = Cast<UVerseStruct>(UeClassOrStruct))
	{
		UeStruct->Shape.Set(Context, Shape);
		if (GUObjectArray.IsDisregardForGC(UeStruct))
		{
			UeStruct->Shape->AddRef(Context);
		}
	}
}

void VClass::CreateShapeForExistingUStruct(FAllocationContext Context)
{
	UStruct* Struct = GetOrCreateUEType<UStruct>(Context);

	auto CreateProperty = [this, Struct](VArchetype::VEntry& Entry, int32) {
		FString Name = Entry.Name.Get()->AsString();
		FStringView PropName = Verse::Names::RemoveQualifier(Name);
		FStringView CrcPropName = Entry.UseCRCName() ? Name : PropName;
		FName UePropName = Verse::Names::VersePropToUEFName(PropName, CrcPropName);
		FProperty* FieldProperty = Struct->FindPropertyByName(UePropName);
		check(FieldProperty); // should have been verified at script compile time
		return FieldProperty;
	};
	auto CreateFunction = [](VArchetype::VEntry&) {
		return nullptr;
	};
	CreateShapeForUStruct(Context, CreateProperty, CreateFunction);
}

namespace
{
DEFINE_FUNCTION(InvokeCalleeThunk)
{
	Verse::FOpResult OpResult{Verse::FOpResult::Error};

	AutoRTFM::Open([&] {
		// TODO: Marshal arguments
		Verse::VFunction::Args ArgValues;
		P_FINISH;

		FRunningContext VMContext{FRunningContextPromise{}};
		UVerseFunction* ThisFunction = CastChecked<UVerseFunction>(Stack.CurrentNativeFunction);
		Verse::VValue Self(Context);

		// Invoke the callee
		VMContext.EnterVM([VMContext, ThisFunction, Self, &ArgValues, &OpResult] {
			Verse::VValue Callee = ThisFunction->Callee.Get();
			if (Verse::VFunction* Function = Callee.DynamicCast<Verse::VFunction>())
			{
				OpResult = Function->InvokeWithSelf(VMContext, Self, MoveTemp(ArgValues));
			}
			else if (Verse::VNativeFunction* NativeFunction = Callee.DynamicCast<Verse::VNativeFunction>())
			{
				OpResult = NativeFunction->Thunk(VMContext, Self, ArgValues);
			}
		});
	});

	// TODO: Marshal return value and handle other outcomes
	ensure(OpResult.IsReturn());
}
} // namespace

UFunction* VClass::MaybeCreateUFunctionForCallee(Verse::FAllocationContext Context, VValue Callee)
{
	VUniqueString* Name = nullptr;
	uint32 NumPositionalParameters = 0;
	uint32 NumNamedParameters = 0;
	if (VFunction* Function = Callee.DynamicCast<VFunction>())
	{
		Name = Function->Procedure->Name.Get();
		NumPositionalParameters = Function->Procedure->NumPositionalParameters;
		NumNamedParameters = Function->Procedure->NumNamedParameters;
	}
	else if (VNativeFunction* NativeFunction = Callee.DynamicCast<VNativeFunction>())
	{
		Name = NativeFunction->Name.Get();
		NumPositionalParameters = NativeFunction->NumPositionalParameters;
		NumNamedParameters = 0;
	}

	// For now, we support only functions with no arguments
	const bool bShouldGenerateUFunction = (NumPositionalParameters + NumNamedParameters == 0);
	if (!bShouldGenerateUFunction)
	{
		return nullptr;
	}

	// Create a new UFunction and add it the class's field list and function map
	UClass* UeClass = GetUETypeChecked<UClass>();
	FName FunctionName = Verse::Names::VerseFuncToUEFName(FString(Name->AsStringView()));
	ensure(!StaticFindObjectFast(UVerseFunction::StaticClass(), UeClass, FunctionName));
	UVerseFunction* CalleeFunction = NewObject<UVerseFunction>(UeClass, FunctionName);
	CalleeFunction->FunctionFlags |= FUNC_Public | FUNC_Native;
	CalleeFunction->SetNativeFunc(&InvokeCalleeThunk);
	CalleeFunction->InitializeDerivedMembers();
	CalleeFunction->Callee.Set(Context, Callee);
	return CalleeFunction;
}

void VClass::ValidateImportAs(FAllocationContext Context)
{
	UStruct* UeClassOrStruct = GetUETypeChecked<UStruct>(); // will fail if not set, must be set

	// Loop over entries and validate they match our imports properties
	// TODO: Log validation errors and throw a runtime_error after we've reported them all
	for (uint32 Index = 0; Index < Archetype->NumEntries; ++Index)
	{
		VArchetype::VEntry& Entry = Archetype->Entries[Index];
		if (!Entry.IsMethod()) // only validate data members
		{
			FString Name = Entry.Name.Get()->AsString();
			FStringView PropName = Verse::Names::RemoveQualifier(Name);
			FStringView CrcPropName = Entry.UseCRCName() ? Name : PropName;
			FName UEVerseName = Verse::Names::VersePropToUEFName(PropName, CrcPropName);
			if (FProperty* ExistingProperty = UeClassOrStruct->FindPropertyByName(UEVerseName))
			{
				VType* PropertyType = Entry.Type.Get().Follow().DynamicCast<VType>();
				if (!VerseVM::GetEngineEnvironment()->ValidateProperty(Context, FName(PropName.GetData()), PropertyType, ExistingProperty, Entry.IsInstanced()))
				{
					V_DIE("The imported type: `%s` does not have the required property type for the property `%s`", *GetBaseName().AsString(), PropName.GetData());
				}
			}
			else
			{
				V_DIE("The imported type: `%s` does not contain the required property `%s`", *GetBaseName().AsString(), PropName.GetData());
			}
		}
	}
}

void VClass::Prepare(FAllocationContext Context, FInitOrValidateUVerseStruct& InitOrValidate, UVerseStruct* Type)
{
	// -----------------------------------------------------------------------------------------------------
	// Keep the following code in sync with FSolClassGenerator::Prepare for structs, classes, and interfaces

	TUtf8StringBuilder<Names::DefaultNameLength> QualifiedName;
	AppendQualifiedName(QualifiedName);

	InitOrValidate.SetValue(Type->Guid, FGuid(FCrc::Strihash_DEPRECATED(*Type->GetName()), GetTypeHash(Type->GetPackage()->GetName()), 0, 0), TEXT("Guid"));
	//??? InitOrValidate.ForceValue(VerseStruct->ConstructorEffects, MakeEffectSet(SemanticType->_ConstructorEffects));
	InitOrValidate.SetValue(Type->QualifiedName, QualifiedName.ToString(), TEXT("QualifiedName")); // TODO - Enable move temp???
}

void VClass::Prepare(FAllocationContext Context, FInitOrValidateUVerseClass& InitOrValidate, UVerseClass* Type)
{
	// -----------------------------------------------------------------------------------------------------
	// Keep the following code in sync with FSolClassGenerator::Prepare for structs, classes, and interfaces

	checkf(Type->GetDefaultObject(false) == nullptr || InitOrValidate.IsValidating(), TEXT("Class `%s` instantiated twice!"), *Type->GetName());

	// Preserve Verse identity of this class or module
	// InitOrValidate.SetValue(VerseClass->PackageRelativeVersePath, GetPackageRelativeVersePathFromDefinition(AstNode), TEXT("PackageRelativeVersePath"));

	InitOrValidate.SetValue(Type->ClassWithin, UObject::StaticClass(), TEXT("ClassWithin"));
	InitOrValidate.SetClassFlags(CLASS_EditInlineNew, true, TEXT("EditInlineNew"));
	InitOrValidate.SetClassFlags(CLASS_HasInstancedReference, true, TEXT("HasInstancedReference"));
	InitOrValidate.SetClassFlagsNoValidate(CLASS_CompiledFromBlueprint, true);

	InitOrValidate.SetClassFlags(CLASS_Interface, GetKind() == VClass::EKind::Interface, TEXT("Interface"));
	InitOrValidate.ForceVerseClassFlags(VCLASS_Concrete, EnumHasAllFlags(Flags, EFlags::Concrete));
	InitOrValidate.ForceVerseClassFlags(VCLASS_Castable, EnumHasAllFlags(Flags, EFlags::ExplicitlyCastable));
	InitOrValidate.ForceVerseClassFlags(VCLASS_FinalSuper, EnumHasAllFlags(Flags, EFlags::FinalSuper));
	// InitOrValidate.ForceVerseClassFlags(VCLASS_UniversallyAccessible, bIsUniversal && bIsPublicScope);
	// InitOrValidate.ForceVerseClassFlags(VCLASS_EpicInternal, bIsEpicInternal || !bIsPublicScope);

	if (GetKind() == VClass::EKind::Class)
	{
		// const bool bIsConstructorEpicInternal = GetConstructorAccessibilityScope(*SemanticType->_Definition).IsEpicInternal();
		// InitOrValidate.ForceVerseClassFlags(VCLASS_EpicInternalConstructor, bIsConstructorEpicInternal || !bIsPublicScope);
		// InitOrValidate.ForceValue(VerseClass->ConstructorEffects, MakeEffectSet(SemanticType->_ConstructorEffects));

#if WITH_EDITOR
		// const FString UnmangledClassName = FULangConversionUtils::ULangStrToFString(SemanticType->GetParametricTypeScope().GetScopeName().AsString());
		// InitOrValidate.SetMetaData(true, TEXT("DisplayName"), *UnmangledClassName);

		//// Splitting the development status from the engine as it is not as clean as expected. We seem to be using Experimental classes in UEFN.
		// const UValkyrieMetaData* ValkrieMetaData = GetDefault<UValkyrieMetaData>();
		// InitOrValidate.SetMetaData(Definition->IsExperimental(), ValkrieMetaData->DevelopmentStatusKey, *ValkrieMetaData->DevelopmentStatusValue_Experimental);
		// InitOrValidate.SetMetaData(Definition->IsDeprecated(), ValkrieMetaData->DeprecationStatusKey, *ValkrieMetaData->DeprecationStatusValue_Deprecated);
#endif

		// if constexpr (FQuery::bIsClass || FQuery::bIsModule)
		//{
		//	FName PackageVersePath = NAME_None;
		//	if (const uLang::CAstPackage* Package = SemanticType->GetPackage())
		//	{
		//		// We mangle the FName so that it remains case sensitive
		//		FString FPath = FULangConversionUtils::ULangStrToFString(Package->_VersePath);
		//		PackageVersePath = FName(Verse::Names::Private::MangleCasedName(FPath, FPath));
		//	}
		//	InitOrValidate.SetValue(VerseClass->MangledPackageVersePath, PackageVersePath, TEXT("MangledPackageVersePath"));
		// }
	}

	// Also create UE classes for the superclass and for interfaces
	UClass* SuperUClass;
	int32 FirstInterfaceIndex;
	if (NumInherited > 0 && Inherited[0]->GetKind() == VClass::EKind::Class)
	{
		SuperUClass = Inherited[0]->GetOrCreateUEType<UClass>(Context);
		FirstInterfaceIndex = 1;
	}
	else
	{
		SuperUClass = GetKind() == VClass::EKind::Interface ? UInterface::StaticClass() : UObject::StaticClass();
		FirstInterfaceIndex = 0;
	}
	InitOrValidate.SetSuperStruct(SuperUClass);
	InitOrValidate.SetValue(Type->ClassConfigName, SuperUClass->ClassConfigName, TEXT("ClassConfigName"));

	int32 NumDirectInterfaces = NumInherited - FirstInterfaceIndex;
	Type->Interfaces.Reserve(NumDirectInterfaces);
	for (int32 Index = FirstInterfaceIndex; Index < NumInherited; ++Index)
	{
		VClass* SuperInterface = Inherited[Index].Get();
		V_DIE_UNLESS(SuperInterface->GetKind() == VClass::EKind::Interface);
		UClass* InheritedUClass = SuperInterface->GetOrCreateUEType<UClass>(Context);
		InitOrValidate.AddInterface(InheritedUClass, Verse::EAddInterfaceType::Direct);
	}

	// Compute transitive closure of interface hierarchy
	// Each of the added interfaces already has a respective transitive closure so all we need is to merge
	for (uint32 Index = 0; Index < NumDirectInterfaces; ++Index)
	{
		uint32 NumCurrentInterfaces = Type->Interfaces.Num();
		for (const FImplementedInterface& SuperInterface : Type->Interfaces[Index].Class->Interfaces)
		{
			InitOrValidate.AddInterface(SuperInterface.Class, Verse::EAddInterfaceType::Indirect);
		}
	}
	InitOrValidate.ValidateInterfaces();
}

template <typename UEType>
void VClass::CommonPrepare(FAllocationContext Context, UEType* Type)
{
	// -----------------------------------------------------------------------------------------------------
	// Keep the following code in sync with FSolClassGenerator::Prepare for structs, classes, and interfaces

	Private::FVerseVMInitOrValidate<UEType> InitOrValidate(Type);

#if WITH_EDITOR
	InitOrValidate.SetMetaData(true, TEXT("IsBlueprintBase"), TEXT("false"));
#endif

	if (IsNativeBound() && InitOrValidate.IsInitializing())
	{
		Type->SetNativeBound();
	}

	Prepare(Context, InitOrValidate, Type);
}

UStruct* VClass::CreateUEType(FAllocationContext Context)
{
	ensure(!HasUEType()); // Caller must ensure that this is not already set

	IEngineEnvironment* Environment = VerseVM::GetEngineEnvironment();
	check(Environment);

	// Imported types only need to do validation and create a shape.
	UStruct* UeClassOrStruct = nullptr;
	UVerseStruct* UeStruct = nullptr;
	UVerseClass* UeClass = nullptr;
	bool bIsInitializing = true;
	if (UObject* Object = ImportStruct.Get().ExtractUObject())
	{
		bIsInitializing = false;
		if (IsStruct())
		{
			UeClassOrStruct = UeStruct = Cast<UVerseStruct>(Object);
			if (UeStruct == nullptr)
			{
				UScriptStruct* Struct = Cast<UScriptStruct>(Object);
				check(Struct);
				AssociatedUEType.Set(Context, Struct);
				return Struct;
			}
			check(UeStruct->IsUHTNative());
		}
		else
		{
			UeClassOrStruct = UeClass = Cast<UVerseClass>(Object);
			if (UeClass == nullptr)
			{
				UClass* Class = Cast<UClass>(Object);
				check(Class);
				AssociatedUEType.Set(Context, Class);
				return Class;
			}
			check(UeClass->IsUHTNative());
		}
	}

	AddRedirect(IsStruct() ? ECoreRedirectFlags::Type_Struct : ECoreRedirectFlags::Type_Class);

	// If we don't have a type, create one
	if (UeClassOrStruct == nullptr)
	{
		// Create package for the class/struct
		UPackage* UePackage = Package->GetOrCreateUPackage(Context);

		EVersePackageType PackageType;
		Names::GetUPackagePath(Package->GetName().AsStringView(), &PackageType);
		UTF8CHAR Separator = PackageType == EVersePackageType::VNI ? UTF8CHAR('_') : UTF8CHAR('-');

		TUtf8StringBuilder<Names::DefaultNameLength> UeName;
		AppendMangledName(UeName, Separator);

		if (IsStruct())
		{
			UeClassOrStruct = UeStruct = NewObject<UVerseStruct>(UePackage, FName(UeName), RF_Public /* | RF_Transient*/);
		}
		else
		{
			UeClassOrStruct = UeClass = NewObject<UVerseClass>(UePackage, FName(UeName), RF_Public /* | RF_Transient*/);
		}
	}

	// Associate the verse type with the UE type
	if (IsStruct())
	{
		UeStruct->Class.Set(Context, this);
	}
	else
	{
		UeClass->Class.Set(Context, this);
	}
	AssociatedUEType.Set(Context, UeClassOrStruct);

	// Prepare the type definition
	IsStruct() ? CommonPrepare(Context, UeStruct) : CommonPrepare(Context, UeClass);

	TArray<TPair<int32, FAttributeElement>> AttributeElements;
	AttributeElements.Reserve(Archetype->NumEntries);

	AttributeElements.Emplace(0, FAttributeElement(UeClassOrStruct));

	// Populate shape and class members
	{
		FField::FLinkedListBuilder PropertyListBuilder(&UeClassOrStruct->ChildProperties);
		auto CreateProperty = [this, Context, Environment, UeClassOrStruct, &AttributeElements, &PropertyListBuilder, bIsInitializing](VArchetype::VEntry& Entry, int32 Index) {
			// Create the property
			bool bHasAttributes = AttributeIndices && AttributeIndices->GetValue(1 + Index + 0).AsInt32() < AttributeIndices->GetValue(1 + Index + 1).AsInt32();
			FUtf8StringView PropName = Verse::Names::RemoveQualifier(Entry.Name.Get()->AsStringView());
			FUtf8StringView CrcPropName = Entry.UseCRCName() ? Entry.Name.Get()->AsStringView() : PropName;

			FProperty* FieldProperty = nullptr;
			if (bIsInitializing)
			{
				FieldProperty = Environment->CreateProperty(
					Context,
					Package.Get(),
					UeClassOrStruct,
					PropName,
					CrcPropName,
					&Entry.Type.Follow().StaticCast<VType>(),
					Entry.IsNative() || bHasAttributes,
					Entry.IsInstanced());

				// If needed update the property flags prior to finalizing the UClass
				if (!Entry.HasDefaultValueExpression())
				{
					// For the editor: This property has no default and therefore must be specified
					FieldProperty->PropertyFlags |= EPropertyFlags::CPF_RequiredParm;
				}

				if (bHasAttributes)
				{
					AttributeElements.Emplace(1 + Index, FAttributeElement(FieldProperty));
				}

				// Link the newly created property to the property list
				PropertyListBuilder.AppendNoTerminate(*FieldProperty);
			}
			else
			{
				FString PropNameString(PropName);
				FString CrcPropNameString(CrcPropName);
				FName UEVerseName = Verse::Names::VersePropToUEFName(PropNameString, CrcPropNameString);
				FieldProperty = UeClassOrStruct->FindPropertyByName(UEVerseName);
				if (FieldProperty != nullptr)
				{
					VType* PropertyType = Entry.Type.Get().Follow().DynamicCast<VType>();
					if (!VerseVM::GetEngineEnvironment()->ValidateProperty(Context, FName(*PropNameString), PropertyType, FieldProperty, Entry.IsInstanced()))
					{
						V_DIE("The imported type: `%s` does not have the required property type for the property `%s`", *GetBaseName().AsString(), *PropNameString);
					}
				}
				else
				{
					V_DIE("The imported type: `%s` does not contain the required property `%s`", *GetBaseName().AsString(), *PropNameString);
				}
			}
			return FieldProperty;
		};

		UField::FLinkedListBuilder ChildrenBuilder(ToRawPtr(MutableView(UeClassOrStruct->Children)));
		auto CreateFunction = [this, Context, UeClass, &ChildrenBuilder](VArchetype::VEntry& Entry) {
			UFunction* Function = MaybeCreateUFunctionForCallee(Context, Entry.Value.Get());
			if (Function != nullptr)
			{
				ChildrenBuilder.AppendNoTerminate(*Function);
				UeClass->AddFunctionToFunctionMap(Function, Function->GetFName());
			}
			return Function;
		};
		CreateShapeForUStruct(Context, CreateProperty, CreateFunction);

		if (UeClass != nullptr)
		{
			if (!bIsInitializing)
			{
				TUtf8StringBuilder<Names::DefaultNameLength> ScopeName;
				AppendScopeName(ScopeName);
				UeClass->BindVerseCallableFunctions(Package.Get(), ScopeName);
			}
		}
	}

	if (bIsInitializing)
	{
		// Finalize class/struct
		UeClassOrStruct->Bind();
		UeClassOrStruct->StaticLink(/*bRelinkExistingProperties =*/true);

		// Bind the class if native.  This will adjust the offsets for all the properties to point
		// to the actual location of the data in the native C++ class.
		if (IsNativeBound())
		{
			Environment->TryBindVniType(Package.Get(), UeClassOrStruct);
		}

		if (Attributes)
		{
			TArray<FString> Errors;
			V_DIE_UNLESS(Attributes->GetArrayType() == Verse::EArrayType::VValue || Attributes->GetArrayType() == Verse::EArrayType::None);
			VValue* AttributeValues = Attributes->GetData<VValue>();
			for (TPair<int32, FAttributeElement>& Element : AttributeElements)
			{
				int32 Begin = AttributeIndices->GetValue(Element.Key + 0).AsInt32();
				int32 End = AttributeIndices->GetValue(Element.Key + 1).AsInt32();
				for (VValue AttributeValue : TArrayView<VValue>(AttributeValues + Begin, End - Begin))
				{
					Element.Value.Apply(Context, AttributeValue.Follow(), Errors);
				}
			}
			FStringBuilderBase ErrorMessages;
			for (const FString& Error : Errors)
			{
				ErrorMessages.Append(Error);
			}
			V_DIE_UNLESS_MSG(Errors.IsEmpty(), "%s", ErrorMessages.ToString());
		}

		if (UeClass != nullptr)
		{
			// Collect all UObjects referenced by FProperties and assemble the GC token stream

			UeClass->CollectBytecodeAndPropertyReferencedObjectsRecursively();
			UeClass->AssembleReferenceTokenStream(/*bForce=*/true);

			// Setup the CDO

			UObject* CDO = UeClass->GetDefaultObject();
			V_DIE_UNLESS(CDO);

			// TODO(SOL-7928): Don't run default field initializers on CDOs. This is a hack for BPVM compatibility.
			// Serialized instances of Verse classes use these subobjects as their archetypes.
			{
				FRunningContext RunningContext = FRunningContextPromise{};

				FInstantiationScope InitCtx(CDO, RF_Public | RF_Transactional | RF_ArchetypeObject | RF_DefaultSubObject);
				VNativeConstructorWrapper& Wrapper = VNativeConstructorWrapper::New(RunningContext, *CDO);
				FOpResult InitResult = GetConstructor().InvokeWithSelf(RunningContext, VValue(Wrapper), {/*SkipSupers = */ GlobalTrue(), /*SkipBlocks = */ GlobalTrue()});
				V_DIE_UNLESS(InitResult.IsReturn());

				UVerseClass::RenameDefaultSubobjects(CDO);
			}
		}

		if (UeStruct != nullptr)
		{
			UeStruct->AssembleReferenceTokenStream(/*bForce=*/true);
			if (!UeStruct->ReferenceSchema.Get().IsEmpty())
			{
				EnumAddFlags(Flags, EFlags::NativeStructWithObjectReferences);
			}
		}
	}
	return UeClassOrStruct;
}

bool VClass::SubsumesImpl(FAllocationContext Context, VValue Value)
{
	VClass* InputType = nullptr;
	if (VObject* Object = Value.DynamicCast<VObject>())
	{
		VCell* TypeCell = Object->GetEmergentType()->Type.Get();
		checkSlow(TypeCell->IsA<VClass>());
		InputType = static_cast<VClass*>(TypeCell);
	}
	else if (Value.IsUObject())
	{
		InputType = CastChecked<UVerseClass>(Value.AsUObject()->GetClass())->Class.Get();
	}
	else
	{
		return false;
	}

	if (InputType == this)
	{
		return true;
	}

	TArray<VClass*, TInlineAllocator<8>> ToCheck;
	auto PushInherited = [&ToCheck](VClass* Class) {
		for (uint32 I = 0; I < Class->NumInherited; ++I)
		{
			ToCheck.Push(Class->Inherited[I].Get());
		}
	};

	PushInherited(InputType);
	while (ToCheck.Num())
	{
		VClass* Class = ToCheck.Pop();
		if (Class == this)
		{
			return true;
		}
		PushInherited(Class);
	}

	return false;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

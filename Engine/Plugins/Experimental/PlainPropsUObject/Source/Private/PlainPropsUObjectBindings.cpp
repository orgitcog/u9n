// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsUObjectRuntime.h"
#include "PlainPropsUeCoreBindings.h"
#include "PlainPropsBuildSchema.h"
#include "PlainPropsDiff.h"
#include "PlainPropsParse.h"
#include "PlainPropsPrint.h"
#include "PlainPropsRead.h"
#include "PlainPropsVisualize.h"
#include "PlainPropsWrite.h"
#include "Algo/Compare.h"
#include "Algo/Find.h"
#include "Containers/PagedArray.h"
#include "HAL/FileManager.h"
#include "Hash/xxhash.h"
#include "Logging/StructuredLog.h"
#include "Math/PreciseFP.h"
#include "Misc/AsciiSet.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DefinePrivateMemberPtr.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Templates/UniquePtr.h"
#include "UObject/AnsiStrProperty.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/FieldPathProperty.h"
#include "UObject/Object.h"
#include "UObject/PropertyOptional.h"
#include "UObject/StrProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/Utf8StrProperty.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "UObject/VerseValueProperty.h"
#include "UObject/VerseStringProperty.h"


using FUnicastScriptDelegate = TScriptDelegate<FNotThreadSafeNotCheckedDelegateMode>;
using FMulticastInvocationList = TArray<FUnicastScriptDelegate>;
using FMulticastInvocationView = TConstArrayView<FUnicastScriptDelegate>;
using FDelegateBase = TDelegateAccessHandlerBase<FNotThreadSafeDelegateMode>;

// Temp hacks. Long-term either add FProperty getters for ctor/dtor/hash function pointers 
// and delegate APIs for non-intrusive serialization or integrate PlainProps into Core/CoreUObject
UE_DEFINE_PRIVATE_MEMBER_PTR(void(void*) const, GInitPropertyValue, FProperty, InitializeValueInternal);
UE_DEFINE_PRIVATE_MEMBER_PTR(void(void*) const, GDestroyPropertyValue, FProperty, DestroyValueInternal);
UE_DEFINE_PRIVATE_MEMBER_PTR(uint32(const void*) const, GHashPropertyValue, FProperty, GetValueTypeHashInternal);
UE_DEFINE_PRIVATE_MEMBER_PTR(TArray<FName>, GFieldPathPath, FFieldPath, Path);
UE_DEFINE_PRIVATE_MEMBER_PTR(TWeakObjectPtr<UStruct>, GFieldPathOwner, FFieldPath, ResolvedOwner);
UE_DEFINE_PRIVATE_MEMBER_PTR(FWeakObjectPtr, GDelegateObject, FScriptDelegate, Object);
UE_DEFINE_PRIVATE_MEMBER_PTR(FName, GDelegateFunctionName, FScriptDelegate, FunctionName);
UE_DEFINE_PRIVATE_MEMBER_PTR(FWeakObjectPtr, GUnicastDelegateObject, FUnicastScriptDelegate, Object);
UE_DEFINE_PRIVATE_MEMBER_PTR(FName, GUnicastDelegateFunctionName, FUnicastScriptDelegate, FunctionName);
UE_DEFINE_PRIVATE_MEMBER_PTR(FMulticastInvocationList, GMulticastDelegateInvocationList, FMulticastScriptDelegate, InvocationList);
UE_DEFINE_PRIVATE_MEMBER_PTR(bool, GSparseDelegateIsBound, FSparseDelegate, bIsBound);

#if UE_DETECT_DELEGATES_RACE_CONDITIONS && 0
UE_DEFINE_PRIVATE_MEMBER_PTR(FMRSWRecursiveAccessDetector, GDelegateAccessDetector, FDelegateBase, AccessDetector);
struct FDelegateAccess : public FDelegateBase
{
	struct FReadScope : public FDelegateBase::FReadAccessScope
	{
		explicit FReadScope(const FDelegateBase& In) : FDelegateBase::FReadAccessScope(In.*GDelegateAccessDetector) {}
	};
	struct FWriteScope : public FDelegateBase::FWriteAccessScope
	{
		explicit FWriteScope(FDelegateBase& In) : FDelegateBase::FWriteAccessScope(In.*GDelegateAccessDetector) {}
	};
};
#else
struct FDelegateAccess
{
	struct FReadScope { FReadScope(const FDelegateBase& In) {} };
	using FWriteScope = FReadScope;
};
#endif // UE_DETECT_DELEGATES_RACE_CONDITIONS


DEFINE_LOG_CATEGORY_STATIC(LogPlainPropsUObject, Log, All);

namespace PlainProps::UE
{

static constexpr ERangeSizeType DefaultRangeMax = RangeSizeOf(FDefaultAllocator::SizeType{});

////////////////////////////////////////////////////////////////////////////////////////////////

struct FDefaultStruct
{
	UScriptStruct::ICppStructOps&		Ops;
	alignas(16) uint8					Instance[0];
};

static FDefaultStruct* NewDefaultStruct(UScriptStruct::ICppStructOps& Ops)
{
	check(Ops.GetAlignment() <= 16);
	uint32 Size = sizeof(FDefaultStruct) + Ops.GetSize();
	FDefaultStruct Header = {Ops};
	FDefaultStruct* Out = new (FMemory::MallocZeroed(Size)) FDefaultStruct(Header);
	Ops.Construct(Out->Instance);
	return Out;
}

inline void	DeleteDefaultStruct(uint8* Instance)
{
	FDefaultStruct* Struct = reinterpret_cast<FDefaultStruct*>(Instance - offsetof(FDefaultStruct, Instance));
	if (Struct->Ops.HasDestructor())
	{
		Struct->Ops.Destruct(Instance);
	}
	FMemory::Free(Struct);
}

static constexpr uint64			DefaultInstanceStaticMask = 1;
inline FDefaultInstance			MakeStaticInstance(const void* Static)			{ return { reinterpret_cast<uint64>(Static) | DefaultInstanceStaticMask }; }
inline FDefaultInstance			MakeDefaultInstance(FDefaultStruct* Default)	{ return { reinterpret_cast<uint64>(Default->Instance) }; }
inline uint8*					GetInstance(FDefaultInstance Instance)			{ return reinterpret_cast<uint8*>(Instance.Ptr & ~DefaultInstanceStaticMask); }
inline void						DeleteInstance(FDefaultInstance Instance)
{
	if (!(Instance.Ptr & DefaultInstanceStaticMask))
	{
		DeleteDefaultStruct(reinterpret_cast<uint8*>(Instance.Ptr));
	}
}

static void ReserveZeroes(/* in-out */ FMutableMemoryView& Zeroes, SIZE_T Size, uint32 Alignment)
{
	Size +=  FMath::Max<int32>(0, Alignment - 16);
	Size = Align(Size, 4096);
	if (Zeroes.GetSize() < Size)
	{
		FMemory::Free(Zeroes.GetData());
		Zeroes = FMutableMemoryView(FMemory::MallocZeroed(Size, 16), Size);
	}
}

FDefaultStructs::~FDefaultStructs()
{
	for (TPair<FBindId, FDefaultInstance> Instance : Instances)
	{
		DeleteInstance(Instance.Value);
	}
}

inline bool Flip(FBitReference Bit)
{
	Bit = !Bit;
	return Bit;
}

void FDefaultStructs::ReserveFlags(uint32 Idx)
{
	if (Idx >= static_cast<uint32>(Instanced.Num()))
	{
		Instanced.SetNum(FMath::RoundUpToPowerOfTwo64(Idx + 1), false);
#if DO_CHECK
		Bound.SetNum(Instanced.Num(), false);
#endif
	}
}

void FDefaultStructs::Bind(FBindId Id, const UScriptStruct* Struct)
{
	const EStructFlags Flags = Struct->StructFlags;
	const SIZE_T Size = Struct->GetStructureSize();
	const uint32 Alignment = Struct->GetMinAlignment();
	UScriptStruct::ICppStructOps* Ops = Struct->GetCppStructOps();

	ReserveFlags(Id.Idx);
#if DO_CHECK
	checkf(Flip(Bound[Id.Idx]), TEXT("'%s' already bound"), *GUE.Debug.Print(Id));
#endif

	if (const UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Struct))
	{
		const void* DefaultInstance = UserStruct->GetDefaultInstance();
		check(DefaultInstance);
		if (FMemory::MemIsZero(DefaultInstance, Size))
		{
			ReserveZeroes(Zeroes, Size, Alignment);
		}
		else
		{
			Instanced[Id.Idx] = true;
			Instances.Emplace(Id, MakeStaticInstance(DefaultInstance));
		}
	}
	else if (!!(Flags & STRUCT_ZeroConstructor) || Ops == nullptr)
	{
		ReserveZeroes(Zeroes, Size, Alignment);
	}
	else
	{
		check(Ops->GetSize() == Size);
		FDefaultStruct* Default = NewDefaultStruct(*Ops);
		if (FMemory::MemIsZero(Default->Instance, Size))
		{
			DeleteDefaultStruct(Default->Instance);
			ReserveZeroes(Zeroes, Size, Alignment);
		}
		else
		{
			Instanced[Id.Idx] = true;
			Instances.Add(Id, MakeDefaultInstance(Default));
		}
	}
}

void FDefaultStructs::BindZeroes(FBindId Id, SIZE_T Size, uint32 Alignment)
{
	ReserveFlags(Id.Idx);
#if DO_CHECK
	checkf(Flip(Bound[Id.Idx]), TEXT("'%s' already bound"), *GUE.Debug.Print(Id));
#endif
	ReserveZeroes(Zeroes, Size, Alignment);
}

void FDefaultStructs::BindStatic(FBindId Id, const void* Struct)
{
	ReserveFlags(Id.Idx);
#if DO_CHECK
	checkf(Flip(Bound[Id.Idx]), TEXT("'%s' already bound"), *GUE.Debug.Print(Id));
#endif
	check(!Instanced[Id.Idx]);
	check(GetInstance(MakeStaticInstance(Struct)) == Struct);

	Instanced[Id.Idx] = true;
	Instances.Add(Id, MakeStaticInstance(Struct));
}

void FDefaultStructs::Drop(FBindId Id)
{
#if DO_CHECK
	checkf(!Flip(Bound[Id.Idx]), TEXT("'%s' isn't bound"), *GUE.Debug.Print(Id));
#endif
	if (Instanced[Id.Idx])
	{
		Instanced[Id.Idx] = false;
		DeleteInstance(Instances.FindAndRemoveChecked(Id));
	}
}

const void* FDefaultStructs::Get(FBindId Id)
{
#if DO_CHECK
	checkf(Bound[Id.Idx], TEXT("'%s' lack default"), *GUE.Debug.Print(Id));
#endif
	return Instanced[Id.Idx] ? GetInstance(Instances.FindChecked(Id)) : Zeroes.GetData(); 
}

////////////////////////////////////////////////////////////////////////////////////////////////

FMemberId FNumeralGenerator::Grow(int32 Max)
{
	check(Max > Cache.Num());
	int32 OldNum = Cache.Num();
	Cache.SetNumUninitialized(Max + 1);

	FName Numeral("_");
	for (int32 Idx = OldNum; Idx <= Max; ++Idx)
	{
		Numeral.SetNumber(NAME_EXTERNAL_TO_INTERNAL(Idx));
		Cache[Idx] = GUE.Names.NameMember(Numeral);
	}

	return Cache[Max];
}

////////////////////////////////////////////////////////////////////////////////////////////////

FCommonScopeIds::FCommonScopeIds(TIdIndexer<FName>& Names)
: Core(						Names.MakeScope("/Script/Core"))
, CoreUObject(				Names.MakeScope("/Script/CoreUObject"))
{}

FCommonTypenameIds::FCommonTypenameIds(TIdIndexer<FName>& Names)
: Optional(					Names.NameType("Optional"))
, Map(						Names.NameType("Map"))
, Set(						Names.NameType("Set"))
, Pair(						Names.NameType("Pair"))
, LeafArray(				Names.NameType("LeafArray"))
, TrivialArray(				Names.NameType("TrivialArray"))
, NonTrivialArray(			Names.NameType("NonTrivialArray"))
, StaticArray(				Names.NameType("StaticArray"))
, TrivialOptional(			Names.NameType("TrivialOptional"))
, IntrusiveOptional(		Names.NameType("IntrusiveOptional"))
, NonIntrusiveOptional(		Names.NameType("NonIntrusiveOptional"))
, String(					Names.NameType("String"))
, Utf8String(				Names.NameType("Utf8String"))
, AnsiString(				Names.NameType("AnsiString"))
, VerseString(				Names.NameType("VerseString"))
{}

FCommonStructIds::FCommonStructIds(const FCommonScopeIds& Scopes, TIdIndexer<FName>& Names)
: Name(						Names.IndexStruct({Scopes.Core, Names.MakeTypename("Name")}))
, Text(						Names.IndexStruct({Scopes.Core, Names.MakeTypename("Text")}))
, Guid(						Names.IndexStruct({Scopes.Core, Names.MakeTypename("Guid")}))
, FieldPath(				Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("FieldPath")}))
, SoftObjectPath(			Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("SoftObjectPath")}))
, ClassPtr(					Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("ClassPtr")}))
, ObjectPtr(				Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("ObjectPtr")}))
, WeakObjectPtr(			Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("WeakObjectPtr")}))
, LazyObjectPtr(			Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("LazyObjectPtr")}))
, SoftObjectPtr(			Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("SoftObjectPtr")}))
, ScriptInterface(			Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("ScriptInterface")}))
, Delegate(					Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("Delegate")}))
, MulticastDelegate(		Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("MulticastDelegate")}))
, MulticastInlineDelegate(	Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("MulticastInlineDelegate")}))
, MulticastSparseDelegate(	Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("MulticastSparseDelegate")}))
, VerseFunction(			Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("VerseFunction")}))
, DynamicallyTypedValue(	Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("DynamicallyTypedValue")}))
, ReferencePropertyValue(	Names.IndexStruct({Scopes.CoreUObject, Names.MakeTypename("ReferencePropertyValue")}))
{}

FCommonMemberIds::FCommonMemberIds(TIdIndexer<FName>& Names)
: Key(						Names.NameMember("Key"))
, Value(					Names.NameMember("Value"))
, Assign(					Names.NameMember("Assign"))
, Remove(					Names.NameMember("Remove"))
, Insert(					Names.NameMember("Insert"))
, Id(						Names.NameMember("Id"))
, Object(					Names.NameMember("Object"))
, Function(					Names.NameMember("Function"))
, Invocations(				Names.NameMember("Invocations"))
, Path(						Names.NameMember("Path"))
, Owner(					Names.NameMember("Owner"))
{}

////////////////////////////////////////////////////////////////////////////////////////////////

FGlobals::FGlobals() 
: Types(FDebugIds(Names))
, Schemas(FDebugIds(Names))
, Customs(FDebugIds(Names))
, Scopes(Names)
, Structs(Scopes, Names)
, Typenames(Names)
, Members(Names)
, Debug(Names)
{}

FGlobals GUE;

////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr uint64 LeafMask =			CASTCLASS_FNumericProperty | CASTCLASS_FEnumProperty| CASTCLASS_FBoolProperty;
static constexpr uint64 IntSMask =			CASTCLASS_FInt8Property | CASTCLASS_FInt16Property | CASTCLASS_FIntProperty | CASTCLASS_FInt64Property;
static constexpr uint64 IntUMask =			CASTCLASS_FByteProperty | CASTCLASS_FUInt16Property | CASTCLASS_FUInt32Property | CASTCLASS_FUInt64Property;
static constexpr uint64 ContainerMask =		CASTCLASS_FArrayProperty | CASTCLASS_FSetProperty | CASTCLASS_FMapProperty  | CASTCLASS_FOptionalProperty;
static constexpr uint64 StringMask =		CASTCLASS_FStrProperty | CASTCLASS_FUtf8StrProperty | CASTCLASS_FAnsiStrProperty | CASTCLASS_FVerseStringProperty;
static constexpr uint64 CommonStructMask =	CASTCLASS_FNameProperty | CASTCLASS_FTextProperty |  CASTCLASS_FFieldPathProperty | CASTCLASS_FClassProperty |
											CASTCLASS_FObjectProperty | CASTCLASS_FWeakObjectProperty | CASTCLASS_FSoftObjectProperty | CASTCLASS_FLazyObjectProperty |
											CASTCLASS_FDelegateProperty | CASTCLASS_FMulticastInlineDelegateProperty;
static constexpr uint64 MiscMask =			CASTCLASS_FMulticastSparseDelegateProperty | CASTCLASS_FInterfaceProperty;

static FBindId FlagsToCommonBindId(uint64 MaskedCastFlags)
{
	switch (MaskedCastFlags)
	{
		case CASTCLASS_FNameProperty:								return GUE.Structs.Name;
		case CASTCLASS_FClassProperty | CASTCLASS_FObjectProperty:	return GUE.Structs.ClassPtr;
		case CASTCLASS_FObjectProperty:								return GUE.Structs.ObjectPtr;
		case CASTCLASS_FWeakObjectProperty:							return GUE.Structs.WeakObjectPtr;
		case CASTCLASS_FSoftObjectProperty:							return GUE.Structs.SoftObjectPtr;
		case CASTCLASS_FLazyObjectProperty:							return GUE.Structs.LazyObjectPtr;
		case CASTCLASS_FDelegateProperty:							return GUE.Structs.Delegate;
		case CASTCLASS_FMulticastInlineDelegateProperty:			return GUE.Structs.MulticastInlineDelegate;
		case CASTCLASS_FTextProperty:								return GUE.Structs.Text;
		case CASTCLASS_FFieldPathProperty:							return GUE.Structs.FieldPath;
		default:													break; // error
	}

	check(MaskedCastFlags); // @pre violated
	check((MaskedCastFlags & CommonStructMask) == MaskedCastFlags); // @pre violated
	checkf(FMath::CountBits(MaskedCastFlags) == 1, TEXT("Masked CASTCLASS flags %llx match more than one common property type"), MaskedCastFlags);
	check(false); // Mismatch between this function and CommonStructMask
	return {};
}

template<uint64 Mask>
static bool HasAny(uint64 Flags)
{
	return (Mask & Flags) != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static bool ShouldBind(const UStruct* Struct);

inline bool ShouldBind(const FProperty* Property)
{
	if (HasAny<CASTCLASS_FStructProperty>(Property->GetCastFlags()))
	{
		return ShouldBind(static_cast<const FStructProperty*>(Property)->Struct);
	}

	return true;
}

static bool ShouldBind(const UStruct* Struct)
{
	for (FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (ShouldBind(Property))
		{
			return true;
		}
	}
	return false;
}

// Serialize property-less UObject and UScriptStructs as their super class.
//
// E.g. FVector_NetQuantize10 is a pure runtime abstraction serialized as FVector.
//		FAttenuationSubmixSendSettings is just a FSoundSubmixSendInfoBase but has a different 
//		default constructor that matters in sparse delta serialization.
//		UObjects are never instantiated during serialization so can be safely simplified.
//
// These heuristics might need more tuning
static const UStruct* SkipEmptyBases(const UStruct* In)
{
	const UStruct* FirstOwner = In->PropertyLink ? In->PropertyLink->GetOwnerChecked<UStruct>() : In;
	if (In != FirstOwner)
	{
		if (const UScriptStruct* Struct = Cast<const UScriptStruct>(In))
		{
			if (!(Struct->StructFlags & STRUCT_ZeroConstructor))
			{
				return In;
			}
		}

		return FirstOwner;
	}
	return In;
}

static FType IndexType(const UField* Field)
{
	check(Field);
	FTypenameId Name = GUE.Names.MakeTypename(Field->GetFName());
	
	// This wouldn't be needed in an intrusive or cached solution
	TArray<FFlatScopeId, TInlineAllocator<64>> ReversedOuters;
	for (const UObject* Outer = Field->GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		ReversedOuters.Add(GUE.Names.NameScope(Outer->GetFName()));
	}
	
	return { GUE.Names.NestReversedScopes(ReversedOuters), Name };
}

static FOptionalDeclId IndexSuper(const UStruct* Struct)
{
	if (const UStruct* Super = Struct->GetInheritanceSuper())
	{
		if (ShouldBind(Super))
		{
			const UStruct* NonEmptySuper = SkipEmptyBases(Super);
			return GUE.Names.IndexDeclId(IndexType(NonEmptySuper));
		}
	}
	
	return NoId;
}

static EMemberPresence GetOccupancy(const UStruct* Struct)
{
	if (Struct->HasAnyCastFlags(CASTCLASS_UScriptStruct))
	{
		EStructFlags Flags = static_cast<const UScriptStruct*>(Struct)->StructFlags;
		return (Flags & (STRUCT_Immutable | STRUCT_Atomic)) ? EMemberPresence::RequireAll : EMemberPresence::AllowSparse; 
	}
	return EMemberPresence::AllowSparse;
}

using FMemberArray = TArray<FMemberId, TInlineAllocator<64>>;

inline void DeclareMembers(FMemberArray& Out, const UStruct* Struct)
{
	for (FProperty* It = Struct->PropertyLink; It && It->GetOwner<UStruct>() == Struct; It = It->PropertyLinkNext)
	{
		if (ShouldBind(It))
		{
			Out.Add(GUE.Names.NameMember(It->GetFName()));
		}
	}
}

// Must match BindSuperMembers
static void DeclareSuperMembers(FMemberArray& Out, const UStruct* Struct)
{
	if (const UStruct* Super = Struct->GetInheritanceSuper())
	{
		DeclareSuperMembers(Out, Super);
		if (ShouldBind(Super))
		{
			DeclareMembers(Out, Super);
		}
	}
}

static void DeclareStruct(const UStruct* Struct, FType Type, FDeclId Id)
{
	FOptionalDeclId Super = IndexSuper(Struct);
	EMemberPresence Occupancy = GetOccupancy(Struct);
	
	FMemberArray Members;
	if (Super && Occupancy == EMemberPresence::RequireAll)
	{
		// Flatten inheritance chain for dense structs
		Super = NoId;
		DeclareSuperMembers(/* out */ Members, Struct);
	}
	DeclareMembers(/* out */ Members, Struct);

	GUE.Types.DeclareStruct(Id, Type, 0, Members, Occupancy, Super);
}

static FDeclId DeclareStruct(const UStruct* Struct)
{
	FType Type = IndexType(Struct);
	FDeclId Id = GUE.Names.IndexDeclId(Type);
	DeclareStruct(Struct, Type, Id);
	return Id;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static FTypedRange SaveNames(TConstArrayView<FName> Names, const FSaveContext& Ctx)
{
	FBindId Id = GUE.Structs.Name;
	FStructRangeSaver Out(Ctx.Scratch, Names.Num());
	for (FName Name : Names)
	{
		Out.AddItem(SaveStruct(&Name, Id, Ctx));
	}
	return Out.Finalize(MakeStructRangeSchema(DefaultRangeMax, Id));
}

static void LoadNames(TArray<FName>& Dst, FStructRangeLoadView Src)
{
	Dst.SetNumUninitialized(static_cast<int32>(Src.Num()));
	FName* DstIt = Dst.GetData();
	for (FStructLoadView Name : Src)
	{
		LoadStruct(DstIt++, Name);
	}
}

void FFieldPathBinding::Save(FMemberBuilder& Dst, const FFieldPath& Src, const FFieldPath*, const FSaveContext& Ctx) const
{
	Dst.AddRange(GUE.Members.Path, SaveNames(Src.*GFieldPathPath, Ctx));
	Dst.AddStruct(GUE.Members.Owner, GUE.Structs.WeakObjectPtr, SaveStruct(&(Src.*GFieldPathOwner), GUE.Structs.WeakObjectPtr, Ctx));
}

void FFieldPathBinding::Load(FFieldPath& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
{
	FMemberLoader Members(Src);
	Dst.Reset(); // ClearCachedField() more optimal
	LoadNames(Dst.*GFieldPathPath, Members.GrabRange().AsStructs());
	LoadStruct(&(Dst.*GFieldPathOwner), Members.GrabStruct());
}

bool FFieldPathBinding::Diff(const FFieldPath& A, const FFieldPath& B, const FBindContext&)
{
	return A != B;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FDelegateBinding::Save(FMemberBuilder& Dst, const FScriptDelegate& Src, const FScriptDelegate* Default, const FSaveContext& Ctx) const
{
	FDelegateAccess::FReadScope Scope(Src);
	if (FName Function = Src.*GDelegateFunctionName; Function != FName())
	{
		Dst.AddStruct(GUE.Members.Object, GUE.Structs.WeakObjectPtr, SaveStruct(&(Src.*GDelegateObject), GUE.Structs.WeakObjectPtr, Ctx));
		Dst.AddStruct(GUE.Members.Function, GUE.Structs.Name, SaveStruct(&Function, GUE.Structs.Name, Ctx));
	}
}

void FDelegateBinding::Load(FScriptDelegate& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
{
	FMemberLoader Members(Src);
	if (Members.HasMore())
	{
		FDelegateAccess::FWriteScope Scope(Dst);
		LoadStruct(&(Dst.*GDelegateObject), Members.GrabStruct());
		LoadStruct(&(Dst.*GDelegateFunctionName), Members.GrabStruct());
	}
	else
	{
		Dst.Clear();
	}
}

bool FDelegateBinding::Diff(const FScriptDelegate& A, const FScriptDelegate& B, const FBindContext&)
{
	return A != B;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static void SaveUnicastDelegate(FMemberBuilder& Dst, const FUnicastScriptDelegate& Src, const FSaveContext& Ctx)
{
	Dst.AddStruct(GUE.Members.Object, GUE.Structs.WeakObjectPtr, SaveStruct(&(Src.*GUnicastDelegateObject), GUE.Structs.WeakObjectPtr, Ctx));
	Dst.AddStruct(GUE.Members.Function, GUE.Structs.Name, SaveStruct(&(Src.*GUnicastDelegateFunctionName), GUE.Structs.Name, Ctx));
}

static void LoadUnicastDelegate(FUnicastScriptDelegate& Dst, FStructLoadView Src)
{
	FMemberLoader Members(Src);
	LoadStruct(&(Dst.*GUnicastDelegateObject), Members.GrabStruct());
	LoadStruct(&(Dst.*GUnicastDelegateFunctionName), Members.GrabStruct());
}

static FTypedRange SaveInvocations(const FMulticastInvocationList& In, const FSaveContext& Ctx)
{
	FBindId ItemId = GUE.Structs.Delegate;
	FMemberSchema Schema = MakeStructRangeSchema(DefaultRangeMax, ItemId);
	if (int32 NumTotal = In.Num())
	{
		TBitArray<> Keep;
		Keep.Reserve(NumTotal);
		for (const FUnicastScriptDelegate& Invocation : In)
		{
			Keep.Add(!Invocation.IsCompactable());
		}
	
		if (int32 NumKept = Keep.CountSetBits())
		{
			const FStructDeclaration& ItemDecl = GUE.Types.Get(LowerCast(ItemId));
			const FUnicastScriptDelegate* Src = In.GetData();
			FStructRangeSaver Dst(Ctx.Scratch, static_cast<uint64>(NumKept));
			FMemberBuilder Tmp;
			for (int32 Idx = 0; Idx < NumTotal; ++Idx)
			{
				if (Keep[Idx])
				{
					SaveUnicastDelegate(/* out */ Tmp, Src[Idx], Ctx);
					Dst.AddItem(Tmp.BuildAndReset(Ctx.Scratch, ItemDecl, GUE.Debug));
				}
			}
			return Dst.Finalize(Schema);
		}
	}

	return {Schema, nullptr};
}

static void SaveMulticastDelegate(FMemberBuilder& Dst, const FMulticastScriptDelegate& Src, const FSaveContext& Ctx)
{
	FDelegateAccess::FReadScope Scope(Src);
	Dst.AddRange(GUE.Members.Invocations, SaveInvocations(Src.*GMulticastDelegateInvocationList, Ctx));
}

static void SaveEmptyMulticastDelegate(FMemberBuilder& Dst)
{
	Dst.AddRange(GUE.Members.Invocations, { MakeStructRangeSchema(DefaultRangeMax, GUE.Structs.Delegate), nullptr });
}

static void LoadInvocations(FMulticastInvocationList& Dst, FStructRangeLoadView Src)
{
	Dst.Reset(static_cast<int32>(Src.Num()));
	for (FStructLoadView Invocation : Src)
	{
		LoadUnicastDelegate(Dst.AddDefaulted_GetRef(), Invocation);
	}
}

static void LoadMulticastDelegate(FMulticastScriptDelegate& Dst, FMemberLoader& Src)
{
	FDelegateAccess::FWriteScope Scope(Dst);
	LoadInvocations(Dst.*GMulticastDelegateInvocationList, Src.GrabRange().AsStructs());
}

inline bool DiffInvocations(TConstArrayView<FUnicastScriptDelegate> A, TConstArrayView<FUnicastScriptDelegate> B)
{
	if (A.Num() + B.Num() == 0)
	{
		return false;
	}

	const FUnicastScriptDelegate* EndA = A.GetData() + A.Num();
	const FUnicastScriptDelegate* EndB = B.GetData() + B.Num();
	for (const FUnicastScriptDelegate* ItA = A.GetData(), *ItB = B.GetData(); true; ++ItA, ++ItB)
	{
		for (; ItA != EndA && ItA->IsCompactable(); ++ItA) {}
		for (; ItB != EndB && ItB->IsCompactable(); ++ItB) {}

		if (ItA == EndA || ItB == EndB)
		{
			return ItA != EndA || ItB != EndB;
		}
		else if (*ItA != *ItB)
		{
			return true;
		}
	}
}

static bool DiffMulticastDelegate(const FMulticastScriptDelegate& A, const FMulticastScriptDelegate& B)
{
	return DiffInvocations(A.*GMulticastDelegateInvocationList, B.*GMulticastDelegateInvocationList);
}

void FMulticastInlineDelegateBinding::Save(FMemberBuilder& Dst, const FMulticastScriptDelegate& Src, const FMulticastScriptDelegate* Default, const FSaveContext& Ctx) const
{
	SaveMulticastDelegate(Dst, Src, Ctx);
}

void FMulticastInlineDelegateBinding::Load(FMulticastScriptDelegate& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
{
	check(Method == ECustomLoadMethod::Assign);
	FMemberLoader Members(Src);
	LoadMulticastDelegate(Dst, Members);
}

bool FMulticastInlineDelegateBinding::Diff(const FMulticastScriptDelegate& A, const FMulticastScriptDelegate& B, const FBindContext&)
{
	return DiffMulticastDelegate(A, B);
}

////////////////////////////////////////////////////////////////////////////////////////////////

struct FMulticastSparseDelegateBinding final : ICustomBinding
{
	explicit FMulticastSparseDelegateBinding(const USparseDelegateFunction* SignatureFunction)
	: OwningClassName(SignatureFunction->OwningClassName)
	, DelegateName(SignatureFunction->DelegateName)
	{}

	const FName OwningClassName;
	const FName DelegateName;

	virtual void SaveCustom(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) override
	{
		if (!Default || DiffCustom(Src, Default, Ctx))
		{
			Save(Dst, *static_cast<const FSparseDelegate*>(Src), Ctx);
		}
	}

	virtual void LoadCustom(void* Dst, FStructLoadView Src, ECustomLoadMethod Method) const override
	{
		check(Method == ECustomLoadMethod::Assign);
		Load(*static_cast<FSparseDelegate*>(Dst), Src);
	}

	virtual bool DiffCustom(const void* A, const void* B, const FBindContext&) const override
	{
		return Diff(*static_cast<const FSparseDelegate*>(A), *static_cast<const FSparseDelegate*>(B));
	}

	const FMulticastScriptDelegate* GetMulticastDelegate(const FSparseDelegate& Sparse) const
	{
		if (Sparse.IsBound())
		{
			const UObject* Owner = FSparseDelegateStorage::ResolveSparseOwner(Sparse, OwningClassName, DelegateName);
			return FSparseDelegateStorage::GetMulticastDelegate(Owner, DelegateName);
		}
		return nullptr;
	}
	
	void Save(FMemberBuilder& Dst, const FSparseDelegate& Src, const FSaveContext& Ctx) const
	{
		if (const FMulticastScriptDelegate* Delegate = GetMulticastDelegate(Src))
		{
			SaveMulticastDelegate(Dst, *Delegate, Ctx);
		}
		else
		{
			SaveEmptyMulticastDelegate(Dst);
		}
	}
	
	void Load(FSparseDelegate& Dst, FStructLoadView Src) const
	{
		if (Dst.IsBound())
		{
			UObject* Owner = FSparseDelegateStorage::ResolveSparseOwner(Dst, OwningClassName, DelegateName);
			FSparseDelegateStorage::Clear(Owner, DelegateName);
			Dst.*GSparseDelegateIsBound = false;
		}

		FMemberLoader Members(Src);
		if (Members.HasMore())
		{
			UObject* Owner = FSparseDelegateStorage::ResolveSparseOwner(Dst, OwningClassName, DelegateName);
			FMulticastScriptDelegate Tmp;
			LoadMulticastDelegate(Tmp, Members);
			FSparseDelegateStorage::SetMulticastDelegate(Owner, DelegateName, MoveTemp(Tmp));
			Dst.*GSparseDelegateIsBound = true;
		}
	}

	bool Diff(const FSparseDelegate& SparseA, const FSparseDelegate& SparseB) const
	{
		const FMulticastScriptDelegate* A = GetMulticastDelegate(SparseA);
		const FMulticastScriptDelegate* B = GetMulticastDelegate(SparseB);
		if (A && B)
		{
			return DiffMulticastDelegate(*A, *B);
		}
		return !!A != !!B;
	}
};

static FBindId BindSparseDelegate(FBindId Owner, FMulticastSparseDelegateProperty* Property)
{
	// Todo: Ownership / memory leak
	ICustomBinding* Leak = new FMulticastSparseDelegateBinding(CastChecked<USparseDelegateFunction>(Property->SignatureFunction));

	FType MulticastSparseDelegate = GUE.Names.Resolve(GUE.Structs.MulticastSparseDelegate);
	FType OwnerParam = GUE.Names.Resolve(Owner);
	FType PropertyParam = { GUE.Scopes.CoreUObject, FTypenameId(GUE.Names.NameType(Property->GetFName())) };
	FType UniqueBindName = GUE.Names.MakeParametricType(MulticastSparseDelegate, {OwnerParam, PropertyParam});
	FBindId Id = GUE.Names.IndexBindId(UniqueBindName);

	GUE.Customs.BindStruct(Id, *Leak, GUE.Types.Get(GUE.Structs.MulticastDelegate));

	return Id;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FVerseFunctionBinding::Save(FMemberBuilder& Dst, const FVerseFunction& Src, const FVerseFunction* Default, const FSaveContext& Ctx) const
{
	unimplemented();
}

void FVerseFunctionBinding::Load(FVerseFunction& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
{
	unimplemented();
}

bool FVerseFunctionBinding::Diff(const FVerseFunction& A, const FVerseFunction& B, const FBindContext&)
{
	//return !(A == B);
	return false;
}
	
////////////////////////////////////////////////////////////////////////////////////////////////

void FDynamicallyTypedValueBinding::Save(FMemberBuilder& Dst, const Type& Src, const Type* Default, const FSaveContext& Ctx) const
{
	unimplemented();
}

void FDynamicallyTypedValueBinding::Load(Type& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
{
	unimplemented();
}

bool FDynamicallyTypedValueBinding::Diff(const ::UE::FDynamicallyTypedValue& A, const ::UE::FDynamicallyTypedValue& B, const FBindContext&)
{
	//return !UE::Verse::FRuntimeTypeDynamic::Get().AreIdentical(&A, &B);
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FReferencePropertyBinding::Save(FMemberBuilder& Dst, const FReferencePropertyValue& Src, const FReferencePropertyValue* Default, const FSaveContext& Ctx) const
{
	unimplemented();
}

void FReferencePropertyBinding::Load(FReferencePropertyValue& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
{
	unimplemented();
}

bool FReferencePropertyBinding::Diff(const FReferencePropertyValue& A, const FReferencePropertyValue& B, const FBindContext&)
{
	unimplemented();
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////

struct FInterfaceBinding final : ICustomBinding
{
	explicit FInterfaceBinding(UClass* Class) : InterfaceClass(Class) {}

	const TObjectPtr<UClass> InterfaceClass;

	virtual void SaveCustom(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) override
	{
		if (!Default || DiffCustom(Src, Default, Ctx))
		{
			Save(Dst, *static_cast<const FScriptInterface*>(Src), Ctx);
		}
	}

	virtual void LoadCustom(void* Dst, FStructLoadView Src, ECustomLoadMethod Method) const override
	{
		check(Method == ECustomLoadMethod::Assign);
		Load(*static_cast<FScriptInterface*>(Dst), Src);
	}

	virtual bool DiffCustom(const void* A, const void* B, const FBindContext&) const override
	{
		return *static_cast<const FScriptInterface*>(A) != *static_cast<const FScriptInterface*>(B);
	}
	
	void Save(FMemberBuilder& Dst, const FScriptInterface& Src, const FSaveContext& Ctx) const
	{
		const TObjectPtr<UObject>& ObjectRef = const_cast<FScriptInterface&>(Src).GetObjectRef();
		Dst.AddStruct(GUE.Members.Object, GUE.Structs.ObjectPtr, SaveStruct(&ObjectRef, GUE.Structs.ObjectPtr, Ctx));
	}

	void Load(FScriptInterface& Dst, FStructLoadView Src) const
	{
		LoadSoleStruct(&Dst.GetObjectRef(), Src);
		UObject* Object = Dst.GetObject();
		Dst.SetInterface(Object ? Object->GetInterfaceAddress(InterfaceClass) : nullptr);
	}
};

class FInterfaceBindings
{
	const FType ScriptInterface;
	const FStructDeclaration& Declaration;

	TMap<FType, FBindId> BoundClasses;

public:
	FInterfaceBindings()
	: ScriptInterface(GUE.Names.Resolve(GUE.Structs.ScriptInterface))
	, Declaration(GUE.Types.DeclareStruct(GUE.Structs.ScriptInterface, ScriptInterface, 0, {GUE.Members.Object}, EMemberPresence::RequireAll))
	{}

	FBindId Bind(FInterfaceProperty* Property)
	{
		FType Class = IndexType(Property->InterfaceClass);
		if (const FBindId* BindId = BoundClasses.Find(Class))
		{
			return *BindId;
		}
		
		FType UniqueBindName = GUE.Names.MakeParametricType(ScriptInterface, {Class});
		FBindId BindId = GUE.Names.IndexBindId(UniqueBindName);
		BoundClasses.Emplace(Class, BindId);

		// Todo: Ownership / memory leak
		ICustomBinding* Leak = new FInterfaceBinding(Property->InterfaceClass);
		GUE.Customs.BindStruct(BindId, *Leak, Declaration);

		return BindId;
	}
};

static FBindId BindInterface(FInterfaceProperty* Property)
{
	static FInterfaceBindings Bindings;
	return Bindings.Bind(Property);
}

////////////////////////////////////////////////////////////////////////////////////////////////

inline uint32 HashRangeBindings(TConstArrayView<FRangeBinding> In)
{
	return static_cast<uint32>(FXxHash64::HashBuffer(In.GetData(), In.NumBytes()).Hash);
}

inline uint32 HashSkipOffset(FMemberBinding In)
{
	uint32 Out = HashCombineFast(GetTypeHash(In.InnermostSchema), GetTypeHash(In.InnermostType));
	return In.RangeBindings.IsEmpty() ? Out : HashCombineFast(Out, HashRangeBindings(In.RangeBindings));
}

inline bool EqSkipOffset(FMemberBinding A, FMemberBinding B)
{
	return A.InnermostType == B.InnermostType && A.InnermostSchema == B.InnermostSchema && Algo::Compare(A.RangeBindings, B.RangeBindings);
}

// Helper to cache various property bindings instead of a TMap KeyFunc
struct FParameterBinding : FMemberBinding
{
	friend uint32 GetTypeHash(FParameterBinding In) { return HashSkipOffset(In); };
	inline bool operator==(FParameterBinding O) const { return EqSkipOffset(*this, O); }
};

////////////////////////////////////////////////////////////////////////////////////////////////

template<const std::string_view& Suffix>
static bool EndsWithDelimitedSuffix(FName EnumName, FName ValueName)
{
	if (ValueName.GetNumber() != NAME_NO_NUMBER_INTERNAL)
	{
		return false;
	}

	// All type names and enum constants are ASCII
	ANSICHAR Buffer[NAME_SIZE];
	ValueName.GetComparisonNameEntry()->GetAnsiName(Buffer);
	FAnsiStringView Value(Buffer);
	if (Value.Len() >= Suffix.size() + 2 && Value.EndsWith(ToAnsiView(Suffix)))
	{
		// Todo: Check EnumName too, maybe based on ECppForm
		char Delimiter = Value[Value.Len() - Suffix.size() - 1];
		return Delimiter == ':' || Delimiter == '_';	
	}
	return false;
}

static bool DenyMaxValue(FName Enum)
{
	static const FName AllowsMax[] = {
		"ESlateBrushMirrorType", 
		"EFortFeedbackAddressee",
		"ECameraFocusMethod" };
	return !Algo::Find(AllowsMax, Enum);
}

enum class ERoundtrip : uint8
{
	None 		= 0,
	PP 			= 1 << 0,
	TPS 		= 1 << 1,
	UPS 		= 1 << 2,
	TextMemory	= 1 << 3,
	TextStable	= 1 << 4,
};
ENUM_CLASS_FLAGS(ERoundtrip);

static FEnumId DeclareEnum(UEnum* Enum)
{
	FType Type = IndexType(Enum);
	FEnumId Id = GUE.Names.IndexEnum(Type);
	EEnumMode Mode = Enum->HasAnyEnumFlags(EEnumFlags::Flags) ? EEnumMode::Flag : EEnumMode::Flat;

	// Skip _MAX and _All enumerators
	FName EnumName = Enum->GetFName();
	int32 Num = Enum->NumEnums();
	if (Num > 0 && DenyMaxValue(EnumName))
	{
		static constexpr std::string_view Max = "MAX";
		static constexpr std::string_view All = "All";
		Num -= EndsWithDelimitedSuffix<Max>(EnumName, Enum->GetNameByIndex(Num - 1));
		Num -= Num > 0 && Mode == EEnumMode::Flag && 
				EndsWithDelimitedSuffix<All>(EnumName, Enum->GetNameByIndex(Num - 1));
	}

	TArray<FEnumerator, TInlineAllocator<64>> Enumerators;
	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		FName ValueName = Enum->GetNameByIndex(Idx);
		Enumerators.Emplace(GUE.Names.MakeName(ValueName), static_cast<uint64>(Enum->GetValueByIndex(Idx)));
	}

#if WITH_METADATA
	// IsMax() classifies more names as "max" than Enum->ContainsExistingMax()
	checkSlow(Enumerators.Num() == Num || Enum->ContainsExistingMax() || Enum->HasMetaData(TEXT("Hidden"), Num));
#endif

	GUE.Types.DeclareEnum(Id, Type, Mode, Enumerators, EEnumAliases::Strip);
	return Id;
}

////////////////////////////////////////////////////////////////////////////////////////////////

inline uint64 NumBytes(int32 NumItems, SIZE_T ItemSize)
{
	return static_cast<uint64>(NumItems) * ItemSize;
}

inline bool HasConstructor(const FProperty* Property)
{
	return !(Property->PropertyFlags & CPF_ZeroConstructor);
}

inline bool HasDestructor(const FProperty* Property)
{
	return !(Property->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor));
}

inline bool HasHash(const FProperty* Property)
{
	return !!(Property->PropertyFlags & CPF_HasGetValueTypeHash);
}

inline void ConstructValue(const FProperty* Property, void* Value)
{
	((*Property).*GInitPropertyValue)(Value);
}

inline void DestroyValue(const FProperty* Property, void* Value)
{
	((*Property).*GDestroyPropertyValue)(Value);
}

inline uint32 HashValue(const FProperty* Property, const void* Item)
{
	return ((*Property).*GHashPropertyValue)(Item);
}

inline void ConstructValues(const FProperty* Property, uint8* Values, int32 Num, SIZE_T Stride)
{
	for (uint8* It = Values, *End = Values + Num*Stride; It != End; It += Stride)
	{
		((*Property).*GInitPropertyValue)(It);
	}
}

inline void MemzeroStrided(uint8* Values, int32 Num, SIZE_T Size, SIZE_T Stride)
{
	for (uint8* It = Values, *End = Values + Num*Stride; It != End; It += Stride)
	{
		FMemory::Memzero(It, Size);
	}
}

inline void DestroyValues(const FProperty* Property, uint8* Values, int32 Num, SIZE_T Stride)
{
	for (uint8* It = Values, *End = Values + Num*Stride; It != End; It += Stride)
	{
		((*Property).*GDestroyPropertyValue)(It);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

// Helps cache array property range bindings
struct FArrayPropertyInfo
{
	explicit FArrayPropertyInfo(FArrayProperty* Property)
	: bFreezable(!!(Property->ArrayFlags & EArrayPropertyFlags::UsesMemoryImageAllocator))
	, bDestructor(HasDestructor(Property->Inner))
	, bConstructor(HasConstructor(Property->Inner))
	, ItemAlign(Property->Inner->GetMinAlignment())
	, ItemSize(Property->Inner->GetElementSize())
	{}

	union
	{
		struct
		{
			uint32	bFreezable : 1;
			uint32	bDestructor : 1;	
			uint32	bConstructor : 1;
			uint32	ItemAlign : 29;
		};
		uint32		Int;
	};
	uint32			ItemSize;

	bool			IsTrivial() const						{ return !bDestructor && !bConstructor; }
	bool			operator==(FArrayPropertyInfo O) const	{ return Int == O.Int && ItemSize == O.ItemSize; }
	friend uint32	GetTypeHash(FArrayPropertyInfo I)		{ return HashCombineFast(I.Int, I.ItemSize); };
};
static_assert(sizeof(FArrayPropertyInfo) == 8);

// Cacheable FArrayProperty binding
template<class ScriptArray>
struct TTrivialArrayBinding : IItemRangeBinding
{
	const FArrayPropertyInfo Info;

	TTrivialArrayBinding(FArrayPropertyInfo InFo, FConcreteTypenameId BindName = GUE.Typenames.TrivialArray)
	: IItemRangeBinding(BindName)
	, Info(InFo) {}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		const ScriptArray& Array = Ctx.Request.GetRange<ScriptArray>();
		Ctx.Items.SetAll(Array.GetData(), static_cast<uint64>(Array.Num()), Info.ItemSize);
	}

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		ScriptArray& Array = Ctx.Request.GetRange<ScriptArray>();

		int32 NewNum = static_cast<int32>(Ctx.Request.NumTotal());
		Array.SetNumUninitialized(NewNum, Info.ItemSize, Info.ItemAlign);
		if (NewNum)
		{
			FMemory::Memzero(Array.GetData(), NumBytes(NewNum, Info.ItemSize));
		}
		
		Ctx.Items.Set(Array.GetData(), static_cast<uint64>(NewNum), Info.ItemSize);
	}
};

// Currently can't extract constructor/destructor function pointers from FProperty, which
// requires keeping FProperty* and prevents range binding reuse, @see AllocateArrayBinding()
template<class ScriptArray>
struct TNonTrivialArrayBinding : TTrivialArrayBinding<ScriptArray>
{
	const FProperty* Inner;

	using TTrivialArrayBinding<ScriptArray>::Info;

	TNonTrivialArrayBinding(FArrayPropertyInfo InFo, const FProperty* InNer)
	: TTrivialArrayBinding<ScriptArray>(InFo, GUE.Typenames.NonTrivialArray)
	, Inner(InNer)
	{}

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		ScriptArray& Array = Ctx.Request.GetRange<ScriptArray>();

		int32 NumDestroy = Info.bDestructor * Array.Num();
		DestroyValues(Inner, static_cast<uint8*>(Array.GetData()), NumDestroy, Info.ItemSize);

		uint64 NewNum = Ctx.Request.NumTotal();
		Array.SetNumUninitialized(static_cast<int32>(NewNum), Info.ItemSize, Info.ItemAlign);
		InitItems(NewNum, Array.GetData());
		
		Ctx.Items.Set(Array.GetData(), NewNum, Info.ItemSize);
	}

	inline void InitItems(uint64 Num, void* Items) const
	{
		if (Info.bConstructor)
		{
			ConstructValues(Inner, static_cast<uint8*>(Items), Num, Info.ItemSize);
		}
		else if (Num)
		{
			FMemory::Memzero(Items, Num * Info.ItemSize);
		}
	}
};

template<ELeafType Type, ELeafWidth Width>
struct TLeafArrayBinding : ILeafRangeBinding
{
	inline static constexpr SIZE_T LeafSize = SizeOf(Width);
	TLeafArrayBinding() : ILeafRangeBinding(GUE.Typenames.LeafArray) {}

	virtual void SaveLeaves(const void* Range, FLeafRangeAllocator& Out) const override
	{
		const FScriptArray& Array = *static_cast<const FScriptArray*>(Range);
		if (int32 Num = Array.Num())
		{
			void* Dst = Out.AllocateNonEmptyRange(Num, Width);
			FMemory::Memcpy(Dst, Array.GetData(), NumBytes(Num));
		}
	}

	virtual void LoadLeaves(void* Range, FLeafRangeLoadView Leaves) const override
	{
		FScriptArray& Array = *static_cast<FScriptArray*>(Range);
		Array.SetNumUninitialized(static_cast<int32>(Leaves.Num()), LeafSize, LeafSize);
		Leaves.AsBitCast<Type, Width>().Copy(Array.GetData(), NumBytes(Array.Num()));
	}

	virtual bool DiffLeaves(const void* RangeA, const void* RangeB) const override
	{
		const FScriptArray& A = *static_cast<const FScriptArray*>(RangeA);
		const FScriptArray& B = *static_cast<const FScriptArray*>(RangeB);
		return Diff(A.Num(), B.Num(), A.GetData(), B.GetData(), LeafSize);
	}

	inline constexpr uint64 NumBytes(int32 NumItems) const
	{
		return static_cast<uint64>(NumItems) * LeafSize;
	}
};

// Reusable cache of FArrayProperty range bindings
class FArrayPropertyBindings
{
	static constexpr ERangeSizeType SizeType = DefaultRangeMax;
	
	const TLeafArrayBinding<ELeafType::Bool, ELeafWidth::B8>	Bool;
	const TLeafArrayBinding<ELeafType::Float, ELeafWidth::B32>	Float;
	const TLeafArrayBinding<ELeafType::Float, ELeafWidth::B64>	Double;
	const TLeafArrayBinding<ELeafType::IntS, ELeafWidth::B8>	IntS8;
	const TLeafArrayBinding<ELeafType::IntS, ELeafWidth::B16>	IntS16;
	const TLeafArrayBinding<ELeafType::IntS, ELeafWidth::B32>	IntS32;
	const TLeafArrayBinding<ELeafType::IntS, ELeafWidth::B64>	IntS64;
	const TLeafArrayBinding<ELeafType::IntU, ELeafWidth::B8>	IntU8;
	const TLeafArrayBinding<ELeafType::IntU, ELeafWidth::B16>	IntU16;
	const TLeafArrayBinding<ELeafType::IntU, ELeafWidth::B32>	IntU32;
	const TLeafArrayBinding<ELeafType::IntU, ELeafWidth::B64>	IntU64;
	const FRangeBinding											Integers[2][4];

	TMap<FArrayPropertyInfo, IItemRangeBinding*>				Others;

	inline const ILeafRangeBinding& DownCast(const ILeafRangeBinding& In) { return In; }

public:
	FArrayPropertyBindings()
	: Integers{{FRangeBinding(IntU8, SizeType),	FRangeBinding(IntU16, SizeType), FRangeBinding(IntU32, SizeType), FRangeBinding(IntU64, SizeType)},
			   {FRangeBinding(IntS8, SizeType),	FRangeBinding(IntS16, SizeType), FRangeBinding(IntS32, SizeType), FRangeBinding(IntS64, SizeType)}}
	{}

	~FArrayPropertyBindings()
	{
		for (TPair<FArrayPropertyInfo, IItemRangeBinding*>& Cached : Others)
		{
			FMemory::Free(Cached.Value);
		}
	}

	FRangeBinding RangeBind(FArrayPropertyInfo Info, uint64 InnerCastFlags)
	{
		if (HasAny<LeafMask>(InnerCastFlags) && !Info.bFreezable)
		{
			uint32 SizeIdx = FMath::FloorLog2NonZero(Info.ItemSize);
			check(SizeIdx < 4);

			// Note that we throw away enum schema, only size not needed to load/save enums
			if (HasAny<IntSMask | IntUMask | CASTCLASS_FEnumProperty>(InnerCastFlags))
			{
				return Integers[HasAny<IntSMask>(InnerCastFlags)][SizeIdx];
			}
			
			check(HasAny<CASTCLASS_FFloatProperty | CASTCLASS_FDoubleProperty | CASTCLASS_FBoolProperty >(InnerCastFlags));
			const ILeafRangeBinding& Binding = HasAny<CASTCLASS_FBoolProperty>(InnerCastFlags) 
				? DownCast(Bool) : (HasAny<CASTCLASS_FFloatProperty>(InnerCastFlags) ? DownCast(Float) : Double);
			return FRangeBinding(Binding, SizeType);
		}
		else if (IItemRangeBinding** Cached = Others.Find(Info))
		{
			return FRangeBinding(**Cached, SizeType);
		}
		
		IItemRangeBinding* New = Info.bFreezable	? CreateAndCache<FFreezableScriptArray>(Info)
													: CreateAndCache<FScriptArray>(Info);
		return FRangeBinding(*New, SizeType);
	}

	template<typename ScriptArray>
	IItemRangeBinding* CreateAndCache(FArrayPropertyInfo Info)
	{
		static_assert(std::is_trivially_destructible_v<TTrivialArrayBinding<ScriptArray>>);
		return Others.Emplace(Info, new TTrivialArrayBinding<ScriptArray>(Info));
	}
};
	
static FArrayPropertyBindings GCachedArrayBindings;

template<typename ScriptArray>
IItemRangeBinding* CreateAndLeak(FArrayPropertyInfo Info, FProperty* Inner)
{
	return new TNonTrivialArrayBinding<ScriptArray>(Info, Inner);
}

static FRangeBinding AllocateArrayBinding(FArrayProperty* Property)
{
	FProperty* Inner = Property->Inner;
	FArrayPropertyInfo Info(Property);
	if (Info.IsTrivial())
	{
		return GCachedArrayBindings.RangeBind(Info, Inner->GetCastFlags());
	}

	// Todo: Ownership / memory leak, try make non-trivial case cacheable by making FProperty ctor/dtor extractable
	IItemRangeBinding* Out = Info.bFreezable	? CreateAndLeak<FFreezableScriptArray>(Info, Inner)
												: CreateAndLeak<FScriptArray>(Info, Inner);
	return FRangeBinding(*Out, ERangeSizeType::S32);
}

////////////////////////////////////////////////////////////////////////////////////////////////

// Helpers to avoid using leaf FProperty instances after binding
//
// Below must match FFloatProperty, FDoubleProperty, FBoolProperty, FEnumProperty, TNumericProperty
// Identical() and GetValueTypeHashInternal() implementations perfectly except not supporting nullptrs
inline uint32	LeafPropertyHash(float In)											{ return ::UE::PreciseFPHash(In); }
inline uint32	LeafPropertyHash(double In)											{ return ::UE::PreciseFPHash(In); }
inline bool		LeafPropertyIdentical(float A, float B)								{ return ::UE::PreciseFPEqual(A, B); }
inline bool		LeafPropertyIdentical(double A, double B)							{ return ::UE::PreciseFPEqual(A, B); }
template<typename T>
inline uint32	LeafPropertyHash(T In) requires (std::is_unsigned_v<T>)				{ return GetTypeHash(In); }
template<typename T>
inline bool		LeafPropertyIdentical(T A, T B) requires (std::is_unsigned_v<T>)	{ return A == B; }

// Type-erased just enough to call LeafPropertyHash / LeafPropertyIdentical and FLeafRangeLoadView::As/AsBitcast
enum class EPropertyKind : uint8 { Range, Struct, Bool, U8, U16, U32, U64, F32, F64 };

template<EPropertyKind Kind>
struct TEquivalentLeafType									{ using Type = void; };
template<> struct TEquivalentLeafType<EPropertyKind::Bool>	{ using Type = bool; };
template<> struct TEquivalentLeafType<EPropertyKind::U8>	{ using Type = uint8; };
template<> struct TEquivalentLeafType<EPropertyKind::U16>	{ using Type = uint16; };
template<> struct TEquivalentLeafType<EPropertyKind::U32>	{ using Type = uint32; };
template<> struct TEquivalentLeafType<EPropertyKind::U64>	{ using Type = uint64; };
template<> struct TEquivalentLeafType<EPropertyKind::F32>	{ using Type = float; };
template<> struct TEquivalentLeafType<EPropertyKind::F64>	{ using Type = double; };

template<EPropertyKind Kind>
using EquivalentLeafType = typename TEquivalentLeafType<Kind>::Type;

template<typename LeafType>
uint32 LeafHash(const void* In)
{
	return LeafPropertyHash(*static_cast<const LeafType*>(In));
}

template<typename LeafType>
bool LeafIdentical(const void* A, const void* B)
{
	return LeafPropertyIdentical(*static_cast<const LeafType*>(A), *static_cast<const LeafType*>(B));
}

template<typename LeafType>
inline auto CastAs(FLeafRangeLoadView In)
{
	if constexpr (std::is_floating_point_v<LeafType>)
	{
		return In.As<LeafType>();
	}
	else
	{
		return In.AsBitCast<LeafType>();
	}
}

inline EPropertyKind GetPropertyKind(FLeafBindType In)
{
	ELeafType Type = ToLeafType(In.Bind.Type);
	ELeafWidth Width = In.Basic.Width;

	if (Type == ELeafType::Float)
	{
		return Width == ELeafWidth::B32 ? EPropertyKind::F32 : EPropertyKind::F64;
	}
	else if (Type == ELeafType::Bool)
	{
		return EPropertyKind::Bool;
	}
	else switch (Width)
	{
		case ELeafWidth::B8:	return EPropertyKind::U8;
		case ELeafWidth::B16:	return EPropertyKind::U16;
		case ELeafWidth::B32:	return EPropertyKind::U32;
		default:				return EPropertyKind::U64;
	}								
}

static EPropertyKind GetPropertyKind(FMemberBinding In)
{
	return (In.RangeBindings.Num() > 0) ? EPropertyKind::Range
										: In.InnermostType.IsStruct()
										? EPropertyKind::Struct
										: GetPropertyKind(In.InnermostType.AsLeaf());
}

////////////////////////////////////////////////////////////////////////////////////////////////

// Inner leaf property, e.g. FEnumProperty, FNumericProperty
template<Arithmetic EquivalentType>
struct TInnerLeafProperty
{
	static constexpr EMemberKind	Kind = EMemberKind::Leaf;
	static constexpr bool			bConstruct = false;
	static constexpr bool			bDestruct = false;
	static constexpr bool			bHashable = true;
	static constexpr int32			Size = sizeof(EquivalentType);

	TInnerLeafProperty(FProperty* In) { check(Size == In->GetElementSize());}

	inline static void				InitItem(void* In)						{} // Note doesn't zero out items about to be overwritten
	inline static void				DestroyItem(void* In)					{}
	inline static EquivalentType	Cast(const void* In)					{ return *static_cast<const EquivalentType*>(In); } // Todo: Consider if BitCast is needed
	inline static uint32			Hash(const void* In)					{ return LeafPropertyHash(Cast(In)); }
	inline static bool				Identical(const void* A, const void* B) { return LeafPropertyIdentical(Cast(A), Cast(B)); }
};

// Inner range-bound property, e.g. FArrayProperty, FStringProperty, FSetProperty
struct FInnerRangeProperty
{
	static constexpr EMemberKind	Kind = EMemberKind::Range;
	static constexpr bool			bConstruct = false;
	static constexpr bool			bDestruct = true;

	FProperty*						Property;
	uint32							Size;
	bool							bHashable;

	FInnerRangeProperty(FProperty* In)
	: Property(In)
	, Size(In->GetElementSize())
	, bHashable(HasHash(In))
	{
		check(!HasConstructor(In));
		check(HasDestructor(In));
	}

	inline void						InitItem(void* In) const				{ FMemory::Memzero(In, Size); }
	inline void						DestroyItem(void* In) const				{ DestroyValue(Property, In); }
};

// Inner struct-bound property, e.g. FStructProperty, FNameProperty, FObjectProperty
struct FInnerStructProperty
{
	static constexpr EMemberKind	Kind = EMemberKind::Struct;

	FProperty*						Property;
	uint32							Size;
	bool							bConstruct;
	bool							bDestruct;
	bool							bHashable;

	FInnerStructProperty(FProperty* In)
	: Property(In)
	, Size(In->GetElementSize())
	, bConstruct(HasConstructor(In))
	, bDestruct(HasDestructor(In))
	, bHashable(HasHash(In))
	{}

	inline void						InitItem(void* Item) const
	{ 
		if (bConstruct)
		{
			ConstructValue(Property, Item);
		}
		else
		{
			FMemory::Memzero(Item, Size);
		}
	}

	inline void						DestroyItem(void* Item) const
	{
		if (bDestruct)
		{
			DestroyValue(Property, Item);
		}
	}
};

template<EPropertyKind Kind> struct TSelectInnerProperty		{ using Type = TInnerLeafProperty<EquivalentLeafType<Kind>>; };
template<> struct TSelectInnerProperty<EPropertyKind::Range>	{ using Type = FInnerRangeProperty; };
template<> struct TSelectInnerProperty<EPropertyKind::Struct>	{ using Type = FInnerStructProperty; };

template<EPropertyKind Kind>
using TInnerProperty = typename TSelectInnerProperty<Kind>::Type;

template<typename InnerPropertyType>
inline auto	MakeHashFn(const InnerPropertyType& Inner)
{
	if constexpr (InnerPropertyType::Kind == EMemberKind::Leaf)
	{
		return &InnerPropertyType::Hash;
	}
	else
	{
		return [P = Inner.Property](const void* In) { return HashValue(P, In); };
	}
}

template<typename InnerPropertyType>
inline auto	MakeIdenticalFn(const InnerPropertyType& Inner)
{
	if constexpr (InnerPropertyType::Kind == EMemberKind::Leaf)
	{
		return &InnerPropertyType::Identical;
	}
	else
	{
		return [P = Inner.Property](const void* A, const void* B) { return P->Identical(A, B); };
	}
}

template<typename InnerPropertyType>
inline void InitStridedItems(const InnerPropertyType& Inner, void* Items, uint64 Num, SIZE_T Stride)
{
	if constexpr (InnerPropertyType::Kind == EMemberKind::Leaf)
	{}
	else if (Inner.bConstruct)
	{
		ConstructValues(Inner.Property, static_cast<uint8*>(Items), Num, Stride);
	}
	else
	{
		MemzeroStrided(static_cast<uint8*>(Items), Num, Inner.Size, Stride);
	}
}

template<typename InnerPropertyType>
inline void DestroyStridedItems(const InnerPropertyType& Inner, void* Items, uint64 Num, SIZE_T Stride)
{
	if constexpr (InnerPropertyType::Kind == EMemberKind::Leaf)
	{}
	else if (Inner.bDestruct)
	{
		DestroyValues(Inner.Property, static_cast<uint8*>(Items), Num, Stride);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

template<typename EquivalentType>
struct TLeafRangeSerializer
{
	using RangeSaver = TLeafRangeSaver<EquivalentType>;
	static constexpr SIZE_T	Size = sizeof(EquivalentType);

	FMemberType				InnerType;
	FOptionalInnerId		EnumId;

	explicit TLeafRangeSerializer(FMemberBinding In)
	: InnerType(ToLeafType(In.InnermostType.AsLeaf()))
	, EnumId(In.InnermostSchema)
	{
		check(In.RangeBindings.IsEmpty());
		check(InnerType.AsLeaf().Width == WidthOf(Size));
	}

	inline static EquivalentType Cast(const void* In)
	{
		return *static_cast<const EquivalentType*>(In);
	}

	inline FMemberSchema MakeMemberSchema() const
	{
		return { FMemberType(DefaultRangeMax), InnerType, 1, EnumId, nullptr };
	}

	inline static EquivalentType SaveItem(const void* In, const FSaveContext&)
	{
		return Cast(In);
	}

	inline static void LoadItem(void* Dst, FByteReader& SrcBytes, FBitCacheReader& SrcBits, FOptionalSchemaId, const FLoadBatch&) requires (std::is_same_v<EquivalentType, bool>)
	{
		*static_cast<bool*>(Dst) = SrcBits.GrabNext(SrcBytes);
	}

	inline static void LoadItem(void* Dst, FByteReader& SrcBytes, FBitCacheReader&, FOptionalSchemaId, const FLoadBatch&)
	{
		*static_cast<EquivalentType*>(Dst) = Cast(SrcBytes.GrabBytes(Size));
	}
};

struct FStructRangeSerializer
{
	using RangeSaver = FStructRangeSaver;
	
	FMemberType				InnerType;
	FBindId					SaveId;

	explicit FStructRangeSerializer(FMemberBinding Item)
	: InnerType(Item.InnermostType.AsStruct())
	, SaveId(Item.InnermostSchema.Get().AsStruct())
	{
		check(Item.RangeBindings.IsEmpty());
	}

	inline FMemberSchema MakeMemberSchema() const
	{
		return { FMemberType(DefaultRangeMax), InnerType, 1, FInnerId(SaveId), nullptr };
	}

	inline FBuiltStruct* SaveItem(const void* In, const FSaveContext& Ctx) const
	{
		return SaveStruct(In, SaveId, Ctx);
	}

	void LoadItem(void* Dst, FByteReader& SrcBytes, FBitCacheReader&, FOptionalSchemaId LoadId, const FLoadBatch& Batch) const
	{
		LoadStruct(Dst, FByteReader(SrcBytes.GrabSkippableSlice()), static_cast<FStructSchemaId>(LoadId.Get()), Batch);
	}
};

struct FNestedRangeSerializer
{
	using RangeSaver = FNestedRangeSaver;

	FOptionalInnerId								InnermostSaveId;
	uint16											NumInners;
	TArray<FMemberType, TInlineAllocator<8>>		InnerTypes;
	TArray<FMemberBindType, TInlineAllocator<8>>	InnerBindTypes;
	TArray<FRangeBinding, TInlineAllocator<2>>		InnerBindings;

	explicit FNestedRangeSerializer(FMemberBinding Item)
	: InnermostSaveId(Item.InnermostSchema)
	, NumInners(IntCastChecked<uint16>(1 + Item.RangeBindings.Num()))
	, InnerBindings(Item.RangeBindings)
	{
		check(NumInners >= 2);
		for (FRangeBinding Inner : Item.RangeBindings)
		{
			InnerTypes.Emplace(Inner.GetSizeType());
			InnerBindTypes.Emplace(Inner.GetSizeType());
		}
		InnerTypes.Emplace(Item.InnermostType.IsStruct() ? FMemberType(Item.InnermostType.AsStruct()) 
														 : FMemberType(ToLeafType(Item.InnermostType.AsLeaf())));
		InnerBindTypes.Emplace(Item.InnermostType);
	}

	inline FMemberSchema MakeMemberSchema() const
	{
		return { FMemberType(DefaultRangeMax), InnerTypes[0], NumInners, InnermostSaveId, InnerTypes.GetData() };
	}

	FBuiltRange* SaveItem(const void* In, const FSaveContext& Ctx) const
	{
		FRangeMemberBinding Member = { InnerBindTypes.GetData() + 1, InnerBindings.GetData(), NumInners - 1, InnermostSaveId, 0 };
		return SaveRange(In, Member, Ctx);
	}

	inline void LoadItem(void* Dst, FByteReader& SrcBytes, FBitCacheReader& SrcBits, FOptionalSchemaId InnermostLoadId, const FLoadBatch& Batch) const
	{
		FRangeLoadSchema Schema = { InnerTypes[1], InnermostLoadId, InnerTypes.GetData() + 2, Batch};
		LoadRange(Dst, SrcBytes, SrcBits, DefaultRangeMax, Schema, InnerBindings);
	}
};

template<EPropertyKind Kind> struct TSelectRangeSerializer		{ using Type = TLeafRangeSerializer<EquivalentLeafType<Kind>>; };
template<> struct TSelectRangeSerializer<EPropertyKind::Range>	{ using Type = FNestedRangeSerializer; };
template<> struct TSelectRangeSerializer<EPropertyKind::Struct>	{ using Type = FStructRangeSerializer; };

template<EPropertyKind Kind>
using TPropertyRangeSerializer = typename TSelectRangeSerializer<Kind>::Type;

////////////////////////////////////////////////////////////////////////////////////////////////

inline FScriptSparseArray& AsSparseArray(FScriptSet& In) { return reinterpret_cast<FScriptSparseArray&>(In); }
inline FScriptSparseArray& AsSparseArray(FScriptMap& In) { return reinterpret_cast<FScriptSparseArray&>(In); }

template<typename ScriptSet>
inline bool IsCompact(const ScriptSet& Set)
{
	return Set.NumUnchecked() == Set.GetMaxIndex();
}

// There's no TScriptSparseArray::SetNumUninitialized() (yet), 
// reserve using Empty() and add items one by one instead
template<class ScriptType, class LayoutType>
uint8* SetNumUninitialized(ScriptType& Dst, const LayoutType& Layout, uint64 Num)
{
	check(Dst.IsEmpty());
	Dst.Empty(static_cast<int32>(Num), Layout);
	for (uint64 Idx = 0; Idx < Num; ++Idx)
	{
		Dst.AddUninitialized(Layout);
	}
	check(IsCompact(Dst));

	return static_cast<uint8*>(Dst.GetData(0, Layout));
}

// @pre Elems.Num() > 0
inline FExistingItemSlice GetContiguousSlice(int32 Idx, const FScriptSparseArray& Elems, const uint8* Data, SIZE_T Stride)
{
	checkSlow(!Elems.IsEmpty());
	int32 Num = 1;
	for (;!Elems.IsValidIndex(Idx); ++Idx) { checkSlow(Idx < Elems.GetMaxIndex()); }
	for (; Elems.IsValidIndex(Idx + Num); ++Num) {}
	return { Data + NumBytes(Idx, Stride), static_cast<uint64>(Num) };
}

// Save flat TSet/TMap
inline void ReadSparseItems(FExistingItems& Dst, const FScriptSparseArray& Src, const FScriptSparseArrayLayout& Layout)
{
	const uint8* Data = static_cast<const uint8*>(Src.GetData(0, Layout));

	if (Src.IsEmpty())
	{
		Dst.SetAll(nullptr, 0, Layout.Size);
	}
	else if (FExistingItemSlice LastRead = Dst.Slice)
	{
		// Continue partial response
		int64 PriorBytesRead = static_cast<const uint8*>(LastRead.Data) - Data;
		check(PriorBytesRead % Layout.Size == 0);
		int32 LastIdx = PriorBytesRead / Layout.Size;
		int32 NextIdx = LastIdx + LastRead.Num + /* skip one known invalid */ 1;
		check(NextIdx < Src.GetMaxIndex());
		Dst.Slice = GetContiguousSlice(NextIdx, Src, Data, Layout.Size);
	}
	else if (Src.IsCompact())
	{
		Dst.SetAll(Data, static_cast<uint64>(Src.Num()), Layout.Size);
	}
	else
	{
		// Start partial response
		Dst.NumTotal = Src.Num();
		Dst.Stride = Layout.Size;
		Dst.Slice = GetContiguousSlice(0, Src, Data, Layout.Size);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

// Helps save TSet/TMap deltas
template<class ScriptType, class LayoutType>
struct TSubSetIterator
{
	const LayoutType			Layout;
	const ScriptType&			Set;
	const TBitArray<>&			Subset;
	const int32					Max;
	int32						Idx;

	TSubSetIterator(const LayoutType& InLayout, const ScriptType& InSet, const TBitArray<>& InSubset)
	: Layout(InLayout)
	, Set(InSet)
	, Subset(InSubset)
	, Max(InSet.GetMaxIndex())
	, Idx(Max > 0 ? Subset.Find(true) : INDEX_NONE)
	{}

	explicit operator bool() const	{ return Idx != INDEX_NONE; }
	const void* operator*() const	{ return Set.GetData(Idx, Layout); }
	void operator++()				{ Idx = ++Idx < Max ? Subset.FindFrom(true, Idx) : INDEX_NONE; }
	uint32 CountNum() const			{ return Subset.CountSetBits(); }
};

// Helps save TSet/TMap deltas
template<class ScriptType, class LayoutType, class SerializerType>
FTypedRange SaveAll(const ScriptType& Set, const LayoutType& Layout, const SerializerType& Serializer, const FSaveContext& Ctx)
{
	if (Set.IsEmpty())
	{
		return { Serializer.MakeMemberSchema(), nullptr };
	}

	typename SerializerType::RangeSaver Range(Ctx.Scratch, static_cast<uint64>(Set.Num()));
	for (int32 Idx = 0, Max = Set.GetMaxIndex(); Idx < Max; ++Idx)
	{
		if (Set.IsValidIndex(Idx))
		{
			Range.AddItem(Serializer.SaveItem(Set.GetData(Idx, Layout), Ctx));
		}
	}
	return Range.Finalize(Serializer.MakeMemberSchema());
}

// Helps save TSet/TMap deltas
template<class SubSetIteratorType, class SerializerType>
FTypedRange SaveSome(SubSetIteratorType& It, const SerializerType& Serializer, const FSaveContext& Ctx)
{
	typename SerializerType::RangeSaver Range(Ctx.Scratch, It.CountNum());
	for (; It; ++It)
	{
		Range.AddItem(Serializer.SaveItem(*It, Ctx));
	}
	return Range.Finalize(Serializer.MakeMemberSchema());
}

template<typename BindingType, typename ScriptType>
void SaveSetDelta(const BindingType& Binding, FMemberBuilder& Dst, const ScriptType& Src, const ScriptType* Default, const FSaveContext& Ctx)
{
	if (!Default)
	{
		Dst.AddRange(GUE.Members.Assign, SaveAll(Src, Binding.Layout, Binding.GetItemRange(), Ctx));
	}
	else if (Default->IsEmpty())
	{
		if (!Src.IsEmpty())
		{
			Dst.AddRange(GUE.Members.Insert, SaveAll(Src, Binding.Layout, Binding.GetItemRange(), Ctx));
		}
	}
	else if (Src.IsEmpty())
	{
		Dst.AddRange(GUE.Members.Remove, SaveAll(*Default, Binding.Layout, Binding.GetKeyRange(), Ctx));
	}
	else // Neither are empty
	{
		TBitArray<> RemoveIds(false, Default->GetMaxIndex());
		for (int32 Idx = 0, Max = Default->GetMaxIndex(); Idx < Max; ++Idx)
		{
			RemoveIds[Idx] = Default->IsValidIndex(Idx) && !Binding.HasKey(Src, Default->GetData(Idx, Binding.Layout));
		}
		if (typename BindingType::SubSetIterator Removed{Binding.Layout, *Default, RemoveIds})
		{
			Dst.AddRange(GUE.Members.Remove, SaveSome(Removed, Binding.GetKeyRange(), Ctx));
		}
			
		TBitArray<> InsertIds(false, Src.GetMaxIndex());
		for (int32 Idx = 0, Max = Src.GetMaxIndex(); Idx < Max; ++Idx)
		{
			InsertIds[Idx] = Src.IsValidIndex(Idx) && !Binding.HasItem(*Default, Src.GetData(Idx, Binding.Layout));
		}
		if (typename BindingType::SubSetIterator Inserted{Binding.Layout, Src, InsertIds})
		{
			Dst.AddRange(GUE.Members.Insert, SaveSome(Inserted, Binding.GetItemRange(), Ctx));
		}
	}
}

template<typename BindingType, typename ScriptType>
void InsertSetItems(const BindingType& Binding, ScriptType& Dst, FRangeLoadView Items)
{
	// Insert
	if (Dst.IsEmpty())
	{
		Binding.AssignEmpty(Dst, Items);
	}
	else
	{
		Binding.InsertNonEmpty(Dst, Items);
	}
}

template<typename BindingType, typename ScriptType>
void LoadSetDelta(const BindingType& Binding, ScriptType& Dst, FStructLoadView Src)
{
	FMemberLoader Members(Src);
	FOptionalMemberId Name = Members.PeekName();
	FRangeLoadView Range = Members.GrabRange();
	if (Name == GUE.Members.Insert)
	{
		InsertSetItems(Binding, Dst, Range);
	} 
	else if (Name == GUE.Members.Assign)
	{
		Binding.DestroyAll(Dst);
		Binding.AssignEmpty(Dst, Range);
	}
	else
	{
		checkSlow(Name == GUE.Members.Remove);
		Binding.Remove(Dst, Range);
		if (Members.HasMore())
		{
			checkSlow(Members.PeekNameUnchecked() == GUE.Members.Insert);
			InsertSetItems(Binding, Dst, Members.GrabRange());
		}
	}

	checkSlow(!Members.HasMore());
}

template<typename BindingType, typename ScriptType>
inline bool DiffSet(const BindingType& Binding, const ScriptType& A, const ScriptType& B)
{
	if (A.NumUnchecked() != B.NumUnchecked())
	{
		return true;
	}

	if (A.NumUnchecked() > 0)
	{
		for (int32 IdxA = 0, MaxA = A.GetMaxIndex(); IdxA < MaxA; ++IdxA)
		{
			if (A.IsValidIndex(IdxA) && !Binding.HasItem(B, A.GetData(IdxA, Binding.Layout)))
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////

template<EPropertyKind ElemKind>
struct TSetPropertyBinding : IItemRangeBinding, ICustomBinding
{
	using SetType = FScriptSet;
	using LeafType = EquivalentLeafType<ElemKind>;
	using SubSetIterator = TSubSetIterator<FScriptSet, FScriptSetLayout>;
	static constexpr bool bLeaves = !std::is_void_v<LeafType>;

	const FScriptSetLayout							Layout;
	const TInnerProperty<ElemKind>					Inner;
	const TPropertyRangeSerializer<ElemKind>		Range;

	const TPropertyRangeSerializer<ElemKind>& GetKeyRange() const { return Range; }
	const TPropertyRangeSerializer<ElemKind>& GetItemRange() const { return Range; }

	TSetPropertyBinding(FSetProperty* In, FMemberBinding Elem)
	: IItemRangeBinding(GUE.Typenames.Set)
	, Layout(In->SetLayout)
	, Inner(In->ElementProp)
	, Range(Elem)
	{
		check(Layout.Size == GetStride());
		check(Inner.bHashable);
	}

	inline SIZE_T GetStride() const requires (bLeaves)	{ return sizeof(TSetElement<LeafType>); }
	inline SIZE_T GetStride() const						{ return Layout.Size; }

	inline int32 FindIndex(const FScriptSet& Set, const void* Elem) const
	{
		return Set.FindIndex(Elem, Layout, MakeHashFn(Inner), MakeIdenticalFn(Inner));
	}

	inline void RemoveElem(FScriptSet& Set, const void* Elem) const
	{
		if (int32 Idx = FindIndex(Set, Elem); Idx != INDEX_NONE)
		{
			DestroyElem(Set, Idx);
			Set.RemoveAt(Idx, Layout);
		}
	}
	
	inline bool HasItem(const FScriptSet& Set, const void* Elem) const
	{
		return FindIndex(Set, Elem) != INDEX_NONE;
	}

	inline bool HasKey(const FScriptSet& Set, const void* Elem) const
	{
		return HasItem(Set, Elem);
	}

	inline void Rehash(FScriptSet& Set) const
	{
		Set.Rehash(Layout, MakeHashFn(Inner));
	}

	inline void DestroyElem(FScriptSet& Set) const requires (bLeaves) {}
	inline void DestroyElem(FScriptSet& Set, int32 Idx) const
	{
		Inner.DestroyItem(Set.GetData(Idx, Layout));
	}
	
	inline void DestroyAll(FScriptSet& Set) const requires (bLeaves) {}
	inline void DestroyAll(FScriptSet& Set) const
	{
		uint8* It = static_cast<uint8*>(Set.GetData(0, Layout));
		SIZE_T Stride = GetStride();
		if (IsCompact(Set))
		{
			DestroyStridedItems(Inner, It, Set.NumUnchecked(), Stride);
		}
		else
		{
			for (int32 Idx = 0, Max = Set.GetMaxIndex(); Idx < Max; ++Idx)
			{
				if (Set.IsValidIndex(Idx))
				{
					DestroyValue(Inner.Property, It);
				}
				It += Stride;
			}
		}
	}
	
	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		ReadSparseItems(/* out */ Ctx.Items, Ctx.Request.GetRange<FScriptSparseArray>(), Layout.SparseArrayLayout);
	}

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		FScriptSet& Set = Ctx.Request.GetRange<FScriptSet>();
		int32 NewNum = static_cast<int32>(Ctx.Request.NumTotal());

		if (Ctx.Request.IsFirstCall())
		{
			DestroyAll(Set);
			Set.Empty(NewNum, Layout);
			if (NewNum)
			{
				uint8* Items = SetNumUninitialized(Set, Layout, NewNum);
				InitStridedItems(Inner, Items, NewNum, GetStride());
				Ctx.Items.Set(Items, Ctx.Request.NumTotal(), GetStride());
				Ctx.Items.RequestFinalCall();
			}
		}
		else
		{
			check(Ctx.Request.IsFinalCall());
			Rehash(Set);
		}
	}

	virtual void SaveCustom(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) override
	{
		SaveSetDelta(*this, Dst, *static_cast<const FScriptSet*>(Src), static_cast<const FScriptSet*>(Default), Ctx);
	}

	virtual void LoadCustom(void* Dst, FStructLoadView Src, ECustomLoadMethod Method) const override
	{
		check(Method == ECustomLoadMethod::Assign);
		LoadSetDelta(*this, *static_cast<FScriptSet*>(Dst), Src);
	}

	virtual bool DiffCustom(const void* A, const void* B, const FBindContext&) const override
	{
		return DiffSet(*this, *static_cast<const FScriptSet*>(A), *static_cast<const FScriptSet*>(B));
	}

	// Load into empty set
	inline void AssignEmpty(FScriptSet& Dst, FRangeLoadView Src) const
	{
		SIZE_T Stride = GetStride();
		uint8* It = SetNumUninitialized(Dst, Layout, Src.Num());
		InitStridedItems(Inner, It, Src.Num(), Stride);

		if constexpr (bLeaves)
		{
			for (LeafType Item : CastAs<LeafType>(Src.AsLeaves()))
			{
				*reinterpret_cast<LeafType*>(It) = Item;
				It += Stride;
			}
		}
		else if constexpr (ElemKind == EPropertyKind::Range)
		{
			TConstArrayView<FRangeBinding> InnerBindings = Range.InnerBindings;
			for (FRangeLoadView Item : Src.AsRanges())
			{
				LoadRange(It, Item, InnerBindings);
				It += Stride;
			}
		}
		else
		{
			for (FStructLoadView Item : Src.AsStructs())
			{
				LoadStruct(It, Item);
				It += Stride;
			}
		}

		Rehash(Dst);
	}

	// Load leaves into non-empty set
	inline void InsertNonEmpty(FScriptSet& Dst, FRangeLoadView Src) const requires(bLeaves)
	{	
		for (LeafType Item : CastAs<LeafType>(Src.AsLeaves()))
		{
			if (!HasItem(Dst, &Item))
			{
				 void* Elem = Dst.GetData(Dst.AddUninitialized(Layout), Layout);
				 *static_cast<LeafType*>(Elem) = Item;
			}
		}

		Rehash(Dst);
	}

	inline void* AddItem(FScriptSparseArray& Dst, int32& OutIdx) const
	{
		OutIdx = Dst.AddUninitialized(Layout.SparseArrayLayout);
		void* Out = Dst.GetData(OutIdx, Layout.SparseArrayLayout);
		Inner.InitItem(Out);
		return Out;
	}

	// Load structs or ranges into non-empty set
	inline void InsertNonEmpty(FScriptSet& DstSet, FRangeLoadView Src) const
	{
		// Written to avoid FProperty::CopyCompleteValue_InContainer dependency
		// Items are loaded directly into sparse array and then removed if a duplicate existed
		FScriptSparseArray& Dst = AsSparseArray(DstSet);
		const int32 OldNum = Dst.NumUnchecked();
		int32 TmpIdx;
		void* Tmp = AddItem(Dst, /* out */ TmpIdx);
		if constexpr (ElemKind == EPropertyKind::Range)
		{
			TConstArrayView<FRangeBinding> InnerBindings = Range.InnerBindings;
			for (FRangeLoadView Item : Src.AsRanges())
			{
				LoadRange(Tmp, Item, InnerBindings);
				Tmp = HasItem(DstSet, Tmp) ? Tmp : AddItem(Dst, /* out */ TmpIdx);
			}
		}
		else
		{
			for (FStructLoadView Item : Src.AsStructs())
			{
				LoadStruct(Tmp, Item);
				Tmp = HasItem(DstSet, Tmp) ? Tmp : AddItem(Dst, /* out */ TmpIdx);
			}
		}

		Inner.DestroyItem(Tmp);
		Dst.RemoveAtUninitialized(Layout.SparseArrayLayout, TmpIdx, 1);

		if (Dst.NumUnchecked() != OldNum)
		{
			Rehash(DstSet);
		}
	}

	inline void Remove(FScriptSet& Dst, FRangeLoadView Src) const requires(bLeaves)
	{
		for (LeafType Item : CastAs<LeafType>(Src.AsLeaves()))
		{
			RemoveElem(Dst, &Item);
		}
	}

	inline void Remove(FScriptSet& Dst, FRangeLoadView Src) const
	{
		TArray<uint8, TInlineAllocator<64>> Buffer;
		Buffer.SetNumUninitialized(Inner.Size);
		Inner.InitItem(Buffer.GetData());
		void* Tmp = Buffer.GetData();

		if constexpr (ElemKind == EPropertyKind::Range)
		{
			TConstArrayView<FRangeBinding> Inners = Range.InnerBindings;
			for (FRangeLoadView Item : Src.AsRanges())
			{
				LoadRange(Tmp, Item, Inners);
				RemoveElem(Dst, Tmp);
			}
		}
		else
		{
			for (FStructLoadView Item : Src.AsStructs())
			{
				LoadStruct(Tmp, Item);
				RemoveElem(Dst, Tmp);
			}
		}

		Inner.DestroyItem(Tmp);
	}

	inline void DestroyRemoved(FScriptSet& Dst, int32 Idx) const
	{
		checkSlow(Idx < Dst.GetMaxIndex());
		checkSlow(!Dst.IsValidIndex(Idx));
		void* Elem = AsSparseArray(Dst).GetData(Idx, Layout.SparseArrayLayout);
		Inner.DestroyItem(Elem);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

class FSetBindings
{
	TMap<FParameterBinding, FBindId> Bindings;

	template<EPropertyKind ItemKind>
	void BindNew(FSetProperty* Property, FMemberBinding Elem, FBindId BindId, const FStructDeclaration& Declaration) 
	{
		// Todo: Ownership / memory leak
		TSetPropertyBinding<ItemKind>* Leak = new TSetPropertyBinding<ItemKind>(Property, Elem);
		GUE.Customs.BindStruct(BindId, *Leak, Declaration, {});
	}

	void DeltaBindNew(FSetProperty* Property, FMemberBinding Elem, FBindId BindId, const FStructDeclaration& Declaration)
	{
		switch (GetPropertyKind(Elem))
		{
		case EPropertyKind::Range:		BindNew<EPropertyKind::Range >(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::Struct:		BindNew<EPropertyKind::Struct>(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::Bool:		BindNew<EPropertyKind::Bool	 >(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::U8:			BindNew<EPropertyKind::U8	 >(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::U16:		BindNew<EPropertyKind::U16	 >(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::U32:		BindNew<EPropertyKind::U32	 >(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::U64:		BindNew<EPropertyKind::U64	 >(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::F32:		BindNew<EPropertyKind::F32	 >(Property, Elem, BindId, Declaration); break;
		case EPropertyKind::F64:		BindNew<EPropertyKind::F64	 >(Property, Elem, BindId, Declaration); break;
		default:						check(false); break;
		}
	}
public:

	FBindId Bind(FSetProperty* Property, FMemberBinding Elem)
	{
		check(Elem.Offset == 0);
		if (const FBindId* BindId = Bindings.Find(FParameterBinding(Elem)))
		{
			return *BindId;
		}

		// Index custom delta binding struct name
		FBothType Param = Elem.IndexParameterName(GUE.Names);
		FType BindType = FType{ GUE.Scopes.Core, FTypenameId(GUE.Names.MakeParametricTypeId(GUE.Typenames.Set, MakeArrayView(&Param.BindType, 1))) };
		FType DeclType = Param.IsLowered()
						 ? FType{ GUE.Scopes.Core, FTypenameId(GUE.Names.MakeParametricTypeId(GUE.Typenames.Set, MakeArrayView(&Param.DeclType, 1))) }
						 : BindType;
		FBindId BindId = GUE.Names.IndexBindId(BindType);
		FDeclId DeclId = Param.IsLowered() ? GUE.Names.IndexDeclId(DeclType) : LowerCast(BindId);
		FMemberId Members[] = { GUE.Members.Assign, GUE.Members.Remove, GUE.Members.Insert };

		// Todo: Ownership / memory leak
		const FStructDeclaration& Declaration = GUE.Types.DeclareStruct(DeclId, DeclType, 0, Members, EMemberPresence::AllowSparse);
		DeltaBindNew(Property, Elem, BindId, Declaration);
		return Bindings.Emplace(Elem, BindId);
	}
};
static FSetBindings GSets;

////////////////////////////////////////////////////////////////////////////////////////////////

// Flat TMap binding
template<class ScriptMap, EPropertyKind KeyKind, EPropertyKind ValueKind>
struct TMapPropertyItemBinding : IItemRangeBinding
{
	const FScriptMapLayout							Layout;
	const TInnerProperty<KeyKind>					InnerKey;
	const TInnerProperty<ValueKind>					InnerValue;
	
	TMapPropertyItemBinding(FMapProperty* In)
	: IItemRangeBinding(GUE.Typenames.Map)
	, Layout(In->MapLayout)
	, InnerKey(In->KeyProp)
	, InnerValue(In->ValueProp)
	{}

	inline SIZE_T GetStride() const	{ return Layout.SetLayout.Size; }

	inline uint8* InitMap(ScriptMap& Map, int32 Num) const
	{
		uint8* It = SetNumUninitialized(Map, Layout, Num);
		InitStridedItems(InnerKey, It, Num, GetStride());
		InitStridedItems(InnerValue, It + Layout.ValueOffset, Num, GetStride());
		return It;
	}
	
	inline void Rehash(ScriptMap& Map) const
	{
		Map.Rehash(Layout, MakeHashFn(InnerKey));
	}

	inline void DestroyAll(ScriptMap& Map) const
	{
		if (InnerKey.bDestruct || InnerValue.bDestruct)
		{
			const SIZE_T Stride = GetStride();
			const int32 ValueOffset = Layout.ValueOffset;
			const int32 Num = Map.NumUnchecked();
			uint8* It = static_cast<uint8*>(Map.GetData(0, Layout));
			if (IsCompact(Map))
			{
				DestroyStridedItems(InnerKey, It, Num, Stride);
				DestroyStridedItems(InnerValue, It + ValueOffset, Num, Stride);
			}
			else
			{
				for (int32 Idx = 0, Max = Map.GetMaxIndex(); Idx < Max; ++Idx)
				{
					if (Map.IsValidIndex(Idx))
					{
						InnerKey.DestroyItem(It);
						InnerValue.DestroyItem(It + ValueOffset);
					}
					It += Stride;
				}
			}
		}
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		ReadSparseItems(/* out */ Ctx.Items, Ctx.Request.GetRange<FScriptSparseArray>(), Layout.SetLayout.SparseArrayLayout);
	}

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		ScriptMap& Map = Ctx.Request.GetRange<ScriptMap>();
		int32 NewNum = static_cast<int32>(Ctx.Request.NumTotal());

		if (Ctx.Request.IsFirstCall())
		{
			DestroyAll(Map);
			Map.Empty(NewNum, Layout);
			if (NewNum)
			{
				void* Items = InitMap(Map, NewNum);
				Ctx.Items.Set(Items, Ctx.Request.NumTotal(), GetStride());
				Ctx.Items.RequestFinalCall();
			}
		}
		else
		{
			check(Ctx.Request.IsFinalCall());
			Rehash(Map);
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FMapMemberBindings
{
	FMemberBinding Key;
	FMemberBinding Value;
	FMemberBinding Pair;
};

template <EPropertyKind KeyKind, EPropertyKind ValueKind>
struct TMapPropertyCustomBinding : TMapPropertyItemBinding<FScriptMap, KeyKind, ValueKind>, ICustomBinding
{
	using Super = TMapPropertyItemBinding<FScriptMap, KeyKind, ValueKind>;
	using Super::Layout;
	using Super::InnerKey;
	using Super::InnerValue;
	using Super::GetStride;
	using Super::InitMap;
	using Super::Rehash;
	using SubSetIterator = TSubSetIterator<FScriptMap, FScriptMapLayout>;
	
	const TPropertyRangeSerializer<KeyKind>			KeyRange;
	const TPropertyRangeSerializer<ValueKind>		ValueRange;
	const FStructRangeSerializer					PairRange;

	const TPropertyRangeSerializer<KeyKind>&		GetKeyRange() const { return KeyRange; }
	const FStructRangeSerializer&					GetItemRange() const { return PairRange; }

	TMapPropertyCustomBinding(FMapProperty* Map, FMapMemberBindings Members)
	: Super(Map)
	, KeyRange(Members.Key)
	, ValueRange(Members.Value)
	, PairRange(Members.Pair)
	{}

	inline const void* GetValue(const void* Pair) const
	{
		return static_cast<const uint8*>(Pair) + Layout.ValueOffset;
	} 

	inline int32 FindKey(const FScriptMap& Map, const void* Key) const
	{
		return Map.FindPairIndex(Key, Layout, MakeHashFn(InnerKey), MakeIdenticalFn(InnerKey));
	}

	inline bool HasKey(const FScriptMap& Map, const void* Key) const
	{
		return FindKey(Map, Key) != INDEX_NONE;
	}

	inline bool HasItem(const FScriptMap& Map, const void* Pair) const
	{
		const void* Key = Pair;
		if (int32 Idx = FindKey(Map, Key); Idx != INDEX_NONE)
		{
			const void* FoundPair = Map.GetData(Idx, Layout);
			return MakeIdenticalFn(InnerValue)(GetValue(Pair), GetValue(FoundPair));
		}
		return false;
	}

	inline uint8* AddPair(FScriptSparseArray& Dst, int32& OutIdx, int32 ValueOffset) const
	{
		OutIdx = Dst.AddUninitialized(Layout.SetLayout.SparseArrayLayout);
		uint8* Out = static_cast<uint8*>(Dst.GetData(OutIdx, Layout.SetLayout.SparseArrayLayout));
		InnerKey.InitItem(Out);
		InnerValue.InitItem(Out + ValueOffset);
		return Out;
	}

	inline void DestroyPair(FScriptMap& Map, int32 Idx) const
	{
		if (InnerKey.bDestruct || InnerValue.bDestruct)
		{
			void* Pair = Map.GetData(Idx, Layout);
			InnerKey.DestroyItem(Pair);
			InnerValue.DestroyItem(static_cast<uint8*>(Pair) + Layout.ValueOffset);
		}
	}

	inline void RemoveKey(FScriptMap& Map, const void* Key) const
	{
		if (int32 Idx = FindKey(Map, Key); Idx != INDEX_NONE)
		{
			DestroyPair(Map, Idx);
			Map.RemoveAt(Idx, Layout);
		}
	}

	virtual void SaveCustom(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) override
	{
		SaveSetDelta(*this, Dst, *static_cast<const FScriptMap*>(Src), static_cast<const FScriptMap*>(Default), Ctx);
	}

	virtual void LoadCustom(void* Dst, FStructLoadView Src, ECustomLoadMethod Method) const override
	{
		check(Method == ECustomLoadMethod::Assign);
		LoadSetDelta(*this, *static_cast<FScriptMap*>(Dst), Src);
	}

	// Load into empty set
	inline void AssignEmpty(FScriptMap& Dst, FRangeLoadView Src) const
	{
		const int32 ValueOffset = Layout.ValueOffset;
		const SIZE_T Stride = GetStride();

		uint8* It = InitMap(Dst, Src.Num());
		
		if (!Src.IsEmpty())
		{
			FOptionalSchemaId InnerLoadIds[2];
			FStructRangeLoadView Structs = Src.AsStructs();
			Structs.GetSchema().GetInnerLoadIds(/* out */ MakeArrayView(InnerLoadIds));
			for (FStructLoadView Struct : Structs)
			{
				// Equivalent to LoadStruct(It, Struct);
				FBitCacheReader Bits;
				KeyRange.LoadItem(It,					/* in-out */ Struct.Values, /* in-out */ Bits, InnerLoadIds[0], Struct.Schema.Batch);
				ValueRange.LoadItem(It + ValueOffset,	/* in-out */ Struct.Values, /* in-out */ Bits, InnerLoadIds[1], Struct.Schema.Batch);
				It += Stride;
			}
		}

		Rehash(Dst);
	}

	// Load structs or ranges into non-empty map
	inline void InsertNonEmpty(FScriptMap& Dst, FRangeLoadView Src) const
	{
		// Written to avoid FProperty::CopyCompleteValue_InContainer dependency
		// Items are loaded directly into sparse array and then removed if a duplicate existed

		FScriptSparseArray& DstArray = AsSparseArray(Dst);
		const int32 OldNum = DstArray.NumUnchecked();
		const int32 ValueOffset = Layout.ValueOffset;
		int32 TmpIdx;
		uint8* Tmp = AddPair(DstArray, /* out*/ TmpIdx, ValueOffset);

		FOptionalSchemaId InnerLoadIds[2];
		FStructRangeLoadView Structs = Src.AsStructs();
		Structs.GetSchema().GetInnerLoadIds(/* out */ MakeArrayView(InnerLoadIds));
		for (FStructLoadView Struct : Structs)
		{
			// Equivalent to LoadStruct(It, Struct);
			FBitCacheReader Bits;
			KeyRange.LoadItem(Tmp,					/* in-out */ Struct.Values, /* in-out */ Bits, InnerLoadIds[0], Struct.Schema.Batch);

			if (int32 Idx = FindKey(Dst, Tmp); Idx != INDEX_NONE)
			{
				// Load value into existing pair
				uint8* Pair = static_cast<uint8*>(DstArray.GetData(Idx, Layout.SetLayout.SparseArrayLayout));
				ValueRange.LoadItem(Pair + ValueOffset,	/* in-out */ Struct.Values, /* in-out */ Bits, InnerLoadIds[1], Struct.Schema.Batch);
			}
			else
			{
				// Load value into tmp pair and add new temporary
				ValueRange.LoadItem(Tmp + ValueOffset,	/* in-out */ Struct.Values, /* in-out */ Bits, InnerLoadIds[1], Struct.Schema.Batch);
				Tmp = AddPair(DstArray, /* out*/ TmpIdx, ValueOffset);
			}
		}

		InnerKey.DestroyItem(Tmp);
		InnerValue.DestroyItem(Tmp + ValueOffset);
		DstArray.RemoveAtUninitialized(Layout.SetLayout.SparseArrayLayout, TmpIdx, 1);

		if (DstArray.NumUnchecked() != OldNum)
		{
			Rehash(Dst);
		}
	}

	inline void Remove(FScriptMap& Dst, FRangeLoadView Src) const
	{
		using LeafType = EquivalentLeafType<KeyKind>;
		for (LeafType Item : CastAs<LeafType>(Src.AsLeaves()))
		{
			RemoveKey(Dst, &Item);
		}
	}

	inline void Remove(FScriptMap& Dst, FRangeLoadView Src) const requires (KeyKind == EPropertyKind::Range)
	{
		TArray<uint8, TInlineAllocator<64>> Buffer;
		Buffer.SetNumUninitialized(InnerKey.Size);
		void* Tmp = Buffer.GetData();

		InnerKey.InitItem(Tmp);
		for (FRangeLoadView Item : Src.AsRanges())
		{
			LoadRange(Tmp, Item, KeyRange.InnerBindings);
			RemoveKey(Dst, Tmp);
		}
		InnerKey.DestroyItem(Tmp);
	}

	inline void Remove(FScriptMap& Dst, FRangeLoadView Src) const requires (KeyKind == EPropertyKind::Struct)
	{
		TArray<uint8, TInlineAllocator<64>> Buffer;
		Buffer.SetNumUninitialized(InnerKey.Size);
		void* Tmp = Buffer.GetData();

		InnerKey.InitItem(Tmp);
		for (FStructLoadView Item : Src.AsStructs())
		{
			LoadStruct(Tmp, Item);
			RemoveKey(Dst, Tmp);
		}
		InnerKey.DestroyItem(Tmp);
	}

	virtual bool DiffCustom(const void* A, const void* B, const FBindContext&) const override
	{
		return DiffSet(*this, *static_cast<const FScriptMap*>(A), *static_cast<const FScriptMap*>(B));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

class FMapBindings
{
	TMap<FBindId, FBindId>			NormalBindings;
	TMap<FBindId, FRangeBinding>	FrozenBindings;

	template<EPropertyKind KeyKind, EPropertyKind ValueKind>
	static IItemRangeBinding* New3(FMapProperty* Property, FMapMemberBindings Members, FBindId* OutCustomId)
	{
		if (OutCustomId == nullptr) // Freezable maps aren't delta serialized
		{
			return new TMapPropertyItemBinding<FFreezableScriptMap, KeyKind, ValueKind>(Property);
		}

		// Index custom delta binding struct name
		FParametricTypeId PairTypename = GUE.Names.Resolve(Members.Pair.InnermostSchema.Get().AsStruct()).Name.AsParametric();
		TConstArrayView<FType> Params = GUE.Names.Resolve(PairTypename).GetParameters();
		FType Type = { GUE.Scopes.Core, FTypenameId(GUE.Names.MakeParametricTypeId(GUE.Typenames.Map, Params)) };
		FBindId Id = GUE.Names.IndexBindId(Type);
		FMemberId MemberIds[] = { GUE.Members.Assign, GUE.Members.Remove, GUE.Members.Insert };

		// Todo: Ownership / memory leak
		const FStructDeclaration& Declaration = GUE.Types.DeclareStruct(LowerCast(Id), Type, 0, MemberIds, EMemberPresence::AllowSparse);
		auto Out = new TMapPropertyCustomBinding<KeyKind, ValueKind>(Property, Members);
		GUE.Customs.BindStruct(Id, *Out, Declaration, {});

		*OutCustomId = Id;
		return Out;
	}

	template<EPropertyKind KeyKind>
	inline IItemRangeBinding* New2(FMapProperty* Property, FMapMemberBindings Members, FBindId* OutCustomId)
	{
		switch (GetPropertyKind(Members.Value))
		{
		case EPropertyKind::Range:		return New3<KeyKind, EPropertyKind::Range>(Property, Members, OutCustomId);
		case EPropertyKind::Struct:		return New3<KeyKind, EPropertyKind::Struct>(Property, Members, OutCustomId);
		case EPropertyKind::Bool:		return New3<KeyKind, EPropertyKind::Bool>(Property, Members, OutCustomId);
		case EPropertyKind::U8:			return New3<KeyKind, EPropertyKind::U8>(Property, Members, OutCustomId);
		case EPropertyKind::U16:		return New3<KeyKind, EPropertyKind::U16>(Property, Members, OutCustomId);
		case EPropertyKind::U32:		return New3<KeyKind, EPropertyKind::U32>(Property, Members, OutCustomId);
		case EPropertyKind::U64:		return New3<KeyKind, EPropertyKind::U64>(Property, Members, OutCustomId);
		case EPropertyKind::F32:		return New3<KeyKind, EPropertyKind::F32>(Property, Members, OutCustomId);
		case EPropertyKind::F64:		return New3<KeyKind, EPropertyKind::F64>(Property, Members, OutCustomId);
		default:						check(false); return nullptr;
		}
	}

	inline IItemRangeBinding* New(FMapProperty* Property, FMapMemberBindings Members, FBindId* OutCustomId)
	{
		switch (GetPropertyKind(Members.Key))
		{
		case EPropertyKind::Range:		return New2<EPropertyKind::Range>(Property, Members, OutCustomId);
		case EPropertyKind::Struct:		return New2<EPropertyKind::Struct>(Property, Members, OutCustomId);
		case EPropertyKind::Bool:		return New2<EPropertyKind::Bool>(Property, Members, OutCustomId);
		case EPropertyKind::U8:			return New2<EPropertyKind::U8>(Property, Members, OutCustomId);
		case EPropertyKind::U16:		return New2<EPropertyKind::U16>(Property, Members, OutCustomId);
		case EPropertyKind::U32:		return New2<EPropertyKind::U32>(Property, Members, OutCustomId);
		case EPropertyKind::U64:		return New2<EPropertyKind::U64>(Property, Members, OutCustomId);
		case EPropertyKind::F32:		return New2<EPropertyKind::F32>(Property, Members, OutCustomId);
		case EPropertyKind::F64:		return New2<EPropertyKind::F64>(Property, Members, OutCustomId);
		default:						check(false); return nullptr;
		}
	}

public:
	FBindId BindNormal(FMapProperty* Property, FBindId PairId, FMapMemberBindings Members)
	{
		if (const FBindId* CustomId = NormalBindings.Find(PairId))
		{
			return *CustomId;
		}

		FBindId CustomId;
		New(Property, Members, /* out */ &CustomId);
		return NormalBindings.Emplace(PairId, CustomId);
	}

	FRangeBinding BindFreezable(FMapProperty* Property, FBindId PairId, FMapMemberBindings Members)
	{
		if (const FRangeBinding* RangeBinding = FrozenBindings.Find(PairId))
		{
			return *RangeBinding;
		}

		IItemRangeBinding* Leak = New(Property, Members, nullptr);
		return FrozenBindings.Emplace(PairId, FRangeBinding(*Leak, DefaultRangeMax));
	}
};
static FMapBindings GMaps;

////////////////////////////////////////////////////////////////////////////////////////////////

class FPairBindings
{
	struct FPair
	{
		FMemberBinding KV[2];
		friend uint32 GetTypeHash(FPair In) { return HashCombineFast(HashSkipOffset(In.KV[0]), HashSkipOffset(In.KV[1])); };
		inline bool operator==(const FPair& O) const { return EqSkipOffset(KV[0], O.KV[0]) && EqSkipOffset(KV[1], O.KV[1]); }
	};

	TMap<FPair, FBindId>	Bindings;

	inline FBindId BindImpl(FPair Pair)
	{
		check(Pair.KV[0].Offset == 0 && Pair.KV[1].Offset > 0);
		if (const FBindId* BindId = Bindings.Find(Pair))
		{
			return *BindId;
		}

		// Index names, can be optimized by checking if KeyParam / BindParam IsLowered()
		FBothType KeyParam = Pair.KV[0].IndexParameterName(GUE.Names);
		FBothType ValueParam = Pair.KV[1].IndexParameterName(GUE.Names);
		FType BindParams[2] = { KeyParam.BindType, ValueParam.BindType };
		FType DeclParams[2] = { KeyParam.DeclType, ValueParam.DeclType };
		FType BindType = { GUE.Scopes.Core, FTypenameId(GUE.Names.MakeParametricTypeId(GUE.Typenames.Pair, BindParams)) };
		FType DeclType = { GUE.Scopes.Core, FTypenameId(GUE.Names.MakeParametricTypeId(GUE.Typenames.Pair, DeclParams)) };
		FBindId BindId = GUE.Names.IndexBindId(BindType);
		FDeclId DeclId = GUE.Names.IndexDeclId(DeclType);
		FMemberId Members[2] = { GUE.Members.Key, GUE.Members.Value };

		// Todo: Ownership / memory leak
		GUE.Types.DeclareStruct(DeclId, DeclType, 0, Members, EMemberPresence::RequireAll);
		GUE.Schemas.BindStruct(BindId, DeclId, Pair.KV);

		Bindings.Emplace(Pair, BindId);

		return BindId;
	}
public:

	inline FMemberBinding Bind(FMemberBinding Key, FMemberBinding Value)
	{
		FMemberBinding Out(0);
		Out.InnermostSchema = FInnerId(BindImpl(FPair{Key, Value}));
		Out.InnermostType = DefaultStructBindType;
		return Out;
	}
};
static FPairBindings GPairs;

//////////////////////////////////////////////////////////////////////////////////////////////

inline void ReadBoolOptionalItem(FSaveRangeContext& Ctx, uint32 ItemSize)
{
	checkf((&Ctx.Request.GetRange<uint8>())[ItemSize] <= uint8(true),
		TEXT("Non-intrusive TOptional::bIsSet should be true or false, but byte at offset %d was %d"), ItemSize, (&Ctx.Request.GetRange<uint8>())[ItemSize]);
	bool bSet = (&Ctx.Request.GetRange<bool>())[ItemSize];
	Ctx.Items.SetAll(bSet ? Ctx.Request.Range : nullptr, uint64(bSet), ItemSize);
}

inline void MakeBoolOptionalItem(FLoadRangeContext& Ctx, uint32 ItemSize)
{
	check((&Ctx.Request.GetRange<uint8>())[ItemSize] <= 1);
	bool& bSet = (&Ctx.Request.GetRange<bool>())[ItemSize];
	bSet = Ctx.Request.NumTotal() > 0;
	Ctx.Items.Set(&Ctx.Request.GetRange<uint8>(), uint64(bSet), ItemSize);
}

static constexpr std::string_view TrivialOptionalName = "TrivialOptional";
template<uint32 ItemSize>
struct TTrivialOptionalBinding : IItemRangeBinding
{
	TTrivialOptionalBinding() : IItemRangeBinding(GUE.Names.IndexRangeBindName(Concat<TrivialOptionalName, HexString<ItemSize>>.data())) {}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override { ReadBoolOptionalItem(Ctx, ItemSize); }
	virtual void MakeItems(FLoadRangeContext& Ctx) const override { MakeBoolOptionalItem(Ctx, ItemSize); }
};

struct FTrivialOptionalBinding : IItemRangeBinding
{
	const uint32 ItemSize;
	explicit FTrivialOptionalBinding(uint32 Size) : IItemRangeBinding(GUE.Typenames.TrivialOptional), ItemSize(Size) {}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override { ReadBoolOptionalItem(Ctx, ItemSize); }
	virtual void MakeItems(FLoadRangeContext& Ctx) const override { MakeBoolOptionalItem(Ctx, ItemSize); }
};

struct FOptionalBindingBase : IItemRangeBinding
{
	const FProperty*	Inner;
	const uint32		ItemSize;
	const bool			bConstructor;
	const bool			bDestructor;
	
	explicit FOptionalBindingBase(const FProperty* In, FConcreteTypenameId BindName)
	: IItemRangeBinding(BindName)
	, Inner(In)
	, ItemSize(In->GetElementSize())
	, bConstructor(HasConstructor(In))
	, bDestructor(HasDestructor(In))
	{}

	void InitItem(void* Value) const
	{
		if (bConstructor)
		{
			ConstructValue(Inner, Value);
		}
		else
		{
			FMemory::Memzero(Value, ItemSize);
		}
	}
};

struct FIntrusiveOptionalBinding : FOptionalBindingBase
{
	explicit FIntrusiveOptionalBinding(const FProperty* In)
	: FOptionalBindingBase(In, GUE.Typenames.IntrusiveOptional)
	{}
	
	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		bool bSet = Inner->IsIntrusiveOptionalValueSet(Ctx.Request.Range);
		Ctx.Items.SetAll(bSet ? Ctx.Request.Range : nullptr, uint64(bSet), ItemSize);
	}

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		uint8* Value = &Ctx.Request.GetRange<uint8>();
		Inner->ClearIntrusiveOptionalValue(Value);

		if (Ctx.Request.NumTotal() > 0)
		{
			InitItem(Value);
			Ctx.Items.Set(Value, 1, ItemSize);
		}
		else
		{
			Ctx.Items.Set(nullptr, 0, ItemSize);
		}
	}
};

struct FNonIntrusiveOptionalBinding : FOptionalBindingBase
{
	explicit FNonIntrusiveOptionalBinding(const FProperty* In)
	: FOptionalBindingBase(In, GUE.Typenames.NonIntrusiveOptional)
	{}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		ReadBoolOptionalItem(Ctx, ItemSize);
	}

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		uint8* Value = &Ctx.Request.GetRange<uint8>();
		bool& bSet = reinterpret_cast<bool&>(Value[ItemSize]);
		if (bDestructor && bSet)
		{
			DestroyValue(Inner, Value);
		}

		bSet = Ctx.Request.NumTotal() > 0;
		Ctx.Items.Set(bSet ? Value : nullptr , 1, ItemSize);
		if (bSet)
		{
			InitItem(Value);	
		}
	}
};

class FOptionalBindings
{
	TTrivialOptionalBinding<1> Trivial1;
	TTrivialOptionalBinding<2> Trivial2;
	TTrivialOptionalBinding<4> Trivial4;
	TTrivialOptionalBinding<8> Trivial8;
	TTrivialOptionalBinding<12> Trivial12;
	TTrivialOptionalBinding<16> Trivial16;
	TTrivialOptionalBinding<24> Trivial24;
	TTrivialOptionalBinding<32> Trivial32;

	TMap<FParameterBinding, FRangeBinding>	NormalBindings;
	TMap<FParameterBinding, FRangeBinding>	IntrusiveBindings;

	const IItemRangeBinding* BindNew(FProperty* Inner)
	{
		if (HasConstructor(Inner) || HasDestructor(Inner))
		{
			return new FNonIntrusiveOptionalBinding(Inner);
		}
		else switch (Inner->GetElementSize())
		{
			case 1:		return &Trivial1;
			case 2:		return &Trivial2;
			case 4:		return &Trivial4;
			case 8:		return &Trivial8;
			case 12:	return &Trivial12;
			case 16:	return &Trivial16;
			case 24:	return &Trivial24;
			case 32:	return &Trivial32;
			default:	return new FTrivialOptionalBinding(Inner->GetElementSize());
		}
	}

public:
	FRangeBinding Bind(FProperty* Inner, FMemberBinding Key)
	{
		check(Key.Offset == 0);
		
		bool bIntrusive = Inner->HasIntrusiveUnsetOptionalState();
		TMap<FParameterBinding, FRangeBinding>& Bindings = bIntrusive ? IntrusiveBindings : NormalBindings;
		if (const FRangeBinding* Binding = Bindings.Find(FParameterBinding(Key)))
		{
			return *Binding;
		}

		// Todo: Ownership / memory leak
		const IItemRangeBinding* Out = bIntrusive ? new FIntrusiveOptionalBinding(Inner) : BindNew(Inner);
		return Bindings.Emplace(Key, FRangeBinding(*Out, ERangeSizeType::Uni));
	}
};

static FOptionalBindings GOptionals;

//////////////////////////////////////////////////////////////////////////////////////////////

struct FStringBindings
{
	TStringBinding<FString>			TCharInstance{GUE.Typenames.String};
	TStringBinding<FUtf8String>		Utf8Instance{GUE.Typenames.Utf8String};
	TStringBinding<FAnsiString>		AnsiInstance{GUE.Typenames.AnsiString};
	TStringBinding<FUtf8String>		VerseInstance{GUE.Typenames.VerseString}; // Bypass Verse::FNativeString for now
	FRangeBinding					TChar{TCharInstance, DefaultRangeMax};
	FRangeBinding					Utf8{Utf8Instance, DefaultRangeMax};
	FRangeBinding					Ansi{AnsiInstance, DefaultRangeMax};
	FRangeBinding					Verse{VerseInstance, DefaultRangeMax};

	inline const FRangeBinding&	SelectBinding(uint64 CastFlags) const
	{
		switch (CastFlags & StringMask)
		{
			case CASTCLASS_FStrProperty:			return TChar;
			case CASTCLASS_FUtf8StrProperty:		return Utf8;
			case CASTCLASS_FAnsiStrProperty:		return Ansi;
			case CASTCLASS_FVerseStringProperty:	return Verse;
			default:								break;
		}
		check(FMath::CountBits(CastFlags & StringMask) == 1);
		check(false);
		return TChar;
	}

	FMemberBinding Bind(FProperty* Property, uint64 CastFlags) const
	{
		const FRangeBinding& Binding = SelectBinding(CastFlags);

		FMemberBinding Out(Property->GetOffset_ForInternal());
		Out.InnermostType = FMemberBindType(ReflectLeaf<char8_t>);
		Out.RangeBindings = MakeArrayView(&Binding, 1);
		return Out;
	}
};
static const FStringBindings GStrings; // static init dependency after GUE

////////////////////////////////////////////////////////////////////////////////////////////////

struct FStaticArrayBinding : IItemRangeBinding
{
	uint32 Num;
	uint32 Stride;

	FStaticArrayBinding(uint32 InNum, uint32 InStride)
	: IItemRangeBinding(GUE.Typenames.StaticArray)
	, Num(InNum)
	, Stride(InStride)
	{}

	virtual void				ReadItems(FSaveRangeContext& Ctx) const override	{ Ctx.Items.SetAll(Ctx.Request.Range, Num, Stride); }
	virtual void				MakeItems(FLoadRangeContext& Ctx) const override	{ Ctx.Items.Set(&Ctx.Request.GetRange<uint8>(), Num, Stride); }
};

////////////////////////////////////////////////////////////////////////////////////////////////

class FPropertyBinder
{
public:
	FPropertyBinder(FBindId Id) : Owner(Id) {}

	void								AddMember(FMemberBinding Member)	{ Members.Add(Member); }
	void								BindMember(FProperty* Property)		{ Members.Emplace(BindProperty(Property)); }
	TConstArrayView<FMemberBinding>		GetMembers() const					{ return Members; }

	// Only true for noexport UScriptStruct with STRUCT_Immutable | STRUCT_Atomic flags
	bool								IsDense() { return GetOwnerOccupancy() == EMemberPresence::RequireAll; }

private:
	const FBindId									Owner;
	TOptional<FScopeId>								OwnerScope;
	TOptional<EMemberPresence>						OwnerOccupancy;
	TPagedArray<FRangeBinding, 1024>				Ranges;
	TArray<FMemberBinding, TInlineAllocator<64>>	Members;
	// BPVM only?
	const FName										VerseFunctionProperty{"VerseFunctionProperty"};
	const FName										VerseDynamicProperty{"VerseDynamicProperty"};
	const FName										ReferenceProperty{"ReferenceProperty"}; // Verse reference + FProperty*

	TConstArrayView<FRangeBinding> AllocateRangeBindings(FRangeBinding Head, TConstArrayView<FRangeBinding> Tail)
	{
		if (Tail.Num() == 0)
		{
			return MakeArrayView(&Ranges.Add_GetRef(Head), 1);
		}
		
		// Ensure contiguous out range by padding up with dummy tail slice
		static constexpr uint32 PageMax = decltype(Ranges)::MaxPerPage();
		const int32 OutNum = 1 + Tail.Num();
		check(OutNum <= PageMax);
		const int32 NewPages = Align(Ranges.Num() + OutNum, PageMax) / PageMax;
		if (NewPages > Ranges.NumPages() && !Ranges.IsEmpty())
		{
			int32 NumPad = Align(Ranges.Num(), PageMax) - Ranges.Num();
			Ranges.Append(Tail.Slice(0, NumPad));
			check(Ranges.Num() % PageMax == 0);
		}

		const FRangeBinding* OutData = &Ranges.Add_GetRef(Head);
		Ranges.Append(Tail);
		return MakeArrayView(OutData, OutNum);
	}

	static FMemberBinding Todo(FProperty* Property)
	{
		return FMemberBinding(Property->GetOffset_ForInternal());
	}

	FScopeId GetOwnerScope()
	{
		if (!OwnerScope)
		{
			FType OwnerType = GUE.Names.Resolve(Owner);
			OwnerScope = GUE.Names.NestFlatScope(OwnerType.Scope, {OwnerType.Name.AsConcrete().Id});
		}

		return OwnerScope.GetValue();
	}

	EMemberPresence GetOwnerOccupancy()
	{
		if (!OwnerOccupancy)
		{
			OwnerOccupancy = GUE.Types.Get(LowerCast(Owner)).Occupancy;
		}

		return OwnerOccupancy.GetValue();
	}

	inline FMemberBinding BindAsRange(FProperty* Property, FRangeBinding RangeBinding, FMemberBinding Inner)
	{
		if (Inner.InnermostType.IsLeaf() && Inner.InnermostType.AsLeaf().Bind.Type == ELeafBindType::BitfieldBool)
		{
			UE_LOGFMT(LogPlainPropsUObject, Warning, "Property '{Property}' is a '{Container}' of bitfield bools, which make no sense. Binding as range of bools.", 
				Property->GetFName(), GUE.Debug.Print(RangeBinding.GetBindName()));
			Inner.InnermostType = FMemberBindType(ReflectArithmetic<bool>);
		}

		FMemberBinding Out(Property->GetOffset_ForInternal());
		Out.InnermostType = Inner.InnermostType;
		Out.InnermostSchema = Inner.InnermostSchema;
		Out.RangeBindings = AllocateRangeBindings(RangeBinding, Inner.RangeBindings);
		return Out;
	}

	inline FMemberBinding BindAsStruct(FProperty* Property, FBindId Id)
	{
		FMemberBinding Out(Property->GetOffset_ForInternal());
		Out.InnermostSchema = FInnerId(Id);
		Out.InnermostType = DefaultStructBindType;
		return Out;
	}

	inline FMemberBinding BindAsStruct(FProperty* Property, UStruct* Struct)
	{
		FType Type = IndexType(SkipEmptyBases(Struct));
		return BindAsStruct(Property, GUE.Names.IndexBindId(Type));
	}

	inline static FBitfieldBoolBindType MakeBitfieldBool(uint8 BitIdx)
	{
		return {EMemberKind::Leaf, ELeafBindType::BitfieldBool, BitIdx};
	}
	
	inline FMemberBinding BindBool(FBoolProperty* Property)
	{
		check(Property->GetByteOffset() == 0);
		FMemberBinding Out(Property->GetOffset_ForInternal());
		uint8 BitIdx = static_cast<uint8>(FMath::FloorLog2NonZero(Property->GetFieldMask()));
		FLeafBindType Type = Property->IsNativeBool()	? FLeafBindType(ELeafBindType::Bool, ELeafWidth::B8)
														: FLeafBindType(MakeBitfieldBool(BitIdx));
		Out.InnermostType = FMemberBindType(Type);
		return Out;
	}

	inline FMemberBinding BindEnum(FEnumProperty* Property)
	{
		FMemberBinding Out(Property->GetOffset_ForInternal());
		Out.InnermostSchema = FInnerId(GUE.Names.IndexEnum(IndexType(Property->GetEnum())));
		FUnpackedLeafType Leaf = {ELeafType::Enum, WidthOf(Property->GetElementSize())};
		Out.InnermostType = FMemberBindType(Leaf);
		return Out;
	}

	inline FLeafBindType BindByte(FByteProperty* Property, FOptionalInnerId& OutEnumId)
	{
		if (const UEnum* Enum = Property->GetIntPropertyEnum())
		{
			OutEnumId = FInnerId(GUE.Names.IndexEnum(IndexType(Enum)));
			return FLeafBindType(ELeafBindType::Enum, ELeafWidth::B8);
		}
		return FLeafBindType(ELeafBindType::IntU, ELeafWidth::B8);
	}

	inline FMemberBinding BindNumeric(FNumericProperty* Property, uint64 Flags)
	{
		FMemberBinding Out(Property->GetOffset_ForInternal());
		bool bFloat = HasAny<CASTCLASS_FFloatProperty | CASTCLASS_FDoubleProperty>(Flags);
		bool bIntS = HasAny<CASTCLASS_FInt8Property | CASTCLASS_FInt16Property | CASTCLASS_FIntProperty | CASTCLASS_FInt64Property>(Flags);
		if (HasAny<CASTCLASS_FByteProperty>(Flags))
		{
			FLeafBindType Leaf = BindByte(static_cast<FByteProperty*>(Property), /* out enum */ Out.InnermostSchema);
			Out.InnermostType = FMemberBindType(Leaf);
		}
		else
		{
			ELeafType Type = bFloat ? ELeafType::Float : (bIntS ? ELeafType::IntS : ELeafType::IntU);
			FUnpackedLeafType Leaf = {Type, WidthOf(Property->GetElementSize())};
			Out.InnermostType = FMemberBindType(Leaf);	
		}
		
		return Out;
	}
	
	inline FMemberBinding BindArray(FArrayProperty* Property)
	{
		return BindAsRange(Property, AllocateArrayBinding(Property), BindSingleProperty(Property->Inner));
	}
	inline FMemberBinding BindMap(FMapProperty* Property)
	{
		FMemberBinding Key = BindSingleProperty(Property->KeyProp);
		FMemberBinding Value = BindSingleProperty(Property->ValueProp);
		FMemberBinding Pair = GPairs.Bind(Key, Value);
		FBindId PairId = Pair.InnermostSchema.Get().AsStructBindId();

		bool bFreezable = EnumHasAnyFlags(Property->MapFlags, EMapPropertyFlags::UsesMemoryImageAllocator);
		return bFreezable	? BindAsRange(Property, GMaps.BindFreezable(Property, PairId, {Key, Value, Pair}), Pair)
							: BindAsStruct(Property, GMaps.BindNormal(Property, PairId, {Key, Value, Pair}));
	}
	inline FMemberBinding BindSet(FSetProperty* Property)
	{
		FMemberBinding Elem = BindSingleProperty(Property->ElementProp);
		return BindAsStruct(Property, GSets.Bind(Property, Elem));
	}
	inline FMemberBinding BindOptional(FOptionalProperty* Property)
	{
		FMemberBinding Inner = BindSingleProperty(Property->GetValueProperty());
		return BindAsRange(Property, GOptionals.Bind(Property->GetValueProperty(), Inner), Inner);
	}

	#if WITH_VERSE_VM
	inline FMemberBinding BindVValue(FVValueProperty* Property) { return Todo(Property); }
	inline FMemberBinding BindVRestValue(FVRestValueProperty* Property) { return Todo(Property); }
	#endif

	FMemberBinding BindSingleProperty(FProperty* Property)
	{
		FName PropertyTypename = Property->GetClass()->GetFName();
		uint64 Flags = Property->GetCastFlags();
		if (HasAny<LeafMask>(Flags))
		{
			if (HasAny<CASTCLASS_FNumericProperty>(Flags))
			{
				return BindNumeric(static_cast<FNumericProperty*>(Property), Flags);
			}
			return HasAny<CASTCLASS_FEnumProperty>(Flags)
				? BindEnum(static_cast<FEnumProperty*>(Property))
				: BindBool(static_cast<FBoolProperty*>(Property));
		}
		else if (HasAny<CommonStructMask>(Flags))
		{
			return BindAsStruct(Property, FlagsToCommonBindId(Flags & CommonStructMask));
		}
		else if (HasAny<CASTCLASS_FStructProperty>(Flags))
		{
			return BindAsStruct(Property, static_cast<FStructProperty*>(Property)->Struct);
		}
		else if (HasAny<ContainerMask>(Flags))
		{
			if (HasAny<CASTCLASS_FArrayProperty>(Flags))
			{
				return BindArray(static_cast<FArrayProperty*>(Property));
			}
			if (HasAny<CASTCLASS_FMapProperty>(Flags))
			{
				return BindMap(static_cast<FMapProperty*>(Property));
			}
			return HasAny<CASTCLASS_FSetProperty>(Flags)
				? BindSet(static_cast<FSetProperty*>(Property))
				: BindOptional(static_cast<FOptionalProperty*>(Property));
		}
		else if (HasAny<StringMask>(Flags))
		{
			return GStrings.Bind(Property, Flags);
		}
		else if (HasAny<MiscMask>(Flags))
		{
			FBindId BindId = HasAny<CASTCLASS_FInterfaceProperty>(Flags)
				? BindInterface(static_cast<FInterfaceProperty*>(Property))
				: BindSparseDelegate(Owner, static_cast<FMulticastSparseDelegateProperty*>(Property));
			return BindAsStruct(Property, BindId);
		}
#if WITH_VERSE_VM
		else if (HasAny<CASTCLASS_FVValueProperty | CASTCLASS_FVRestValueProperty>(Flags))
		{
			return HasAny<CASTCLASS_FVValueProperty>(Flags)
				? BindVValue(static_cast<FVValueProperty*>(Property))
				: BindVRestValue(static_cast<FVRestValueProperty*>(Property)); 
		}
#else // Verse BPVM
		else
		{
			if (PropertyTypename == VerseFunctionProperty) // FVerseFunctionProperty
			{
				return BindAsStruct(Property, GUE.Structs.VerseFunction);
			}
			else if (PropertyTypename == VerseDynamicProperty) // FVerseDynamicProperty
			{
				return BindAsStruct(Property, GUE.Structs.DynamicallyTypedValue);
			}
			else if (PropertyTypename == ReferenceProperty) // FReferenceProperty
			{
				return BindAsStruct(Property, GUE.Structs.ReferencePropertyValue);
			}
		}
#endif

		checkf(false, TEXT("Unrecognized class cast flags %llx in %s %s"), Flags, *PropertyTypename.ToString(), *Property->GetNameCPP());
		return FMemberBinding(Property->GetOffset_ForInternal());
	}

	FType MakeStaticArrayTypename(FName PropertyName)
	{
		return { GetOwnerScope(), GUE.Names.MakeTypename(PropertyName) };
	}

	FMemberBinding BindProperty(FProperty* Property)
	{
		FMemberBinding Out = BindSingleProperty(Property);
		if (Property->GetSize() == Property->GetElementSize())
		{
			return Out;
		}

		// Bind static array
		EMemberPresence Occupancy = GetOwnerOccupancy();
		uint32 TotalSize = static_cast<uint32>(Property->GetSize());
		uint32 ElementSize = static_cast<uint32>(Property->GetElementSize());
		uint32 ArrayDim = TotalSize / ElementSize;
		check(ArrayDim * ElementSize == TotalSize);
		if (Occupancy == EMemberPresence::RequireAll || ArrayDim > FStructDeclaration::MaxMembers)
		{
			// Create range binding that isn't delta-serializable
			//
			// Could generate nested numeral structs instead. Unsure if automatic
			// per-element delta serialization for massive arrays is desirable.
			//
			// To delta-serialize massive arrays, custom-bind the owning struct
			// and implement delta serialization manually

			// Todo: Ownership / memory leak
			const FStaticArrayBinding& ItemBinding = *new FStaticArrayBinding(ArrayDim, ElementSize);
			ERangeSizeType SizeType = ArrayDim < 256 ? ERangeSizeType::U8 : ((ArrayDim < 65536) ? ERangeSizeType::U16 : ERangeSizeType::U32);
			Out.RangeBindings =	AllocateRangeBindings(FRangeBinding(ItemBinding, SizeType), Out.RangeBindings);
		}
		else
		{
			// Create struct binding to allow delta serialization
			FType StaticArrayType = MakeStaticArrayTypename(Property->GetFName());
			FBindId StaticArrayId = GUE.Names.IndexBindId(StaticArrayType);

			// Todo: Ownership
			TConstArrayView<FMemberId> Numerals = GUE.Numerals.MakeRange(IntCastChecked<uint16>(ArrayDim));
			GUE.Types.DeclareNumeralStruct(LowerCast(StaticArrayId), StaticArrayType, Numerals, Occupancy);

			TArray<FMemberBinding, TInlineAllocator<64>> Elements;
			Elements.Init(Out, ArrayDim);
			uint64 Offset = 0;
			for (FMemberBinding& Element : Elements)
			{
				Element.Offset = Offset;
				Offset += ElementSize;
			}
			GUE.Schemas.BindStruct(StaticArrayId, LowerCast(StaticArrayId), Elements);
			
			Out.InnermostType = DefaultStructBindType;
			Out.InnermostSchema = FInnerId(StaticArrayId);
			Out.RangeBindings = {};
		}
	
		return Out;	
	}
};

inline void BindMembers(FPropertyBinder& Out, const UStruct* Struct)
{
	for (FProperty* It = Struct->PropertyLink; It && It->GetOwner<UStruct>() == Struct; It = It->PropertyLinkNext)
	{
		if (ShouldBind(It))
		{
			Out.BindMember(It);
		}
	}
}

// Must match DeclareSuperMembers
void BindSuperMembers(FPropertyBinder& Out, const UStruct* Struct)
{
	if (const UStruct* Super = Struct->GetInheritanceSuper())
	{
		BindSuperMembers(Out, Super);
		if (ShouldBind(Super))
		{
			BindMembers(Out, Super);
		}
	}
}

void BindStruct(FBindId Id, const UStruct* Struct)
{
	if (GUE.Customs.FindStruct(Id))
	{
		return;
	}

	FPropertyBinder Binder(Id);
	if (Struct->GetInheritanceSuper())
	{
		if (Binder.IsDense())
		{
			BindSuperMembers(/* out */ Binder, Struct);
		}
		else
		{
			const FStructDeclaration& Declared = GUE.Types.Get(LowerCast(Id));
			if (Declared.Super)
			{
				FMemberBinding Member;
				Member.InnermostType = SuperStructBindType;
				Member.InnermostSchema = FInnerId(Declared.Super.Get());
				Binder.AddMember(Member);
			}
		}
	}
	BindMembers(/* out */ Binder, Struct);

	GUE.Schemas.BindStruct(Id, LowerCast(Id), Binder.GetMembers());
	
	// Don't bind CDOs, object defaults are passed in from top and objects aren't owned by containers 
	if (Struct->HasAnyCastFlags(CASTCLASS_UScriptStruct))
	{
		GUE.Defaults.Bind(Id, static_cast<const UScriptStruct*>(Struct));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

static void BindInitialTypes()
{
	UE_LOGFMT(LogPlainPropsUObject, Display, "Binding types to PlainProps schemas...");

	// Declare all UScriptStruct/UClass/UFunction/UEnum
	static const FBindId SkipStructs[] = { GUE.Structs.VerseFunction };
	TArray<FBindId> Ids;
	for (TObjectIterator<UField> It; It; ++It)
	{
		UField* Field = *It;
		if (UStruct* Struct = Cast<UStruct>(Field))
		{
			FType Type = IndexType(Struct);
			FStructId Id = GUE.Names.IndexStruct(Type);
			if (!Algo::Find(SkipStructs, FBindId(Id)))
			{
				DeclareStruct(Struct, Type, FDeclId(Id));
			}
			Ids.Add(FBindId(Id));
		}
		else if (UEnum* Enum = Cast<UEnum>(Field))
		{
			DeclareEnum(Enum);
		}
	}

	// Bind all UScriptStruct/UClass/UFunction
	const FBindId* IdIt = &Ids[0];
	for (TObjectIterator<UStruct> It; It; ++It, ++IdIt)
	{
		if (!Algo::Find(SkipStructs, *IdIt))
		{
			BindStruct(*IdIt, *It);
		}
	}
	check(IdIt == &Ids[0] + Ids.Num());
}

static void DeclareProperty(FDeclId Id, TConstArrayView<FMemberId> Members, EMemberPresence Occupancy)
{
	GUE.Types.DeclareStruct(Id,	GUE.Names.Resolve(Id), 0, Members, Occupancy);
}

static void InitBatchedProperties()
{
	// All property batch types share declaration, but some members could be exclusive to a specific type e.g. FMemoryPropertyBatch
	DeclareProperty(GUE.Structs.Name,			{ GUE.Members.Id }, EMemberPresence::RequireAll);
	DeclareProperty(GUE.Structs.Text,			{ GUE.Members.Id }, EMemberPresence::RequireAll);
	DeclareProperty(GUE.Structs.ClassPtr,		{ GUE.Members.Id }, EMemberPresence::RequireAll);
	DeclareProperty(GUE.Structs.ObjectPtr,		{ GUE.Members.Id }, EMemberPresence::RequireAll);
	DeclareProperty(GUE.Structs.WeakObjectPtr,	{ GUE.Members.Id }, EMemberPresence::RequireAll);
	DeclareProperty(GUE.Structs.SoftObjectPtr,	{ GUE.Members.Id }, EMemberPresence::RequireAll);
	DeclareProperty(GUE.Structs.LazyObjectPtr,	{ GUE.Members.Id }, EMemberPresence::RequireAll);

	GUE.Defaults.BindZeroes(GUE.Structs.Name, sizeof(FName), alignof(FName));
	GUE.Defaults.BindStatic(GUE.Structs.Text, &FText::GetEmpty());
	GUE.Defaults.BindZeroes(GUE.Structs.ClassPtr, sizeof(TSubclassOf<UClass>), alignof(TSubclassOf<UClass>));
	GUE.Defaults.BindZeroes(GUE.Structs.ObjectPtr, sizeof(FObjectPtr), alignof(FObjectPtr));
	GUE.Defaults.BindZeroes(GUE.Structs.WeakObjectPtr, sizeof(FWeakObjectPtr), alignof(FWeakObjectPtr));
	GUE.Defaults.BindZeroes(GUE.Structs.SoftObjectPtr, sizeof(FSoftObjectPtr), alignof(FSoftObjectPtr));
	GUE.Defaults.BindZeroes(GUE.Structs.LazyObjectPtr, sizeof(FLazyObjectPtr), alignof(FLazyObjectPtr));
}

#if UE_FNAME_OUTLINE_NUMBER
static uint32 ToInt(FName In)
{
	return In.GetDisplayIndex().ToUnstableInt();
}
static FName FromInt(uint32 In)
{
	return FName::CreateFromDisplayId(FNameEntryId::FromUnstableInt(In), NAME_NO_NUMBER_INTERNAL);
}
#else
static uint64 ToInt(FName In)
{
	return (uint64(In.GetNumber()) << 32) | In.GetDisplayIndex().ToUnstableInt();
}
static FName FromInt(uint64 In)
{
	return FName::CreateFromDisplayId(FNameEntryId::FromUnstableInt(static_cast<uint32>(In)), static_cast<uint32>(In >> 32));
}
#endif

struct FMemoryPropertyBatch
{
	TArray<FText> Texts; // Tricky to serialize intrusively

	static void Save(FMemberBuilder& Out, FName In, const FSaveContext&)
	{
		Out.Add(GUE.Members.Id, ToInt(In));
	}

	static void Load(FName& Out, FStructLoadView In)
	{
		Out = FromInt(LoadSole<uint64>(In));
	}

	void Save(FMemberBuilder& Out, const FText& In, const FSaveContext&)
	{
		Out.Add(GUE.Members.Id, Texts.Num());
		Texts.Add(In);
	}

	void Load(FText& Out, FStructLoadView In) const
	{
		Out = Texts[LoadSole<int32>(In)];
	}

	static void Save(FMemberBuilder& Out, FObjectHandle In, const FSaveContext&)
	{
		static_assert(sizeof(In) == sizeof(uint64));
		Out.Add(GUE.Members.Id, reinterpret_cast<const uint64&>(In));
	}

	static void Load(FObjectHandle& Out, FStructLoadView In)
	{
		LoadSole<uint64>(&Out, In);
	}

	static void Save(FMemberBuilder& Out, const FWeakObjectPtr& In, const FSaveContext&)
	{
		// Save ObjectSerialNumber + ObjectIndex a single uint64
		static_assert(sizeof(FWeakObjectPtr) == sizeof(uint64));
		Out.Add(GUE.Members.Id, reinterpret_cast<const uint64&>(In));
	}

	static void Load(FWeakObjectPtr& Out, FStructLoadView In)
	{
		LoadSole<uint64>(&Out, In);
	}

	static void Save(FMemberBuilder& Out, const FSoftObjectPtr& In, const FSaveContext& Ctx)
	{
		FBuiltStruct* SoftPath = SaveStruct(&In.GetUniqueID(), GUE.Structs.SoftObjectPath, Ctx);
		Out.AddStruct(GUE.Members.Id, GUE.Structs.SoftObjectPath, SoftPath);
	}

	static void Load(FSoftObjectPtr& Out, FStructLoadView In)
	{
		Out.ResetWeakPtr();
		LoadSoleStruct(&Out.GetUniqueID(), In);
	}

	static void Save(FMemberBuilder& Out, const FLazyObjectPtr& In, const FSaveContext& Ctx)
	{
		FBuiltStruct* Guid = SaveStruct(&In.GetUniqueID(), GUE.Structs.Guid, Ctx);
		Out.AddStruct(GUE.Members.Id, GUE.Structs.Guid, Guid);
	}

	static void Load(FLazyObjectPtr& Out, FStructLoadView In)
	{
		Out.ResetWeakPtr();
		LoadSoleStruct(&Out.GetUniqueID(), In);
	}
};

inline bool DiffProperty(FName A, FName B) { return !A.IsEqual(B, ENameCase::CaseSensitive); }
inline bool DiffProperty(const FText& A, const FText& B) { return !FTextProperty::Identical_Implementation(A, B, 0); }
inline bool DiffProperty(FObjectHandle A, FObjectHandle B) { return A != B; }
inline bool DiffProperty(const FWeakObjectPtr& A, const FWeakObjectPtr& B) { return A != B; }
inline bool DiffProperty(const FSoftObjectPtr& A, const FSoftObjectPtr& B) { return A != B; }
inline bool DiffProperty(const FLazyObjectPtr& A, const FLazyObjectPtr& B) { return A != B; }


template<class Type, class BatchType>
struct TCustomPropertyBinding final : ICustomBinding
{
	TCustomPropertyBinding(BatchType& InBatch) : Batch(InBatch) {}

	BatchType& Batch;

	virtual void SaveCustom(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) override
	{
		if (!Default || DiffCustom(Src, Default, Ctx))
		{
			Batch.Save(Dst, *static_cast<const Type*>(Src), Ctx);
		}
	}

	virtual void LoadCustom(void* Dst, FStructLoadView Src, ECustomLoadMethod) const override
	{
		Batch.Load(*static_cast<Type*>(Dst), Src);
	}

	virtual bool DiffCustom(const void* A, const void* B, const FBindContext&) const override
	{
		return DiffProperty(*static_cast<const Type*>(A), *static_cast<const Type*>(B));
	}
};

template<class BatchType>
struct TCustomPropertyBindings
{
	TCustomPropertyBindings(BatchType& Batch, const FCustomBindings& Underlay)
	: Overlay(Underlay)
	, Name(Batch)
	, Text(Batch)
	, ObjectPtr(Batch)
	, SoftObjectPtr(Batch)
	, WeakObjectPtr(Batch)
	, LazyObjectPtr(Batch)
	{
		Bind(GUE.Structs.Name, Name);
		Bind(GUE.Structs.Text, Text);
		Bind(GUE.Structs.ClassPtr, ObjectPtr); // TSubclassOf<> is essentially a TObjectPtr
		Bind(GUE.Structs.ObjectPtr, ObjectPtr);
		Bind(GUE.Structs.SoftObjectPtr, SoftObjectPtr);
		Bind(GUE.Structs.WeakObjectPtr, WeakObjectPtr);
		Bind(GUE.Structs.LazyObjectPtr, LazyObjectPtr);
	}

	void Bind(FBindId Id, ICustomBinding& Binding)
	{
		Overlay.BindStruct(Id, Binding, GUE.Types.Get(LowerCast(Id)));
	}

	FCustomBindingsOverlay								Overlay;
	TCustomPropertyBinding<FName, BatchType>			Name;
	TCustomPropertyBinding<FText, BatchType>			Text;
	TCustomPropertyBinding<FObjectHandle, BatchType>	ObjectPtr;
	TCustomPropertyBinding<FSoftObjectPtr, BatchType>	SoftObjectPtr;
	TCustomPropertyBinding<FWeakObjectPtr, BatchType>	WeakObjectPtr;
	TCustomPropertyBinding<FLazyObjectPtr, BatchType>	LazyObjectPtr;
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemoryBatch
{
	TArray64<uint8>				Data;
	TArray<FStructId>			RuntimeIds; // To avoid reindexing schema FType
	FMemoryPropertyBatch		Properties; // Contains FTexts referenced from Data
};

static constexpr uint32 Magics[] = { 0xFEEDF00D, 0xABCD1234, 0xDADADAAA, 0x99887766, 0xF0F1F2F3 };
volatile static const UObject* GDebugNoteObject;

class FBatchSaver
{
public:
	FBatchSaver(FCustomBindings& Customs, int32 NumReserve)
	: FlatCtx{{GUE.Types, GUE.Schemas, Customs}, Scratch, nullptr}
	, DeltaCtx{{GUE.Types, GUE.Schemas, Customs}, Scratch, &GUE.Defaults}
	{
		SavedObjects.Reserve(NumReserve);
	}

	FORCENOINLINE void Save(FBindId Id, const UObject* Object, const UObject* Arch)
	{
		FBuiltStruct* Built = Arch ? SaveStructDelta(Object, Arch, Id, DeltaCtx) : SaveStruct(Object, Id, FlatCtx);
		SavedObjects.Emplace(Id, Built, Object);
	}

	TArray64<uint8>	Write(TArray<FStructId>* OutRuntimeIds = nullptr) const
	{
		ESchemaFormat Format = OutRuntimeIds ? ESchemaFormat::InMemoryNames : ESchemaFormat::StableNames;

		// Build partial schemas
		const FStructBindIds BindIds(GUE.Customs, GUE.Schemas);
		FSchemasBuilder SchemaBuilders(GUE.Types, GUE.Names, BindIds, Scratch, Format);
		for (const FSavedObject& Object : SavedObjects)
		{
			GDebugNoteObject = Object.Input;
			SchemaBuilders.NoteStructAndMembers(Object.Id, *Object.Built);
		}
		GDebugNoteObject = nullptr;
		FBuiltSchemas Schemas = SchemaBuilders.Build();

		// Save schema ids on the side when using InMemoryNames
		if (OutRuntimeIds)
		{
			*OutRuntimeIds = ExtractRuntimeIds(Schemas);
		}

		FWriter Writer(GUE.Names, BindIds, Schemas, Format);
		TArray64<uint8> Out;

		// Write out FNames when using StableNames
		if (!OutRuntimeIds)
		{
			TArray<FName> UsedNames;
			UsedNames.Reserve(Writer.GetUsedNames().Num());
			for (FNameId Name : Writer.GetUsedNames())
			{
				UsedNames.Add(GUE.Names.ResolveName(Name));
			}

			WriteInt(Out, Magics[0]);
			WriteNumAndArray(Out, TArrayView<const FName, int32>(UsedNames));
		}

		// Write schemas
		WriteInt(Out, Magics[1]);
		WriteAlignmentPadding<uint32>(Out);
		TArray64<uint8> Tmp;
		Writer.WriteSchemas(/* Out */ Tmp);
		WriteNumAndArray(Out, TArrayView<const uint8, int64>(Tmp));
		Tmp.Reset();

		// Write objects
		WriteInt(Out, Magics[2]);
		for (const FSavedObject& Object : SavedObjects)
		{
			WriteInt(/* out */ Tmp, Magics[3]);
			WriteInt(/* out */ Tmp, Writer.GetWriteId(Object.Id).Get().Idx);
			Writer.WriteMembers(/* out */ Tmp, Object.Id, *Object.Built);
			WriteSkippableSlice(Out, Tmp);
			Tmp.Reset();
		}

		// Write object terminator
		WriteSkippableSlice(Out, TConstArrayView64<uint8>());
		WriteInt(Out, Magics[4]);
		
		return Out;
	}

private:
	struct FSavedObject
	{
		FBindId				Id;
		FBuiltStruct*		Built;
		const UObject*		Input; // For debug
	};

	TArray<FSavedObject>		SavedObjects;
	mutable FScratchAllocator	Scratch;
	FSaveContext				FlatCtx;
	FSaveContext				DeltaCtx;

	template<typename ArrayType>
	static void WriteNumAndArray(TArray64<uint8>& Out, const ArrayType& Items)
	{
		WriteInt(Out, IntCastChecked<uint32>(Items.Num()));
		WriteArray(Out, Items);
	}
};

class FMemoryBatchLoader
{
public:
	FMemoryBatchLoader(const FCustomBindings& Customs, FMemoryView Data, TConstArrayView<FStructId> RuntimeIds)
	{
		//// Read ids
		
		// Read and mount schemas
		FByteReader It(Data);
		check(It.Grab<uint32>() == Magics[1]);
		It.SkipAlignmentPadding<uint32>();
		uint32 SchemasSize = It.Grab<uint32>();
		const FSchemaBatch* SavedSchemas = ValidateSchemas(It.GrabSlice(SchemasSize));
		check(It.Grab<uint32>() == Magics[2]);
		
		FSchemaBatchId Batch = MountReadSchemas(SavedSchemas);

		// Read objects
 		while (uint64 NumBytes = It.GrabVarIntU())
		{	
			FByteReader ObjIt(It.GrabSlice(NumBytes));
			check(ObjIt.Grab<uint32>() == Magics[3]);
			FStructSchemaId Id = { ObjIt.Grab<uint32>() };
			Objects.Add({ { Id, Batch }, ObjIt });
		}
		
		check(It.Grab<uint32>() == Magics[4]);
		check(!Objects.IsEmpty());

		// Finally create load plans
		Plans = CreateLoadPlans(Batch, GUE.Types, Customs, GUE.Schemas, RuntimeIds, ESchemaFormat::InMemoryNames);
	}

	~FMemoryBatchLoader()
	{
		check(LoadIdx == Objects.Num()); // Test should load all saved objects
		Plans.Reset();
		UnmountReadSchemas(Objects[0].Schema.Batch);
	}

	FORCENOINLINE void Load(UObject* Dst)
	{
		FStructView In = Objects[LoadIdx];
		LoadStruct(Dst, In.Values, In.Schema.Id, *Plans);
		++LoadIdx;
	}

	FORCENOINLINE void Reload(UObject* Dst, int32 ReloadIdx)
	{
		FStructView In = Objects[ReloadIdx];
		LoadStruct(Dst, In.Values, In.Schema.Id, *Plans);
	}

private:
	FLoadBatchPtr				Plans;
	TArray<FStructView>			Objects;
	int32						LoadIdx = 0;
	
	template<typename T>
	static TConstArrayView<T> GrabNumAndArray(/* in-out */ FByteReader& It)
	{
		uint32 Num = It.Grab<uint32>();
		return MakeArrayView(reinterpret_cast<const T*>(It.GrabBytes(Num * sizeof(T))), Num);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

// Similar to PlainProps FMemoryBatch but for storing FArchive property serialization results
struct FArchivedProperties
{
	TArray<uint8>			Data;
	TArray<FText>			Texts;
};

static constexpr uint32 RoundtripPortFlags = PPF_UseDeprecatedProperties | PPF_ForceTaggedSerialization;

// Match FMemoryPropertyBatch somewhat for a fair comparison, e.g. save FText on side and FName as integer
class FPropertyWriter final : public FMemoryWriter
{
	TArray<FText>& Texts;
public:
	FPropertyWriter(FArchivedProperties& Out) 
	: FMemoryWriter(Out.Data)
	, Texts(Out.Texts)
	{
		SetPortFlags(RoundtripPortFlags);
	}

	FORCENOINLINE void WriteProperties(UObject* Object, UObject* Defaults)
	{
		UClass* Class = Object->GetClass();
		Class->SerializeTaggedProperties(*this, reinterpret_cast<uint8*>(Object), Class, reinterpret_cast<uint8*>(Defaults));
	}

	virtual FArchive& operator<<(FText& Value) override
	{
		int32 Idx = INDEX_NONE;
		if (!Value.IsEmpty())
		{
			Idx = Texts.Num();
			Texts.Add(Value);
		}
		return WriteValue(Idx);
	}
	virtual FArchive& operator<<(FName& Value) override				{ return WriteValue(ToInt(Value));}
	virtual FArchive& operator<<(UObject*& Value) override			{ return WriteValue(reinterpret_cast<uint64&>(Value)); }
	virtual FArchive& operator<<(FObjectPtr& Value) override		{ return WriteValue(reinterpret_cast<uint64&>(Value)); }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override	{ return WriteValue(reinterpret_cast<uint64&>(Value)); }
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override	{ return WriteValue(Value.GetUniqueID()); }
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override	{ Value.GetUniqueID().SerializePath(*this); return *this; }
	virtual FArchive& operator<<(FSoftObjectPath& Value) override	{ Value.SerializePath(*this); return *this; }
	virtual FString GetArchiveName() const override					{ return "FPropertyWriter"; }

	template<typename T>
	FArchive& WriteValue(T Value)
	{
		return *this << Value;
	}
};

class FPropertyReader final : public FMemoryReader
{
	const TArray<FText>& Texts;
public:
	FPropertyReader(const FArchivedProperties& Out)
	: FMemoryReader(Out.Data)
	, Texts(Out.Texts)
	{
		SetPortFlags(RoundtripPortFlags);
	}

	FORCENOINLINE void ReadProperties(UObject* Object, UObject* Defaults)
	{
		UClass* Class = Object->GetClass();
		Class->SerializeTaggedProperties(*this, reinterpret_cast<uint8*>(Object), Class, reinterpret_cast<uint8*>(Defaults));
	}

	virtual FArchive& operator<<(FText& Value) override
	{ 
		int32 Idx = ReadValue<int32>();
		Value = Idx == INDEX_NONE ? FText::GetEmpty() : Texts[Idx];
		return *this;
	}

	virtual FArchive& operator<<(FName& Value) override				{ Value = FromInt(ReadValue<decltype(ToInt(FName()))>()); return *this; }
	virtual FArchive& operator<<(UObject*& Value) override			{ reinterpret_cast<uint64&>(Value) = ReadValue<uint64>(); return *this; }
	virtual FArchive& operator<<(FObjectPtr& Value) override		{ reinterpret_cast<uint64&>(Value) = ReadValue<uint64>(); return *this; }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override	{ reinterpret_cast<uint64&>(Value) = ReadValue<uint64>(); return *this; }
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override	{ Value.ResetWeakPtr(); Value.GetUniqueID() = ReadValue<FUniqueObjectGuid>(); return *this; }
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override	{ Value.ResetWeakPtr(); Value.GetUniqueID().SerializePath(*this); return *this; }
	virtual FArchive& operator<<(FSoftObjectPath& Value) override	{ Value.SerializePath(*this); return *this; }
	virtual FString GetArchiveName() const override					{ return "FPropertyReader"; }

	template<typename T>
	T ReadValue()
	{
		T Out;
		*this << Out;
		return Out;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FInstance
{
	FString PathName;
	UObject* Orig;
	UObject* Arch;
	UObject* Base;
	UObject* PP;
	UObject* TPS;
	UObject* UPS;
	FBindId Id;

	void Init()
	{
		UClass* Class = Orig->GetClass();
		Class->GetDefaultObject(/* create lazily */ true);
		Arch = Orig->GetArchetype();
		check(Arch != Orig);
		Id = GUE.Names.IndexBindId(IndexType(Class));
	}
};

static UObject* MakeEmptyInstance(UObject* Obj, FName Name)
{
	FStaticConstructObjectParameters Params(Obj->GetClass());
	Params.Outer = Obj->GetOuter();//GetTransientPackage();
	Params.Name = Name;
	Params.SetFlags = Obj->GetFlags();
	Params.Template = Obj->GetArchetype();
	Params.bAssumeTemplateIsArchetype = true;
	Params.bCopyTransientsFromClassDefaults = true;
	return StaticConstructObject_Internal(Params);
}

static bool IncludeClass(UClass* Class)
{
	static const FName Exclusions[] = {
		"CitySampleUnrealEdEngine", // Cloning MTAccessDetector crash
		"GameFeaturePluginStateMachine", // Cloning ensure
		"WorldSettings", // QAGame
		// CitySample - Enum value 8 is undeclared in /Script/Engine.ERichCurveTangentMode, illegal value detected in /Script/Engine.RichCurveKey::TangentMode
		"AnimSequence", "AnimationSequencerDataModel", "MovieSceneControlRigParameterSection"
	};
	static const FName SuperExclusions[] = { "LevelScriptActor" };

	if (!ShouldBind(Class) || !!Algo::Find(Exclusions, Class->GetFName()))
	{
		return false;
	}

	// Exclude IDOs
	static constexpr EClassFlags IdoFlags = CLASS_NotPlaceable | CLASS_Hidden | CLASS_HideDropDown;
	if (Class->HasAllClassFlags(IdoFlags))
	{
		return false;
	}

	for (UStruct* Super = Class->GetInheritanceSuper(); Super; Super = Super->GetInheritanceSuper())
	{
		if (!!Algo::Find(SuperExclusions, Super->GetFName()))
		{
			return false;
		}
	}
	return true;
}

FORCENOINLINE static void SavePlainProps(FBatchSaver& Batch, TConstArrayView<FInstance> Instances)
{
	for (const FInstance& Instance : Instances)
	{	
		Batch.Save(Instance.Id, Instance.Orig, Instance.Base);
	}
}

FORCENOINLINE static void LoadPlainProps(FMemoryBatchLoader& Batch, TConstArrayView<FInstance> Instances)
{
	for (const FInstance& Instance : Instances)
	{	
		Batch.Load(Instance.PP);
	}
}

template<bool bUps>
FORCENOINLINE void SaveArchive(FPropertyWriter& Archive, TConstArrayView<FInstance> Instances)
{
	Archive.SetUseUnversionedPropertySerialization(bUps);
	for (const FInstance& Instance : Instances)
	{
		Archive.WriteProperties(Instance.Orig, Instance.Base);
	}
}

template<bool bUps>
FORCENOINLINE void LoadArchive(FPropertyReader& Archive, TConstArrayView<FInstance> Instances)
{
	Archive.SetUseUnversionedPropertySerialization(bUps);
	for (const FInstance& Instance : Instances)
	{
		Archive.ReadProperties(bUps ? Instance.UPS : Instance.TPS, Instance.Base);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

class FStableNameBatchIds final : public FStableBatchIds
{
	TConstArrayView<FName> Names;
public:
	FStableNameBatchIds(FSchemaBatchId Batch, TConstArrayView<FName> InNames) : FStableBatchIds(Batch), Names(InNames) {}
	using FStableBatchIds::AppendString;

	virtual uint32				NumNames() const override							{ return static_cast<uint32>(Names.Num()); }
	virtual void				AppendString(FUtf8Builder& Out, FNameId Name) const override { Names[Name.Idx].AppendString(Out); }
};

[[nodiscard]] static FSchemaBatchId ParseBatch(
	TArray64<uint8>& OutData,
	TArray<FStructView>& OutObjects,
	FUtf8StringView YamlView)
{
	// Parse yaml
	ParseYamlBatch(OutData, YamlView);

	// Grab and mount parsed schemas
	FByteReader It(MakeMemoryView(OutData));
	const uint32 SchemasSize = It.Grab<uint32>();
	FMemoryView SchemasView = It.GrabSlice(SchemasSize);
	const FSchemaBatch* Schemas = ValidateSchemas(SchemasView);
	FSchemaBatchId Batch = MountReadSchemas(Schemas);

	// Grab parsed objects
	while (uint64 NumBytes = It.GrabVarIntU())
	{	
		FByteReader ObjIt(It.GrabSlice(NumBytes));
		FStructSchemaId Schema = { ObjIt.Grab<uint32>() };
		OutObjects.Add({ { Schema, Batch }, ObjIt });
	}
	
	return Batch;
}

static void RoundtripText(
	const FBatchIds& BatchIds,
	TConstArrayView<FStructView> Objects,
	TConstArrayView<FInstance> Instances,
	ESchemaFormat Format)
{
	check(Objects.Num() == Instances.Num());

	// Print yaml
	UE_LOGFMT(LogPlainPropsUObject, Display, "Printing to PlainProps text using {Format}...", ToString(Format));
	TUtf8StringBuilder<256> Yaml;
	Yaml.Reserve(INT_MAX);
	PrintYamlBatch(Yaml, BatchIds, Objects);
	FUtf8StringView YamlView = Yaml.ToView();

	// Write to file
	const FString Filename = FPaths::ProjectSavedDir() / TEXT("PlainProps") /
		(Format == ESchemaFormat::InMemoryNames ? TEXT("InMemoryNames.yaml") : TEXT("StableNames.yaml"));
	if (TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(*Filename)); FileWriter)
	{
		UE_LOGFMT(LogPlainPropsUObject, Display, "Writing {KB}KB yaml as {Filename}...", Yaml.Len() >> 10, *Filename);
		FileWriter->Serialize((void*)YamlView.GetData(), YamlView.Len());
	}

	// Parse yaml
	UE_LOGFMT(LogPlainPropsUObject, Display, "Parsing PlainProps text using {Format}...", ToString(Format));
	TArray64<uint8> Data;
	TArray<FStructView> ParsedObjects;
	FSchemaBatchId ParsedBatch = ParseBatch(Data, ParsedObjects, YamlView);

	if (Format == ESchemaFormat::StableNames)
	{
		UE_LOGFMT(LogPlainPropsUObject, Display, "Diffing PlainProps parsed objects using {Format}...", ToString(Format));

		// Diff schemas
		check(!DiffSchemas(BatchIds.GetBatchId(), ParsedBatch));

		// Diff objects
		check(Objects.Num() == ParsedObjects.Num());
		const int32 NumObjects = FMath::Min(Objects.Num(), ParsedObjects.Num());
		uint32 NumDiffs = 0;
		TUtf8StringBuilder<256> Diffs;
		for (int32 I = 0; I < NumObjects; ++I)
		{
			FStructView In = Objects[I];
			FStructView Parsed = ParsedObjects[I];
			FReadDiffPath DiffPath;
			if (DiffStruct(In, Parsed, DiffPath))
			{
				PrintDiff(Diffs, BatchIds, DiffPath);
				Diffs.Append(" in ");
				Diffs.Append(Instances[I].PathName);
				Diffs.Append("\n");
				++NumDiffs;
			}
		}
		UE_LOGFMT(LogPlainPropsUObject, Display,
			"Detected {Diffs} diffs in {Objs} PlainProps parsed objects from {KB}KB yaml text using StableNames\n{Diffs}",
			NumDiffs, NumObjects, Yaml.Len() >> 10, Diffs.ToString());
	}

	// Unmount parsed schemas
	UnmountReadSchemas(ParsedBatch);
}

class FBatchTextRoundtripper
{
public:
	FBatchTextRoundtripper(FMemoryView Data, ESchemaFormat InFormat) : Format(InFormat)
	{
		FByteReader It(Data);

		// Read FNames when using Stable Names
		TConstArrayView<FName> Names;
		if (Format == ESchemaFormat::StableNames)
		{
			verify(It.Grab<uint32>() == Magics[0]);
			Names = GrabNumAndArray<FName>(It);
		}
		
		// Read and mount schemas
		check(It.Grab<uint32>() == Magics[1]);
		It.SkipAlignmentPadding<uint32>();
		const uint32 SchemasSize = It.Grab<uint32>();
		FMemoryView SavedSchemasView = It.GrabSlice(SchemasSize);
		const FSchemaBatch* SavedSchemas = ValidateSchemas(SavedSchemasView);
		check(It.Grab<uint32>() == Magics[2]);
		FSchemaBatchId Batch = MountReadSchemas(SavedSchemas);
		
		// Read objects
		while (uint64 NumBytes = It.GrabVarIntU())
		{	
			FByteReader ObjIt(It.GrabSlice(NumBytes));
			check(ObjIt.Grab<uint32>() == Magics[3]);
			FStructSchemaId Id = { ObjIt.Grab<uint32>() };
			Objects.Add({ { Id, Batch }, ObjIt });
		}
		
		check(It.Grab<uint32>() == Magics[4]);
		check(!Objects.IsEmpty());

		// Create BatchIds 
		if (Format == ESchemaFormat::StableNames)
		{
			BatchIds = MakeUnique<FStableNameBatchIds>(Batch, Names);
		}
		else
		{
			BatchIds = MakeUnique<FMemoryBatchIds>(Batch, GUE.Names);
		}
	}

	~FBatchTextRoundtripper()
	{
		UnmountReadSchemas(BatchIds->GetBatchId());
	}

	void RoundtripText(TConstArrayView<FInstance> Instances) const
	{
		PlainProps::UE::RoundtripText(*BatchIds, Objects, Instances, Format);
	}

private:
	TArray<FStructView>			Objects;
	TUniquePtr<FBatchIds>		BatchIds;
	ESchemaFormat				Format;
	
	template<typename T>
	static TConstArrayView<T> GrabNumAndArray(/* in-out */ FByteReader& It)
	{
		uint32 Num = It.Grab<uint32>();
		return MakeArrayView(reinterpret_cast<const T*>(It.GrabBytes(Num * sizeof(T))), Num);
	}
};

////////////////////////////////////////////////////////////////////////////////

struct FDiffDebug
{
	volatile FInstance* Last;
	volatile const UTF8CHAR* Str;
};
static FDiffDebug GPpDiff, GTpsDiff, GUpsDiff;

static int32 Roundtrip(ERoundtrip Options)
{
	UE_LOGFMT(LogPlainPropsUObject, Display, "Gathering all non-empty UObjects...");
	static constexpr EObjectFlags SkipFlags = RF_ClassDefaultObject | RF_MirroredGarbage | RF_InheritableComponentTemplate;
	TArray<FInstance> Instances;
	for (TObjectIterator<UObject> It(SkipFlags); It; ++It)
	{
		UObject* Object = *It;
		if (IncludeClass(Object->GetClass()))
		{
			Instances.Emplace(Object->GetPathName(), Object);
		}
	}
	UE_LOGFMT(LogPlainPropsUObject, Display, "Sorting {Num} UObjects ...", Instances.Num());
	Algo::Sort(Instances, [](const FInstance& A, const FInstance& B)
	{
		return FPlatformString::Strcmp(*A.PathName, *B.PathName) < 0;
	});

	// Create CDOs if needed and then clones for PP and TPS tests
	UE_LOGFMT(LogPlainPropsUObject, Display, "Cloning {Num} UObjects up to 4 times...", Instances.Num());
	for (FInstance& Instance : Instances)
	{
		Instance.Init();
	}
	FlushAsyncLoading();

	for (FInstance& Instance : Instances)
	{
		uint32 N = 1 + &Instance - Instances.GetData();
		Instance.Base = Instance.Arch ? MakeEmptyInstance(Instance.Orig, FName("Base", N)) : nullptr;
		if (EnumHasAnyFlags(Options, ERoundtrip::PP))
		{
			Instance.PP = MakeEmptyInstance(Instance.Orig, FName("PP", N));
		}
		if (EnumHasAnyFlags(Options, ERoundtrip::TPS))
		{
			Instance.TPS = MakeEmptyInstance(Instance.Orig, FName("TPS", N));
		}
		if (EnumHasAnyFlags(Options, ERoundtrip::UPS))
		{
			Instance.UPS = MakeEmptyInstance(Instance.Orig, FName("UPS", N));
		}
	}

	// Save
	UE_LOGFMT(LogPlainPropsUObject, Display, "Saving UObjects to PlainProps with InMemoryNames...");
	FMemoryBatch Plain;
	TCustomPropertyBindings<FMemoryPropertyBatch> Customs(Plain.Properties, GUE.Customs);
	{
		FBatchSaver Batch(Customs.Overlay, GUObjectArray.GetObjectArrayNum());
		SavePlainProps(Batch, Instances);
		Plain.Data = Batch.Write(/* out */ &Plain.RuntimeIds);

		if (EnumHasAnyFlags(Options, ERoundtrip::TextMemory))
		{
			FBatchTextRoundtripper MemoryBatch(MakeMemoryView(Plain.Data), ESchemaFormat::InMemoryNames);
			MemoryBatch.RoundtripText(Instances);
		}
		if (EnumHasAnyFlags(Options, ERoundtrip::TextStable))
		{
			UE_LOGFMT(LogPlainPropsUObject, Display, "Saving UObjects to PlainProps with StableNames...");
			TArray64<uint8> StableData = Batch.Write();

			FBatchTextRoundtripper StableBatch(MakeMemoryView(StableData), ESchemaFormat::StableNames);
			StableBatch.RoundtripText(Instances);
		}
	}

	// Load
	uint32 NumPpDiffs = 0;
	if (EnumHasAnyFlags(Options, ERoundtrip::PP))
	{
		UE_LOGFMT(LogPlainPropsUObject, Display, "Loading UObjects from PlainProps...");
		FMemoryBatchLoader Batch(Customs.Overlay, MakeMemoryView(Plain.Data), Plain.RuntimeIds);
		LoadPlainProps(Batch, Instances);

		// Diff original vs PlainProps
		UE_LOGFMT(LogPlainPropsUObject, Display, "Diffing UObjects roundtripped via PlainProps...");
		FDiffContext DiffCtx = {{ GUE.Types, GUE.Schemas, Customs.Overlay }};
		TUtf8StringBuilder<256> PpDiffs;
		for (FInstance& Instance : Instances)
		{
			if (DiffStructs(Instance.Orig, Instance.PP, Instance.Id, /* in-out */ DiffCtx))
			{
				GPpDiff.Last = &Instance;
				PrintDiff(/* out */ PpDiffs, GUE.Names, DiffCtx.Out);
				//Batch.Reload(Instance.UPS, &Instance - Instances.GetData());
				DiffCtx.Out.Reset();
				PpDiffs.Append(" in ");
				PpDiffs.Append(Instance.Orig->GetFullName());
				PpDiffs.Append("\n");
				++NumPpDiffs;
			}
		}
		GPpDiff.Str = PpDiffs.GetData();
		UE_LOGFMT(LogPlainPropsUObject, Display, "Detected {Diffs} diffs in {Objs} UObjects saved in a {KB}KB value stream using PlainProps\n{DiffText}", NumPpDiffs, Instances.Num(), Plain.Data.NumBytes() / 1024, PpDiffs.ToString());
	}

	FArchivedProperties Tps;
	if (EnumHasAnyFlags(Options, ERoundtrip::TPS))
	{
		UE_LOGFMT(LogPlainPropsUObject, Display, "Saving UObjects to TPS archive...");
		FPropertyWriter Archive(/* out */ Tps);
		SaveArchive<false>(Archive, Instances);
	}

	if (EnumHasAnyFlags(Options, ERoundtrip::TPS))
	{
		UE_LOGFMT(LogPlainPropsUObject, Display, "Loading UObjects from TPS archive...");
		FPropertyReader Archive(Tps);
		LoadArchive<false>(Archive, Instances);
	}

	static const FName SkipClasses[] = {
		"BodySetup", // Skips some structs due to native FCollisionResponse::operator==
		"NiagaraScript", "NiagaraNodeFunctionCall", "NiagaraMeshRendererProperties", // FNiagaraTypeDefinition::Serialize resets ClassStructOrEnum
	};

	if (EnumHasAnyFlags(Options, ERoundtrip::TPS))
	{
		// Diff original vs TPS
		UE_LOGFMT(LogPlainPropsUObject, Display, "Diffing UObjects roundtripped via TPS...");
		FDiffContext DiffCtx = {{ GUE.Types, GUE.Schemas, Customs.Overlay }};
		TUtf8StringBuilder<256> TpsDiffs;
		TArray<int32> TpsDiffIdxs;
		for (FInstance& Instance : Instances)
		{
			if (Algo::Find(SkipClasses, Instance.Orig->GetClass()->GetFName()))
			{
				continue;
			}

			if (DiffStructs(Instance.Orig, Instance.TPS, Instance.Id, /* in-out */ DiffCtx))
			{
				GTpsDiff.Last = &Instance;
				PrintDiff(/* out */ TpsDiffs, GUE.Names, DiffCtx.Out);
				
				DiffCtx.Out.Reset();
				TpsDiffs.Append(" in ");
				TpsDiffs.Append(Instance.Orig->GetFullName());
				TpsDiffs.Append("\n");	
				TpsDiffIdxs.Add(&Instance - &Instances[0]);
				
				//FArchivedProperties Tmp;
				//FPropertyWriter(/* out */ Tmp).WriteProperties(Instance.Orig, Instance.Base);
				//FPropertyReader(Tmp).ReadProperties(Instance.UPS, Instance.Base);

				//FDiffContext TmpCtx = DiffCtx;
				//bool bOU = DiffStructs(Instance.Id, Instance.Orig, Instance.UPS, /* in-out */ TmpCtx);
				//bool bOT = DiffStructs(Instance.Id, Instance.Orig, Instance.TPS, /* in-out */ TmpCtx);
				//bool bOP = DiffStructs(Instance.Id, Instance.Orig, Instance.PP, /* in-out */ TmpCtx);
				//TUtf8StringBuilder<256> TmpDiff;
				//PrintDiff(/* out */ TmpDiff, TmpCtx.Out, GUE.Names);
			}
		}
		GTpsDiff.Str = TpsDiffs.GetData();
		UE_LOGFMT(LogPlainPropsUObject, Display, "Detected {Diffs} diffs in {Objs} UObjects saved in a {KB}KB value stream using TPS", TpsDiffIdxs.Num(), Instances.Num(), Tps.Data.NumBytes() / 1024);
	}


	if (EnumHasAnyFlags(Options, ERoundtrip::UPS))
	{
		FArchivedProperties Ups;
		UE_LOGFMT(LogPlainPropsUObject, Display, "Saving UObjects to UPS archive...");
		{
			FPropertyWriter Archive(/* out */ Ups);
			SaveArchive<true>(Archive, Instances);
		}

		UE_LOGFMT(LogPlainPropsUObject, Display, "Loading UObjects from UPS archive...");
		{
			FPropertyReader Archive(Ups);
			LoadArchive<true>(Archive, Instances);
		}

		// Diff original vs UPS
		UE_LOGFMT(LogPlainPropsUObject, Display, "Diffing UObjects roundtripped via UPS...");
		FDiffContext DiffCtx = {{ GUE.Types, GUE.Schemas, Customs.Overlay }};
		TUtf8StringBuilder<256> UpsDiffs;
		TArray<int32> UpsDiffIdxs;
		for (FInstance& Instance : Instances)
		{
			if (Algo::Find(SkipClasses, Instance.Orig->GetClass()->GetFName()))
			{
				continue;
			}

			if (DiffStructs(Instance.Orig, Instance.UPS, Instance.Id, /* in-out */ DiffCtx))
			{
				GUpsDiff.Last = &Instance;
				PrintDiff(/* out */ UpsDiffs, GUE.Names, DiffCtx.Out);
				
				DiffCtx.Out.Reset();
				UpsDiffs.Append(" in ");
				UpsDiffs.Append(Instance.Orig->GetFullName());
				UpsDiffs.Append("\n");	
				UpsDiffIdxs.Add(&Instance - &Instances[0]);
			}
		}
		GUpsDiff.Str = UpsDiffs.GetData();
		UE_LOGFMT(LogPlainPropsUObject, Display, "Detected {Diffs} diffs in {Objs} UObjects saved in a {KB}KB value stream using UPS", UpsDiffIdxs.Num(), Instances.Num(), Ups.Data.NumBytes() / 1024);
	}

	return NumPpDiffs;
}

static int32 TestBindings(ERoundtrip Options)
{
	TScopedStructBinding<FTransform, FDefaultRuntime> Transform;
	TScopedStructBinding<FGuid, FDefaultRuntime> Guid;
	TScopedStructBinding<FColor, FDefaultRuntime> Color;
	TScopedStructBinding<FLinearColor, FDefaultRuntime> LinearColor;
	TScopedStructBinding<FFieldPath, FDefaultRuntime> FieldPath(GUE.Structs.FieldPath);
	TScopedStructBinding<FScriptDelegate, FDefaultRuntime> Delegate(GUE.Structs.Delegate);
	// MulticastDelegate declaration is shared with MulticastSparseDelegate
	TScopedStructBinding<FMulticastScriptDelegate, FDefaultRuntime> InlineMulticast({GUE.Structs.MulticastInlineDelegate, GUE.Structs.MulticastDelegate});
	// Verse
	TScopedStructBinding<FVerseFunction, FDefaultRuntime> VerseFunction(GUE.Structs.VerseFunction);
	TScopedStructBinding<::UE::FDynamicallyTypedValue, FDefaultRuntime> DynamicallyTypedValue(GUE.Structs.DynamicallyTypedValue);
	TScopedStructBinding<FReferencePropertyValue, FDefaultRuntime> ReferencePropertyValue(GUE.Structs.ReferencePropertyValue);

	GUE.Defaults.BindZeroes(GUE.Structs.FieldPath, sizeof(FFieldPath), alignof(FFieldPath));

	InitBatchedProperties();
	BindInitialTypes();
	return Roundtrip(Options);
}

} // namespace PlainProps::UE

#include "PlainPropsCommandlets.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"

UTestPlainPropsCommandlet::UTestPlainPropsCommandlet(const FObjectInitializer& Init)
: Super(Init)
{}

int32 UTestPlainPropsCommandlet::Main(const FString& Params)
{
	using namespace PlainProps;
	using namespace PlainProps::UE;
	DbgVis::FIdScope _(GUE.Names, "FName");

	if (int32 LoadIdx = Params.Find("-load="); LoadIdx != INDEX_NONE)
	{
		// E.g. -run=TestPlainProps -load=/BRRoot/BRRoot.BRRoot,/Game/Maps/FrontEnd.FrontEnd

		const TCHAR* It = &Params[LoadIdx + 6];
		FStringView Assets(It, FAsciiSet::FindFirstOrEnd(It, FAsciiSet(" ")) - It);
		UE_LOGFMT(LogPlainPropsUObject, Display, "Loading {Assets}...", Assets);

		int32 CommaIndex;
		while (Assets.FindChar(',', CommaIndex))
		{
			FSoftObjectPath(Assets.Left(CommaIndex)).LoadAsync({});
			Assets.RightChopInline(CommaIndex + 1);
		}

		FSoftObjectPath(Assets).LoadAsync({});
	}
	else if (Params.Find("-loadmaps") != INDEX_NONE)
	{
		// load all .umaps in asset registry
		UE_LOGFMT(LogPlainPropsUObject, Display, "Loading asset registry...");
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		AssetRegistryModule.Get().SearchAllAssets(true);

		UE_LOGFMT(LogPlainPropsUObject, Display, "Gathering all maps...");
		TArray<FAssetData> Maps;
		AssetRegistryModule.Get().GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(), /* out */ Maps, true);

		UE_LOGFMT(LogPlainPropsUObject, Display, "Loading all {Maps} maps...", Maps.Num());
		for (FAssetData& Map : Maps)
		{
			Map.GetSoftObjectPath().LoadAsync({});
		}
	}

	FlushAsyncLoading();

	UE_LOGFMT(LogPlainPropsUObject, Display, "Starting test...");

	ERoundtrip Options = ERoundtrip::PP | ERoundtrip::UPS | ERoundtrip::TPS | ERoundtrip::TextMemory;
	if (Params.Find("-pp") != INDEX_NONE)
	{
		Options = ERoundtrip::PP | ERoundtrip::TextMemory;
	}
	else if (Params.Find("-text") != INDEX_NONE)
	{
		Options = ERoundtrip::TextMemory | ERoundtrip::TextStable;
	}
	else if (Params.Find("-notext") != INDEX_NONE)
	{
		EnumRemoveFlags(Options, ERoundtrip::TextMemory | ERoundtrip::TextStable);
	}
	return TestBindings(Options);
}


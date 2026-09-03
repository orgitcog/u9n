// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/Transform.h"
#include "Misc/Optional.h"
#include "PlainPropsBind.h"
#include "PlainPropsIndex.h"
#include "PlainPropsLoadMember.h"
#include "PlainPropsSaveMember.h"
#include "PlainPropsStringUtil.h"
#include "UObject/NameTypes.h"

namespace Verse { class FNativeString; }
using FVerseString = Verse::FNativeString;

PP_REFLECT_STRUCT_TEMPLATE(, TTuple, void, Key, Value); // Todo handle TTuple and higher arities

namespace UE::Math
{
PP_REFLECT_STRUCT(, FVector, void, X, Y, Z);
PP_REFLECT_STRUCT(, FVector4, void, X, Y, Z, W);
PP_REFLECT_STRUCT(, FQuat, void, X, Y, Z, W);
}

template <typename T>
struct TIsContiguousContainer<PlainProps::TRangeView<T>>
{
	static inline constexpr bool Value = true;
};

namespace PlainProps::UE
{

//class FReflection
//{
//	TIdIndexer<FName>	Names;
//	FRuntime			Types;
//
//public:
//	TIdIndexer<FName>&		GetIds() { return Names; }

	//template<typename Ctti>
	//FStructSchemaId			BindStruct();

	//template<typename Ctti>
	//FStructSchemaId			BindStructInterlaced(TConstArrayView<FMemberBinding> NonCttiMembers);
	//FStructSchemaId			BindStruct(FStructSchemaId Id, const ICustomBinding& Custom);
	//FStructSchemaId			BindStruct(FType Type, FOptionalSchemaId Super, TConstArrayView<FNamedMemberBinding> Members, EMemberPresence Occupancy);
	//void					DropStruct(FStructSchemaId Id) { Types.DropStruct(Id); }

	//template<typename Ctti>
	//FEnumSchemaId			BindEnum();
	//FEnumSchemaId			BindEnum(FType Type, EEnumMode Mode, ELeafWidth Width, TConstArrayView<FEnumerator> Enumerators);
	//void					DropEnum(FEnumSchemaId Id) { Types.DropEnum(Id); }
//};
//
//PLAINPROPS_API FReflection GReflection;
//
//struct FIds
//{
//	static FMemberId		IndexMember(FAnsiStringView Name)			{ return GReflection.GetIds().NameMember(FName(Name)); }
//	static FTypenameId		IndexTypename(FAnsiStringView Name)			{ return GReflection.GetIds().MakeTypename(FName(Name)); }
//	static FScopeId			IndexCurrentModule()						{ return GReflection.GetIds().MakeScope(FName(UE_MODULE_NAME)); }
//	static FType			IndexNativeType(FAnsiStringView Typename)	{ return {IndexCurrentModule(), IndexTypename(Typename)}; }
//	static FEnumSchemaId	IndexEnum(FType Name)						;
//	static FEnumSchemaId	IndexEnum(FAnsiStringView Name)				;
//	static FStructSchemaId	IndexStruct(FType Name)					;
//	static FStructSchemaId	IndexStruct(FAnsiStringView Name)			;
//	
//};

// todo: use generic cached instance template?
//template<class Ids>
//FScopeId GetModuleScope()
//{
//	static FScopeId Id = Ids::IndexScope(UE_MODULE_NAME);
//	return Id;
//}

//template<class Ctti>
//class TBindRtti
//{
//	FSchemaId Id;
//public:
//	TBindRtti() : Id(BindRtti<Ctti, FIds>(GReflection.GetTypes()))
//	{}
//
//	~TBindRtti()
//	{
//		if constexpr (std::is_enum_v<Ctti::Type>)
//		{
//			GReflection.DropEnum(static_cast<FEnumSchemaId>(Id));
//		}
//		else
//		{
//			GReflection.DropStruct(static_cast<FStructSchemaId>(Id));
//		}
//	}
//};

} // namespace PlainProps::UE

//#define UEPP_BIND_STRUCT(T) 
	
//////////////////////////////////////////////////////////////////////////
// Below container bindings should be moved to some suitable header
//////////////////////////////////////////////////////////////////////////

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Templates/UniquePtr.h"

namespace PlainProps::UE
{

template <typename T, class Allocator>
struct TArrayBinding : IItemRangeBinding
{
	using SizeType = int32;
	using ItemType = T;
	using ArrayType = TArray<T, Allocator>;
	using IItemRangeBinding::IItemRangeBinding;
	inline static constexpr std::string_view BindName = TTypename<ArrayType>::RangeBindName;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		ArrayType& Array = Ctx.Request.GetRange<ArrayType>();
		if constexpr (std::is_default_constructible_v<T>)
		{
			Array.SetNum(Ctx.Request.NumTotal());
		}
		else
		{
			Array.SetNumUninitialized(Ctx.Request.NumTotal());
			Ctx.Items.SetUnconstructed();
		}
		
		Ctx.Items.Set(Array.GetData(), Ctx.Request.NumTotal());
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		const ArrayType& Array = Ctx.Request.GetRange<ArrayType>();
		Ctx.Items.SetAll(Array.GetData(), static_cast<uint64>(Array.Num()));
	}
};

//////////////////////////////////////////////////////////////////////////

template<class StringType>
struct TStringBinding : ILeafRangeBinding
{
	using SizeType = int32;
	using ItemType = char8_t;
	using CharType = typename StringType::ElementType;
	using ILeafRangeBinding::ILeafRangeBinding;
	inline static constexpr std::string_view BindName = TTypename<StringType>::RangeBindName;
	
	virtual void SaveLeaves(const void* Range, FLeafRangeAllocator& Out) const override
	{
		const TArray<CharType>& SrcArray = static_cast<const StringType*>(Range)->GetCharArray();
		const CharType* Src = SrcArray.GetData();
		int32 SrcLen = SrcArray.Num() - 1;
		if (SrcLen <= 0)
		{
		}
		else if constexpr (sizeof(CharType) == sizeof(char8_t))
		{
			char8_t* Utf8 = Out.AllocateRange<char8_t>(SrcLen);
			FMemory::Memcpy(Utf8, Src, SrcLen);
		}
		else
		{
			int32 Utf8Len = FPlatformString::ConvertedLength<UTF8CHAR>(Src, SrcLen);
			char8_t* Utf8 = Out.AllocateRange<char8_t>(Utf8Len);
			if (Utf8Len == SrcLen)
			{
				for (int32 Idx = 0; Idx < SrcLen; ++Idx)
				{
					Utf8[Idx] = static_cast<char8_t>(Src[Idx]);
				}
			}
			else
			{
				UTF8CHAR* Utf8End = FPlatformString::Convert(reinterpret_cast<UTF8CHAR*>(Utf8), Utf8Len, Src, SrcLen);	
				check((char8_t*)Utf8End - Utf8 == Utf8Len);
			}
		}
	}

	virtual void LoadLeaves(void* Range, FLeafRangeLoadView Items) const override
	{
		TArray<CharType>& Dst = static_cast<StringType*>(Range)->GetCharArray();
		TRangeView<char8_t> Utf8 = Items.As<char8_t>();
		// Todo: Better abstraction that hides internal representation
		const UTF8CHAR* Src = reinterpret_cast<const UTF8CHAR*>(Utf8.begin());
		int32 SrcLen = static_cast<int32>(Utf8.Num());
		if (SrcLen == 0)
		{
			Dst.Reset();
		}
		else if constexpr (sizeof(CharType) == sizeof(char8_t))
		{
			Dst.SetNumUninitialized(SrcLen + 1);
			FMemory::Memcpy(Dst.GetData(), Src, SrcLen);
			Dst[SrcLen] = CharType('\0');	
		}
		else
		{
			int32 DstLen = FPlatformString::ConvertedLength<CharType>(Src, SrcLen);
			Dst.SetNumUninitialized(DstLen + 1);
			if (DstLen == SrcLen)
			{
				CharType* Out = Dst.GetData();
				for (int32 Idx = 0; Idx < SrcLen; ++Idx)
				{
					Out[Idx] = static_cast<CharType>(Src[Idx]);
				}
				Out[SrcLen] = '\0';
			}
			else
			{
				CharType* DstEnd = FPlatformString::Convert(Dst.GetData(), DstLen, Src, SrcLen);
				check(DstEnd - Dst.GetData() == DstLen);
				*DstEnd = '\0';
			}
		}
	}

	virtual bool DiffLeaves(const void* RangeA, const void* RangeB) const override
	{
		const StringType& A = *static_cast<const StringType*>(RangeA);
		const StringType& B = *static_cast<const StringType*>(RangeB);
		// Case-sensitive unnormalized comparison
		return Diff(A.Len(), B.Len(), GetData(A), GetData(B), sizeof(CharType));
	}
};

//////////////////////////////////////////////////////////////////////////

template <typename T>
struct TUniquePtrBinding : IItemRangeBinding
{
	using SizeType = bool;
	using ItemType = T;
	using IItemRangeBinding::IItemRangeBinding;
	inline static constexpr std::string_view BindName = "UniquePtr";

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		TUniquePtr<T>& Ptr = Ctx.Request.GetRange<TUniquePtr<T>>();
		
		if (Ctx.Request.NumTotal() == 0)
		{
			Ptr.Reset();
			return;
		}
		
		if (!Ptr)
		{
			if constexpr (std::is_default_constructible_v<T>)
			{
				Ptr.Reset(new T);
			}
			else
			{
				Ptr.Reset(reinterpret_cast<T*>(FMemory::Malloc(sizeof(T), alignof(T))));
				Ctx.Items.SetUnconstructed();
			}
		}
		
		Ctx.Items.Set(Ptr.Get(), 1);
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		const TUniquePtr<T>& Ptr = Ctx.Request.GetRange<TUniquePtr<T>>();
		Ctx.Items.SetAll(Ptr.Get(), Ptr ? 1 : 0);
	}
};

//////////////////////////////////////////////////////////////////////////

template <typename T>
struct TOptionalBinding : IItemRangeBinding
{
	using SizeType = bool;
	using ItemType = T;
	using IItemRangeBinding::IItemRangeBinding;
	inline static constexpr std::string_view BindName = "Optional";

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		TOptional<T>& Opt = Ctx.Request.GetRange<TOptional<T>>();
		Opt.Reset();

		if (Ctx.Request.NumTotal() == 0)
		{
			return;
		}
		
		if constexpr (std::is_default_constructible_v<T>)
		{
			Opt.Emplace();
			Ctx.Items.Set(reinterpret_cast<T*>(&Opt), 1);
		}
		else if (Ctx.Request.IsFirstCall())
		{
			Ctx.Items.SetUnconstructed();
			Ctx.Items.RequestFinalCall();
			Ctx.Items.Set(reinterpret_cast<T*>(&Opt), 1);	
		}
		else // Move-construct from self reference
		{
			Opt.Emplace(reinterpret_cast<T&&>(Opt));
		}
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		const TOptional<T>& Opt = Ctx.Request.GetRange<TOptional<T>>();
		check(!Opt || reinterpret_cast<const T*>(&Opt) == &Opt.GetValue());
		Ctx.Items.SetAll(reinterpret_cast<const T*>(Opt ? &Opt : nullptr), Opt ? 1 : 0);
	}
};

//////////////////////////////////////////////////////////////////////////

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TSetBinding : IItemRangeBinding
{
	using SizeType = int32;
	using ItemType = T;
	using SetType = TSet<T, KeyFuncs, SetAllocator>;
	using IItemRangeBinding::IItemRangeBinding;
	inline static constexpr std::string_view BindName = TTypename<SetType>::RangeBindName;

	virtual void MakeItems(FLoadRangeContext& Ctx) const override
	{
		SetType& Set = Ctx.Request.GetRange<SetType>();
		SizeType Num = static_cast<SizeType>(Ctx.Request.NumTotal());

		static constexpr bool bAllocate = sizeof(T) > sizeof(FLoadRangeContext::Scratch);
		static constexpr uint64 MaxItems = bAllocate ? 1 : sizeof(FLoadRangeContext::Scratch) / SIZE_T(sizeof(T));
		
		if (Ctx.Request.IsFirstCall())
		{
			Set.Reset();

			if (uint64 NumRequested = Ctx.Request.NumTotal())
			{
				Set.Reserve(NumRequested);

				// Create temporary buffer
				uint64 NumTmp = FMath::Min(MaxItems, NumRequested);
				void* Tmp = bAllocate ? FMemory::Malloc(sizeof(T)) : Ctx.Scratch;
				Ctx.Items.Set(Tmp, NumTmp, sizeof(T));
				if constexpr (std::is_default_constructible_v<T>)
				{
					for (T* It = static_cast<T*>(Tmp), *End = It + NumTmp; It != End; ++It)
					{
						::new (It) T;
					}
				}
				else
				{
					Ctx.Items.SetUnconstructed();
				}

				Ctx.Items.RequestFinalCall();
			}
		}
		else
		{
			// Add items that have been loaded
			TArrayView<T> Tmp = Ctx.Items.Get<T>();
			for (T& Item : Tmp)
			{
				Set.Emplace(MoveTemp(Item));
			}

			if (Ctx.Request.IsFinalCall())
			{
				// Destroy and free temporaries
				uint64 NumTmp = FMath::Min(MaxItems, Ctx.Request.NumTotal());
				for (T& Item : MakeArrayView(Tmp.GetData(), NumTmp))
				{
					Item.~T();
				}
				if constexpr (bAllocate)
				{
					FMemory::Free(Tmp.GetData());
				}	
			}
			else
			{
				Ctx.Items.Set(Tmp.GetData(), FMath::Min(static_cast<uint64>(Tmp.Num()), Ctx.Request.NumMore()));
				check(Ctx.Items.Get<T>().Num());
			}
		}
	}

	virtual void ReadItems(FSaveRangeContext& Ctx) const override
	{
		static_assert(offsetof(TSetElement<T>, Value) == 0);
		const TSparseArray<TSetElement<T>>& Elems = Ctx.Request.GetRange<TSparseArray<TSetElement<T>>>();

		if (Elems.IsEmpty())
		{
			Ctx.Items.SetAll(nullptr, 0, sizeof(TSetElement<T>));
		}
		else if (FExistingItemSlice LastRead = Ctx.Items.Slice)
		{
			// Continue partial response
			const TSetElement<T>* NextElem = static_cast<const TSetElement<T>*>(LastRead.Data) + LastRead.Num + /* skip known invalid */ 1;
			Ctx.Items.Slice = GetContiguousSlice(Elems.PointerToIndex(NextElem), Elems);
		}
		else if (Elems.IsCompact())
		{
			Ctx.Items.SetAll(&Elems[0], Elems.Num());
		}
		else
		{
			// Start partial response
			Ctx.Items.NumTotal = Elems.Num();
			Ctx.Items.Stride = sizeof(TSetElement<T>);
			Ctx.Items.Slice = GetContiguousSlice(0, Elems);
		}
	}

	static FExistingItemSlice GetContiguousSlice(int32 Idx, const TSparseArray<TSetElement<T>>& Elems)
	{
		int32 Num = 1;
		for (;!Elems.IsValidIndex(Idx); ++Idx) {}
		for (; Elems.IsValidIndex(Idx + Num); ++Num) {}
		return { &Elems[Idx], static_cast<uint64>(Num) };
	}
};

//////////////////////////////////////////////////////////////////////////

// Only used for non-default constructible pairs
template <typename K, typename V>
struct TPairBinding : ICustomBinding
{
	using Type = TPair<K,V>;

	template<class Ids>
	TPairBinding(TCustomInit<Ids> Init)
	: MemberIds{Ids::IndexMember("Key"), Ids::IndexMember("Value")}
	, Key(Init, { MemberIds[0] })
	, Value(Init, { MemberIds[1] })
	{}

	void Save(FMemberBuilder& Dst, const TPair<K,V>& Src, const Type* Default, const FSaveContext& Ctx) const
	{
		Dst.Add(MemberIds[0], Key.SaveMember(Src.Key, Ctx));
		Dst.Add(MemberIds[1], Value.SaveMember(Src.Value, Ctx));
	}
		
	void Load(TPair<K,V>& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
	{
		FMemberLoader Members(Src);
		check(Members.PeekName() == MemberIds[0]);
		if (Method == ECustomLoadMethod::Construct)
		{
			Key.ConstructAndLoadMember(/* out */ &Dst.Key, /* in-out */ Members);
			Value.ConstructAndLoadMember(/* out */ &Dst.Value, /* in-out */ Members);		
		}
		else
		{
			Key.LoadMember(/* out */ Dst.Key, /* in-out */ Members);
			Value.LoadMember(/* out */ Dst.Value, /* in-out */ Members);				
		}
	}

	template<typename ContextType>
	bool Diff(const TPair<K,V>& A, const TPair<K,V>& B, ContextType& Ctx) const
	{
		return Key.DiffMember(A.Key, B.Key, MemberIds[0], Ctx) || 
			Value.DiffMember(A.Value, B.Value, MemberIds[1], Ctx);
	}

	const FMemberId				MemberIds[2];
	const TMemberSerializer<K>	Key;
	const TMemberSerializer<V>	Value;
};

//////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
struct TMapBinding : public TSetBinding<TPair<K, V>, KeyFuncs, SetAllocator>
{
	using MapType = TMap<K, V, SetAllocator, KeyFuncs>;
	using Super = TSetBinding<TPair<K, V>, KeyFuncs, SetAllocator>;
	using Super::Super;
	inline static constexpr std::string_view BindName = TTypename<MapType>::RangeBindName;
};

//////////////////////////////////////////////////////////////////////////

//TODO: Consider macroifying parts of this, e.g PP_CUSTOM_BIND(PLAINPROPS_API, FTransform, Transform, Translate, Rotate, Scale)
struct FTransformBinding : ICustomBinding
{
	using Type = FTransform;
	enum class EMember : uint8 { Translate, Rotate, Scale };
	const FMemberId		MemberIds[3];
	const FBindId		VectorId;
	const FBindId		QuatId;

	template<class Ids>
	FTransformBinding(TCustomInit<Ids>)
	: MemberIds{Ids::IndexMember("Translate"), Ids::IndexMember("Rotate"), Ids::IndexMember("Scale")}
	, VectorId(GetStructBindId<Ids, FVector>())
	, QuatId(GetStructBindId<Ids, FQuat>())
	{}

	PLAINPROPS_API void	Save(FMemberBuilder& Dst, const FTransform& Src, const FTransform* Default, const FSaveContext& Context) const;
	PLAINPROPS_API void	Load(FTransform& Dst, FStructLoadView Src, ECustomLoadMethod Method) const;
	PLAINPROPS_API bool Diff(const FTransform& A, const FTransform& B, FDiffContext& Ctx) const;
	inline static  bool Diff(const FTransform& A, const FTransform& B, const FBindContext&) { return !A.Equals(B, 0.0); }
};

//////////////////////////////////////////////////////////////////////////

struct FGuidBinding : ICustomBinding
{
	using Type = FGuid;
	const FMemberId MemberIds[4];

	template<class Ids>
	FGuidBinding(TCustomInit<Ids>)
	: MemberIds{Ids::IndexMember("A"), Ids::IndexMember("B"), Ids::IndexMember("C"), Ids::IndexMember("D")}
	{}

	PLAINPROPS_API void	Save(FMemberBuilder& Dst, const FGuid& Src, const FGuid* Default, const FSaveContext&) const;
	PLAINPROPS_API void	Load(FGuid& Dst, FStructLoadView Src, ECustomLoadMethod Method) const;
	inline static  bool Diff(FGuid A, FGuid B, const FBindContext&) { return A != B; }
};

//////////////////////////////////////////////////////////////////////////

struct FColorBinding : ICustomBinding
{
	using Type = FColor;
	const FMemberId MemberIds[4];

	template<class Ids>
	FColorBinding(TCustomInit<Ids>)
	: MemberIds{Ids::IndexMember("B"), Ids::IndexMember("G"), Ids::IndexMember("R"), Ids::IndexMember("A")}
	{}

	PLAINPROPS_API void	Save(FMemberBuilder& Dst, const FColor& Src, const FColor* Default, const FSaveContext&) const;
	PLAINPROPS_API void	Load(FColor& Dst, FStructLoadView Src, ECustomLoadMethod Method) const;
	inline static  bool Diff(FColor A, FColor B, const FBindContext&) { return A != B; }
};

//////////////////////////////////////////////////////////////////////////

struct FLinearColorBinding : ICustomBinding
{
	using Type = FLinearColor;
	const FMemberId MemberIds[4];

	template<class Ids>
	FLinearColorBinding(TCustomInit<Ids>)
	: MemberIds{Ids::IndexMember("R"), Ids::IndexMember("G"), Ids::IndexMember("B"), Ids::IndexMember("A")}
	{}

	PLAINPROPS_API void	Save(FMemberBuilder& Dst, const FLinearColor& Src, const FLinearColor* Default, const FSaveContext&) const;
	PLAINPROPS_API void	Load(FLinearColor& Dst, FStructLoadView Src, ECustomLoadMethod Method) const;
	inline static  bool Diff(FLinearColor A, FLinearColor B, const FBindContext&) { return A != B; }
};

//////////////////////////////////////////////////////////////////////////

struct FBaseDeltaBinding
{
	enum class EOp { Del, Add };
	FMemberId MemberIds[2];
	FMemberId DelId() const { return MemberIds[(uint32)EOp::Del]; }
	FMemberId AddId() const { return MemberIds[(uint32)EOp::Add]; }
	
	template<class Ids>	
	FBaseDeltaBinding(TInit<Ids>)
	: MemberIds{Ids::IndexMember("Del"), Ids::IndexMember("Add")}
	{}

	template<class Ids>	
	static FBaseDeltaBinding Cache()
	{
		static FBaseDeltaBinding Out(TInit<Ids>{});
		return Out;
	}
};

template <typename SetType, typename KeyType>
struct TBaseDeltaBinding : ICustomBinding, FBaseDeltaBinding
{
	using Type = SetType;
	using ElemType = typename SetType::ElementType;
	static constexpr ERangeSizeType MaxSize = ERangeSizeType::S32;
	static constexpr bool bIsSet = std::is_same_v<KeyType, ElemType>;
	
	const TMemberSerializer<ElemType> Elems;

	template<class Ids>	
	TBaseDeltaBinding(TCustomInit<Ids> Init)
	: FBaseDeltaBinding(FBaseDeltaBinding::Cache<Ids>())
	, Elems(Init, bIsSet ? MakeArrayView(MemberIds) : MakeArrayView({AddId()}))
	{}

	template<typename KeyOrElemType>
	static const KeyOrElemType& Get(const ElemType& Elem)
	{
		if constexpr (std::is_same_v<KeyOrElemType, ElemType>)
		{
			return Elem;
		}
		else
		{
			return Elem.Key;
		}
	}

	// Todo: Reimplement with Assign/Remove/Insert like SaveSetDelta in PlainPropsUObjectBindings.cpp
	inline void SaveDelta(FMemberBuilder& Dst, const SetType& Src, const SetType* Default, const FSaveContext& Ctx, const TMemberSerializer<KeyType>& Keys) const
	{
		if (Src.IsEmpty())
		{
			if (Default && !Default->IsEmpty())
			{
				Dst.AddRange(DelId(), SaveAll<KeyType>(*Default, Keys, Ctx));
			}
		}
		else if (Default && !Default->IsEmpty())
		{
			TBitArray<> DelSubset(false, Default->GetMaxIndex());
			for (auto It = Default->CreateConstIterator(); It; ++It)
			{
				DelSubset[It.GetId().AsInteger()] = !Src.Contains(Get<KeyType>(*It));
			}
			if (DelSubset.Find(true) != INDEX_NONE)
			{
				Dst.AddRange(DelId(), SaveSome<KeyType>(*Default, DelSubset, Keys, Ctx));
			}

			TBitArray<> AddSubset(false, Src.GetMaxIndex());
			for (auto It = Src.CreateConstIterator(); It; ++It)
			{
				AddSubset[It.GetId().AsInteger()] = !ContainsValue(*Default, *It);
			}
			if (AddSubset.Find(true) != INDEX_NONE)
			{
				Dst.AddRange(AddId(), SaveSome<ElemType>(Src, AddSubset, Elems, Ctx));
			}
		}
		else
		{
			Dst.AddRange(AddId(), SaveAll<ElemType>(Src, Elems, Ctx));
		}
	}

	template<typename ItemType>
	FTypedRange SaveAll(const SetType& Set, const TMemberSerializer<ItemType>& Schema, const FSaveContext& Ctx) const
	{
		check(!Set.IsEmpty());

		TRangeSaver<ItemType> Items(Ctx, static_cast<uint64>(Set.Num()), Schema);
		for (const ElemType& Elem : Set)
		{
			Items.AddItem(Get<ItemType>(Elem));
		}
		return Items.Finalize(MaxSize);
	}
	
	template<typename ItemType>
	FTypedRange SaveSome(const SetType& Set, const TBitArray<>& Subset, const TMemberSerializer<ItemType>& Schema, const FSaveContext& Ctx) const
	{
		TRangeSaver<ItemType> Items(Ctx, Subset.CountSetBits(), Schema);
		for (int32 Idx = 0, Max = Set.GetMaxIndex(); Idx < Max; ++Idx)
		{
			if (Subset[Idx])
			{
				const ElemType& Elem = Set.Get(FSetElementId::FromInteger(Idx));
				Items.AddItem(Get<ItemType>(Elem));
			}
		}
		return Items.Finalize(MaxSize);
	}

	inline void LoadDelta(Type& Dst, FStructLoadView Src, ECustomLoadMethod Method, const TMemberSerializer<KeyType>& Keys) const
	{
		FMemberLoader Members(Src);

		if (Method == ECustomLoadMethod::Construct)
		{
			::new (&Dst) SetType;
		}
				
		while (Members.HasMore())
		{
			if (Members.PeekNameUnchecked() == AddId())
			{
				ApplyItems<EOp::Add, ElemType>(Dst, Members.GrabRange(), Elems);
				check(!Members.HasMore());
				break;
			}

			check(Members.PeekNameUnchecked() == DelId());
			ApplyItems<EOp::Del, KeyType>(Dst, Members.GrabRange(), Keys);
		}
	}

	template<EOp Op, typename T>
	void ApplyItems(SetType& Out, FRangeLoadView Items, const TMemberSerializer<T>& Schema) const
	{
		check(!Items.IsEmpty());
 
		if constexpr (Op == EOp::Add && !LeafType<T>)
		{
			Out.Reserve(static_cast<int32>(Items.Num()));
		}

		if constexpr (LeafType<T>)
		{
			ApplyLeaves<Op, T>(Out, Items.AsLeaves());
		}
		else if constexpr (TMemberSerializer<T>::Kind == EMemberKind::Struct)
		{
			ApplyStructs<Op, T>(Out, Items.AsStructs());
		}
		else
		{
			ApplyRanges<Op, T>(Out, Items.AsRanges(), Schema);
		}
	}

	template<EOp Op, typename T>
	void ApplyLeaves(SetType& Out, FLeafRangeLoadView Items) const requires (Op == EOp::Add && !std::is_same_v<T, bool>)
	{
		Out.Append(MakeArrayView(Items.As<T>()));
	}

	template<EOp Op, typename T>
	void ApplyLeaves(SetType& Out, FLeafRangeLoadView Items) const
	{
		for (T Item : Items.As<T>())
		{
			ApplyItem<Op>(Out, Item);
		}
	}

	template<EOp Op, typename T>
	void ApplyRanges(SetType& Out, FNestedRangeLoadView Items, const TMemberSerializer<T>& Schema) const
	{
		static_assert(std::is_default_constructible_v<T>, TEXT("Ranges must be default-constructible"));

		TConstArrayView<FRangeBinding> Bindings(Schema.Bindings, Schema.NumRanges);
		for (FRangeLoadView Item : Items)
		{
			T Tmp;
			LoadRange(&Tmp, Item, Bindings);
			ApplyItem<Op>(Out, MoveTemp(Tmp));
		}
	}
	
	template<EOp Op, typename T>
	void ApplyStructs(SetType& Out, FStructRangeLoadView Items) const
	{
		for (FStructLoadView Item : Items)
		{
			if constexpr (std::is_default_constructible_v<T>)
			{
				T Tmp;
				LoadStruct(&Tmp, Item);
				ApplyItem<Op>(Out, MoveTemp(Tmp));
			}
			else
			{
				alignas(T) uint8 Buffer[sizeof(T)];
				ConstructAndLoadStruct(Buffer, Item);
				T& Tmp = *reinterpret_cast<T*>(Buffer);
				ApplyItem<Op>(Out, MoveTemp(Tmp));
				Tmp.~T();
			}
		}
	}
	
	template<EOp Op, typename T>
	void ApplyItem(SetType& Out, T&& Item) const
	{
		if constexpr (Op == EOp::Add)
		{
			Out.Add(MoveTemp(Item));
		}
		else
		{
			Out.Remove(Item);
		}
	}

	inline static bool ContainsValue(const Type& Set, const ElemType& Elem)
	{
		if constexpr (bIsSet)
		{
			return Set.Contains(Elem);
		}
		else
		{
			const auto* Value = Set.Find(Elem.Key);
			return Value && *Value == Elem.Value;
		}
	}

	inline static bool Diff(const Type& As, const Type& Bs, const FBindContext&)
	{
		if (As.Num() != Bs.Num())
		{
			return true;
		}

		for (const ElemType& A : As)
		{
			if (!ContainsValue(Bs, A))
			{
				return true;
			}
		}

		return false;
	}
};

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TSetDeltaBinding : TBaseDeltaBinding<TSet<T, KeyFuncs, SetAllocator>, T>
{
	using SetType = TSet<T, KeyFuncs, SetAllocator>;
	using Super = TBaseDeltaBinding<SetType, T>;

	struct FCustomTypename
	{
		inline static constexpr std::string_view DeclName = "SetDelta";
		inline static constexpr std::string_view BindName = Concat<DeclName, ShortTypename<KeyFuncs>, ShortTypename<SetAllocator>>;
		inline static constexpr std::string_view Namespace;
		using Parameters = std::tuple<T>;
	};
		
	template<class Ids>	
	TSetDeltaBinding(TCustomInit<Ids> Init)
	: Super(Init)
	{}
	
	inline void Save(FMemberBuilder& Dst, const SetType& Src, const SetType* Default, const FSaveContext& Ctx) const
	{
		Super::SaveDelta(Dst, Src, Default, Ctx, /* Keys */ Super::Elems);
	}

	inline void Load(SetType& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
	{
		Super::LoadDelta(Dst, Src, Method, /* Keys */ Super::Elems);
	}
};

template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
struct TMapDeltaBinding : TBaseDeltaBinding<TMap<K, V, SetAllocator, KeyFuncs>, K>
{
	using MapType = TMap<K, V, SetAllocator, KeyFuncs>;
	using Super = TBaseDeltaBinding<MapType, K>;

	struct FCustomTypename
	{
		inline static constexpr std::string_view DeclName = "MapDelta";
		inline static constexpr std::string_view BindName = Concat<DeclName, ShortTypename<KeyFuncs>, ShortTypename<SetAllocator>>;
		inline static constexpr std::string_view Namespace;
		using Parameters = std::tuple<K, V>;
	};
	
	const TMemberSerializer<K> Keys;
	
	template<class Ids>	
	TMapDeltaBinding(TCustomInit<Ids> Init)
	: Super(Init)
	, Keys(Init, {FBaseDeltaBinding::DelId()})
	{}
	
	inline void Save(FMemberBuilder& Dst, const MapType& Src, const MapType* Default, const FSaveContext& Ctx) const
	{
		Super::SaveDelta(Dst, Src, Default, Ctx, Keys);
	}

	inline void Load(MapType& Dst, FStructLoadView Src, ECustomLoadMethod Method) const
	{
		Super::LoadDelta(Dst, Src, Method, Keys);
	}
};


//	template <typename T, typename VariantType>
//	struct TVariantConstructFromMember
//	{
//		/** Default construct the type and load it from the FArchive */
//		static void Construct(VariantType& Dst, FMemberReader Src)
//		{
//			if constexpr (std::is_arithmetic_v<T>)
//			{
//				Dst.template Emplace<T>(Src.GrabLeaf().As<T>());
//			}
//			else constexpr 
//
//		}
//	};
//
//template <typename... Ts>
//struct TVariantBinding : ICustomBinding
//{
//	using VariantType = TVariant<Ts...>;
//
//	const FStructDeclaration& Declaration;
//
//	static constexpr void(*Loaders[])(FMemberReader&, VariantType&) = { &TLoader<Ts, VariantType>::Load... };
//
//	virtual void LoadStruct(void* Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const override
//	{
//		VariantType& Variant = *reinterpret_cast<VariantType*>(Dst);
//
//		if (Method == ECustomLoadMethod::Assign)
//		{
//			Variant.~VariantType();
//		}
//
//		FMemberReader Members(Src);
//		const FMemberId* DeclaredName = Algo::Find(Declaration.Names, Members.PeekName());
//		check(DeclaredName);
//		int64 Idx = DeclaredName - &Declaration.Names[0];
//
//		check(TypeIndex < UE_ARRAY_COUNT(Loaders));
//		Loaders[TypeIndex](Ar, OutVariant);		
//		
//	}
//
//	template<typename T>
//	void Load(TVariant<Ts...>& Dst, FCustomMemberLoader& Src, ECustomLoadMethod Method)
//	{
//
//		
//		{
//			Dst.Emplace(MoveTemp(*reinterpret_cast<T*>(Temp)));
//		}
//		else
//		{
//			new (Dst) TVariant<Ts...>(MoveTemp(*reinterpret_cast<T*>(Temp)));
//		}
//	}
//
//	virtual FBuiltStruct*	SaveStruct(const void* Src, FDebugIds Debug) const override
//	{
//		...
//	}
//};

} // namespace PlainProps::UE

namespace PlainProps
{

template <> struct TTypename<FName>					{ inline static constexpr std::string_view DeclName = "Name"; };
template <> struct TTypename<FTransform>			{ inline static constexpr std::string_view DeclName = "Transform"; };
template <> struct TTypename<FGuid>					{ inline static constexpr std::string_view DeclName = "Guid"; };
template <> struct TTypename<FColor>				{ inline static constexpr std::string_view DeclName = "Color"; };
template <> struct TTypename<FLinearColor>			{ inline static constexpr std::string_view DeclName = "LinearColor"; };
template <> struct TTypename<FString>				{ inline static constexpr std::string_view RangeBindName = "String"; };
template <> struct TTypename<FUtf8String>			{ inline static constexpr std::string_view RangeBindName = "Utf8String"; };
template <> struct TTypename<FAnsiString>			{ inline static constexpr std::string_view RangeBindName = "AnsiString"; };

template <typename K, typename V>
struct TTypename<TPair<K,V>>
{
	inline static constexpr std::string_view DeclName = "Pair";
	using Parameters = std::tuple<K, V>;
};

inline static constexpr std::string_view UeArrayName = "Array";
inline static constexpr std::string_view UeSetName = "Set";
inline static constexpr std::string_view UeMapName = "Map";

template<typename T, typename Allocator>
struct TTypename<TArray<T, Allocator>>
{
	inline static constexpr std::string_view RangeBindName = Concat<UeArrayName, ShortTypename<Allocator>>;
};

template<>
struct TShortTypename<FDefaultAllocator> : FOmitTypename {};
template<>
struct TShortTypename<FDefaultSetAllocator> : FOmitTypename {};
template<typename T>
struct TShortTypename<DefaultKeyFuncs<T, false>> : FOmitTypename {};
template<typename K, typename V>
struct TShortTypename<TDefaultMapHashableKeyFuncs<K, V, false>> : FOmitTypename {};

inline constexpr std::string_view InlineAllocatorPrefix = "InlX";
template<int N>
struct TShortTypename<TInlineAllocator<N>>
{
	inline static constexpr std::string_view Value = Concat<InlineAllocatorPrefix, HexString<N>>;
};

template<int N>
struct TShortTypename<TInlineSetAllocator<N>> : TShortTypename<TInlineAllocator<N>> {};

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TTypename<TSet<T, KeyFuncs, SetAllocator>>
{
	inline static constexpr std::string_view RangeBindName = Concat<UeSetName, ShortTypename<KeyFuncs>, ShortTypename<SetAllocator>>;
};

template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
struct TTypename<TMap<K, V, SetAllocator, KeyFuncs>>
{
	inline static constexpr std::string_view RangeBindName = Concat<UeMapName, ShortTypename<SetAllocator>, ShortTypename<KeyFuncs>>;
};

template<>
PLAINPROPS_API void AppendString(FUtf8StringBuilderBase& Out, const FName& Name);

template<typename T, typename Allocator>
struct TRangeBind<TArray<T, Allocator>>
{
	using Type = UE::TArrayBinding<T, Allocator>;
};

template<> struct TRangeBind<FString> { using Type = UE::TStringBinding<FString>; };
template<> struct TRangeBind<FAnsiString> {	using Type = UE::TStringBinding<FAnsiString>; };
template<> struct TRangeBind<FUtf8String> {	using Type = UE::TStringBinding<FUtf8String>; };
template<> struct TRangeBind<FVerseString> { using Type = UE::TStringBinding<FVerseString>; };

template<typename T>
struct TRangeBind<TUniquePtr<T>>
{
	using Type = UE::TUniquePtrBinding<T>;
};

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TRangeBind<TSet<T, KeyFuncs, SetAllocator>>
{
	using Type = UE::TSetBinding<T, KeyFuncs, SetAllocator>;
};

template <typename T, typename KeyFuncs, typename SetAllocator>
struct TCustomDeltaBind<TSet<T, KeyFuncs, SetAllocator>>
{
	using Type = UE::TSetDeltaBinding<T, KeyFuncs, SetAllocator>;
};

template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
struct TRangeBind<TMap<K, V, SetAllocator, KeyFuncs>>
{
	using Type = UE::TMapBinding<K, V, SetAllocator, KeyFuncs>;
};

template <typename K, typename V, typename SetAllocator, typename KeyFuncs>
struct TCustomDeltaBind<TMap<K, V, SetAllocator, KeyFuncs>>
{
	using Type = UE::TMapDeltaBinding<K, V, SetAllocator, KeyFuncs>;
};

template<typename T>
struct TRangeBind<TOptional<T>>
{
	using Type = UE::TOptionalBinding<T>;
};

template<> struct TOccupancyOf<FQuat> : FRequireAll {};
template<> struct TOccupancyOf<FVector> : FRequireAll {};
template<> struct TOccupancyOf<FGuid> : FRequireAll {};
template<> struct TOccupancyOf<FColor> : FRequireAll {};
template<> struct TOccupancyOf<FLinearColor> : FRequireAll {};

template<> struct TCustomBind<FTransform> {	using Type = UE::FTransformBinding; };
template<> struct TCustomBind<FGuid> { using Type = UE::FGuidBinding; };
template<> struct TCustomBind<FColor> { using Type = UE::FColorBinding; };
template<> struct TCustomBind<FLinearColor> { using Type = UE::FLinearColorBinding; };


template<typename K, typename V>
struct TOccupancyOf<TPair<K, V>> : FRequireAll {};

template<typename K, typename V> requires (!std::is_default_constructible_v<TPair<K,V>>)
struct TCustomBind<TPair<K, V>>
{
	using Type = UE::TPairBinding<K, V>;
};

} // namespace PlainProps

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Containers/StringView.h"
#include "Memory/MemoryFwd.h"
#include "Memory/MemoryView.h"
#include "PlainPropsCtti.h"
#include "PlainPropsDeclare.h"
#include "PlainPropsTypename.h"
#include "PlainPropsRead.h"
#include "PlainPropsTypes.h"
#include <tuple>

namespace PlainProps 
{

struct FBindContext;
struct FBuiltRange;
struct FBuiltStruct;
struct FCustomBindingHandle;
struct FDiffContext;
class FIdIndexerBase;
struct FLoadBatch;
class FMemberBuilder;
class FRangeLoader;
struct FSchemaBatch;
class FScratchAllocator;
class FStructBinding;
struct FStructLoadView;
class FRangeBinding;
struct FSaveContext;
struct FTypedRange;
struct IItemRangeBinding;
template<class T> class TIdIndexer;

////////////////////////////////////////////////////////////////////////////////////////////////

inline FAnsiStringView ToAnsiView(std::string_view Str) { return FAnsiStringView(Str.data(), Str.length()); }

////////////////////////////////////////////////////////////////////////////////////////////////

enum class ELeafBindType : uint8 { Bool, IntS, IntU, Float, Hex, Enum, Unicode, BitfieldBool };

inline static constexpr ELeafBindType ToLeafBindType(ELeafType Type)
{
	return static_cast<ELeafBindType>(static_cast<uint8>(Type));
}

inline static constexpr ELeafType ToLeafType(ELeafBindType Type)
{
	return Type == ELeafBindType::BitfieldBool ? ELeafType::Bool : static_cast<ELeafType>(static_cast<uint8>(Type));
}

struct FBasicLeafBindType
{
    EMemberKind		_ : 2;
	ELeafBindType	__ : 3;
	ELeafWidth		Width : 2;
	uint8			_Pad : 1;
};

struct FBitfieldBoolBindType
{
    EMemberKind		_ : 2;
	ELeafBindType	__ : 3;
	uint8			Idx : 3;
};

union FLeafBindType
{
	constexpr explicit FLeafBindType(ELeafBindType BasicType, ELeafWidth Width) : Basic({EMemberKind::Leaf, BasicType, Width}) {}
	constexpr explicit FLeafBindType(FUnpackedLeafType In) : Basic({EMemberKind::Leaf, ToLeafBindType(In.Type), In.Width}) {}
	constexpr explicit FLeafBindType(FLeafType In) : FLeafBindType(FUnpackedLeafType(In)) {}
	constexpr explicit FLeafBindType(FBitfieldBoolBindType In) : Bitfield({EMemberKind::Leaf, ELeafBindType::BitfieldBool, In.Idx}) {}
	constexpr explicit FLeafBindType(uint8 BitfieldIdx) : Bitfield({EMemberKind::Leaf, ELeafBindType::BitfieldBool, BitfieldIdx}) {}

	struct
	{
		EMemberKind			_ : 2;
		ELeafBindType		Type : 3;
		uint8				_Pad : 3;
	}						Bind;
	FBasicLeafBindType		Basic;
	FBitfieldBoolBindType	Bitfield;
};

inline static constexpr FLeafType ToLeafType(FLeafBindType Leaf)
{
	if (Leaf.Bind.Type == ELeafBindType::BitfieldBool)
	{
		return { EMemberKind::Leaf, ELeafWidth::B8, ELeafType::Bool };
	}
	
	return { EMemberKind::Leaf,  Leaf.Basic.Width, ToLeafType(Leaf.Bind.Type) };
}

struct FRangeBindType : FRangeType {};

struct FStructBindType : FStructType {};

union FMemberBindType
{
	constexpr explicit FMemberBindType(FLeafType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FUnpackedLeafType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FLeafBindType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FBitfieldBoolBindType In) : Leaf(In) {}
	constexpr explicit FMemberBindType(FRangeType In) : Range(In) {}
	constexpr explicit FMemberBindType(ERangeSizeType MaxSize) : Range({EMemberKind::Range, MaxSize}) {}
	constexpr explicit FMemberBindType(FStructType In) : Struct(In) {}
	
	bool					IsLeaf() const		{ return Kind == EMemberKind::Leaf; }
	bool					IsRange() const		{ return Kind == EMemberKind::Range; }
	bool					IsStruct() const	{ return Kind == EMemberKind::Struct; }
	EMemberKind				GetKind() const		{ return Kind; }
	
	FLeafBindType			AsLeaf() const		{ checkSlow(IsLeaf());		return Leaf; }
	FRangeBindType			AsRange() const		{ checkSlow(IsRange());		return Range; }
	FStructBindType			AsStruct() const	{ checkSlow(IsStruct());	return Struct; }
	uint8					AsByte() const		{ return BitCast<uint8>(*this); }

	friend inline bool operator==(FMemberBindType A, FMemberBindType B) { return A.AsByte() == B.AsByte(); }
	friend inline uint32 GetTypeHash(FMemberBindType In) { return In.AsByte(); }
private:
    EMemberKind				Kind : 2;
	FLeafBindType			Leaf;
    FRangeBindType			Range;
    FStructBindType			Struct;
};

static_assert(sizeof(FMemberBindType) == 1);

////////////////////////////////////////////////////////////////////////////////////////////////

// Members are loaded in saved FStructSchema order, not current offset order unless upgrade layer reorders
struct alignas(/*FRangeBinding*/ 8) FSchemaBinding
{
	FDeclId					DeclId;
	uint16					NumMembers;
	uint16					NumInnerSchemas;
	uint16					NumInnerRanges;
	FMemberBindType			Members[0];

	const FMemberBindType*	GetInnerRangeTypes() const	{ return Members + NumMembers; }
	const uint32*			GetOffsets() const			{ return AlignPtr<uint32>(GetInnerRangeTypes() + NumInnerRanges); }
	const FInnerId*			GetInnerSchemas() const		{ return AlignPtr<FInnerId>(GetOffsets() + NumMembers); }
	const FRangeBinding*	GetRangeBindings() const	{ return AlignPtr<FRangeBinding>(GetInnerSchemas() + NumInnerSchemas); }
	uint32					CalculateSize() const;
	bool					HasSuper() const			{ return NumInnerSchemas > 0 && Members[0].IsStruct() && Members[0].AsStruct().IsSuper; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FUnpackedLeafBindType
{
	ELeafBindType			Type;
	union
	{
		ELeafWidth			Width;
		uint8				BitfieldIdx;
	};
	
	constexpr FUnpackedLeafBindType(FLeafBindType In)
	: Type(In.Bind.Type)
	{
		if (Type != ELeafBindType::BitfieldBool)
		{
			Width = In.Basic.Width;
		}
		else
		{
			BitfieldIdx = In.Bitfield.Idx;
		}
	}

	FMemberBindType Pack() const
	{ 
		return FMemberBindType(Type == ELeafBindType::BitfieldBool ? FLeafBindType(BitfieldIdx) : FLeafBindType(Type, Width));
	}
};

inline static constexpr FUnpackedLeafType ToUnpackedLeafType(FUnpackedLeafBindType Leaf)
{
	if (Leaf.Type == ELeafBindType::BitfieldBool)
	{
		return { ELeafType::Bool, ELeafWidth::B8 };
	}
	
	return { ToLeafType(Leaf.Type), Leaf.Width };
}

// @pre Type != ELeafBindType::BitfieldBool
inline FUnpackedLeafType UnpackNonBitfield(FLeafBindType Packed)
{
	FUnpackedLeafBindType Unpacked(Packed);
	return { ToLeafType(Unpacked.Type), Unpacked.Width };
}

struct FLeafMemberBinding
{
	FUnpackedLeafBindType	Leaf;
	FOptionalEnumId			Enum;
	SIZE_T					Offset;
};

struct FRangeMemberBinding
{
	const FMemberBindType*	InnerTypes;
	const FRangeBinding*	RangeBindings;
	uint16					NumRanges; // At least 1, >1 for nested ranges
	FOptionalInnerId		InnermostSchema;
	SIZE_T					Offset;
};

struct FStructMemberBinding
{
	FStructType				Type;
	FBindId					Id;
	SIZE_T					Offset;
};

PLAINPROPS_API FRangeMemberBinding GetInnerRange(FRangeMemberBinding In); // @pre In.NumRanges > 1

////////////////////////////////////////////////////////////////////////////////////////////////

struct FBothStructId
{
	FBindId					BindId;
	FDeclId					DeclId;

	bool					IsLowered() const { return BindId != DeclId; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

// FStructId statically known to share FBindId and FDeclId, i.e. not lowered
struct FDualStructId : FStructId
{
	explicit FDualStructId(FStructId In) : FStructId(In) {}
	
	operator FBindId() const { return {Idx}; }
	operator FDeclId() const { return {Idx}; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

// Helps raise lowered FDeclId by seeing which FBindId a particular named member has
struct FInnerStruct
{
	FMemberId				Name; // Of outer Range or Struct
	FBindId					Id;
};

enum ECustomLoadMethod { Construct, Assign };

/// Load/save a struct with custom code to handle:
/// * reference types
/// * private members
/// * non-default constructible types
/// * custom delta semantics
/// * other runtime representations than struct/class, e.g. serialize database
/// * optimizations of very common structs
struct ICustomBinding
{
	virtual ~ICustomBinding() {}
	virtual void	SaveCustom(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) = 0;
	virtual void	LoadCustom(void* Dst, FStructLoadView Src, ECustomLoadMethod Method) const = 0;
	virtual bool	DiffCustom(const void* StructA, const void* StructB, const FBindContext& Ctx) const = 0;

	// Overload to track the first diffing member, see FDiffPath
	PLAINPROPS_API virtual bool	DiffCustom(const void* StructA, const void* StructB, FDiffContext& Ctx) const;
};

struct FInnersHandle
{
	uint32			Num = 0;
	uint32			Idx = 0;
};

struct FIdWindow
{
	uint32			Min = 0;
	uint32			Num = 0;
};

// Slightly optimized map from FStructId to ICustomBinding/FStructDeclaration/FInnersHandle
class FCustomBindingMap
{
public:
	UE_NONCOPYABLE(FCustomBindingMap);
	explicit FCustomBindingMap(FDebugIds Dbg) : Debug(Dbg) {}
	~FCustomBindingMap() { FMemory::Free(Values); }

	void							Bind(FBindId Id, ICustomBinding& Binding, const FStructDeclaration& Declaration, FInnersHandle LoweredInners);
	FCustomBindingHandle			Find(FBindId Id) const;
	void							Drop(FBindId Id);

private:
	FIdWindow						Window;
	TSet<FStructId>			Keys;
	ICustomBinding**				Values = nullptr;
	uint32							MaxValues = 0;
public:
	FDebugIds						Debug;
};

class FCustomBindings
{
public:
	// @param Binding must outlive this or call DropStruct()
	PLAINPROPS_API void							BindStruct(FBindId Id, ICustomBinding& Binding, const FStructDeclaration& Declaration, TConstArrayView<FInnerStruct> LoweredInners = {});
	PLAINPROPS_API const ICustomBinding*		FindStruct(FBindId Id) const;
	PLAINPROPS_API const ICustomBinding*		FindStruct(FBindId Id, TConstArrayView<FInnerStruct>& OutLoweredInners) const;
	PLAINPROPS_API ICustomBinding*				FindStructToSave(FBindId Id, const FStructDeclaration*& OutDeclaration) const;
	PLAINPROPS_API const FStructDeclaration*	FindDeclaration(FBindId Id) const;
	PLAINPROPS_API void							DropStruct(FBindId Id);

	virtual FCustomBindingHandle				Find(FBindId Id) const = 0;
protected:
	FCustomBindings(TArray<FInnerStruct>& Inners, FDebugIds Dbg) : Map(Dbg), BottomInners(Inners) {}
	FCustomBindings(const FCustomBindings* Under) : Map(Under->Map.Debug), BottomInners(Under->BottomInners) {}
	~FCustomBindings() = default;

	FCustomBindingMap							Map;
	TArray<FInnerStruct>&						BottomInners;
};

class FCustomBindingsBottom final : public FCustomBindings
{
	mutable TArray<FInnerStruct>				AllInners;
	PLAINPROPS_API virtual FCustomBindingHandle	Find(FBindId Id) const override;	
public:
	explicit FCustomBindingsBottom(FDebugIds Dbg) : FCustomBindings(/* unconstructed */  AllInners, Dbg) {}
};

class FCustomBindingsOverlay final : public FCustomBindings
{
	const FCustomBindings&						Underlay;
	PLAINPROPS_API virtual FCustomBindingHandle	Find(FBindId Id) const override;
public:
	explicit FCustomBindingsOverlay(const FCustomBindings& Under) : FCustomBindings(&Under), Underlay(Under) {}
};

template<typename T>
struct TCustomBind { using Type = void; };

template<class T>
struct TCustomDeltaBind { using Type = void; };

template<typename T>
using CustomBind = typename TCustomBind<T>::Type;

template<typename T>
using CustomDeltaBind = typename TCustomDeltaBind<T>::Type;

////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TOccupancyOf
{
	static constexpr EMemberPresence Value = EMemberPresence::AllowSparse;
};

struct FRequireAll
{
	static constexpr EMemberPresence Value = EMemberPresence::RequireAll;
};

////////////////////////////////////////////////////////////////////////////////////////////////

// Request from loading layer to IItemRangeBinding to allocate and construct items
class FConstructionRequest
{
	friend FRangeLoader;

	void* const Range = nullptr;
	const uint64 Num = 0;
	uint64 Index = 0;

	FConstructionRequest(void* InRange, uint64 InNum) : Range(InRange), Num(InNum) {}
	
public:
	template<typename T>
	T& GetRange() const { return *reinterpret_cast<T*>(Range); }
	
	uint64 NumTotal() const { return Num; }
	uint64 NumMore() const { return Num - Index; }
	uint64 GetIndex() const { return Index; }
	bool IsFirstCall() const { return Index == 0;}
	bool IsFinalCall() const { return Index == Num;}
};

// Response from IItemRangeBinding with contiguous items ready to be loaded
//
// Non-contiguous containers provide one items one by one or use
// FLoadRangeContext::Scratch or a temp allocation to avoid that
class FConstructedItems
{
public:
	// E.g. allow hash table to rehash after all items are loaded
	void RequestFinalCall() { bNeedFinalize = true; }

	void SetUnconstructed() { bUnconstructed = true; }

	// Non-contiguous items must be set individually
	template<typename ItemType>
	void Set(ItemType* Items, uint64 NumItems)
	{
		Set(Items, NumItems, sizeof(ItemType));
	}

	void Set(void* Items, uint64 NumItems, uint32 ItemSize)
	{
		check(NumItems == 0 || Items != Data);
		Data = reinterpret_cast<uint8*>(Items);
		Num = NumItems;
		Size = ItemSize;
	}
	
	void UpdateNum(uint64 NumItems)
	{
		check(Data);
		Num = NumItems;
	}

	template<typename ItemType>
	TArrayView<ItemType> Get()
	{ 
		return MakeArrayView(reinterpret_cast<ItemType*>(Data), Num);
	}

	inline uint8* GetData()		{ return Data; }
	inline uint8* GetDataEnd()	{ return Data + NumBytes(); }

private:
	friend FRangeLoader;
	uint8*	Data = nullptr;
	uint64	Num = 0;			
	uint32	Size = 0;
	bool	bNeedFinalize = false;
	bool	bUnconstructed = false;

	uint64	NumBytes() const { return Num * Size; }
};

struct FLoadRangeContext
{
	FConstructionRequest	Request;		// Request to construct items to be loaded
	FConstructedItems		Items;			// Response from IItemRangeBinding
	uint64					Scratch[64];	// Scratch memory for IItemRangeBinding
};

// todo: switch to class
struct FGetItemsRequest
{
	template<typename T>
	const T& GetRange() const { return *reinterpret_cast<const T*>(Range); }

	bool IsFirstCall() const { return NumRead == 0;}

	const void* Range = nullptr;
	uint64 NumRead = 0;
};

struct FExistingItemSlice
{
	const void*		Data = nullptr;			
	uint64			Num = 0;

	explicit operator bool() const { return !!Num; }

	const uint8* At(uint64 Idx, uint32 Stride) const
	{
		check(Idx < Num);
		return reinterpret_cast<const uint8*>(Data) + Idx * Stride;
	}
};

struct FExistingItems
{
	uint64				NumTotal = 0;
	uint32				Stride = 0;
	FExistingItemSlice	Slice;

	void SetAll(const void* Items, uint64 Num, uint32 InStride)
	{
		NumTotal = Num;
		Stride = InStride;
		Slice = { Items, Num };
	}

	template<typename ItemType>
	void SetAll(const ItemType* Items, uint64 Num)
	{
		SetAll(Items, Num, sizeof(ItemType));
	}
};

struct FSaveRangeContext
{
	FGetItemsRequest		Request;		// Request to get items to be saved
	FExistingItems			Items;			// Response from IRangeBinding
	uint64					Scratch[8]; 	// Scratch memory for IRangeBinding
};

struct alignas(16) IItemRangeBinding
{
	virtual void ReadItems(FSaveRangeContext& Ctx) const = 0;
	virtual void MakeItems(FLoadRangeContext& Ctx) const = 0;

	// Try living without virtual destructor and trivially destructible members
	explicit IItemRangeBinding(FConcreteTypenameId RangeBindName) : BindName(RangeBindName) {}
	const FConcreteTypenameId BindName;
};

////////////////////////////////////////////////////////////////////////////////////////////////

// Possible save opt: Use paged linear allocator that only allocates on page exhaustion
class FLeafRangeAllocator
{
	FScratchAllocator&			Scratch;
	FBuiltRange*				Range = nullptr;
	const FUnpackedLeafType		Expected;

	PLAINPROPS_API void* Allocate(uint64 Num, SIZE_T LeafSize);

public:
	FLeafRangeAllocator(FScratchAllocator& InScratch, FUnpackedLeafType InExpected) : Scratch(InScratch), Expected(InExpected) {}

	template<LeafType T, typename SizeType>
	T* AllocateRange(SizeType Num)
	{
		check(ReflectArithmetic<T> == Expected);
		return Num ? static_cast<T*>(Allocate(static_cast<uint64>(Num), sizeof(T))) : nullptr;
	}

	template<typename SizeType>
	void* AllocateNonEmptyRange(SizeType Num, ELeafWidth Width)
	{
		check(Num > 0);
		check(Width == Expected.Width);
		return Allocate(static_cast<uint64>(Num), SizeOf(Width));
	}

	FBuiltRange* GetAllocatedRange() { return Range; }
};


// Specialized binding for transcoding leaf ranges
struct alignas(16) ILeafRangeBinding
{
	virtual void		SaveLeaves(const void* Range, FLeafRangeAllocator& Out) const = 0;
	virtual void		LoadLeaves(void* Range, FLeafRangeLoadView Leaves) const = 0;
	virtual bool		DiffLeaves(const void* RangeA, const void* RangeB) const = 0;

	explicit ILeafRangeBinding(FConcreteTypenameId RangeBindName) : BindName(RangeBindName) {}
	const FConcreteTypenameId BindName;

	template<typename SizeType>
	static bool Diff(SizeType NumA, SizeType NumB, const void* A, const void* B, SIZE_T ItemSize)
	{
		return NumA != NumB || (NumA > 0 && FMemory::Memcmp(A, B, static_cast<uint64>(NumA) * ItemSize) != 0);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

class FRangeBinding
{
	static constexpr uint64		SizeMask = 0b1111;
	static constexpr uint64		LeafMask = uint64(1) << FPlatformMemory::KernelAddressBit;
	static constexpr uint64		BindMask = ~(SizeMask | LeafMask);
	uint64						Handle;

public:
	PLAINPROPS_API FRangeBinding(const IItemRangeBinding& Binding, ERangeSizeType SizeType);
	PLAINPROPS_API FRangeBinding(const ILeafRangeBinding& Binding, ERangeSizeType SizeType);
	
	bool						IsLeafBinding() const					{ return !!(LeafMask & Handle); }
	const IItemRangeBinding&	AsItemBinding() const					{ check(!IsLeafBinding()); return *reinterpret_cast<IItemRangeBinding*>(Handle & BindMask); }
	const ILeafRangeBinding&	AsLeafBinding() const					{ check( IsLeafBinding()); return *reinterpret_cast<ILeafRangeBinding*>(Handle & BindMask); }
	ERangeSizeType				GetSizeType() const						{ return static_cast<ERangeSizeType>(Handle & SizeMask); }
	FConcreteTypenameId			GetBindName() const						{ return IsLeafBinding() ? AsLeafBinding().BindName : AsItemBinding().BindName; }
	inline bool	operator==(FRangeBinding O) const { return Handle == O.Handle; }
};

template<typename T>
struct TRangeBind{ using Type = void; };

template<typename T>
using RangeBind = typename TRangeBind<T>::Type;

////////////////////////////////////////////////////////////////////////////////////////////////

struct FBothType
{
	FType		BindType;
	FType		DeclType;

	bool IsLowered() const { return BindType != DeclType; }
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemberBinding
{
	explicit FMemberBinding(uint64 InOffset = 0)
	: Offset(InOffset)
	, InnermostType(FLeafBindType(ELeafBindType::Bool, ELeafWidth::B8))
	{}

	uint64							Offset;
	FMemberBindType					InnermostType;		// Always Leaf or Struct
	FOptionalInnerId				InnermostSchema;	// Enum or struct schema
	TConstArrayView<FRangeBinding>	RangeBindings;		// Non-empty -> Range

	// @pre InnermostSchema isn't type-erased / lowered
	PLAINPROPS_API FBothType		IndexParameterName(FIdIndexerBase& Ids) const;
};

class FSchemaBindings : public IBindIds
{
public:
	UE_NONCOPYABLE(FSchemaBindings);
	explicit FSchemaBindings(FDebugIds In) : Debug(In) {}
	PLAINPROPS_API ~FSchemaBindings();

	PLAINPROPS_API void						BindStruct(FBindId Id, FDeclId DeclId, TConstArrayView<FMemberBinding> Schema);
	PLAINPROPS_API const FSchemaBinding*	FindStruct(FBindId Id) const;
	PLAINPROPS_API const FSchemaBinding&	GetStruct(FBindId Id) const;
	PLAINPROPS_API void						DropStruct(FBindId Id);

	PLAINPROPS_API virtual FDeclId			Lower(FBindId Id) const override final;
private:
	TArray<TUniquePtr<FSchemaBinding>>		Bindings;
	FDebugIds								Debug;
};

////////////////////////////////////////////////////////////////////////////////////////////////

// Lookup default instances when delta-saving struct ranges
struct IDefaultStructs
{
	virtual const void* Get(FBindId Id) = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FBindContext
{
	const FDeclarations&		Declarations;
	const FSchemaBindings&		Schemas;
	FCustomBindings&			Customs;
};

////////////////////////////////////////////////////////////////////////////////////////////////

struct FStructBindIds final : IBindIds
{
	FStructBindIds(const FCustomBindings& InCustoms, const FSchemaBindings& InSchemas)
	: Customs(InCustoms)
	, Schemas(InSchemas)
	{}

	const FCustomBindings& Customs;
	const FSchemaBindings& Schemas;

	PLAINPROPS_API virtual FDeclId Lower(FBindId Id) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////

template<class Ids, class Typename>
FScopeId IndexNamespaceId() requires (Typename::Namespace.size() > 0)
{
	// Opt: Make cached GetNamespaceId(), either via new namespace CTTI types (maybe PP_REFLECT_NAMESPACE)
	//		or some compile time string template parameters, perhaps a variadic template taking any number of chars
	return Ids::IndexScope(ToAnsiView(Typename::Namespace));
}

template<class Ids, class Typename>
FScopeId IndexNamespaceId()
{
	return NoId;
}

template<ETypename Kind, class Typename>
constexpr std::string_view SelectStructName()
{	
	if constexpr (Kind == ETypename::Bind && ExplicitBindName<Typename>)
	{
		return Typename::BindName;
	}
	else
	{
		return Typename::DeclName;	
	}
}

template<class Ids, ETypename Kind, class Typename>
FType IndexStructName()
{
	FType BaseName = { IndexNamespaceId<Ids, Typename>(), 
						 FTypenameId(Ids::IndexTypename(ToAnsiView(SelectStructName<Kind, Typename>()))) };
	
	if constexpr (ParametricName<Typename>)
	{
		return IndexParametricType<Ids, Kind>(BaseName, (typename Typename::Parameters*)nullptr);
	}
	else
	{
		return BaseName;
	}
}

template<class Ids, class Typename>
FBindId IndexStructBindIdIfNeeded(FDeclId DeclId)
{
	if constexpr (ExplicitBindName<Typename> || ParametricName<Typename>)
	{
		// Note could pass in and reuse declared namespace here
		return FBindId(Ids::IndexStruct(IndexStructName<Ids, ETypename::Bind, Typename>()));
	}
	else
	{
		return UpCast(DeclId);
	}
}

template<class Ids, class Typename>
FBothStructId IndexStructBothId(FType DeclName = IndexStructName<Ids, ETypename::Decl, Typename>())
{	
	FBothStructId Out;
	Out.DeclId = FDeclId(Ids::IndexStruct(DeclName));
	Out.BindId = UpCast(Out.DeclId);

	if constexpr (ExplicitBindName<Typename> || ParametricName<Typename>)
	{
		FType BindName = IndexStructName<Ids, ETypename::Bind, Typename>();
		Out.BindId = BindName != DeclName ? FBindId(Ids::IndexStruct(BindName)) : UpCast(Out.DeclId);
	}

	return Out;
}

// Cached by function static
template<class Ids, typename Struct>
FDeclId GetStructDeclId()
{
	static FDeclId Id = FDeclId(Ids::IndexStruct(IndexStructName<Ids, ETypename::Decl, TTypename<Struct>>()));
	return Id;
}

// Cached by function static
template<class Ids, typename Struct>
FBindId GetStructBindId()
{
	static FBindId Id = FBindId(Ids::IndexStruct(IndexStructName<Ids, ETypename::Bind, TTypename<Struct>>()));
	return Id;
}

// Cached by function static
template<class Ids, typename Struct>
FBothStructId GetStructBothId()
{
	static FBothStructId Id = IndexStructBothId<Ids, TTypename<Struct>>();
	return Id;
}

template<class Ids, typename Ctti>
FType IndexCttiName()
{
	FTypenameId Name{Ids::IndexTypename(Ctti::Name)};
	FScopeId Namespace = NoId;
	if constexpr (Ctti::Namespace[0] != '\0')
	{
		Namespace = Ids::IndexScope(ToAnsiView(Ctti::Namespace));
	}
	return { Namespace, Name };
}

// Cached by function static
template<class Ids, typename Enum>
FEnumId GetEnumId()
{
	static FEnumId Id = Ids::IndexEnum(IndexCttiName<Ids, CttiOf<Enum>>());
	return Id;
}

template<class Ids, Arithmetic T>
FType IndexArithmeticName()
{
	static constexpr FUnpackedLeafType Leaf = ReflectArithmetic<T>;
	return { NoId, FTypenameId(Ids::IndexTypename(ToAnsiView(ArithmeticName<Leaf.Type, Leaf.Width>))) };
}

template<class Ids, ETypename, Arithmetic Leaf>
FType IndexParameterName()
{
	return IndexArithmeticName<Ids, Leaf>();
}

template<class Ids, ETypename, Enumeration Enum>
FType IndexParameterName()
{
	return GetEnumId<Ids, Enum>();
}

template<class Ids, ETypename Kind, typename T>
FType IndexParameterName()
{
	using RangeBinding = RangeBind<T>;
	if constexpr (std::is_void_v<RangeBinding>)
	{
		return IndexStructName<Ids, Kind, TTypename<T>>();
	}
	else
	{
		FType ItemParam = IndexParameterName<Ids, Kind, typename RangeBinding::ItemType>();
		FType SizeParam = Ids::GetIndexer().MakeRangeParameter(RangeSizeOf(typename RangeBinding::SizeType{}));

		if constexpr (Kind == ETypename::Decl)
		{
			// Type-erase range type
			return Ids::GetIndexer().MakeAnonymousParametricType({ItemParam, SizeParam});
		}
		else
		{
			using Typename = TTypename<T>;
			FType RangeBindName = { IndexNamespaceId<Ids, Typename>(), FTypenameId(Ids::IndexTypename(ToAnsiView(RangeBinding::BindName))) };
			return Ids::GetIndexer().MakeParametricType(RangeBindName, {ItemParam, SizeParam});
		}
	}
}

template<class Ids, ETypename Kind, typename... Ts>
FType IndexParametricType(FType TemplatedType, const std::tuple<Ts...>*)
{
	FType Parameters[] = { (IndexParameterName<Ids, Kind, Ts>())... };
	return Ids::GetIndexer().MakeParametricType(TemplatedType, Parameters);
}

////////////////////////////////////////////////////////////////////////////////////////////////

// Works around C++'s lack of templated nullary constructors
template<class Ids>
struct TInit {};

////////////////////////////////////////////////////////////////////////////////////////////////

using InnerStructArray = TArray<FInnerStruct, TInlineAllocator<8>>;

struct FCustomInit
{
	InnerStructArray& OutLowered;

	// Only needed for inner structs that might get type-erased/lowered
	void RegisterInnerStruct(FBothStructId Id, TConstArrayView<FMemberId> Names)
	{
		if (Id.IsLowered())
		{
			for (FMemberId Name : Names)
			{
				OutLowered.Add({Name, Id.BindId});
			}
		}
	}
};

// Helps custom binding constructors create ids and register types that might get type-erased 
template<class Ids>
struct TCustomInit : FCustomInit, TInit<Ids>
{};

// Workaround for clang requiring FStructLoadView definition, make it dependent on any other type
template<typename>
struct TDependent { using StructLoadView = FStructLoadView; };

// Scoped custom binding for native type
template<class T, class Runtime, typename CustomBinding = CustomBind<T>>
struct TScopedStructBinding : FBothStructId, CustomBinding
{
	using Ids = typename Runtime::Ids;
	using Typename = TCustomTypename<CustomBinding>::Type;

	explicit TScopedStructBinding(FType DeclName = IndexStructName<Ids, ETypename::Decl, Typename>(), InnerStructArray Lowered = {})
	: TScopedStructBinding(DeclName, IndexStructBothId<Ids, Typename>(DeclName), Lowered)
	{}

	explicit TScopedStructBinding(FBothStructId Id, InnerStructArray Lowered = {})
	: TScopedStructBinding(Ids::GetIndexer().Resolve(Id.DeclId), Id, Lowered)
	{}

	explicit TScopedStructBinding(FDualStructId Id, InnerStructArray Lowered = {})
	: TScopedStructBinding(Ids::GetIndexer().Resolve(Id), FBothStructId{Id, Id}, Lowered)
	{}

	TScopedStructBinding(FType DeclName, FBothStructId BothId, InnerStructArray& Lowered)
	: FBothStructId(BothId)
	, CustomBinding(TCustomInit<Ids>{{Lowered}})
	{
		const FStructDeclaration& Declaration = Runtime::GetTypes().DeclareStruct(DeclId, DeclName, 0, CustomBinding::MemberIds, TOccupancyOf<T>::Value);
		Runtime::GetCustoms().BindStruct(BindId, *this, Declaration, Lowered);
	}

	~TScopedStructBinding()
	{
		Runtime::GetCustoms().DropStruct(BindId);
		Runtime::GetTypes().DropStructRef(DeclId);
	}

	virtual void SaveCustom(FMemberBuilder& Dst, const void* Src, const void* Default, const FSaveContext& Ctx) override
	{
		CustomBinding::Save(Dst, *static_cast<const T*>(Src), static_cast<const T*>(Default), Ctx);
	}

	virtual void LoadCustom(void* Dst, TDependent<Ids>::StructLoadView Src, ECustomLoadMethod Method) const override 
	{
		CustomBinding::Load(*static_cast<T*>(Dst), Src, Method);
	}
	
	virtual bool DiffCustom(const void* A, const void* B, const FBindContext& Ctx) const override
	{
		return CustomBinding::Diff(*static_cast<const T*>(A), *static_cast<const T*>(B), Ctx);
	}

	virtual bool DiffCustom(const void* A, const void* B, FDiffContext& Ctx) const override
	{
		return CustomBinding::Diff(*static_cast<const T*>(A), *static_cast<const T*>(B), Ctx);
	}
};

// Specialization for schemabound structs
template<class T, class Runtime>
struct TScopedStructBinding<T, Runtime, void>
{
	using Ids = typename Runtime::Ids;
	using Typename = TTypename<T>;
	
	const FDeclId DeclId;
	const FBindId BindId;

	TScopedStructBinding(FType DeclName = IndexStructName<Ids, ETypename::Decl, Typename>())
	: DeclId(DeclareNativeStruct<CttiOf<T>, Ids>(Runtime::GetTypes(), TOccupancyOf<T>::Value))
	, BindId(IndexStructBindIdIfNeeded<Ids, Typename>(DeclId))
	{
		BindNativeStruct<CttiOf<T>, Runtime>(Runtime::GetSchemas(), BindId, DeclId);
	}

	~TScopedStructBinding()
	{
		Runtime::GetSchemas().DropStruct(BindId);
		Runtime::GetTypes().DropStructRef(DeclId);
	}
};

template<class Struct, typename CustomBinding, class Runtime>
FBindId BindCustomStructOnce()
{
	static TScopedStructBinding<Struct, Runtime, CustomBinding> Instance;
	return Instance.BindId;
}

inline constexpr FMemberBindType DefaultStructBindType = FMemberBindType(FStructType{EMemberKind::Struct, /* IsDynamic */ 0, /* IsSuper */ 0});
inline constexpr FMemberBindType SuperStructBindType = FMemberBindType(FStructType{EMemberKind::Struct, /* IsDynamic */ 0, /* IsSuper */ 1});

template<class Struct, class CustomBinding, class Runtime>
FMemberBindType BindMemberStruct(FOptionalInnerId& OutSchema)
{
	// Todo: Consider auto-binding all nested structs
	if constexpr (std::is_void_v<CustomBinding>)
	{
		OutSchema = FInnerId(GetStructBindId<typename Runtime::Ids, Struct>());
	}
	else
	{
		OutSchema = FInnerId(BindCustomStructOnce<Struct, CustomBinding, Runtime>());
	}

	return DefaultStructBindType;
}

template<typename Struct, class Ids>
FMemberBindType BindInnermostType(FOptionalInnerId& OutSchema)
{
	OutSchema = FInnerId(GetStructBindId<Ids, Struct>());
	return DefaultStructBindType;
}

template<Arithmetic Type, class Ids>
FMemberBindType BindInnermostType(FOptionalInnerId& OutSchema)
{
	OutSchema = NoId;
	return FMemberBindType(ReflectArithmetic<Type>);
}

template<Enumeration Enum, class Ids>
FMemberBindType BindInnermostType(FOptionalInnerId& OutSchema)
{
	OutSchema = FInnerId(GetEnumId<Ids, Enum>());
	return FMemberBindType(ReflectEnum<Enum>);
}

template<typename RangeBinding>
constexpr uint32 CountRangeBindings()
{
	using InnerBinding = RangeBind<typename RangeBinding::ItemType>;
	return 1 + CountRangeBindings<InnerBinding>();
}

template<>
constexpr uint32 CountRangeBindings<void>()
{
	return 0;
}

template<typename RangeBinding, uint32 NestLevel>
struct TInnermostType
{
	using InnerType = typename RangeBinding::ItemType;
	using Type = TInnermostType<RangeBind<InnerType>, NestLevel - 1>::Type;
};

template<typename RangeBinding>
struct TInnermostType<RangeBinding, 1>
{
	using Type = typename RangeBinding::ItemType;
};

template<typename RangeBinding, typename Ids, uint32 N>
TConstArrayView<FRangeBinding> GetRangeBindings() requires (N > 0)
{
	struct FOnce
	{
		FOnce(FAnsiStringView RangeBindName)
		: Instance(Ids::IndexTypename(RangeBindName))
		, Binding(Instance, RangeSizeOf(typename RangeBinding::SizeType{}))
		{}
		RangeBinding Instance;
		FRangeBinding Binding;
	};

	if constexpr (N == 1)
	{
		static FOnce Static(ToAnsiView(RangeBinding::BindName));
		return MakeArrayView(&Static.Binding, N);
	}
	else
	{
		using InnerType = typename RangeBinding::ItemType;
		using InnerRangeBinding = RangeBind<InnerType>;

		struct FNestedOnce : FOnce
		{
			FNestedOnce()
			: FOnce(ToAnsiView(InnerRangeBinding::BindName))
			{
				FMemory::Memcpy(NestedBindings, GetRangeBindings<InnerRangeBinding, Ids,  N - 1>().GetData(), sizeof(NestedBindings));
			}
			
			uint8 NestedBindings[sizeof(FRangeBinding) * (N - 1)] = {};
		};
		static_assert(std::is_trivially_destructible_v<FRangeBinding>);
		static_assert(offsetof(FNestedOnce, Binding) + sizeof(FRangeBinding) == offsetof(FNestedOnce, NestedBindings));	

		static FNestedOnce Static;
		return MakeArrayView(&Static.Binding, N);
	}
}

template<LeafType Type, class Runtime>
FMemberBinding BindMember(uint64 Offset)
{
	FMemberBinding Out(Offset);
	Out.InnermostType = BindInnermostType<Type, typename Runtime::Ids>(Out.InnermostSchema);
	return Out;
}

template<typename Type, class Runtime>
FMemberBinding BindMember(uint64 Offset)
{
	using Ids = typename Runtime::Ids;
	using CustomBinding = typename Runtime::template CustomBindings<Type>::Type;

	FMemberBinding Out(Offset);
	if constexpr (!std::is_void_v<CustomBinding>)
	{
		Out.InnermostType = BindMemberStruct<Type, CustomBinding, Runtime>(Out.InnermostSchema);
	}
	else
	{
		using RangeBinding = RangeBind<Type>;
		if constexpr (!std::is_void_v<RangeBinding>)
		{
			constexpr uint32 NumRangeBindings = CountRangeBindings<RangeBinding>();
			using InnermostType = typename TInnermostType<RangeBinding, NumRangeBindings>::Type;

			Out.RangeBindings = GetRangeBindings<RangeBinding, Ids, NumRangeBindings>();
			Out.InnermostType = BindInnermostType<InnermostType, Ids>(Out.InnermostSchema);
		}
		else
		{
			Out.InnermostType = BindMemberStruct<Type, CustomBinding, Runtime>(Out.InnermostSchema);
		}
	}
	
	return Out;
}

////////////////////////////////////////////////////////////////////////////////////////////////

template<class Ctti, class Ids>
FEnumId DeclareNativeEnum(FDeclarations& Out, EEnumMode Mode)
{
	using UnderlyingType = std::underlying_type_t<typename Ctti::Type>;

	FType Type = IndexCttiName<Ids, Ctti>();
	FEnumId Id = Ids::IndexEnum(Type);
	FEnumerator Enumerators[Ctti::NumEnumerators];
	for (FEnumerator& Enumerator : Enumerators)
	{
		Enumerator.Name = Ids::IndexName(Ctti::Enumerators[&Enumerator - Enumerators].Name);
		Enumerator.Constant = static_cast<uint64>(static_cast<UnderlyingType>(Ctti::Enumerators[&Enumerator - Enumerators].Constant));
	}
	Out.DeclareEnum(Id, Type, Mode, Enumerators, EEnumAliases::Fail);

	return Id;
}

template<class Ctti, class Ids>
FStructId DeclareNativeStruct(FDeclarations& Out, EMemberPresence Occupancy)
{
	using Typename = TTypename<typename Ctti::Type>;
	using SuperType = typename Ctti::Super;

	FType Type = IndexStructName<Ids, ETypename::Decl, Typename>();
	FDeclId Id(Ids::IndexStruct(Type));
	FOptionalDeclId SuperId;
	if constexpr (!std::is_void_v<SuperType>)
	{
		SuperId = GetStructDeclId<Ids, SuperType>();
	}

	FMemberId MemberIds[Ctti::NumVars];
	ForEachVar<Ctti>([&]<class Var>()
	{ 
		MemberIds[Var::Index] = Ids::IndexMember(Var::Name);
	});
	Out.DeclareStruct(Id, Type, 0, MemberIds, Occupancy, SuperId);

	return Id;
}

template<class Ctti, class Runtime>
void BindNativeStruct(FSchemaBindings& Out, FBindId BindId, FDeclId DeclId)
{
	FMemberBinding MemberBindings[Ctti::NumVars];
	ForEachVar<Ctti>([&]<class Var>()
	{ 
		MemberBindings[Var::Index] = BindMember<typename Var::Type, Runtime>(Var::Offset);
	});
	Out.BindStruct(BindId, DeclId, MemberBindings);
}

//////////////////////////////////////////////////////////////////////////

// Save -> load struct ids for ESchemaFormat::InMemoryNames, alternative to side-channel with ExtractRuntimeIds()
[[nodiscard]] PLAINPROPS_API TArray<FStructId> IndexRuntimeIds(const FSchemaBatch& Schemas,  FIdIndexerBase& Indexer);

// Save -> load ids for ESchemaFormat::StableNames
struct FIdBinding
{
	TConstArrayView<FNameId>			Names;
	TConstArrayView<FNestedScopeId>		NestedScopes;
	TConstArrayView<FParametricTypeId>	ParametricTypes;
	TConstArrayView<FInnerId>			Schemas;

	FNameId								Remap(FNameId Old) const				{ return Names[Old.Idx]; }
	FMemberId							Remap(FMemberId Old) const				{ return { Remap(Old.Id) }; }
	FFlatScopeId						Remap(FFlatScopeId Old) const			{ return { Remap(Old.Name) }; }
	FNestedScopeId						Remap(FNestedScopeId Old) const			{ return NestedScopes[Old.Idx]; }
	FScopeId							Remap(FScopeId Old) const				{ return Old.IsFlat() ? FScopeId(Remap(Old.AsFlat())) : Old ? FScopeId(Remap(Old.AsNested())) : Old; }
	FConcreteTypenameId					Remap(FConcreteTypenameId Old) const	{ return { Remap(Old.Id) }; }	
	FParametricTypeId					Remap(FParametricTypeId Old) const		{ return ParametricTypes[Old.Idx]; }
	FTypenameId							Remap(FTypenameId Old) const			{ return Old.IsConcrete() ? FTypenameId(Remap(Old.AsConcrete())) : FTypenameId(Remap(Old.AsParametric())); }
	FType								Remap(FType Old) const				{ return { Remap(Old.Scope), Remap(Old.Name) }; }

	template<typename T>
	TOptionalId<T>						Remap(TOptionalId<T> Old) const			{ return Old ? ToOptional(Remap(Old.Get())) : Old; }

	TConstArrayView<FStructId>			GetStructIds(int32 NumStructs) const
	{
		// All saved struct schema ids are lower than enum schema ids
		check(NumStructs <= Schemas.Num());
		return MakeArrayView(reinterpret_cast<const FStructId*>(Schemas.GetData()), NumStructs);
	}
};

struct FIdTranslatorBase
{
	PLAINPROPS_API static uint32 CalculateTranslationSize(int32 NumSavedNames, const FSchemaBatch& Batch);
	PLAINPROPS_API static FIdBinding TranslateIds(FMutableMemoryView To, FIdIndexerBase& Indexer, TConstArrayView<FNameId> TranslatedNames, const FSchemaBatch& From);
};

// Maps saved ids -> runtime load ids for ESchemaFormat::StableNames
struct FIdTranslator : FIdTranslatorBase
{
	template<class NameType>
	FIdTranslator(TIdIndexer<NameType>& Indexer, TConstArrayView<NameType> SavedNames, const FSchemaBatch& Batch)
	{
		Allocator.SetNumUninitialized(CalculateTranslationSize(SavedNames.Num(), Batch));
		
		// Translate names
		TArrayView<FNameId> NewNames(reinterpret_cast<FNameId*>(&Allocator[0]), SavedNames.Num());
		FNameId* NameIt = NewNames.GetData();
		for (const NameType& SavedName : SavedNames)
		{
			(*NameIt++) = Indexer.MakeName(SavedName);
		}

		FMutableMemoryView OtherIds(NameIt, Allocator.Num() - NewNames.Num() * sizeof(FNameId));
		Translation = TranslateIds(/* out */ OtherIds, Indexer, NewNames, Batch);
	}
	
	FIdBinding								Translation;
	TArray<uint8, TInlineAllocator<1024>>	Allocator;
};

PLAINPROPS_API FSchemaBatch*	CreateTranslatedSchemas(const FSchemaBatch& Schemas, FIdBinding NewIds);
PLAINPROPS_API void				DestroyTranslatedSchemas(const FSchemaBatch* Schemas);

} // namespace PlainProps

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsTypes.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Templates/UniquePtr.h"

namespace PlainProps
{

struct FEnumerator
{
	FNameId					Name;
	uint64					Constant;
};

enum class EEnumMode { Flat, Flag };

struct FEnumDeclaration
{
	FType					Type;			// Could be removed
	EEnumMode				Mode;
	//EOptionalLeafWidth	Width;			// Possible approach to declare enums w/o any noted values
	uint16					NumEnumerators;
	FEnumerator				Enumerators[0];	// Constants must be unique, no aliases allowed

	TConstArrayView<FEnumerator> GetEnumerators() const
	{
		return MakeArrayView(Enumerators, NumEnumerators);
	}
};

enum class EMemberPresence : uint8 { RequireAll, AllowSparse };

struct FStructDeclaration
{
	uint32					RefCount;
	FDeclId					Id;				// Could be removed, might allow declaration dedup among templated types
	FType					Type;			// Could be removed, might allow declaration dedup among templated types
	FOptionalDeclId			Super;
	uint16					Version;
	uint16					NumMembers;
	EMemberPresence			Occupancy;
	FMemberId				MemberOrder[0];

	TConstArrayView<FMemberId> GetMemberOrder() const
	{
		return MakeArrayView(MemberOrder, NumMembers);
	}

	inline static constexpr uint16 MaxMembers = 0xFFFF;
};

// Enum values are stored as integers. Aliased enum values are illegal, including composite flags.
// Aliases can be automatically removed on declaration or detected and fail hard.
enum class EEnumAliases { Strip, Fail };

class FDeclarations
{
public:
	UE_NONCOPYABLE(FDeclarations);
	explicit FDeclarations(FDebugIds In) : Debug(In) {}

	PLAINPROPS_API const FEnumDeclaration&			DeclareEnum(FEnumId Id, FType Type, EEnumMode Mode, TConstArrayView<FEnumerator> Enumerators, EEnumAliases Policy);
	// Declare struct with ref count 1 or increment it and check that previous declaration matches
	PLAINPROPS_API const FStructDeclaration&		DeclareStruct(FDeclId Id, FType Type, uint16 Version, TConstArrayView<FMemberId> MemberOrder, EMemberPresence Occupancy, FOptionalDeclId Super = {});
	PLAINPROPS_API const FStructDeclaration&		DeclareNumeralStruct(FDeclId Id, FType Type, TConstArrayView<FMemberId> Numerals, EMemberPresence Occupancy);
	
	void											DropEnum(FEnumId Id)			{ Check(Id); DeclaredEnums[Id.Idx].Reset(); }
	PLAINPROPS_API void								DropStructRef(FDeclId Id);

	const FEnumDeclaration&							Get(FEnumId Id) const			{ Check(Id); return *DeclaredEnums[Id.Idx]; }
	const FStructDeclaration&						Get(FDeclId Id) const			{ Check(Id); return *DeclaredStructs[Id.Idx]; }
	PLAINPROPS_API const FStructDeclaration*		Find(FDeclId Id) const;
	
	TConstArrayView<TUniquePtr<FEnumDeclaration>>	GetEnums() const				{ return DeclaredEnums; }
	TConstArrayView<TUniquePtr<FStructDeclaration>>	GetStructs() const				{ return DeclaredStructs; }
	FDebugIds										GetDebug() const				{ return Debug; }

protected:
	TArray<TUniquePtr<FEnumDeclaration>>			DeclaredEnums;
	TArray<TUniquePtr<FStructDeclaration>>			DeclaredStructs;
	FDebugIds										Debug;

#if DO_CHECK
	PLAINPROPS_API void								Check(FEnumId Id) const;
	PLAINPROPS_API void								Check(FDeclId Id) const;
#else
	void											Check(...) const {}
#endif
};

} // namespace PlainProps

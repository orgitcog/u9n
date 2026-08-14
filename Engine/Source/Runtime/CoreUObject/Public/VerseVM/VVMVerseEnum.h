// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Utf8String.h"
#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEnumeration.h"
#endif
#include "VVMVerseEnum.generated.h"

class UEnumCookedMetaData;

namespace uLang
{
class CEnumeration;
}

UENUM(Flags)
enum class EVerseEnumFlags : uint32
{
	None = 0x00000000u,
	NativeBound = 0x00000001u,
	UHTNative = 0x00000002u,
};
ENUM_CLASS_FLAGS(EVerseEnumFlags)

UCLASS(MinimalAPI)
class UVerseEnum : public UEnum
{
	GENERATED_BODY()
public:
	COREUOBJECT_API UVerseEnum(const FObjectInitializer& ObjectInitialzer);

	UPROPERTY()
	EVerseEnumFlags VerseEnumFlags = EVerseEnumFlags::None;

	UPROPERTY()
	FUtf8String QualifiedName;

	// UObject interface.
	COREUOBJECT_API virtual void Serialize(FArchive& Ar) override;
	COREUOBJECT_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	bool IsUHTNative() const
	{
		return EnumHasAnyFlags(VerseEnumFlags, EVerseEnumFlags::UHTNative);
	}

	void SetNativeBound()
	{
		VerseEnumFlags |= EVerseEnumFlags::NativeBound;
	}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	Verse::TWriteBarrier<Verse::VEnumeration> Enumeration;
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UEnumCookedMetaData> CachedCookedMetaDataPtr;
#endif // WITH_EDITORONLY_DATA
};

/** Corresponds to "false" in Verse, a type with no possible values. */
UENUM()
enum class EVerseFalse : uint8
{
	Value // UHT doesn't correctly support empty enums, so we need a dummy case to make it compile.
};

/** Corresponds to "true" in Verse, a type with one possible value: false. */
UENUM()
enum class EVerseTrue : uint8
{
	Value // UHT errors if this is called "False".
};

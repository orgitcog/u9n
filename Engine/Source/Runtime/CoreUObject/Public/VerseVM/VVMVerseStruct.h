// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Utf8String.h"
#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "VerseVM/VVMVerseClassFlags.h"
#include "VerseVM/VVMVerseEffectSet.h"
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "UObject/GarbageCollection.h"
#include "VerseVM/VVMWriteBarrier.h"
#endif
#include "VVMVerseStruct.generated.h"

class UStructCookedMetaData;
class UVerseClass;
namespace Verse
{
struct VClass;
struct VShape;
} // namespace Verse

UCLASS(MinimalAPI)
class UVerseStruct : public UScriptStruct
{
	GENERATED_BODY()

	COREUOBJECT_API virtual void Serialize(FArchive& Ar) override;

	/** Creates the field/property links and gets structure ready for use at runtime */
	COREUOBJECT_API virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	COREUOBJECT_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	COREUOBJECT_API virtual uint32 GetStructTypeHash(const void* Src) const override;

public:
	UVerseStruct() = default;
	COREUOBJECT_API explicit UVerseStruct(
		const FObjectInitializer& ObjectInitializer,
		UScriptStruct* InSuperStruct,
		ICppStructOps* InCppStructOps = nullptr,
		EStructFlags InStructFlags = STRUCT_NoFlags,
		SIZE_T ExplicitSize = 0,
		SIZE_T ExplicitAlignment = 0);
	COREUOBJECT_API explicit UVerseStruct(const FObjectInitializer& ObjectInitializer);

	/** EVerseClassFlags */
	UPROPERTY()
	uint32 VerseClassFlags = 0;

	UPROPERTY()
	FUtf8String QualifiedName;

	virtual FGuid GetCustomGuid() const override
	{
		return Guid;
	}

	/** Function used for initialization */
	UPROPERTY()
	TObjectPtr<UFunction> InitFunction;

	/** Parent module class */
	UPROPERTY()
	TObjectPtr<UVerseClass> ModuleClass;

	/** GUID to be able to match old version of this struct to new one */
	UPROPERTY()
	FGuid Guid;

	UPROPERTY()
	TObjectPtr<UFunction> FactoryFunction;

	UPROPERTY()
	TObjectPtr<UFunction> OverrideFactoryFunction;

	UPROPERTY()
	EVerseEffectSet ConstructorEffects = EVerseEffectSet::None;

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	static COREUOBJECT_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	COREUOBJECT_API void AssembleReferenceTokenStream(bool bForce = false);

	UPROPERTY()
	Verse::TWriteBarrier<Verse::VClass> Class;

	Verse::TWriteBarrier<Verse::VShape> Shape;

	/** GC schema if needed, finalized in AssembleReferenceTokenStream */
	UE::GC::FSchemaOwner ReferenceSchema;
#endif

	COREUOBJECT_API virtual FString GetAuthoredNameForField(const FField* Field) const override;

	COREUOBJECT_API void InvokeDefaultFactoryFunction(uint8* InStructData) const;

	bool IsUHTNative() const { return (VerseClassFlags & VCLASS_UHTNative) != VCLASS_None; }

	void SetNativeBound()
	{
		VerseClassFlags |= VCLASS_NativeBound;
	}

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UStructCookedMetaData> CachedCookedMetaDataPtr;
#endif // WITH_EDITORONLY_DATA
};

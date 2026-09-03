// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/NotNull.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "VerseVM/VVMVerseClassFlags.h"
#include "VerseVM/VVMVerseEffectSet.h"
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMShape.h"
#endif
#include "VVMVerseClass.generated.h"

class UClassCookedMetaData;

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
namespace Verse
{
struct VPackage;
}
#endif

USTRUCT()
struct FVersePersistentVar
{
	GENERATED_BODY()

	FVersePersistentVar(FString Path, TFieldPath<FMapProperty> Property)
		: Path(::MoveTemp(Path))
		, Property(::MoveTemp(Property))
	{
	}

	FVersePersistentVar() = default;

	UPROPERTY()
	FString Path;
	UPROPERTY()
	TFieldPath<FMapProperty> Property;
};

USTRUCT()
struct FVerseSessionVar
{
	GENERATED_BODY()

	explicit FVerseSessionVar(TFieldPath<FMapProperty> Property)
		: Property(::MoveTemp(Property))
	{
	}

	FVerseSessionVar() = default;

	UPROPERTY()
	TFieldPath<FMapProperty> Property;
};

USTRUCT()
struct FVerseClassVarAccessor
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UFunction> Func{};

	UPROPERTY()
	bool bIsInstanceMember{false};

	UPROPERTY()
	bool bIsFallible{false};
};

USTRUCT()
struct FVerseClassVarAccessors
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<int, FVerseClassVarAccessor> Getters;

	UPROPERTY()
	TMap<int, FVerseClassVarAccessor> Setters;
};

struct FVerseFunctionDescriptor
{
	UObject* Owner = nullptr;
	UFunction* Function = nullptr; // May be nullptr even when valid
	FName DisplayName = NAME_None;
	FName UEName = NAME_None;

	FVerseFunctionDescriptor() = default;

	FVerseFunctionDescriptor(
		UObject* InOwner,
		UFunction* InFunction,
		FName InDisplayName,
		FName InUEName)
		: Owner(InOwner)
		, Function(InFunction)
		, DisplayName(InDisplayName)
		, UEName(InUEName)
	{
	}

	operator bool() const
	{
		return Owner != nullptr;
	}
};

// This class is deliberately simple (i.e. POD) to keep generated code size down.
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
struct FVerseCallableThunk
{
	const char* NameUTF8;
	Verse::VNativeFunction::FThunkFn Pointer;
};
#endif

UCLASS(MinimalAPI, within = Package, Config = Engine)
class UVerseClass : public UClass
{
	GENERATED_BODY()

public:
	UVerseClass() = default;
	explicit UVerseClass(
		EStaticConstructor,
		FName InName,
		uint32 InSize,
		uint32 InAlignment,
		EClassFlags InClassFlags,
		EClassCastFlags InClassCastFlags,
		const TCHAR* InClassConfigName,
		EObjectFlags InFlags,
		ClassConstructorType InClassConstructor,
		ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
		FUObjectCppClassStaticFunctions&& InCppClassStaticFunctions);
	explicit UVerseClass(const FObjectInitializer& ObjectInitializer);

public:
	//~ Begin UObjectBaseUtility interface
	COREUOBJECT_API virtual UE::Core::FVersePath GetVersePath() const override;
	//~ End UObjectBaseUtility interface

#if WITH_EDITORONLY_DATA
	UE_INTERNAL COREUOBJECT_API virtual void TrackDefaultInitializedProperties(void* DefaultData) const override;
#endif

private:
	//~ Begin UObject interface
	virtual bool IsAsset() const override;
	COREUOBJECT_API virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	COREUOBJECT_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	COREUOBJECT_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	//~ End UObject interface

	//~ Begin UStruct interface
	COREUOBJECT_API virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	COREUOBJECT_API virtual void PreloadChildren(FArchive& Ar) override;
	COREUOBJECT_API virtual FProperty* CustomFindProperty(const FName InName) const override;
	COREUOBJECT_API virtual FString GetAuthoredNameForField(const FField* Field) const override;
	COREUOBJECT_API virtual bool SupportsDynamicInstancedReference() const override;
	//~ End UStruct interface

	//~ Begin UClass interface
	COREUOBJECT_API virtual void PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph) override;
	COREUOBJECT_API virtual void PostLoadInstance(UObject* InObj) override;
	virtual bool CanCreateAssetOfClass() const override
	{
		return false;
	}
#if WITH_EDITORONLY_DATA
	COREUOBJECT_API virtual bool CanCreateInstanceDataObject() const override;
	COREUOBJECT_API virtual void SerializeDefaultObject(UObject* Object, FStructuredArchive::FSlot Slot) override;
#endif
#if WITH_EDITOR
	COREUOBJECT_API virtual FTopLevelAssetPath GetReinstancedClassPathName_Impl() const;
#endif
	//~ End UClass interface

	// UField interface.
	COREUOBJECT_API virtual const TCHAR* GetPrefixCPP() const override;
	// End of UField interface.

public:
	UPROPERTY()
	uint32 SolClassFlags = VCLASS_None;

	// All coroutine task classes belonging to this class (one for each coroutine in this class)
	UPROPERTY()
	TArray<TObjectPtr<UVerseClass>> TaskClasses;

	/** Initialization function */
	UPROPERTY()
	TObjectPtr<UFunction> InitInstanceFunction;

	UPROPERTY()
	TArray<FVersePersistentVar> PersistentVars;

	UPROPERTY()
	TArray<FVerseSessionVar> SessionVars;

	UPROPERTY()
	TMap<FName, FVerseClassVarAccessors> VarAccessors;

	UPROPERTY()
	EVerseEffectSet ConstructorEffects;

	UPROPERTY()
	FName MangledPackageVersePath; // Storing as FName since it's shared between classes

	UPROPERTY()
	FString PackageRelativeVersePath;

	//~ This map is technically wrong since the FName is caseless...
	UPROPERTY()
	TMap<FName, FName> DisplayNameToUENameFunctionMap;

	// All interface class types that this class implements
	UPROPERTY()
	TArray<TObjectPtr<UVerseClass>> DirectInterfaces;

	UPROPERTY()
	TArray<TFieldPath<FProperty>> PropertiesWrittenByInitCDO;

	// Store a mapping from all previous function mangled names used by the
	// code generator to the current version of name mangling.  Store
	// NAME_None if there are multiple possible current versions for any
	// previous version.  If a previous function mangled name matches the
	// current mangled name, nothing is stored.
	UPROPERTY()
	TMap<FName, FName> FunctionMangledNames;

	UPROPERTY()
	TArray<FName> PredictsFunctionNames;

#if WITH_VERSE_COMPILER && WITH_EDITORONLY_DATA
	/** Path name this class had before it was marked as DEAD */
	FString PreviousPathName;
#endif // WITH_VERSE_COMPILER && WITH_EDITORONLY_DATA

	COREUOBJECT_API static const FName NativeParentClassTagName;
	COREUOBJECT_API static const FName PackageVersePathTagName;
	COREUOBJECT_API static const FName PackageRelativeVersePathTagName;

	// Name of the CDO init function
	COREUOBJECT_API static const FName InitCDOFunctionName;
	COREUOBJECT_API static const FName StructPaddingDummyName;

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	static Verse::FOpResult LoadField(Verse::FAllocationContext Context, UObject* Object, Verse::VUniqueString& FieldName);
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	UPROPERTY()
	Verse::TWriteBarrier<Verse::VClass> Class;

	Verse::TWriteBarrier<Verse::VShape> Shape;
#endif

	/**
	 * Renames default sub-objects on a CDO so that they're unique (named after properties they are assigned to)
	 * @param  InObject Object (usually a CDO) whose default sub-objects are to be renamed
	 */
	COREUOBJECT_API static void RenameDefaultSubobjects(UObject* InObject);

	/**
	 * Checks that the sub-objects of a given Verse object are using the correct sub-archetype.
	 * @param  InObject Object whose default sub-objects we are validating
	 * @param  InArchetype The archetype of InObject
	 */
	COREUOBJECT_API static bool ValidateSubobjectArchetypes(UObject* InObject, UObject* InArchetype);

	// Delegate for detecting unresolved properties during reinstancing
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPropertyRemoved, const UVerseClass* Class, FName PropertyName);
	COREUOBJECT_API static FOnPropertyRemoved OnPropertyRemoved;

	void SetNeedsSubobjectInstancingForLoadedInstances(bool bNeedsInstancing)
	{
		bNeedsSubobjectInstancingForLoadedInstances = bNeedsInstancing;
	}

	// Allows dynamic instanced reference support to be toggled on/off for this class.
	COREUOBJECT_API void EnableDynamicInstancedReferenceSupport();
	COREUOBJECT_API void DisableDynamicInstancedReferenceSupport();

	bool IsUniversallyAccessible() const { return (SolClassFlags & VCLASS_UniversallyAccessible) != VCLASS_None; }
	bool IsVerseModule() const { return (SolClassFlags & VCLASS_Module) != VCLASS_None; }
	bool IsConcrete() const { return (SolClassFlags & VCLASS_Concrete) != VCLASS_None; }
	bool IsEpicInternal() const { return (SolClassFlags & VCLASS_EpicInternal) != VCLASS_None; }
	bool IsConstructorEpicInternal() const { return (SolClassFlags & VCLASS_EpicInternalConstructor) != VCLASS_None; }
	bool IsFinalSuper() const { return (SolClassFlags & VCLASS_FinalSuper) != VCLASS_None; }
	bool IsExplicitlyCastable() const { return (SolClassFlags & VCLASS_Castable) != VCLASS_None; }
	bool HasInstancedSemantics() const { return (SolClassFlags & VCLASS_HasInstancedSemantics) != VCLASS_None; }
	bool IsUHTNative() const { return (SolClassFlags & VCLASS_UHTNative) != VCLASS_None; }

	void SetNativeBound() { SolClassFlags |= VCLASS_NativeBound; }

	const FVerseClassVarAccessors* FindAccessors(FName VarName) const
	{
		const UVerseClass* VerseClass = this;
		while (VerseClass)
		{
			if (const FVerseClassVarAccessors* Accessors = VerseClass->VarAccessors.Find(VarName))
			{
				return Accessors;
			}

			VerseClass = Cast<UVerseClass>(VerseClass->GetSuperClass());
		}
		return nullptr;
	}

	bool CanMemberFunctionBeCalledFromPredicts(FName FuncName)
	{
		const UVerseClass* VerseClass = this;
		while (VerseClass)
		{
			if (VerseClass->PredictsFunctionNames.Contains(FuncName))
			{
				return true;
			}

			VerseClass = Cast<UVerseClass>(VerseClass->GetSuperClass());
		}
		return false;
	}

	/**
	 * Iterates over Verse Function Properties on an object instance and executes a callback with VerseFunction value and its Verse name.
	 * @param Object Object instance to iterate Verse Functions for
	 * @param Operation callback for each of the found Verse Functions. When the callback returns false, iteration is stopped.
	 * @param IterationFlags Additional options used when iterating over Verse Function properties
	 */
	COREUOBJECT_API void ForEachVerseFunction(UObject* Object, TFunctionRef<bool(FVerseFunctionDescriptor)> Operation, EFieldIterationFlags IterationFlags = EFieldIterationFlags::None);

	FName GetFunctionMangledName(FName MangledName)
	{
		if (FName* NewMangledName = FindFunctionMangledName(MangledName))
		{
			return *NewMangledName;
		}
		return MangledName;
	}

	FName* FindFunctionMangledName(FName MangledName)
	{
		if (FName* NewMangledName = FindClassFunctionMangledName(MangledName))
		{
			return NewMangledName;
		}
		if (FName* NewMangledName = FindInterfaceFunctionMangledName(MangledName))
		{
			return NewMangledName;
		}
		return nullptr;
	}

	FName* FindInterfaceFunctionMangledName(FName MangledName)
	{
		for (const FImplementedInterface& Interface : Interfaces)
		{
			if (UVerseClass* SuperVerseClass = Cast<UVerseClass>(Interface.Class))
			{
				if (FName* NewMangledName = SuperVerseClass->FunctionMangledNames.Find(MangledName))
				{
					// @note there may not be two interface methods where one does not override
					// the other that share the same old mangled name, as the function name is
					// based on the base overridden definition.
					return NewMangledName;
				}
			}
		}
		return nullptr;
	}

	FName* FindClassFunctionMangledName(FName MangledName)
	{
		if (FName* NewMangledName = FunctionMangledNames.Find(MangledName))
		{
			return NewMangledName;
		}
		if (UVerseClass* SuperVerseClass = Cast<UVerseClass>(GetSuperClass()))
		{
			return SuperVerseClass->FindClassFunctionMangledName(MangledName);
		}
		return nullptr;
	}

	void AddFunctionMangledNames(FName OldMangledName, FName NewMangledName)
	{
		if (OldMangledName != NewMangledName)
		{
			if (FName* OtherNewMangledName = FindFunctionMangledName(OldMangledName))
			{
				if (*OtherNewMangledName != NewMangledName)
				{
					FunctionMangledNames.Add(OldMangledName, NAME_None);
				}
			}
			else
			{
				FunctionMangledNames.Add(OldMangledName, NewMangledName);
			}
		}
	}

	/**
	 * Returns a VerseFunction value given its display name
	 * @param Object Object instance to iterate Verse Functions for
	 * @param VerseName Display name of the function
	 * @param SearchFlags Additional options used when iterating over Verse Function properties
	 * @return VerseFunction value acquired from the provided Object instance or invalid function value if none was found.
	 */
#if WITH_VERSE_BPVM
	COREUOBJECT_API FVerseFunctionDescriptor FindVerseFunctionByDisplayName(UObject* Object, const FString& DisplayName, EFieldIterationFlags SearchFlags = EFieldIterationFlags::None);
#endif // WITH_VERSE_BPVM

	/**
	 * Returns the number of parameters a verse function takes
	 */
	COREUOBJECT_API static int32 GetVerseFunctionParameterCount(UFunction* Func);

	struct FStaleClassInfo
	{
		TObjectPtr<UVerseClass> SourceClass;
		TMap<FName, FName> DisplayNameToUENameFunctionMap;
		TMap<FName, FName> FunctionMangledNames;
		TArray<TObjectPtr<UVerseClass>> TaskClasses;
		TArray<TKeyValuePair<FName, TObjectPtr<UField>>> Children;
	};

	// Reset the contents of the UHT class and return the reset information so it can be restored if the compiled failed.
	// Being able to restore will probably not be needed once BPVM is removed
	COREUOBJECT_API FStaleClassInfo ResetUHTNative();

	// Strip verse generated functions from the function list and place into the output container for later restoring
	COREUOBJECT_API void StripVerseGeneratedFunctions(TArray<TKeyValuePair<FName, TObjectPtr<UField>>>* StrippedFields);

#if WITH_VERSE_BPVM
	COREUOBJECT_API void BindVerseFunction(const char* DecoratedFunctionName, FNativeFuncPtr NativeThunkPtr);
	COREUOBJECT_API void BindVerseCoroClass(const char* DecoratedFunctionName, FNativeFuncPtr NativeThunkPtr);
#endif

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	COREUOBJECT_API void SetVerseCallableThunks(const FVerseCallableThunk* InThunks, uint32 NumThunks);
	COREUOBJECT_API void BindVerseCallableFunctions(Verse::VPackage* VersePackage, FUtf8StringView VerseScopePath);
#endif

private:
	COREUOBJECT_API void CallInitInstanceFunctions(UObject* InObj, FObjectInstancingGraph* InstanceGraph);
	COREUOBJECT_API void CallPropertyInitInstanceFunctions(UObject* InObj, FObjectInstancingGraph* InstanceGraph);
	COREUOBJECT_API void InstanceNewSubobjects(TNotNull<UObject*> InObj);

	COREUOBJECT_API void AddPersistentVars(UObject*);

	COREUOBJECT_API void AddSessionVars(UObject*);

	/** True if this class needs to run subobject instancing on loaded instances of classes (by default the engine does not run subobject instancing on instances that are being loaded) */
	bool bNeedsSubobjectInstancingForLoadedInstances = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UClassCookedMetaData> CachedCookedMetaDataPtr;
#endif // WITH_EDITORONLY_DATA

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	TArray<FVerseCallableThunk> VerseCallableThunks;
#endif
};

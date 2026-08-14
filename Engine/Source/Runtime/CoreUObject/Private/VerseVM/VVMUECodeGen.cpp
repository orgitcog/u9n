// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMUECodeGen.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectConstructInternal.h"
#include "UObject/UObjectGlobals.h"
#include "VerseVM/VVMVerseClass.h"
#include "VerseVM/VVMVerseEnum.h"
#include "VerseVM/VVMVerseFunction.h"
#include "VerseVM/VVMVerseStruct.h"

namespace Verse::CodeGen::Private
{
void ConstructUVerseClassNoInit(
	const TCHAR* PackageName,
	const TCHAR* Name,
	UClass*& ReturnClass,
	void (*RegisterNativeFunc)(),
	uint32 InSize,
	uint32 InAlignment,
	EClassFlags InClassFlags,
	EClassCastFlags InClassCastFlags,
	const TCHAR* InConfigName,
	UClass::ClassConstructorType InClassConstructor,
	UClass::ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
	FUObjectCppClassStaticFunctions&& InCppClassStaticFunctions,
	UClass::StaticClassFunctionType InSuperClassFn,
	UClass::StaticClassFunctionType InWithinClassFn)
{
	UECodeGen_Private::ConstructUClassNoInitHelper<UVerseClass>(
		PackageName,
		Name,
		ReturnClass,
		RegisterNativeFunc,
		InSize,
		InAlignment,
		InClassFlags,
		InClassCastFlags,
		InConfigName,
		InClassConstructor,
		InClassVTableHelperCtorCaller,
		MoveTemp(InCppClassStaticFunctions),
		InSuperClassFn,
		InWithinClassFn,
		[](UVerseClass* VerseClass) {
			VerseClass->SolClassFlags |= VCLASS_UHTNative;
		});
}

void ConstructUVerseEnum(UEnum*& OutEnum, const UECodeGen_Private::FVerseEnumParams& Params)
{
	UECodeGen_Private::ConstructUEnumHelper<UVerseEnum>(OutEnum, Params, [](UVerseEnum* VerseEnum, const UECodeGen_Private::FVerseEnumParams& Params) {
		EnumAddFlags(VerseEnum->VerseEnumFlags, EVerseEnumFlags::UHTNative);
		VerseEnum->QualifiedName = Params.QaulifiedName;
	});
}

void ConstructUVerseStruct(UScriptStruct*& OutStruct, const UECodeGen_Private::FVerseStructParams& Params)
{
	UECodeGen_Private::ConstructUScriptStructHelper<UVerseStruct>(OutStruct, Params, [](UVerseStruct* VerseStruct, const UECodeGen_Private::FVerseStructParams& Params) {
		VerseStruct->VerseClassFlags |= VCLASS_UHTNative;
		VerseStruct->Guid = FGuid(FCrc::Strihash_DEPRECATED(*VerseStruct->GetName()), GetTypeHash(VerseStruct->GetPackage()->GetName()), 0, 0);
		VerseStruct->QualifiedName = Params.QaulifiedName;
	});
}

void ConstructUVerseFunction(UFunction** SingletonPtr, const UECodeGen_Private::FVerseFunctionParams& Params)
{
	UECodeGen_Private::ConstructUFunctionHelper<UVerseFunction>(*SingletonPtr, Params, SingletonPtr,
		[](UObject* Outer, UFunction* Super, FName FuncName, const UECodeGen_Private::FVerseFunctionParams& Params) -> UFunction* {
			UVerseFunction* NewFunction = new (EC_InternalUseOnlyConstructor, Outer, FuncName, Params.ObjectFlags) UVerseFunction(
				FObjectInitializer(),
				Super,
				Params.FunctionFlags,
				Params.StructureSize);
			NewFunction->AlternateName = FName(UTF8_TO_TCHAR(Params.AlternateName));
			NewFunction->VerseFunctionFlags |= EVerseFunctionFlags::UHTNative;
			return NewFunction;
		});
}

void ConstructUVerseClass(UClass*& OutClass, const UECodeGen_Private::FVerseClassParams& Params)
{
	UECodeGen_Private::ConstructUClassHelper<UVerseClass>(OutClass, Params, [](UVerseClass* VerseClass, const UECodeGen_Private::FVerseClassParams& Params) {
		if (int32 NumImplementedInterfaces = Params.NumImplementedInterfaces)
		{
			for (const UECodeGen_Private::FImplementedInterfaceParams *ImplementedInterface = Params.ImplementedInterfaceArray, *ImplementedInterfaceEnd = ImplementedInterface + NumImplementedInterfaces;
				 ImplementedInterface != ImplementedInterfaceEnd; ++ImplementedInterface)
			{
				if (ImplementedInterface->bDirectInterface)
				{
					UClass* (*ClassFunc)() = ImplementedInterface->ClassFunc;
					UClass* InterfaceClass = ClassFunc ? ClassFunc() : nullptr;
					if (InterfaceClass != nullptr)
					{
						VerseClass->DirectInterfaces.Add(CastChecked<UVerseClass>(InterfaceClass));
					}
				}
			}
		}

		if (!EnumHasAnyFlags(VerseClass->ClassFlags, CLASS_Interface))
		{
			VerseClass->PackageRelativeVersePath = VerseClass->GetFName().ToString();
		}
		VerseClass->MangledPackageVersePath = FName(UTF8_TO_TCHAR(Params.MangledPackageVersePath));
	});
}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
COREUOBJECT_API void RegisterVerseCallableThunks(UClass* Class, const FVerseCallableThunk* InThunks, uint32 InNumThunks)
{
	UVerseClass* VerseClass = static_cast<UVerseClass*>(Class);
	VerseClass->SetVerseCallableThunks(InThunks, InNumThunks);
}
#endif

} // namespace Verse::CodeGen::Private
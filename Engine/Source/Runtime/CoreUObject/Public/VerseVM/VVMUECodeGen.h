// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMVerseClass.h"
#endif

class UEnum;
class UScriptStruct;

namespace UECodeGen_Private
{
struct FEnumParams;
struct FStructParams;
} // namespace UECodeGen_Private

namespace Verse::CodeGen::Private
{
/**
 * Construct but do not initialize a UVerseClass
 *
 * @param PackageName name of the package this class will be inside
 * @param Name of the class
 * @param ReturnClass reference to pointer to result. This must be PrivateStaticClass.
 * @param RegisterNativeFunc Native function registration function pointer.
 * @param InSize Size of the class
 * @param InAlignment Alignment of the class
 * @param InClassFlags Class flags
 * @param InClassCastFlags Class cast flags
 * @param InConfigName Class config name
 * @param InClassConstructor Class constructor function pointer
 * @param InClassVTableHelperCtorCaller Class constructor function for vtable pointer
 * @param InCppClassStaticFunctions Function pointers for the class's version of Unreal's reflected static functions
 * @param InSuperClassFn Super class function pointer
 * @param InWithinClassFn Within class function pointer
 */
COREUOBJECT_API void ConstructUVerseClassNoInit(
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
	UClass::StaticClassFunctionType InWithinClassFn);

COREUOBJECT_API void ConstructUVerseEnum(UEnum*& OutEnum, const UECodeGen_Private::FVerseEnumParams& Params);
COREUOBJECT_API void ConstructUVerseStruct(UScriptStruct*& OutStruct, const UECodeGen_Private::FVerseStructParams& Params);
COREUOBJECT_API void ConstructUVerseFunction(UFunction** SingletonPtr, const UECodeGen_Private::FVerseFunctionParams& Params);
COREUOBJECT_API void ConstructUVerseClass(UClass*& OutClass, const UECodeGen_Private::FVerseClassParams& Params);

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
COREUOBJECT_API void RegisterVerseCallableThunks(UClass* Class, const FVerseCallableThunk* InThunks, uint32 InNumThunks);
#endif
} // namespace Verse::CodeGen::Private
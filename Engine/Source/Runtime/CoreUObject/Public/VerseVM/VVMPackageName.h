// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VerseVM/VVMNames.h"

#define UE_API COREUOBJECT_API

class UObject;

enum class EVersePackageScope : uint8;
enum class EVersePackageType : uint8;

namespace Verse
{

class FPackageName
{
public:
	// The following methods are being deprecated and should use Verse::Names methods found in VVMNames.h
	static UE_API FString GetVersePackageNameForVni(const TCHAR* MountPointName, const TCHAR* CppModuleName);
	static UE_API FString GetVersePackageNameForContent(const TCHAR* MountPointName);
	static UE_API FString GetVersePackageNameForPublishedContent(const TCHAR* MountPointName);
	static UE_API FString GetVersePackageNameForAssets(const TCHAR* MountPointName);
	static UE_API FString GetVersePackageDirForContent(const TCHAR* MountPointName);
	static UE_API FString GetVersePackageDirForAssets(const TCHAR* MountPointName);

	// The following methods don't have a Verse::Names version yet
	static UE_API FName GetVersePackageNameFromUPackagePath(FName UPackagePath, EVersePackageType* OutPackageType = nullptr);
	static UE_API FString GetMountPointName(const TCHAR* VersePackageName);
	static UE_API FName GetCppModuleName(const TCHAR* VersePackageName);
	static UE_API EVersePackageType GetPackageType(const TCHAR* VersePackageName);
	static UE_API EVersePackageType GetPackageType(const UTF8CHAR* VersePackageName);

	static UE_API FString GetTaskUClassName(const TCHAR* OwnerScopeName, const TCHAR* DecoratedAndMangledFunctionName);
	static UE_API FString GetTaskUClassName(const UObject& OwnerScope, const TCHAR* DecoratedAndMangledFunctionName);

	static UE_API bool PackageRequiresInternalAPI(const char* Name, const EVersePackageScope VerseScope);

	// Class name substitute for root module classes of a package
	static constexpr char const* const RootModuleClassName = "_Root"; // Keep in sync with RootModuleClassName in NativeInterfaceWriter.cpp

	// Prefix Constants
	static constexpr TCHAR const* const TaskUClassPrefix = TEXT("task_");
};

} // namespace Verse

#undef UE_API

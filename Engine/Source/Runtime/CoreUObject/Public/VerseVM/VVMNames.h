// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "Containers/Utf8String.h"
#include "CoreMinimal.h"
#include "VerseVM/VVMPackageTypes.h"

namespace Verse::Names
{
static constexpr int32 DefaultNameLength = 64;

//--------------------------------------------------------------------------------------------------------------------
// Private helper methods that should not be used outside of Verse/Solaris code
//--------------------------------------------------------------------------------------------------------------------

namespace Private
{

//--------------------------------------------------------------------------------------------------------------------
// Name mangling to make a cased name a cassless name
//--------------------------------------------------------------------------------------------------------------------

// The following method are intended to take a case sensitive name (which maybe already adorned with package information)
// and convert it into a case insensitive name. This is done by prepending a CRC of the name. The Crc is calculated on the
// CrcName, which is a qualified name in the case of fields in interfaces. Otherwise it's the same as Name.
COREUOBJECT_API FString MangleCasedName(FStringView Name, FStringView CrcName, bool* bOutNameWasMangled = nullptr);
COREUOBJECT_API FString UnmangleCasedName(const FName MaybeMangledName, bool* bOutNameWasMangled = nullptr);

//--------------------------------------------------------------------------------------------------------------------
// Encoding and decoding
//--------------------------------------------------------------------------------------------------------------------

// Encode and decode a verse names.  This is currently only used to encode functions.
// The method takes characters that could be considered invalid for UE names and makes them valid
COREUOBJECT_API FUtf8String EncodeName(FUtf8StringView Path);
COREUOBJECT_API FString EncodeName(FStringView Path);
COREUOBJECT_API FUtf8String DecodeName(FUtf8StringView Path);
COREUOBJECT_API FString DecodeName(FStringView Path);
} // namespace Private

//--------------------------------------------------------------------------------------------------------------------
// String constants
//--------------------------------------------------------------------------------------------------------------------

// Create Get methods to return the given constant strings
#define UE_MAKE_CONSTANT_STRING_METHODS(Name, Text)   \
	template <typename CharType>                      \
	const CharType* Get##Name();                      \
	template <>                                       \
	FORCEINLINE const UTF8CHAR* Get##Name<UTF8CHAR>() \
	{                                                 \
		return UTF8TEXT(Text);                        \
	}                                                 \
	template <>                                       \
	FORCEINLINE const TCHAR* Get##Name<TCHAR>()       \
	{                                                 \
		return TEXT(Text);                            \
	}

UE_MAKE_CONSTANT_STRING_METHODS(VerseSubPath, "_Verse")
UE_MAKE_CONSTANT_STRING_METHODS(VniSubPath, "VNI")
UE_MAKE_CONSTANT_STRING_METHODS(AssetsSubPath, "Assets")
UE_MAKE_CONSTANT_STRING_METHODS(AssetsSubPathForPackageName, "Assets")
UE_MAKE_CONSTANT_STRING_METHODS(PublishedPackageNameSuffix, "-Published")

#undef UE_MAKE_CONSTANT_STRING_METHODS

//--------------------------------------------------------------------------------------------------------------------
// UE Package names for Verse
//--------------------------------------------------------------------------------------------------------------------

// The following methods assist in generating the UE package names

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetDecoratedName(TStringView<CharType> Path, TStringView<CharType> Module, TStringView<CharType> Name);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetDecoratedName(FUtf8StringView Path, FUtf8StringView Module, FUtf8StringView Name);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetDecoratedName(FStringView Path, FStringView Module, FStringView Name);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetDecoratedName(TStringView<CharType> Path, TStringView<CharType> Name);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetDecoratedName(FUtf8StringView Path, FUtf8StringView Name);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetDecoratedName(FStringView Path, FStringView Name);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForVni(TStringView<CharType> MountPointName, TStringView<CharType> CppModuleName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetVersePackageNameForVni(FUtf8StringView MountPointName, FUtf8StringView CppModuleName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetVersePackageNameForVni(FStringView MountPointName, FStringView CppModuleName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForContent(TStringView<CharType> MountPointName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetVersePackageNameForContent(FUtf8StringView MountPointName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetVersePackageNameForContent(FStringView MountPointName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForPublishedContent(TStringView<CharType> MountPointName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetVersePackageNameForPublishedContent(FUtf8StringView MountPointName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetVersePackageNameForPublishedContent(FStringView MountPointName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForAssets(TStringView<CharType> MountPointName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetVersePackageNameForAssets(FUtf8StringView MountPointName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetVersePackageNameForAssets(FStringView MountPointName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageDirForContent(TStringView<CharType> MountPointName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetVersePackageDirForContent(FUtf8StringView MountPointName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetVersePackageDirForContent(FStringView MountPointName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageDirForAssets(TStringView<CharType> MountPointName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetVersePackageDirForAssets(FUtf8StringView MountPointName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetVersePackageDirForAssets(FStringView MountPointName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUPackagePathForVni(TStringView<CharType> MountPointName, TStringView<CharType> CppModuleName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetUPackagePathForVni(FUtf8StringView MountPointName, FUtf8StringView CppModuleName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetUPackagePathForVni(FStringView MountPointName, FStringView CppModuleName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUPackagePathForContent(TStringView<CharType> MountPointName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetUPackagePathForContent(FUtf8StringView MountPointName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetUPackagePathForContent(FStringView MountPointName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUPackagePathForAssets(TStringView<CharType> MountPointName);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetUPackagePathForAssets(FUtf8StringView MountPointName);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetUPackagePathForAssets(FStringView MountPointName);

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUPackagePath(TStringView<CharType> VersePackageName, EVersePackageType* OutPackageType = nullptr);
extern template COREUOBJECT_API TUtf8StringBuilder<DefaultNameLength> GetUPackagePath(FUtf8StringView VersePackageName, EVersePackageType* OutPackageType);
extern template COREUOBJECT_API TStringBuilder<DefaultNameLength> GetUPackagePath(FStringView VersePackageName, EVersePackageType* OutPackageType);

//--------------------------------------------------------------------------------------------------------------------
// Verse path helper methods
//--------------------------------------------------------------------------------------------------------------------

// Test to see if the given path is a full Verse path (begins with open parenthesis)
inline bool IsFullPath(FUtf8StringView Name)
{
	return Name.Len() > 0 && Name[0] == UTF8CHAR('(');
}
inline bool IsFullPath(FStringView Name)
{
	return Name.Len() > 0 && Name[0] == TCHAR('(');
}
inline FUtf8StringView RemoveQualifier(FUtf8StringView QualifiedName)
{
	int32 Index = 0;
	if (QualifiedName.FindChar(UTF8CHAR(')'), Index))
	{
		QualifiedName.RemovePrefix(Index + 1);
	}
	return QualifiedName;
}
inline FStringView RemoveQualifier(FStringView QualifiedName)
{
	int32 Index = 0;
	if (QualifiedName.FindChar(')', Index))
	{
		QualifiedName.RemovePrefix(Index + 1);
	}
	return QualifiedName;
}

//--------------------------------------------------------------------------------------------------------------------
// Property name conversions
//
// NOTE: VVMULangNames.h contains helper methods specific to uLang types
//--------------------------------------------------------------------------------------------------------------------

// Convert a Verse property name to a UE name as a string
// If bWasVerseName is true, then the name needed to be modified to be used as a UE name
COREUOBJECT_API FString VersePropToUEName(FStringView VerseName, FStringView CrcVerseName, bool* bWasVerseName = nullptr);
COREUOBJECT_API FString VersePropToUEName(FStringView VerseName, bool* bWasVerseName = nullptr);

// Convert a Verse property name to a UE name as an FName.  If the resulting name is too long, the engine will check.
// If bWasVerseName is true, then the name needed to be modified to be used as a UE name
COREUOBJECT_API FName VersePropToUEFName(FStringView VerseName, FStringView CrcVerseName, bool* bWasVerseName = nullptr);
COREUOBJECT_API FName VersePropToUEFName(FStringView VerseName, bool* bWasVerseName = nullptr);

// Convert a UE property name to the original Verse name.
// WARNING: The resulting string is case sensitive and should NEVER be converted to an FName
// If bIsVerseName is true, then the UE name was originally a verse name.
COREUOBJECT_API FString UEPropToVerseName(FStringView UEName, bool* bIsVerseName = nullptr);
COREUOBJECT_API FString UEPropToVerseName(FName UEName, bool* bIsVerseName = nullptr);

// WARNING: This version is commonly used to signal that the code is depending on the verse name
// being stored in an FName which is not valid.
COREUOBJECT_API FName UEPropToVerseFName(FStringView UEName, bool* bIsVerseName = nullptr);
COREUOBJECT_API FName UEPropToVerseFName(FName UEName, bool* bIsVerseName = nullptr);

//--------------------------------------------------------------------------------------------------------------------
// Function name conversions
//
// NOTE: VVMULangNames.h contains helper methods specific to uLang types
//--------------------------------------------------------------------------------------------------------------------

// Convert a Verse function name to a UE name as a string
COREUOBJECT_API FString VerseFuncToUEName(FStringView VerseName);

// Convert a Verse function name to a UE name as an FName.  If the resulting name is too long, the engine will check.
COREUOBJECT_API FName VerseFuncToUEFName(FStringView VerseName);

// Convert a UE function name to the original Verse name.
// WARNING: The resulting string is case sensitive and should NEVER be converted to an FName
// If bIsVerseName is true, then the UE name was originally a verse name.
COREUOBJECT_API FString UEFuncToVerseName(FStringView UEName);
COREUOBJECT_API FString UEFuncToVerseName(FName UEName);

//--------------------------------------------------------------------------------------------------------------------
// Methods to handle the VERSE_DEAD_ prefix
//--------------------------------------------------------------------------------------------------------------------

// Return the verse dead prefix
COREUOBJECT_API FStringView GetVerseDeadPrefix();

// Test to see if the given name has the prefix
COREUOBJECT_API bool HasVerseDeadPrefix(FStringView Name);

// Add the verse dead prefix.  Will not add a prefix if one already exists
COREUOBJECT_API FString AddVerseDeadPrefix(FStringView Name);

// Remove the verse dead prefix if it exists
COREUOBJECT_API FStringView RemoveVerseDeadPrefix(FStringView Name);

// Rename the given type to dead and move it to the given new outer
COREUOBJECT_API void MakeTypeDead(UObject* Object, UObject* NewOuter);
} // namespace Verse::Names

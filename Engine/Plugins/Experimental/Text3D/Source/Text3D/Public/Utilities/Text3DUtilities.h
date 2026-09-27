// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Text3DTypes.h"

class UFontFace;
class UFont;
struct FText3DTypeFaceMetrics;
struct FTypefaceEntry;

/** Utilities to manipulate text font */
namespace UE::Text3D::Utilities
{
	/** Get proper font name without invalid chars */
	TEXT3D_API bool GetSanitizeFontName(const UFont* InFont, FString& OutFontName);

	/** Get font name based on object/import/legacy name */
	TEXT3D_API bool GetFontName(const UFont* InFont, FString& OutFontName);

	/** Replace invalid object name chars and spaces */
	TEXT3D_API void SanitizeFontName(FString& InOutFontName);

	/** Detect font style (italic, bold, monospaced) */
	TEXT3D_API bool GetFontStyle(const UFont* InFont, EText3DFontStyleFlags& OutFontStyleFlags);
	
	/** Detect font style (italic, bold, monospaced) */
	TEXT3D_API bool GetFontStyle(const FText3DFontFamily& InFontFamily, EText3DFontStyleFlags& OutFontStyleFlags);
	
	/** Get font faces that compose the font */
	TEXT3D_API bool GetFontFaces(const UFont* InFont, TArray<UFontFace*>& OutFontFaces);
}
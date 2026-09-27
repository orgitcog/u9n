// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/Text3DUtilities.h"

#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Logs/Text3DLogs.h"
#include "Misc/Paths.h"
#include "Text3DModule.h"

#if WITH_FREETYPE
THIRD_PARTY_INCLUDES_START
#include "ft2build.h"
#include FT_FREETYPE_H
THIRD_PARTY_INCLUDES_END
#endif

bool UE::Text3D::Utilities::GetSanitizeFontName(const UFont* InFont, FString& OutFontName)
{
	if (GetFontName(InFont, OutFontName))
	{
		SanitizeFontName(OutFontName);
		return true;
	}

	return false;
}

bool UE::Text3D::Utilities::GetFontName(const UFont* InFont, FString& OutFontName)
{
	if (!IsValid(InFont))
	{
		return false;
	}
	
	FString FontAssetName;
	FString FontImportName = InFont->ImportOptions.FontName;

	if (InFont->GetFName() == NAME_None)
	{
		FontAssetName = InFont->LegacyFontName.ToString();
	}
	else
	{
		FontAssetName = InFont->GetName();
	}

	// Roboto fonts are actually from the Arial family and their import name is "Arial", so we try to list them as well
	// this will likely lead to missing spaces in their names
	if (FontAssetName.Contains(TEXT("Roboto")) || FontImportName == TEXT("Arial"))
	{
		OutFontName = FontAssetName;
	}
	else
	{
		OutFontName = FontImportName;
	}

	return !OutFontName.IsEmpty();
}

void UE::Text3D::Utilities::SanitizeFontName(FString& InOutFontName)
{
	// spaces would be also handled by code below, but spaces used to be removed
	// so let's to this anyway in order to avoid any potential issues with new vs. old imported fonts

	const TCHAR* InvalidObjectChar = INVALID_OBJECTNAME_CHARACTERS;
	while (*InvalidObjectChar)
	{
		InOutFontName.ReplaceCharInline(*InvalidObjectChar, TCHAR(' '), ESearchCase::CaseSensitive);
		++InvalidObjectChar;
	}

	const TCHAR* InvalidPackageChar = INVALID_LONGPACKAGE_CHARACTERS;
	while (*InvalidPackageChar)
	{
		InOutFontName.ReplaceCharInline(*InvalidPackageChar, TCHAR(' '), ESearchCase::CaseSensitive);
		++InvalidPackageChar;
	}

	InOutFontName.RemoveSpacesInline();
}

bool UE::Text3D::Utilities::GetFontStyle(const UFont* InFont, EText3DFontStyleFlags& OutFontStyleFlags)
{
#if WITH_FREETYPE
	if (!IsValid(InFont))
	{
		return false;
	}

	const FCompositeFont* const CompositeFont = InFont->GetCompositeFont();
	if (!CompositeFont || CompositeFont->DefaultTypeface.Fonts.IsEmpty())
	{
		return false;
	}
	
	OutFontStyleFlags = EText3DFontStyleFlags::None;

	for (const FTypefaceEntry& TypefaceEntry : CompositeFont->DefaultTypeface.Fonts)
	{
		const FFontFaceDataConstPtr FaceData = TypefaceEntry.Font.GetFontFaceData();

		if (FaceData.IsValid() && FaceData->HasData() && FaceData->GetData().Num() > 0)
		{
			const TArray<uint8> Data = FaceData->GetData();

			FT_Face FreeTypeFace;
			FT_New_Memory_Face(FText3DModule::GetFreeTypeLibrary(), Data.GetData(), Data.Num(), 0, &FreeTypeFace);

			if (FreeTypeFace)
			{
				if (FreeTypeFace->style_flags & FT_STYLE_FLAG_ITALIC)
				{
					EnumAddFlags(OutFontStyleFlags, EText3DFontStyleFlags::Italic);
				}

				if (FreeTypeFace->style_flags & FT_STYLE_FLAG_BOLD)
				{
					EnumAddFlags(OutFontStyleFlags, EText3DFontStyleFlags::Bold);
				}

				if (FreeTypeFace->style_flags & FT_FACE_FLAG_FIXED_WIDTH)
				{
					EnumAddFlags(OutFontStyleFlags, EText3DFontStyleFlags::Monospace);
				}
				
				FT_Done_Face(FreeTypeFace);
			}
		}
	}

	{
		int32 Height;

		int32 SpaceWidth;
		InFont->GetStringHeightAndWidth(TEXT(" "), Height, SpaceWidth);

		int32 LWidth;
		InFont->GetStringHeightAndWidth(TEXT("l"), Height, LWidth);

		int32 WWidth;
		InFont->GetStringHeightAndWidth(TEXT("W"), Height, WWidth);

		if ((SpaceWidth == LWidth) && (SpaceWidth == WWidth))
		{
			EnumAddFlags(OutFontStyleFlags, EText3DFontStyleFlags::Monospace);
		}
	}

	return true;
#else
	UE_LOG(LogText3D, Warning, TEXT("FreeType is not available, cannot proceed without it"))
	return false;
#endif
}

bool UE::Text3D::Utilities::GetFontStyle(const FText3DFontFamily& InFontFamily, EText3DFontStyleFlags& OutFontStyleFlags)
{
	OutFontStyleFlags = EText3DFontStyleFlags::None;

#if WITH_FREETYPE
	if (InFontFamily.FontFacePaths.IsEmpty())
	{
		return false;
	}

	for (const TPair<FString, FString>& FontFace : InFontFamily.FontFacePaths)
	{
		if (FPaths::FileExists(FontFace.Value))
		{
			FT_Face FreeTypeFace;
			FT_New_Face(FText3DModule::GetFreeTypeLibrary(), TCHAR_TO_ANSI(*FontFace.Value), 0, &FreeTypeFace);

			if (FreeTypeFace)
			{
				if (FreeTypeFace->style_flags & FT_STYLE_FLAG_ITALIC)
				{
					EnumAddFlags(OutFontStyleFlags, EText3DFontStyleFlags::Italic);
				}

				if (FreeTypeFace->style_flags & FT_STYLE_FLAG_BOLD)
				{
					EnumAddFlags(OutFontStyleFlags, EText3DFontStyleFlags::Bold);
				}

				if (FreeTypeFace->style_flags & FT_FACE_FLAG_FIXED_WIDTH)
				{
					EnumAddFlags(OutFontStyleFlags, EText3DFontStyleFlags::Monospace);
				}

				FT_Done_Face(FreeTypeFace);
			}
		}
	}

	return true;
#else
	UE_LOG(LogText3D, Warning, TEXT("FreeType is not available, cannot proceed without it"))
	return false;
#endif
}

bool UE::Text3D::Utilities::GetFontFaces(const UFont* InFont, TArray<UFontFace*>& OutFontFaces)
{
	OutFontFaces.Empty();
	
	if (!IsValid(InFont))
	{
		return false;
	}

	for (const FTypefaceEntry& TypefaceEntry : InFont->GetCompositeFont()->DefaultTypeface.Fonts)
	{
		if (const UFontFace* FontFace = Cast<UFontFace>(TypefaceEntry.Font.GetFontFaceAsset()))
		{
			OutFontFaces.Add(const_cast<UFontFace*>(FontFace));
		}
	}

	return !OutFontFaces.IsEmpty();
}

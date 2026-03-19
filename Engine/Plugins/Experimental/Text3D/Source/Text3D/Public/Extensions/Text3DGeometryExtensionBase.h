// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Text3DExtensionBase.h"
#include "Text3DTypes.h"
#include "Text3DGeometryExtensionBase.generated.h"

class FFreeTypeFace;
class UStaticMesh;
class UText3DCharacterBase;
struct FTypefaceFontData;

/** Extension that handles geometry data for Text3D */
UCLASS(MinimalAPI, Abstract)
class UText3DGeometryExtensionBase : public UText3DExtensionBase
{
	GENERATED_BODY()

public:
	UText3DGeometryExtensionBase()
		: UText3DExtensionBase(/** Priority */1)
	{}

	virtual UStaticMesh* FindOrLoadGlyphMesh(UText3DCharacterBase* InCharacter) const
	{
		return nullptr;
	}

	virtual EText3DHorizontalTextAlignment GetGlyphHAlignment() const
	{
		return EText3DHorizontalTextAlignment::Left;
	}

	virtual EText3DVerticalTextAlignment GetGlyphVAlignment() const
	{
		return EText3DVerticalTextAlignment::Bottom;
	}

	virtual FTypefaceFontData* GetTypefaceFontData() const
	{
		return nullptr;
	}
};

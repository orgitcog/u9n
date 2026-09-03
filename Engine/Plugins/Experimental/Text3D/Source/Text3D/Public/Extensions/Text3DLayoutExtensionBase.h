// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Text3DExtensionBase.h"
#include "Text3DTypes.h"
#include "Text3DLayoutExtensionBase.generated.h"

struct FText3DShapedGlyphText;

/** Extension that handles layout data for Text3D */
UCLASS(MinimalAPI, Abstract)
class UText3DLayoutExtensionBase : public UText3DExtensionBase
{
	GENERATED_BODY()

public:
	UText3DLayoutExtensionBase()
		: UText3DExtensionBase(/** Priority */2)
	{}

	virtual float GetTextHeight() const
	{
		return 0.f;
	}

	virtual FVector GetTextScale() const
	{
		return FVector::OneVector;
	}

	virtual FVector GetLineLocation(int32 LineIndex) const
	{
		return FVector::ZeroVector;
	}
};

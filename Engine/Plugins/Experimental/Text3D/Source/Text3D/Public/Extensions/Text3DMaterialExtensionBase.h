// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Text3DExtensionBase.h"
#include "Text3DTypes.h"
#include "Text3DMaterialExtensionBase.generated.h"

class UMaterialInterface;

/** Extension that handles materials for Text3D */
UCLASS(MinimalAPI, Abstract)
class UText3DMaterialExtensionBase : public UText3DExtensionBase
{
	GENERATED_BODY()

public:
	UText3DMaterialExtensionBase()
		: UText3DExtensionBase(/** Priority */3)
	{}

	/** Set the material for a specific group */
	virtual void SetMaterial(EText3DGroupType InGroup, UMaterialInterface* InMaterial)
	{
		
	}

	/** Get the material for a specific group */
	virtual UMaterialInterface* GetMaterial(EText3DGroupType InGroup) const
	{
		return nullptr;
	}
};

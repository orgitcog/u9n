// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Text3DEffectExtensionBase.h"
#include "Text3DLayoutEffectBase.generated.h"

/** Extension for layout effects on Text3D */
UCLASS(MinimalAPI, Abstract)
class UText3DLayoutEffectBase : public UText3DEffectExtensionBase
{
	GENERATED_BODY()

public:
	UText3DLayoutEffectBase()
		: UText3DEffectExtensionBase(/** Priority */3)
	{}
};

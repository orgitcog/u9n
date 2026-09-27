// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotNull.h"

class UMaterialInstance;
class UMaterialInstanceConstant;

namespace UE::MetaHuman::PaletteUnpackHelpers
{
	/**
	 * Creates a material instance constant from the given material instance. Parameters are only copied if they differ from the base material.
	 * This prevents the new material from having all of its parameters overridden
	 */
	METAHUMANCHARACTERPALETTEEDITOR_API UMaterialInstanceConstant* CreateMaterialInstanceCopy(TNotNull<const UMaterialInstance*> InMaterialInstance, TNotNull<UObject*> InOuter);
}

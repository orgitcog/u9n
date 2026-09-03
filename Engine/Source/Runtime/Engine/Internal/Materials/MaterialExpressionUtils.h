// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * MaterialExpressionUtils - Utility functions for the material translators
 */

#pragma once

#include "CoreMinimal.h"


struct FMaterialExternalCodeDeclaration;

namespace MaterialExpressionUtils
{

	FString FormatUnsupportedMaterialDomainError(const FMaterialExternalCodeDeclaration& InExternalCode, const FName& AssetPathName = NAME_None);

} // MaterialExpressionUtils

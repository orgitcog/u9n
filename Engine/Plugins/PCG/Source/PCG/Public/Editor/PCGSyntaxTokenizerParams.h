// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"

/** Parameters to create a PCG syntax tokenizer. */
struct FPCGSyntaxTokenizerParams
{
	TArray<FString> AdditionalKeywords;
};
#endif

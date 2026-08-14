// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGHLSLSyntaxTokenizer.h"

FPCGHLSLSyntaxTokenizer::FPCGHLSLSyntaxTokenizer(const FPCGSyntaxTokenizerParams& InParams)
	: FHlslSyntaxTokenizer()
{
	Keywords.Append(InParams.AdditionalKeywords);
}

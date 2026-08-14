// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FNamingTokenData;

namespace UE::NamingTokens::Utils
{
	/** Return the signature function all blueprint token processing should use. */
	NAMINGTOKENS_API UFunction* GetProcessTokenFunctionSignature();

	/** Validate that a token function can be used for token processing. */
	NAMINGTOKENS_API bool ValidateTokenFunction(const UFunction* InFunction);

	/** Formats a token key into {key}. */
	NAMINGTOKENS_API FString CreateFormattedToken(const FNamingTokenData& InToken);

	/** Retrieve the delimiter for the namespace. */
	NAMINGTOKENS_API FString GetNamespaceDelimiter();
	
	/** Extract all token keys from a string. */
	NAMINGTOKENS_API TArray<FString> GetTokenKeysFromString(const FString& InTokenString);

	/** Checks if a token is present in a string. */
	NAMINGTOKENS_API bool IsTokenInString(const FString& InTokenKey, const FString& InTokenString);
	
	/**
	 * Return the namespace from a token key, or an empty string.
	 * @param InTokenKey The token key shouldn't have any additional formatting beyond the namespace, eg "namespace.token"
	 * @return The namespace, or an empty string.
	 */
	NAMINGTOKENS_API FString GetNamespaceFromTokenKey(const FString& InTokenKey);
	
	/**
	 * Remove the namespace from the given token key.
	 * @param InTokenKey The token key shouldn't have any additional formatting beyond the namespace, eg "namespace.token"
	 * @return The token key without the namespace, or the InTokenKey if there was no namespace.
	 */
	NAMINGTOKENS_API FString RemoveNamespaceFromTokenKey(const FString& InTokenKey);

	/** Validates a name used for a token key or namespace. */
	NAMINGTOKENS_API bool ValidateName(const FString& InName, FText& OutErrorMessage);
}
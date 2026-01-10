// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NamingTokens.h"

#include "CaptureManagerTemplateTokens.generated.h"

namespace UE::CaptureManager
{
struct FArchiveToken
{
	FString Name;
	FText Description;
};

namespace GeneralTokens
{
static constexpr FStringView IdKey = TEXTVIEW("id");
static constexpr FStringView DeviceKey = TEXTVIEW("device");
static constexpr FStringView SlateKey = TEXTVIEW("slate");
static constexpr FStringView TakeKey = TEXTVIEW("take");
}

namespace VideoEncoderTokens
{
static constexpr FStringView InputKey = TEXTVIEW("input");
static constexpr FStringView OutputKey = TEXTVIEW("output");
static constexpr FStringView ParamsKey = TEXTVIEW("params");
}

namespace AudioEncoderTokens
{
static constexpr FStringView InputKey = TEXTVIEW("input");
static constexpr FStringView OutputKey = TEXTVIEW("output");
}
}

UCLASS(NotBlueprintable)
class CAPTUREMANAGERSETTINGS_API UCaptureManagerGeneralTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerGeneralTokens();

	UE::CaptureManager::FArchiveToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& OutTokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FArchiveToken> GeneralTokens;
};


UCLASS(NotBlueprintable)
class CAPTUREMANAGERSETTINGS_API UCaptureManagerVideoEncoderTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerVideoEncoderTokens();

	UE::CaptureManager::FArchiveToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& OutTokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FArchiveToken> VideoEncoderTokens;
};


UCLASS(NotBlueprintable)
class CAPTUREMANAGERSETTINGS_API UCaptureManagerAudioEncoderTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerAudioEncoderTokens();

	UE::CaptureManager::FArchiveToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& OutTokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FArchiveToken> AudioEncoderTokens;
};

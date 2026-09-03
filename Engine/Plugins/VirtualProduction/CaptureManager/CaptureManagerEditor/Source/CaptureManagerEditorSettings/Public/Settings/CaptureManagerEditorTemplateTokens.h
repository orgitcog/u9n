// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NamingTokens.h"

#include "CaptureManagerEditorTemplateTokens.generated.h"

namespace UE::CaptureManager
{

struct FIngestToken
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

namespace VideoTokens
{
static constexpr FStringView NameKey = TEXTVIEW("name");
static constexpr FStringView FrameRateKey = TEXTVIEW("frameRate");
}

namespace AudioTokens
{
static constexpr FStringView NameKey = TEXTVIEW("name");
}

namespace CalibTokens
{
static constexpr FStringView NameKey = TEXTVIEW("name");
}

namespace LensFileTokens
{
static constexpr FStringView CameraNameKey = TEXTVIEW("cameraName");
}
}

UCLASS()
class CAPTUREMANAGEREDITORSETTINGS_API UCaptureManagerIngestNamingTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerIngestNamingTokens();

	UE::CaptureManager::FIngestToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& Tokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FIngestToken> IngestGeneralTokens;
};

UCLASS()
class CAPTUREMANAGEREDITORSETTINGS_API UCaptureManagerVideoNamingTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerVideoNamingTokens();

	UE::CaptureManager::FIngestToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& Tokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FIngestToken> IngestVideoTokens;
};

UCLASS()
class CAPTUREMANAGEREDITORSETTINGS_API UCaptureManagerAudioNamingTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerAudioNamingTokens();

	UE::CaptureManager::FIngestToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& Tokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FIngestToken> IngestAudioTokens;
};

UCLASS()
class CAPTUREMANAGEREDITORSETTINGS_API UCaptureManagerCalibrationNamingTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerCalibrationNamingTokens();

	UE::CaptureManager::FIngestToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& Tokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FIngestToken> IngestCalibTokens;
};

UCLASS()
class CAPTUREMANAGEREDITORSETTINGS_API UCaptureManagerLensFileNamingTokens final : public UNamingTokens
{
	GENERATED_BODY()

public:
	UCaptureManagerLensFileNamingTokens();

	UE::CaptureManager::FIngestToken GetToken(const FString& InKey) const;

protected:
	// ~Begin UNamingTokens
	virtual void OnCreateDefaultTokens(TArray<FNamingTokenData>& Tokens) override;
	virtual void OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData) override;
	virtual void OnPostEvaluate_Implementation() override;
	// ~End UNamingTokens

private:
	TMap<FString, UE::CaptureManager::FIngestToken> IngestLensFileTokens;
};


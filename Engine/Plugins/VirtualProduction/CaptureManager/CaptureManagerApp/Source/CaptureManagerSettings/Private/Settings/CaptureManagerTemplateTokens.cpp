// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/CaptureManagerTemplateTokens.h"

#define LOCTEXT_NAMESPACE "CaptureManagerNamingTokens"


UCaptureManagerGeneralTokens::UCaptureManagerGeneralTokens()
{
	Namespace = TEXT("cpman");

	GeneralTokens.Emplace(FString(UE::CaptureManager::GeneralTokens::IdKey), { FString(UE::CaptureManager::GeneralTokens::IdKey), LOCTEXT("ArchiveId", "Archive Unique Id") });
	GeneralTokens.Emplace(FString(UE::CaptureManager::GeneralTokens::DeviceKey), { FString(UE::CaptureManager::GeneralTokens::DeviceKey), LOCTEXT("ArchiveDeviceId", "Archive Device User Id") });
	GeneralTokens.Emplace(FString(UE::CaptureManager::GeneralTokens::SlateKey), { FString(UE::CaptureManager::GeneralTokens::SlateKey), LOCTEXT("ArchiveSlate", "Archive Slate") });
	GeneralTokens.Emplace(FString(UE::CaptureManager::GeneralTokens::TakeKey), { FString(UE::CaptureManager::GeneralTokens::TakeKey), LOCTEXT("ArchiveTake", "Archive Take") });
}

UE::CaptureManager::FArchiveToken UCaptureManagerGeneralTokens::GetToken(const FString& InKey) const
{
	check(GeneralTokens.Contains(InKey));
	return GeneralTokens[InKey];
}

void UCaptureManagerGeneralTokens::OnCreateDefaultTokens(TArray<FNamingTokenData>& OutTokens)
{
	Super::OnCreateDefaultTokens(OutTokens);

	for (const TPair<FString, UE::CaptureManager::FArchiveToken>& Token : GeneralTokens)
	{
		OutTokens.Add({
			Token.Value.Name,
			Token.Value.Description,
			FNamingTokenData::FTokenProcessorDelegateNative::CreateLambda([Name = Token.Value.Name] {
					return FText::FromString(Name);
				})
			});
	}

}

void UCaptureManagerGeneralTokens::OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData)
{
	Super::OnPreEvaluate_Implementation(InEvaluationData);
}

void UCaptureManagerGeneralTokens::OnPostEvaluate_Implementation()
{
	Super::OnPostEvaluate_Implementation();
}


UCaptureManagerVideoEncoderTokens::UCaptureManagerVideoEncoderTokens()
{
	Namespace = TEXT("cmvidenc");

	VideoEncoderTokens.Emplace(FString(UE::CaptureManager::VideoEncoderTokens::InputKey), { FString(UE::CaptureManager::VideoEncoderTokens::InputKey), LOCTEXT("VideoInputPath", "Input File Path")});
	VideoEncoderTokens.Emplace(FString(UE::CaptureManager::VideoEncoderTokens::OutputKey), { FString(UE::CaptureManager::VideoEncoderTokens::OutputKey), LOCTEXT("VideoOutputPath", "Output File Path") });
	VideoEncoderTokens.Emplace(FString(UE::CaptureManager::VideoEncoderTokens::ParamsKey), { FString(UE::CaptureManager::VideoEncoderTokens::ParamsKey), LOCTEXT("VideoParams", "Conversion Parameters (e.g. Pixel Format, Rotation etc") });
}

UE::CaptureManager::FArchiveToken UCaptureManagerVideoEncoderTokens::GetToken(const FString& InKey) const
{
	check(VideoEncoderTokens.Contains(InKey));
	return VideoEncoderTokens[InKey];
}

void UCaptureManagerVideoEncoderTokens::OnCreateDefaultTokens(TArray<FNamingTokenData>& OutTokens)
{
	Super::OnCreateDefaultTokens(OutTokens);

	for (const TPair<FString, UE::CaptureManager::FArchiveToken>& Token : VideoEncoderTokens)
	{
		OutTokens.Add({
			Token.Value.Name,
			Token.Value.Description,
			FNamingTokenData::FTokenProcessorDelegateNative::CreateLambda([Name = Token.Value.Name] {
					return FText::FromString(Name);
				})
			});
	}

}

void UCaptureManagerVideoEncoderTokens::OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData)
{
	Super::OnPreEvaluate_Implementation(InEvaluationData);
}

void UCaptureManagerVideoEncoderTokens::OnPostEvaluate_Implementation()
{
	Super::OnPostEvaluate_Implementation();
}


UCaptureManagerAudioEncoderTokens::UCaptureManagerAudioEncoderTokens()
{
	Namespace = TEXT("cmaudenc");

	AudioEncoderTokens.Emplace(FString(UE::CaptureManager::AudioEncoderTokens::InputKey), { FString(UE::CaptureManager::AudioEncoderTokens::InputKey), LOCTEXT("AudioInputPath", "Input File Path")});
	AudioEncoderTokens.Emplace(FString(UE::CaptureManager::AudioEncoderTokens::OutputKey), { FString(UE::CaptureManager::AudioEncoderTokens::OutputKey), LOCTEXT("AudioOutputPath", "Output File Path") });
}

UE::CaptureManager::FArchiveToken UCaptureManagerAudioEncoderTokens::GetToken(const FString& InKey) const
{
	check(AudioEncoderTokens.Contains(InKey));
	return AudioEncoderTokens[InKey];
}

void UCaptureManagerAudioEncoderTokens::OnCreateDefaultTokens(TArray<FNamingTokenData>& OutTokens)
{
	Super::OnCreateDefaultTokens(OutTokens);

	for (const TPair<FString, UE::CaptureManager::FArchiveToken>& Token : AudioEncoderTokens)
	{
		OutTokens.Add({
			Token.Value.Name,
			Token.Value.Description,
			FNamingTokenData::FTokenProcessorDelegateNative::CreateLambda([Name = Token.Value.Name] {
					return FText::FromString(Name);
				})
			});
	}

}

void UCaptureManagerAudioEncoderTokens::OnPreEvaluate_Implementation(const FNamingTokensEvaluationData& InEvaluationData)
{
	Super::OnPreEvaluate_Implementation(InEvaluationData);
}

void UCaptureManagerAudioEncoderTokens::OnPostEvaluate_Implementation()
{
	Super::OnPostEvaluate_Implementation();
}

#undef LOCTEXT_NAMESPACE

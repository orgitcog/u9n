// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubSettings.h"

#include "Config/LiveLinkHubTemplateTokens.h"
#include "NamingTokensEngineSubsystem.h"

#include "Engine/Engine.h"
#include "UObject/Package.h"

void ULiveLinkHubSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSettings, FilenameTemplate))
	{
		CalculateExampleOutput();
	}
}

ULiveLinkHubSettings::ULiveLinkHubSettings()
{
}

void ULiveLinkHubSettings::PostInitProperties()
{
	Super::PostInitProperties();
}

void ULiveLinkHubSettings::CalculateExampleOutput()
{
	if (const TObjectPtr<ULiveLinkHubNamingTokens> Tokens = GetNamingTokens())
	{
		FNamingTokenFilterArgs Filter;
		Filter.AdditionalNamespacesToInclude.Add(Tokens->GetNamespace());
		
		check(GEngine);
		const FNamingTokenResultData TemplateData = GEngine->GetEngineSubsystem<UNamingTokensEngineSubsystem>()->EvaluateTokenString(
			GetDefault<ULiveLinkHubSettings>()->FilenameTemplate, Filter);
		FilenameOutput = TemplateData.EvaluatedText.ToString();
	}
	else
	{
		FilenameOutput = GetDefault<ULiveLinkHubSettings>()->FilenameTemplate;
	}
}

TObjectPtr<ULiveLinkHubNamingTokens> ULiveLinkHubSettings::GetNamingTokens() const
{
	if (NamingTokens == nullptr)
	{
		NamingTokens = NewObject<ULiveLinkHubNamingTokens>(const_cast<ULiveLinkHubSettings*>(this),
			ULiveLinkHubNamingTokens::StaticClass());
		NamingTokens->CreateDefaultTokens();
	}

	return NamingTokens;
}

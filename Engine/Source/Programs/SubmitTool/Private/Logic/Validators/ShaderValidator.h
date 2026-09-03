// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ValidatorBaseAsync.h"

class FShaderValidator final : public FValidatorBaseAsync
{
public:		
	FShaderValidator(const FName& InNameId, const FSubmitToolParameters& InParameters, TSharedRef<FSubmitToolServiceProvider> InServiceProvider, const FString& InDefinition);
	virtual ~FShaderValidator() = default;

	virtual void StartAsyncWork(const FString& InCLDescription, const TArray<FSourceControlStateRef>& InFilteredFilesInCL, const TArray<const FTag*>& InTags) override;

	virtual const FString& GetValidatorTypeName() const override
	{
		return SubmitToolParseConstants::ShaderValidator;
	}
};

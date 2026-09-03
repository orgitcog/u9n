// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ValidatorBase.h"
#include "ISourceControlChangelistState.h"

class FCrossChangelistValidator : public FValidatorBase
{
public:
	using FValidatorBase::FValidatorBase;

	virtual bool Validate(const FString& InCLDescription, const TArray<FSourceControlStateRef>& InFilteredFilesInCL, const TArray<const FTag*>& InTags) override;
	virtual const FString& GetValidatorTypeName() const override { return SubmitToolParseConstants::CrossChangelistValidator; }

private:
	bool CheckHeaderAndCppInDifferentChangelist(const TArray<FSourceControlChangelistStatePtr>& OtherChangelistsStates);
	bool CheckForFilesInUncontrolledCLFile(const TSet<FString>& InUProjects, const TSet<FString>& InUEFNProjects);
	TMap<FString, TArray<FString>> LoadUncontrolledCLs(const FString& InFile) const;

	TSet<FString> ExtractUProjectFiles(const TArray<FString>& InFiles);
	TSet<FString> ExtractUEFNProjectFiles(const TArray<FString>& InFiles);

};
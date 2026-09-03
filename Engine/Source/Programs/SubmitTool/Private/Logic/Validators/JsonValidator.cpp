// Copyright Epic Games, Inc. All Rights Reserved.

#include "JsonValidator.h"

#include "Internationalization/Regex.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Models/ModelInterface.h"

FJsonValidator::FJsonValidator(const FName& InNameId, const FSubmitToolParameters& InParameters, TSharedRef<FSubmitToolServiceProvider> InServiceProvider, const FString& InDefinition) :
	FValidatorBaseAsync(InNameId, InParameters, InServiceProvider, InDefinition)
{
	ParseDefinition(InDefinition);
}

void FJsonValidator::StartAsyncWork(const FString& CLDescription, const TArray<FSourceControlStateRef>& FilteredFilesInCL, const TArray<const FTag*>& Tags)
{
	TArray<TArray<FSourceControlStateRef>> Batches;
	Batches.Add(TArray<FSourceControlStateRef>());

	const int32 MaxTasks = FMath::Max(1, FilteredFilesInCL.Num() / 4);
	for (const FSourceControlStateRef& File : FilteredFilesInCL)
	{
		if (Batches.Last().Num() >= MaxTasks)
		{
			Batches.Add(TArray<FSourceControlStateRef>());
		}

		Batches.Last().Add(File);
	}

	for (const TArray<FSourceControlStateRef>& Batch : Batches)
	{
		this->StartAsyncTask([this, Batch](const UE::Tasks::FCancellationToken& InCancellationToken) -> bool
			{
				return ValidateJson(Batch, InCancellationToken);
			});
	}
}

bool FJsonValidator::ValidateJson(const TArray<FSourceControlStateRef>& InFilteredFilesInCL, const UE::Tasks::FCancellationToken& InCancellationToken) const
{
	bool bValid = true;

	for (const FSourceControlStateRef& File : InFilteredFilesInCL)
	{
		if (InCancellationToken.IsCanceled())
		{
			break;
		}

		const FString& Filename = File->GetFilename();

		FString JSON;
		const FString& RegexFiltering = GetTypedDefinition<FJsonValidatorDefinition>()->RegexLineExclusion;
		if (!RegexFiltering.IsEmpty())
		{
			FRegexPattern Pattern = FRegexPattern(RegexFiltering, ERegexPatternFlags::CaseInsensitive);

			TArray<FString> Lines;
			if (!FFileHelper::LoadFileToStringArrayWithPredicate(Lines, *Filename, [&Pattern](const FString& Line) { return !FRegexMatcher(Pattern, Line).FindNext(); }))
			{
				LogFailure(FString::Printf(TEXT("[%s] %s could not be loaded"), *GetValidatorName(), *Filename));
				bValid = false;
				continue;
			}

			JSON = FString::Join(Lines, TEXT("\n"));
		}
		else
		{
			if (!FFileHelper::LoadFileToString(JSON, *Filename))
			{
				LogFailure(FString::Printf(TEXT("[%s] %s could not be loaded"), *GetValidatorName(), *Filename));
				bValid = false;
				continue;
			}
		}

		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(MoveTemp(JSON));
		TSharedPtr<FJsonValue> JsonValue;
		if (!FJsonSerializer::Deserialize(JsonReader, JsonValue) || !JsonValue.IsValid())
		{
			const FString& Error = JsonReader->GetErrorMessage();
			LogFailure(FString::Printf(TEXT("[%s] %s is an invalid JSON file: %s"), *GetValidatorName(), *Filename, *Error));
			bValid = false;
		}
	}

	return bValid;
}

void FJsonValidator::ParseDefinition(const FString& InDefinition)
{
	FStringOutputDevice Errors;

	FJsonValidatorDefinition* DefinitionToLoad = new FJsonValidatorDefinition;
	FJsonValidatorDefinition::StaticStruct()->ImportText(*InDefinition, DefinitionToLoad, nullptr, 0, &Errors, FJsonValidatorDefinition::StaticStruct()->GetName());

	Definition.Reset(DefinitionToLoad);

	if (!Errors.IsEmpty())
	{
		UE_LOG(LogSubmitTool, Error, TEXT("[%s] Error loading parameter file %s"), *GetValidatorName(), *Errors);
		FModelInterface::SetErrorState();
	}
}

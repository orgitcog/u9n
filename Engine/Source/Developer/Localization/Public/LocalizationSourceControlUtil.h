// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "LocTextHelper.h"
#include "Templates/SharedPointer.h"

#define UE_API LOCALIZATION_API

class FText;

class FLocalizationSCC
{
public:
	UE_API FLocalizationSCC();
	UE_API ~FLocalizationSCC();

	UE_API bool CheckOutFile(const FString& InFile, FText& OutError);
	UE_API bool CheckinFiles(const FText& InChangeDescription, FText& OutError);
	UE_API bool CleanUp(FText& OutError);
	UE_API bool RevertFile(const FString& InFile, FText& OutError);
	UE_API bool IsReady(FText& OutError);

private:
	TArray<FString> CheckedOutFiles;
};

class FLocFileSCCNotifies : public ILocFileNotifies
{
public:
	FLocFileSCCNotifies(const TSharedPtr<FLocalizationSCC>& InSourceControlInfo)
		: SourceControlInfo(InSourceControlInfo)
	{
	}

	/** Virtual destructor */
	virtual ~FLocFileSCCNotifies() {}

	//~ ILocFileNotifies interface
	virtual void PreFileRead(const FString& InFilename) override {}
	virtual void PostFileRead(const FString& InFilename) override {}
	UE_API virtual void PreFileWrite(const FString& InFilename) override;
	UE_API virtual void PostFileWrite(const FString& InFilename) override;

private:
	TSharedPtr<FLocalizationSCC> SourceControlInfo;
};

#undef UE_API

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GatherTextCommandlet.generated.h"

namespace EOutputJson
{
	enum Format { Manifest, Archive };
}

/**
 *	UGatherTextCommandlet: One commandlet to rule them all. This commandlet loads a config file and then calls other localization commandlets. Allows localization system to be easily extendable and flexible. 
 */
UCLASS(MinimalAPI)
class UGatherTextCommandlet final : public UGatherTextCommandletBase
{
    GENERATED_UCLASS_BODY()
public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	/** Internal implementation of Main that can be used to provide additional options when running embedded within another process */
	UNREALED_API int32 Execute(const FString& Params, const TSharedPtr<const FGatherTextCommandletEmbeddedContext>& InEmbeddedContext);

	int32 ProcessGatherConfig(const FString& GatherTextConfigPath, const TSharedPtr<FLocalizationSCC>& CommandletSourceControlInfo, const TArray<FString>& Tokens, const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals);

	static const FString UsageText;
//~ Begin UGatherTextCommandletBase  Interface
	virtual bool ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const override
	{
		// This commandlet is the driver for other commandlet. This should always run even in preview
		return true;
	}
	//~ End UGatherTextCommandletBase  Interface

private:
	// Helper function to generate a changelist description
	FText GetChangelistDescription(const TArray<FString>& GatherTextConfigPaths) const;
};

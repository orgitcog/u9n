// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "InterchangeDnaModule.generated.h"

class IDNAReader;

UENUM()
enum class EMetaHumanImportDNAType : uint8
{
	Face,
	Body,
	Combined
};

class INTERCHANGEDNA_API FInterchangeDnaModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FInterchangeDnaModule& GetModule();

	/** DNAInterchangeModule implementation */

	class USkeletalMesh* ImportSync(const FString& InNewRigAssetName, const FString& InNewRigPath, TSharedPtr<IDNAReader> InDNAReader, const EMetaHumanImportDNAType InImportDNAType = EMetaHumanImportDNAType::Face);
};

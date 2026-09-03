// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SourceControlSettings.generated.h"

/** Serializes source control window settings. */
UCLASS(config=Editor)
class USourceControlSettings : public UObject
{
	GENERATED_BODY()

public:
	USourceControlSettings() {}

	UPROPERTY(config, EditAnywhere, Category="Revision Control Changelist View", meta = (Keywords = "Source Control"))
	bool bShowAssetTypeColumn = true;

	UPROPERTY(config, EditAnywhere, Category="Revision Control Changelist View", meta = (Keywords = "Source Control"))
	bool bShowAssetLastModifiedTimeColumn = true;

	UPROPERTY(config, EditAnywhere, Category="Revision Control Changelist View", meta = (Keywords = "Source Control"))
	bool bShowAssetCheckedOutByColumn = true;
};

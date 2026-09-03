// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetRegistryTagProviderInterface.h"
#include "UObject/ScriptMacros.h"
#include "ToolMenuEntryScript.h"
#include "ToolMenuSection.h"
#include "EditorUtilityToolMenu.generated.h"


UCLASS(Blueprintable, abstract)
class BLUTILITY_API UEditorUtilityToolMenuEntry : public UToolMenuEntryScript, public IAssetRegistryTagProviderInterface
{
	GENERATED_BODY()

	//~ Begin IAssetRegistryTagProviderInterface interface
	virtual bool ShouldAddCDOTagsToBlueprintClass() const override
	{
		return true;
	}
	//~ End IAssetRegistryTagProviderInterface interface

protected:
	/** Run this editor utility on start-up (after asset discovery)? */
	UPROPERTY(Category=Settings, EditDefaultsOnly, AssetRegistrySearchable, DisplayName="Run on Start-up")
	bool bRunEditorUtilityOnStartup = false;
};

UCLASS(Blueprintable, abstract)
class BLUTILITY_API UEditorUtilityToolMenuSection : public UToolMenuSectionDynamic, public IAssetRegistryTagProviderInterface
{
	GENERATED_BODY()

	//~ Begin IAssetRegistryTagProviderInterface interface
	virtual bool ShouldAddCDOTagsToBlueprintClass() const override
	{
		return true;
	}
	//~ End IAssetRegistryTagProviderInterface interface

protected:
	/** Run this editor utility on start-up (after asset discovery)? */
	UPROPERTY(Category=Settings, EditDefaultsOnly, AssetRegistrySearchable, DisplayName="Run on Start-up")
	bool bRunEditorUtilityOnStartup = false;
};


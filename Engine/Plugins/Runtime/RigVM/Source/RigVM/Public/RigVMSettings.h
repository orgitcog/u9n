// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RigVMSettings.h: Declares the RigVMEditorSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RigVMCore/RigVMVariant.h"

#include "RigVMSettings.generated.h"

class UStaticMesh;

/**
 * Customize RigVM Rig Editor.
 */
UCLASS(config = EditorPerProjectUserSettings, meta=(DisplayName="RigVM Editor"), MinimalAPI)
class URigVMEditorSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const override { return TEXT("ContentEditors"); }
	
#if WITH_EDITORONLY_DATA

	// When this is checked mutable nodes (nodes with an execute pin)
	// will be hooked up automatically.
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bAutoLinkMutableNodes;

	/** When the Blueprint graph context menu is invoked (e.g. by right-clicking in the graph or dragging off a pin), do not block the UI while populating the available actions list. */
	UPROPERTY(EditAnywhere, config, Category = Workflow, meta = (DisplayName = "Enable Non-Blocking Context Menu"))
	bool bEnableContextMenuTimeSlicing;
#endif
};


UCLASS(config = Editor, meta=(DisplayName="RigVM Project Settings"), MinimalAPI)
class URigVMProjectSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, config, Category = Variants)
	TArray<FRigVMTag> VariantTags;

	UFUNCTION(BlueprintPure, Category= Variants)
	RIGVM_API FRigVMTag GetTag(FName InTagName) const;
	RIGVM_API const FRigVMTag* FindTag(FName InTagName) const;
};
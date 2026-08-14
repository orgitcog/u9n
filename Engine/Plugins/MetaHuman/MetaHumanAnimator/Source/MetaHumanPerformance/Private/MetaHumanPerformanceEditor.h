// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"

#include "MetaHumanPerformanceEditor.generated.h"

class FBaseAssetToolkit;

/**
 * Configurator asset editor
 */

UCLASS()
class UMetaHumanPerformanceEditor : public UAssetEditor
{
	GENERATED_BODY()

public:
	void GetObjectsToEdit(TArray<UObject*>& InObjectsToEdit) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;

	void SetObjectToEdit(UObject* InObjects);

protected:
	UPROPERTY()
	TObjectPtr<UObject> ObjectToEdit;
};

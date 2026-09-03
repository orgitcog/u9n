// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"
#include "DataLinkGraphAssetEditor.generated.h"

class FDataLinkGraphAssetToolkit;
class UDataLinkEdGraph;
class UDataLinkGraph;

UCLASS(Transient)
class UDataLinkGraphAssetEditor : public UAssetEditor
{
	GENERATED_BODY()

public:
	void Initialize(UDataLinkGraph* InDataLinkGraph);

	TSharedPtr<FDataLinkGraphAssetToolkit> GetToolkit() const;

	TSharedPtr<FUICommandList> GetToolkitCommands() const;

	UDataLinkGraph* GetDataLinkGraph() const;

	UDataLinkEdGraph* GetDataLinkEdGraph() const;

protected:
	//~ Begin UAssetEditor
	virtual void GetObjectsToEdit(TArray<UObject*>& InObjectsToEdit) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
	//~ End UAssetEditor

private:
	UPROPERTY()
	TObjectPtr<UDataLinkGraph> DataLinkGraph;
};

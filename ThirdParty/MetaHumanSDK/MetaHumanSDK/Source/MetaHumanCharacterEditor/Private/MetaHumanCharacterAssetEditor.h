// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"

#include "MetaHumanCharacterAssetEditor.generated.h"

UCLASS()
class UMetaHumanCharacterAssetEditor : public UAssetEditor
{
	GENERATED_BODY()

public:

	//~Begin UAssetEditor Interface
	virtual void GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
	//~End UAssetEditor Interface

	class UMetaHumanCharacter* GetObjectToEdit() const;
	void SetObjectToEdit(class UMetaHumanCharacter* InObject);

private:

	UPROPERTY()
	TObjectPtr<class UMetaHumanCharacter> ObjectToEdit;

};
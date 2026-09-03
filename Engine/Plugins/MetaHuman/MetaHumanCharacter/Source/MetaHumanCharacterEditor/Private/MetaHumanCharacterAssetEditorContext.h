// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetaHumanCharacterAssetEditorContext.generated.h"

class FMetaHumanCharacterEditorToolkit;

UCLASS()
class UMetaHumanCharacterAssetEditorContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<FMetaHumanCharacterEditorToolkit> MetaHumanCharacterAssetEditor;
};
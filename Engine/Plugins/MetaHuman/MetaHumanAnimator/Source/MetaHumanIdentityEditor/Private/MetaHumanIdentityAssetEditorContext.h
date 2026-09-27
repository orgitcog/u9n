// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetaHumanIdentityAssetEditorContext.generated.h"

class FMetaHumanIdentityAssetEditorToolkit;

UCLASS()
class UMetaHumanIdentityAssetEditorContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<FMetaHumanIdentityAssetEditorToolkit> MetaHumanIdentityAssetEditor;
};
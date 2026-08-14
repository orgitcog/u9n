// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetaHumanPerformanceEditorContext.generated.h"

class FMetaHumanPerformanceEditorToolkit;

UCLASS()
class UMetaHumanPerformanceEditorContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<FMetaHumanPerformanceEditorToolkit> MetaHumanPerformanceEditorToolkit;
};
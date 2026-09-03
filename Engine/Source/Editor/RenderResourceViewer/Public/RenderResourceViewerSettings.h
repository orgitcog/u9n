// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RenderResourceViewerSettings.generated.h"

USTRUCT()
struct FRenderResourceViewerTreemapFilter
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = "Section")
	FString FilterString;

	UPROPERTY(config, EditAnywhere, Category = "Section")
	FString DisplayName;
};

UCLASS(config = Editor, DefaultConfig, meta = (DisplayName = "Render Resource Viewer"))
class RENDERRESOURCEVIEWER_API URenderResourceViewerSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Treemap")
	TArray<FRenderResourceViewerTreemapFilter> Filters;
};

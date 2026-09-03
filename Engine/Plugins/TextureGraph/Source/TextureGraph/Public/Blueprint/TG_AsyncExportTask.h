// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TG_SystemTypes.h"
#include "TextureGraph.h"
#include "TG_HelperFunctions.h"
#include "Export/TextureExporter.h"
#include "TG_AsyncTask.h"
#include "TG_AsyncRenderTask.h"
#include "TG_AsyncExportTask.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTSExportTaskDelegate);

UCLASS()
class TEXTUREGRAPH_API UTG_AsyncExportTask : public UTG_AsyncRenderTaskBase
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TextureGraph" , meta=(DisplayName="Texture Graph Export (Async)", BlueprintInternalUseOnly = "true"))
	static UTG_AsyncExportTask* TG_AsyncExportTask(UTextureGraphBase* TextureGraph, const bool OverwriteTextures, const bool bSave, const bool bExportAll, const bool bDisableCache = false);

	virtual void Activate() override;
	virtual const TArray<UTextureRenderTarget2D*>& ActivateBlocking(JobBatchPtr Batch);

	virtual void FinishDestroy() override;

	UPROPERTY(BlueprintAssignable, Category = "TextureGraph")
	FTSExportTaskDelegate OnDone;

private:

	bool bBlobberCachingState;
	UFUNCTION()
	void OnExportDone();
	bool bOverwriteTextures = true;
	bool bSave = false;
	bool bExportAll = false;
	FExportSettings TargetExportSettings;
};


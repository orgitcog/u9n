// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TG_SystemTypes.h"
#include "TextureGraph.h"
#include "TG_HelperFunctions.h"
#include "Data/Blob.h"
#include "Data/TiledBlob.h"
#include "TG_AsyncTask.h"
#include "Engine/World.h"
#include "TG_AsyncRenderTask.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSRenderTaskDelegate, const TArray<UTextureRenderTarget2D*>&, OutputRts);

UCLASS()
class TEXTUREGRAPH_API UTG_AsyncRenderTaskBase : public UTG_AsyncTask
{
	GENERATED_UCLASS_BODY()

public:
	virtual const TArray<UTextureRenderTarget2D*>& ActivateBlocking(JobBatchPtr Batch);
	virtual void FinishDestroy() override;
	virtual void SetReadyToDestroy() override;

protected:
	AsyncBool FinalizeAllOutputBlobs();
	JobBatchPtr PrepareActivate(JobBatchPtr Batch, bool bIsAsync);
	void GatherAllOutputBlobs();
	void GatherAllRenderTargets();
	AsyncBool GetRenderTextures();

	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool CleanupResources);

	TArray<UTextureRenderTarget2D*> OutputRts;
	TArray<BlobPtr> OutputBlobs;
	UTextureGraphBase* OriginalTextureGraphPtr;
	UTextureGraphBase* TextureGraphPtr;
	bool bShouldDestroyOnRenderComplete = false;
	bool bRenderComplete = false;
};

/////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_AsyncRenderTask : public UTG_AsyncRenderTaskBase
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TextureGraph" , meta=(DisplayName="Texture Graph Render (Async)" , BlueprintInternalUseOnly = "true"))
	static UTG_AsyncRenderTask* TG_AsyncRenderTask(UTextureGraphBase* InTextureGraph);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable, Category = "TextureGraph")
	FTSRenderTaskDelegate OnDone;
};


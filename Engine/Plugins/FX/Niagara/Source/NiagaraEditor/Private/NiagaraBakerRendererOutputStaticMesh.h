// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraBakerRenderer.h"

#include "Factories/Factory.h"

#include "NiagaraBakerRendererOutputStaticMesh.generated.h"

class UTextureRenderTarget2D;
class UStaticMesh;

struct FNiagaraRendererReadbackResult;

UCLASS()
class UNiagaraBakerStaticMeshFactoryNew : public UFactory
{
	GENERATED_BODY()

	UNiagaraBakerStaticMeshFactoryNew();
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

class FNiagaraBakerRendererOutputStaticMesh : public FNiagaraBakerOutputRenderer
{
public:
	virtual TArray<FNiagaraBakerOutputBinding> GetRendererBindings(UNiagaraBakerOutput* InBakerOutput) const override;

	virtual FIntPoint GetPreviewSize(UNiagaraBakerOutput* InBakerOutput, FIntPoint InAvailableSize) const override;
	virtual void RenderPreview(UNiagaraBakerOutput* InBakerOutput, const FNiagaraBakerRenderer& BakerRenderer, UTextureRenderTarget2D* RenderTarget, TOptional<FString>& OutErrorString) const override;

	virtual FIntPoint GetGeneratedSize(UNiagaraBakerOutput* InBakerOutput, FIntPoint InAvailableSize) const override;
	virtual void RenderGenerated(UNiagaraBakerOutput* InBakerOutput, const FNiagaraBakerRenderer& BakerRenderer, UTextureRenderTarget2D* RenderTarget, TOptional<FString>& OutErrorString) const override;

	virtual bool BeginBake(FNiagaraBakerFeedbackContext& FeedbackContext, UNiagaraBakerOutput* InBakerOutput) override;
	virtual void BakeFrame(FNiagaraBakerFeedbackContext& FeedbackContext, UNiagaraBakerOutput* InBakerOutput, int FrameIndex, const FNiagaraBakerRenderer& BakerRenderer) override;
	virtual void EndBake(FNiagaraBakerFeedbackContext& FeedbackContext, UNiagaraBakerOutput* InBakerOutput) override;

	static bool ConvertReadbackResultsToStaticMesh(const FNiagaraRendererReadbackResult& ReadbackResult, UStaticMesh* StaticMesh);

private:
	UTextureRenderTarget2D* BakeRenderTarget = nullptr;
};

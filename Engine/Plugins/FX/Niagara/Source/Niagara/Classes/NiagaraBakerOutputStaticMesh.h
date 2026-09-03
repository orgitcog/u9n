// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraBakerOutput.h"
#include "NiagaraRendererReadback.h"
#include "NiagaraBakerOutputStaticMesh.generated.h"

UCLASS(meta = (DisplayName = "Static Mesh Output"), MinimalAPI)
class UNiagaraBakerOutputStaticMesh : public UNiagaraBakerOutput
{
	GENERATED_BODY()

public:
	UNiagaraBakerOutputStaticMesh(const FObjectInitializer& Init)
	{
	}

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString FramesAssetPathFormat = TEXT("{AssetFolder}/{AssetName}_BakedFrame_{OutputName}_{FrameIndex}");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FNiagaraRendererReadbackParameters ExportParameters;

	NIAGARA_API virtual bool Equals(const UNiagaraBakerOutput& Other) const override;

#if WITH_EDITOR
	NIAGARA_API FString MakeOutputName() const override;
#endif

#if WITH_EDITORONLY_DATA
	NIAGARA_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

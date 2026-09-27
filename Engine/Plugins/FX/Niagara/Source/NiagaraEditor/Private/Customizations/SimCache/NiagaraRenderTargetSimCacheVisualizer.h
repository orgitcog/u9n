// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "Customizations/NiagaraDataInterfaceSimCacheVisualizer.h"

class FNiagaraRenderTargetSimCacheVisualizer : public INiagaraDataInterfaceSimCacheVisualizer
{
public:
	FNiagaraRenderTargetSimCacheVisualizer() {}
	virtual ~FNiagaraRenderTargetSimCacheVisualizer() override = default;
	
	virtual TSharedPtr<SWidget> CreateWidgetFor(const UObject* CachedData, TSharedPtr<FNiagaraSimCacheViewModel> ViewModel) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackRenderItemGroup.h"
#include "Widgets/SCompoundWidget.h"


struct FNiagaraRendererStackEntryObserver : public TSharedFromThis<FNiagaraRendererStackEntryObserver>
{
public:
	FNiagaraRendererStackEntryObserver();
	~FNiagaraRendererStackEntryObserver();

	void Initialize(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);
	
	DECLARE_MULTICAST_DELEGATE(FOnRendererGroupStructureChanged);

	FOnRendererGroupStructureChanged OnRenderersChanged;
	
private:	
	void OnRenderGroupStructureChanged(ENiagaraStructureChangedFlags NiagaraStructureChangedFlags) const;
	void OnMaterialCompiled(class UMaterialInterface* MaterialInterface) const;
	void OnStackDataObjectChanged(TArray<UObject*> ChangedObjects, ENiagaraDataObjectChange NiagaraDataObjectChange) const;

private:
	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModelWeak;
};

/**
 * 
 */
class NIAGARAEDITORWIDGETS_API SNiagaraEmitterRendererThumbnails : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnRendererThumbnailClicked, UNiagaraStackRendererItem* StackRendererItem);
	
	SLATE_BEGIN_ARGS(SNiagaraEmitterRendererThumbnails)
		{}
		SLATE_EVENT(FOnRendererThumbnailClicked, OnRendererThumbnailClicked)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);
private:
	void RefreshThumbnails();

	static void SetupRendererThumbnailTooltip(TSharedPtr<SWidget> InWidget, TSharedPtr<SWidget> InTooltipWidget);
	FReply OnRendererThumbnailClicked(UNiagaraStackRendererItem* StackRendererItem) const;

private:
	TSharedPtr<SBox> Content;
	TSharedPtr<FNiagaraRendererStackEntryObserver> RendererObserver;
	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModelWeak;
	FOnRendererThumbnailClicked OnRendererThumbnailClickedDelegate;
};

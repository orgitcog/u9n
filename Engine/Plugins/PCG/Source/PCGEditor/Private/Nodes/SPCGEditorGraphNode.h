// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNode.h"

#include "PCGCommon.h"

struct FOverlayBrushInfo;

class UPCGEditorGraphNodeBase;

class SPCGEditorGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNode){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode);
	void CreateAddPinButtonWidget();

	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;
	virtual const FSlateBrush* GetNodeBodyBrush() const override;
	virtual void RequestRenameOnSpawn() override { /* Empty to avoid the default behavior to rename on node spawn */ }
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> InNodeTitle) override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	virtual EVisibility IsAddPinButtonVisible() const override;
	virtual FReply OnAddPin() override;
	//~ End SGraphNode Interface

	//~ Begin SNodePanel::SNode Interface
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2f& WidgetSize) const override;
	virtual void GetOverlayBrushes(bool bSelected, const FVector2f& WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	//~ End SNodePanel::SNode Interface

protected:
	void OnNodeChanged();

	/** Will add the Hierarchical Generation overlay to the node. */
	virtual bool UsesHiGenOverlay() const;
	/** Will add the GPU icon overlay to the node. */
	virtual bool UsesGPUOverlay() const;

private:
	static FLinearColor GetGridLabelColor(EPCGHiGenGrid NodeGrid);

	/** Adds the Hierarchical Generation overlay to the array, displaying the HiGen grid size on the node. */
	void AddHiGenOverlayWidget(TArray<FOverlayWidgetInfo>& OverlayWidgets) const;

	/** Adds the "GPU" tag to the node to indicate the node will execute on the GPU. */
	void AddGPUOverlayWidget(TArray<FOverlayWidgetInfo>& OverlayWidgets) const;

	/** Indicates an upload of data to the GPU occurred as a result of this node. */
	void AddGPUUploadWidget(TArray<FOverlayWidgetInfo>& OverlayWidgets) const;

	/** Indicates a readback of data from the GPU occurred as a result of this node. */
	void AddGPUReadbackWidget(TArray<FOverlayWidgetInfo>& OverlayWidgets) const;

	/**
	* Get the border brush for the given combination of grid sizes and enabled state. All a big workaround for FSlateRoundedBoxBrush not respecting
	* the tint colour.
	*/
	const FSlateBrush* GetBorderBrush(EPCGHiGenGrid InspectedGrid, EPCGHiGenGrid NodeGrid) const;

	UPCGEditorGraphNodeBase* PCGEditorGraphNode = nullptr;
};

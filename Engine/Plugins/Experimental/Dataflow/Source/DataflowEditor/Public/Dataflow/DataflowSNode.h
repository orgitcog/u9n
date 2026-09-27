// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"
#include "SGraphNode.h"
#include "UObject/GCObject.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowSEditorInterface.h"

class UDataflowEdNode;
class SCheckBox;
class SImage;
class STextBlock;

//
// SDataflowEdNode
//

class DATAFLOWEDITOR_API SDataflowEdNode : public SGraphNode , public FGCObject
{
	typedef SGraphNode Super;

public:
	typedef TFunction<void(UEdGraphNode* InNode, bool InEnabled)> FToggleRenderCallback;

	SLATE_BEGIN_ARGS(SDataflowEdNode)
		: _GraphNodeObj(nullptr)
	{}
	SLATE_ARGUMENT(UDataflowEdNode*, GraphNodeObj)
	SLATE_ARGUMENT(FDataflowSEditorInterface*, DataflowInterface)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, UDataflowEdNode* InNode);

	// SGraphNode interface
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2f& WidgetSize) const override;
	virtual void UpdateErrorInfo() override;
	virtual void RequestRenameOnSpawn() override { /* No auto rename on spawn, because it can interfere with Copy/Paste and cause a crash */ }

	static void CopyDataflowNodeSettings(TSharedPtr<FDataflowNode> SourceDataflowNode, TSharedPtr<FDataflowNode> TargetDataflowNode);

	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SDataflowEdNode");
	}
	//~ End FGCObject interface


private:
	// SGraphNode interface
	/** Override this to create a button to add pins on the input side of the node */
	virtual void CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox) override;
	/** Override this to create a button to add pins on the output side of the node */
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
	/** Callback function executed when Add pin button is clicked */
	virtual FReply OnAddPin() override;
	/** Checks whether Add pin button should currently be visible */
	virtual EVisibility IsAddPinButtonVisible() const;

	/** when clicking on the show/hide input pins button */
	FReply OnShowHideInputs();

	const FSlateBrush* GetPinButtonImage() const;

	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	virtual TSharedRef<SWidget> CreateTitleRightWidget() override;

	TObjectPtr<UDataflowEdNode> DataflowGraphNode = nullptr;	

	FCheckBoxStyle CheckBoxStyle;
	TSharedPtr<SCheckBox> RenderCheckBoxWidget;
	TSharedPtr<SImage> FreezeImageWidget;
	TSharedPtr<STextBlock> PerfWidget;
	TSharedPtr<STextBlock> WatchWidget;

	//FCheckBoxStyle CacheStatusStyle;
	//TSharedPtr<SCheckBox> CacheStatus;

	FDataflowSEditorInterface* DataflowInterface;

};


// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SPCGEditorGraphNode.h"

class UPCGEditorGraphNode;

class SPCGEditorGraphNodeCompact : public SPCGEditorGraphNode
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNodeCompact) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode);

	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> InNodeTitle) override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	//~ End SGraphNode Interface

private:
	FSlateColor GetSubduedSpillColor() const;

	UPCGEditorGraphNode* PCGEditorGraphNode = nullptr;
	TSharedPtr<SNodeTitle> NodeTitle = nullptr;
};

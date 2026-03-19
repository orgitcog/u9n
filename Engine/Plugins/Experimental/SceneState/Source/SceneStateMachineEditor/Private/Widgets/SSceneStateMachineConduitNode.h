// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SSceneStateMachineNode.h"

class USceneStateMachineConduitNode;

namespace UE::SceneState::Editor
{

class SStateMachineConduitNode : public SStateMachineNode
{
public:
	using Super = SStateMachineNode;

	SLATE_BEGIN_ARGS(SStateMachineConduitNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USceneStateMachineConduitNode* InNode);

private:
	TSharedRef<SWidget> MakeNodeInnerWidget();

	//~ Begin SGraphNode
	virtual void UpdateGraphNode() override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	//~ End SGraphNode
};

} // UE::SceneState::Editor

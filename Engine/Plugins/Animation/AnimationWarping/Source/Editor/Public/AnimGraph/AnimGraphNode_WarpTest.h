// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "BoneControllers/AnimNode_WarpTest.h"
#include "AnimGraphNode_WarpTest.generated.h"

namespace ENodeTitleType { enum Type : int; }

UCLASS(Experimental)
class ANIMATIONWARPINGEDITOR_API UAnimGraphNode_WarpTest : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_WarpTest Node;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	// End of UEdGraphNode interface
};

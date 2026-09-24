// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraph/AnimGraphNode_WarpTest.h"

#define LOCTEXT_NAMESPACE "AnimationWarping"

UAnimGraphNode_WarpTest::UAnimGraphNode_WarpTest(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FText UAnimGraphNode_WarpTest::GetTooltipText() const
{
	return LOCTEXT("WarpTestTooltip", "Warp Test Node");
}

FText UAnimGraphNode_WarpTest::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("WarpTest", "Warp Test Node");
}

FLinearColor UAnimGraphNode_WarpTest::GetNodeTitleColor() const
{
	return FLinearColor(FColor(153.f, 0.f, 0.f));
}

#undef LOCTEXT_NAMESPACE

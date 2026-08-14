// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HitProxies.h"
#include "UObject/NameTypes.h"

struct HIKRigEditorBoneProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FName BoneName;

	HIKRigEditorBoneProxy(const FName& InBoneName);

	virtual EMouseCursor::Type GetMouseCursor() override;
	virtual bool AlwaysAllowsTranslucentPrimitives() const override;
};

struct HIKRigEditorGoalProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FName GoalName;
	
	HIKRigEditorGoalProxy(const FName& InGoalName);

	virtual EMouseCursor::Type GetMouseCursor() override;
	virtual bool AlwaysAllowsTranslucentPrimitives() const override;
};
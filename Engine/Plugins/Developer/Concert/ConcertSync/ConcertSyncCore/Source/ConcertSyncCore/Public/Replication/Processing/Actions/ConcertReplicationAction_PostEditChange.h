// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertReplicationAction.h"
#include "UObject/Object.h"
#include "ConcertReplicationAction_PostEditChange.generated.h"

/** Calls PostEditChange on the object. */
USTRUCT(DisplayName = "Post Edit Change")
struct CONCERTSYNCCORE_API FConcertReplicationAction_PostEditChange : public FConcertReplicationAction
{
	GENERATED_BODY()

	virtual void Apply(const UE::ConcertSyncCore::FReplicationActionArgs& InArgs) const override
	{
#if WITH_EDITOR
		InArgs.Object->PostEditChange();
#endif
	}
};
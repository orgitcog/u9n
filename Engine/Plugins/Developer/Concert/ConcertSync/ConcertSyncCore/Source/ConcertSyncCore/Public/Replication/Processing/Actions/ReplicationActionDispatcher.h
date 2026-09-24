// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/BitArray.h"

struct FConcertReplicationActionEntry;

namespace UE::ConcertSyncCore
{
struct FReplicationActionArgs;

/** Util that you can use when calling IObjectReplicationFormat::ApplyReplicationEvent to execute actions. */
class CONCERTSYNCCORE_API FReplicationActionDispatcher
{
public:

	explicit FReplicationActionDispatcher(const TArray<FConcertReplicationActionEntry>& InActions UE_LIFETIMEBOUND, bool bDebugActions = false);

	/** Checks whether InProperty should trigger actions. */
	void OnReplicateProperty(const FProperty& InProperty);

	/** Call all properties have been processed using OnReplicateProperty. Triggers the actions that need to triggered.  */
	void ExecuteActions(const FReplicationActionArgs& InArgs);

private:

	/** The actions to perform. */
	const TArray<FConcertReplicationActionEntry>& Actions;

	/** Whether the actions should be debugged. */
	const bool bDebugActions;

	/** Bit mask of equal length as the action array. Each index marks whether the action at the equivalent action array index should be performed. */
	TBitArray<> ActionsToTrigger;
};
}

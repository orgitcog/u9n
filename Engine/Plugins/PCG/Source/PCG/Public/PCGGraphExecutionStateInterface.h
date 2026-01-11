// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Utils/PCGExtraCapture.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "PCGGraphExecutionStateInterface.generated.h"

class UPCGData;
class FPCGGraphExecutionInspection;
class IPCGGraphExecutionSource;
class UPCGGraph;
class UPCGGraphInstance;

namespace PCGUtils
{
	class FExtraCapture;
}

/**
* Interface returned by a IPCGGraphExecutionSource that is queried / updated during execution of a PCG Graph.
*/
class IPCGGraphExecutionState
{
public:
	virtual ~IPCGGraphExecutionState() = default;

	/** Returns a UPCGData representation of the ExecutionState. */
	virtual UPCGData* GetSelfData() const = 0;

	/** Returns a Seed for graph execution. */
	virtual int32 GetSeed() const = 0;

	/** Returns a Debug name that can be used for logging. */
	virtual FString GetDebugName() const = 0;

	/** Returns a World, can be null */
	virtual UWorld* GetWorld() const = 0;

	/** Returns true if the ExecutionState has network authority */
	virtual bool HasAuthority() const = 0;

	/** Returns a Transform if the ExecutionState is a spatial one. */
	virtual FTransform GetTransform() const = 0;

	/** Returns the ExecutionState bounds if the ExecutionState is a spatial one. */
	virtual FBox GetBounds() const = 0;

	/** Returns the UPCGGraph this ExecutionState is executing. */
	virtual UPCGGraph* GetGraph() const = 0;

	/** Returns the UPCGGrahInstance this ExecutionState is executing. */
	virtual UPCGGraphInstance* GetGraphInstance() const = 0;

	/** Cancel execution of this ExecutionState. */
	virtual void Cancel() = 0;

	/** Notify ExecutionState that its execution is being aborted */
	virtual void OnGraphExecutionAborted(bool bQuiet = false, bool bCleanupUnusedResources = true) = 0;

#if WITH_EDITOR

	/** Returns the FExtraCapture object for this ExecutionState */
	virtual const PCGUtils::FExtraCapture& GetExtraCapture() const = 0;

	virtual PCGUtils::FExtraCapture& GetExtraCapture() = 0;

	/** Returns the FPCGGraphExecutionInspection object for this ExecutionState */
	virtual const FPCGGraphExecutionInspection& GetInspection() const = 0;

	virtual FPCGGraphExecutionInspection& GetInspection() = 0;

	/** Register tracking dependencies, so ExecutionState can be updated when they change */
	virtual void RegisterDynamicTracking(const UPCGSettings* InSettings, const TArrayView<TPair<FPCGSelectionKey, bool>>& InDynamicKeysAndCulling) = 0;

	/** Register multiple tracking dependencies, so ExecutionState can be updated when they change */
	virtual void RegisterDynamicTracking(const FPCGSelectionKeyToSettingsMap& InKeysToSettings) = 0;

#endif
};

UINTERFACE(BlueprintType, MinimalAPI)
class UPCGGraphExecutionSource : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
* Interface used by the FPCGGraphExecutor to get an IPCGGraphExecutionState used to query/update execution.
*/
class IPCGGraphExecutionSource
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual IPCGGraphExecutionState& GetExecutionState() = 0;
	virtual const IPCGGraphExecutionState& GetExecutionState() const = 0;
};

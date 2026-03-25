// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Async/Future.h"
#include "Templates/UniquePtr.h"

class UMetaHumanAssetReport;
namespace UE::MetaHuman
{
struct FMetaHumanImportDescription;
class FMetaHumanAssetUpdateHandlerImpl;

/**
 * Class that handles the actual import and update of assets asynchronously on the correct thread and in the correct tick phase.
 */
class METAHUMANSDKEDITOR_API FMetaHumanAssetUpdateHandler
{
public:
	/**
	 * Enqueues an import operation to occur on the main game thread during the correct tick phase
	 * @param ImportDescription The parameters for the import operation
	 * @param Report The Asset Report to use for this operation. Lifetime must be handled by the caller
	 */
	static TFuture<bool> Enqueue(const FMetaHumanImportDescription& ImportDescription);

	/**
	 * Shuts down the queue cancelling any in-flight requests
	 */
	static void Shutdown();

private:
	FMetaHumanAssetUpdateHandler() = default;
	static TUniquePtr<FMetaHumanAssetUpdateHandlerImpl> Instance;
};
}

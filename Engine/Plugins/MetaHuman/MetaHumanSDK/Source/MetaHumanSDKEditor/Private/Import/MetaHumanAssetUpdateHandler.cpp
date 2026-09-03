// Copyright Epic Games, Inc. All Rights Reserved.
#include "Import/MetaHumanAssetUpdateHandler.h"

#include "MetaHumanAssetReport.h"
#include "MetaHumanSDKEditor.h"
#include "Import/MetaHumanImport.h"

#include "Containers/Queue.h"
#include "Logging/StructuredLog.h"
#include "TickableEditorObject.h"

namespace UE::MetaHuman
{
/**
 * Class that  implements FTickableEditorObject. This means that the Tick function gets called during the correct phase
 * for creating and destroying assets etc. without causing crashes later on.
 */
class FMetaHumanAssetUpdateHandlerImpl final : public FTickableEditorObject
{
public:
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMetaHumanAssetInjectionHandlerImpl, STATGROUP_Tickables);
	}

	virtual ETickableTickType GetTickableTickType() const override
	{
		return ETickableTickType::Conditional;
	}

	virtual bool IsTickable() const override
	{
		return !(IsGarbageCollecting() || GIsSavingPackage || ImportQueue.IsEmpty());
	}

	virtual void Tick(float DeltaTime) override
	{
		TPair<FMetaHumanImportDescription, TSharedPtr<TPromise<bool>>> Item;
		ImportQueue.Dequeue(Item);
		Item.Value->SetValue(FMetaHumanImport::Get()->ImportMetaHuman(Item.Key).IsSet());
	}

	virtual ~FMetaHumanAssetUpdateHandlerImpl() override
	{
		while (!ImportQueue.IsEmpty())
		{
			// Complete any pending operations with a null value
			TPair<FMetaHumanImportDescription, TSharedPtr<TPromise<bool>>> Item;
			ImportQueue.Dequeue(Item);
			Item.Value->SetValue(false);
		}
	}

	// Queue of operations to process
	TQueue<TPair<FMetaHumanImportDescription, TSharedPtr<TPromise<bool>>>> ImportQueue;
};

TUniquePtr<FMetaHumanAssetUpdateHandlerImpl> FMetaHumanAssetUpdateHandler::Instance;

TFuture<bool> FMetaHumanAssetUpdateHandler::Enqueue(const FMetaHumanImportDescription& ImportDescription)
{
	if (!Instance.IsValid())
	{
		UE_LOGFMT(LogMetaHumanSDK, Verbose, "FMetaHumanAssetUpdateHandler: Initialising Instance");
		Instance = MakeUnique<FMetaHumanAssetUpdateHandlerImpl>();
	}

	UE_LOGFMT(LogMetaHumanSDK, Verbose, "FMetaHumanAssetUpdateHandler: Enqueuing import of {0}", ImportDescription.CharacterName);
	TSharedPtr<TPromise<bool>> Promise = MakeShared<TPromise<bool>>();
	TFuture<bool> Future = Promise->GetFuture();
	Instance->ImportQueue.Enqueue({ImportDescription, Promise});
	return Future;
}

void FMetaHumanAssetUpdateHandler::Shutdown()
{
	UE_LOGFMT(LogMetaHumanSDK, Verbose, "FMetaHumanAssetUpdateHandler: Shutting down");
	Instance.Reset();
}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/MutableStreamableManager.h"


int32 FMutableStreamableManager::Tick(bool bBlocking)
{
	if (bBlocking)
	{
		for (const TSharedRef<FMutableStreamableHandle>& MutableStreamableHandle : StreamableHandles)
		{
			if (!MutableStreamableHandle->bCompleted)
			{
				TArray<FSoftObjectPath> RequestedAssets;
				MutableStreamableHandle->Handle->GetRequestedAssets(RequestedAssets);

				FCookLoadScope CookLoadScope(ECookLoadType::EditorOnly);
				StreamableManager.RequestSyncLoad(RequestedAssets);

				MutableStreamableHandle->Delegate.ExecuteIfBound();
				MutableStreamableHandle->Handle->CancelHandle();
			}			
		}

		StreamableHandles.Empty();
	}

	return 0;
}

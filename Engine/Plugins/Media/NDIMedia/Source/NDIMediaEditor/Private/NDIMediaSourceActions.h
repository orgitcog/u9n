// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTools/MediaSourceActions.h"

/**
 * Implements an action for UNDIMediaSource assets. 
 */
class FNDIMediaSourceActions : public FMediaSourceActions
{
public:
	//~ Begin FAssetTypeActions_Base
	virtual bool CanFilter() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	//~ End FAssetTypeActions_Base
};

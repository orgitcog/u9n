// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaStreamObjectHandler.h"

class FMediaStreamMediaSourceHandler : public IMediaStreamObjectHandler
{
public:
	static UClass* GetClass();

	//~ Begin IMediaStreamObjectHandler
	virtual UMediaPlayer* CreateOrUpdatePlayer(const FMediaStreamObjectHandlerCreatePlayerParams& InParams) override;
	//~ End IMediaStreamObjectHandler
};

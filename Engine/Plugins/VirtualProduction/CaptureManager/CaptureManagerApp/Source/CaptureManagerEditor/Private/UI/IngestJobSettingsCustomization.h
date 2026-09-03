// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"

class FIngestJobSettingsCustomization : public IDetailCustomization
{
public:
	~FIngestJobSettingsCustomization();

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	FIngestJobSettingsCustomization();
};


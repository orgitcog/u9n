// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

struct FNamingTokenFilterArgs;

/**
 * Customization for the capture manager settings.
 */
class FCaptureManagerSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	
private:
	FText GetDisplayTokenText(FNamingTokenFilterArgs InArgs) const;
};
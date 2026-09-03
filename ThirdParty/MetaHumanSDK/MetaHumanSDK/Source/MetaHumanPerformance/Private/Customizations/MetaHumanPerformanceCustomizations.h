// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IDetailCustomization.h"
#include "AssetRegistry/AssetData.h"

class FMetaHumanPerformanceCustomization
	: public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~End IDetailCustomization interface

private:
	/** Filter ControlRig assets that are compatible with this Performance */
	bool ShouldFilterControlRigAsset(const FAssetData& InAssetData) const;
};
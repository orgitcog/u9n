// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStructDetails.h"

class FDataLinkInstancedStructNodeBuilder : public FInstancedStructDataDetails
{
public:
	explicit FDataLinkInstancedStructNodeBuilder(const TSharedRef<IPropertyHandle>& InInputDataElementHandle);

	//~ Begin IDetailCustomNodeBuilder
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& InNodeRow) override;
	virtual void Tick(float InDeltaTime) override;
	//~ End IDetailCustomNodeBuilder

private:
	TSharedRef<IPropertyHandle> InputDataElementHandle;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class UDataLinkGraph;
struct FInstancedStruct;

class FDataLinkInstanceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(bool bInGenerateHeader)
	{
		return MakeShared<FDataLinkInstanceCustomization>(bInGenerateHeader);
	}

	explicit FDataLinkInstanceCustomization(bool bInGenerateHeader);

	virtual ~FDataLinkInstanceCustomization() override;

protected:
	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	//~ End IPropertyTypeCustomization

	void OnGraphCompiled(UDataLinkGraph* InDataLinkGraph);

	UDataLinkGraph* GetDataLinkGraph() const;

	TArray<FInstancedStruct>* GetInputData() const;

	void OnGraphChanged();
	void UpdateInputData();

	TSharedPtr<IPropertyHandle> DataLinkGraphHandle;
	TSharedPtr<IPropertyHandle> InputDataHandle;

	TArray<FText> InputDisplayNames;

	bool bGenerateHeader;
};

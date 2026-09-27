// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"

/**
 * Detail Customization for the UMetaHumanCharacterImportTemplateProperties class, which is 
 * used for importing from template
 */
class FMetaHumanCharacterImportTemplatePropertiesCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~End IDetailCustomization interface
private:
	IDetailLayoutBuilder* DetailBuilder;

	void OnMeshPropertyChanged();
};

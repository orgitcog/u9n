// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Misc/Optional.h"

class UMetaHumanCollection;
class UMetaHumanCollectionPipeline;
enum class EMetaHumanQualityLevel : uint8;

namespace UE::MetaHuman::Private
{
	struct FPipelineOption;
	struct FPipelineSelection;
}

/**
 * Detail Customization for the UMetaHumanCharacterEditorPipelineToolProperties class, which is 
 * used for selecting a build pipeline
 */
class FMetaHumanCharacterEditorPipelineToolPropertiesCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FMetaHumanCharacterEditorPipelineToolPropertiesCustomization);
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
	// End of IDetailCustomization interface

private:

	void RebuildDetailsView();

	TWeakPtr<IDetailLayoutBuilder> CachedDetailBuilder;
};

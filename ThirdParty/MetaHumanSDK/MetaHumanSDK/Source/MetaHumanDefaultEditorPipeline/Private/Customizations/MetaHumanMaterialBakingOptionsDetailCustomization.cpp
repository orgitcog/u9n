// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMaterialBakingOptionsDetailCustomization.h"
#include "MetaHumanDefaultEditorPipelineBase.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"

TSharedRef<IPropertyTypeCustomization> FMetaHumanMaterialBakingOptionsDetailCustomziation::MakeInstance()
{
	return MakeShared<FMetaHumanMaterialBakingOptionsDetailCustomziation>();
}

void FMetaHumanMaterialBakingOptionsDetailCustomziation::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	InHeaderRow.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		InPropertyHandle->CreatePropertyValueWidget()
	];
}

void FMetaHumanMaterialBakingOptionsDetailCustomziation::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	TArray<UObject*> Objects;
	InPropertyHandle->GetOuterObjects(Objects);

	if (!Objects.IsEmpty())
	{
		if (UMetaHumanDefaultEditorPipelineBase* Pipeline = Cast<UMetaHumanDefaultEditorPipelineBase>(Objects[0]))
		{
			FMetaHumanMaterialBakingOptions* BakingOptions = (FMetaHumanMaterialBakingOptions*) InPropertyHandle->GetValueBaseAddress((uint8*) Pipeline);

			TSharedPtr<IPropertyHandle> BakingSettingsProperty = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaHumanMaterialBakingOptions, BakingSettings));

			BakingSettingsProperty->SetOnPropertyValueChanged(
				FSimpleDelegate::CreateSPLambda(this, [BakingSettingsProperty, BakingOptions]()
				{
					UObject* BakingSettingsObject = nullptr;
					if (BakingSettingsProperty->GetValue(BakingSettingsObject) == FPropertyAccess::Success)
					{
						TSet<FName> OutputTextures;
						if (UMetaHumanMaterialBakingSettings* BakingSettings = Cast<UMetaHumanMaterialBakingSettings>(BakingSettingsObject))
						{
							for (const FMetaHumanTextureGraphOutputProperties& Graph : BakingSettings->TextureGraphs)
							{
								for (const FMetaHumanOutputTextureProperties& OutputTexture : Graph.OutputTextures)
								{
									// BakingOptions->TextureResolutions.FindOrAdd(OutputTexture.OutputTextureName);
									OutputTextures.Add(OutputTexture.OutputTextureName);
								}
							}
						}

						// Remove the entries that are not in the baking settings object anymore but keep the ones that are already in the map
						for (TMap<FName, EMetaHumanBuildTextureResolution>::TIterator It = BakingOptions->TextureResolutionsOverrides.CreateIterator(); It; ++It)
						{
							if (!OutputTextures.Contains(It.Key()))
							{
								It.RemoveCurrent();
							}
						}

						// Add the new ones
						for (FName OutputTexture : OutputTextures)
						{
							BakingOptions->TextureResolutionsOverrides.FindOrAdd(OutputTexture);
						}
					}
				})
			);
		}
	}


	// Add all children to the builder
	uint32 NumChildren = 0;
	if (InPropertyHandle->GetNumChildren(NumChildren) == FPropertyAccess::Success)
	{
		for (uint32 Index = 0; Index < NumChildren; ++Index)
		{
			if (TSharedPtr<IPropertyHandle> ChildProperty = InPropertyHandle->GetChildHandle(Index))
			{
				InChildBuilder.AddProperty(ChildProperty.ToSharedRef());
			}
		}
	}
}


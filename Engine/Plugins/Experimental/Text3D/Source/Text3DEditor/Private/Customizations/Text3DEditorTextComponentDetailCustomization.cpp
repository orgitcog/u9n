// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/Text3DEditorTextComponentDetailCustomization.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Text3DComponent.h"

namespace UE::Text3DEditor::Customization
{
	void FText3DEditorTextComponentDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
	{
		static bool bSectionInitialized = false;
		if (!bSectionInitialized)
		{
			bSectionInitialized = true;

			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			const FName ComponentClassName = UText3DComponent::StaticClass()->GetFName();

			const FName GeometrySectionName = TEXT("Geometry");
			const TSharedRef<FPropertySection> GeometrySection = PropertyModule.FindOrCreateSection(ComponentClassName, GeometrySectionName, FText::FromName(GeometrySectionName));
			GeometrySection->AddCategory(TEXT("Geometry"));

			const FName LayoutSectionName = TEXT("Layout");
			const TSharedRef<FPropertySection> LayoutSection = PropertyModule.FindOrCreateSection(ComponentClassName, LayoutSectionName, FText::FromName(LayoutSectionName));
			LayoutSection->AddCategory(TEXT("Layout"));
			LayoutSection->AddCategory(TEXT("LayoutEffects"));
			LayoutSection->AddCategory(TEXT("Character"));

			const FName RenderingSectionName = TEXT("Rendering");
			const TSharedRef<FPropertySection> RenderingSection = PropertyModule.FindOrCreateSection(ComponentClassName, RenderingSectionName, FText::FromName(RenderingSectionName));
			RenderingSection->AddCategory(TEXT("Rendering"));

			const FName TextSectionName = TEXT("Text");
			const TSharedRef<FPropertySection> TextSection = PropertyModule.FindOrCreateSection(ComponentClassName, TextSectionName, FText::FromName(TextSectionName));
			TextSection->AddCategory(TEXT("Text"));

			const FName StyleSectionName = TEXT("Style");
			const TSharedRef<FPropertySection> MaterialSection = PropertyModule.FindOrCreateSection(ComponentClassName, StyleSectionName, FText::FromName(StyleSectionName));
			MaterialSection->AddCategory(TEXT("Material"));

			const FName EffectSectionName = TEXT("Effects");
			const TSharedRef<FPropertySection> EffectsSection = PropertyModule.FindOrCreateSection(ComponentClassName, EffectSectionName, FText::FromName(EffectSectionName));
			EffectsSection->AddCategory(TEXT("LayoutEffects"));
		}
	}
}

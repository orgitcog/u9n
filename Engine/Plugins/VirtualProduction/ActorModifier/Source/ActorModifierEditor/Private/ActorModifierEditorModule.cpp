// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorModifierEditorModule.h"

#include "ActorModifierTypes.h"
#include "Customizations/ActorModifierEditorAnchorAlignmentPropertyTypeCustomization.h"
#include "Customizations/ActorModifierEditorSplinePathModifierDetailCustomization.h"
#include "Modifiers/ActorModifierSplinePathModifier.h"
#include "PropertyEditorModule.h"
#include "Styles/ActorModifierEditorStyle.h"

IMPLEMENT_MODULE(FActorModifierEditorModule, ActorModifierEditor)

void FActorModifierEditorModule::StartupModule()
{
	FActorModifierEditorStyle::Get();
	
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	PropertyModule.RegisterCustomPropertyTypeLayout(FActorModifierAnchorAlignment::StaticStruct()->GetFName()
	, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FActorModifierEditorAnchorAlignmentPropertyTypeCustomization::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(UActorModifierSplinePathModifier::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FActorModifierEditorSplinePathModifierDetailCustomization::MakeInstance));
}

void FActorModifierEditorModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(FActorModifierAnchorAlignment::StaticStruct()->GetFName());
		PropertyModule->UnregisterCustomClassLayout(UActorModifierSplinePathModifier::StaticClass()->GetFName());
	}
}
    

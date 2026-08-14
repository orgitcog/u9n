// Copyright Epic Games, Inc. All Rights Reserved.

#include "UIComponentUtils.h"

#include "Blueprint/UserWidget.h"
#include "ClassViewerModule.h"
#include "CoreGlobals.h"
#include "UIComponentWidgetBlueprintExtension.h"
#include "Extensions/UIComponent.h"
#include "WidgetBlueprintEditor.h"
#include "Extensions/UIComponentUserWidgetExtension.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UMG"

FClassViewerInitializationOptions FUIComponentUtils::CreateClassViewerInitializationOptions()
{
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	TSharedPtr<FUIComponentUtils::FUIComponentClassFilter> Filter = MakeShared<FUIComponentUtils::FUIComponentClassFilter>();
	Options.ClassFilters.Add(Filter.ToSharedRef());

	Filter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown | CLASS_Abstract;
	Filter->AllowedChildrenOfClasses.Add(UUIComponent::StaticClass());

	return Options;
}


void FUIComponentUtils::OnWidgetRenamed(const TSharedRef<FWidgetBlueprintEditor>& BlueprintEditor, UWidgetBlueprint* WidgetBlueprint, const FName& OldVarName, const FName& NewVarName)
{
	// On a Widget rename in the Editor we update the Widget names in UI Components extensions	
	if (UUIComponentWidgetBlueprintExtension* Extension = UWidgetBlueprintExtension::GetExtension<UUIComponentWidgetBlueprintExtension>(WidgetBlueprint))
	{
		Extension->RenameWidget(OldVarName, NewVarName);
	}

	if (const UUserWidget* PreviewWidget = BlueprintEditor->GetPreview())
	{
		if(UUIComponentUserWidgetExtension* UserWidgetExtension = PreviewWidget->GetExtension<UUIComponentUserWidgetExtension>())
		{
			UserWidgetExtension->RenameWidget(OldVarName, NewVarName);
		}
	}	
}

void FUIComponentUtils::AddComponent(const TSharedRef<FWidgetBlueprintEditor>& BlueprintEditor, const UClass* ComponentClass, const FName WidgetName)
{	
	UWidgetBlueprint* WidgetBlueprint = BlueprintEditor->GetWidgetBlueprintObj();

	const FScopedTransaction Transaction(LOCTEXT("AddComponent", "Add Component"));
	if (UUIComponentWidgetBlueprintExtension* WidgetBlueprintExtension = UUIComponentWidgetBlueprintExtension::RequestExtension<UUIComponentWidgetBlueprintExtension>(WidgetBlueprint))
	{
		UUIComponent* ComponentArchetype = WidgetBlueprintExtension->AddComponent(ComponentClass, WidgetName);		
		UUserWidget* PreviewWidget = BlueprintEditor->GetPreview();
		
		if (ComponentArchetype && PreviewWidget)
		{
			// If the extension do not exist, we create it which will create a copy of the component we just added.
			if (UUIComponentUserWidgetExtension* UserWidgetExtension = WidgetBlueprintExtension->GetOrCreateExtension(PreviewWidget))			
			{
				UserWidgetExtension->CreateAndAddComponent(ComponentArchetype, WidgetName);
			}	
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}
}

void FUIComponentUtils::RemoveComponent(const TSharedRef<FWidgetBlueprintEditor>& BlueprintEditor, const UClass* ComponentClass, const FName WidgetName)
{
	UWidgetBlueprint* WidgetBlueprint = BlueprintEditor->GetWidgetBlueprintObj();
	const FScopedTransaction Transaction(LOCTEXT("RemoveComponent", "Remove Component"));
	if (UUIComponentWidgetBlueprintExtension* WidgetBlueprintExtension = UWidgetBlueprintExtension::GetExtension<UUIComponentWidgetBlueprintExtension>(WidgetBlueprint))
	{
		WidgetBlueprintExtension->RemoveComponent(ComponentClass, WidgetName);

		// Also Remove it from the Preview
		if (const UUserWidget* PreviewWidget = BlueprintEditor->GetPreview())
		{
			if (UUIComponentUserWidgetExtension* UserWidgetExtension = PreviewWidget->GetExtension<UUIComponentUserWidgetExtension>())
			{
				UserWidgetExtension->RemoveComponent(ComponentClass, WidgetName);
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}	
}

bool FUIComponentUtils::FUIComponentClassFilter::IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs)
{
	return !InClass->HasAnyClassFlags(DisallowedClassFlags)
		&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
}

bool FUIComponentUtils::FUIComponentClassFilter::IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs)
{
	return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
		&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
}

#undef LOCTEXT_NAMESPACE
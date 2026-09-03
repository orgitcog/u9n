// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Internationalization/Internationalization.h"
#include "Misc/AssertionMacros.h"
#include "ScopedTransaction.h"
#include "UObject/Script.h"

/////////////////////////////////////////////////////
#define LOCTEXT_NAMESPACE "EditorUtility"

UWidget* UEditorUtilityWidget::FindChildWidgetByName(FName WidgetName) const
{
	if (WidgetTree)
	{
		return WidgetTree->FindWidget(WidgetName);
	}

	return nullptr;
}

void UEditorUtilityWidget::ExecuteDefaultAction()
{
	check(bAutoRunDefaultAction);

	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action"));
	FEditorScriptExecutionGuard ScriptGuard;

	Run();
}



#undef LOCTEXT_NAMESPACE
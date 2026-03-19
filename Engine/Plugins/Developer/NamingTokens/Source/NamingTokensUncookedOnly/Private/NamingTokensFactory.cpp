// Copyright Epic Games, Inc. All Rights Reserved.

#include "NamingTokensFactory.h"

#include "NamingTokens.h"

#include "BlueprintEditorSettings.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "K2Node_Event.h"
#include "Kismet2/KismetEditorUtilities.h"

UNamingTokensFactory::UNamingTokensFactory()
{
	SupportedClass = UNamingTokens::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UNamingTokensFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
	FFeedbackContext* Warn)
{
	UClass* ParentClass = SupportedClass;
	check(InClass->IsChildOf(ParentClass));
    
	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		InParent,
		InName,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);
	
	const UBlueprintEditorSettings* Settings = GetDefault<UBlueprintEditorSettings>();
	if (Settings && Settings->bSpawnDefaultBlueprintNodes)
	{
		// Create default events.
		int32 NodePositionY = 0;

		UEdGraph* EventGraph = FindObject<UEdGraph>(NewBP, *(UEdGraphSchema_K2::GN_EventGraph.ToString()));
		check(EventGraph);
		
		if (UK2Node_Event* OnPreEvaluateNode = FKismetEditorUtilities::AddDefaultEventNode(NewBP, EventGraph,
			GET_FUNCTION_NAME_CHECKED(UNamingTokens, OnPreEvaluate), UNamingTokens::StaticClass(), NodePositionY))
		{
			// Set the node comment. The comment is displayed because we are automatically placed as ghost nodes.
			// Once a connection is made the comment will go away, but is still accessible via tooltip.
			const UFunction* Function = UNamingTokens::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNamingTokens, OnPreEvaluate));
			check(Function);
			OnPreEvaluateNode->NodeComment = Function->GetToolTipText().ToString();
		}
		if (UK2Node_Event* OnPostEvaluateNode = FKismetEditorUtilities::AddDefaultEventNode(NewBP, EventGraph,
			GET_FUNCTION_NAME_CHECKED(UNamingTokens, OnPostEvaluate), UNamingTokens::StaticClass(), NodePositionY))
		{
			// Set the node comment. The comment is displayed because we are automatically placed as ghost nodes.
			// Once a connection is made the comment will go away, but is still accessible via tooltip.
			const UFunction* Function = UNamingTokens::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNamingTokens, OnPostEvaluate));
			check(Function);
			OnPostEvaluateNode->NodeComment = Function->GetToolTipText().ToString();
		}
	}

	return NewBP;
}

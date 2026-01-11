// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateMachineGraphCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyBagDetails.h"
#include "SceneStateBlueprintEditorUtils.h"
#include "SceneStateMachineGraph.h"
#include "SceneStateParameterDetails.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::SceneState::Editor
{

void FStateMachineGraphCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TSharedRef<IPropertyHandle> StateMachineIdHandle = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USceneStateMachineGraph, ParametersId));
	TSharedRef<IPropertyHandle> ParametersHandle = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USceneStateMachineGraph, Parameters));
	TSharedRef<IPropertyHandle> RunModeHandle = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USceneStateMachineGraph, RunMode));

	StateMachineIdHandle->MarkHiddenByCustomization();
	ParametersHandle->MarkHiddenByCustomization();
	RunModeHandle->MarkHiddenByCustomization();

	// Listen to Parameter Changes to broadcast change
	{
		TArray<TWeakObjectPtr<USceneStateMachineGraph>> Graphs = InDetailBuilder.GetObjectsOfTypeBeingCustomized<USceneStateMachineGraph>();
		ParametersHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSPLambda(this,
			[Graphs = MoveTemp(Graphs)]()
			{
				for (TWeakObjectPtr<USceneStateMachineGraph> GraphWeak : Graphs)
				{
					if (USceneStateMachineGraph* Graph = GraphWeak.Get())
					{
						Graph->NotifyParametersChanged();
					}
				}
			}));
	}

	// State Machine Category
	IDetailCategoryBuilder& StateMachineCategory = InDetailBuilder.EditCategory(TEXT("State Machine"));
	StateMachineCategory.AddProperty(RunModeHandle);

	FGuid StateMachineId;
	GetGuid(StateMachineIdHandle, StateMachineId);

	// Parameters Category
	IDetailCategoryBuilder& ParametersCategory = InDetailBuilder.EditCategory(TEXT("Parameters"));
	ParametersCategory.HeaderContent(FParameterDetails::BuildHeader(InDetailBuilder, ParametersHandle), /*bWholeRowContent*/true);

	ParametersCategory.AddCustomBuilder(MakeShared<FParameterDetails>(ParametersHandle
		, InDetailBuilder.GetPropertyUtilities()
		, StateMachineId
		, /*bFixedLayout*/false));
}

} // UE::SceneState::Editor

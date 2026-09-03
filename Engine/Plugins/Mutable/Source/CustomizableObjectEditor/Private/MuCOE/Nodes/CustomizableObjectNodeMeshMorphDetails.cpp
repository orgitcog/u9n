// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphDetails.h"

#include "DetailLayoutBuilder.h"
#include "Engine/SkeletalMesh.h"
#include "IDetailsView.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/STextComboBox.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"

TSharedRef<IDetailCustomization> FCustomizableObjectNodeMeshMorphDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeMeshMorphDetails );
}

void FCustomizableObjectNodeMeshMorphDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FCustomizableObjectNodeDetails::CustomizeDetails(DetailBuilder);

	TSharedPtr<const IDetailsView> DetailsView = DetailBuilder.GetDetailsViewSharedPtr();
	if (DetailsView && DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeMeshMorph>( DetailsView->GetSelectedObjects()[0].Get() );
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "Morph Target" );
	TSharedRef<IPropertyHandle> MorphTargetNameProperty = DetailBuilder.GetProperty("MorphTargetName");

	//BlocksCategory.CategoryIcon( "ActorClassIcon.CustomizableObject" );
	
	MorphTargetComboOptions.Empty();

	bool SourceMeshFound = false;

	if (!Node)
	{
		return;
	}

	if (const UEdGraphPin* ConnectedPin = FollowInputPin(*Node->MeshPin()))
	{
		TSharedPtr<FString> ItemToSelect;
		const UEdGraphPin* BaseSourcePin = FindMeshBaseSource(*ConnectedPin, false );
		
		if ( BaseSourcePin )
		{
			USkeletalMesh* SkeletalMesh = nullptr;
			if ( const UCustomizableObjectNodeSkeletalMesh* TypedNodeSkeletalMesh = Cast<UCustomizableObjectNodeSkeletalMesh>(BaseSourcePin->GetOwningNode()) )
			{
				SkeletalMesh = MutablePrivate::LoadObject(TypedNodeSkeletalMesh->SkeletalMesh);
			}
			else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(BaseSourcePin->GetOwningNode()))
			{
				SkeletalMesh = TypedNodeTable->GetColumnDefaultAssetByType<USkeletalMesh>(BaseSourcePin);
			}

			if (SkeletalMesh)
			{
				SourceMeshFound = true;

				for (int m = 0; m < SkeletalMesh->GetMorphTargets().Num(); ++m)
				{
					FString MorphName = *SkeletalMesh->GetMorphTargets()[m]->GetName();
					MorphTargetComboOptions.Add(MakeShareable(new FString(MorphName)));

					if (Node->MorphTargetName == MorphName)
					{
						ItemToSelect = MorphTargetComboOptions.Last();
					}
				}

				MorphTargetComboOptions.Sort(CompareNames);

				DetailBuilder.EditDefaultProperty(MorphTargetNameProperty)->
				CustomWidget()
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MeshMorphDetails_MorphTargetName", "Morph Target Name")).Font(IDetailLayoutBuilder::GetDetailFont())
					.IsEnabled(this, &FCustomizableObjectNodeMeshMorphDetails::IsMorphNameSelectorWidgetEnabled)
					.ToolTipText(this, &FCustomizableObjectNodeMeshMorphDetails::MorphNameSelectorWidgetTooltip)
				]
				.ValueContent()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(STextComboBox)
						.OptionsSource(&MorphTargetComboOptions)
						.InitiallySelectedItem(ItemToSelect)
						.OnSelectionChanged(this, &FCustomizableObjectNodeMeshMorphDetails::OnMorphTargetComboBoxSelectionChanged, MorphTargetNameProperty)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.IsEnabled(this, &FCustomizableObjectNodeMeshMorphDetails::IsMorphNameSelectorWidgetEnabled)
						.ToolTipText(this, &FCustomizableObjectNodeMeshMorphDetails::MorphNameSelectorWidgetTooltip)
					]
				];
			}
		}
	}

	if (!SourceMeshFound)
	{
		FText Message = Node->IsInMacro() ? LOCTEXT("MeshMorphDetails_PinMessage", "In Mutable Macros, Morph Target Names are deffined through String Nodes.") : LOCTEXT("MeshMorphDetails_NoSource", "No source mesh found.");

		DetailBuilder.EditDefaultProperty(MorphTargetNameProperty)->
			CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MeshMorphDetails_MorphTargetName", "Morph Target Name")).Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(STextBlock)
				.Text(Message)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}
}


void FCustomizableObjectNodeMeshMorphDetails::OnMorphTargetComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty)
{	
	if (Selection.IsValid())
	{
		ParentProperty->SetValue(*Selection);	
	}
}


bool FCustomizableObjectNodeMeshMorphDetails::IsMorphNameSelectorWidgetEnabled() const
{
	// Disabled if there is a string node linked to the "Required Tags" pin.
	return !(Node->MorphTargetNamePin() && FollowInputPin(*Node->MorphTargetNamePin()));
}


FText FCustomizableObjectNodeMeshMorphDetails::MorphNameSelectorWidgetTooltip() const
{
	if (Node->MorphTargetNamePin() && FollowInputPin(*Node->MorphTargetNamePin()))
	{
		return LOCTEXT("MorphTargetNameWidgetTooltip_Ignored", "Disabled. When there is a string node linked to the Morph Target Name pin the morph target name selected in this widget is ignored.");
	}
	else
	{
		return LOCTEXT("MorphTargetNameWidgetTooltip", "Select the morph target name.");
	}
}

#undef LOCTEXT_NAMESPACE


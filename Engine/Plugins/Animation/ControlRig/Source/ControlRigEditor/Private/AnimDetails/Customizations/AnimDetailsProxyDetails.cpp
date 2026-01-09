// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyDetails.h"

#include "AnimDetails/AnimDetailsProxyManager.h"
#include "AnimDetails/AnimDetailsSelection.h"
#include "AnimDetails/Proxies/AnimDetailsProxyBase.h"
#include "AnimDetails/Widgets/SAnimDetailsPropertySelectionBorder.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "IDetailGroup.h"

#define LOCTEXT_NAMESPACE "AnimDetailsProxyDetails"

namespace UE::ControlRigEditor
{
	FAnimDetailsProxyDetails::FAnimDetailsProxyDetails()
	{}

	TSharedRef<IDetailCustomization> FAnimDetailsProxyDetails::MakeInstance()
	{
		return MakeShared<FAnimDetailsProxyDetails>();
	}

	void FAnimDetailsProxyDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		DetailBuilder.GetObjectsBeingCustomized(EditedObjects);

		bool bIsIndividual = false;
		TArray<UAnimDetailsProxyBase*> AllProxies;
		for (int32 ProxyIndex = 0; ProxyIndex < EditedObjects.Num(); ProxyIndex++)
		{
			UAnimDetailsProxyBase* Proxy = Cast<UAnimDetailsProxyBase>(EditedObjects[ProxyIndex].Get());
			if (Proxy)
			{
				AllProxies.Add(Proxy);

				bIsIndividual |= Proxy->bIsIndividual;
			}
		}

		if (AllProxies.IsEmpty())
		{
			return;
		}
		const UAnimDetailsProxyBase* FirstProxy = AllProxies[0];

		// Find the category name text
		FText DisplayNameText;
		if (AllProxies.Num() == 1 && AllProxies[0])
		{
			DisplayNameText = AllProxies[0]->GetDisplayNameText();
			AllProxies[0]->UpdatePropertyDisplayNames(DetailBuilder);
		}
		else
		{
			FString DisplayString = TEXT("Multiple");
			DisplayNameText = FText::FromString(*DisplayString);
		}

		// Create a custom row to display the header instead of using the category row, so it cannot be collapsed
		IDetailCategoryBuilder& NoCategory = DetailBuilder.EditCategory("NoCategory");

		if (!bIsIndividual)
		{
			const FText& NoFilterText = FText::GetEmpty();
			NoCategory.AddCustomRow(NoFilterText)
				.WholeRowContent()
				[
					SNew(SBorder)
					.VAlign(VAlign_Center)
					.OnMouseButtonDown(this, &FAnimDetailsProxyDetails::OnHeaderCategoryRowClicked)
					.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
					[
						SNew(STextBlock)
						.Text(DisplayNameText)
						.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					]
				];
		}

		// Add properties to anim details specific categories instead of the default category
		const FName CategoryName = FirstProxy->GetCategoryName();

		IDetailCategoryBuilder& DefaultCategory = DetailBuilder.EditCategory(CategoryName);
		TArray<TSharedRef<IPropertyHandle>> PropertyHandles;
		DefaultCategory.GetDefaultProperties(PropertyHandles);
		DefaultCategory.SetCategoryVisibility(false);

		IDetailGroup* PropertyGroupPtr = nullptr;
		for (const TSharedRef<IPropertyHandle>& PropertyHandle : PropertyHandles)
		{	
			FProperty* Property = PropertyHandle->GetProperty();
			if (!Property)
			{
				continue;
			}

			Property->SetPropertyFlags(CPF_TextExportTransient); // Hack to turn off shift copy/paste

			uint32 NumChildren;
			PropertyHandle->GetNumChildren(NumChildren);

			if (bIsIndividual && NumChildren < 2)
			{
				NoCategory.AddProperty(PropertyHandle);
				NoCategory.InitiallyCollapsed(false);
			}
			else if (bIsIndividual)
			{
				if (!PropertyGroupPtr)
				{
					constexpr bool bForAdvanced = false;
					constexpr bool bStartExpanded = true;
					PropertyGroupPtr = &NoCategory.AddGroup(CategoryName, DisplayNameText, bForAdvanced, bStartExpanded);
				}

				PropertyGroupPtr->AddPropertyRow(PropertyHandle);
			}			
			else
			{
				NoCategory.AddProperty(PropertyHandle);
				NoCategory.InitiallyCollapsed(false);
			}
		}
	}

	FReply FAnimDetailsProxyDetails::OnHeaderCategoryRowClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		// Reset selection
		FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
		UAnimDetailsProxyManager* ProxyManager = EditMode ? EditMode->GetAnimDetailsProxyManager() : nullptr;
		UAnimDetailsSelection* Selection = ProxyManager ? ProxyManager->GetAnimDetailsSelection() : nullptr;
		if (Selection)
		{
			Selection->ClearSelection();
		}

		return FReply::Handled();
	}
}

#undef LOCTEXT_NAMESPACE

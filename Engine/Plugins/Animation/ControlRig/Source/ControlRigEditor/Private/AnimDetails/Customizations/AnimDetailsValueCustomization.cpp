// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsValueCustomization.h"

#include "AnimDetails/AnimDetailsProxyManager.h"
#include "AnimDetails/Proxies/AnimDetailsProxyLocation.h"
#include "AnimDetails/Proxies/AnimDetailsProxyRotation.h"
#include "AnimDetails/Proxies/AnimDetailsProxyScale.h"
#include "AnimDetails/Proxies/AnimDetailsProxyVector2D.h"
#include "AnimDetails/Widgets/SAnimDetailsPropertySelectionBorder.h"
#include "AnimDetails/Widgets/SAnimDetailsValueBoolean.h"
#include "AnimDetails/Widgets/SAnimDetailsValueNumeric.h"
#include "DetailWidgetRow.h"
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "AnimDetailsValueCustomization"

namespace UE::ControlRigEditor
{
	TSharedRef<IPropertyTypeCustomization> FAnimDetailsValueCustomization::MakeInstance()
	{
		return MakeShared<FAnimDetailsValueCustomization>();
	}

	void FAnimDetailsValueCustomization::CustomizeChildren(
		TSharedRef<IPropertyHandle> InStructPropertyHandle, 
		IDetailChildrenBuilder& InStructBuilder, 
		IPropertyTypeCustomizationUtils& InStructCustomizationUtils)
	{
		if (IsStructPropertyHiddenByFilter(InStructPropertyHandle))
		{
			InStructPropertyHandle->MarkHiddenByCustomization();
			return;
		}

		IDetailCategoryBuilder& CategoryBuilder = InStructBuilder.GetParentCategory();
		DetailBuilder = &CategoryBuilder.GetParentLayout();

		StructPropertyHandle = InStructPropertyHandle;

		// Show custom children if expanded, allowing to select individual controls in the property name row
		for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

			if (IsChildPropertyHiddenByFilter(ChildHandle))
			{
				continue;
			}

			const FName PropertyName = ChildHandle->GetProperty()->GetFName();
			const FText PropertyDisplayText = ChildHandle->GetPropertyDisplayName();

			const TSharedRef<SWidget> ValueWidget = MakeChildWidget(InStructPropertyHandle, ChildHandle);
			
			InStructBuilder.AddProperty(ChildHandle)
				.CustomWidget()
				.NameContent()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SAnimDetailsPropertySelectionBorder, ChildHandle)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
						.Text_Lambda([PropertyDisplayText]()
							{
								return PropertyDisplayText;
							})
					]
				]
				.ValueContent()
				.HAlign(HAlign_Fill)
				[
					SNew(SAnimDetailsPropertySelectionBorder, ChildHandle)
					.RequiresModifierKeys(true)
					[
						ValueWidget
					]
				]
				.ExtensionContent()
				[
					ChildHandle->CreateDefaultPropertyButtonWidgets()
				]
				.PasteAction(
					FUIAction(
						FExecuteAction::CreateLambda([]() 
							{ 
								checkNoEntry(); 
							}),
						FCanExecuteAction::CreateLambda([]() 
							{ 
								checkNoEntry(); 
								return false; 
							})
					)
				);
		}
	}

	void FAnimDetailsValueCustomization::MakeHeaderRow(TSharedRef<IPropertyHandle>& InStructPropertyHandle, FDetailWidgetRow& InRow)
	{
		constexpr TCHAR ShowOnlyInnerPropertiesMetaDataName[] = TEXT("ShowOnlyInnerProperties");
		bool bShowHeader = !InStructPropertyHandle->HasMetaData(ShowOnlyInnerPropertiesMetaDataName);
		if (!bShowHeader || 
			IsStructPropertyHiddenByFilter(InStructPropertyHandle))
		{
			return;
		}

		StructPropertyHandle = InStructPropertyHandle;
		
		// Make enough space for each child handle
		const float DesiredWidth = 125.f * SortedChildHandles.Num();

		TSharedPtr<SHorizontalBox> HorizontalBox;

		InRow.NameContent()
			[
				MakePropertyNameWidget(InStructPropertyHandle)
			]
			.PasteAction(FUIAction(
				FExecuteAction::CreateLambda([]()
					{
						checkNoEntry();
					}),
				FCanExecuteAction::CreateLambda([]() 
					{ 
						return false; 
					}))
			)
			.ValueContent()
			.MinDesiredWidth(DesiredWidth)
			.MaxDesiredWidth(DesiredWidth)
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
				.Visibility(this, &FAnimDetailsValueCustomization::GetVisibilityFromExpansionState)
				.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, StructPropertyHandle.ToWeakPtr())
			];

		// Create inline children if collapsed, the children can only be selected in the property value row
		for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];
			FProperty* Property = ChildHandle->GetProperty();
			if (!Property)
			{
				continue;
			}

			Property->SetPropertyFlags(CPF_TextExportTransient); // Hack to turn off shift copy/paste

			const TSharedRef<SWidget> ChildWidget = MakeChildWidget(InStructPropertyHandle, ChildHandle);

			// Always display childs in the struct row but disable them if they're filtered out
			if (IsChildPropertyHiddenByFilter(ChildHandle))
			{
				ChildWidget->SetEnabled(false);
				ChildWidget->SetToolTipText(LOCTEXT("PropertyNotInFilterTooltip", "Excluded by search"));
			}

			const bool bLastChild = SortedChildHandles.Num() - 1 == ChildIndex;
			if (ChildHandle->GetPropertyClass() == FBoolProperty::StaticClass())
			{
				HorizontalBox->AddSlot()
					.Padding(FMargin(0.f, 2.f, bLastChild ? 0.f : 3.f, 2.f))
					.AutoWidth()  // keep the check box slots small
					[
						ChildWidget
					];
			}
			else
			{
				if (ChildHandle->GetPropertyClass() == FDoubleProperty::StaticClass())
				{
					NumericEntryBoxWidgetList.Add(ChildWidget);
				}

				HorizontalBox->AddSlot()
					.Padding(FMargin(0.f, 2.f, bLastChild ? 0.f : 3.f, 2.f))
					[
						ChildWidget
					];
			}
		}
	}

	TSharedRef<SWidget> FAnimDetailsValueCustomization::MakePropertyNameWidget(const TSharedRef<IPropertyHandle>& InStructPropertyHandle) const
	{
		uint32 NumChildren;
		InStructPropertyHandle->GetNumChildren(NumChildren);

		// For properties with only one child, display the control name instead of the struct property name
		if (NumChildren == 1)
		{
			TArray<UObject*> OuterObjects;
			InStructPropertyHandle->GetOuterObjects(OuterObjects);
			const UAnimDetailsProxyBase* OuterProxy = OuterObjects.IsEmpty() ? nullptr : Cast<UAnimDetailsProxyBase>(OuterObjects[0]);
			if (OuterProxy)
			{
				return
					SNew(STextBlock)
					.Text(InStructPropertyHandle->GetPropertyDisplayName())
					.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"));
			}
		}

		return InStructPropertyHandle->CreatePropertyNameWidget();
	}

	TSharedRef<SWidget> FAnimDetailsValueCustomization::MakeChildWidget(
		TSharedRef<IPropertyHandle>& StructurePropertyHandle,
		TSharedRef<IPropertyHandle>& PropertyHandle)
	{
		const FProperty* Property = PropertyHandle->GetProperty();
		if (!Property)
		{
			return SNullWidget::NullWidget;
		}

		const FName PropertyName = Property->GetFName();
		const FFieldClass* PropertyClass = PropertyHandle->GetPropertyClass();
		const FLinearColor LabelColor = GetColorFromProperty(PropertyName);
		const TSharedRef<SWidget> Label = SAnimDetailsValueNumeric<double>::BuildNarrowColorLabel(LabelColor);

		if (PropertyClass == FDoubleProperty::StaticClass())
		{
			return
				SNew(SAnimDetailsPropertySelectionBorder, PropertyHandle)
				.RequiresModifierKeys(true)
				[
					SNew(SAnimDetailsValueNumeric<double>, PropertyHandle)
					.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, PropertyHandle.ToWeakPtr())
					.LabelPadding(FMargin(3))
					.LabelLocation(SAnimDetailsValueNumeric<double>::ELabelLocation::Inside)
					.Label()
					[
						Label
					]
				];
		}
		else if (PropertyClass == FInt64Property::StaticClass())
		{
			return
				SNew(SAnimDetailsPropertySelectionBorder, PropertyHandle)
				.RequiresModifierKeys(true)
				[
					SNew(SAnimDetailsValueNumeric<int64>, PropertyHandle)
					.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, PropertyHandle.ToWeakPtr())
					.LabelPadding(FMargin(3))
					.LabelLocation(SAnimDetailsValueNumeric<int64>::ELabelLocation::Inside)
					.Label()
					[
						Label
					]
				];
		}
		else if (PropertyClass == FBoolProperty::StaticClass())
		{
			return
				SNew(SAnimDetailsPropertySelectionBorder, PropertyHandle)
				.RequiresModifierKeys(true)
				[
					SNew(SAnimDetailsValueBoolean, PropertyHandle)
					.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, PropertyHandle.ToWeakPtr())
				];
		}

		ensureMsgf(false, TEXT("Unsupported property class, cannot create an Anim Detail Values customization."));
		return SNullWidget::NullWidget;
	}

	bool FAnimDetailsValueCustomization::IsStructPropertyHiddenByFilter(const TSharedRef<class IPropertyHandle>& InStructPropertyHandle) const
	{
		FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
		UAnimDetailsProxyManager* ProxyManager = EditMode ? EditMode->GetAnimDetailsProxyManager() : nullptr;

		return 
			ProxyManager && 
			!ProxyManager->GetAnimDetailsFilter().ContainsStructProperty(InStructPropertyHandle);
	}

	bool FAnimDetailsValueCustomization::IsChildPropertyHiddenByFilter(const TSharedRef<IPropertyHandle>& InPropertyHandle) const
	{
		FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
		UAnimDetailsProxyManager* ProxyManager = EditMode ? EditMode->GetAnimDetailsProxyManager() : nullptr;

		return
			ProxyManager &&
			!ProxyManager->GetAnimDetailsFilter().ContainsProperty(InPropertyHandle);
	}

	EVisibility FAnimDetailsValueCustomization::GetVisibilityFromExpansionState() const
	{
		const bool bExpanded = StructPropertyHandle.IsValid() && StructPropertyHandle->IsExpanded();
		return bExpanded ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FLinearColor FAnimDetailsValueCustomization::GetColorFromProperty(const FName& PropertyName) const
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, X) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LX) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RX) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SX))
		{
			return SNumericEntryBox<double>::RedLabelBackgroundColor;
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, Y) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LY) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RY) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SY))
		{
			return SNumericEntryBox<double>::GreenLabelBackgroundColor;
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LZ) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RZ) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SZ))
		{
			return SNumericEntryBox<double>::BlueLabelBackgroundColor;
		}

		return FLinearColor::White;
	}
}

#undef LOCTEXT_NAMESPACE

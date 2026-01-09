// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsOverrideDetails.h"

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
#include "IPropertyUtilities.h"
#include "IDetailChildrenBuilder.h"

#define LOCTEXT_NAMESPACE "AnimDetailsProxyDetails"

namespace UE::ControlRigEditor
{
	void FAnimDetailsOverrideDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		HeaderRow.NameContent()
		.MaxDesiredWidth(30)
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
	}
	
	void FAnimDetailsOverrideDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized = StructCustomizationUtils.GetPropertyUtilities()->GetSelectedObjects();
		const UControlRig* ControlRig = nullptr;
		for(const TWeakObjectPtr<UObject>& ObjectBeingCustomized : ObjectsBeingCustomized)
		{
			if(const UAnimDetailsProxyBase* ControlsProxy = Cast<UAnimDetailsProxyBase>(ObjectBeingCustomized.Get()))
			{
				ControlRig = ControlsProxy->GetControlRig();
				if(ControlRig)
				{
					break;
				}
			}
		}
		if(ControlRig == nullptr)
		{
			return;
		}
		const TMap<FString, FString>& LibraryNameMap = ControlRig->GetShapeLibraryNameMap();
		TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries = ControlRig->GetShapeLibraries();
		for(const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary : ShapeLibraries)
		{
			if(ShapeLibrary.IsNull() || !ShapeLibrary.IsValid())
			{
				(void)ShapeLibrary.LoadSynchronous();
			}
			if(ShapeLibrary.IsNull() || !ShapeLibrary.IsValid())
			{
				continue;
			}
			const bool bUseNameSpace = ShapeLibraries.Num() > 1;
			FString LibraryName = ShapeLibrary->GetName();
			if(const FString* RemappedName = LibraryNameMap.Find(LibraryName))
			{
				LibraryName = *RemappedName;
			}
			
			const FString NameSpace = bUseNameSpace ? LibraryName + TEXT(".") : FString();
			ShapeNameList.Add(MakeShared<FRigVMStringWithTag>(UControlRigShapeLibrary::GetShapeName(ShapeLibrary.Get(), bUseNameSpace, LibraryNameMap, ShapeLibrary->DefaultShape)));
			for (const FControlRigShapeDefinition& Shape : ShapeLibrary->Shapes)
			{
				ShapeNameList.Add(MakeShared<FRigVMStringWithTag>(UControlRigShapeLibrary::GetShapeName(ShapeLibrary.Get(), bUseNameSpace, LibraryNameMap, Shape)));
			}
		}
		if(ShapeNameList.IsEmpty())
		{
			return;
		}
		StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRigUnit_HierarchyAddControl_ShapeSettings, Color)).ToSharedRef());
		Property = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRigUnit_HierarchyAddControl_ShapeSettings, Name));
		if(Property.IsValid())
		{
			TSharedPtr<FRigVMStringWithTag> InitialSelected;
			const FString CurrentShapeName = GetShapeNameListText().ToString();
			for (TSharedPtr<FRigVMStringWithTag> Item : ShapeNameList)
			{
				if (Item->Equals(CurrentShapeName))
				{
					InitialSelected = Item;
				}
			}
			IDetailPropertyRow& Row = StructBuilder.AddProperty(Property.ToSharedRef());
			constexpr bool bShowChildren = true;
			Row.CustomWidget(bShowChildren)
			.NameContent()
			[
				Property->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SAssignNew(ShapeNameListWidget, SRigVMGraphPinNameListValueWidget)
				.OptionsSource(&ShapeNameList)
				.OnGenerateWidget(this, &FAnimDetailsOverrideDetails::MakeShapeNameListItemWidget)
				.OnSelectionChanged(this, &FAnimDetailsOverrideDetails::OnShapeNameListChanged)
				.OnComboBoxOpening(this, &FAnimDetailsOverrideDetails::OnShapeNameListComboBox)
				.InitiallySelectedItem(InitialSelected)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &FAnimDetailsOverrideDetails::GetShapeNameListText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
		}
		
		StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRigUnit_HierarchyAddControl_ShapeSettings, bVisible)).ToSharedRef());
		if(TSharedPtr<IPropertyHandle> TransformHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRigUnit_HierarchyAddControl_ShapeSettings, Transform)))
		{
			StructBuilder.AddProperty(TransformHandle->GetChildHandle(TEXT("Translation")).ToSharedRef());
			StructBuilder.AddProperty(TransformHandle->GetChildHandle(TEXT("Rotation")).ToSharedRef());
			StructBuilder.AddProperty(TransformHandle->GetChildHandle(TEXT("Scale3D")).ToSharedRef());
		}
	}
	
	TSharedRef<SWidget> FAnimDetailsOverrideDetails::MakeShapeNameListItemWidget(TSharedPtr<FRigVMStringWithTag> InItem)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(InItem->GetStringWithTag()))
			.Font(IDetailLayoutBuilder::GetDetailFont());
	}
	
	void FAnimDetailsOverrideDetails::OnShapeNameListChanged(TSharedPtr<FRigVMStringWithTag> NewSelection, ESelectInfo::Type SelectInfo)
	{
		if (SelectInfo == ESelectInfo::Direct)
		{
			return;
		}
		
		const FString& NewShapeNameString = NewSelection->GetString();
		if(Property)
		{
			const FName ShapeName = NewShapeNameString.IsEmpty() ? FName(NAME_None) : FName(*NewShapeNameString);
			Property->SetValue(ShapeName);
		}
	}
	
	void FAnimDetailsOverrideDetails::OnShapeNameListComboBox()
	{
		const FString ShapeNameListText = GetShapeNameListText().ToString();
		const TSharedPtr<FRigVMStringWithTag>* CurrentlySelectedItem =
			ShapeNameList.FindByPredicate([ShapeNameListText](const TSharedPtr<FRigVMStringWithTag>& InItem)
			{
				return ShapeNameListText == InItem->GetString();
			});
		
		if(CurrentlySelectedItem)
		{
			ShapeNameListWidget->SetSelectedItem(*CurrentlySelectedItem);
		}
	}
	
	FText FAnimDetailsOverrideDetails::GetShapeNameListText() const
	{
		if(!Property)
		{
			return FText();
		}
		
		TOptional<FString> SharedValue;
		for(int32 Index = 0; Index < Property->GetNumPerObjectValues(); Index++)
		{
			FString SingleValue;
			if(!Property->GetPerObjectValue(Index, SingleValue))
			{
				SharedValue.Reset();
				break;
			}
			if(!SharedValue.IsSet())
			{
				SharedValue = SingleValue;
			}
			else if(SharedValue.GetValue() != SingleValue)
			{
				SharedValue.Reset();
				break;
			}
		}
		
		if(SharedValue.IsSet())
		{
			return FText::FromString(SharedValue.GetValue());
		}
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
}

#undef LOCTEXT_NAMESPACE

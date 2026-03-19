// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextCallFunctionSharedDataDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "UncookedOnlyUtils.h"
#include "Traits/CallFunction.h"
#include "Common/SRigVMFunctionPicker.h"
#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "IWorkspaceEditor.h"
#include "IWorkspaceEditorModule.h"
#include "ScopedTransaction.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "Modules/ModuleManager.h"
#include "RigVMModel/RigVMController.h"

#define LOCTEXT_NAMESPACE "FCallFunctionSharedDataDetails"

namespace UE::AnimNext::Editor
{

void FCallFunctionSharedDataDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	InHeaderRow
	.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		InPropertyHandle->CreatePropertyValueWidget()
	];
}

void FCallFunctionSharedDataDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	FunctionPropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAnimNextCallFunctionSharedData, Function));
	IDetailPropertyRow& PropertyRow = InChildBuilder.AddProperty(FunctionPropertyHandle.ToSharedRef());

	TSharedPtr<IPropertyHandle> CallSitePropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAnimNextCallFunctionSharedData, CallSite));
	InChildBuilder.AddProperty(CallSitePropertyHandle.ToSharedRef());

	FunctionHeaderPropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAnimNextCallFunctionSharedData, FunctionHeader));
	InChildBuilder.AddProperty(FunctionHeaderPropertyHandle.ToSharedRef())
	.Visibility(EVisibility::Collapsed);

	FunctionEventPropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAnimNextCallFunctionSharedData, FunctionEvent));
	InChildBuilder.AddProperty(FunctionEventPropertyHandle.ToSharedRef())
	.Visibility(EVisibility::Collapsed);

	TArray<TWeakObjectPtr<UObject>> Objects = InCustomizationUtils.GetPropertyUtilities()->GetSelectedObjects();
	URigVMEdGraphNode* EdGraphNode = Objects.Num() ? Cast<URigVMEdGraphNode>(Objects[0].Get()) : nullptr;
	UAnimNextRigVMAsset* CurrentAsset = nullptr;
	URigVMController* Controller = nullptr;
	if(EdGraphNode != nullptr)
	{
		CurrentAsset = EdGraphNode->GetTypedOuter<UAnimNextRigVMAsset>();
		Controller = EdGraphNode->GetController();
	}

	auto OnFunctionPicked = [this, WeakController = TWeakObjectPtr<URigVMController>(Controller)](const FRigVMGraphFunctionHeader& InFunctionHeader)
	{
		URigVMController* Controller = WeakController.Get();
		if(Controller == nullptr)
		{
			return;
		}

		FRigVMControllerCompileBracketScope CompileScope(Controller);
		FScopedTransaction Transaction(LOCTEXT("SetFunctionTransaction", "Set Function"));

		// Update function event & header from function name
		FunctionPropertyHandle->SetValue(InFunctionHeader.IsValid() ? InFunctionHeader.Name : NAME_None);
		FunctionEventPropertyHandle->SetValue(InFunctionHeader.IsValid() ? FName(*UncookedOnly::FUtils::MakeFunctionWrapperEventName(InFunctionHeader.Name)): NAME_None);

		FunctionHeaderPropertyHandle->NotifyPreChange();
		FunctionHeaderPropertyHandle->EnumerateRawData([&InFunctionHeader](void* InRawData, const int32 InDataIndex, const int32 InNumDatas)
		{
			FRigVMGraphFunctionHeader* FunctionHeader = static_cast<FRigVMGraphFunctionHeader*>(InRawData);
			if(InFunctionHeader.IsValid())
			{
				*FunctionHeader = InFunctionHeader;
			}
			else
			{
				*FunctionHeader = FRigVMGraphFunctionHeader();
			}
			return true;
		});
		FunctionHeaderPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		FunctionHeaderPropertyHandle->NotifyFinishedChangingProperties();
	};

	PropertyRow.CustomWidget()
	.NameContent()
	[
		FunctionPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SRigVMFunctionPicker)
		.CurrentAsset(FAssetData(CurrentAsset))
		.FunctionName_Lambda([this]()
		{
			bool bMultipleValues = false;
			TOptional<FRigVMGraphFunctionHeader> Header;
			PropertyHandle->EnumerateConstRawData([&Header, &bMultipleValues](const void* InRawData, const int32 InDataIndex, const int32 InNumDatas)
			{
				const FAnimNextCallFunctionSharedData* SharedData = static_cast<const FAnimNextCallFunctionSharedData*>(InRawData);
				if(!Header.IsSet())
				{
					Header = SharedData->FunctionHeader;
				}
				else if(Header.GetValue() != SharedData->FunctionHeader)
				{
					Header = FRigVMGraphFunctionHeader();
					bMultipleValues = true;
					return false;
				}
				return true;
			});

			if(bMultipleValues)
			{
				return LOCTEXT("MultipleValuesLabel", "Multiple Values");
			}
			else if(Header.IsSet() && Header.GetValue().IsValid())
			{
				return FText::FromName(Header.GetValue().Name);
			}
			return LOCTEXT("NoFunctionSelectedLabel", "None");
		})
		.FunctionToolTip_Lambda([this]()
		{
			bool bMultipleValues = false;
			TOptional<FRigVMGraphFunctionHeader> Header;
			PropertyHandle->EnumerateConstRawData([&Header, &bMultipleValues](const void* InRawData, const int32 InDataIndex, const int32 InNumDatas)
			{
				const FAnimNextCallFunctionSharedData* SharedData = static_cast<const FAnimNextCallFunctionSharedData*>(InRawData);
				if(!Header.IsSet())
				{
					Header = SharedData->FunctionHeader;
				}
				else if(Header.GetValue() != SharedData->FunctionHeader)
				{
					Header = FRigVMGraphFunctionHeader();
					bMultipleValues = true;
					return false;
				}
				return true;
			});

			if(bMultipleValues)
			{
				return LOCTEXT("MultipleValuesLabel", "Multiple Values");
			}
			else if(Header.IsSet() && Header.GetValue().IsValid())
			{
				static const FTextFormat ToolTipFormat(LOCTEXT("FunctionToolTipFormat", "{0}\n{1}"));
				
				return Header.GetValue().Description.Len() > 0 ?
					FText::FromString(Header.GetValue().Description) :
					FText::Format(ToolTipFormat, FText::FromString(Header.GetValue().LibraryPointer.GetFunctionName()), FText::FromString(Header.GetValue().LibraryPointer.GetLibraryNodePath()));
			}
			return LOCTEXT("NoFunctionSelectedLabel", "None");
		})
		.OnRigVMFunctionPicked_Lambda(OnFunctionPicked)
		.OnNewFunction_Lambda([this, CurrentAsset, WeakController = TWeakObjectPtr<URigVMController>(Controller), OnFunctionPicked]()
		{
			URigVMController* Controller = WeakController.Get();
			if(Controller == nullptr)
			{
				return;
			}

			UAnimNextRigVMAssetEditorData* EditorData = UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(CurrentAsset);
			URigVMLibraryNode* NewFunction = nullptr;
			{
				FRigVMControllerCompileBracketScope CompileScope(Controller);
				FScopedTransaction Transaction(LOCTEXT("AddFunctionTransaction", "Add Function"));
				NewFunction = EditorData->AddFunction(TEXT("NewFunction"), true, true, true);

				if(NewFunction == nullptr)
				{
					return;
				}

				const FRigVMGraphFunctionData* FunctionData = EditorData->GraphFunctionStore.FindFunction(NewFunction->GetFunctionIdentifier());
				if(FunctionData == nullptr)
				{
					return;
				}

				// Set our function
				OnFunctionPicked(FunctionData->Header);
			}

			// Open the new function's graph
			UE::Workspace::IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<UE::Workspace::IWorkspaceEditorModule>("WorkspaceEditor");
			if(UE::Workspace::IWorkspaceEditor* WorkspaceEditor = WorkspaceEditorModule.OpenWorkspaceForObject(CurrentAsset, UE::Workspace::EOpenWorkspaceMethod::Default))
			{
				UObject* EditorObject = EditorData->GetEditorObjectForRigVMGraph(NewFunction->GetContainedGraph());
				WorkspaceEditor->OpenObjects({EditorObject});
			}
		})
	];
}

}

#undef LOCTEXT_NAMESPACE
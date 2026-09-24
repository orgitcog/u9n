// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigDragOps.h"
#include "Editor/SRigHierarchyTagWidget.h"

//////////////////////////////////////////////////////////////
/// FRigElementHierarchyDragDropOp
///////////////////////////////////////////////////////////

TSharedRef<FRigElementHierarchyDragDropOp> FRigElementHierarchyDragDropOp::New(const TArray<FRigHierarchyKey>& InElements)
{
	TSharedRef<FRigElementHierarchyDragDropOp> Operation = MakeShared<FRigElementHierarchyDragDropOp>();
	Operation->Elements = InElements;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FRigElementHierarchyDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedElementNames()))
			//.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
		];
}

FString FRigElementHierarchyDragDropOp::GetJoinedElementNames() const
{
	TArray<FString> ElementNameStrings;
	for (const FRigHierarchyKey& Element: Elements)
	{
		ElementNameStrings.Add(Element.GetName());
	}
	return FString::Join(ElementNameStrings, TEXT(","));
}

bool FRigElementHierarchyDragDropOp::IsDraggingSingleConnector() const
{
	if(Elements.Num() == 1)
	{
		if(Elements[0].IsElement())
		{
			return Elements[0].GetElement().Type == ERigElementType::Connector;
		}
	}
	return false;
}

bool FRigElementHierarchyDragDropOp::IsDraggingSingleSocket() const
{
	if(Elements.Num() == 1)
	{
		if(Elements[0].IsElement())
		{
			return Elements[0].GetElement().Type == ERigElementType::Socket;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////
/// FRigHierarchyTagDragDropOp
///////////////////////////////////////////////////////////

TSharedRef<FRigHierarchyTagDragDropOp> FRigHierarchyTagDragDropOp::New(TSharedPtr<SRigHierarchyTagWidget> InTagWidget)
{
	TSharedRef<FRigHierarchyTagDragDropOp> Operation = MakeShared<FRigHierarchyTagDragDropOp>();
	Operation->Text = InTagWidget->Text.Get();
	Operation->Identifier = InTagWidget->Identifier.Get();
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FRigHierarchyTagDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(Text)
		];
}

//////////////////////////////////////////////////////////////
/// FModularRigModuleDragDropOp
///////////////////////////////////////////////////////////

TSharedRef<FModularRigModuleDragDropOp> FModularRigModuleDragDropOp::New(const TArray<FName>& InModuleNames)
{
	TSharedRef<FModularRigModuleDragDropOp> Operation = MakeShared<FModularRigModuleDragDropOp>();
	Operation->ModuleNames = InModuleNames;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FModularRigModuleDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedModuleNames()))
			//.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
		];
}

FString FModularRigModuleDragDropOp::GetJoinedModuleNames() const
{
	TArray<FString> ElementNameStrings;
	for (const FName& ModuleName: ModuleNames)
	{
		ElementNameStrings.Add(ModuleName.ToString());
	}
	return FString::Join(ElementNameStrings, TEXT(","));
}

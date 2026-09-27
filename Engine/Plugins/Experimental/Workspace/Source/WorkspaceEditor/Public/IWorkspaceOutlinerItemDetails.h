// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkspaceAssetRegistryInfo.h"
#include "Styling/SlateColor.h"

class UPackage;
struct FToolMenuContext;
struct FSlateBrush;

namespace UE::Workspace
{
	constexpr bool bShowFullIdentifierInOutliner = false;	

typedef FName FOutlinerItemDetailsId;
static FOutlinerItemDetailsId MakeOutlinerDetailsId(const FWorkspaceOutlinerItemExport& InExport)
{
    return InExport.HasData() ? InExport.GetData().GetScriptStruct()->GetFName() : NAME_None;
}

class IWorkspaceOutlinerItemDetails : public TSharedFromThis<IWorkspaceOutlinerItemDetails>
{
public:
    virtual ~IWorkspaceOutlinerItemDetails() = default;
	virtual FString GetDisplayString(const FWorkspaceOutlinerItemExport& Export) const
	{			
		return bShowFullIdentifierInOutliner ? FString::Printf(TEXT("%s - %s - %s"),
		*Export.GetIdentifier().ToString(), *(Export.HasData() ? Export.GetData().GetScriptStruct()->GetFName() : NAME_None).ToString(), *Export.GetFirstAssetPath().ToString()) : Export.GetIdentifier().ToString();
	}
	
    virtual const FSlateBrush* GetItemIcon(const FWorkspaceOutlinerItemExport& Export) const { return nullptr; }
	virtual FSlateColor GetItemColor(const FWorkspaceOutlinerItemExport& Export) const { return FSlateColor::UseForeground(); }
    virtual bool HandleDoubleClick(const FToolMenuContext& ToolMenuContext) const { return false; }
	virtual bool CanDelete(const FWorkspaceOutlinerItemExport& Export) const { return true; }
	virtual void Delete(TConstArrayView<FWorkspaceOutlinerItemExport> Exports) const {}
    virtual bool CanRename(const FWorkspaceOutlinerItemExport& Export) const { return false; }
    virtual void Rename(const FWorkspaceOutlinerItemExport& Export, const FText& InName) const {}
    virtual bool ValidateName(const FWorkspaceOutlinerItemExport& Export, const FText& InName, FText& OutErrorMessage) const { return false; }
    virtual UPackage* GetPackage(const FWorkspaceOutlinerItemExport& Export) const { return nullptr; }
	virtual bool HandleSelected(const FToolMenuContext& ToolMenuContext) const { return false; }
	virtual bool IsExpandedByDefault() const { return true; }
};

}

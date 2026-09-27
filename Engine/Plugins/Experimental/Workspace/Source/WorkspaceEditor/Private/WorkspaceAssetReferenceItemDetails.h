// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IWorkspaceOutlinerItemDetails.h"
#include "WorkspaceEditorModule.h"
#include "Styling/AppStyle.h"

class UAnimNextRigVMAssetEditorData;
enum class EAnimNextEditorDataNotifType : uint8;

namespace UE::Workspace
{
class FWorkspaceAssetReferenceOutlinerItemDetails : public IWorkspaceOutlinerItemDetails
{
public:
	virtual ~FWorkspaceAssetReferenceOutlinerItemDetails() override = default;

	static TSharedPtr<IWorkspaceOutlinerItemDetails> GetInnerDetails(const FWorkspaceOutlinerItemExport& Export)
	{
		TSharedPtr<IWorkspaceOutlinerItemDetails> SharedFactory = nullptr;
		const FWorkspaceOutlinerAssetReferenceItemData& Data = Export.GetData().Get<FWorkspaceOutlinerAssetReferenceItemData>();
		if(Data.ReferredExport.HasData())
		{
			const FOutlinerItemDetailsId Id = Data.ReferredExport.GetData().GetScriptStruct()->GetFName();		
			SharedFactory = FWorkspaceEditorModule::GetOutlinerItemDetails(Id);
		}

		return SharedFactory;
	}

	static const FWorkspaceOutlinerItemExport& GetInnerExport(const FWorkspaceOutlinerItemExport& Export)
	{
		FWorkspaceOutlinerItemExport InnerExport = Export;
		check(Export.HasData());
		const FWorkspaceOutlinerAssetReferenceItemData& Data = Export.GetData().Get<FWorkspaceOutlinerAssetReferenceItemData>();
		return Data.ReferredExport;
	}

	virtual FString GetDisplayString(const FWorkspaceOutlinerItemExport& Export) const override
	{
		if (const TSharedPtr<IWorkspaceOutlinerItemDetails> SharedFactory = GetInnerDetails(Export))
		{
			return GetInnerDetails(Export)->GetDisplayString(GetInnerExport(Export));
		}

		return bShowFullIdentifierInOutliner ? FString::Printf(TEXT("%s - %s - %s"), *Export.GetIdentifier().ToString(), *(Export.GetData().GetScriptStruct() ? Export.GetData().GetScriptStruct()->GetFName() : NAME_None).ToString(), *Export.GetFirstAssetPath().ToString()) :
			Export.GetFirstAssetPath().GetAssetName();
	}
	
	virtual const FSlateBrush* GetItemIcon(const FWorkspaceOutlinerItemExport& Export) const override
	{
		const FSlateBrush* Icon = nullptr;
		{
			if (const TSharedPtr<IWorkspaceOutlinerItemDetails> SharedFactory = GetInnerDetails(Export))
			{
				const FWorkspaceOutlinerItemExport InnerExport = GetInnerExport(Export);
				Icon = SharedFactory->GetItemIcon(InnerExport);
			}
		}
				
		return Icon ? Icon : FAppStyle::GetBrush(TEXT("GenericLink"));
	}

	virtual FSlateColor GetItemColor(const FWorkspaceOutlinerItemExport& Export) const override
	{
		const FWorkspaceOutlinerAssetReferenceItemData& Data = Export.GetData().Get<FWorkspaceOutlinerAssetReferenceItemData>();	
		return Data.bRecursiveReference ? FLinearColor::Red : FSlateColor::UseSubduedForeground();
	}	

	virtual bool IsExpandedByDefault() const override
	{
		return false;
	}	
};

}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAssetWorkspaceAssetUserData.h"

#include "UncookedOnlyUtils.h"
#include "DataInterface/AnimNextDataInterface.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_EditorData.h"
#include "UObject/AssetRegistryTagsContext.h"

void UAnimNextAssetWorkspaceAssetUserData::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	// Updated cached export data outside of saving call-path, and simply return cached data when actually saving 
	if (!Context.IsSaving())
	{
		CachedExports.Exports.Reset();
		{
			UAnimNextRigVMAsset* Asset = CastChecked<UAnimNextRigVMAsset>(GetOuter());
			const UAnimNextRigVMAssetEditorData* GraphEditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(Asset);
			{
				FWorkspaceOutlinerItemExport& RootAssetExport = CachedExports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(Asset->GetFName(), Asset));

				if(UAnimNextModule* Module = Cast<UAnimNextModule>(Asset))
				{
					RootAssetExport.GetData().InitializeAsScriptStruct(FAnimNextModuleOutlinerData::StaticStruct());
				}
				else if(UAnimNextDataInterface* DataInterface = Cast<UAnimNextDataInterface>(Asset))
				{
					RootAssetExport.GetData().InitializeAsScriptStruct(FAnimNextDataInterfaceOutlinerData::StaticStruct());
				}
				else
				{
					RootAssetExport.GetData().InitializeAsScriptStruct(FAnimNextRigVMAssetOutlinerData::StaticStruct());
				}
				FAnimNextRigVMAssetOutlinerData& Data = RootAssetExport.GetData().GetMutable<FAnimNextRigVMAssetOutlinerData>();
				Data.SoftAssetPtr = Asset;
			}
		
			UE::AnimNext::UncookedOnly::FUtils::GetAssetOutlinerItems(GraphEditorData, CachedExports, Context);
		}
	}
	
	FString TagValue;
	FWorkspaceOutlinerItemExports::StaticStruct()->ExportText(TagValue, &CachedExports, nullptr, nullptr, PPF_None, nullptr);
	Context.AddTag(FAssetRegistryTag(UE::Workspace::ExportsWorkspaceItemsRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}


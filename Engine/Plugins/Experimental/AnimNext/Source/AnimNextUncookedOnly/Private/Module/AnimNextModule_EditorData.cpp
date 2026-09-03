// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModule_EditorData.h"

#include "Compilation/AnimNextGetGraphCompileContext.h"
#include "Compilation/AnimNextProcessGraphCompileContext.h"
#include "ExternalPackageHelper.h"
#include "UncookedOnlyUtils.h"
#include "Module/AnimNextModule.h"
#include "Entries/AnimNextEventGraphEntry.h"
#include "Entries/AnimNextDataInterfaceEntry.h"
#include "Entries/AnimNextVariableEntry.h"
#include "String/ParseTokens.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/LinkerLoad.h"
#include "Variables/AnimNextUniversalObjectLocatorBindingData.h"

void UAnimNextModule_EditorData::RecompileVM()
{
	Super::RecompileVM();

	if (bIsCompiling)
	{
		return;
	}

#if WITH_EDITOR
	if(!bSuspendCompilationNotifications)
	{
		UAnimNextModule::OnModuleCompiled().Broadcast(UE::AnimNext::UncookedOnly::FUtils::GetAsset<UAnimNextModule>(this));
	}
#endif
}

TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> UAnimNextModule_EditorData::GetEntryClasses() const
{
	static const TSubclassOf<UAnimNextRigVMAssetEntry> Classes[] =
	{
		UAnimNextEventGraphEntry::StaticClass(),
		UAnimNextVariableEntry::StaticClass(),
		UAnimNextDataInterfaceEntry::StaticClass(),
	};
	
	return Classes;
}

void UAnimNextModule_EditorData::OnPreCompileAsset(FRigVMCompileSettings& InSettings)
{
	using namespace UE::AnimNext::UncookedOnly;

	UAnimNextModule* Module = FUtils::GetAsset<UAnimNextModule>(this);

	Module->RequiredComponents.Empty();
	Module->Dependencies.Empty();
}

void UAnimNextModule_EditorData::OnPreCompileGetProgrammaticGraphs(const FRigVMCompileSettings& InSettings, FAnimNextGetGraphCompileContext& OutCompileContext)
{
	using namespace UE::AnimNext::UncookedOnly;

	FUtils::CompileVariableBindings(InSettings, FUtils::GetAsset<UAnimNextModule>(this), OutCompileContext.GetMutableProgrammaticGraphs());
}

void UAnimNextModule_EditorData::OnPreCompileProcessGraphs(const FRigVMCompileSettings& InSettings, FAnimNextProcessGraphCompileContext& OutCompileContext)
{
	using namespace UE::AnimNext::UncookedOnly;

	UAnimNextModule* Module = FUtils::GetAsset<UAnimNextModule>(this);

	// Gather any required components from node metadata
	for(URigVMGraph* Graph : OutCompileContext.GetMutableAllGraphs())
	{
		for(URigVMNode* Node : Graph->GetNodes())
		{
			URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(Node);
			if(TemplateNode == nullptr)
			{
				continue;
			}

			UScriptStruct* Struct = TemplateNode->GetScriptStruct();
			if(Struct == nullptr)
			{
				continue;
			}

			static FName Metadata_RequiredComponents("RequiredComponents");
			FString ComponentsString = Struct->GetMetaData(Metadata_RequiredComponents);
			if(ComponentsString.Len() == 0)
			{
				continue;
			}

			UE::String::ParseTokens(ComponentsString, TEXT(','), [Module](FStringView InToken)
			{
				FString StructName(InToken);
				UScriptStruct* ComponentStruct = FindFirstObject<UScriptStruct>(*StructName);
				if(ComponentStruct)
				{
					Module->RequiredComponents.Add(ComponentStruct);
				}
			});
		}
	}

	// Copy dependencies
	Module->Dependencies = Dependencies;
}

void UAnimNextModule_EditorData::CustomizeNewAssetEntry(UAnimNextRigVMAssetEntry* InNewEntry) const
{
	Super::CustomizeNewAssetEntry(InNewEntry);
	
	UAnimNextVariableEntry* VariableEntry = Cast<UAnimNextVariableEntry>(InNewEntry);
	if(VariableEntry == nullptr)
	{
		return;
	}

	VariableEntry->SetBindingType(FAnimNextUniversalObjectLocatorBindingData::StaticStruct(), false);
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "UncookedOnlyUtils.h"

#include "K2Node_CallFunction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Compilation/AnimNextGetVariableCompileContext.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_EditorData.h"
#include "Serialization/MemoryReader.h"
#include "RigVMCore/RigVM.h"
#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "Entries/AnimNextRigVMAssetEntry.h"
#include "AnimNextUncookedOnlyModule.h"
#include "IAnimNextRigVMExportInterface.h"
#include "Logging/StructuredLog.h"
#include "AnimNextAssetWorkspaceAssetUserData.h"
#include "IWorkspaceEditor.h"
#include "IWorkspaceEditorModule.h"
#include "Misc/EnumerateRange.h"
#include "Module/RigUnit_AnimNextModuleEvents.h"
#include "Variables/AnimNextProgrammaticVariable.h"
#include "Variables/IVariableBindingType.h"
#include "Variables/RigUnit_CopyModuleProxyVariables.h"
#include "WorkspaceAssetRegistryInfo.h"
#include "Entries/AnimNextDataInterfaceEntry.h"
#include "DataInterface/AnimNextDataInterface.h"
#include "DataInterface/AnimNextDataInterface_EditorData.h"
#include "Logging/MessageLog.h"
#include "RigVMFunctions/Execution/RigVMFunction_UserDefinedEvent.h"
#include "UObject/AssetRegistryTagsContext.h"

#define LOCTEXT_NAMESPACE "AnimNextUncookedOnlyUtils"

namespace UE::AnimNext::UncookedOnly
{

TAutoConsoleVariable<bool> CVarDumpProgrammaticGraphs(
	TEXT("AnimNext.DumpProgrammaticGraphs"),
	false,
	TEXT("When true the transient programmatic graphs will be automatically opened for any that are generated."));

void FUtils::RecreateVM(UAnimNextRigVMAsset* InAsset)
{
	if (InAsset->VM == nullptr)
	{
		InAsset->VM = NewObject<URigVM>(InAsset, TEXT("VM"), RF_NoFlags);
	}
	InAsset->VM->Reset(InAsset->ExtendedExecuteContext);
	InAsset->RigVM = InAsset->VM; // Local serialization
}


void FUtils::CompileVariables(const FRigVMCompileSettings& InSettings, UAnimNextRigVMAsset* InAsset, FAnimNextGetVariableCompileContext& OutCompileContext)
{
	check(InAsset);

	UAnimNextDataInterface* DataInterface = Cast<UAnimNextDataInterface>(InAsset);
	if(DataInterface == nullptr)
	{
		// Currently only support data interface types (TODO: could make UAnimNextDataInterface the common base rather than UAnimNextRigVMAsset)
		return;
	}

	FMessageLog Log("AnimNextCompilerResults");

	UAnimNextDataInterface_EditorData* EditorData = GetEditorData<UAnimNextDataInterface_EditorData>(DataInterface);

	// Gather programmatic variables regenerated each compile
	EditorData->OnPreCompileGetProgrammaticVariables(InSettings, OutCompileContext);
	const TArray<FAnimNextProgrammaticVariable>& ProgrammaticVariables = OutCompileContext.GetProgrammaticVariables();

	struct FStructEntryInfo
	{
		const UAnimNextDataInterface* FromDataInterface = nullptr;
		const UScriptStruct* NativeInterface = nullptr;
		FName Name;
		FAnimNextParamType Type;
		EAnimNextExportAccessSpecifier AccessSpecifier = EAnimNextExportAccessSpecifier::Private;
		bool bAutoBindDataInterfaceToHost = false;
		TConstArrayView<uint8> Value;
		EPropertyFlags PropertyFlags = CPF_Edit;
	};

	// Gather all variables in this asset.
	// Variables are harvested from the valid entries and data interfaces.
	// Data interface harvesting is performed recursively
	// The topmost value for a data interface 'wins' if a value is to be supplied
	TMap<FName, int32> EntryInfoIndexMap;
	TArray<FStructEntryInfo> StructEntryInfos;
	StructEntryInfos.Reserve(EditorData->Entries.Num() + ProgrammaticVariables.Num());
	int32 NumPublicVariables = 0;

	auto AddVariable = [&Log, &NumPublicVariables, &StructEntryInfos, &EntryInfoIndexMap](const UAnimNextVariableEntry* InVariable, const UAnimNextDataInterfaceEntry* InFromInterfaceEntry, const UAnimNextDataInterface* InFromInterface, bool bInAutoBindInterface)
	{
		const FName Name = InVariable->GetExportName();
		const FAnimNextParamType& Type = InVariable->GetExportType();
		if(!Type.IsValid())
		{
			Log.Error(FText::Format(LOCTEXT("InvalidVariableTypeFound", "Variable '{0}' with invalid type found"), FText::FromName(Name)));
			return;
		}

		const EAnimNextExportAccessSpecifier AccessSpecifier = InVariable->GetExportAccessSpecifier();

		// Check for type conflicts
		int32* ExistingIndexPtr = EntryInfoIndexMap.Find(Name);
		if(ExistingIndexPtr)
		{
			const FStructEntryInfo& ExistingInfo = StructEntryInfos[*ExistingIndexPtr];
			if(ExistingInfo.Type != Type)
			{
				Log.Error(FText::Format(LOCTEXT("ConflictingVariableTypeFound", "Variable '{0}' with conflicting type found ({1} vs {2})"), FText::FromName(Name), FText::FromString(ExistingInfo.Type.ToString()), FText::FromString(Type.ToString())));
				return;
			}

			if(ExistingInfo.AccessSpecifier != AccessSpecifier)
			{
				Log.Error(FText::Format(LOCTEXT("ConflictingVariableAccessFound", "Variable '{0}' with conflicting access specifier found ({1} vs {2})"), FText::FromName(Name), FText::FromString(UEnum::GetValueAsString(ExistingInfo.AccessSpecifier)), FText::FromString(UEnum::GetValueAsString(AccessSpecifier))));
				return;
			}
		}
		else
		{
			if(AccessSpecifier == EAnimNextExportAccessSpecifier::Public)
			{
				NumPublicVariables++;
			}
		}

		// Check the overrides to see if this variable's default is overriden
		const FProperty* OverrideProperty = nullptr;
		TConstArrayView<uint8> OverrideValue;
		if(InFromInterfaceEntry != nullptr)
		{
			InFromInterfaceEntry->FindValueOverrideRecursive(Name, OverrideProperty, OverrideValue);
		}

		TConstArrayView<uint8> Value;
		if(!OverrideValue.IsEmpty())
		{
			Value = OverrideValue;
		}
		else
		{
			Value = TConstArrayView<uint8>(InVariable->GetValuePtr(), Type.GetSize());
		}

		if(ExistingIndexPtr)
		{
			// Found a variable of the same name/type, overwrite its value
			StructEntryInfos[*ExistingIndexPtr].Value = Value;
		}
		else
		{
			// This is a new variable, check if it belongs to a native interface
			const UAnimNextDataInterface_EditorData* FromInterfaceEditorData = GetEditorData<const UAnimNextDataInterface_EditorData>(InFromInterface);
			check(FromInterfaceEditorData != nullptr);

			bool bFound = false;
			for (const UScriptStruct* NativeInterfaceStruct : FromInterfaceEditorData->NativeInterfaces)
			{
				if (NativeInterfaceStruct != nullptr && NativeInterfaceStruct->FindPropertyByName(Name) != nullptr)
				{
					// Found it
					int32 Index = StructEntryInfos.Add(
						{
							InFromInterface,
							NativeInterfaceStruct,
							Name,
							FAnimNextParamType(Type.GetValueType(), Type.GetContainerType(), Type.GetValueTypeObject()),
							AccessSpecifier,
							bInAutoBindInterface,
							Value,
							CPF_Edit
						});

					EntryInfoIndexMap.Add(Name, Index);
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				// Legacy codepath
				int32 Index = StructEntryInfos.Add(
					{
						InFromInterface,
						nullptr,
						Name,
						FAnimNextParamType(Type.GetValueType(), Type.GetContainerType(), Type.GetValueTypeObject()),
						AccessSpecifier,
						bInAutoBindInterface,
						Value,
						CPF_Edit
					});

				EntryInfoIndexMap.Add(Name, Index);
			}
		}
	};

	auto AddDataInterface = [&Log, &AddVariable, &DataInterface](const UAnimNextDataInterface* InDataInterface, const UAnimNextDataInterfaceEntry* InDataInterfaceEntry, bool bInPublicOnly, bool bInAutoBindInterface, auto& InAddDataInterface) -> void
	{
		const UAnimNextDataInterface_EditorData* DataInterfaceEditorData = GetEditorData<const UAnimNextDataInterface_EditorData>(InDataInterface);
		check(DataInterfaceEditorData != nullptr);

		for(UAnimNextRigVMAssetEntry* OtherEntry : DataInterfaceEditorData->Entries)
		{
			if(const UAnimNextVariableEntry* VariableEntry = Cast<UAnimNextVariableEntry>(OtherEntry))
			{
				if(!bInPublicOnly || VariableEntry->GetExportAccessSpecifier() == EAnimNextExportAccessSpecifier::Public)
				{
					AddVariable(VariableEntry, InDataInterfaceEntry, InDataInterface, bInAutoBindInterface);
				}
			}
			else if(const UAnimNextDataInterfaceEntry* DataInterfaceEntry = Cast<UAnimNextDataInterfaceEntry>(OtherEntry))
			{
				UAnimNextDataInterface* SubDataInterface = DataInterfaceEntry->GetDataInterface();
				if(SubDataInterface == nullptr)
				{
					Log.Error(FText::Format(LOCTEXT("MissingDataInterfaceWarning", "Invalid data interface found: {0}"), FText::FromString(DataInterfaceEntry->GetDataInterfacePath().ToString())));
					return;
				}
				else if(DataInterface == SubDataInterface)
				{
					Log.Error(FText::Format(LOCTEXT("CircularDataInterfaceRefError", "Circular data interface reference found: {0}"), FText::FromString(DataInterfaceEntry->GetDataInterfacePath().ToString())));
					return;
				}
				else
				{
					bool bAutoBindInterface = DataInterfaceEntry->AutomaticBinding == EAnimNextDataInterfaceAutomaticBindingMode::BindSharedInterfaces;
					InAddDataInterface(SubDataInterface, DataInterfaceEntry, true, bAutoBindInterface, InAddDataInterface);
				}
			}
		}
	};

	AddDataInterface(DataInterface, nullptr, false, true, AddDataInterface);

	auto AddProgrammaticVariable = [&Log, &StructEntryInfos, &EntryInfoIndexMap](const FAnimNextProgrammaticVariable& ProgrammaticVariable)
	{
		const FName Name = ProgrammaticVariable.Name;
		const FAnimNextParamType& Type = ProgrammaticVariable.Type;
		if(!Type.IsValid())
		{
			Log.Error(FText::Format(LOCTEXT("InvalidProgrammaticVariableTypeFound", "Programmatic Variable '{0}' with invalid type found"), FText::FromName(Name)));
			return;
		}

		// Check for type conflicts
		int32* ExistingIndexPtr = EntryInfoIndexMap.Find(Name);
		if(ExistingIndexPtr)
		{
			Log.Error(FText::Format(LOCTEXT("ConflictingProgrammaticVariableFound", "Programmatic Variable '{0}' already exists, should be created new each compile with no conflicts"), FText::FromName(Name)));
			return;
		}

		TConstArrayView<uint8> Value = TConstArrayView<uint8>(ProgrammaticVariable.GetValuePtr(), Type.GetSize());

		int32 Index = StructEntryInfos.Add(
		{
			nullptr,
			nullptr,
			Name,
			FAnimNextParamType(Type.GetValueType(), Type.GetContainerType(), Type.GetValueTypeObject()),
			EAnimNextExportAccessSpecifier::Private,
			false,
			Value,
			CPF_AdvancedDisplay
		});

		EntryInfoIndexMap.Add(Name, Index);
	};

	for (const FAnimNextProgrammaticVariable& ProgrammaticVariable : ProgrammaticVariables)
	{
		AddProgrammaticVariable(ProgrammaticVariable);
	}

	// Sort public entries first, then by data interface & then by size, largest first, for better packing
	static_assert(EAnimNextExportAccessSpecifier::Private < EAnimNextExportAccessSpecifier::Public, "Private must be less than Public as parameters are sorted internally according to this assumption");
	StructEntryInfos.Sort([](const FStructEntryInfo& InLHS, const FStructEntryInfo& InRHS)
	{
		if(InLHS.AccessSpecifier != InRHS.AccessSpecifier)
		{
			return InLHS.AccessSpecifier > InRHS.AccessSpecifier;
		}
		else if(InLHS.FromDataInterface != InRHS.FromDataInterface)
		{
			// If we have a null (Ex: programmatic variables), compare ptrs so that those without interfaces are last
			if (!InLHS.FromDataInterface || !InRHS.FromDataInterface)
			{
				return InLHS.FromDataInterface > InRHS.FromDataInterface;
			}

			return InLHS.FromDataInterface->GetFName().LexicalLess(InRHS.FromDataInterface->GetFName());
		}
		else if(InLHS.Type.GetSize() != InRHS.Type.GetSize())
		{
			return InLHS.Type.GetSize() > InRHS.Type.GetSize();
		}
		else
		{
			return InLHS.Name.LexicalLess(InRHS.Name);
		}
	});

	DataInterface->DefaultInjectionSiteIndex = INDEX_NONE;

	if(StructEntryInfos.Num() > 0)
	{
		// Build PropertyDescs and values to batch-create the property bag
		TArray<FPropertyBagPropertyDesc> PropertyDescs;
		PropertyDescs.Reserve(StructEntryInfos.Num());
		TArray<TConstArrayView<uint8>> Values;
		Values.Reserve(StructEntryInfos.Num());

		DataInterface->ImplementedInterfaces.Empty();

		for (TEnumerateRef<const FStructEntryInfo> StructEntryInfo : EnumerateRange(StructEntryInfos))
		{
			PropertyDescs.Emplace(StructEntryInfo->Name, StructEntryInfo->Type.ContainerType, StructEntryInfo->Type.ValueType, StructEntryInfo->Type.ValueTypeObject, StructEntryInfo->PropertyFlags);
			Values.Add(StructEntryInfo->Value);

			// Note: string comparison here because otherwise we would have a circular dependency
			if( EditorData->DefaultInjectionSite == StructEntryInfo->Name &&
				StructEntryInfo->Type.ValueTypeObject->GetPathName() == TEXT("/Script/AnimNextAnimGraph.AnimNextAnimGraph"))
			{
				DataInterface->DefaultInjectionSiteIndex = StructEntryInfo.GetIndex();
			}
			
			if(StructEntryInfo->AccessSpecifier != EAnimNextExportAccessSpecifier::Public)
			{
				continue;
			}

			// Now process any data interfaces (sets of public variables) 
			auto CheckForExistingDataInterface = [&StructEntryInfo](const FAnimNextImplementedDataInterface& InImplementedDataInterface)
			{
				return InImplementedDataInterface.DataInterface == StructEntryInfo->FromDataInterface;
			};

			FAnimNextImplementedDataInterface* ExistingImplementedDataInterface = DataInterface->ImplementedInterfaces.FindByPredicate(CheckForExistingDataInterface);
			if(ExistingImplementedDataInterface == nullptr)
			{
				FAnimNextImplementedDataInterface& NewImplementedDataInterface = DataInterface->ImplementedInterfaces.AddDefaulted_GetRef();
				NewImplementedDataInterface.DataInterface = StructEntryInfo->FromDataInterface;
				NewImplementedDataInterface.NativeInterface = StructEntryInfo->NativeInterface;
				NewImplementedDataInterface.VariableIndex = StructEntryInfo.GetIndex();
				NewImplementedDataInterface.NumVariables = 1;
				NewImplementedDataInterface.bAutoBindToHost = StructEntryInfo->bAutoBindDataInterfaceToHost; 
			}
			else
			{
				ExistingImplementedDataInterface->NumVariables++;
			}
		}

		if(EditorData->DefaultInjectionSite != NAME_None && DataInterface->DefaultInjectionSiteIndex == INDEX_NONE)
		{
			Log.Error(FText::Format(LOCTEXT("MissingDefaultInjectionSiteWarning", "Could not find default injection site: {0}"), FText::FromName(EditorData->DefaultInjectionSite)));
		}

		// Create new property bags and migrate
		EPropertyBagResult Result = DataInterface->VariableDefaults.ReplaceAllPropertiesAndValues(PropertyDescs, Values);
		check(Result == EPropertyBagResult::Success);

		if(NumPublicVariables > 0)
		{
			TConstArrayView<FPropertyBagPropertyDesc> PublicPropertyDescs(PropertyDescs.GetData(), NumPublicVariables);
			TConstArrayView<TConstArrayView<uint8>> PublicValues(Values.GetData(), NumPublicVariables);
			Result = DataInterface->PublicVariableDefaults.ReplaceAllPropertiesAndValues(PublicPropertyDescs, PublicValues);
			check(Result == EPropertyBagResult::Success);
		}
		else
		{
			DataInterface->PublicVariableDefaults.Reset();
		}

		// Rebuild external variables
		DataInterface->VM->SetExternalVariableDefs(DataInterface->GetExternalVariablesImpl(false));
	}
	else
	{
		DataInterface->ImplementedInterfaces.Empty();
		DataInterface->VariableDefaults.Reset();
		DataInterface->PublicVariableDefaults.Reset();
		DataInterface->VM->ClearExternalVariables(DataInterface->ExtendedExecuteContext);
		DataInterface->DefaultInjectionSiteIndex = INDEX_NONE;
	}


}

void FUtils::CompileVariableBindings(const FRigVMCompileSettings& InSettings, UAnimNextRigVMAsset* InAsset, TArray<URigVMGraph*>& OutGraphs)
{
	CompileVariableBindingsInternal(InSettings, InAsset, OutGraphs, true);
	CompileVariableBindingsInternal(InSettings, InAsset,  OutGraphs, false);
}

void FUtils::CompileVariableBindingsInternal(const FRigVMCompileSettings& InSettings, UAnimNextRigVMAsset* InAsset, TArray<URigVMGraph*>& OutGraphs, bool bInThreadSafe)
{
	check(InAsset);

	FModule& Module = FModuleManager::LoadModuleChecked<FModule>("AnimNextUncookedOnly");
	UAnimNextRigVMAssetEditorData* EditorData = GetEditorData(InAsset);
	TMap<IVariableBindingType*, TArray<IVariableBindingType::FBindingGraphInput>> BindingGroups;

	for(const UAnimNextRigVMAssetEntry* Entry : EditorData->Entries)
	{
		const IAnimNextRigVMVariableInterface* Variable = Cast<IAnimNextRigVMVariableInterface>(Entry);
		if(Variable == nullptr)
		{
			continue;
		}

		TConstStructView<FAnimNextVariableBindingData> Binding = Variable->GetBinding();
		if(!Binding.IsValid() || !Binding.Get<FAnimNextVariableBindingData>().IsValid())
		{
			continue;
		}

		if(Binding.Get<FAnimNextVariableBindingData>().IsThreadSafe() != bInThreadSafe)
		{
			continue;
		}

		TSharedPtr<IVariableBindingType> BindingType = Module.FindVariableBindingType(Binding.GetScriptStruct());
		if(!BindingType.IsValid())
		{
			continue;
		}

		TArray<IVariableBindingType::FBindingGraphInput>& Group = BindingGroups.FindOrAdd(BindingType.Get());
		FRigVMTemplateArgumentType RigVMArg = Variable->GetType().ToRigVMTemplateArgument();
		Group.Add({ Variable->GetVariableName(), RigVMArg.GetBaseCPPType(), RigVMArg.CPPTypeObject, Binding});
	}

	const bool bHasBindings = BindingGroups.Num() > 0;
	const bool bHasPublicVariablesToCopy = EditorData->IsA<UAnimNextModule_EditorData>() && EditorData->HasPublicVariables() && bInThreadSafe;
	if(!bHasBindings && !bHasPublicVariablesToCopy)
	{
		// Nothing to do here
		return;
	}

	URigVMGraph* BindingGraph = NewObject<URigVMGraph>(EditorData, NAME_None, RF_Transient);

	FRigVMClient* VMClient = EditorData->GetRigVMClient();
	URigVMController* Controller = VMClient->GetOrCreateController(BindingGraph);
	UScriptStruct* BindingsNodeType = bInThreadSafe ? FRigUnit_AnimNextExecuteBindings_WT::StaticStruct() : FRigUnit_AnimNextExecuteBindings_GT::StaticStruct();
	URigVMNode* ExecuteBindingsNode = Controller->AddUnitNode(BindingsNodeType, FRigVMStruct::ExecuteName, FVector2D::ZeroVector, FString(), false);
	if(ExecuteBindingsNode == nullptr)
	{
		InSettings.ReportError(TEXT("Could not spawn Execute Bindings node"));
		return;
	}
	URigVMPin* ExecuteBindingsExecPin = ExecuteBindingsNode->FindPin(FRigVMStruct::ExecuteContextName.ToString());
	if(ExecuteBindingsExecPin == nullptr)
	{
		InSettings.ReportError(TEXT("Could not find execute pin on Execute Bindings node"));
		return;
	}
	URigVMPin* ExecPin = ExecuteBindingsExecPin;

	// Copy public vars in the WT event
	if(bHasPublicVariablesToCopy && bInThreadSafe)
	{
		URigVMNode* CopyProxyVariablesNode = Controller->AddUnitNode(FRigUnit_CopyModuleProxyVariables::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(200, 0.0f), FString(), false);
		if(CopyProxyVariablesNode == nullptr)
		{
			InSettings.ReportError(TEXT("Could not spawn Copy Module Proxy Variables node"));
			return;
		}
		URigVMPin* CopyProxyVariablesExecPin = CopyProxyVariablesNode->FindPin(FRigVMStruct::ExecuteContextName.ToString());
		if(ExecPin == nullptr)
		{
			InSettings.ReportError(TEXT("Could not find execute pin on Copy Module Proxy Variables node"));
			return;
		}
		bool bLinkAdded = Controller->AddLink(ExecuteBindingsExecPin, CopyProxyVariablesExecPin, false);
		if(!bLinkAdded)
		{
			InSettings.ReportError(TEXT("Could not link Copy Module Proxy Variables node"));
			return;
		}
		ExecPin = CopyProxyVariablesExecPin;
	}

	IVariableBindingType::FBindingGraphFragmentArgs Args;
	Args.Event = BindingsNodeType;
	Args.Controller = Controller;
	Args.BindingGraph = BindingGraph;
	Args.ExecTail = ExecPin;
	Args.bThreadSafe = bInThreadSafe;

	FVector2D Location(0.0f, 0.0f);
	for(const TPair<IVariableBindingType*, TArray<IVariableBindingType::FBindingGraphInput>>& BindingGroupPair : BindingGroups)
	{
		Args.Inputs = BindingGroupPair.Value;
		BindingGroupPair.Key->BuildBindingGraphFragment(InSettings, Args, ExecPin, Location);
	}

	OutGraphs.Add(BindingGraph);
}

UAnimNextRigVMAsset* FUtils::GetAsset(UAnimNextRigVMAssetEditorData* InEditorData)
{
	check(InEditorData);
	return CastChecked<UAnimNextRigVMAsset>(InEditorData->GetOuter());
}

UAnimNextRigVMAssetEditorData* FUtils::GetEditorData(UAnimNextRigVMAsset* InAsset)
{
	check(InAsset);
	return CastChecked<UAnimNextRigVMAssetEditorData>(InAsset->EditorData);
}

FAnimNextParamType FUtils::GetParamTypeFromPinType(const FEdGraphPinType& InPinType)
{
	FAnimNextParamType::EValueType ValueType = FAnimNextParamType::EValueType::None;
	FAnimNextParamType::EContainerType ContainerType = FAnimNextParamType::EContainerType::None;
	UObject* ValueTypeObject = nullptr;

	if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		ValueType = FAnimNextParamType::EValueType::Bool;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		if (UEnum* Enum = Cast<UEnum>(InPinType.PinSubCategoryObject.Get()))
		{
			ValueType = FAnimNextParamType::EValueType::Enum;
			ValueTypeObject = Enum;
		}
		else
		{
			ValueType = FAnimNextParamType::EValueType::Byte;
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		ValueType = FAnimNextParamType::EValueType::Int32;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		ValueType = FAnimNextParamType::EValueType::Int64;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			ValueType = FAnimNextParamType::EValueType::Float;
		}
		else if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			ValueType = FAnimNextParamType::EValueType::Double;
		}
		else
		{
			ensure(false);	// Reals should be either floats or doubles
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Float)
	{
		ValueType = FAnimNextParamType::EValueType::Float;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Double)
	{
		ValueType = FAnimNextParamType::EValueType::Double;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		ValueType = FAnimNextParamType::EValueType::Name;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		ValueType = FAnimNextParamType::EValueType::String;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		ValueType = FAnimNextParamType::EValueType::Text;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
	{
		ValueType = FAnimNextParamType::EValueType::Enum;
		ValueTypeObject = Cast<UEnum>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		ValueType = FAnimNextParamType::EValueType::Struct;
		ValueTypeObject = Cast<UScriptStruct>(InPinType.PinSubCategoryObject.Get());
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Object || InPinType.PinCategory == UEdGraphSchema_K2::AllObjectTypes)
	{
		ValueType = FAnimNextParamType::EValueType::Object;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		ValueType = FAnimNextParamType::EValueType::SoftObject;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		ValueType = FAnimNextParamType::EValueType::SoftClass;
		ValueTypeObject = Cast<UClass>(InPinType.PinSubCategoryObject.Get());
		ensure(ValueTypeObject);
	}

	if(InPinType.ContainerType == EPinContainerType::Array)
	{
		ContainerType = FAnimNextParamType::EContainerType::Array;
	}
	else if(InPinType.ContainerType == EPinContainerType::Set)
	{
		ensureMsgf(false, TEXT("Set pins are not yet supported"));
	}
	if(InPinType.ContainerType == EPinContainerType::Map)
	{
		ensureMsgf(false, TEXT("Map pins are not yet supported"));
	}
	
	return FAnimNextParamType(ValueType, ContainerType, ValueTypeObject);
}

FEdGraphPinType FUtils::GetPinTypeFromParamType(const FAnimNextParamType& InParamType)
{
	FEdGraphPinType PinType;
	PinType.PinSubCategory = NAME_None;

	// Container type
	switch (InParamType.ContainerType)
	{
	case FAnimNextParamType::EContainerType::Array:
		PinType.ContainerType = EPinContainerType::Array;
		break;
	default:
		PinType.ContainerType = EPinContainerType::None;
	}

	// Value type
	switch (InParamType.ValueType)
	{
	case EPropertyBagPropertyType::Bool:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		break;
	case EPropertyBagPropertyType::Byte:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		break;
	case EPropertyBagPropertyType::Int32:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		break;
	case EPropertyBagPropertyType::Int64:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		break;
	case EPropertyBagPropertyType::Float:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		break;
	case EPropertyBagPropertyType::Double:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		break;
	case EPropertyBagPropertyType::Name:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		break;
	case EPropertyBagPropertyType::String:
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		break;
	case EPropertyBagPropertyType::Text:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		break;
	case EPropertyBagPropertyType::Enum:
		// @todo: some pin coloring is not correct due to this (byte-as-enum vs enum). 
		PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Struct:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Object:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftObject:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Class:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftClass:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
		PinType.PinSubCategoryObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	default:
		break;
	}

	return PinType;
}

FRigVMTemplateArgumentType FUtils::GetRigVMArgTypeFromParamType(const FAnimNextParamType& InParamType)
{
	FRigVMTemplateArgumentType ArgType;

	FString CPPTypeString;

	// Value type
	switch (InParamType.ValueType)
	{
	case EPropertyBagPropertyType::Bool:
		CPPTypeString = RigVMTypeUtils::BoolType;
		break;
	case EPropertyBagPropertyType::Byte:
		CPPTypeString = RigVMTypeUtils::UInt8Type;
		break;
	case EPropertyBagPropertyType::Int32:
		CPPTypeString = RigVMTypeUtils::UInt32Type;
		break;
	case EPropertyBagPropertyType::Int64:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::Float:
		CPPTypeString = RigVMTypeUtils::FloatType;
		break;
	case EPropertyBagPropertyType::Double:
		CPPTypeString = RigVMTypeUtils::DoubleType;
		break;
	case EPropertyBagPropertyType::Name:
		CPPTypeString = RigVMTypeUtils::FNameType;
		break;
	case EPropertyBagPropertyType::String:
		CPPTypeString = RigVMTypeUtils::FStringType;
		break;
	case EPropertyBagPropertyType::Text:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::Enum:
		CPPTypeString = RigVMTypeUtils::CPPTypeFromEnum(Cast<UEnum>(InParamType.ValueTypeObject.Get()));
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Struct:
		CPPTypeString = RigVMTypeUtils::GetUniqueStructTypeName(Cast<UScriptStruct>(InParamType.ValueTypeObject.Get()));
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Object:
		CPPTypeString = RigVMTypeUtils::CPPTypeFromObject(Cast<UClass>(InParamType.ValueTypeObject.Get()), RigVMTypeUtils::EClassArgType::AsObject);
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftObject:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	case EPropertyBagPropertyType::Class:
		CPPTypeString = RigVMTypeUtils::CPPTypeFromObject(Cast<UClass>(InParamType.ValueTypeObject.Get()), RigVMTypeUtils::EClassArgType::AsClass);
		ArgType.CPPTypeObject = const_cast<UObject*>(InParamType.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftClass:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled value type %d"), InParamType.ValueType);
		break;
	}

	// Container type
	switch (InParamType.ContainerType)
	{
	case FAnimNextParamType::EContainerType::None:
		break;
	case FAnimNextParamType::EContainerType::Array:
		CPPTypeString = FString::Printf(RigVMTypeUtils::TArrayTemplate, *CPPTypeString);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled container type %d"), InParamType.ContainerType);
		break;
	}

	ArgType.CPPType = *CPPTypeString;

	return ArgType;
}

void FUtils::SetupEventGraph(URigVMController* InController, UScriptStruct* InEventStruct, FName InEventName, bool bPrintPythonCommand)
{
	// Clear the graph
	InController->RemoveNodes(InController->GetGraph()->GetNodes());

	if (InEventStruct->IsChildOf(FRigUnit_AnimNextUserEvent::StaticStruct()))
	{
		FRigUnit_AnimNextUserEvent Defaults;
		Defaults.Name = InEventName;
		Defaults.SortOrder = InEventName.GetNumber();
		InController->AddUnitNodeWithDefaults(InEventStruct, Defaults, FRigVMStruct::ExecuteName, FVector2D(-200.0f, 0.0f), FString(), false);
	}
	else if (InEventStruct == FRigVMFunction_UserDefinedEvent::StaticStruct())
	{
		FRigVMFunction_UserDefinedEvent Defaults;
		Defaults.EventName = InEventName;
		InController->AddUnitNodeWithDefaults(InEventStruct, Defaults, FRigVMStruct::ExecuteName, FVector2D(-200.0f, 0.0f), FString(), false);
	}
	else
	{
		InController->AddUnitNode(InEventStruct, FRigVMStruct::ExecuteName, FVector2D(-200.0f, 0.0f), FString(), false);
	}
}

FAnimNextParamType FUtils::GetParameterTypeFromName(FName InName)
{
	// Query the asset registry for other params
	TMap<FAssetData, FAnimNextAssetRegistryExports> ExportMap;
	GetExportedVariablesFromAssetRegistry(ExportMap);
	for(const TPair<FAssetData, FAnimNextAssetRegistryExports>& ExportPair : ExportMap)
	{
		for(const FAnimNextAssetRegistryExportedVariable& Parameter : ExportPair.Value.Variables)
		{
			if(Parameter.Name == InName)
			{
				return Parameter.Type;
			}
		}
	}

	return FAnimNextParamType();
}

bool FUtils::GetExportedVariablesForAsset(const FAssetData& InAsset, FAnimNextAssetRegistryExports& OutExports)
{
	const FString TagValue = InAsset.GetTagValueRef<FString>(UE::AnimNext::ExportsAnimNextAssetRegistryTag);
	return FAnimNextAssetRegistryExports::StaticStruct()->ImportText(*TagValue, &OutExports, nullptr, PPF_None, nullptr, FAnimNextAssetRegistryExports::StaticStruct()->GetName()) != nullptr;
}

bool FUtils::GetExportedVariablesFromAssetRegistry(TMap<FAssetData, FAnimNextAssetRegistryExports>& OutExports)
{
	TArray<FAssetData> AssetData;
	IAssetRegistry::GetChecked().GetAssetsByTags({UE::AnimNext::ExportsAnimNextAssetRegistryTag}, AssetData);

	for (const FAssetData& Asset : AssetData)
	{
		const FString TagValue = Asset.GetTagValueRef<FString>(UE::AnimNext::ExportsAnimNextAssetRegistryTag);
		FAnimNextAssetRegistryExports AssetExports;
		if (FAnimNextAssetRegistryExports::StaticStruct()->ImportText(*TagValue, &AssetExports, nullptr, PPF_None, nullptr, FAnimNextAssetRegistryExports::StaticStruct()->GetName()) != nullptr)
		{
			OutExports.Add(Asset, MoveTemp(AssetExports));
		}
	}

	return OutExports.Num() > 0;
}

void FUtils::GetAssetFunctions(const UAnimNextRigVMAssetEditorData* InEditorData, FRigVMGraphFunctionHeaderArray& OutExports)
{
	for (const FRigVMGraphFunctionData& FunctionData : InEditorData->GraphFunctionStore.PublicFunctions)
	{
		if (FunctionData.CompilationData.IsValid())
		{
			OutExports.Headers.Add(FunctionData.Header);
		}
	}
}

void FUtils::GetAssetPrivateFunctions(const UAnimNextRigVMAssetEditorData* InEditorData, FRigVMGraphFunctionHeaderArray& OutExports)
{
	for (const FRigVMGraphFunctionData& FunctionData : InEditorData->GraphFunctionStore.PrivateFunctions)
	{
		// Note: We dont check compilation data here as private functions are not compiled if they are not referenced
		OutExports.Headers.Add(FunctionData.Header);
	}
}

bool FUtils::GetExportedFunctionsForAsset(const FAssetData& InAsset, FName Tag, FRigVMGraphFunctionHeaderArray& OutExports)
{
	const FString TagValue = InAsset.GetTagValueRef<FString>(Tag);
	const FArrayProperty* HeadersProperty = CastField<FArrayProperty>(FRigVMGraphFunctionHeaderArray::StaticStruct()->FindPropertyByName(TEXT("Headers")));
	HeadersProperty->ImportText_Direct(*TagValue, &OutExports, nullptr, EPropertyPortFlags::PPF_None);
	return OutExports.Headers.Num() > 0;
}

bool FUtils::GetExportedFunctionsFromAssetRegistry(FName Tag, TMap<FAssetData, FRigVMGraphFunctionHeaderArray>& OutExports)
{
	TArray<FAssetData> AssetData;
	IAssetRegistry::GetChecked().GetAssetsByTags({ Tag }, AssetData);

	const FArrayProperty* HeadersProperty = CastField<FArrayProperty>(FRigVMGraphFunctionHeaderArray::StaticStruct()->FindPropertyByName(TEXT("Headers")));

	for (const FAssetData& Asset : AssetData)
	{
		const FString TagValue = Asset.GetTagValueRef<FString>(Tag);
		FRigVMGraphFunctionHeaderArray AssetExports;

		if (HeadersProperty->ImportText_Direct(*TagValue, &AssetExports, nullptr, EPropertyPortFlags::PPF_None) != nullptr)
		{
			if (AssetExports.Headers.Num() > 0)
			{
				FRigVMGraphFunctionHeaderArray& AssetArray = OutExports.FindOrAdd(Asset);
				AssetArray.Headers.Append(MoveTemp(AssetExports.Headers));
			}
		}
	}

	return OutExports.Num() > 0;
}

static void AddParamToSet(const FAnimNextAssetRegistryExportedVariable& InNewParam, TSet<FAnimNextAssetRegistryExportedVariable>& OutExports)
{
	if(FAnimNextAssetRegistryExportedVariable* ExistingEntry = OutExports.Find(InNewParam))
	{
		if(ExistingEntry->Type != InNewParam.Type)
		{
			UE_LOGFMT(LogAnimation, Warning, "Type mismatch between parameter {ParameterName}. {ParamType1} vs {ParamType1}", InNewParam.Name, InNewParam.Type.ToString(), ExistingEntry->Type.ToString());
		}
		ExistingEntry->Flags |= InNewParam.Flags;
	}
	else
	{
		OutExports.Add(InNewParam);
	}
}

void FUtils::GetAssetVariables(const UAnimNextRigVMAssetEditorData* EditorData, FAnimNextAssetRegistryExports& OutExports)
{
	OutExports.Variables.Reset();
	OutExports.Variables.Reserve(EditorData->Entries.Num());

	TSet<FAnimNextAssetRegistryExportedVariable> ExportSet;
	GetAssetVariables(EditorData, ExportSet);
	OutExports.Variables = ExportSet.Array();
}

void FUtils::GetAssetVariables(const UAnimNextRigVMAssetEditorData* InEditorData, TSet<FAnimNextAssetRegistryExportedVariable>& OutExports)
{
	for(const UAnimNextRigVMAssetEntry* Entry : InEditorData->Entries)
	{
		if(const IAnimNextRigVMExportInterface* ExportInterface = Cast<IAnimNextRigVMExportInterface>(Entry))
		{
			EAnimNextExportedVariableFlags Flags = EAnimNextExportedVariableFlags::Declared;
			if(ExportInterface->GetExportAccessSpecifier() == EAnimNextExportAccessSpecifier::Public)
			{
				Flags |= EAnimNextExportedVariableFlags::Public;
				FAnimNextAssetRegistryExportedVariable NewParam(ExportInterface->GetExportName(), ExportInterface->GetExportType(), Flags);
				AddParamToSet(NewParam, OutExports);
			}
		}
		else if(const UAnimNextDataInterfaceEntry* DataInterfaceEntry = Cast<UAnimNextDataInterfaceEntry>(Entry))
		{
			if(DataInterfaceEntry->DataInterface)
			{
				UAnimNextDataInterface_EditorData* EditorData = GetEditorData<UAnimNextDataInterface_EditorData>(DataInterfaceEntry->DataInterface.Get());
				GetAssetVariables(EditorData, OutExports);
			}
		}
	}
}

void FUtils::GetAssetOutlinerItems(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FAssetRegistryTagsContext Context)
{
	FWorkspaceOutlinerItemExport AssetIdentifier = FWorkspaceOutlinerItemExport(EditorData->GetOuter()->GetFName(), EditorData->GetOuter());
	for(UAnimNextRigVMAssetEntry* Entry : EditorData->Entries)
	{
		if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
		{
			if (Entry->IsHiddenInOutliner())
			{
				if (URigVMEdGraph* RigVMEdGraph = GraphInterface->GetEdGraph())
				{
					CreateSubGraphsOutlinerItemsRecursive(EditorData, OutExports, AssetIdentifier, INDEX_NONE, RigVMEdGraph, Context);
				}
			}
			else
			{
				FWorkspaceOutlinerItemExport& Export = OutExports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(Entry->GetEntryName(), AssetIdentifier));

				Export.GetData().InitializeAsScriptStruct(FAnimNextGraphOutlinerData::StaticStruct());
				FAnimNextGraphOutlinerData& GraphData = Export.GetData().GetMutable<FAnimNextGraphOutlinerData>();
				GraphData.SoftEntryPtr = Entry;

				if (URigVMEdGraph* RigVMEdGraph = GraphInterface->GetEdGraph())
				{
					int32 ExportIndex = OutExports.Exports.Num() - 1;
					CreateSubGraphsOutlinerItemsRecursive(EditorData, OutExports, AssetIdentifier, ExportIndex, RigVMEdGraph, Context);
				}
			}
		}
	}

	CreateFunctionLibraryOutlinerItemsRecursive(EditorData, OutExports, AssetIdentifier, INDEX_NONE, EditorData->GetRigVMGraphFunctionStore()->PublicFunctions, EditorData->GetRigVMGraphFunctionStore()->PrivateFunctions);
}

void ProcessPinAssetReferences(const URigVMPin* InPin, FWorkspaceOutlinerItemExports& OutExports, FWorkspaceOutlinerItemExport& RootExport, int32 ParentExportIndex)
{
	if (InPin)
	{
		const UObject* TypeObject = InPin->GetCPPTypeObject();
		if (Cast<UClass>(TypeObject))
		{
			const FSoftObjectPath ObjectPath(InPin->GetDefaultValue());
			if (ObjectPath.IsValid())
			{
				// Only add export if object is loaded, or the path actually points to an asset
				FAssetData ReferenceAssetData;
				if (ObjectPath.ResolveObject() || IAssetRegistry::GetChecked().TryGetAssetByObjectPath(ObjectPath, ReferenceAssetData) == UE::AssetRegistry::EExists::Exists)
				{
					FWorkspaceOutlinerItemExport& ParentExport = ParentExportIndex == INDEX_NONE ? RootExport : OutExports.Exports[ParentExportIndex];
					FWorkspaceOutlinerItemExport& ReferenceExport = OutExports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(FName(ObjectPath.ToString()), ParentExport));
					ReferenceExport.GetData().InitializeAs<FWorkspaceOutlinerAssetReferenceItemData>();
					ReferenceExport.GetData().GetMutable<FWorkspaceOutlinerAssetReferenceItemData>().ReferredObjectPath = ObjectPath;
				}
			}
		}

		for (const URigVMPin* SubPin : InPin->GetSubPins())
		{
			ProcessPinAssetReferences(SubPin, OutExports, RootExport, ParentExportIndex);
		}
	}
	
}

void FUtils::CreateSubGraphsOutlinerItemsRecursive(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FWorkspaceOutlinerItemExport& RootExport, int32 ParentExportIndex, URigVMEdGraph* RigVMEdGraph, FAssetRegistryTagsContext Context)
{
	if (RigVMEdGraph == nullptr)
	{
		return;
	}

	// Handle pin asset references (disabled during save as GetMetaData can cause StaticFindFast calls which is prohibited during save)
	if (!Context.IsSaving())
	{
		for (const TObjectPtr<class UEdGraphNode>& Node : RigVMEdGraph->Nodes)
		{
			if (URigVMEdGraphNode* RigVMEdNode = Cast<URigVMEdGraphNode>(Node))
			{
				if (URigVMTemplateNode* TemplateRigVMNode = Cast<URigVMTemplateNode>(RigVMEdNode->GetModelNode()))
				{
					if (TemplateRigVMNode->GetScriptStruct() && TemplateRigVMNode->GetScriptStruct()->IsChildOf(FRigUnit_AnimNextBase::StaticStruct()))
					{
						for (URigVMPin* ModelPin : TemplateRigVMNode->GetPins())
						{
							if (ModelPin->GetDirection() == ERigVMPinDirection::Input)
							{
								auto HandlePin = [&OutExports, &RootExport, ParentExportIndex](const URigVMPin* InPin)
								{
									if (InPin->GetMetaData(TEXT("ExportAsReference")) == TEXT("true"))
									{
										ProcessPinAssetReferences(InPin, OutExports, RootExport, ParentExportIndex);
									}										
								};
								
								HandlePin(ModelPin);
								
								for (URigVMPin* TraitPin : ModelPin->GetSubPins())
								{									
									HandlePin(TraitPin);
								}
							}
						}
					}
				}
			}
		}
	}

	// ---- Collapsed graphs ---
	for (const TObjectPtr<UEdGraph>& SubGraph : RigVMEdGraph->SubGraphs)
	{
		URigVMEdGraph* EditorObject = Cast<URigVMEdGraph>(SubGraph);
		if (IsValid(EditorObject))
		{
			if(ensure(EditorObject->GetModel()))
			{
				URigVMCollapseNode* CollapseNode = CastChecked<URigVMCollapseNode>(EditorObject->GetModel()->GetOuter());

				FWorkspaceOutlinerItemExport& ParentExport = ParentExportIndex == INDEX_NONE ? RootExport : OutExports.Exports[ParentExportIndex];
				FWorkspaceOutlinerItemExport& Export = OutExports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(CollapseNode->GetFName(), ParentExport));
				Export.GetData().InitializeAsScriptStruct(FAnimNextCollapseGraphOutlinerData::StaticStruct());
			
				FAnimNextCollapseGraphOutlinerData& FnGraphData = Export.GetData().GetMutable<FAnimNextCollapseGraphOutlinerData>();
				FnGraphData.SoftEditorObject = EditorObject;

				int32 ExportIndex = OutExports.Exports.Num() - 1;
				CreateSubGraphsOutlinerItemsRecursive(EditorData, OutExports, RootExport, ExportIndex, EditorObject, Context);
			}
		}
	}

	// ---- Function References ---
	TArray<URigVMEdGraphNode*> EdNodes;
	RigVMEdGraph->GetNodesOfClass(EdNodes);

	for (URigVMEdGraphNode* EdNode : EdNodes)
	{
		if (URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(EdNode->GetModelNode()))
		{
			// Only export referenced functions which are part of same outer
			const FRigVMGraphFunctionIdentifier FunctionIdentifier = FunctionReferenceNode->GetFunctionIdentifier();
			if (FunctionIdentifier.HostObject == FSoftObjectPath(EditorData))
			{
				const URigVMLibraryNode* FunctionNode = EditorData->RigVMClient.GetFunctionLibrary()->FindFunction(FunctionIdentifier.GetFunctionFName());
				if (URigVMGraph* ContainedGraph = FunctionNode->GetContainedGraph())
				{
					FWorkspaceOutlinerItemExport& ParentExport = ParentExportIndex == INDEX_NONE ? RootExport : OutExports.Exports[ParentExportIndex];
					FWorkspaceOutlinerItemExport& Export = OutExports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(FunctionNode->GetFName(), ParentExport));
					Export.GetData().InitializeAsScriptStruct(FAnimNextGraphFunctionOutlinerData::StaticStruct());
					FAnimNextGraphFunctionOutlinerData& FnGraphData = Export.GetData().GetMutable<FAnimNextGraphFunctionOutlinerData>();

					if (URigVMEdGraph* ContainedEdGraph = Cast<URigVMEdGraph>(EditorData->GetEditorObjectForRigVMGraph(ContainedGraph)))
					{
						FnGraphData.SoftEditorObject = ContainedEdGraph;
						FnGraphData.SoftEdGraphNode = EdNode;
						
						int32 ExportIndex = OutExports.Exports.Num() - 1;
						CreateSubGraphsOutlinerItemsRecursive(EditorData, OutExports, RootExport, ExportIndex, ContainedEdGraph, Context);
					}
				}
			}
		}
	}
}

void FUtils::CreateFunctionLibraryOutlinerItemsRecursive(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FWorkspaceOutlinerItemExport& RootExport, int32 ParentExportIndex, const TArray<FRigVMGraphFunctionData>& PublicFunctions, const TArray<FRigVMGraphFunctionData>& PrivateFunctions)
{
	if (PrivateFunctions.Num() > 0 || PublicFunctions.Num() > 0)
	{
		CreateFunctionsOutlinerItemsRecursive(EditorData, OutExports, RootExport, ParentExportIndex, PrivateFunctions, false);
		CreateFunctionsOutlinerItemsRecursive(EditorData, OutExports, RootExport, ParentExportIndex, PublicFunctions, true);
	}
}

void FUtils::CreateFunctionsOutlinerItemsRecursive(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FWorkspaceOutlinerItemExport& RootExport, int32 ParentExportIndex, const TArray<FRigVMGraphFunctionData>& Functions, bool bPublicFunctions)
{
	for (const FRigVMGraphFunctionData& FunctionData : Functions)
	{
		const URigVMLibraryNode* FunctionNode = EditorData->RigVMClient.GetFunctionLibrary()->FindFunction(FunctionData.Header.LibraryPointer.GetFunctionFName());
		if (FunctionNode)
		{
			if (URigVMGraph* ContainedModelGraph = FunctionNode->GetContainedGraph())
			{
				if (URigVMEdGraph* EditorObject = Cast<URigVMEdGraph>(EditorData->GetEditorObjectForRigVMGraph(ContainedModelGraph)))
				{
					FWorkspaceOutlinerItemExport& ParentExport = ParentExportIndex == INDEX_NONE ? RootExport : OutExports.Exports[ParentExportIndex];
					FWorkspaceOutlinerItemExport& Export = OutExports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(FunctionData.Header.Name, ParentExport));

					Export.GetData().InitializeAsScriptStruct(FAnimNextGraphFunctionOutlinerData::StaticStruct());
					FAnimNextGraphFunctionOutlinerData& FnGraphData = Export.GetData().GetMutable<FAnimNextGraphFunctionOutlinerData>();
					FnGraphData.SoftEditorObject = EditorObject;
				}
			}
		}
	}
}

const FText& FUtils::GetFunctionLibraryDisplayName()
{
	static const FText FunctionLibraryName = LOCTEXT("WorkspaceFunctionLibraryName", "Function Library");
	return FunctionLibraryName;
}

#if WITH_EDITOR
void FUtils::OpenProgrammaticGraphs(UAnimNextRigVMAssetEditorData* EditorData, const TArray<URigVMGraph*>& ProgrammaticGraphs)
{
	UAnimNextRigVMAsset* OwningAsset = FUtils::GetAsset(EditorData);
	UE::Workspace::IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<UE::Workspace::IWorkspaceEditorModule>("WorkspaceEditor");
	if(UE::Workspace::IWorkspaceEditor* WorkspaceEditor = WorkspaceEditorModule.OpenWorkspaceForObject(OwningAsset, UE::Workspace::EOpenWorkspaceMethod::Default))
	{
		TArray<UObject*> Graphs;
		for(URigVMGraph* ProgrammaticGraph : ProgrammaticGraphs)
		{
			// Some explanation needed here!
			// URigVMEdGraph caches its underlying model internally in GetModel depending on its outer if it is no attached to a RigVMClient
			// So here we rename the graph into the transient package so we dont get any notifications
			ProgrammaticGraph->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);

			// then create the graph (transient so it outers to the RigVMGraph)
			URigVMEdGraph* EdGraph = CastChecked<URigVMEdGraph>(EditorData->CreateEdGraph(ProgrammaticGraph, true));

			// Then cache the model
			EdGraph->GetModel();
			Graphs.Add(EdGraph);

			// Now rename into this asset again to be able to correctly create a controller (needed to view the graph and interact with it)
			ProgrammaticGraph->Rename(nullptr, EditorData, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			URigVMController* ProgrammaticController = EditorData->GetOrCreateController(ProgrammaticGraph);

			// Resend notifications to rebuild the EdGraph
			ProgrammaticController->ResendAllNotifications();
		}

		WorkspaceEditor->OpenObjects(Graphs);
	}
}
#endif // WITH_EDITOR

FString FUtils::MakeFunctionWrapperVariableName(FName InFunctionName, FName InVariableName)
{
	// We assume the function name is enough for variable name uniqueness in this graph (We don't yet desire global uniqueness).
	return TEXT("__InternalVar_") + InFunctionName.ToString() + "_" + InVariableName.ToString();
}

FString FUtils::MakeFunctionWrapperEventName(FName InFunctionName)
{
	return TEXT("__InternalCall_") + InFunctionName.ToString();
}

}

#undef LOCTEXT_NAMESPACE